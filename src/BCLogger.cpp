/*
 * BCLogger.cpp
 *
 *  Created on: 25.02.2023
 *      Author: ian
 */

#include <BCLogger.h>
#include <DateTime.h>
#include <SD_MMC.h>
#include <LittleFS.h>
#include "Singletons.h"
#include <esp_task_wdt.h>
#include <esp_core_dump.h>
#include <esp_heap_caps.h>
#include <sys/time.h>
#include "WebInstrument.h"
#include "WebPage.h"
#include "ClockSync.h"
#include "LogSessions.h"
#include "SessionStats.h"


const char *BCLogger::TAG_STRING[LogTagMax] = { "RAW", "FL", "BLE", "STAT", "WIFI", "SD", "OP", "CLI", "UI", "WEB" };
const char *BCLogger::LEVEL_STRING[LogTypeMax] = { "DEBUG", "INFO", "WARN", "ERROR" };

const String BCLogger::TAG_SYMBOL[LogTagMax] = { String("📜"), String("📟"), String("🔵"), String("📊"), String("📶"), String("💾"), String("🎮"), String("⌨"), String("🖥️"), String("🌐") };
const String BCLogger::LEVEL_SYMBOL[LogTypeMax] = { String("🐛"), String("ℹ️"), String("⚠️"), String("❌") };

// Adding a tag or level means extending, in lockstep: the enum in BCLogger.h, the four
// arrays above, and the loglevel[][] default matrix in BCLogger.h. The web pages take the
// tag list and icons from /log/get, so nothing under data/site/ needs touching (log.html
// used to carry its own copy). The array bounds come from the enum, so a forgotten entry is NOT a
// compile error -- it just leaves a nullptr that blows up the first "%s" that hits it.
// A static_assert can't see that (the elements aren't constant expressions), so check at
// boot instead; see the call at the top of setup().
void BCLogger::checkTagTablesComplete() const {
	for (uint_fast8_t t = 0; t < LogTagMax; t++) {
		if (TAG_STRING[t] == nullptr || TAG_SYMBOL[t].isEmpty()) {
			// Can't route this through log() -- that would use the very table that's broken.
			Serial.printf("!!! BCLogger: tag table incomplete at index %u (LogTagMax=%u) -- "
			              "extend TAG_STRING/TAG_SYMBOL in BCLogger.cpp\n", t, LogTagMax);
		}
	}
	for (uint_fast8_t l = 0; l < LogTypeMax; l++) {
		if (LEVEL_STRING[l] == nullptr || LEVEL_SYMBOL[l].isEmpty()) {
			Serial.printf("!!! BCLogger: level table incomplete at index %u (LogTypeMax=%u)\n", l, LogTypeMax);
		}
	}
}

const String BCLogger::LOGDIR = "/BIKECOMP";

void cmdCB(cmd *c) {
	const Command cmd(c); // Create wrapper object
	bclog.handleCommand(cmd);
}

// Tab completion for "loglevel <tag> <level> [-serial] [-file]"
static void completeLoglevel(uint8_t pos, SerialConsole::Matches& m) {
	if (pos == 1) {
		for (uint8_t t = 0; t < BCLogger::LogTagMax; t++) m.add(BCLogger::TAG_STRING[t]);
	} else if (pos == 2) {
		for (uint8_t l = 0; l < BCLogger::LogTypeMax; l++) m.add(BCLogger::LEVEL_STRING[l]);
	} else {
		m.add("-serial");
		m.add("-file");
	}
}

BCLogger::BCLogger():
		logevents("/debug/logevent")
{
	xPrintMutex = xSemaphoreCreateMutex();

}

