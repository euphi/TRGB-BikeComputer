/*
 * BikeOverviewProtocol.h
 *
 * Wire format constants for the TrailBridge route-overview service v1 ("Streckenübersicht-
 * Service"). Authoritative source: ../TrailBridge/PROTOCOL.md -- do not change these values
 * without updating that spec first.
 *
 * Fourth, independent service on the same peer as the navigation service (DEV_NAV in
 * BLEDevices.cpp), not advertised. Unlike the others it is READ ONLY: no Indicate, no CCCD, no
 * heartbeat. What tells that there is something new is the tag OVERVIEW_REVISION (BikeNavProtocol.h)
 * in the nav frames; if it differs from the REVISION of the overview held, the characteristic is
 * read (a long read, up to 512 byte -- never from the indicate callback, readValue() blocks).
 * Distances and times are anchors ("remaining route distance at this point"); the firmware
 * subtracts them from the nav frame's REMAINING_DISTANCE_M / REMAINING_TIME_S (RouteOverview.h).
 */

#pragma once

#include <stdint.h>

#define OVERVIEW_PROTOCOL_VERSION 1

// Frame byte 1: message type
typedef enum {
	OVERVIEW_MSG_HELLO = 0x00,				// state after the app started, no overview
	OVERVIEW_MSG_OVERVIEW = 0x01,			// an overview, TLV entries follow
	OVERVIEW_MSG_OVERVIEW_NONE = 0x02		// no route active, no overview
} EOverviewMessageType;

// TLV tags carried after byte 1 when message type is OVERVIEW
typedef enum {
	OVERVIEW_TAG_REVISION = 0x01,			// uint8, 1..255 -- the OVERVIEW_REVISION of the nav frames this overview belongs to
	OVERVIEW_TAG_WAYPOINTS_AHEAD = 0x02,	// uint8, waypoints ahead without the destination (255 = 255 or more)
	OVERVIEW_TAG_CLIMBS_TOTAL = 0x03,		// uint8, climbs of the whole route (0 = none, or no altitude data)
	OVERVIEW_TAG_WAYPOINT = 0x04,			// 9 + N byte, a waypoint or the destination, repeated
	OVERVIEW_TAG_CLIMB = 0x05				// 11 byte, a climb, repeated
} EOverviewTlvTag;

// WAYPOINT value: flags(1) | REMAINING_AT_M u32 LE | REMAINING_TIME_AT_S u32 LE | name (UTF-8, <= 32 byte)
#define OVERVIEW_WAYPOINT_FIXED_LEN 9
#define OVERVIEW_WAYPOINT_FLAG_DESTINATION 0x01
#define OVERVIEW_TIME_UNKNOWN 0xFFFFFFFFu	// REMAINING_TIME_AT_S: the GPX has no timestamps
#define OVERVIEW_NAME_MAX 32

// CLIMB value: number(1) | gain m u16 LE | length m u32 LE | FOOT_REMAINING_AT_M u32 LE. A longer
// value is read up to here, the rest ignored.
#define OVERVIEW_CLIMB_LEN 11

// The frame is 512 byte at most. Fewest bytes per entry: a waypoint without a name 11, a climb 13.
