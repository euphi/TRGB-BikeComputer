/*
 * Host test for src/SessionStats.{h,cpp} -- no hardware, no PlatformIO. Build and run from
 * the repository root:
 *
 *   g++ -std=c++17 -O2 -Wall -Isrc src/SessionStats.cpp test/native_sessionstats/sessionstats_test.cpp -o /tmp/ss_test && /tmp/ss_test
 *
 * Exit code 0 = all checks passed.
 */

#include "SessionStats.h"
#include "LogRecords.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace SessStats;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static LogRec::Data dataRec(int64_t ms, float speed, float dist, bool gps = false) {
	LogRec::Data d = {};
	d.timestamp = ms / 1000;
	d.timestampMs = ms % 1000;
	d.speed = speed;
	d.dist_m = dist;
	d.recordType = LogRec::TYPE_DATA;
	d.formatVersion = LogRec::FORMAT_VERSION;
	if (gps) {d.gpsFlags = LogRec::LOG_GPS_VALID; d.gpsFixAgeMs = 800;}
	return d;
}

static void testHints() {
	printf("TimeHints\n");
	TimeHints h;
	h.parseLine("start 3000 0 3000");
	h.parseLine("");
	h.parseLine("garbage");
	CHECK(h.hasStart && h.startMs == 3000, "start");
	CHECK(!h.hasCorrection, "no correction before a step");
	int64_t t = 5000;
	CHECK(!h.correct(t) && t == 5000, "invalid time stays untouched without a correction");

	// The clock is first set by NTP (1970 -> 2026), then corrected by GPS twice.
	const int64_t real = 1790000000000LL;		// 2026-09-21
	h.parseLine("step ntp 1789999997000 1790000000000 3000");
	h.parseLine("step gps 1500 1790003600000 3603000");
	h.parseLine("step gps -700 1790007200000 7203000");
	CHECK(h.hasCorrection && h.correctionMs == real - 3000, "correction = first valid step: %lld", (long long)h.correctionMs);
	CHECK(!strcmp(h.source, "ntp"), "source: %s", h.source);
	CHECK(h.steps == 3, "steps: %u", h.steps);
	t = 4000;
	CHECK(h.correct(t) && t == real + 1000, "corrected 1970 time: %lld", (long long)t);
	t = real + 5;
	CHECK(h.correct(t) && t == real + 5, "valid time untouched");

	// Two steps while still invalid (e.g. a bogus source) add up.
	TimeHints h2;
	h2.parseLine("start 1000 0 1000");
	h2.parseLine("step ? 500 1500 1000");
	h2.parseLine("step gps 1789999998500 1790000000000 2000");
	CHECK(h2.correctionMs == 1789999998500LL + 500, "cumulative: %lld", (long long)h2.correctionMs);
	CHECK(!strcmp(h2.source, "gps"), "source of the valid step: %s", h2.source);

	// Clock already valid at the start (kept by the RTC over a soft reset), then NTP
	// corrects it by 99 s: drift, no correction for 1970 timestamps.
	TimeHints h4;
	h4.parseLine("start 1790502064197 1 4884");
	h4.parseLine("step ntp 99199 1790502168027 9514");
	CHECK(!h4.hasCorrection && h4.correctionMs == 0 && h4.steps == 1, "drift step: %lld", (long long)h4.correctionMs);

	// Source is sanitised (it ends up in the listing's HTML via the summary).
	TimeHints h3;
	h3.parseLine("step <b>x 1790000000000 1790000000000 1");
	CHECK(!strchr(h3.source, '<') && !strchr(h3.source, '>'), "sanitised: %s", h3.source);
}

static void testRecordTime() {
	printf("record time\n");
	LogRec::Data d = dataRec(1234567, 0, 0);
	CHECK(recordTimeMs(reinterpret_cast<uint8_t*>(&d)) == 1234567, "data time");
	setRecordTimeMs(reinterpret_cast<uint8_t*>(&d), 1790000000123LL);
	CHECK(d.timestamp == 1790000000 && d.timestampMs == 123, "data set");

	LogRec::Shock s = {};
	s.recordType = LogRec::TYPE_SHOCK;
	s.timestamp = 42; s.timestampMs = 999;
	CHECK(recordTimeMs(reinterpret_cast<uint8_t*>(&s)) == 42999, "shock time");
	setRecordTimeMs(reinterpret_cast<uint8_t*>(&s), 1790000000001LL);
	CHECK(s.timestamp == 1790000000 && s.timestampMs == 1, "shock set");
}

