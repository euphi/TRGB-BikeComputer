/*
 * BikeNavProtocol.h
 *
 * Wire format constants for the TrailBridge BLE navigation protocol v1.
 * Authoritative source: ../TrailBridge/PROTOCOL.md -- do not change these
 * values without updating that spec first, it is the contract between the
 * TrailBridge Android app (peripheral) and this firmware (central).
 */

#pragma once

#include <stdint.h>

#define BIKENAV_PROTOCOL_VERSION 1

// Frame byte 1: message type
typedef enum {
	NAV_MSG_HELLO = 0x00,
	NAV_MSG_NAV_UPDATE = 0x01,
	NAV_MSG_NAV_NONE = 0x02
} ENavMessageType;

// TLV tags carried after byte 1 when message type is NAV_UPDATE
typedef enum {
	NAV_TAG_MANEUVER = 0x01,				// 1 byte, maneuver code (ENavManeuver)
	NAV_TAG_MANEUVER_DISTANCE_M = 0x02,	// uint32 LE, meters
	NAV_TAG_ROUNDABOUT_EXIT = 0x03,			// 1 byte, 1-based exit number, only if maneuver == ROUNDABOUT
	NAV_TAG_STREET_NAME = 0x04,				// UTF-8, no null terminator, may be empty
	NAV_TAG_NEXT_MANEUVER = 0x05,			// 1 byte, maneuver code
	NAV_TAG_NEXT_MANEUVER_DISTANCE_M = 0x06,	// uint32 LE, meters (from the *next* maneuver on)
	NAV_TAG_NEXT_STREET_NAME = 0x07,		// UTF-8
	NAV_TAG_REMAINING_DISTANCE_M = 0x08,	// uint32 LE, meters to destination
	NAV_TAG_REMAINING_TIME_S = 0x09,			// uint32 LE, seconds to destination
	NAV_TAG_LANES = 0x0A,					// N (multiple of 4) bytes, lane list, see NavLane below
	NAV_TAG_NEXT_LANES = 0x0B,				// like 0x0A, but for the maneuver after next (like 0x05/0x06/0x07)
	NAV_TAG_LANE_DISTANCE_M = 0x0C,			// uint32 LE, meters - only present alongside tag 0x0A. Independent
												// of NAV_TAG_MANEUVER_DISTANCE_M: the point where a lane choice
												// becomes relevant (road forks into lanes) is typically well
												// before the maneuver itself, do not conflate the two.
	NAV_TAG_NEXT_LANE_DISTANCE_M = 0x0D		// uint32 LE, meters - like 0x0C, only present alongside tag 0x0B
} ENavTlvTag;

// One entry of a LANES/NEXT_LANES list (tag 0x0A/0x0B), left-to-right as in
// OsmAnd's own lane array. 4 bytes per lane - see PROTOCOL.md "Fahrspur-
// Informationen" for the full rationale (this deliberately does NOT mirror
// OsmAnd's bit-packed int[] layout, each direction is already translated to
// our own ENavManeuver codes on the app side).
typedef struct {
	uint8_t primary;	// ENavManeuver, main arrow direction for this lane - never NAV_MANEUVER_NONE
	uint8_t secondary;	// ENavManeuver, NAV_MANEUVER_NONE if this lane has no second direction
	uint8_t tertiary;	// ENavManeuver, NAV_MANEUVER_NONE if this lane has no third direction
	uint8_t flags;		// bit 0 = NAV_LANE_FLAG_ACTIVE, bits 1-7 reserved (currently 0)
} NavLane;

#define NAV_LANE_FLAG_ACTIVE 0x01

// PROTOCOL.md allows up to 63 lanes (1-byte TLV length / 4), real roads
// rarely exceed 6-8 - cap storage here rather than allocating for the
// theoretical worst case. Extra lanes beyond this are dropped, not an error.
#define NAV_LANES_MAX 8

// Maneuver codes, independent of OsmAnd's internal TurnType constants. Reference implementation on the
// Android side: Maneuver.java.
typedef enum {
	NAV_MANEUVER_NONE = 0,
	NAV_MANEUVER_DEPART = 1,
	NAV_MANEUVER_ARRIVE = 2,
	NAV_MANEUVER_STRAIGHT = 3,
	NAV_MANEUVER_TURN_SLIGHT_LEFT = 4,
	NAV_MANEUVER_TURN_LEFT = 5,
	NAV_MANEUVER_TURN_SHARP_LEFT = 6,
	NAV_MANEUVER_TURN_SLIGHT_RIGHT = 7,
	NAV_MANEUVER_TURN_RIGHT = 8,
	NAV_MANEUVER_TURN_SHARP_RIGHT = 9,
	NAV_MANEUVER_KEEP_LEFT = 10,
	NAV_MANEUVER_KEEP_RIGHT = 11,
	NAV_MANEUVER_UTURN_LEFT = 12,
	NAV_MANEUVER_UTURN_RIGHT = 13,
	NAV_MANEUVER_ROUNDABOUT = 14,
	NAV_MANEUVER_COUNT = 15,			// number of defined codes 0..14
	NAV_MANEUVER_UNKNOWN = 255
} ENavManeuver;

#ifdef __cplusplus
// Human-readable name, for logging. Defined inline so both BLEDevices.cpp
// and any future consumer can use it without a separate translation unit.
inline const char* navManeuverToString(uint8_t maneuver) {
	switch (maneuver) {
	case NAV_MANEUVER_NONE: return "NONE";
	case NAV_MANEUVER_DEPART: return "DEPART";
	case NAV_MANEUVER_ARRIVE: return "ARRIVE";
	case NAV_MANEUVER_STRAIGHT: return "STRAIGHT";
	case NAV_MANEUVER_TURN_SLIGHT_LEFT: return "TURN_SLIGHT_LEFT";
	case NAV_MANEUVER_TURN_LEFT: return "TURN_LEFT";
	case NAV_MANEUVER_TURN_SHARP_LEFT: return "TURN_SHARP_LEFT";
	case NAV_MANEUVER_TURN_SLIGHT_RIGHT: return "TURN_SLIGHT_RIGHT";
	case NAV_MANEUVER_TURN_RIGHT: return "TURN_RIGHT";
	case NAV_MANEUVER_TURN_SHARP_RIGHT: return "TURN_SHARP_RIGHT";
	case NAV_MANEUVER_KEEP_LEFT: return "KEEP_LEFT";
	case NAV_MANEUVER_KEEP_RIGHT: return "KEEP_RIGHT";
	case NAV_MANEUVER_UTURN_LEFT: return "UTURN_LEFT";
	case NAV_MANEUVER_UTURN_RIGHT: return "UTURN_RIGHT";
	case NAV_MANEUVER_ROUNDABOUT: return "ROUNDABOUT";
	default: return "UNKNOWN";
	}
}
#endif
