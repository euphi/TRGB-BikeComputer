/*
 * RouteOverview.cpp
 *
 * See RouteOverview.h.
 */

#include "RouteOverview.h"

#include <stdint.h>
#include <string.h>

namespace RouteOv {

namespace {

uint32_t le32(const uint8_t* p) {
	return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

uint16_t le16(const uint8_t* p) {
	return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

// Length of the UTF-8 sequence that starts with this byte (1 for anything that is not a lead byte)
uint8_t sequenceLength(uint8_t lead) {
	if (lead >= 0xF0 && lead <= 0xF7) return 4;
	if (lead >= 0xE0) return lead <= 0xEF ? 3 : 1;
	if (lead >= 0xC0) return 2;
	return 1;
}

// The name as a C string: at most OVERVIEW_NAME_MAX byte, control characters replaced by a space, a
// character that was cut off at the end dropped, trailing spaces trimmed.
void copyName(char* dst, const uint8_t* src, size_t n) {
	if (n > OVERVIEW_NAME_MAX) n = OVERVIEW_NAME_MAX;
	size_t keep = n;
	size_t i = n;
	while (i > 0 && (src[i - 1] & 0xC0) == 0x80) i--;			// back over continuation bytes to the lead byte
	if (i > 0 && src[i - 1] >= 0xC0 && n - (i - 1) < sequenceLength(src[i - 1])) keep = i - 1;
	for (size_t k = 0; k < keep; k++) dst[k] = src[k] < 0x20 || src[k] == 0x7F ? ' ' : static_cast<char>(src[k]);
	while (keep > 0 && dst[keep - 1] == ' ') keep--;
	dst[keep] = '\0';
}

// Distances are metres of a route, far from 2^31; garbage in a frame must not wrap
int32_t clamp32(int64_t v) {
	return v > INT32_MAX ? INT32_MAX : v < INT32_MIN ? INT32_MIN : static_cast<int32_t>(v);
}

void clearOverview(Overview& o) {
	o.revision = 0;
	o.waypointsAhead = 0;
	o.climbsTotal = 0;
	o.hasDestination = false;
	o.count = 0;
	o.dropped = 0;
}

void setWaypoint(Entry& e, const uint8_t* v, uint8_t len) {
	e.kind = KIND_WAYPOINT;
	e.atM = le32(v + 1);
	e.timeAtS = le32(v + 5);
	e.number = 0;
	e.gainM = 0;
	e.lengthM = 0;
	copyName(e.name, v + OVERVIEW_WAYPOINT_FIXED_LEN, len - OVERVIEW_WAYPOINT_FIXED_LEN);
}

void setClimb(Entry& e, const uint8_t* v) {
	e.kind = KIND_CLIMB;
	e.name[0] = '\0';
	e.number = v[0];
	e.gainM = le16(v + 1);
	e.lengthM = le32(v + 3);
	e.atM = le32(v + 7);
	e.timeAtS = TIME_UNKNOWN;
}

}	// namespace

FrameResult parseFrame(const uint8_t* data, size_t length, Overview& out) {
	if (length < 2 || data[0] != OVERVIEW_PROTOCOL_VERSION) return FRAME_INVALID;
	switch (data[1]) {
	case OVERVIEW_MSG_HELLO: return FRAME_HELLO;
	case OVERVIEW_MSG_OVERVIEW_NONE: return FRAME_NONE;
	case OVERVIEW_MSG_OVERVIEW: break;
	default: return FRAME_INVALID;
	}

	clearOverview(out);
	size_t pos = 2;
	while (pos + 2 <= length) {
		const uint8_t tag = data[pos];
		const uint8_t len = data[pos + 1];
		pos += 2;
		if (pos + len > length) break;		// cut off: keep what is there
		const uint8_t* v = data + pos;
		switch (tag) {
		case OVERVIEW_TAG_REVISION:
			if (len >= 1) out.revision = v[0];
			break;
		case OVERVIEW_TAG_WAYPOINTS_AHEAD:
			if (len >= 1) out.waypointsAhead = v[0];
			break;
		case OVERVIEW_TAG_CLIMBS_TOTAL:
			if (len >= 1) out.climbsTotal = v[0];
			break;
		case OVERVIEW_TAG_WAYPOINT:
			if (len >= OVERVIEW_WAYPOINT_FIXED_LEN) {
				if (v[0] & OVERVIEW_WAYPOINT_FLAG_DESTINATION) {
					setWaypoint(out.destination, v, len);
					out.hasDestination = true;
				} else if (out.count < MAX_ENTRIES) {
					setWaypoint(out.entries[out.count++], v, len);
				} else {
					out.dropped++;
				}
			}
			break;
		case OVERVIEW_TAG_CLIMB:
			if (len >= OVERVIEW_CLIMB_LEN) {
				if (out.count < MAX_ENTRIES) {
					setClimb(out.entries[out.count++], v);
				} else {
					out.dropped++;
				}
			}
			break;
		default:
			break;		// unknown tag: skipped by its length
		}
		pos += len;
	}
	if (out.revision == 0) {		// can't be matched to the nav frames
		clearOverview(out);
		return FRAME_INVALID;
	}
	return FRAME_OVERVIEW;
}

// ******************** Tracker ********************

void Tracker::onNavFrame(bool hasRevision, uint8_t revision, bool hasRemaining, uint32_t remainingM, uint32_t remainingTimeS, int64_t epoch) {
	if (hasRevision) {
		wanted = revision;
	} else {
		wanted = 0;
		if (ov.revision != 0) {		// navigation from OsmAnd (or the route was left for another source)
			clearOverview(ov);
			ver++;
		}
	}
	haveNav = hasRemaining;
	if (hasRemaining) {
		navRemM = remainingM;
		navRemTimeS = remainingTimeS;
		navEpoch = epoch;
	}
}

void Tracker::onGone() {
	wanted = 0;
	haveNav = false;
	attempted = false;
	if (ov.revision != 0) {
		clearOverview(ov);
		ver++;
	}
}

FrameResult Tracker::feedFrame(const uint8_t* data, size_t length) {
	const uint8_t before = ov.revision;
	const FrameResult result = parseFrame(data, length, ov);
	switch (result) {
	case FRAME_OVERVIEW:
		ver++;
		break;
	case FRAME_NONE:
		if (before != 0) {
			clearOverview(ov);
			ver++;
		}
		break;
	default:
		if (before != 0 && ov.revision == 0) ver++;		// an OVERVIEW frame without REVISION wiped it
		break;
	}
	return result;
}

bool Tracker::readWanted(uint32_t nowMs, uint8_t& revision) const {
	if (wanted == 0 || wanted == ov.revision) return false;
	if (attempted && attemptedFor == wanted && static_cast<uint32_t>(nowMs - attemptMs) < RETRY_MS) return false;
	revision = wanted;
	return true;
}

void Tracker::readAttempted(uint32_t nowMs) {
	attempted = true;
	attemptMs = nowMs;
	attemptedFor = wanted;
}

bool Tracker::buildView(float remainingM, View& view) const {
	view.valid = false;
	view.count = 0;
	view.hasDestination = false;
	view.shortened = false;
	if (!haveNav || ov.revision == 0) return false;

	const int64_t now = static_cast<int64_t>(remainingM < 0 ? 0 : remainingM + 0.5f);
	view.revision = ov.revision;
	view.waypointsAhead = ov.waypointsAhead;
	view.climbsTotal = ov.climbsTotal;
	view.navEpoch = navEpoch;

	// Time of a waypoint: the frame's remaining time less the anchor, else the frame's remaining
	// time spread over the distance (frame values both, so it does not change between two frames)
	auto timeTo = [this](const Entry& e) -> uint32_t {
		if (e.timeAtS != TIME_UNKNOWN) return navRemTimeS > e.timeAtS ? navRemTimeS - e.timeAtS : 0;
		if (navRemM == 0 || e.atM >= navRemM) return 0;
		return static_cast<uint32_t>(static_cast<uint64_t>(navRemTimeS) * (navRemM - e.atM) / navRemM);
	};
	auto fill = [&](Row& r, const Entry& e) {
		r.kind = e.kind;
		r.destination = false;
		strncpy(r.name, e.name, sizeof(r.name));
		r.name[sizeof(r.name) - 1] = '\0';
		r.number = e.number;
		r.gainM = e.gainM;
		r.lengthM = e.lengthM;
		r.distM = clamp32(now - e.atM);
		r.summitM = clamp32(static_cast<int64_t>(r.distM) + e.lengthM);
		r.timeS = e.kind == KIND_WAYPOINT ? timeTo(e) : TIME_UNKNOWN;
	};

	if (ov.hasDestination) {
		view.hasDestination = true;
		fill(view.destination, ov.destination);
		view.destination.destination = true;
		if (view.destination.distM < 0) view.destination.distM = 0;
	}

	uint8_t listedWaypoints = 0, reachedWaypoints = 0, lastClimb = 0;
	bool cutByRows = false;
	for (uint8_t i = 0; i < ov.count; i++) {
		const Entry& e = ov.entries[i];
		if (e.kind == KIND_WAYPOINT) {
			listedWaypoints++;
		} else if (e.number > lastClimb) {
			lastClimb = e.number;
		}
		if (view.count >= MAX_VIEW_ROWS) {		// full: what is left is the far end
			cutByRows = true;
			break;
		}
		Row& r = view.rows[view.count];
		fill(r, e);
		// Reached: a waypoint behind the rider, a climb whose summit is. A newer overview is on its way.
		if (e.kind == KIND_WAYPOINT ? r.distM < 0 : r.summitM <= 0) {
			if (e.kind == KIND_WAYPOINT) reachedWaypoints++;
			continue;
		}
		view.count++;
	}
	view.waypointsLeft = ov.waypointsAhead == 255 ? 255 : ov.waypointsAhead > reachedWaypoints ? ov.waypointsAhead - reachedWaypoints : 0;
	// The last climb listed is the last one of the route when nothing was cut; none listed says nothing (all ridden)
	view.shortened = cutByRows || ov.dropped > 0 || listedWaypoints < ov.waypointsAhead || (lastClimb != 0 && lastClimb < ov.climbsTotal);
	view.valid = true;
	return true;
}

}	// namespace RouteOv
