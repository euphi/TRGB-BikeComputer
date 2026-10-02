/*
 * ClimbProfile.cpp
 *
 * See ClimbProfile.h.
 */

#include "ClimbProfile.h"
#include "BikeProfileProtocol.h"

#include <cstdlib>
#include <cstring>
#include <strings.h>

namespace Climb {

// ******************** Settings by name ********************

namespace {

enum ParamType : uint8_t {T_F32, T_U8, T_U16, T_U32, T_BOOL};

struct ParamDef {
	ParamInfo info;
	ParamType type;
	uint16_t offset;
};

#define CAT_OFFSET(i) (offsetof(Config, catScore) + (i) * sizeof(uint32_t))
#define BAND_OFFSET(i) (offsetof(Config, gradeBandPct) + (i) * sizeof(float))

const ParamDef PARAMS[] = {
	{{"startgrade", "%", 0.5f, 15, "foot: mean gradient over startwindow that starts a climb"}, T_F32, offsetof(Config, startGradePct)},
	{{"startwindow", "m", 50, 500, "foot: distance the start gradient is taken over"}, T_U16, offsetof(Config, startWindowM)},
	{{"contgrade", "%", 0, 10, "mean gradient from the summit so far that carries the climb on"}, T_F32, offsetof(Config, contGradePct)},
	{{"summitflat", "m", 100, 3000, "flat stretch that ends a climb (and gap to a climb that continues it)"}, T_U16, offsetof(Config, summitFlatM)},
	{{"summitdip", "m", 1, 100, "descent that ends a climb"}, T_F32, offsetof(Config, summitDipM)},
	{{"summitpass", "m", 0, 500, "distance past the summit after which the climb is over"}, T_U16, offsetof(Config, summitPassM)},
	{{"minlength", "m", 0, 5000, "shorter climbs are not rated"}, T_U16, offsetof(Config, minLengthM)},
	{{"mingrade", "%", 0, 15, "climbs flatter on average are not rated"}, T_F32, offsetof(Config, minAvgGradePct)},
	{{"cat6", "m*%", 100, 1000000, "score (length x mean gradient) for category 6"}, T_U32, CAT_OFFSET(0)},
	{{"cat5", "m*%", 100, 1000000, "... category 5"}, T_U32, CAT_OFFSET(1)},
	{{"cat4", "m*%", 100, 1000000, "... category 4"}, T_U32, CAT_OFFSET(2)},
	{{"cat3", "m*%", 100, 1000000, "... category 3"}, T_U32, CAT_OFFSET(3)},
	{{"cat2", "m*%", 100, 1000000, "... category 2"}, T_U32, CAT_OFFSET(4)},
	{{"cat1", "m*%", 100, 1000000, "... category 1"}, T_U32, CAT_OFFSET(5)},
	{{"cathc", "m*%", 100, 1000000, "... HC"}, T_U32, CAT_OFFSET(6)},
	{{"autoshow", "", 0, 1, "1: switch to the climb screen automatically"}, T_BOOL, offsetof(Config, autoShow)},
	{{"autorank", "", 1, 7, "lowest rank shown automatically: 1 = category 6 .. 6 = category 1, 7 = HC"}, T_U8, offsetof(Config, autoShowMinRank)},
	{{"showahead", "m", 0, 2000, "show the climb once its foot is this close"}, T_U16, offsetof(Config, showAheadM)},
	{{"hidedelay", "s", 0, 600, "leave the climb screen this long after the climb"}, T_U16, offsetof(Config, hideDelayS)},
	{{"lookahead", "m", 25, 1000, "distance of the \"gradient ahead\""}, T_U16, offsetof(Config, lookAheadM)},
	{{"band1", "%", -5, 30, "profile colour: flat/downhill below this gradient"}, T_F32, BAND_OFFSET(0)},
	{{"band2", "%", -5, 30, "... second colour below this"}, T_F32, BAND_OFFSET(1)},
	{{"band3", "%", -5, 30, "... third colour below this"}, T_F32, BAND_OFFSET(2)},
	{{"band4", "%", -5, 30, "... fourth colour below this, steepest above"}, T_F32, BAND_OFFSET(3)},
	{{"infocycle", "s", 1, 60, "the rotating info field changes this often"}, T_U8, offsetof(Config, infoCycleS)},
};
const uint8_t PARAM_COUNT = sizeof(PARAMS) / sizeof(PARAMS[0]);

}	// anonymous namespace

uint8_t paramCount() {
	return PARAM_COUNT;
}

const ParamInfo& paramInfo(uint8_t index) {
	return PARAMS[index < PARAM_COUNT ? index : 0].info;
}

int8_t paramFind(const char* name) {
	for (uint8_t i = 0; i < PARAM_COUNT; i++) {
		if (strcasecmp(name, PARAMS[i].info.name) == 0) return i;
	}
	return -1;
}

float paramGet(const Config& cfg, uint8_t index) {
	if (index >= PARAM_COUNT) return NAN;
	const uint8_t* p = reinterpret_cast<const uint8_t*>(&cfg) + PARAMS[index].offset;
	switch (PARAMS[index].type) {
	case T_F32: {float v; memcpy(&v, p, sizeof(v)); return v;}
	case T_U8: return *p;
	case T_U16: {uint16_t v; memcpy(&v, p, sizeof(v)); return v;}
	case T_U32: {uint32_t v; memcpy(&v, p, sizeof(v)); return static_cast<float>(v);}
	case T_BOOL: {bool v; memcpy(&v, p, sizeof(v)); return v ? 1.0f : 0.0f;}
	}
	return NAN;
}

bool paramSet(Config& cfg, uint8_t index, float value) {
	if (index >= PARAM_COUNT || std::isnan(value)) return false;
	const ParamDef& d = PARAMS[index];
	if (value < d.info.min || value > d.info.max) return false;
	uint8_t* p = reinterpret_cast<uint8_t*>(&cfg) + d.offset;
	switch (d.type) {
	case T_F32: memcpy(p, &value, sizeof(value)); break;
	case T_U8: *p = static_cast<uint8_t>(lroundf(value)); break;
	case T_U16: {uint16_t v = static_cast<uint16_t>(lroundf(value)); memcpy(p, &v, sizeof(v)); break;}
	case T_U32: {uint32_t v = static_cast<uint32_t>(lroundf(value)); memcpy(p, &v, sizeof(v)); break;}
	case T_BOOL: {bool v = value >= 0.5f; memcpy(p, &v, sizeof(v)); break;}
	}
	return true;
}

const char* rankLabel(uint8_t rank) {
	static const char* const LABEL[RANK_COUNT] = {"", "6", "5", "4", "3", "2", "1", "HC"};
	return rank < RANK_COUNT ? LABEL[rank] : "";
}

uint8_t gradeBand(const Config& cfg, float gradePct) {
	uint8_t band = 0;
	while (band < GRADE_BANDS - 1 && gradePct >= cfg.gradeBandPct[band]) band++;
	return band;
}

// ******************** Frames ********************

Tracker::FrameResult Tracker::feedFrame(const uint8_t* data, size_t length) {
	if (length < 2 || data[0] != PROFILE_PROTOCOL_VERSION) return FRAME_INVALID;

	switch (data[1]) {
	case PROFILE_MSG_HELLO:
		return FRAME_HELLO;

	case PROFILE_MSG_PROFILE_NONE:
		clearProfile();
		return FRAME_NONE;

	case PROFILE_MSG_PROFILE_UPDATE: {
		bool hasStart = false, hasStep = false, hasBase = false;
		uint32_t start = 0;
		uint8_t stepM = 0, scale = 1;
		int16_t base = 0;
		const int8_t* deltas = nullptr;
		uint16_t n = 0;

		size_t pos = 2;
		while (pos + 2 <= length) {
			const uint8_t tag = data[pos];
			const uint8_t len = data[pos + 1];
			pos += 2;
			if (pos + len > length) return FRAME_INVALID;
			const uint8_t* val = data + pos;
			switch (tag) {
			case PROFILE_TAG_START_REMAINING_DISTANCE_M:
				if (len >= 4) {start = val[0] | (val[1] << 8) | (val[2] << 16) | (static_cast<uint32_t>(val[3]) << 24); hasStart = true;}
				break;
			case PROFILE_TAG_STEP_M:
				if (len >= 1) {stepM = val[0]; hasStep = true;}
				break;
			case PROFILE_TAG_BASE_ALT_DM:
				if (len >= 2) {base = static_cast<int16_t>(val[0] | (val[1] << 8)); hasBase = true;}
				break;
			case PROFILE_TAG_DELTAS_DM:
				deltas = reinterpret_cast<const int8_t*>(val);
				n = len;
				break;
			case PROFILE_TAG_DELTA_SCALE_DM:
				if (len >= 1) scale = val[0];
				break;
			default:
				break;	// unknown tag: skipped by its length
			}
			pos += len;
		}
		if (!hasStart || !hasStep || !hasBase || stepM == 0 || scale == 0 || n == 0) return FRAME_INVALID;
		setProfile(start, stepM, base, deltas, n, scale);
		return FRAME_PROFILE;
	}

	default:
		return FRAME_INVALID;
	}
}

void Tracker::setProfile(uint32_t startRemainingM, uint8_t stepM, int16_t baseAltDm, const int8_t* deltasDm, uint16_t n, uint8_t deltaScaleDm) {
	if (stepM == 0 || n == 0 || deltaScaleDm == 0) return;
	if (n > MAX_POINTS - 1) n = MAX_POINTS - 1;

	// Same raster and overlapping what we have: keep the points behind its start (the part of
	// the climb already ridden), the new frame replaces everything from there on.
	uint16_t keep = 0;
	if (count > 0 && stepM == step && startRemainingM <= startRem && (startRem - startRemainingM) % step == 0) {
		const uint32_t k = (startRem - startRemainingM) / step;
		if (k < count && abs(alt[k] - baseAltDm) <= 50) {		// more than 5 m apart: not the same route profile
			keep = static_cast<uint16_t>(k);
			const uint32_t total = k + n + 1;
			if (total > MAX_POINTS) {
				const uint16_t drop = static_cast<uint16_t>(total - MAX_POINTS);		// <= k, as n + 1 <= MAX_POINTS
				memmove(alt, alt + drop, (keep - drop) * sizeof(alt[0]));
				keep -= drop;
				startRem -= static_cast<uint32_t>(drop) * step;
			}
		}
	}
	if (keep == 0) {
		// Nothing to join it to (a join at k == 0 is a replacement, too -- but of the same
		// stretch, so the climbs found in it stay; a new stretch starts from scratch).
		if (!(count > 0 && stepM == step && startRemainingM == startRem)) {
			cur = Segment();
			prev = Segment();
		}
		step = stepM;
		startRem = startRemainingM;
	}

	int16_t a = baseAltDm;
	alt[keep] = a;
	for (uint16_t i = 0; i < n; i++) {
		a += deltasDm[i] * deltaScaleDm;
		alt[keep + 1 + i] = a;
	}
	count = keep + n + 1;
	shown = true;
	pointsVersion++;
	update();
}

void Tracker::clearProfile() {
	// The climb the rider is on may go on behind a flat step: findClimb() picks it up from prev.
	if (cur.valid && posKnown && remM <= cur.footRem) prev = cur;
	cur = Segment();
	if (shown) pointsVersion++;
	shown = false;
	update();
}

void Tracker::reset() {
	count = 0;
	shown = false;
	posKnown = false;
	cur = Segment();
	prev = Segment();
	pointsVersion++;
	update();
}

void Tracker::setRemaining(float remainingM) {
	remM = remainingM;
	posKnown = true;
	update();
}

void Tracker::clearRemaining() {
	posKnown = false;
	update();
}

float Tracker::altitudeAtM(float offsetM) const {
	if (count == 0) return NAN;
	float x = offsetM / step;
	if (x <= 0) return alt[0] / 10.0f;
	if (x >= count - 1) return alt[count - 1] / 10.0f;
	const int32_t i = static_cast<int32_t>(x);
	const float t = x - i;
	return (alt[i] + t * (alt[i + 1] - alt[i])) / 10.0f;
}

// ******************** Climbs ********************

// Walks on from the summit so far (fromIndex) as long as the profile keeps rising. The end of
// the profile is a summit, too: that is where the phone ends it.
void Tracker::scanSummit(Segment& seg, int32_t fromIndex) {
	int32_t c = fromIndex;
	for (int32_t i = c + 1; i < count; i++) {
		const float distM = static_cast<float>(i - c) * step;
		const float riseM = (alt[i] - alt[c]) / 10.0f;
		if (riseM > 0 && riseM >= cfg.contGradePct / 100.0f * distM) {
			c = i;
		} else if (-riseM > cfg.summitDipM || distM > cfg.summitFlatM) {
			break;
		}
	}
	seg.summitRem = remOfIndex(c);
	seg.summitAltDm = alt[c];
}

// The next climb at or ahead of the rider.
void Tracker::findClimb(float posM) {
	const int32_t last = count - 1;
	int32_t window = cfg.startWindowM / step;
	if (window < 2) window = 2;

	int32_t foot = -1, windowEnd = 0;
	for (int32_t i = static_cast<int32_t>(posM / step); i + 2 <= last; i++) {
		const int32_t j = (i + window < last) ? i + window : last;
		// altitudes in dm: dm / 10 / m * 100 = %
		if ((alt[j] - alt[i]) * 10.0f / (static_cast<float>(j - i) * step) >= cfg.startGradePct) {
			foot = i;
			windowEnd = j;
			break;
		}
	}
	if (foot < 0) return;
	// The window may start on the flat before the rise
	while (foot + 1 < windowEnd && (alt[foot + 1] - alt[foot]) * 10.0f / step < cfg.contGradePct) foot++;

	Segment seg;
	seg.valid = true;
	seg.footRem = remOfIndex(foot);
	seg.footAltDm = alt[foot];
	scanSummit(seg, foot);
	if (seg.summitRem >= seg.footRem) return;

	// Close behind the last climb, and no real descent in between: the same climb goes on
	// (a flat step, on which the phone also takes its profile back for a moment).
	bool continued = false;
	if (prev.valid && prev.summitRem <= seg.footRem) {
		continued = true;		// this foot lies on the last climb, before its summit
	} else if (prev.valid && prev.summitRem - seg.footRem <= static_cast<int32_t>(cfg.summitFlatM)) {
		const float dipLimitDm = cfg.summitDipM * 10.0f;
		continued = prev.summitAltDm - seg.footAltDm <= dipLimitDm;
		const int32_t from = indexOfRem(prev.summitRem);
		for (int32_t i = (from < 0 ? 0 : from); continued && i < foot; i++) {
			if (prev.summitAltDm - alt[i] > dipLimitDm) continued = false;
		}
	}
	if (continued) {
		seg.footRem = prev.footRem;
		seg.footAltDm = prev.footAltDm;
		seg.id = prev.id;
	} else {
		seg.id = ++lastId;
	}
	prev = Segment();
	cur = seg;
}

uint8_t Tracker::rate(const Segment& seg) const {
	const float lengthM = static_cast<float>(seg.footRem - seg.summitRem);
	const float gainM = (seg.summitAltDm - seg.footAltDm) / 10.0f;
	if (lengthM <= 0 || lengthM < cfg.minLengthM) return RANK_NONE;
	const float avgPct = gainM / lengthM * 100.0f;
	if (avgPct < cfg.minAvgGradePct) return RANK_NONE;
	const float score = lengthM * avgPct;
	uint8_t rank = RANK_NONE;
	for (uint8_t k = 0; k < RANK_COUNT - 1; k++) {
		if (score >= cfg.catScore[k]) rank = k + 1;
	}
	return rank;
}

void Tracker::update() {
	st = Status();
	st.hasProfile = shown && count >= 2;
	if (!st.hasProfile) return;

	const float lengthM = static_cast<float>(count - 1) * step;
	float posM = static_cast<float>(startRem) - remM;
	// PROTOCOL.md: outside the profile it is stale or not reached yet. One step of slack, as
	// the profile is cut at the raster point nearest to the rider.
	if (!posKnown || posM < -static_cast<float>(step) || posM > lengthM + step) {
		cur = Segment();
		return;
	}
	if (posM < 0) posM = 0;
	if (posM > lengthM) posM = lengthM;
	st.positionValid = true;
	st.posM = posM;
	st.altM = altitudeAtM(posM);

	const float look = cfg.lookAheadM;
	if (posM + look <= lengthM) {
		st.aheadGradePct = (altitudeAtM(posM + look) - st.altM) / look * 100.0f;
	} else if (lengthM - posM >= step) {
		st.aheadGradePct = (altitudeAtM(lengthM) - st.altM) / (lengthM - posM) * 100.0f;
	}

	if (cur.valid) {
		const int32_t summitIndex = indexOfRem(cur.summitRem);
		if (summitIndex < 0 || summitIndex >= count) {
			cur = Segment();					// its points are gone
		} else {
			scanSummit(cur, summitIndex);		// more of the profile may have arrived (a climb longer than one frame)
			if (remM <= cur.summitRem - static_cast<int32_t>(cfg.summitPassM)) {
				prev = cur;
				cur = Segment();
			}
		}
	}
	if (!cur.valid) findClimb(posM);
	if (!cur.valid) return;

	st.active = true;
	st.climbId = cur.id;
	st.rank = rate(cur);
	st.footM = static_cast<float>(static_cast<int32_t>(startRem) - cur.footRem);
	st.summitM = static_cast<float>(static_cast<int32_t>(startRem) - cur.summitRem);
	st.lengthM = st.summitM - st.footM;
	st.totalAscentM = (cur.summitAltDm - cur.footAltDm) / 10.0f;
	st.summitAltM = cur.summitAltDm / 10.0f;
	st.avgGradePct = st.lengthM > 0 ? st.totalAscentM / st.lengthM * 100.0f : 0;
	st.toFootM = st.footM > posM ? st.footM - posM : 0;
	st.toSummitM = st.summitM > posM ? st.summitM - posM : 0;
	st.onClimb = posM >= st.footM && posM <= st.summitM;
	if (st.toFootM > 0) {
		st.remainingAscentM = st.totalAscentM;
	} else {
		st.remainingAscentM = st.summitAltM - st.altM;
		if (st.remainingAscentM < 0 || st.toSummitM <= 0) st.remainingAscentM = 0;
		if (st.remainingAscentM > st.totalAscentM) st.remainingAscentM = st.totalAscentM;	// in a dip below the foot
		if (st.totalAscentM > 0) st.doneFraction = 1.0f - st.remainingAscentM / st.totalAscentM;
	}
}

}	// namespace Climb