static void testStats() {
	printf("Stats\n");
	TimeHints h;
	h.parseLine("start 10000 0 10000");
	h.parseLine("step gps 1789999990000 1790000000000 10000");		// 10 s after boot

	Stats st;
	// 1 h ride, record every 5 s, 20 km/h = 27.78 m per record. Clock gets set at 10 s.
	const int64_t base = 1789999990000LL;
	float dist = 0;
	int64_t t = 5000;
	for (int i = 0; i < 720; i++, t += 5000) {
		const bool stopped = (i >= 100 && i < 160);			// 5 min at the traffic light
		const float v = stopped ? 0.0f : 20.0f;
		if (!stopped && i) dist += 20.0f / 3.6f * 5.0f;
		const int64_t ts = t < 10000 ? t : base + t;			// before/after the clock was set
		LogRec::Data d = dataRec(ts, v, dist, i % 2 == 0);
		int64_t corrected = 0;
		st.add(reinterpret_cast<uint8_t*>(&d), h, &corrected);
		CHECK(corrected == base + t, "corrected time of record %d: %lld", i, (long long)corrected);
		if (i == 300) {											// a reset of dist_m must not count
			LogRec::Data bad = dataRec(base + t + 1, 20.0f, 1e7f);
			st.add(reinterpret_cast<uint8_t*>(&bad), h);
			LogRec::Data nan = dataRec(base + t + 2, NAN, NAN);
			st.add(reinterpret_cast<uint8_t*>(&nan), h);
		}
	}
	for (int i = 0; i < 360; i++) {
		LogRec::RoadQuality rq = {};
		rq.recordType = LogRec::TYPE_ROAD_QUALITY;
		rq.timestamp = (base + 20000) / 1000 + i * 10;
		rq.roadClass = i % 3 ? 2 : 0;
		rq.eventsSuppressed = i == 5 ? 3 : 0;
		st.add(reinterpret_cast<uint8_t*>(&rq), h);
	}
	for (int i = 0; i < 7; i++) {
		LogRec::Shock s = {};
		s.recordType = LogRec::TYPE_SHOCK;
		s.timestamp = (base + 100000) / 1000 + i;
		st.add(reinterpret_cast<uint8_t*>(&s), h);
	}
	const uint8_t reasons[] = {LogRec::LABEL_CHANGE, LogRec::LABEL_REFRESH, LogRec::LABEL_CHANGE, LogRec::LABEL_CAPTURE_START};
	for (uint8_t r : reasons) {
		LogRec::Label l = {};
		l.recordType = LogRec::TYPE_LABEL;
		l.timestamp = (base + 200000) / 1000;
		l.reason = r;
		l.surface = LogRec::SURFACE_GRAVEL;
		st.add(reinterpret_cast<uint8_t*>(&l), h);
	}

	printf("  dist %.0f m, moving %u s, vmax %.1f, data %u, corrected %u, invalid %u\n",
	       st.distM, st.movingMs / 1000, st.vmaxKmh, st.nData, st.nTimeCorrected, st.nTimeInvalid);
	CHECK(fabs(st.distM - dist) < 1.0, "distance %.1f vs %.1f", st.distM, dist);
	CHECK(st.nData == 722, "data count %u", st.nData);
	CHECK(st.nRq == 360 && st.nRqRated == 240, "rq %u rated %u", st.nRq, st.nRqRated);
	CHECK(st.nShock == 7 && st.nShockSuppressed == 3, "shocks %u (+%u)", st.nShock, st.nShockSuppressed);
	CHECK(st.nLabel == 2, "labels %u", st.nLabel);
	CHECK(st.nTimeCorrected == 1 && st.nTimeInvalid == 0, "time corrected %u invalid %u", st.nTimeCorrected, st.nTimeInvalid);
	CHECK(st.nGps == 360, "gps %u", st.nGps);
	// 720 records, 60 stopped: moving = (720 - 60 - 1) steps of 5 s (the first has no predecessor),
	// plus the two extra records around i == 300 (same total duration).
	CHECK(llabs((int64_t)st.movingMs - 659LL * 5000) <= 5000, "moving %u ms", st.movingMs);
	CHECK(st.vmaxKmh == 20.0f, "vmax %.1f", st.vmaxKmh);
	CHECK(st.firstMs == base + 5000, "first %lld", (long long)st.firstMs);

	Summary s;
	s.fromStats(st, h);
	s.nRaw = 2;
	s.lCorrected = true;
	CHECK(!strcmp(s.time, "corrected"), "time %s", s.time);
	CHECK(s.vavgKmh > 19.9f && s.vavgKmh < 20.2f, "vavg %.2f", s.vavgKmh);

	char buf[512];
	const size_t n = s.format(buf, sizeof(buf));
	CHECK(n > 0, "format");
	printf("%s", buf);
	Summary r;
	CHECK(r.parse(buf), "parse");
	CHECK(r.distM == s.distM && r.durS == s.durS && r.moveS == s.moveS && r.nShock == 7 && r.nRaw == 2
	      && r.startS == s.startS && r.correctionMs == s.correctionMs && r.lCorrected && !strcmp(r.time, "corrected")
	      && !strcmp(r.source, "gps"), "round trip");
	CHECK(s.format(buf, 40) == 0, "too small buffer reported");

	char line[256];
	r.describe(line, sizeof(line));
	printf("  %s\n", line);
	CHECK(strstr(line, "km") && strstr(line, "7 shocks") && strstr(line, "1:00 h (54 min moving)"), "describe");
	r.describe(line, sizeof(line), false);
	printf("  %s\n", line);
	CHECK(!strchr(line, '&'), "plain text without entities");
	char tiny[16];
	r.describe(tiny, sizeof(tiny));
	CHECK(strlen(tiny) < sizeof(tiny), "describe truncates safely");

	// A session whose clock was never set: no correction, time "none".
	TimeHints none;
	none.parseLine("start 1000 0 1000");
	Stats sn;
	LogRec::Data d = dataRec(5000, 10.0f, 0);
	sn.add(reinterpret_cast<uint8_t*>(&d), none);
	Summary snS;
	snS.fromStats(sn, none);
	CHECK(!strcmp(snS.time, "none") && sn.nTimeInvalid == 1, "never set: %s", snS.time);

	Summary junk;
	CHECK(!junk.parse("no key value here\n\n"), "junk");
}

int main() {
	testHints();
	testRecordTime();
	testStats();
	printf(failures ? "\n%d FAILURE(S)\n" : "\nall passed\n", failures);
	return failures ? 1 : 0;
}
