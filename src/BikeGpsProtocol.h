/*
 * BikeGpsProtocol.h
 *
 * Wire format constants for the TrailBridge GPS-Positions-Service BLE
 * protocol v1. Authoritative source: ../TrailBridge/PROTOCOL.md, section
 * "GPS-Positions-Service" -- do not change these values without updating
 * that spec first, it is the contract between the TrailBridge Android app
 * (peripheral) and this firmware (central).
 *
 * This is a second, independent service on the same peer as the
 * BikeNavRelay/TrailBridge nav service (see BikeNavProtocol.h) -- it is not
 * advertised and only shows up via GATT service discovery once connected to
 * the nav service.
 */

#pragma once

#include <stdint.h>

#define GPS_PROTOCOL_VERSION 1

// Frame byte 1: message type
typedef enum {
	GPS_MSG_HELLO = 0x00,
	GPS_MSG_POSITION_UPDATE = 0x01,
	GPS_MSG_POSITION_NONE = 0x02
} EGpsMessageType;

// TLV tags carried after byte 1 when message type is POSITION_UPDATE
typedef enum {
	GPS_TAG_LATITUDE_E7 = 0x01,			// int32 LE, degrees * 1e7, always present
	GPS_TAG_LONGITUDE_E7 = 0x02,			// int32 LE, degrees * 1e7, always present
	GPS_TAG_ALTITUDE_M = 0x03,				// int32 LE, meters above ellipsoid, only if the fix has an altitude
	GPS_TAG_SPEED_CMS = 0x04,				// uint32 LE, cm/s, only if the fix has a speed
	GPS_TAG_BEARING_DEG_X100 = 0x05,		// uint16 LE, degrees * 100 (0..35999), only if the fix has a bearing
	GPS_TAG_ACCURACY_M_X10 = 0x06,			// uint16 LE, meters * 10 (horizontal accuracy), only if the fix has one
	GPS_TAG_FIX_AGE_MS = 0x07				// uint32 LE, ms since this fix, always present
} EGpsTlvTag;

// In-memory representation of the latest parsed GPS fix, shared between
// BLEDevices (producer, parses the BLE frame) and BCLogger (consumer,
// snapshots it into the binary datalog every dataStore() cycle).
//
// "valid" only means a POSITION_UPDATE was received at all (GPS on, fix
// available) -- it does NOT mean the fix is fresh. fixAgeMs must be checked
// separately, since the phone resends the same fix on every heartbeat while
// waiting for a new one (e.g. in a tunnel). That staleness judgement is left
// to the offline GPX conversion script, not decided here.
struct SGpsFix {
	bool valid = false;
	int32_t latitudeE7 = 0;
	int32_t longitudeE7 = 0;
	bool hasAltitude = false;
	int32_t altitudeM = 0;
	bool hasSpeed = false;
	uint32_t speedCms = 0;
	bool hasBearing = false;
	uint16_t bearingDegX100 = 0;
	bool hasAccuracy = false;
	uint16_t accuracyMX10 = 0;
	uint32_t fixAgeMs = 0;
};
