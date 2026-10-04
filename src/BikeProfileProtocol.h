/*
 * BikeProfileProtocol.h
 *
 * Wire format constants for the TrailBridge elevation-profile service v1 ("Höhenprofil-
 * Service"). Authoritative source: ../TrailBridge/PROTOCOL.md -- do not change these values
 * without updating that spec first.
 *
 * Third, independent service on the same peer as the navigation service (DEV_NAV in
 * BLEDevices.cpp), not advertised, its own characteristic/subscription. Event-driven, no
 * heartbeat: TrailBridge sends the profile of the road ahead on its GPX route, always, climb
 * or not (a rolling window; frames with PROFILE_FLAG_ROLLING). A climb in it -- the one the
 * rider is on or whose foot is within 500 m -- is announced by tags 0x07..0x0A with its whole
 * extent (foot and summit, also behind or beyond the frame), so category and length do not
 * change while it is being ridden. Without the flag (older TrailBridge) a frame ends at the
 * summit of a climb ahead and PROFILE_NONE comes at the summit; the firmware then finds the
 * climb itself. PROFILE_NONE in either case: the route ends or the rider leaves it.
 */

#pragma once

#include <stdint.h>

#define PROFILE_PROTOCOL_VERSION 1

// Frame byte 1: message type
typedef enum {
	PROFILE_MSG_HELLO = 0x00,				// state after connecting, no profile
	PROFILE_MSG_PROFILE_UPDATE = 0x01,		// a profile, TLV entries follow; replaces an earlier one
	PROFILE_MSG_PROFILE_NONE = 0x02			// don't show a (climb) profile any more
} EProfileMessageType;

// TLV tags carried after byte 1 when message type is PROFILE_UPDATE
typedef enum {
	PROFILE_TAG_START_REMAINING_DISTANCE_M = 0x01,	// uint32 LE, the route's remaining distance at the first profile point
	PROFILE_TAG_STEP_M = 0x02,						// uint8, distance between profile points in metres (25 .. 250)
	PROFILE_TAG_BASE_ALT_DM = 0x03,					// int16 LE, altitude of the first point in decimetres
	PROFILE_TAG_DELTAS_DM = 0x04,					// N x int8: altitude change from point k to k+1 (N+1 points), in DELTA_SCALE_DM dm
	PROFILE_TAG_DELTA_SCALE_DM = 0x05,				// uint8, optional: unit of the deltas in decimetres (1 if absent)
	PROFILE_TAG_FLAGS = 0x06,						// uint8, optional: PROFILE_FLAG_* bits
	PROFILE_TAG_CLIMB_FOOT_REMAINING_M = 0x07,		// uint32 LE, remaining route distance at the foot of the announced climb (may be behind the first point)
	PROFILE_TAG_CLIMB_SUMMIT_REMAINING_M = 0x08,	// uint32 LE, ... at its summit (may be beyond the last point)
	PROFILE_TAG_CLIMB_FOOT_ALT_DM = 0x09,			// int16 LE, altitude of the foot in decimetres
	PROFILE_TAG_CLIMB_SUMMIT_ALT_DM = 0x0A			// int16 LE, altitude of the summit in decimetres
} EProfileTlvTag;

// PROFILE_TAG_FLAGS
#define PROFILE_FLAG_ROLLING 0x01	// the profile is a rolling window of the road ahead; the climb is only the one announced by tags 0x07..0x0A (all four, or none = no climb close)

// Point k lies at START_REMAINING_DISTANCE_M - k * STEP_M of remaining route distance. The
// rider is at START_REMAINING_DISTANCE_M - REMAINING_DISTANCE_M (nav frame, tag 0x08) metres
// from the first point -- along the route, no odometer needed, survives reconnects.
#define PROFILE_MAX_DELTAS 200				// what TrailBridge sends at most per frame (5 km at 25 m .. 50 km at 250 m)
