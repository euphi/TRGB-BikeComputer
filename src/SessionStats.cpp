/*
 * SessionStats.cpp
 *
 * See SessionStats.h. Deliberately free of Arduino/FS dependencies so it builds on the host.
 */

#include "SessionStats.h"
#include "LogRecords.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace SessStats {

namespace {

// Same threshold the GPS icon uses (Statistics.cpp): well beyond TrailBridge's 5 s heartbeat.
constexpr uint32_t GPS_STALE_MS = 30000;

size_t msOffset(const uint8_t* rec) {
	return rec[LogRec::TYPE_OFFSET] == LogRec::TYPE_DATA ? offsetof(LogRec::Data, timestampMs) : offsetof(LogRec::RoadQuality, timestampMs);
}

void copySource(char* dst, const char* src) {
	size_t i = 0;
	for (; src[i] && i + 1 < SRC_LEN; i++) {
		const char c = src[i];
		dst[i] = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '?' ? c : '?';
	}
	dst[i] = '\0';
}

}	// namespace

int64_t recordTimeMs(const uint8_t* rec) {
	const int64_t s = get<int64_t>(rec, 0);
	const uint16_t ms = get<uint16_t>(rec, msOffset(rec));
	return s * 1000 + (ms < 1000 ? ms : 0);
}

void setRecordTimeMs(uint8_t* rec, int64_t epochMs) {
	int64_t s = epochMs / 1000;
	int64_t ms = epochMs % 1000;
	if (ms < 0) {ms += 1000; s--;}
	put<int64_t>(rec, 0, s);
	put<uint16_t>(rec, msOffset(rec), static_cast<uint16_t>(ms));
}

// ---------------------------------------------------------------- TimeHints

void TimeHints::parseLine(const char* line) {
	char src[16] = "";
	long long a = 0, b = 0, c = 0;
	int valid = 0;
	if (sscanf(line, "start %lld %d %lld", &a, &valid, &c) >= 2) {
		hasStart = true;
		startMs = a;
		return;
	}
	if (sscanf(line, "step %15s %lld %lld %lld", src, &a, &b, &c) >= 3) {
		steps++;
		// The clock was already valid before this step (set earlier, or kept by the RTC over
		// a soft reset): a drift correction, nothing a 1970 timestamp needs.
		if (hasCorrection || isValidMs(b - a)) return;
		correctionMs += a;
		// The clock became valid with this step -- from here on nothing needs correcting.
		if (isValidMs(b)) {
			hasCorrection = true;
			copySource(source, src);
		}
	}
	// anything else (empty line, a later extension) is ignored
}

bool TimeHints::correct(int64_t& epochMs) const {
	if (isValidMs(epochMs)) return true;
	if (!hasCorrection) return false;
	epochMs += correctionMs;
	return isValidMs(epochMs);
}

// ---------------------------------------------------------------- Stats

