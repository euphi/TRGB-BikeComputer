/*
 * Host test for src/ClimbProfile.{h,cpp} -- no hardware, no PlatformIO. Build and run from
 * the repository root:
 *
 *   g++ -std=c++17 -O2 -Wall -Isrc src/ClimbProfile.cpp test/native_climb/climb_test.cpp -o /tmp/climb_test && /tmp/climb_test
 *
 * Exit code 0 = all checks passed. The rides are fed the way TrailBridge does it
 * (RouteNavigator/ElevationProfile there): a window of at most 200 steps from the rider
 * when the next 200 m rise by 4 %, the next one past the middle, PROFILE_NONE below 2 %.
 */

#include "ClimbProfile.h"
#include "BikeProfileProtocol.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace Climb;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NEAR(a, b, tol) (fabsf((a) - (b)) <= (tol))

static const int STEP = 25;

// A route as TrailBridge has it: altitudes on the 25 m raster
struct Route {
	std::vector<float> alt;
	explicit Route(float startAlt) {alt.push_back(startAlt);}
	Route& add(float lengthM, float gradePct) {
		for (int i = 0; i < lengthM / STEP; i++) alt.push_back(alt.back() + STEP * gradePct / 100.0f);
		return *this;
	}
	float totalM() const {return (alt.size() - 1) * (float)STEP;}
	float altAt(float d) const {
		float x = d / STEP;
		if (x <= 0) return alt.front();
		if (x >= alt.size() - 1) return alt.back();
		int i = (int)x;
		return alt[i] + (x - i) * (alt[i + 1] - alt[i]);
	}
	float gradeAhead(float from, float look) const {
		float end = std::min(from + look, totalM());
		if (end - from < 50) return 0;
		return (altAt(end) - altAt(from)) / (end - from);
	}
	// ElevationProfile.slice() + ProfileFrameEncoder.encode()
	std::vector<uint8_t> slice(float fromM, int maxSteps, float* startAlong = nullptr, int* length = nullptr) const {
		int i0 = (int)lroundf(fromM / STEP);
		int steps = std::min(maxSteps, (int)alt.size() - 1 - i0);
		if (steps < 8) return {};
		std::vector<uint8_t> f = {PROFILE_PROTOCOL_VERSION, PROFILE_MSG_PROFILE_UPDATE};
		uint32_t remaining = (uint32_t)lroundf(totalM() - i0 * STEP);
		f.insert(f.end(), {PROFILE_TAG_START_REMAINING_DISTANCE_M, 4, (uint8_t)remaining, (uint8_t)(remaining >> 8), (uint8_t)(remaining >> 16), (uint8_t)(remaining >> 24)});
		f.insert(f.end(), {PROFILE_TAG_STEP_M, 1, (uint8_t)STEP});
		int prev = (int)lroundf(alt[i0] * 10);
		f.insert(f.end(), {PROFILE_TAG_BASE_ALT_DM, 2, (uint8_t)prev, (uint8_t)(prev >> 8)});
		f.insert(f.end(), {PROFILE_TAG_DELTAS_DM, (uint8_t)steps});
		for (int k = 0; k < steps; k++) {
			int cur = (int)lroundf(alt[i0 + k + 1] * 10);
			int d = std::max(-127, std::min(127, cur - prev));
			f.push_back((uint8_t)(int8_t)d);
			prev += d;
		}
		if (startAlong) *startAlong = i0 * STEP;
		if (length) *length = steps * STEP;
		return f;
	}
};

// RouteNavigator.onFix(): decides what goes to the BikeComputer
struct Phone {
	const Route& route;
	Tracker& tracker;
	bool climbing = false, sent = false;
	float sentStart = 0;
	int sentLength = 0;
	int frames = 0, nones = 0;
	Phone(const Route& r, Tracker& t) : route(r), tracker(t) {}

	void cut(float progress) {
		std::vector<uint8_t> f = route.slice(progress, 200, &sentStart, &sentLength);
		if (f.empty()) return;
		sent = true;
		frames++;
		CHECK(tracker.feedFrame(f.data(), f.size()) == Tracker::FRAME_PROFILE, "frame rejected at %.0f m", progress);
	}
	void onFix(float progress) {
		float grade = route.gradeAhead(progress, 200);
		if (!climbing && grade >= 0.04f) {
			climbing = true;
			cut(progress);
		} else if (climbing && grade < 0.02f) {
			climbing = false;
			if (sent) {
				const uint8_t none[] = {PROFILE_PROTOCOL_VERSION, PROFILE_MSG_PROFILE_NONE};
				tracker.feedFrame(none, sizeof(none));
				nones++;
			}
			sent = false;
		} else if (climbing && sent && progress > sentStart + sentLength / 2.0f && sentStart + sentLength < route.totalM() - STEP) {
			cut(progress);
		}
		tracker.setRemaining(route.totalM() - progress);		// the nav frame's REMAINING_DISTANCE_M
	}
};

