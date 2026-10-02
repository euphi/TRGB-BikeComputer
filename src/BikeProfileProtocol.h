/*
 * BikeProfileProtocol.h
 *
 * Wire format constants for the TrailBridge elevation-profile service v1 ("Höhenprofil-
 * Service"). Authoritative source: ../TrailBridge/PROTOCOL.md -- do not change these values
 * without updating that spec first.
 *
 * Third, independent service on the same peer as the navigation service (DEV_NAV in
 * BLEDevices.cpp), not advertised, its own characteristic/subscription. Event-driven, no
 * heartbeat: TrailBridge sends a profile when a climb is ahead on its GPX route, the next
 * stretch on long climbs once the rider is past the middle of the previous one, and
 * PROFILE_NONE when the climb is over, the route ends or the rider leaves the route.
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
	PROFILE_TAG_STEP_M = 0x02,						// uint8, distance between profile points in metres (25 today)
	PROFILE_TAG_BASE_ALT_DM = 0x03,					// int16 LE, altitude of the first point in decimetres
	PROFILE_TAG_DELTAS_DM = 0x04					// N x int8: altitude change in dm from point k to k+1 (N+1 points)
} EProfileTlvTag;

// Point k lies at START_REMAINING_DISTANCE_M - k * STEP_M of remaining route distance. The
// rider is at START_REMAINING_DISTANCE_M - REMAINING_DISTANCE_M (nav frame, tag 0x08) metres
// from the first point -- along the route, no odometer needed, survives reconnects.
#define PROFILE_MAX_DELTAS 200				// what TrailBridge sends at most per frame (5 km at 25 m)