void Stats::add(const uint8_t* rec, const TimeHints& hints, int64_t* correctedMs) {
	int64_t t = recordTimeMs(rec);
	const bool wasValid = isValidMs(t);
	if (hints.correct(t)) {
		if (!wasValid) nTimeCorrected++;
	} else {
		nTimeInvalid++;
	}
	if (correctedMs) *correctedMs = t;

	if (!hasTime) {firstMs = lastMs = t; hasTime = true;}
	if (t < firstMs) firstMs = t;
	if (t > lastMs) lastMs = t;

	switch (rec[LogRec::TYPE_OFFSET]) {
	case LogRec::TYPE_DATA: {
		nData++;
		const float dist = get<float>(rec, offsetof(LogRec::Data, dist_m));
		float speed = get<float>(rec, offsetof(LogRec::Data, speed));
		const uint8_t gpsFlags = rec[offsetof(LogRec::Data, gpsFlags)];		// GPS share only
		const uint32_t fixAge = get<uint32_t>(rec, offsetof(LogRec::Data, gpsFixAgeMs));
		const bool gpsFresh = (gpsFlags & LogRec::LOG_GPS_VALID) && fixAge <= GPS_STALE_MS;
		if (gpsFresh) nGps++;
		// Wheel speed only, the same source as dist_m -- so the average (distance / moving
		// time) is consistent. The phone's GPS speed jitters by several km/h at standstill.
		if (!std::isfinite(speed) || speed < 0.0f || speed > 150.0f) speed = 0.0f;
		if (speed > vmaxKmh) vmaxKmh = speed;

		if (!std::isfinite(dist)) break;
		if (hasPrev) {
			const int64_t dt = t - prevMs;
			if (dt > 0 && dt <= MAX_STEP_MS && speed >= MOVING_KMH) movingMs += static_cast<uint32_t>(dt);
			// dist_m counts up from boot. Only plausible forward steps (< 30 m/s over the
			// gap) are summed. A step back is a reset: start over from the new value. An
			// implausible jump forward is a garbage value: skipped without moving the base,
			// so it neither adds a marathon nor costs the next interval.
			const float d = dist - prevDist;
			const double dtS = dt > 0 ? dt / 1000.0 : 0.0;
			if (d > 30.0 * dtS + 50.0) break;
			if (d > 0.0f) distM += d;
		}
		prevDist = dist;
		prevMs = t;
		hasPrev = true;
		break;
	}
	case LogRec::TYPE_ROAD_QUALITY:
		nRq++;
		if (rec[offsetof(LogRec::RoadQuality, roadClass)] != 0) nRqRated++;
		break;
	case LogRec::TYPE_SHOCK:
		nShock++;
		break;
	case LogRec::TYPE_LABEL:
		// Only actual changes; the 60 s refreshes and capture markers repeat a known label.
		if (rec[offsetof(LogRec::Label, reason)] == LogRec::LABEL_CHANGE && rec[offsetof(LogRec::Label, surface)] != 0) nLabel++;
		break;
	default:
		nOther++;
		break;
	}
	// The road-quality interval records how many shocks the rate limit swallowed.
	if (rec[LogRec::TYPE_OFFSET] == LogRec::TYPE_ROAD_QUALITY) {
		nShockSuppressed += get<uint16_t>(rec, offsetof(LogRec::RoadQuality, eventsSuppressed));
	}
}

// ---------------------------------------------------------------- Summary

void Summary::fromStats(const Stats& s, const TimeHints& h) {
	startS = s.hasTime ? s.firstMs / 1000 : 0;
	endS = s.hasTime ? s.lastMs / 1000 : 0;
	if (!s.hasTime || s.nTimeInvalid) snprintf(time, sizeof(time), "none");
	else if (s.nTimeCorrected) snprintf(time, sizeof(time), "corrected");
	else snprintf(time, sizeof(time), "ok");
	copySource(source, h.hasCorrection ? h.source : "");
	correctionMs = h.hasCorrection ? h.correctionMs : 0;
	distM = static_cast<uint32_t>(s.distM + 0.5);
	durS = s.hasTime ? static_cast<uint32_t>((s.lastMs - s.firstMs) / 1000) : 0;
	moveS = s.movingMs / 1000;
	vmaxKmh = s.vmaxKmh;
	vavgKmh = s.movingMs ? static_cast<float>(s.distM / (s.movingMs / 1000.0) * 3.6) : 0.0f;
	nData = s.nData; nRq = s.nRq; nRqRated = s.nRqRated; nShock = s.nShock;
	nShockSuppressed = s.nShockSuppressed; nLabel = s.nLabel; nGps = s.nGps;
}

size_t Summary::format(char* buf, size_t len) const {
	const int n = snprintf(buf, len,
		"v=%u\nstart=%lld\nend=%lld\ntime=%s\nsrc=%s\ncorr_ms=%lld\nl_corrected=%d\n"
		"dist_m=%lu\ndur_s=%lu\nmove_s=%lu\nvmax_kmh=%.1f\nvavg_kmh=%.1f\n"
		"data=%lu\ngps=%lu\nrq=%lu\nrq_rated=%lu\nshocks=%lu\nshocks_supp=%lu\nlabels=%lu\nraw=%u\n",
		version, (long long)startS, (long long)endS, time, source, (long long)correctionMs, lCorrected ? 1 : 0,
		(unsigned long)distM, (unsigned long)durS, (unsigned long)moveS, vmaxKmh, vavgKmh,
		(unsigned long)nData, (unsigned long)nGps, (unsigned long)nRq, (unsigned long)nRqRated,
		(unsigned long)nShock, (unsigned long)nShockSuppressed, (unsigned long)nLabel, nRaw);
	return (n > 0 && static_cast<size_t>(n) < len) ? static_cast<size_t>(n) : 0;
}

