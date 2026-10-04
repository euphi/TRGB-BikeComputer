/*
 * Host test for src/ClimbProfile.{h,cpp} -- no hardware, no PlatformIO. Build and run from
 * the repository root:
 *
 *   g++ -std=c++17 -O2 -Wall -Isrc src/ClimbProfile.cpp test/native_climb/climb_test.cpp -o /tmp/climb_test && /tmp/climb_test
 *
 * Exit code 0 = all checks passed. The rides are fed the way TrailBridge does it
 * (RouteNavigator/ElevationProfile there). Two phones: Phone is the older one -- the profile
 * from the rider to the summit once the foot of a climb is 500 m ahead, on a raster that fits it
 * into 200 steps, PROFILE_NONE at the summit; RollingPhone sends the road ahead all the time and
 * announces the climb (frame flag ROLLING, tags 0x07..0x0A), no PROFILE_NONE at the summit.
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
	// ElevationProfile.findClimbs(): {foot, summit} as raster indices
	std::vector<std::pair<int, int>> climbs() const {
		std::vector<std::pair<int, int>> out;
		const int last = (int)alt.size() - 1, window = 100 / STEP;
		int i = 0;
		while (i + 2 <= last) {
			int j = std::min(i + window, last);
			if (alt[j] - alt[i] < 3.0f * (j - i) / window) {i++; continue;}
			int foot = i;
			while (foot + 1 < j && alt[foot + 1] - alt[foot] < 0.005f * STEP) foot++;
			int summit = foot;
			for (int k = foot + 1; k <= last; k++) {
				float dist = (k - summit) * (float)STEP, rise = alt[k] - alt[summit];
				if (rise > 0 && rise >= 0.005f * dist) summit = k;
				else if (-rise > 30 || dist > 2000) break;
			}
			if (alt[summit] - alt[foot] >= 10) out.push_back({foot, summit});
			i = std::max(summit, i) + 1;
		}
		return out;
	}
	// ElevationProfile.slice() + ProfileFrameEncoder.encode()
	std::vector<uint8_t> slice(float fromM, float toM, int maxSteps = 200, int stepM = 0, float* startAlong = nullptr, int* length = nullptr) const {
		const int last = (int)alt.size() - 1;
		int iFrom = std::max(0, std::min((int)lroundf(fromM / STEP), last));
		int iTo = std::max(0, std::min((int)lroundf(toM / STEP), last));
		if (iTo <= iFrom) return {};
		int factor = stepM > 0 ? stepM / STEP : std::min(10, (iTo - iFrom + maxSteps - 1) / maxSteps);
		int steps = (iTo - iFrom + factor - 1) / factor;
		int i0 = iTo - steps * factor;
		if (i0 < 0) {i0 += factor; steps--;}
		steps = std::min(steps, maxSteps);
		if (steps < 8) return {};
		const int base = (int)lroundf(alt[i0] * 10);
		std::vector<int8_t> deltas;
		int scale = 0;
		for (bool fits = false; !fits;) {
			scale++;
			deltas.clear();
			fits = true;
			int prev = 0;
			for (int k = 0; k < steps && fits; k++) {
				int cur = (int)lroundf((alt[i0 + (k + 1) * factor] * 10 - base) / scale);
				fits = cur - prev >= -127 && cur - prev <= 127;
				deltas.push_back((int8_t)(cur - prev));
				prev = cur;
			}
		}
		std::vector<uint8_t> f = {PROFILE_PROTOCOL_VERSION, PROFILE_MSG_PROFILE_UPDATE};
		uint32_t remaining = (uint32_t)lroundf(totalM() - i0 * STEP);
		f.insert(f.end(), {PROFILE_TAG_START_REMAINING_DISTANCE_M, 4, (uint8_t)remaining, (uint8_t)(remaining >> 8), (uint8_t)(remaining >> 16), (uint8_t)(remaining >> 24)});
		f.insert(f.end(), {PROFILE_TAG_STEP_M, 1, (uint8_t)(STEP * factor)});
		f.insert(f.end(), {PROFILE_TAG_BASE_ALT_DM, 2, (uint8_t)base, (uint8_t)(base >> 8)});
		if (scale != 1) f.insert(f.end(), {PROFILE_TAG_DELTA_SCALE_DM, 1, (uint8_t)scale});
		f.insert(f.end(), {PROFILE_TAG_DELTAS_DM, (uint8_t)steps});
		for (int8_t d : deltas) f.push_back((uint8_t)d);
		if (startAlong) *startAlong = i0 * STEP;
		if (length) *length = steps * STEP * factor;
		return f;
	}
};

// RouteNavigator.onFix(): decides what goes to the BikeComputer
struct Phone {
	const Route& route;
	Tracker& tracker;
	std::vector<std::pair<int, int>> climbs;
	int maxSteps;
	int sentClimb = -1;
	float sentStart = 0;
	int sentLength = 0, sentStepM = 0;
	int frames = 0, nones = 0;
	Phone(const Route& r, Tracker& t, int maxSteps = 200) : route(r), tracker(t), climbs(r.climbs()), maxSteps(maxSteps) {}

	int climbAhead(float progress) const {
		for (size_t k = 0; k < climbs.size(); k++) {
			if (progress < climbs[k].second * STEP - STEP / 2.0f) return progress >= climbs[k].first * STEP - 500 ? (int)k : -1;
		}
		return -1;
	}
	bool cut(float progress, int k, int stepM) {
		std::vector<uint8_t> f = route.slice(progress, climbs[k].second * STEP, maxSteps, stepM, &sentStart, &sentLength);
		if (f.empty()) return false;
		sentStepM = f[10];
		sentClimb = k;
		frames++;
		CHECK(tracker.feedFrame(f.data(), f.size()) == Tracker::FRAME_PROFILE, "frame rejected at %.0f m", progress);
		return true;
	}
	void onFix(float progress) {
		const int k = climbAhead(progress);
		bool sent = false;
		if (k >= 0 && k != sentClimb) {
			sent = cut(progress, k, 0);
		} else if (k >= 0 && sentStart + sentLength < climbs[k].second * STEP - STEP / 2.0f && progress > sentStart + sentLength / 2.0f) {
			sent = cut(progress, k, sentStepM);
		}
		if (!sent && sentClimb >= 0 && k != sentClimb) {
			const uint8_t none[] = {PROFILE_PROTOCOL_VERSION, PROFILE_MSG_PROFILE_NONE};
			tracker.feedFrame(none, sizeof(none));
			nones++;
			sentClimb = -1;
		}
		tracker.setRemaining(route.totalM() - progress);		// the nav frame's REMAINING_DISTANCE_M
	}
};

// RouteNavigator.nextProfile(): the road ahead all the time, a climb announced with its whole extent
struct RollingPhone {
	const Route& route;
	Tracker& tracker;
	std::vector<std::pair<int, int>> climbs;
	int maxSteps;
	int sentClimb = -1, passed = -1;
	bool haveSent = false;
	float sentStart = 0;
	int sentLength = 0, sentStepM = 0;
	int frames = 0;
	RollingPhone(const Route& r, Tracker& t, int maxSteps = 200) : route(r), tracker(t), climbs(r.climbs()), maxSteps(maxSteps) {}

	int climbAhead(float progress) {
		if (sentClimb >= 0 && progress >= climbs[sentClimb].second * STEP - STEP / 2.0f) passed = sentClimb;
		float p = progress;
		if (passed >= 0) {
			const float summit = climbs[passed].second * (float)STEP;
			if (p < summit - 100) passed = -1;
			else p = std::max(p, summit);
		}
		for (size_t k = 0; k < climbs.size(); k++) {
			if (p < climbs[k].second * STEP - STEP / 2.0f) return p >= climbs[k].first * STEP - 500 ? (int)k : -1;
		}
		return -1;
	}
	void forget() {haveSent = false; sentClimb = -1;}		// profile taken back (PROFILE_NONE sent by the caller)
	void onFix(float progress) {
		const int k = climbAhead(progress);
		const float toM = k >= 0 ? climbs[k].second * (float)STEP : std::min(route.totalM(), progress + maxSteps * (float)STEP);
		bool fresh = !haveSent || k != sentClimb;
		bool send = fresh;
		if (!fresh) {
			const bool endShort = sentStart + sentLength < toM - STEP / 2.0f;
			const bool pastMiddle = progress > sentStart + sentLength / 2.0f;
			send = endShort && pastMiddle;
		}
		if (send) {
			const int stepM = (!fresh && k >= 0) ? sentStepM : 0;
			float start = 0;
			int length = 0;
			std::vector<uint8_t> f = route.slice(progress, toM, maxSteps, stepM, &start, &length);
			if (!f.empty()) {
				sentStart = start;
				sentLength = length;
				sentStepM = f[10];
				sentClimb = k;
				haveSent = true;
				frames++;
				f.insert(f.end(), {PROFILE_TAG_FLAGS, 1, PROFILE_FLAG_ROLLING});
				if (k >= 0) {
					const uint32_t footRem = (uint32_t)lroundf(route.totalM() - climbs[k].first * STEP);
					const uint32_t summitRem = (uint32_t)lroundf(route.totalM() - climbs[k].second * STEP);
					const int16_t footAlt = (int16_t)lroundf(route.alt[climbs[k].first] * 10), summitAlt = (int16_t)lroundf(route.alt[climbs[k].second] * 10);
					f.insert(f.end(), {PROFILE_TAG_CLIMB_FOOT_REMAINING_M, 4, (uint8_t)footRem, (uint8_t)(footRem >> 8), (uint8_t)(footRem >> 16), (uint8_t)(footRem >> 24)});
					f.insert(f.end(), {PROFILE_TAG_CLIMB_SUMMIT_REMAINING_M, 4, (uint8_t)summitRem, (uint8_t)(summitRem >> 8), (uint8_t)(summitRem >> 16), (uint8_t)(summitRem >> 24)});
					f.insert(f.end(), {PROFILE_TAG_CLIMB_FOOT_ALT_DM, 2, (uint8_t)footAlt, (uint8_t)(footAlt >> 8)});
					f.insert(f.end(), {PROFILE_TAG_CLIMB_SUMMIT_ALT_DM, 2, (uint8_t)summitAlt, (uint8_t)(summitAlt >> 8)});
				}
				CHECK(tracker.feedFrame(f.data(), f.size()) == Tracker::FRAME_PROFILE, "rolling frame rejected at %.0f m", progress);
			}
		}
		tracker.setRemaining(route.totalM() - progress);
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

	// Coarse raster: 200 m steps, deltas in 0.2 m (DELTA_SCALE_DM = 2)
	const uint8_t scaled[] = {0x01, 0x01, 0x01, 0x04, 0xA0, 0x8C, 0x00, 0x00, 0x02, 0x01, 0xC8, 0x03, 0x02, 0xE0, 0x2E, 0x05, 0x01, 0x02, 0x04, 0x03, 0x3C, 0x45, 0xFD};
	Tracker sc;
	CHECK(sc.feedFrame(scaled, sizeof(scaled)) == Tracker::FRAME_PROFILE, "scaled frame");
	CHECK(sc.pointCount() == 4 && sc.stepM() == 200 && sc.altitudesDm()[0] == 12000 && sc.altitudesDm()[1] == 12120 && sc.altitudesDm()[3] == 12252, "scaled altitudes");
	const uint8_t scale0[] = {0x01, 0x01, 0x01, 0x04, 0xA0, 0x8C, 0x00, 0x00, 0x02, 0x01, 0xC8, 0x03, 0x02, 0xE0, 0x2E, 0x05, 0x01, 0x00, 0x04, 0x01, 0x3C};
	CHECK(sc.feedFrame(scale0, sizeof(scale0)) == Tracker::FRAME_INVALID && sc.pointCount() == 4, "scale 0");

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
		if (pos < 480) CHECK(!s.hasProfile, "profile before the climb at %.0f", pos);
		if (pos > 520 && pos < 2980) CHECK(s.active, "no climb at %.0f", pos);
		if (!s.active) continue;
		id = s.climbId;
		CHECK(s.rank == RANK_CAT4, "rank %u at %.0f (score 12000 = category 4)", s.rank, pos);
		CHECK(NEAR(s.totalAscentM, 120, 1.0f) && NEAR(s.lengthM, 2000, 26), "ascent %.1f length %.0f at %.0f", s.totalAscentM, s.lengthM, pos);
		CHECK(NEAR(s.avgGradePct, 6, 0.2f), "avg %.2f", s.avgGradePct);
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
	std::vector<uint8_t> f = r2.slice(400, 750);
	t2.feedFrame(f.data(), f.size());
	t2.setRemaining(r2.totalM() - 400);
	CHECK(t2.status().active && t2.status().rank == RANK_NONE, "250 m: active %d rank %u", t2.status().active, t2.status().rank);
	Route r3(200);
	r3.add(500, 0).add(325, 9).add(1000, 0);
	Tracker t3;
	f = r3.slice(400, 825);
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
	CHECK(phone.frames == (expectOne ? 1 : 2) && phone.nones == phone.frames, "the phone sent %d profiles, %d none", phone.frames, phone.nones);
	CHECK(firstId == 1 && lastId == (expectOne ? 1 : 2), "ids %u..%u", firstId, lastId);
}

static void testLongClimb() {
	printf("Long climb in one frame: 10 km at 7 %%\n");
	Route r(400);
	r.add(1000, 0).add(10000, 7).add(2000, -6).add(1000, 0);
	Tracker t;
	Phone phone(r, t);
	bool seenApproach = false;
	for (float pos = 0; pos <= r.totalM(); pos += 5) {
		const uint32_t before = t.version();
		phone.onFix(pos);
		const Status& s = t.status();
		if (t.version() != before && s.hasProfile) printf("  profile at %5.0f m: %3u points every %u m from %u m remaining, foot %.0f m, summit %.0f m, rank %s, %.0f m up\n",
				pos, t.pointCount(), t.stepM(), (unsigned)t.startRemainingM(), s.footM, s.summitM, rankLabel(s.rank), s.totalAscentM);
		if (pos > 520 && pos < 10980) CHECK(s.active, "no climb at %.0f", pos);
		if (!s.active) continue;
		// the whole climb is known from the first frame on
		CHECK(s.climbId == 1 && s.rank == RANK_CAT1, "climb id %u rank %u at %.0f (score 70000 = category 1)", s.climbId, s.rank, pos);
		CHECK(NEAR(s.totalAscentM, 700, 4) && NEAR(s.lengthM, 10000, 76), "ascent %.1f length %.0f at %.0f", s.totalAscentM, s.lengthM, pos);
		CHECK(NEAR(s.toSummitM, 11000 - pos, 1), "to summit %.0f at %.0f", s.toSummitM, pos);
		if (s.toFootM > 0) seenApproach = true;
		if (s.onClimb && pos > 1100) CHECK(NEAR(s.remainingAscentM, (11000 - pos) * 0.07f, 1.5f), "remaining %.1f at %.0f", s.remainingAscentM, pos);
	}
	CHECK(phone.frames == 1 && phone.nones == 1, "%d profiles, %d none", phone.frames, phone.nones);
	CHECK(t.stepM() == 75, "raster %u m", t.stepM());
	CHECK(seenApproach, "no approach");
}

// A pass road from (distance m, altitude m) corner points
static Route road(std::initializer_list<std::pair<float, float>> pts) {
	Route r(pts.begin()->second);
	const std::pair<float, float>* prev = pts.begin();
	for (const std::pair<float, float>* p = prev + 1; p != pts.end(); prev = p++) {
		r.add(p->first - prev->first, (p->second - prev->second) / (p->first - prev->first) * 100);
	}
	return r;
}

static void testGalibier() {
	printf("Galibier from the south: 36 km with false flats, a level km and a dip -> one climb, one frame\n");
	Route south = road({{0, 1200}, {2000, 1200}, {4000, 1260}, {10000, 1400}, {16000, 1475}, {17000, 1475}, {22000, 1700}, {22400, 1685}, {29500, 2058}, {38000, 2642}, {43000, 2300}});
	{
		Tracker t;
		Phone phone(south, t);
		for (float pos = 0; pos <= south.totalM(); pos += 10) {
			const uint32_t before = t.version();
			phone.onFix(pos);
			const Status& s = t.status();
			if (t.version() != before && s.hasProfile) printf("  profile at %5.0f m: %3u points every %u m, foot %.0f m, summit %.0f m, rank %s, %.0f m up\n",
					pos, t.pointCount(), t.stepM(), s.footM, s.summitM, rankLabel(s.rank), s.totalAscentM);
			if (pos > 1520 && pos < 37950) CHECK(s.active && s.climbId == 1, "active %d id %u at %.0f", s.active, s.climbId, pos);
			if (!s.active) continue;
			CHECK(s.rank == RANK_HC && NEAR(s.totalAscentM, 1442, 8) && NEAR(s.lengthM, 36000, 400), "rank %u ascent %.1f length %.0f at %.0f", s.rank, s.totalAscentM, s.lengthM, pos);
			CHECK(NEAR(s.toSummitM, 38000 - pos, 1), "to summit %.0f at %.0f", s.toSummitM, pos);
			CHECK(NEAR(s.altM, south.altAt(pos), 3), "altitude %.1f at %.0f (route %.1f)", s.altM, pos, south.altAt(pos));
		}
		CHECK(phone.frames == 1 && phone.nones == 1 && t.stepM() == 200, "%d profiles, %d none, raster %u m", phone.frames, phone.nones, t.stepM());
	}

	printf("Galibier from the north: Telegraphe, 4.8 km down to Valloire, Galibier -> two climbs\n");
	Route north = road({{0, 710}, {2000, 710}, {14000, 1566}, {18800, 1400}, {36800, 2642}, {42000, 2280}});
	{
		Tracker t;
		Phone phone(north, t);
		for (float pos = 0; pos <= north.totalM(); pos += 10) {
			phone.onFix(pos);
			const Status& s = t.status();
			if (pos > 1520 && pos < 13950) CHECK(s.active && s.climbId == 1 && NEAR(s.totalAscentM, 856, 5) && NEAR(s.toSummitM, 14000 - pos, 1), "Telegraphe: active %d id %u ascent %.1f at %.0f", s.active, s.climbId, s.totalAscentM, pos);
			if (pos > 14100 && pos < 18200) CHECK(!s.hasProfile, "profile on the descent at %.0f", pos);
			if (pos > 18320 && pos < 36750) CHECK(s.active && s.climbId == 2 && NEAR(s.totalAscentM, 1242, 5) && NEAR(s.toSummitM, 36800 - pos, 1), "Galibier: active %d id %u ascent %.1f at %.0f", s.active, s.climbId, s.totalAscentM, pos);
		}
		CHECK(phone.frames == 2 && phone.nones == 2, "%d profiles, %d none", phone.frames, phone.nones);
	}

	printf("Too long for one frame (40 steps): the next stretch joins on the same raster\n");
	{
		Tracker t;
		Phone phone(south, t, 40);
		for (float pos = 0; pos <= south.totalM(); pos += 10) {
			phone.onFix(pos);
			const Status& s = t.status();
			if (pos > 2300 && pos < 37950) CHECK(s.active && s.climbId == 1, "active %d id %u at %.0f", s.active, s.climbId, pos);
			if (pos > 36000 && pos < 37950) CHECK(NEAR(s.toSummitM, 38000 - pos, 1), "to summit %.0f at %.0f", s.toSummitM, pos);
		}
		CHECK(phone.frames > 4 && t.stepM() == 250, "%d profiles, raster %u m", phone.frames, t.stepM());
	}
}

static void testDip() {
	printf("Dip: 20 m keeps the climb, 40 m ends it\n");
	for (float dip : {20.0f, 40.0f}) {
		Route r(100);
		r.add(1000, 6).add(dip / 0.05f, -5).add(1000, 6).add(1000, -5);
		Tracker t;
		std::vector<uint8_t> f = r.slice(0, r.totalM());		// the whole route, beyond what the phone would send
		t.feedFrame(f.data(), f.size());
		t.setRemaining(r.totalM());
		const Status& s = t.status();
		if (dip < 30) CHECK(NEAR(s.totalAscentM, 120 - dip, 1.5f), "dip %.0f: ascent %.1f", dip, s.totalAscentM);
		else CHECK(NEAR(s.totalAscentM, 60, 1.5f) && NEAR(s.summitM, 1000, 26), "dip %.0f: ascent %.1f summit %.0f", dip, s.totalAscentM, s.summitM);
	}
}

static void testPosition() {
	printf("Position outside the profile, reset\n");
	Route r(100);
	r.add(3000, 6);
	Tracker t;
	std::vector<uint8_t> f = r.slice(1000, 2000, 40);
	t.feedFrame(f.data(), f.size());
	t.setRemaining(r.totalM() - 900);
	CHECK(!t.status().positionValid && !t.status().active, "100 m before the profile");
	t.setRemaining(r.totalM() - 990);
	CHECK(t.status().positionValid && NEAR(t.status().posM, 0, 0.1f), "10 m before: clamped to its start");
	t.setRemaining(r.totalM() - 1500);
	CHECK(t.status().positionValid && t.status().active && NEAR(t.status().toSummitM, 500, 1), "inside, summit at the end of the profile");
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
	std::vector<uint8_t> g = r2.slice(1000, 2000, 40);
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
	std::vector<uint8_t> f = r.slice(0, r.totalM());
	t.feedFrame(f.data(), f.size());
	t.setRemaining(r.totalM());
	CHECK(t.status().rank == RANK_CAT2, "320 m gain: rank %u", t.status().rank);
	t.cfg.catScore[4] = 33000;
	t.setRemaining(r.totalM());
	CHECK(t.status().rank == RANK_CAT3, "threshold raised: rank %u", t.status().rank);
}

static void testRolling() {
	printf("Rolling phone: 2 km at 6 %%, the profile all the time, the climb announced whole\n");
	Route r(200);
	r.add(1000, 0).add(2000, 6).add(1000, -5).add(2000, 0);
	Tracker t;
	RollingPhone phone(r, t);
	uint16_t id = 0;
	bool over = false;
	float firstFootM = 1e9f;
	for (float pos = 0; pos <= r.totalM() - 250; pos += 5) {
		phone.onFix(pos);
		const Status& s = t.status();
		CHECK(s.rolling && s.hasProfile, "no profile at %.0f", pos);			// also on the flat and downhill
		if (pos < 460) CHECK(!s.active && !s.climbOver, "climb before its foot is close at %.0f", pos);
		if (pos > 520 && pos < 2980) {
			CHECK(s.active && !s.climbOver, "no climb at %.0f", pos);
			if (!s.active) continue;
			if (!id) id = s.climbId;
			CHECK(s.climbId == id, "climb id changed to %u at %.0f", s.climbId, pos);
			// category and size are the whole climb's from the first frame to the last metre
			CHECK(s.rank == RANK_CAT4, "rank %u at %.0f", s.rank, pos);
			CHECK(NEAR(s.totalAscentM, 120, 1.5f) && NEAR(s.lengthM, 2000, 30), "ascent %.1f length %.0f at %.0f", s.totalAscentM, s.lengthM, pos);
			if (pos > 2500) CHECK(s.footM < s.posM - 1000, "foot %.0f should be far behind the rider %.0f", s.footM, s.posM);
			if (s.onClimb && pos < 2900) CHECK(NEAR(s.remainingAscentM, (3000 - pos) * 0.06f, 1.5f), "remaining %.1f at %.0f", s.remainingAscentM, pos);
			firstFootM = std::min(firstFootM, s.footM);
		}
		if (pos > 3100) {
			CHECK(!s.active && s.climbOver, "climb should be over at %.0f (active %d, over %d)", pos, s.active, s.climbOver);
			over = true;
		}
	}
	CHECK(over && id == 1, "over %d, id %u", over, id);
	CHECK(phone.frames >= 2, "frames %d", phone.frames);

	printf("Rolling phone: PROFILE_NONE for a moment (off the route) does not end the climb\n");
	Tracker t2;
	RollingPhone p2(r, t2);
	uint16_t id2 = 0;
	for (float pos = 400; pos <= 2400; pos += 5) {
		p2.onFix(pos);
		if (pos == 1500) {
			const uint8_t none[] = {PROFILE_PROTOCOL_VERSION, PROFILE_MSG_PROFILE_NONE};
			t2.feedFrame(none, sizeof(none));
			p2.forget();
			CHECK(!t2.status().hasProfile, "profile gone");
		}
		if (pos > 520 && t2.status().active && !id2) id2 = t2.status().climbId;
		if (pos > 1600 && pos < 2900) CHECK(t2.status().active && t2.status().climbId == id2 && t2.status().rank == RANK_CAT4, "climb lost after NONE at %.0f", pos);
	}

	printf("Rolling phone: a flat road is a profile, not a climb\n");
	Route flat(300);
	flat.add(6000, 0.5f);
	Tracker t3;
	RollingPhone p3(flat, t3);
	for (float pos = 0; pos <= 5500; pos += 10) {
		p3.onFix(pos);
		CHECK(t3.status().hasProfile && !t3.status().active && t3.status().rolling, "flat at %.0f", pos);
	}
	CHECK(p3.frames >= 2, "the window moves on (%d frames)", p3.frames);

	printf("Older phone without the flag after a rolling one: back to finding the climb\n");
	Tracker t4;
	RollingPhone p4(r, t4);
	p4.onFix(600);
	CHECK(t4.status().rolling && t4.status().active, "rolling climb");
	Phone old(r, t4);
	for (float pos = 610; pos <= 700; pos += 10) old.onFix(pos);
	CHECK(!t4.status().rolling, "no longer rolling");
}

int main() {
	testFrames();
	testRolling();
	testSingleClimb();
	testHill();
	testStep(300, true);
	testStep(1500, true);
	testStep(2500, false);
	testLongClimb();
	testGalibier();
	testDip();
	testPosition();
	testParams();
	printf(failures ? "\n%d FAILED\n" : "\nAll checks passed\n", failures);
	return failures ? 1 : 0;
}
