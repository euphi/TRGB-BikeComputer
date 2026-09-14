/*
 * BikeNavProtocol.h
 *
 * Wire format constants for the BikeNavRelay BLE navigation protocol v1.
 * Authoritative source: ../BikeNavRelay/PROTOCOL.md -- do not change these
 * values without updating that spec first, it is the contract between the
 * BikeNavRelay Android app (peripheral) and this firmware (central).
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
	NAV_TAG_REMAINING_TIME_S = 0x09			// uint32 LE, seconds to destination
} ENavTlvTag;

// Maneuver codes, independent of the old 32-slot Komoot icon index and of
// OsmAnd's internal TurnType constants. Reference implementation on the
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
