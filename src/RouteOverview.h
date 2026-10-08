/*
 * RouteOverview.h
 *
 * What is still ahead on TrailBridge's GPX route: the destination, the waypoints (wpt of the
 * GPX file, with names) and the climbs of the whole route. Pure algorithm and data, no hardware
 * and no BLE -- the host test is test/native_routeoverview/routeoverview_test.cpp. The
 * connection to BLE and the screen is RouteMonitor.h.
 *
 * Wire format: BikeOverviewProtocol.h (PROTOCOL.md "Streckenübersicht-Service"). The service
 * is read only; the signal "read again" is the tag OVERVIEW_REVISION in the nav frames.
 *
 * The overview holds anchors, not distances: "remaining route distance (and time) at this
 * waypoint". Distance and time to an entry follow from the last nav frame:
 *
 *   distance = REMAINING_DISTANCE_M - REMAINING_AT_M          < 0: reached, not shown
 *   time     = REMAINING_TIME_S - REMAINING_TIME_AT_S         (time anchor known)
 *   time     = REMAINING_TIME_S * distance / REMAINING_DISTANCE_M     (anchor unknown)
 *   climb:   distance to the foot = REMAINING_DISTANCE_M - FOOT_REMAINING_AT_M, <= 0 inside it,
 *            to the summit: that + length. Climbs have no time anchor.
 *
 * The distance is carried on between two nav frames by the caller (wheel sensor) and passed to
 * buildView(); the time is that of the last nav frame, so an arrival clock time derived from
 * it (nav frame's wall clock + time) does not creep.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "BikeOverviewProtocol.h"

namespace RouteOv {

// The frame is 512 byte at most, an entry takes 11 byte at least (a name of ten characters: 21-23): about
// 25 fit, 45 at the very most; the destination is kept apart. What does not fit here is the far end.
static constexpr uint8_t MAX_ENTRIES = 40;
// Rows the screen can be given: nearest first, so the far end is what is cut (View::shortened).
static constexpr uint8_t MAX_VIEW_ROWS = 24;
static constexpr uint32_t TIME_UNKNOWN = OVERVIEW_TIME_UNKNOWN;

enum Kind : uint8_t {KIND_WAYPOINT = 0, KIND_CLIMB = 1};

struct Entry {
	Kind kind = KIND_WAYPOINT;
	char name[OVERVIEW_NAME_MAX + 1] = {};		// waypoints; UTF-8, control characters replaced, cut on a character boundary
	uint32_t atM = 0;							// remaining route distance at the waypoint / at the foot of the climb
	uint32_t timeAtS = TIME_UNKNOWN;			// waypoints: remaining time at the waypoint
	uint8_t number = 0;							// climbs: 1-based over the whole route
	uint16_t gainM = 0;							// climbs: height from foot to summit
	uint32_t lengthM = 0;						// climbs: length from foot to summit
};

// One frame, as sent. Waypoints and climbs are in the order the rider reaches them (a climb counts at
// its foot); the destination, which is always sent first, is kept apart.
struct Overview {
	uint8_t revision = 0;						// 0: none
	uint8_t waypointsAhead = 0;					// without the destination; 255 = 255 or more
	uint8_t climbsTotal = 0;
	bool hasDestination = false;
	Entry destination;
	uint8_t count = 0;
	Entry entries[MAX_ENTRIES];
	uint16_t dropped = 0;						// entries that did not fit into entries[]
};

enum FrameResult : uint8_t {FRAME_INVALID, FRAME_HELLO, FRAME_NONE, FRAME_OVERVIEW};

// Parses a frame into `out` (only touched by an OVERVIEW frame). Unknown tags are skipped by their
// length; a last TLV that is cut off ends the parse and keeps what was read before. A frame without
// REVISION is FRAME_INVALID (and `out` is cleared: it could not be matched to the nav frames).
FrameResult parseFrame(const uint8_t* data, size_t length, Overview& out);

// One line of the list as the screen shows it.
struct Row {
	Kind kind = KIND_WAYPOINT;
	bool destination = false;
	char name[OVERVIEW_NAME_MAX + 1] = {};
	int32_t distM = 0;							// to the waypoint / the foot of the climb; <= 0 for a climb: the rider is in it
	int32_t summitM = 0;						// climbs: to the summit
	uint32_t timeS = TIME_UNKNOWN;				// waypoints: from the last nav frame; TIME_UNKNOWN for a climb
	uint8_t number = 0;							// climbs
	uint16_t gainM = 0;
	uint32_t lengthM = 0;
};

struct View {
	bool valid = false;							// an overview and a nav frame are there
	uint8_t revision = 0;
	uint8_t waypointsAhead = 0;					// as announced with the overview
	uint8_t waypointsLeft = 0;					// ... less those the rider has reached since (255 = 255 or more)
	uint8_t climbsTotal = 0;
	bool shortened = false;						// the far end is missing: the frame left it out, or rows[] is full
	int64_t navEpoch = 0;						// wall clock (s since 1970) of the nav frame the times belong to; 0 = unknown
	bool hasDestination = false;
	Row destination;
	uint8_t count = 0;
	Row rows[MAX_VIEW_ROWS];					// nearest first; entries that are reached are left out
};

class Tracker {
public:
	// A nav frame. revision: OVERVIEW_REVISION (hasRevision false: navigation from OsmAnd, no
	// overview -- the one held is dropped). hasRemaining false: the frame has no
	// REMAINING_DISTANCE_M. epoch: the wall clock in seconds, 0 if it is not set.
	void onNavFrame(bool hasRevision, uint8_t revision, bool hasRemaining, uint32_t remainingM, uint32_t remainingTimeS, int64_t epoch);
	// NAV_NONE, or the phone is gone: overview and nav state go.
	void onGone();
	// The characteristic's value. FRAME_NONE drops the overview, FRAME_OVERVIEW replaces it.
	FrameResult feedFrame(const uint8_t* data, size_t length);

	// Must the characteristic be read? The revision announced by the nav frames is not the one held;
	// after a read that did not help (attempted()) it waits retryMs before it asks again.
	bool readWanted(uint32_t nowMs, uint8_t& revision) const;
	void readAttempted(uint32_t nowMs);
	static constexpr uint32_t RETRY_MS = 4000;

	// remainingM: the rider's remaining route distance now (the nav frame's, carried on by the
	// wheel). Returns false (view.valid false) if there is nothing to show yet.
	bool buildView(float remainingM, View& view) const;

	bool hasOverview() const {return ov.revision != 0;}
	uint8_t revision() const {return ov.revision;}
	uint8_t announcedRevision() const {return wanted;}
	bool navValid() const {return haveNav;}
	uint32_t navRemainingM() const {return navRemM;}
	// Changes whenever the stored overview does.
	uint32_t version() const {return ver;}
	const Overview& overview() const {return ov;}

private:
	Overview ov;
	uint8_t wanted = 0;							// OVERVIEW_REVISION of the latest nav frame, 0 = none
	bool haveNav = false;
	uint32_t navRemM = 0, navRemTimeS = 0;
	int64_t navEpoch = 0;
	uint32_t ver = 0;
	uint32_t attemptMs = 0;
	bool attempted = false;						// attemptMs is valid, for the revision `attemptedFor`
	uint8_t attemptedFor = 0;
};

}	// namespace RouteOv