void BCLogger::setup() {
	checkTagTablesComplete();
	ClockSync::setup();
	logPrefs[OUT_Serial].begin("LSer", true);
	logPrefs[OUT_File].begin("LFile", true);
	for (uint_fast8_t c = 0; c < LogOutputMax; c++) {
		for (uint_fast8_t d = 0; d < LogTagMax; d++) {
			LogType level = static_cast<LogType>(logPrefs[c].getLong(TAG_STRING[d], -1));
			if (level >= 0) {
				loglevel[c][d] = level;
			} else {
				logf(Log_Info, TAG_OP, "Can't load loglevel for %s from preferences [out: %d]", TAG_STRING[d], c);
			}
		}
	}
	logPrefs[OUT_File].end();
	logPrefs[OUT_Serial].end();

	logcmd = console.addCmd("loglevel", cmdCB, completeLoglevel);
	logcmd.addPositionalArgument("logtag");
	logcmd.addPositionalArgument("loglevel");
	logcmd.addFlagArgument("serial");
	logcmd.addFlagArgument("file");

	logShow = console.addCmd("showloglevel", cmdCB);

	replayLog = console.addCmd("replay", cmdCB);
	replayLog.addPositionalArgument("path");

	ClockSync::registerCli();

	// FlusherTask and the SSE log stream don't touch the SD card at all (flushAllFiles() already
	// null-checks each File before using it), so set them up unconditionally -- previously an
	// early return here for a missing SD card also silently disabled the live web log stream,
	// which has nothing to do with SD presence.
	// Before FlusherTask, which drains it, and before the early return for a missing SD card
	// below: producers may enqueue regardless (writeRecord() drops without a data file).
	recordQueue = xQueueCreate(RECORD_QUEUE_LEN, LogRec::RECORD_SIZE);
	rawBuffer = xMessageBufferCreate(RAW_BUFFER_SIZE);
	rawRx = static_cast<uint8_t*>(heap_caps_malloc(RAW_MAX_MSG, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
	// 4608 byte (ESP-IDF's xTaskCreate takes byte, not words): 3072 was already tight for
	// logf()'s 256-byte stack buffer, this task also runs WebInstr::report()/drain(), and it now
	// writes all binary and raw records to the card as well (5120 left 2448 byte free).
	// Measured 2026-09-27 with 4608 after a clock step (checkClockStep() opens the time-hint
	// file): 1376 byte free.
	xTaskCreate(+[](void* thisInstance){((BCLogger*)thisInstance)->flushAllFiles();}, "FlusherTask", 4608, this, 5, &flushTaskHandle);
	webserver.getServer().addHandler(&logevents);

	uint8_t cardType = SD_MMC.cardType();
	if (cardType == CARD_NONE) {
		log(Log_Warn, TAG_SD, "No SD card attached!");
		return;
	}
	logf(Log_Info, TAG_SD, "SD Card Type: %s", (cardType == CARD_MMC) ? "MMC" : (cardType == CARD_SD) ? "SDSC" : (cardType == CARD_SDHC) ? "SDHC" : "UNKNOWN");
	logf(Log_Info, TAG_SD, "SD Card Size: %lluMB", SD_MMC.cardSize() / (1024 * 1024));
	logf(Log_Info, TAG_SD, "Total space: %lluMB", SD_MMC.totalBytes() / (1024 * 1024));
	logf(Log_Info, TAG_SD, "Used space: %lluMB", SD_MMC.usedBytes() / (1024 * 1024));

	if (!SD_MMC.exists(LOGDIR) && !SD_MMC.mkdir(LOGDIR))
		log(Log_Error, TAG_SD, "Failed to create dir " + LOGDIR);
	if (!SD_MMC.exists(LogSessions::WORKDIR) && !SD_MMC.mkdir(LogSessions::WORKDIR))
		log(Log_Error, TAG_SD, "Failed to create dir " + LogSessions::WORKDIR);

	// Every session starts in the working directory under a running number -- the clock is
	// rarely set this early (NTP needs WLAN, GPS the phone). It gets its date and time after
	// the next boot, see LogSessions.h. The counter is the one the NO_TIME names always used.
	noTimeCounter.begin("NoTimeCounter");
	uint16_t c = noTimeCounter.getShort("Counter", 40);
	char stem[12];
	// A number whose files are still there (NVS reset or not writable) would append to the
	// previous session -- skip to a free one.
	for (uint8_t tries = 0; tries < 100; tries++, c++) {
		snprintf(stem, sizeof(stem), "_%04u", c);
		if (!SD_MMC.exists(LogSessions::WORKDIR + "/L" + stem + ".bin")) break;
	}
	snprintf(stem, sizeof(stem), "_%04u", c);
	noTimeCounter.putShort("Counter", c + 1);
	noTimeCounter.end();
	sessionStem = stem;
	const String base = LogSessions::WORKDIR + "/";
	file_data = base + "L" + stem + ".bin";
	file_debuglog = base + "D" + stem + ".log";
	file_nmealog = base + "N" + stem + ".log";
	file_hints = base + "T" + stem + ".txt";
	fdebug = SD_MMC.open(file_debuglog, FILE_APPEND, true);
	fdata  = SD_MMC.open(file_data, FILE_APPEND, true);
	fnmea  = SD_MMC.open(file_nmealog, FILE_APPEND, true);

	// The start line and the step reference must be taken at the same moment: a clock step
	// is measured against exactly this state.
	ClockSync::baseline();
	{
		struct timeval tv;
		gettimeofday(&tv, nullptr);
		const int64_t nowMs = static_cast<int64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
		char line[64];
		snprintf(line, sizeof(line), "start %lld %d %lu", (long long)nowMs, SessStats::isValidMs(nowMs) ? 1 : 0, (unsigned long)millis());
		appendHint(line);
	}

	logf(Log_Info, TAG_SD, "Session %s: %s%s", stem, file_data.c_str(), fdata ? "" : " - could not be opened, data is not logged!");
	LogSessions::startFinalizer(sessionStem);

	// TODO: save_coredump_to_littlefs() is disabled on purpose, not just unfinished -- enabling it
	// previously caused a boot-time crash loop: writing the coredump to LittleFS itself crashed
	// (cause not yet diagnosed), which created a *new* coredump, which then got "saved" (and
	// crashed again) on the next boot, forever. Before re-enabling: find and fix whatever in
	// save_coredump_to_littlefs()/the coredump-partition read path crashes, and confirm a
	// coredump partition already stuck in that state (from a past crash) can't retrigger the
	// loop even after the code fix -- may need to explicitly erase/invalidate it once
	// (esp_core_dump_image_check() / erasing the coredump partition) rather than just fixing
	// the write path.
	//save_coredump_to_littlefs(file_core);
}

// Method to send log messages as events
void BCLogger::sendLogEvent(LogType type, LogTag tag, const String& timeStr, const String& logMessage) {
	// The tag used to go in AsyncEventSource::send()'s second parameter, which is the SSE
	// *event name* -- so every line arrived as "event: BLE", "event: WIFI" and so on,
	// while the page listened with onmessage, which only ever sees unnamed "message"
	// events. The live log was therefore always empty.
	//
	// Everything the serial output shows travels in the payload now, so the web log can
	// render the same information and filter on it, and so none of these tables have to
	// be mirrored in JavaScript -- the pages read tags and icons from here and /log/get:
	//   t  tag name      i  tag icon     l  level name     ts timestamp    m  message
	String payload;
	payload.reserve(logMessage.length() + 72);
	payload += F("{\"t\":\"");
	payload += TAG_STRING[tag];
	payload += F("\",\"i\":\"");
	payload += TAG_SYMBOL[tag];
	payload += F("\",\"l\":\"");
	payload += LEVEL_STRING[type];
	payload += F("\",\"ts\":\"");
	// timeStr carries the ": " separator the serial/file sinks want appended; the web log
	// formats its own, so hand over just the stamp.
	size_t tsLen = timeStr.length();
	if (tsLen >= 2 && timeStr.charAt(tsLen - 2) == ':' && timeStr.charAt(tsLen - 1) == ' ') tsLen -= 2;
	for (size_t i = 0; i < tsLen; i++) payload += timeStr.charAt(i);
	payload += F("\",\"m\":\"");
	for (size_t i = 0; i < logMessage.length(); i++) {
		const char c = logMessage.charAt(i);
		// Minimal JSON escaping. Log lines carry file paths and quoted names, and a bare
		// quote would break the client's JSON.parse for that line.
		if (c == '"' || c == '\\') {
			payload += '\\';
			payload += c;
		} else if (static_cast<unsigned char>(c) >= 0x20) {
			payload += c;			// UTF-8 continuation bytes pass through untouched
		}
		// control characters (including the newlines logf() can produce) are dropped
	}
	payload += F("\"}");
	logevents.send(payload.c_str(), nullptr, millis());
}


void BCLogger::flushAllFiles() {
	// FlusherTask, Prio 5 -- a dedicated always-running task: flushes every 5 s and, in between,
	// writes the binary records queued by appendRecord().
	// log()'s writes to fdebug/fnmea and writeRecord()'s writes to fdata are mutex-protected
	// (xPrintMutex); flush() on those same File handles needs the same protection, or it can
	// race a concurrent write from another task. Same 100ms-timeout-and-skip pattern as log():
	// a missed flush just retries next cycle rather than risking blocking this task indefinitely.
	auto flushLocked = [this](File& f) {
		if (!f) return;
		if (xSemaphoreTake(xPrintMutex, static_cast<TickType_t>(100 / portTICK_PERIOD_MS)) == pdTRUE) {
			f.flush();
			xSemaphoreGive(xPrintMutex);
		} else {
			printf("%d: !!!!! File flush blocked !!!!!", millis());
		}
	};
	uint32_t cycle = 0;
	uint8_t rec[LogRec::RECORD_SIZE];
	const TickType_t period = pdMS_TO_TICKS(5000);
	TickType_t nextFlush = xTaskGetTickCount() + period;
	do {
		// Between flushes: write queued records as they come. The deadline is checked first, so
		// a steady stream of records can never postpone the flush and the memory report.
		// Producers wake this task with a notification (wakeFlusher()) after queueing
		// something, so it sleeps until there is work or the flush is due -- no polling.
		TickType_t now = xTaskGetTickCount();
		if ((int32_t)(nextFlush - now) > 0) {
			ulTaskNotifyTake(pdTRUE, nextFlush - now);
			while (recordQueue && xQueueReceive(recordQueue, rec, 0) == pdTRUE) writeRecord(rec);
			rawDrain();
			continue;
		}
		nextFlush = now + period;

		// Memory report. This replaces the old "Flush (DMA-capable heap free: ...)" line, which
		// had three problems: it logged at Log_Debug under TAG_SD, whose default level is
		// Log_Info, so it was never actually emitted; it reported only MALLOC_CAP_DMA, a subset
		// of the pool that really runs out; and it showed neither the largest free block
		// (fragmentation) nor the low-water mark. WebInstr::report() covers all of it under
		// TAG_WEB at Log_Info -- DMA included, since SD_MMC's transfer buffers still need it and
		// PSRAM cannot substitute (see the starvation incident in 3cb6293).
		// This task is the only place the web instrumentation is allowed to log from: recording
		// happens on async_tcp, which is watchdog-guarded and must never touch the SD card.
		WebInstr::report(++cycle % 12 == 0);		// stack watermarks every 12th cycle = 60s
		WebInstr::drain();
		checkClockStep();
		flushLocked(fdebug);
		yield();
		flushLocked(fnmea);
		yield();
		flushLocked(fdata);
		flushLocked(fraw[RAW_CAPTURE]);
		flushLocked(fraw[RAW_SNIPPETS]);
	} while (true);
}

// Time-hint file of the session (format in SessionStats.h). Opened per line: it gets a
// handful of lines per ride, not worth ~4 KB of internal RAM for an open file.
void BCLogger::appendHint(const char* line) {
	if (file_hints.isEmpty()) return;
	bool ok = false;
	if (xSemaphoreTake(xPrintMutex, static_cast<TickType_t>(500 / portTICK_PERIOD_MS)) == pdTRUE) {
		File f = SD_MMC.open(file_hints, FILE_APPEND, true);
		if (f) {
			ok = f.println(line) > 0;
			f.close();
		}
		xSemaphoreGive(xPrintMutex);
	}
	if (!ok) logf(Log_Warn, TAG_SD, "Time hint not written (%s): %s", file_hints.c_str(), line);
}

// FlusherTask: a clock step (NTP, GPS) goes into the hint file, so the timestamps written
// before it can be corrected when the session is finished after the next boot.
void BCLogger::checkClockStep() {
	ClockSync::Step st;
	if (!ClockSync::pollStep(st)) return;
	char line[80];
	snprintf(line, sizeof(line), "step %s %lld %lld %lu", ClockSync::sourceName(st.source),
	         (long long)st.offsetMs, (long long)st.newEpochMs, (unsigned long)st.uptimeMs);
	appendHint(line);
	logf(Log_Info, TAG_OP, "🕒 Clock stepped by %lld ms (source: %s)", (long long)st.offsetMs, ClockSync::sourceName(st.source));
}

// The files this boot writes to. Deleting or renaming an open file corrupts FAT here
// (CONFIG_FATFS_FS_LOCK is 0), so the web page must not.
bool BCLogger::isActiveSessionFile(const String& path) const {
	if (sessionStem.isEmpty() || !path.startsWith(LogSessions::WORKDIR + "/")) return false;
	return LogSessions::stemOf(path.c_str() + path.lastIndexOf('/') + 1) == sessionStem;
}

void BCLogger::printLoglevels() {
	for (int16_t line = -1; line < LogOutputMax; line++) {
		Serial.print(line == -1 ? "TAG" : line == 0 ? "Serial" : "File");
		Serial.print("\t|");
		for (uint16_t t = TAG_RAW_NMEA; t < LogTagMax; t++) {
			if (line == -1) {
				Serial.print(TAG_STRING[t]);
			} else {
				Serial.print(LEVEL_STRING[loglevel[line][t]]);
			}
			Serial.print("\t|");
			yield();
		}
		Serial.println();
	}
}
void BCLogger::handleCommand(const Command &cmd) {
	if (cmd.equals(logShow)) {
		printLoglevels();
		return;
	}

	if (cmd.equals(replayLog)) {
		Argument pathArg = cmd.getArgument("path");
		logf(Log_Info, TAG_OP, "Replay file %s", pathArg.getValue().c_str());
		replayFile(pathArg.getValue());
		return;
	}
	// Get arguments
	Argument argTag = cmd.getArgument("logtag");
	Argument argLevel = cmd.getArgument("loglevel");
	bool argSerial = cmd.getArgument("serial").isSet();
	bool argFile = cmd.getArgument("file").isSet();

	uint16_t t = TAG_RAW_NMEA;
	for (; t < LogTagMax; t++) {
		if (argTag.getValue().equalsIgnoreCase(TAG_STRING[t]))
			break;
	}
	if (t == LogTagMax) {
		logf(Log_Warn, TAG_OP, "Invalid log tag %s", argTag.getValue().c_str());
		return;
	}
	uint16_t l = Log_Debug;
	for (; l < LogTypeMax; l++) {
		if (argLevel.getValue().equalsIgnoreCase(LEVEL_STRING[l]))
			break;
	}
	if (l == LogTypeMax) {
		logf(Log_Warn, TAG_OP, "Invalid log level %s", argLevel.getValue().c_str());
		return;
	}
	setLogLevel(static_cast<LogType>(l), static_cast<LogTag>(t), argFile, argSerial);
}

void BCLogger::storeLoglevel(LogType level, LogTag tag, bool file, bool serial) {
	if (file) {
		logPrefs[OUT_File].begin("LFile");
		logPrefs[OUT_File].putLong(TAG_STRING[tag], loglevel[OUT_File][tag]);
		logPrefs[OUT_File].end();
	}
	if (serial) {
		logPrefs[OUT_Serial].begin("LSer");
		logPrefs[OUT_Serial].putLong(TAG_STRING[tag], loglevel[OUT_Serial][tag]);
		logPrefs[OUT_Serial].end();
	}
}

void BCLogger::setLogLevel(LogType level, LogTag tag, bool file, bool serial) {
	if (level >= LogTypeMax || level < 0) {
		logf(Log_Error, TAG_OP, "Wrong new log-level %d", level);
		return;
	}
	if (tag >= LogTagMax || tag < 0) {
		logf(Log_Error, TAG_OP, "Wrong new log-tag %d", tag);
		return;
	}
	logf(Log_Info, TAG_OP, "New log-level %s [%d] for tag %s [%d] for %s %s.", LEVEL_STRING[level], level, TAG_STRING[tag], tag, file ? "File" : "",
			serial ? "Serial" : "");
	if (file)
		loglevel[OUT_File][tag] = level;
	if (serial)
		loglevel[OUT_Serial][tag] = level;
	storeLoglevel(level, tag, file, serial);
}


BCLogger::LogType BCLogger::getLogLevel(LogTag tag, bool serial) {
	if (tag >= LogTagMax || tag < 0) {
		logf(Log_Error, TAG_OP, "getLogLevel for wrong log-tag %d", tag);
		return BCLogger::Log_Error;
	}
	return loglevel[serial?OUT_Serial:OUT_File][tag];
}


void BCLogger::log(LogType type, LogTag tag, const String& str) {
	bool write_file = checkLogLevel(type, tag, true) && !file_debuglog.isEmpty();  // empty during init
	bool write_serial = checkLogLevel(type, tag, false);
	if (!(write_file || write_serial))
		return;

	File f;
	if (write_file) {
		f = (tag == TAG_RAW_NMEA) ? fnmea : fdebug;
		if (!f) {
			Serial.printf("❌ 💾 Failed to use file %s for appending.\n", f.name());
			write_file = false;
			write_serial = true; // writing to file failed, so send it to serial
			//if (!write_serial) return;
		}
	}
	time_t now;
	time(&now);
	String format = String(DateFormatter::COMPAT) + ": ";
	String timeStr = DateFormatter::format(format.c_str(), now);

	String symbolStr = LEVEL_SYMBOL[type] + TAG_SYMBOL[tag] + String(" ");

	if (write_file) {
		if (xSemaphoreTake(xPrintMutex, static_cast<TickType_t>(100 / portTICK_PERIOD_MS)) == pdTRUE) {
			f.print(timeStr);
			f.println(str);
			xSemaphoreGive(xPrintMutex);
		} else {
			printf("%d: !!!!! File Log output blocked !!!!!", millis());
		}
	}

	yield();

	if (write_serial) {
		if (xSemaphoreTake(xPrintMutex, static_cast<TickType_t>(100 / portTICK_PERIOD_MS)) == pdTRUE) {
			{
				SerialConsole::Output out(console);		// above the half-typed command line
				Serial.print(symbolStr);
				Serial.print(timeStr);
				Serial.println(str);
			}
			sendLogEvent(type, tag, timeStr, str);
			xSemaphoreGive(xPrintMutex);
		} else {
			printf("%d: !!!!! Serial Log output blocked !!!!!", millis());
		}
	}
}

void BCLogger::logf(LogType type, LogTag tag, const char *format, ...) {
	if (!(checkLogLevel(type, tag, true) || checkLogLevel(type, tag, false))) return;
	va_list arg;
	va_start(arg, format);
	char temp[256];
	int len = vsnprintf(temp, sizeof(temp), format, arg);
	va_end(arg);
	if (len < 0) return;  // encoding error, nothing sensible to log
	if (static_cast<size_t>(len) >= sizeof(temp)) {		
		log(Log_Error, TAG_OP, "Logger: Log-String shortened from " + String(len) + " to " + String((int)sizeof(temp) - 1) + " byte");
	}
	log(type, tag, String(temp));
}


void BCLogger::nowEpoch(time_t& sec, uint16_t& ms) {
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	sec = tv.tv_sec;
	ms = tv.tv_usec / 1000;
}

void BCLogger::appendDataLog(float speed, float temp, float gradient, float distance, float height, uint8_t hr, uint8_t cadence, const SGpsFix& gps,
                             float gradBaro, float gradImu, uint8_t roadClass) {
	LogRec::Data b = {};
	nowEpoch(b.timestamp, b.timestampMs);
	b.speed = speed;
	b.temp = temp;
	b.grad = gradient;
	b.dist_m = distance;
	b.height = height;
	b.hr = hr;
	b.cadence = cadence;
	b.recordType = LogRec::TYPE_DATA;
	b.formatVersion = LogRec::FORMAT_VERSION;

	b.gpsFlags = (gps.valid ? LogRec::LOG_GPS_VALID : 0)
			| (gps.hasAltitude ? LogRec::LOG_GPS_HAS_ALTITUDE : 0)
			| (gps.hasSpeed ? LogRec::LOG_GPS_HAS_SPEED : 0)
			| (gps.hasBearing ? LogRec::LOG_GPS_HAS_BEARING : 0)
			| (gps.hasAccuracy ? LogRec::LOG_GPS_HAS_ACCURACY : 0);
	b.gpsLatitudeE7 = gps.latitudeE7;
	b.gpsLongitudeE7 = gps.longitudeE7;
	b.gpsAltitudeM = gps.altitudeM;
	b.gpsSpeedCms = gps.speedCms;
	b.gpsBearingDegX100 = gps.bearingDegX100;
	b.gpsAccuracyMX10 = gps.accuracyMX10;
	b.gpsFixAgeMs = gps.fixAgeMs;

	b.roadClass = roadClass;
	b.gradBaroX100 = LogRec::gradX100(gradBaro);
	b.gradImuX100 = LogRec::gradX100(gradImu);

	appendRecord(b);
}

void BCLogger::wakeFlusher() {
	if (flushTaskHandle) xTaskNotifyGive(flushTaskHandle);
}

bool BCLogger::enqueueRecord(const void* rec64) {
	if (recordQueue && xQueueSend(recordQueue, rec64, 0) == pdTRUE) {
		wakeFlusher();
		return true;
	}
	uint32_t dropped = ++recordsDropped;
	// No log() here: this runs in the ImuTask too, and a full queue means the card is already
	// slow -- printf only, and only now and then.
	if (dropped <= 3 || dropped % 100 == 0) printf("%lu: !!!!! Record queue full, %lu record(s) lost !!!!!\n", millis(), dropped);
	return false;
}

void BCLogger::writeRecord(const uint8_t* rec) {
	if (!fdata) {
		if (++recordsDropped == 1) log(Log_Warn, TAG_SD, "Data file not open - binary records are dropped");
		return;
	}
	// xPrintMutex serialises this logger's SD access (log() writes fdebug/fnmea under it from
	// any task). A short/failed write (SD full, card pulled mid-ride) is surfaced, not silently lost.
	if (xSemaphoreTake(xPrintMutex, static_cast<TickType_t>(100 / portTICK_PERIOD_MS)) == pdTRUE) {
		size_t written = fdata.write(rec, LogRec::RECORD_SIZE);
		xSemaphoreGive(xPrintMutex);
		if (written != LogRec::RECORD_SIZE) {
			recordsDropped++;
			log(Log_Error, TAG_SD, "Data record write incomplete: " + String(written) + "/" + String(LogRec::RECORD_SIZE) + " byte");
		} else {
			recordsWritten++;
		}
	} else {
		recordsDropped++;
		printf("%lu: !!!!! Data file write blocked, record lost !!!!!\n", millis());
	}
}

// ******************** raw byte streams (RawCapture.h) ********************

bool BCLogger::rawSend(uint8_t* msg, size_t len, uint8_t cmd, uint8_t stream) {
	msg[0] = cmd;
	msg[1] = stream;
	if (rawBuffer && len <= RAW_MAX_MSG && xMessageBufferSend(rawBuffer, msg, len, 0) == len) {
		wakeFlusher();
		return true;
	}
	rawDropped += len;
	return false;
}

void BCLogger::rawDrain() {
	if (!rawBuffer || !rawRx) return;
	size_t len;
	while ((len = xMessageBufferReceive(rawBuffer, rawRx, RAW_MAX_MSG, 0)) > 0) {
		rawHandle(rawRx, len);
	}
}

// The raw files belong to the session of the data log: L_0042.bin -> R_0042_01.bin
// (capture 1 of this session) and S_0042.bin (shock snippets). The shared stem keeps them
// in the same row of the log file list, and LogSessions renames them together with the
// data log (-> R_143012_01.bin) when the session is finished.
String BCLogger::rawPath(uint8_t stream) {
	if (file_data.isEmpty()) return String();
	int slash = file_data.lastIndexOf('/');
	int dot = file_data.lastIndexOf('.');
	String dir = file_data.substring(0, slash + 1);
	String stem = file_data.substring(slash + 2, dot > slash ? dot : file_data.length());	// without the 'L'
	if (stream == RAW_SNIPPETS) return dir + "S" + stem + ".bin";
	char nn[8];
	snprintf(nn, sizeof(nn), "_%02u", ++rawCaptureCount);
	return dir + "R" + stem + nn + ".bin";
}

void BCLogger::getRawFileName(RawStream s, char* out, size_t len) {
	portENTER_CRITICAL(&rawNameMux);
	snprintf(out, len, "%s", rawName[s]);
	portEXIT_CRITICAL(&rawNameMux);
}

void BCLogger::rawHandle(const uint8_t* msg, size_t len) {
	if (len < RAW_PREFIX || msg[1] > RAW_SNIPPETS) return;
	const uint8_t cmd = msg[0];
	const RawStream s = static_cast<RawStream>(msg[1]);
	const uint8_t* payload = msg + RAW_PREFIX;
	const size_t plen = len - RAW_PREFIX;
	File& f = fraw[s];

	// Same lock as all other SD access of this logger (see writeRecord()). A slow card only
	// delays this task; the producer's 8 KB message buffer absorbs > 3 s of capture.
	if (xSemaphoreTake(xPrintMutex, static_cast<TickType_t>(500 / portTICK_PERIOD_MS)) != pdTRUE) {
		rawDropped += plen;
		return;
	}
	String opened, closed;
	uint32_t closedBytes = 0;
	switch (cmd) {
	case RAW_CMD_OPEN: {
		if (f) {closed = f.name(); closedBytes = f.size(); f.close();}
		const String path = rawPath(s);
		if (path.length()) f = SD_MMC.open(path, FILE_WRITE, true);
		rawFilePath[s] = f ? path : String();
		if (f) {
			rawBytes[s] = f.write(payload, plen);
			opened = path;
			portENTER_CRITICAL(&rawNameMux);
			snprintf(rawName[s], sizeof(rawName[s]), "%s", path.substring(path.lastIndexOf('/') + 1).c_str());
			portEXIT_CRITICAL(&rawNameMux);
		} else {
			rawDropped += plen;
		}
		break;
	}
	case RAW_CMD_WRITE:
		if (!f && rawFilePath[s].length()) f = SD_MMC.open(rawFilePath[s], FILE_APPEND);
		if (f) {
			size_t w = f.write(payload, plen);
			rawBytes[s] += w;
			if (w != plen) rawDropped += plen - w;
		} else {
			rawDropped += plen;
		}
		break;
	case RAW_CMD_CLOSE:
		if (f) {closed = f.name(); closedBytes = f.size(); f.close();}
		break;
	}
	xSemaphoreGive(xPrintMutex);
	// log() takes xPrintMutex itself -- only after it was given back
	if (opened.length()) logf(Log_Info, TAG_SD, "Raw %s file opened: %s", s == RAW_CAPTURE ? "capture" : "shock snippet", opened.c_str());
	else if (cmd == RAW_CMD_OPEN) log(Log_Warn, TAG_SD, "Raw file could not be opened (no SD card / data log?) - data dropped");
	// The snippet file is closed after every snippet -- only the capture's close is news
	if (closed.length() && s == RAW_CAPTURE) logf(Log_Info, TAG_SD, "Raw file closed: %s, %u byte", closed.c_str(), closedBytes);
}

int16_t BCLogger::listDir(const String &dirname, uint8_t levels) {
	int16_t max_number = 0;
	logf(Log_Debug, TAG_SD, "📁 Listing directory: %s", dirname.c_str());

	File root = SD_MMC.open(dirname);
	if (!root) {
		log(Log_Error, TAG_SD, "Failed to open directory");
		return -1;
	}
	if (!root.isDirectory()) {
		log(Log_Error, TAG_SD, "Not a directory");
		root.close();
		return -1;
	}
	File file = root.openNextFile();
	while (file) {
		if (file.isDirectory()) {
			Serial.print("  DIR : ");
			Serial.println(file.name());
			if (levels) {
				listDir(String(file.name()), levels - 1);
			}
		} else {
			Serial.print("  FILE: ");
			Serial.print(file.name());
			Serial.print("  SIZE: ");
			Serial.println(file.size());
			unsigned int filenumber;
			if (sscanf(file.name(), "N%4u.BIN", &filenumber) == 1) {
				if (filenumber > max_number)
					max_number = filenumber;
			}
		}
		file = root.openNextFile();
	}
	root.close();
	return ++max_number;
}


namespace {

// ---------------------------------------------------------------------------
// Logfile listing helpers
// ---------------------------------------------------------------------------
// The on-disk layout already carries both things the listing wants to show, so
// neither has to be parsed out of a flat filename list:
//
//   /BIKECOMP/20260920/L_143012.bin     directory = date, first letter = type
//                     D_143012.log      L data (binary), D debug, N raw NMEA, S/R raw IMU
//                     I_143012.txt      I summary (shown as text), T time hints (not shown)
//                                       files of one session share the HHMMSS
//   /BIKECOMP/NO_TIME/L_0042.bin        session number: no time known (older: L0042.bin)
//   /BIKECOMP/CUR/L_0042.bin            running session and those not finished yet
//                                       (LogSessions.h), shown on top
//
// SD hands entries out in FAT order, so showing them newest-first means holding
// them in memory. Both collections are bounded and freed before the response is
// built -- this page must not turn into another unbounded buffer.
constexpr uint16_t MAX_DAYS            = 96;	// ~3 months of riding days
constexpr uint16_t MAX_FILES_PER_DAY   = 128;	// 3 files per session -> ~42 sessions
constexpr size_t   DAY_NAME_LEN        = 12;	// "20260920" / "NO_TIME"
constexpr size_t   FILE_NAME_LEN       = 24;	// "L_143012.bin"

struct LogEntry {
	char     name[FILE_NAME_LEN];
	uint32_t size;
};

struct DayName { char n[DAY_NAME_LEN]; };

// Sort key of a logfile: everything after the type letter and an optional '_', up to
// the extension or a further '_'. "L_143012.bin" -> "143012", "N7.log" -> "7". Compared by length first
// and only then lexicographically, so the zero-padded HHMMSS stamps AND the unpadded
// NO_TIME counters both end up in chronological order.
void timeKey(const char* name, char* out, size_t outLen) {
	const char* p = name;
	if (*p) p++;					// type letter
	if (*p == '_') p++;
	size_t i = 0;
	// stop at a second '_' too: R_143012_01.bin (raw capture 1 of session 143012) -> "143012"
	while (*p && *p != '.' && *p != '_' && i + 1 < outLen) out[i++] = *p++;
	out[i] = '\0';
}

int compareKey(const char* a, const char* b) {
	const size_t la = strlen(a), lb = strlen(b);
	if (la != lb) return (la < lb) ? -1 : 1;
	return strcmp(a, b);
}

// NO_TIME has no date and belongs at the bottom, below every dated day; the working
// directory with the running session goes on top.
int compareDay(const char* a, const char* b) {
	const bool ca = (strcmp(a, LogSessions::WORKDIR_NAME) == 0), cb = (strcmp(b, LogSessions::WORKDIR_NAME) == 0);
	if (ca != cb) return ca ? 1 : -1;
	const bool na = (strcmp(a, "NO_TIME") == 0), nb = (strcmp(b, "NO_TIME") == 0);
	if (na != nb) return na ? -1 : 1;			// "smaller" = shown last
	return strcmp(a, b);
}

// Insert into a descending-sorted array, evicting the smallest once full. Needed
// because the newest entries tend to come LAST out of the FAT iterator -- simply
// stopping at the cap would drop exactly the ones worth looking at.
template <typename T, typename Cmp>
void insertDesc(T* arr, uint16_t& count, uint16_t cap, const T& item, Cmp greaterThan) {
	uint16_t pos = count;
	while (pos > 0 && greaterThan(item, arr[pos - 1])) pos--;
	if (pos >= cap) return;						// smaller than everything we keep
	if (count < cap) count++;
	for (uint16_t i = count - 1; i > pos; i--) arr[i] = arr[i - 1];
	arr[pos] = item;
}

struct TypeInfo { const char* label; const char* badge; };

TypeInfo typeOf(char c) {
	switch (c) {
		case 'L': return { "DATA",  "badge-info"  };		// telemetry, the one you usually want
		case 'D': return { "DEBUG", "badge-debug" };
		case 'N': return { "FL Data", "badge-warn" };		// raw NMEA from the Forumslader
		case 'R': return { "IMU RAW", "badge-debug" };		// accelerometer capture on demand (RawCapture.h)
		case 'S': return { "SHOCKS", "badge-warn" };		// raw snippets around shocks
		default:  return { "?",     ""            };
	}
}

void formatSize(uint32_t bytes, char* out, size_t outLen) {
	if (bytes >= 1024UL * 1024UL)  snprintf(out, outLen, "%.1f MB", bytes / (1024.0 * 1024.0));
	else if (bytes >= 1024UL)      snprintf(out, outLen, "%.1f kB", bytes / 1024.0);
	else                           snprintf(out, outLen, "%u B", static_cast<unsigned>(bytes));
}

// "20260920" -> "2026-09-20"; anything else (NO_TIME, legacy names) passes through.
void formatDay(const char* day, char* out, size_t outLen) {
	if (strcmp(day, LogSessions::WORKDIR_NAME) == 0) {
		snprintf(out, outLen, "In progress");
	} else if (strlen(day) == 8 && strspn(day, "0123456789") == 8) {
		snprintf(out, outLen, "%.4s-%.2s-%.2s", day, day + 4, day + 6);
	} else {
		snprintf(out, outLen, "%s", day);
	}
}

// "143012" -> "14:30:12"; the unpadded NO_TIME counter is shown as "#7".
void formatTime(const char* key, char* out, size_t outLen) {
	if (strlen(key) == 6 && strspn(key, "0123456789") == 6) {
		snprintf(out, outLen, "%.2s:%.2s:%.2s", key, key + 2, key + 4);
	} else {
		snprintf(out, outLen, "#%s", key);
	}
}

}	// anonymous namespace

// Renders one <table> for a single day directory, one row per SESSION rather than per
// file: openFiles() creates the data/debug/FL logs together from the same timestamp, so
// they share their HHMMSS and belong together. dayDir is "" for files sitting directly in
// LOGDIR (legacy layout).
void BCLogger::appendDayTable(String &rc, const char* dayDir, uint32_t& totalFiles, uint32_t& totalBytes) {
	const String dirPath = dayDir[0] ? (LOGDIR + "/" + dayDir) : LOGDIR;
	const bool inWorkdir = strcmp(dayDir, LogSessions::WORKDIR_NAME) == 0;
	File dir = SD_MMC.open(dirPath);
	if (!dir || !dir.isDirectory()) {
		if (dir) dir.close();
		return;
	}

	LogEntry* entries = static_cast<LogEntry*>(malloc(sizeof(LogEntry) * MAX_FILES_PER_DAY));
	if (!entries) {
		rc += F("<p class=\"badge badge-error\">Out of memory while listing this day</p>\n");
		dir.close();
		return;
	}
	uint16_t count = 0;			// entries kept for display (capped)
	uint32_t dayFiles = 0;		// files actually shown, may exceed count when capped
	uint32_t dayBytes = 0;
	bool truncated = false;

	for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
		esp_task_wdt_reset();			// SD iteration is slow and this runs on async_tcp
		// Empty files are skipped outright: without a Forumslader the FL log stays 0 byte
		// for the whole ride, and an empty log is nothing anyone wants a row for. Applies
		// to every type, so a session that produced nothing disappears entirely.
		if (!f.isDirectory() && f.name()[0] != 'x' && f.size() > 0) {	// 'x' = manually hidden
			LogEntry e;
			snprintf(e.name, sizeof(e.name), "%s", f.name());
			e.size = f.size();
			dayBytes += e.size;
			dayFiles++;
			if (count >= MAX_FILES_PER_DAY) truncated = true;
			insertDesc(entries, count, MAX_FILES_PER_DAY, e, [](const LogEntry& a, const LogEntry& b) {
				char ka[FILE_NAME_LEN], kb[FILE_NAME_LEN];
				timeKey(a.name, ka, sizeof(ka));
				timeKey(b.name, kb, sizeof(kb));
				const int c = compareKey(ka, kb);
				return c ? (c > 0) : (strcmp(a.name, b.name) < 0);	// same session stays adjacent
			});
		}
		f.close();
		yield();
	}
	dir.close();

	if (count == 0) { free(entries); return; }
	totalFiles += dayFiles;
	totalBytes += dayBytes;

	char dayLabel[24], sizeBuf[16];
	formatDay(dayDir[0] ? dayDir : "(no date)", dayLabel, sizeof(dayLabel));
	formatSize(dayBytes, sizeBuf, sizeof(sizeBuf));

	rc += F("<h3 style=\"margin-top:22px;\">");
	rc += dayLabel;
	rc += F("</h3>\n<p class=\"eyebrow\">");
	rc += dayFiles;
	rc += F(" files, ");
	rc += sizeBuf;
	if (truncated) {
		rc += F(" &mdash; showing newest ");
		rc += count;
		rc += F(" only");
	}
	rc += F("</p>\n<table><thead><tr><th>Time</th><th>Logs</th><th></th></tr></thead><tbody>\n");

	// Badge order inside a row is fixed by type, not by the sort: telemetry first, then
	// debug, then the Forumslader log -- so the columns line up across rows.
	static const char kTypeOrder[] = { 'L', 'D', 'N', 'S', 'R' };

	for (uint16_t i = 0; i < count; ) {
		char key[FILE_NAME_LEN];
		timeKey(entries[i].name, key, sizeof(key));
		uint16_t j = i;
		while (j < count) {
			char k2[FILE_NAME_LEN];
			timeKey(entries[j].name, k2, sizeof(k2));
			if (strcmp(k2, key) != 0) break;
			j++;
		}

		// Summary (I_*.txt) and time hints (T_*.txt) aren't downloads worth a badge: the
		// summary is shown as text, both go with the row's DEL.
		SessStats::Summary sum;
		bool haveSum = false;
		String paths;			// JS array literal for the row's delete button
		for (uint16_t e = i; e < j; e++) {
			const char type = entries[e].name[0];
			if (type != 'I' && type != 'T') continue;
			if (type == 'I' && !haveSum) haveSum = LogSessions::readSummary(dirPath + "/" + entries[e].name, sum);
			if (paths.length()) paths += ',';
			paths += '\'';
			if (dayDir[0]) {paths += dayDir; paths += '/';}
			paths += entries[e].name;
			paths += '\'';
		}
		const bool running = inWorkdir && (String("_") + key) == sessionStem;

		char timeBuf[16];
		formatTime(key, timeBuf, sizeof(timeBuf));
		rc += F("<tr><td>");
		rc += timeBuf;
		if (haveSum && strcmp(sum.time, "corrected") == 0) {
			// Started without a clock, date and time found out later
			rc += F("<span title=\"time corrected afterwards (");
			rc += sum.source;
			rc += F(")\">*</span>");
		}
		if (running) rc += F("<br><span class=\"badge badge-info\">running</span>");
		else if (inWorkdir) rc += F("<br><span class=\"badge badge-debug\">not finished</span>");
		rc += F("</td><td>");

		String replayPath;
		for (char type : kTypeOrder) {
			for (uint16_t e = i; e < j; e++) {
				if (entries[e].name[0] != type) continue;
				const TypeInfo ti = typeOf(type);
				formatSize(entries[e].size, sizeBuf, sizeof(sizeBuf));
				String uri = dayDir[0] ? (String(dayDir) + "/") : String();
				uri += entries[e].name;
				// The badge IS the download link -- the filename adds nothing the time and
				// type don't already say, and it made every row twice as wide.
				rc += F("<a class=\"badge ");
				rc += ti.badge;
				rc += F("\" href=\"/log/");
				rc += uri;
				rc += F("\">");
				rc += ti.label;
				rc += ' ';
				rc += sizeBuf;
				rc += F("</a>");
				if (paths.length()) paths += ',';
				paths += '\'';
				paths += uri;
				paths += '\'';
				if (type == 'N') replayPath = uri;
			}
		}

		if (haveSum) {
			char line[320];
			sum.describe(line, sizeof(line));
			rc += F("<div class=\"sstat\">");
			rc += line;
			rc += F("</div>");
		}

		rc += F("</td><td style=\"white-space:nowrap;\">");
		if (!fileReplay && replayPath.length()) {
			rc += F("<a class=\"btn btn-ghost\" href=\"#\" onclick=\"replay('");
			rc += replayPath;
			rc += F("');return false;\">Replay</a> ");
		}
		if (!running) {
			rc += F("<a class=\"btn btn-ghost\" href=\"#\" onclick=\"del(this,[");
			rc += paths;
			rc += F("]);return false;\">DEL</a>");
		}
		rc += F("</td></tr>\n");
		i = j;
	}
	rc += F("</tbody></table>\n");
	free(entries);
}

uint16_t BCLogger::getAllFileLinks(String &rc) {
	// Reserve past CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL (4096) up front: the buffer then
	// starts in PSRAM instead of ramping there through hundreds of 16-byte reallocs in
	// the scarce internal heap. One cold allocation per manual page view.
	rc.reserve(24576);		// a full card easily reaches 25KB of table
	WebPage::begin(rc, "Logfiles", ".sstat{font-size:0.8rem;opacity:.8;margin-top:6px;line-height:1.35}");
	File root = SD_MMC.open(LOGDIR);
	if (!root || !root.isDirectory()) {
		if (root) root.close();
		log(Log_Warn, TAG_SD, "Logfile listing: " + LOGDIR + " missing or not a directory");
		rc += F("<p class=\"badge badge-error\">Cannot open ");
		rc += LOGDIR;
		rc += F("</p>\n");
		WebPage::end(rc);
		return 500;
	}

	DayName* days = static_cast<DayName*>(malloc(sizeof(DayName) * MAX_DAYS));
	if (!days) {
		root.close();
		rc += F("<p class=\"badge badge-error\">Out of memory</p>\n");
		WebPage::end(rc);
		return 500;
	}
	uint16_t dayCount = 0;
	bool hasLooseFiles = false;

	for (File f = root.openNextFile(); f; f = root.openNextFile()) {
		esp_task_wdt_reset();
		if (f.isDirectory()) {
			DayName d;
			snprintf(d.n, sizeof(d.n), "%s", f.name());
			insertDesc(days, dayCount, MAX_DAYS, d,
			           [](const DayName& a, const DayName& b) { return compareDay(a.n, b.n) > 0; });
		} else if (f.name()[0] != 'x') {
			hasLooseFiles = true;
		}
		f.close();
		yield();
	}
	root.close();

	uint32_t totalFiles = 0, totalBytes = 0;
	for (uint16_t i = 0; i < dayCount; i++) appendDayTable(rc, days[i].n, totalFiles, totalBytes);
	if (hasLooseFiles) appendDayTable(rc, "", totalFiles, totalBytes);
	free(days);

	// Summary goes at the BOTTOM on purpose: splicing it in at the top would mean
	// substring() + concat over the whole page, i.e. three full-size copies of a 20KB+
	// String -- the exact pattern this listing was rewritten to get rid of.
	if (totalFiles == 0) {
		rc += F("<p class=\"eyebrow\">No logfiles.</p>\n");
	} else {
		char sizeBuf[16];
		formatSize(totalBytes, sizeBuf, sizeof(sizeBuf));
		rc += F("<p class=\"eyebrow\" style=\"margin-top:18px;\">");
		rc += totalFiles;
		rc += F(" files on ");
		rc += dayCount + (hasLooseFiles ? 1 : 0);
		rc += F(" days, ");
		rc += sizeBuf;
		rc += F(" total</p>\n");
	}

	// Cleanup, delete and replay all used to navigate to a bare result page, throwing the
	// listing away for a one-line confirmation. They post in the background now and report
	// through the shared toast.
	rc += F("<a class=\"btn btn-ghost\" href=\"#\" onclick=\"cleanup();return false;\">Cleanup</a>\n");
	WebPage::end(rc,
		"function cleanup(){req('/cleanup','Cleanup done');}\n"
		"function replay(p){req('/replay/'+p,'Replay started');}\n"
		"async function del(el,ps){for(const p of ps){const r=await fetch('/del/'+p).catch(()=>null);"
		"if(!r||!r.ok){toast('Delete failed',true);return;}}\n"
		"const r=el.closest('tr');if(r)r.remove();toast('Deleted');}\n");
	return 200;
}

bool BCLogger::deleteFile(const String& path){
  // The running session's files are open, and the finalizer is moving the earlier ones.
  if (isActiveSessionFile(path) || (LogSessions::busy() && path.startsWith(LogSessions::WORKDIR + "/"))) {
	  logf(Log_Warn, TAG_SD, "File %s belongs to the running session or is being finished - not deleted.", path.c_str());
	  return false;
  }
  if (SD_MMC.remove(path.c_str())) {
	  logf(Log_Info, TAG_SD, "File %s deleted.", path.c_str());
	  return true;
  } else {
	  logf(Log_Warn, TAG_SD, "Deletion of file %s failed.", path.c_str());
	  return false;
  }
}

void BCLogger::autoCleanUp(const char* root_name) {
	  File root = SD_MMC.open(root_name);
	  if(!root){
	    log(Log_Warn, TAG_SD, F("🧹 autoCleanUp - Failed to open directory"));
	    return;
	  }
	  if(!root.isDirectory()){
		log(Log_Warn, TAG_SD, F("🧹 autoCleanUp - Not a directory"));
	    return;
	  }
	  cleanUp(root, 512);
}

void BCLogger::replayNextLine() {
	uint32_t next;
	do {
		String wasteStr = fileReplay.readStringUntil('$');	// waste string necessary for first line. It always helps to get in sync again if replay gets out of sync for whatever reason.
		String dataStr = "$" + fileReplay.readStringUntil('\n');
		String timeStr = fileReplay.readStringUntil(':').substring(11);
		uint32_t timeNext = timeStr.toInt();
		if (!timeNext) {
			bclog.log(Log_Warn, TAG_SD, "End of Replay file. Closing.");
			fileReplay.close();
			replayTicker.detach();		// there should be no timer running, but better safe than sorry
			stats.setConnected(false);
			return;
		}
		if (timeLasteLine == 0)	timeLasteLine = timeNext;
		next = timeNext-timeLasteLine;
		timeLasteLine = timeNext;
		if (next > 40) next -=40; //FIXME: Ugly workaround for 60seconds wrap
		bclog.logf(Log_Debug, TAG_SD, "Replay Strings:\n\tWaste:\t%s\n\tData:\t%s\n\tTime:\t%s (%d -> next in %d sec))",wasteStr.c_str(), dataStr.c_str(), timeStr.c_str(), timeNext, next );
#ifdef BC_FL_SUPPORT
		flparser.updateFromString(dataStr);
#endif
	} while (!next);
	replayTicker.once(next, +[](BCLogger* thisInstance){thisInstance->replayNextLine();}, this);
}

//TODO Also replay binary files
bool BCLogger::replayFile(const String &path) {
	if (fileReplay) {
		logf(Log_Warn, TAG_SD, "ReplayFile %s already open - closing it.", path.c_str());
		fileReplay.close();
		timeLasteLine = 0;
	}
	fileReplay = SD_MMC.open(path);
	if (!fileReplay) {
		logf(Log_Error, TAG_SD, "Failed to open replay file %s", path.c_str());
		return false;
	}
	stats.setConnected(true);	// Fake connect FL
	replayNextLine();
	return true;
}

bool BCLogger::cleanUp(File& root, uint32_t minsize) {
    File file = root.openNextFile();  // First file in root-DIR
    bool allFileDeleted = true;
	while (file) {
		esp_task_wdt_reset();
		const char type = file.name()[0];
		if (file.isDirectory() && LogSessions::WORKDIR == file.path()) {
			// The running session and the ones not finished yet: all open or in use
			allFileDeleted = false;
		} else if (!file.isDirectory() && (type == 'I' || type == 'T')) {
			// Summary and time hints are always small. They go with their data log (below).
			allFileDeleted = false;
		} else if (file.isDirectory()) {
			bool dirClean = cleanUp(file, minsize);
			allFileDeleted &= dirClean;
			bool delOk = SD_MMC.rmdir(file.path());
			logf(delOk ? Log_Info : Log_Warn, TAG_SD, "🧹%s Directory %s empty. Deleting - %s", delOk ? "✅":"❌", file.name(), delOk ? "OK":"failed!");
		} else if (file.size() < minsize) {
			const String path = file.path();
			const size_t size = file.size();
			file.close();
			bool delOk = SD_MMC.remove(path);
			logf(delOk ? Log_Info : Log_Warn, TAG_SD, "🧹%s File %s to small (%u). Deleting - %s", delOk ? "✅":"❌", path.c_str(), (unsigned)size, delOk ? "OK":"failed!");
			// A data log too short to keep takes its summary and time hints with it
			if (delOk && type == 'L') {
				const int slash = path.lastIndexOf('/');
				const String stem = LogSessions::stemOf(path.c_str() + slash + 1);
				const String dir = path.substring(0, slash + 1);
				if (stem.length()) {
					SD_MMC.remove(dir + "I" + stem + ".txt");
					SD_MMC.remove(dir + "T" + stem + ".txt");
				}
			}
		} else {
			allFileDeleted = false;
		}
		yield();
		file = root.openNextFile();   // next file in root-DIR
	}
	return allFileDeleted;
}


// Funktion zum Speichern des Coredumps auf LittleFS
void BCLogger::save_coredump_to_littlefs(const String& filename) {
    size_t coredump_size = 0;
    size_t coredump_addr = 0;

    // Core-Dump aus Flash holen
    esp_err_t err = esp_core_dump_image_get(&coredump_addr, &coredump_size);
    if (err != ESP_OK || coredump_addr == 0 || coredump_size == 0) {
        ESP_LOGE("COREDUMP", "Fehler beim Abrufen des Coredumps: %d", err);
        return;
    }

    // Cast in einen Zeiger auf uint8_t*
    uint8_t *coredump_data = reinterpret_cast<uint8_t *>(coredump_addr);

    // Datei öffnen und schreiben
    File file = LittleFS.open(filename, FILE_WRITE, true);  //TODO: This work because LittleFS is already initialized in WifiWebserver which is started BEFORE BCLogger.
    if (!file) {
        ESP_LOGE("COREDUMP", "Fehler beim Öffnen der Coredump-Datei");
        return;
    }
	logf(Log_Info, TAG_SD, "Existing core-dump found with %d byte at adress %x", coredump_size, coredump_addr);

    const size_t chunksize = 512;
	uint8_t buffer[chunksize];
    size_t offset = 0;

    const esp_partition_t* coredump_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, NULL);
    if (!coredump_part) {
        log(Log_Info, TAG_SD, "Keine Coredump-Partition gefunden!");
        return;
    }

    while (offset < coredump_size) {
        size_t to_read = std::min(chunksize, coredump_size - offset);
        esp_err_t err = esp_partition_read(coredump_part, offset, buffer, to_read);
        if (err != ESP_OK) {
            ESP_LOGE("COREDUMP", "Fehler beim Lesen des Coredumps: %d", err);
            break;
        }
        file.write(buffer, to_read);
        offset += to_read;
    }
    file.close();
    logf(Log_Info, TAG_SD, "Coredump %s erfolgreich gespeichert (%d Bytes)", filename.c_str(), offset);
}



