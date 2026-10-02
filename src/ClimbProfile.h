/*
 * ClimbProfile.h
 *
 * Climbs from the elevation profile TrailBridge sends for its GPX route (PROTOCOL.md
 * "Höhenprofil-Service", constants in BikeProfileProtocol.h). Pure algorithm: no Arduino,
 * FreeRTOS or I/O -- ClimbMonitor feeds it frames and positions and hands the result to the
 * climb screen. That keeps it testable on the host (test/native_climb/).
 *
 * What the phone sends is the climb ahead: the altitudes from the rider -- at most 500 m
 * before the foot -- to the summit, in one frame. The raster is 25 m for up to 5 km and
 * coarser for longer climbs (up to 250 m). PROFILE_NONE comes at the summit. TrailBridge
 * finds foot and summit with the criteria below (ElevationProfile.java there: keep the
 * defaults of the two in sync), so the profile ends at the summit. This module
 *   - finds the climb in it: the foot (where it starts to rise by startGradePct), and the
 *     summit (the last point before a descent of more than summitDipM, or before
 *     summitFlatM without a mean rise of contGradePct -- so a short flat or dip does not end
 *     a climb -- or the end of the profile),
 *   - joins frames into one profile if they are on the same raster and overlap, keeping
 *     what was ridden (a climb longer than one frame even at 250 m, or a small MTU),
 *   - carries a climb on over a PROFILE_NONE followed shortly by a new profile,
 *   - rates it: score = length [m] x mean gradient [%] (= 100 x height gain), the scale
 *     Strava and Garmin use for categories 4..1 and HC, continued downwards by halving for
 *     5 and 6. Below category 6, shorter than minLengthM or flatter than minAvgGradePct it
 *     is a hill: not rated (rank 0), not shown.
 *
 * Distances along the route are "remaining distance to the destination" in metres, as in
 * the protocol: it decreases while riding, and the foot of a climb has a larger value than
 * its summit. The rider's is the REMAINING_DISTANCE_M of the nav frame.
 *
 * Height figures are net: summit minus foot, summit minus rider. With dips of at most
 * summitDipM inside a climb that is within a few metres of the summed ascent, and it needs
 * no history.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>

namespace Climb {

static constexpr uint16_t MAX_POINTS = 321;		// 8 km at the 25 m raster; older points are dropped

// Rank of a climb: 0 = not rated (a hill), 1 = category 6 ... 6 = category 1, 7 = HC
enum : uint8_t {RANK_NONE = 0, RANK_CAT6 = 1, RANK_CAT5, RANK_CAT4, RANK_CAT3, RANK_CAT2, RANK_CAT1, RANK_HC, RANK_COUNT};
const char* rankLabel(uint8_t rank);			// "", "6" .. "1", "HC"

static constexpr uint8_t GRADE_BANDS = 5;		// colour bands of the profile, see Config::gradeBandPct

struct Config {
	// --- Finding the climb in the profile ---
	float startGradePct = 3.0f;			// foot: the first point from which the next startWindowM rise by this much on average
	uint16_t startWindowM = 100;
	float contGradePct = 0.5f;			// the climb goes on to a later point if it rises by this much on average from the summit so far ...
	uint16_t summitFlatM = 2000;			// ... and lies no further than this behind it. Also: a new climb starting this close behind the last one continues it
	float summitDipM = 30.0f;			// a descent of more than this ends the climb
	uint16_t summitPassM = 50;			// the climb is over once the rider is this far past the summit
	// --- Rating ---
	uint16_t minLengthM = 300;			// shorter: a hill, not rated
	float minAvgGradePct = 3.0f;		// flatter on average: not rated
	uint32_t catScore[RANK_COUNT - 1] = {2000, 4000, 8000, 16000, 32000, 64000, 80000};	// length [m] x mean gradient [%] for category 6, 5, 4, 3, 2, 1, HC
	// --- Display ---
	bool autoShow = true;				// switch to the climb screen by itself
	uint8_t autoShowMinRank = RANK_CAT6;	// ... for climbs of at least this rank (1 = category 6 .. 7 = HC)
	uint16_t showAheadM = 300;			// ... once the foot is this close
	uint16_t hideDelayS = 5;			// ... and back this long after the climb is over (summit passed, or the phone took the profile back)
	uint16_t lookAheadM = 25;			// "gradient ahead": mean over this distance from the rider
	float gradeBandPct[GRADE_BANDS - 1] = {1.0f, 4.0f, 7.0f, 10.0f};	// profile colour: below [0] flat/downhill, then four bands of increasing steepness
	uint8_t infoCycleS = 4;				// the rotating info field (time, distance, temperature, cadence, altitude) changes this often
};

// Settings by name, for the serial console, the web page and NVS -- one table instead of a
// getter/setter per field. Values are passed as float (all of them fit exactly).
struct ParamInfo {
	const char* name;					// also the NVS key: at most 15 characters
	const char* unit;
	float min, max;
	const char* help;
};
uint8_t paramCount();
const ParamInfo& paramInfo(uint8_t index);
int8_t paramFind(const char* name);					// -1 if unknown; case-insensitive
float paramGet(const Config& cfg, uint8_t index);
bool paramSet(Config& cfg, uint8_t index, float value);	// false if out of range (cfg unchanged)

// 0 (flat or downhill) .. GRADE_BANDS - 1 (steepest)
uint8_t gradeBand(const Config& cfg, float gradePct);

struct Status {
	bool hasProfile = false;			// a profile to show (not after PROFILE_NONE)
	bool positionValid = false;			// the rider is inside it; nothing below is valid otherwise
	bool active = false;				// a climb is ahead or under the wheels (may be unrated: rank 0)
	bool onClimb = false;				// the rider is between foot and summit
	bool summitOpen = false;			// never set any more: TrailBridge sends the climb up to its summit (was: figures are "at least")
	uint8_t rank = RANK_NONE;
	uint16_t climbId = 0;				// changes with every new climb (a continued one keeps it)
	float posM = 0;						// rider, metres from the first profile point
	float footM = 0, summitM = 0;		// the climb in the same coordinate; footM < 0 if the foot is no longer in the profile
	float lengthM = 0;					// foot to summit
	float totalAscentM = 0;				// summit minus foot
	float remainingAscentM = 0;			// summit minus rider (the whole climb while approaching)
	float toFootM = 0;					// > 0 while approaching
	float toSummitM = 0;
	float avgGradePct = 0;				// foot to summit
	float aheadGradePct = NAN;			// mean over the next lookAheadM, NAN at the end of the profile
	float altM = NAN;					// profile altitude at the rider
	float summitAltM = NAN;
	float doneFraction = 0;				// 0..1 of the height gain behind the rider
};

class Tracker {
public:
	Config cfg;

	enum FrameResult : uint8_t {FRAME_INVALID, FRAME_HELLO, FRAME_PROFILE, FRAME_NONE};
	// One frame of the profile service (indicate payload or read value). Unknown TLV tags
	// are skipped by their length. FRAME_INVALID: wrong version, unknown type, broken or
	// incomplete profile -- state unchanged.
	FrameResult feedFrame(const uint8_t* data, size_t length);

	// PROFILE_UPDATE: n deltas = n + 1 points. Joined to the points already known if it is
	// on the same raster and overlaps them, else it replaces them.
	// The deltas are in units of deltaScaleDm decimetres.
	void setProfile(uint32_t startRemainingM, uint8_t stepM, int16_t baseAltDm, const int8_t* deltasDm, uint16_t n, uint8_t deltaScaleDm = 1);
	// PROFILE_NONE: nothing to show any more. The points and the climb the rider was on are
	// remembered, so a profile following shortly (flat step in a climb) continues it.
	void clearProfile();
	// Connection or route gone: forget everything.
	void reset();

	// The rider's remaining route distance (nav frame, or interpolated from it).
	void setRemaining(float remainingM);
	void clearRemaining();

	const Status& status() const {return st;}

	// The joined profile, for drawing. version() changes whenever the points do.
	uint32_t version() const {return pointsVersion;}
	uint16_t pointCount() const {return shown ? count : 0;}
	const int16_t* altitudesDm() const {return alt;}
	uint8_t stepM() const {return step;}
	uint32_t startRemainingM() const {return startRem;}
	float altitudeAtM(float offsetM) const;			// metres, linear between the points, clamped to the profile

private:
	struct Segment {
		bool valid = false;
		int32_t footRem = 0;			// remaining route distance at the foot ...
		int32_t summitRem = 0;			// ... and at the summit (smaller)
		int16_t footAltDm = 0;
		int16_t summitAltDm = 0;
		uint16_t id = 0;
	};

	int16_t alt[MAX_POINTS] = {};		// decimetres
	uint16_t count = 0;
	uint8_t step = 25;
	uint32_t startRem = 0;				// remaining route distance at alt[0]
	bool shown = false;
	bool posKnown = false;
	float remM = 0;
	uint32_t pointsVersion = 0;
	uint16_t lastId = 0;
	Segment cur, prev;					// the climb ahead/under the rider, and the one before (for continuing it)
	Status st;

	int32_t remOfIndex(int32_t i) const {return static_cast<int32_t>(startRem) - i * step;}
	int32_t indexOfRem(int32_t rem) const {return (static_cast<int32_t>(startRem) - rem) / step;}
	void update();
	void findClimb(float posM);
	void scanSummit(Segment& seg, int32_t fromIndex);
	uint8_t rate(const Segment& seg) const;
};

}	// namespace Climb