static void testFrames() {
	printf("Frames\n");
	Tracker t;
	// The example of PROTOCOL.md: 4200 m remaining, 34.5 m, +1.2 / +1.3 / -0.2 m
	const uint8_t ex[] = {0x01, 0x01, 0x01, 0x04, 0x68, 0x10, 0x00, 0x00, 0x02, 0x01, 0x19, 0x03, 0x02, 0x59, 0x01, 0x04, 0x03, 0x0C, 0x0D, 0xFE};
	CHECK(t.feedFrame(ex, sizeof(ex)) == Tracker::FRAME_PROFILE, "example frame");
	CHECK(t.pointCount() == 4 && t.stepM() == 25 && t.startRemainingM() == 4200, "points %u step %u start %u", t.pointCount(), t.stepM(), (unsigned)t.startRemainingM());
	CHECK(t.altitudesDm()[0] == 345 && t.altitudesDm()[1] == 357 && t.altitudesDm()[2] == 370 && t.altitudesDm()[3] == 368, "altitudes");
	CHECK(NEAR(t.altitudeAtM(37.5f), 36.35f, 0.01f), "interpolated %.2f", t.altitudeAtM(37.5f));
	CHECK(t.status().hasProfile && !t.status().positionValid, "no position yet");

	// Unknown tag in front (skipped by length), negative base altitude
	const uint8_t unk[] = {0x01, 0x01, 0x7F, 0x03, 0xAA, 0xBB, 0xCC, 0x01, 0x04, 0x10, 0x27, 0x00, 0x00, 0x02, 0x01, 0x19, 0x03, 0x02, 0xF6, 0xFF, 0x04, 0x02, 0x05, 0xFB};
	Tracker u;
	CHECK(u.feedFrame(unk, sizeof(unk)) == Tracker::FRAME_PROFILE, "unknown tag");
	CHECK(u.pointCount() == 3 && u.altitudesDm()[0] == -10 && u.altitudesDm()[2] == -10 && u.startRemainingM() == 10000, "unknown tag content");

	const uint32_t v = t.version();
	const uint8_t trunc[] = {0x01, 0x01, 0x01, 0x04, 0x68, 0x10, 0x00, 0x00, 0x02, 0x01, 0x19, 0x03, 0x02, 0x59, 0x01, 0x04, 0x09, 0x0C};
	CHECK(t.feedFrame(trunc, sizeof(trunc)) == Tracker::FRAME_INVALID, "truncated TLV");
	const uint8_t v2[] = {0x02, 0x01};
	CHECK(t.feedFrame(v2, sizeof(v2)) == Tracker::FRAME_INVALID, "version 2");
	const uint8_t noDeltas[] = {0x01, 0x01, 0x01, 0x04, 0x68, 0x10, 0x00, 0x00, 0x02, 0x01, 0x19, 0x03, 0x02, 0x59, 0x01};
	CHECK(t.feedFrame(noDeltas, sizeof(noDeltas)) == Tracker::FRAME_INVALID, "no deltas");
	const uint8_t hello[] = {0x01, 0x00};
	CHECK(t.feedFrame(hello, sizeof(hello)) == Tracker::FRAME_HELLO, "hello");
	CHECK(t.version() == v && t.pointCount() == 4, "invalid frames and HELLO leave the profile alone");
	const uint8_t none[] = {0x01, 0x02};
	CHECK(t.feedFrame(none, sizeof(none)) == Tracker::FRAME_NONE && t.pointCount() == 0 && !t.status().hasProfile, "none");
}