bool Summary::parse(const char* text) {
	bool any = false;
	const char* p = text;
	while (p && *p) {
		const char* eol = strchr(p, '\n');
		const size_t n = eol ? static_cast<size_t>(eol - p) : strlen(p);
		char line[48];
		if (n < sizeof(line)) {
			memcpy(line, p, n);
			line[n] = '\0';
			char* eq = strchr(line, '=');
			if (eq) {
				*eq = '\0';
				const char* k = line;
				const char* v = eq + 1;
				const unsigned long u = strtoul(v, nullptr, 10);
				any = true;
				if (!strcmp(k, "v")) version = static_cast<uint16_t>(u);
				else if (!strcmp(k, "start")) startS = strtoll(v, nullptr, 10);
				else if (!strcmp(k, "end")) endS = strtoll(v, nullptr, 10);
				else if (!strcmp(k, "time")) snprintf(time, sizeof(time), "%s", v);
				else if (!strcmp(k, "src")) copySource(source, v);
				else if (!strcmp(k, "corr_ms")) correctionMs = strtoll(v, nullptr, 10);
				else if (!strcmp(k, "l_corrected")) lCorrected = u != 0;
				else if (!strcmp(k, "dist_m")) distM = u;
				else if (!strcmp(k, "dur_s")) durS = u;
				else if (!strcmp(k, "move_s")) moveS = u;
				else if (!strcmp(k, "vmax_kmh")) vmaxKmh = strtof(v, nullptr);
				else if (!strcmp(k, "vavg_kmh")) vavgKmh = strtof(v, nullptr);
				else if (!strcmp(k, "data")) nData = u;
				else if (!strcmp(k, "gps")) nGps = u;
				else if (!strcmp(k, "rq")) nRq = u;
				else if (!strcmp(k, "rq_rated")) nRqRated = u;
				else if (!strcmp(k, "shocks")) nShock = u;
				else if (!strcmp(k, "shocks_supp")) nShockSuppressed = u;
				else if (!strcmp(k, "labels")) nLabel = u;
				else if (!strcmp(k, "raw")) nRaw = static_cast<uint16_t>(u);
				// unknown key: a later version, ignored
			}
		}
		p = eol ? eol + 1 : nullptr;
	}
	return any;
}

size_t Summary::describe(char* buf, size_t len, bool html) const {
	if (!len) return 0;
	buf[0] = '\0';
	size_t used = 0;
	auto add = [&](const char* fmt, auto... args) {
		if (used + 1 >= len) return;
		const int n = snprintf(buf + used, len - used, fmt, args...);
		if (n > 0) used += static_cast<size_t>(n) < len - used ? static_cast<size_t>(n) : len - used - 1;
	};
	auto duration = [&](uint32_t s) {
		if (s >= 3600) add("%lu:%02lu h", (unsigned long)(s / 3600), (unsigned long)(s / 60 % 60));
		else if (s >= 60) add("%lu min", (unsigned long)(s / 60));
		else add("%lu s", (unsigned long)s);
	};
	const char* sep = html ? " &middot; " : " \xC2\xB7 ";		// middle dot
	const char* avg = html ? "&Oslash;" : "\xC3\x98";			// Ø
	add("%.1f km%s", distM / 1000.0, sep);
	duration(durS);
	if (moveS) {
		add(" (");
		duration(moveS);
		add(" moving)%s%s %.1f km/h", sep, avg, vavgKmh);
	}
	if (vmaxKmh > 0.0f) add("%smax %.1f km/h", sep, vmaxKmh);
	if (nData && nGps) add("%sGPS %lu%%", sep, (unsigned long)(nGps * 100UL / nData));
	if (nRq || nShock || nLabel) {
		add("%sRQ %lu intervals, %lu shocks", sep, (unsigned long)nRq, (unsigned long)nShock);
		if (nShockSuppressed) add(" (+%lu suppressed)", (unsigned long)nShockSuppressed);
		if (nLabel) add(", %lu labels", (unsigned long)nLabel);
	}
	if (nRaw) add("%s%u raw captures", sep, nRaw);
	return used;
}

}	// namespace SessStats
