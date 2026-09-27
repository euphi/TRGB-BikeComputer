/*
 * LogSessions.cpp
 *
 * See LogSessions.h.
 */

#include "LogSessions.h"
#include "LogRecords.h"
#include "Singletons.h"

#include <Preferences.h>
#include <SD_MMC.h>
#include <algorithm>
#include <atomic>
#include <esp_heap_caps.h>
#include <vector>

namespace LogSessions {

// A literal, not LOGDIR + ...: both are globals in different files, and C++ doesn't define
// which of them is constructed first.
const String WORKDIR = "/BIKECOMP/CUR";

namespace {

std::atomic<bool> running{false};
String activeStem;					// set before the task starts, read-only afterwards

constexpr size_t CHUNK_RECORDS = 32;			// 2 KB internal RAM while the task runs
constexpr size_t MAX_STEMS = 32;				// sessions per boot; the rest waits for the next one
constexpr size_t MAX_FILES_PER_STEM = 64;		// raw captures R_*_NN included
constexpr uint8_t MAX_TRIES = 2;				// then the session is moved unparsed
constexpr uint16_t BACKFILL_PER_BOOT = 20;		// older data logs summarised per boot

using SessStats::TimeHints;
using SessStats::Stats;
using SessStats::Summary;

void logInfo(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void logInfo(const char* fmt, ...) {
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	bclog.log(BCLogger::Log_Info, BCLogger::TAG_SD, buf);
}

void logWarn(const String& s) {bclog.log(BCLogger::Log_Warn, BCLogger::TAG_SD, s);}

bool ensureDir(const String& dir) {
	if (SD_MMC.exists(dir) || SD_MMC.mkdir(dir)) return true;
	logWarn("📦 Can't create " + dir);
	return false;
}

void readHints(const String& path, TimeHints& h) {
	File f = SD_MMC.open(path, FILE_READ);
	if (!f) return;
	char line[96];
	for (uint16_t n = 0; f.available() && n < 500; n++) {		// bounded: a hint file has a handful of lines
		const size_t len = f.readBytesUntil('\n', line, sizeof(line) - 1);
		line[len] = '\0';
		h.parseLine(line);
	}
	f.close();
}

bool readFirstRecordTime(const String& path, int64_t& ms) {
	File f = SD_MMC.open(path, FILE_READ);
	if (!f) return false;
	uint8_t rec[LogRec::RECORD_SIZE];
	const bool ok = f.read(rec, sizeof(rec)) == sizeof(rec);
	f.close();
	if (ok) ms = SessStats::recordTimeMs(rec);
	return ok;
}

// Copies src to dst with the 1970 timestamps corrected. Writes to a hidden temporary file
// ('x' = hidden in the listing) and renames it at the end, so an interrupted copy leaves
// the source untouched and nothing half-written under the final name.
bool copyCorrected(const String& src, const String& dst, const TimeHints& hints, uint8_t* buf) {
	const int slash = dst.lastIndexOf('/');
	const String tmp = dst.substring(0, slash + 1) + "x" + dst.substring(slash + 1) + ".tmp";
	File in = SD_MMC.open(src, FILE_READ);
	if (!in) return false;
	File out = SD_MMC.open(tmp, FILE_WRITE, true);
	if (!out) {in.close(); return false;}
	bool ok = true;
	size_t n;
	while ((n = in.read(buf, CHUNK_RECORDS * LogRec::RECORD_SIZE)) > 0) {
		const size_t whole = n - n % LogRec::RECORD_SIZE;	// a torn last record is copied as is
		for (size_t o = 0; o < whole; o += LogRec::RECORD_SIZE) {
			int64_t t = SessStats::recordTimeMs(buf + o);
			if (!SessStats::isValidMs(t) && hints.correct(t)) SessStats::setRecordTimeMs(buf + o, t);
		}
		if (out.write(buf, n) != n) {ok = false; break;}
		vTaskDelay(1);
	}
	in.close();
	out.close();
	if (ok) ok = SD_MMC.rename(tmp, dst);
	if (!ok) SD_MMC.remove(tmp);
	return ok;
}

bool scanStats(const String& path, const TimeHints& hints, Stats& st, uint8_t* buf) {
	File f = SD_MMC.open(path, FILE_READ);
	if (!f) return false;
	size_t n;
	while ((n = f.read(buf, CHUNK_RECORDS * LogRec::RECORD_SIZE)) > 0) {
		for (size_t o = 0; o + LogRec::RECORD_SIZE <= n; o += LogRec::RECORD_SIZE) st.add(buf + o, hints);
		vTaskDelay(1);
	}
	f.close();
	return true;
}

bool writeSummary(const String& path, const Summary& sum) {
	char text[512];
	const size_t len = sum.format(text, sizeof(text));
	if (!len) return false;
	File f = SD_MMC.open(path, FILE_WRITE, true);
	if (!f) return false;
	const bool ok = f.write(reinterpret_cast<const uint8_t*>(text), len) == len;
	f.close();
	return ok;
}

// Rename, and if the target is taken, keep the file under its session number in NO_TIME
// rather than overwrite anything. false = still in CUR, tried again after the next boot.
bool moveFile(const String& src, const String& dst, const char* name) {
	if (SD_MMC.rename(src, dst)) return true;
	const String fallback = BCLogger::LOGDIR + "/NO_TIME/" + name;
	if (fallback != dst && ensureDir(BCLogger::LOGDIR + "/NO_TIME") && !SD_MMC.exists(fallback) && SD_MMC.rename(src, fallback)) {
		logWarn("📦 " + dst + " not possible, moved to " + fallback);
		return true;
	}
	logWarn("📦 Can't move " + src + " - stays for the next boot");
	return false;
}

// Crash-loop guard: count attempts per session in NVS, cleared when done.
uint8_t beginAttempt(const String& stem) {
	Preferences p;
	if (!p.begin("LogFin", false)) return 1;
	uint8_t tries = (p.getString("stem", "") == stem) ? p.getUChar("tries", 0) : 0;
	tries++;
	p.putString("stem", stem);
	p.putUChar("tries", tries);
	p.end();
	return tries;
}

void endAttempt() {
	Preferences p;
	if (!p.begin("LogFin", false)) return;
	p.clear();
	p.end();
}

void finalizeSession(const String& stem, uint8_t* buf) {
	const String cur = WORKDIR + "/";
	const uint8_t tries = beginAttempt(stem);
	const bool unparsed = tries > MAX_TRIES;
	if (unparsed) logWarn("📦 Session " + stem + " failed " + String(tries - 1) + "x - moving it without evaluation");

	// Names first: renaming out of a directory while iterating it can skip entries.
	std::vector<String> names;
	File dir = SD_MMC.open(WORKDIR);
	if (!dir) return;
	for (File f = dir.openNextFile(); f && names.size() < MAX_FILES_PER_STEM; f = dir.openNextFile()) {
		if (!f.isDirectory() && stemOf(f.name()) == stem) names.push_back(f.name());
		f.close();
	}
	dir.close();

	TimeHints hints;
	if (!unparsed) readHints(cur + "T" + stem + ".txt", hints);
	const String lSrc = cur + "L" + stem + ".bin";
	bool hasL = false;
	uint16_t nRaw = 0;
	for (const String& n : names) {
		if (n[0] == 'L' && n == "L" + stem + ".bin") hasL = true;
		if (n[0] == 'R') nRaw++;
	}

	// Where it goes: the day and time the session started
	int64_t startMs = 0;
	bool startValid = false;
	if (!unparsed) {
		if (hints.hasStart) {startMs = hints.startMs; startValid = hints.correct(startMs);}
		else if (hasL && readFirstRecordTime(lSrc, startMs)) startValid = hints.correct(startMs);
	}
	String dstDir = BCLogger::LOGDIR + "/NO_TIME";
	String newStem = stem;
	if (startValid) {
		const time_t s = static_cast<time_t>(startMs / 1000);
		struct tm tm;
		localtime_r(&s, &tm);
		char d[16], t[16];
		strftime(d, sizeof(d), "%Y%m%d", &tm);
		strftime(t, sizeof(t), "_%H%M%S", &tm);
		const String dd = BCLogger::LOGDIR + "/" + d;
		// Never overwrite another session's data log (two starts in the same second): keep
		// the number instead. Without L in CUR an L there is this session's own, moved by an
		// earlier attempt that didn't get to the end -- the rest follows it.
		if (!hasL || !SD_MMC.exists(dd + "/L" + t + ".bin")) {
			dstDir = dd;
			newStem = t;
		}
	}
	if (!ensureDir(dstDir)) return;
	const String dst = dstDir + "/";

	// Data log: copied with corrected timestamps, or just moved
	const String lDst = dst + "L" + newStem + ".bin";
	bool lCorrected = false, lMoved = false;
	if (hasL) {
		// A correction exists only if the clock went from 1970 to valid during the session,
		// so there are 1970 records to rewrite (only those are touched).
		const bool needFix = !unparsed && hints.hasCorrection;
		if (needFix && buf) {
			lCorrected = copyCorrected(lSrc, lDst, hints, buf);
			if (lCorrected) SD_MMC.remove(lSrc);
			else logWarn("📦 Correcting timestamps of " + lSrc + " failed - moved uncorrected");
		}
		lMoved = lCorrected || moveFile(lSrc, lDst, ("L" + stem + ".bin").c_str());
	}

	// Summary -- also when an earlier attempt moved L but didn't get to write it
	const String iDst = dst + "I" + newStem + ".txt";
	String lStats;
	if (lMoved) lStats = SD_MMC.exists(lDst) ? lDst : BCLogger::LOGDIR + "/NO_TIME/L" + stem + ".bin";
	else if (!hasL && SD_MMC.exists(lDst) && !SD_MMC.exists(iDst)) lStats = lDst;
	Summary sum;
	bool haveSum = false;
	if (!unparsed && lStats.length() && buf) {
		Stats st;
		if (scanStats(lStats, hints, st, buf)) {
			sum.fromStats(st, hints);
			sum.lCorrected = lCorrected;
			// The scan above read the rewritten file, where nothing is 1970 any more
			if (lCorrected && strcmp(sum.time, "ok") == 0) snprintf(sum.time, sizeof(sum.time), "corrected");
			sum.nRaw = nRaw;
			haveSum = writeSummary(iDst, sum);
			if (!haveSum) logWarn("📦 Can't write the summary for " + lDst);
		}
	}

	// Everything else of the session under the same new name; empty files are dropped
	// (the Forumslader log stays empty on a bike without one).
	for (const String& n : names) {
		if (n[0] == 'L' && hasL) continue;
		const String src = cur + n;
		File f = SD_MMC.open(src, FILE_READ);
		const size_t size = f ? f.size() : 1;
		if (f) f.close();
		if (size == 0) {SD_MMC.remove(src); continue;}
		// "R_0042_01.bin" -> "R" + newStem + "_01.bin"
		const String rest = n.substring(1 + stem.length());
		moveFile(src, dst + n[0] + newStem + rest, n.c_str());
		vTaskDelay(1);
	}

	endAttempt();
	if (haveSum) {
		char line[160];
		sum.describe(line, sizeof(line), false);
		logInfo("📦 Session %s -> %sL%s.bin (time %s): %s", stem.c_str(), dst.c_str(), newStem.c_str(), sum.time, line);
	} else {
		logInfo("📦 Session %s -> %s*%s (no data log evaluated)", stem.c_str(), dst.c_str(), newStem.c_str());
	}
}

// Summaries for the data logs written before sessions got them, and new ones where the
// summary is of an older Summary::VERSION: until done for this version, a few per boot so
// the card isn't busy for minutes, newest day first. Only format v2 (64-byte records);
// older logs are left alone. Nothing is moved or rewritten; a 1970 log without time hints
// stays undated.
bool backfill(uint8_t* buf) {
	Preferences p;
	uint16_t doneVersion = 0;
	if (p.begin("LogBackfill", true)) {doneVersion = p.getUShort("ver", 0); p.end();}
	if (doneVersion >= Summary::VERSION || !buf) return false;

	std::vector<String> days;
	File root = SD_MMC.open(BCLogger::LOGDIR);
	if (!root || !root.isDirectory()) return false;
	for (File f = root.openNextFile(); f && days.size() < 400; f = root.openNextFile()) {
		if (f.isDirectory() && strcmp(f.name(), WORKDIR_NAME) != 0) days.push_back(f.name());
		f.close();
	}
	root.close();
	std::sort(days.begin(), days.end(), [](const String& a, const String& b) {return a > b;});

	uint16_t budget = BACKFILL_PER_BOOT, written = 0;
	bool complete = true;
	for (const String& day : days) {
		const String dirPath = BCLogger::LOGDIR + "/" + day + "/";
		std::vector<String> names;
		File dir = SD_MMC.open(BCLogger::LOGDIR + "/" + day);
		if (!dir) continue;
		for (File f = dir.openNextFile(); f && names.size() < 512; f = dir.openNextFile()) {
			if (!f.isDirectory()) names.push_back(f.name());
			f.close();
		}
		dir.close();
		for (const String& n : names) {
			if (n[0] != 'L' || !n.endsWith(".bin")) continue;
			const String stem = stemOf(n.c_str());
			if (!stem.length()) continue;
			Summary old;
			const bool hadOld = LogSessions::readSummary(dirPath + "I" + stem + ".txt", old);
			if (hadOld && old.version >= Summary::VERSION) continue;
			if (!budget) {complete = false; break;}
			uint8_t first[LogRec::RECORD_SIZE];
			File f = SD_MMC.open(dirPath + n, FILE_READ);
			const bool v2 = f && f.read(first, sizeof(first)) == sizeof(first) && first[LogRec::VERSION_OFFSET] == LogRec::FORMAT_VERSION;
			if (f) f.close();
			if (!v2) continue;
			budget--;
			TimeHints hints;			// finished sessions keep theirs as T_*.txt
			readHints(dirPath + "T" + stem + ".txt", hints);
			Stats st;
			if (!scanStats(dirPath + n, hints, st, buf)) continue;
			Summary sum;
			sum.fromStats(st, hints);
			if (hadOld && old.lCorrected) {
				sum.lCorrected = true;
				if (strcmp(sum.time, "ok") == 0) snprintf(sum.time, sizeof(sum.time), "corrected");
			}
			for (const String& r : names) if (r[0] == 'R' && stemOf(r.c_str()) == stem) sum.nRaw++;
			if (writeSummary(dirPath + "I" + stem + ".txt", sum)) written++;
		}
		if (!complete) break;
	}
	if (written) logInfo("📦 Summaries added/updated for %u earlier data log(s)%s", written, complete ? "" : ", more after the next boot");
	if (complete && p.begin("LogBackfill", false)) {p.putUShort("ver", Summary::VERSION); p.end();}
	return written > 0;
}

void finalizerTask(void*) {
	std::vector<String> stems;
	File dir = SD_MMC.open(WORKDIR);
	if (dir && dir.isDirectory()) {
		for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
			if (!f.isDirectory()) {
				const String s = stemOf(f.name());
				if (s.length() && s != activeStem && stems.size() < MAX_STEMS) {
					bool known = false;
					for (const String& k : stems) known |= (k == s);
					if (!known) stems.push_back(s);
				}
			}
			f.close();
			vTaskDelay(1);
		}
	}
	if (dir) dir.close();

	// Internal RAM: SD DMA needs it, and a PSRAM buffer read in a loop is what makes the
	// display flicker (doc/PITFALLS.md). Without it sessions are still moved.
	uint8_t* buf = static_cast<uint8_t*>(heap_caps_malloc(CHUNK_RECORDS * LogRec::RECORD_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
	if (!stems.empty()) {
		logInfo("📦 %u earlier session(s) to finish", static_cast<unsigned>(stems.size()));
		if (!buf) logWarn("📦 No memory for evaluating sessions - only moving them");
		for (const String& s : stems) finalizeSession(s, buf);
	}
	const bool worked = backfill(buf) || !stems.empty();
	free(buf);
	// The stack size above is a guess plus margin; this shows what is left of it.
	if (worked) logInfo("📦 Finalizer done (stack: %u byte unused)", static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
	running = false;
	vTaskDelete(nullptr);
}

}	// namespace

String stemOf(const char* name) {
	if (!name || !isalpha(static_cast<unsigned char>(name[0]))) return String();
	const char* p = name + 1;
	String s;
	if (*p == '_') {s += '_'; p++;}
	while (*p && *p != '.' && *p != '_') {
		if (!isdigit(static_cast<unsigned char>(*p))) return String();
		s += *p++;
	}
	return (s.length() > 1 || (s.length() == 1 && s[0] != '_')) ? s : String();
}

bool busy() {return running;}

void startFinalizer(const String& stem) {
	if (running) return;
	activeStem = stem;
	running = true;
	// Priority 1: below everything that matters while riding; SD waits block it anyway.
	// 7 KB stack: FATFS keeps long file names on the stack (CONFIG_FATFS_LFN_STACK) and
	// log() formats on it. Measured with 6 KB: 1224 byte left after correcting a session.
	// Freed when the task ends.
	if (xTaskCreate(finalizerTask, "LogFinalizer", 7168, nullptr, 1, nullptr) != pdPASS) {
		running = false;
		logWarn("📦 Can't start the session finalizer - earlier sessions stay in " + WORKDIR);
	}
}

bool readSummary(const String& path, SessStats::Summary& out) {
	File f = SD_MMC.open(path, FILE_READ);
	if (!f) return false;
	char text[512];
	const size_t n = f.read(reinterpret_cast<uint8_t*>(text), sizeof(text) - 1);
	f.close();
	text[n] = '\0';
	return out.parse(text);
}

}	// namespace LogSessions