static void testSingleClimb() {
	printf("One climb: 2 km at 6 %%\n");
	Route r(200);
	r.add(1000, 0).add(2000, 6).add(1000, -5).add(2000, 0);
	Tracker t;
	Phone phone(r, t);
	bool seenApproach = false, seenOn = false;
	float lastRemaining = 1e9f;
	uint16_t id = 0;
	for (float pos = 0; pos <= r.totalM(); pos += 5) {
		phone.onFix(pos);
		const Status& s = t.status();
		if (pos < 800) CHECK(!s.hasProfile, "profile before the climb at %.0f", pos);
		if (!s.active) continue;
		id = s.climbId;
		CHECK(s.rank == RANK_CAT4, "rank %u at %.0f (score 12000 = category 4)", s.rank, pos);
		CHECK(NEAR(s.totalAscentM, 120, 1.0f) && NEAR(s.lengthM, 2000, 26), "ascent %.1f length %.0f at %.0f", s.totalAscentM, s.lengthM, pos);
		CHECK(NEAR(s.avgGradePct, 6, 0.2f), "avg %.2f", s.avgGradePct);
		CHECK(!s.summitOpen, "summit open at %.0f", pos);
		if (s.toFootM > 0) {
			seenApproach = true;
			CHECK(!s.onClimb && NEAR(s.toFootM, 1000 - pos, 26) && NEAR(s.remainingAscentM, 120, 1), "approach at %.0f: toFoot %.0f", pos, s.toFootM);
		} else if (s.onClimb) {
			seenOn = true;
			CHECK(NEAR(s.remainingAscentM, (3000 - pos) * 0.06f, 1.0f), "remaining %.1f at %.0f", s.remainingAscentM, pos);
			CHECK(NEAR(s.toSummitM, 3000 - pos, 1.0f), "to summit %.0f at %.0f", s.toSummitM, pos);
			CHECK(s.remainingAscentM <= lastRemaining + 0.01f, "remaining rises at %.0f", pos);
			lastRemaining = s.remainingAscentM;
			CHECK(NEAR(s.doneFraction, (pos - 1000) / 2000, 0.02f), "done %.2f at %.0f", s.doneFraction, pos);
			if (pos < 2900) CHECK(NEAR(s.aheadGradePct, 6, 0.3f), "ahead %.2f at %.0f", s.aheadGradePct, pos);
		}
	}
	CHECK(seenApproach && seenOn, "approach %d, on climb %d", seenApproach, seenOn);
	CHECK(phone.frames == 1 && phone.nones == 1, "phone sent %d profiles, %d none", phone.frames, phone.nones);
	CHECK(id == 1, "climb id %u", id);
	CHECK(!t.status().hasProfile, "profile left after the ride");
}

static void testHill() {
	printf("Hill: 200 m at 6 %% is not rated\n");
	Route r(200);
	r.add(1000, 0).add(200, 6).add(1000, 0);
	Tracker t;
	Phone phone(r, t);
	for (float pos = 0; pos <= r.totalM(); pos += 5) {
		phone.onFix(pos);
		CHECK(t.status().rank == RANK_NONE, "rank %u at %.0f", t.status().rank, pos);
	}
	CHECK(phone.frames == 1, "the phone does send it (%d)", phone.frames);

	printf("Ramp: 250 m at 9 %% (22 m) is too short, 325 m is category 6\n");
	Route r2(200);
	r2.add(500, 0).add(250, 9).add(1000, 0);
	Tracker t2;
	std::vector<uint8_t> f = r2.slice(400, 200);
	t2.feedFrame(f.data(), f.size());
	t2.setRemaining(r2.totalM() - 400);
	CHECK(t2.status().active && t2.status().rank == RANK_NONE, "250 m: active %d rank %u", t2.status().active, t2.status().rank);
	Route r3(200);
	r3.add(500, 0).add(325, 9).add(1000, 0);
	Tracker t3;
	f = r3.slice(400, 200);
	t3.feedFrame(f.data(), f.size());
	t3.setRemaining(r3.totalM() - 400);
	CHECK(t3.status().rank == RANK_CAT6, "325 m: rank %u", t3.status().rank);
}

static void testStep(float flatM, bool expectOne) {
	printf("Step: 1 km at 6 %%, %.0f m flat, 1 km at 6 %% -> %s\n", flatM, expectOne ? "one climb" : "two climbs");
	Route r(300);
	r.add(500, 0).add(1000, 6).add(flatM, 0).add(1000, 6).add(1000, -4).add(500, 0);
	Tracker t;
	Phone phone(r, t);
	uint16_t firstId = 0, lastId = 0;
	float maxAscent = 0;
	for (float pos = 0; pos <= r.totalM(); pos += 5) {
		phone.onFix(pos);
		const Status& s = t.status();
		if (!s.active) continue;
		if (!firstId) firstId = s.climbId;
		lastId = s.climbId;
		if (s.totalAscentM > maxAscent) maxAscent = s.totalAscentM;
		if (expectOne) {
			CHECK(NEAR(s.totalAscentM, 120, 1.5f), "ascent %.1f at %.0f", s.totalAscentM, pos);
			CHECK(s.rank == RANK_CAT4, "rank %u at %.0f", s.rank, pos);
			// on the second ramp the first one still counts
			if (pos > 1500 + flatM + 100 && s.onClimb) CHECK(NEAR(s.remainingAscentM, (2500 + flatM - pos) * 0.06f, 1.5f), "remaining %.1f at %.0f", s.remainingAscentM, pos);
		} else {
			CHECK(NEAR(s.totalAscentM, 60, 1.5f) && s.rank == RANK_CAT5, "ascent %.1f rank %u at %.0f", s.totalAscentM, s.rank, pos);
		}
	}
	CHECK(phone.nones == 2, "the phone takes the profile back on the step (%d)", phone.nones);
	CHECK(firstId == 1 && lastId == (expectOne ? 1 : 2), "ids %u..%u", firstId, lastId);
}

static void testLongClimb() {
	printf("Long climb in three windows: 10 km at 7 %%\n");
	Route r(400);
	r.add(500, 0).add(10000, 7).add(2000, -6).add(1000, 0);
	Tracker t;
	Phone phone(r, t);
	uint8_t rankAtStart = 0, rankAtEnd = 0;
	bool openAtStart = false, closedAtEnd = false, footDropped = false;
	for (float pos = 0; pos <= r.totalM(); pos += 5) {
		const uint32_t before = t.version();
		phone.onFix(pos);
		const Status& s = t.status();
		if (t.version() != before && s.hasProfile) printf("  profile at %5.0f m: %3u points from %u m remaining, foot %.0f m, summit %.0f m%s, rank %s, %.0f m up\n",
				pos, t.pointCount(), (unsigned)t.startRemainingM(), s.footM, s.summitM, s.summitOpen ? " (open)" : "", rankLabel(s.rank), s.totalAscentM);
		if (!s.active) continue;
		CHECK(s.climbId == 1, "climb id %u at %.0f", s.climbId, pos);
		if (pos > 500 && pos < 600) {rankAtStart = s.rank; openAtStart = s.summitOpen;}
		if (pos > 9000 && pos < 9100) {rankAtEnd = s.rank; closedAtEnd = !s.summitOpen;}
		if (s.footM < 0) footDropped = true;
		if (s.onClimb && pos > 9000) {
			CHECK(NEAR(s.totalAscentM, 700, 1.5f) && NEAR(s.lengthM, 10000, 26), "ascent %.1f length %.0f at %.0f", s.totalAscentM, s.lengthM, pos);
			CHECK(NEAR(s.remainingAscentM, (10500 - pos) * 0.07f, 1.5f), "remaining %.1f at %.0f", s.remainingAscentM, pos);
		}
		if (s.onClimb) CHECK(NEAR(s.posM - s.footM, pos - 500, 13), "position %.0f in the climb at %.0f", s.posM - s.footM, pos);
	}
	CHECK(phone.frames >= 3, "%d windows", phone.frames);
	CHECK(rankAtStart == RANK_CAT2 && openAtStart, "start: rank %u open %d (5 km known = 35000)", rankAtStart, openAtStart);
	CHECK(rankAtEnd == RANK_CAT1 && closedAtEnd, "end: rank %u closed %d (70000)", rankAtEnd, closedAtEnd);
	CHECK(footDropped, "8 km buffer: the foot has to drop out");
}

static void testDip() {
	printf("Dip: 8 m keeps the climb, 15 m ends it\n");
	for (float dip : {8.0f, 15.0f}) {
		Route r(100);
		r.add(1000, 6).add(dip / 0.05f, -5).add(1000, 6).add(1000, -5);
		Tracker t;
		std::vector<uint8_t> f = r.slice(0, 200);
		t.feedFrame(f.data(), f.size());
		t.setRemaining(r.totalM());
		const Status& s = t.status();
		if (dip < 10) CHECK(NEAR(s.totalAscentM, 120 - dip, 1.5f) && !s.summitOpen, "dip %.0f: ascent %.1f", dip, s.totalAscentM);
		else CHECK(NEAR(s.totalAscentM, 60, 1.5f) && NEAR(s.summitM, 1000, 26), "dip %.0f: ascent %.1f summit %.0f", dip, s.totalAscentM, s.summitM);
	}
}

static void testPosition() {
	printf("Position outside the profile, reset\n");
	Route r(100);
	r.add(3000, 6);
	Tracker t;
	std::vector<uint8_t> f = r.slice(1000, 40);		// 1000..2000 m
	t.feedFrame(f.data(), f.size());
	t.setRemaining(r.totalM() - 900);
	CHECK(!t.status().positionValid && !t.status().active, "100 m before the profile");
	t.setRemaining(r.totalM() - 990);
	CHECK(t.status().positionValid && NEAR(t.status().posM, 0, 0.1f), "10 m before: clamped to its start");
	t.setRemaining(r.totalM() - 1500);
	CHECK(t.status().positionValid && t.status().active && t.status().summitOpen, "inside, open summit");
	CHECK(NEAR(t.status().altM, 190, 0.2f), "altitude %.1f", t.status().altM);
	t.setRemaining(r.totalM() - 2100);
	CHECK(!t.status().positionValid, "100 m past the profile");
	t.setRemaining(r.totalM() - 1500);
	t.clearRemaining();
	CHECK(t.status().hasProfile && !t.status().positionValid, "no route position");
	t.reset();
	CHECK(!t.status().hasProfile && t.pointCount() == 0, "reset");

	// A different stretch replaces the points instead of joining them
	Route r2(500);
	r2.add(4000, 5);
	t.feedFrame(f.data(), f.size());
	std::vector<uint8_t> g = r2.slice(1000, 40);
	t.feedFrame(g.data(), g.size());
	CHECK(t.pointCount() == 41 && t.altitudesDm()[0] == 5500, "replaced: %u points, %d dm", t.pointCount(), t.altitudesDm()[0]);
}

static void testParams() {
	printf("Settings by name\n");
	Config c;
	CHECK(paramCount() == 25, "%u parameters", paramCount());
	for (uint8_t i = 0; i < paramCount(); i++) {
		const ParamInfo& p = paramInfo(i);
		CHECK(strlen(p.name) <= 15, "%s too long for an NVS key", p.name);
		CHECK(paramFind(p.name) == i, "%s not found", p.name);
		const float v = paramGet(c, i);
		CHECK(v >= p.min && v <= p.max, "default of %s (%g) out of range", p.name, v);
		CHECK(paramSet(c, i, v) && paramGet(c, i) == v, "%s round trip", p.name);
	}
	CHECK(paramFind("CatHC") == paramFind("cathc") && paramFind("nope") == -1, "find");
	CHECK(paramSet(c, paramFind("cat4"), 9000) && c.catScore[2] == 9000, "cat4");
	CHECK(paramSet(c, paramFind("band3"), 6.5f) && c.gradeBandPct[2] == 6.5f, "band3");
	CHECK(paramSet(c, paramFind("autoshow"), 0) && !c.autoShow, "autoshow");
	CHECK(paramSet(c, paramFind("hidedelay"), 30) && c.hideDelayS == 30, "hidedelay");
	CHECK(!paramSet(c, paramFind("autorank"), 9) && c.autoShowMinRank == RANK_CAT6, "range check");
	CHECK(!paramSet(c, paramFind("startgrade"), NAN), "NAN");

	Config d;
	CHECK(gradeBand(d, -3) == 0 && gradeBand(d, 0.9f) == 0 && gradeBand(d, 1) == 1 && gradeBand(d, 5) == 2 && gradeBand(d, 8) == 3 && gradeBand(d, 14) == 4, "bands");
	CHECK(strcmp(rankLabel(RANK_HC), "HC") == 0 && strcmp(rankLabel(RANK_CAT6), "6") == 0 && strcmp(rankLabel(RANK_CAT1), "1") == 0, "labels");

	// Scale: 100 x height gain. 800 m in 10 km is HC, 640 m category 1.
	Tracker t;
	Route r(0);
	r.add(100, 0).add(4000, 8).add(500, -5);		// 320 m: category 2
	std::vector<uint8_t> f = r.slice(0, 200);
	t.feedFrame(f.data(), f.size());
	t.setRemaining(r.totalM());
	CHECK(t.status().rank == RANK_CAT2, "320 m gain: rank %u", t.status().rank);
	t.cfg.catScore[4] = 33000;
	t.setRemaining(r.totalM());
	CHECK(t.status().rank == RANK_CAT3, "threshold raised: rank %u", t.status().rank);
}

int main() {
	testFrames();
	testSingleClimb();
	testHill();
	testStep(300, true);
	testStep(800, false);
	testLongClimb();
	testDip();
	testPosition();
	testParams();
	printf(failures ? "\n%d FAILED\n" : "\nAll checks passed\n", failures);
	return failures ? 1 : 0;
}
