/*
 * LogRecords.h
 *
 * Binary record layouts of the data log (L_*.bin), format version 2.
 *
 * A log file is a bare sequence of fixed-size 64-byte records -- no header, no index. Every
 * record carries its type at offset 30 and the format version at offset 31. The version
 * byte stays where v1 (56-byte records, data only) had it, so a reader can tell the two
 * apart from the first record alone; unknown record types are skipped by their size.
 *
 * Reader: Tools/bikelog/record.py (LAYOUTS). Any change here needs the matching change
 * there, and a version bump -- the static_asserts below pin the offsets the reader uses.
 *
 * Conventions:
 *   - All multi-byte values little-endian (ESP32-S3), no padding anywhere.
 *   - timestamp is time_t = 8 bytes on this toolchain (see doc/PITFALLS.md), whole seconds;
 *     timestampMs adds the milliseconds.
 *   - "mg" = milli-g, 1 g = 9.80665 m/s^2.
 *   - Sentinels: GRAD_INVALID for a missing gradient, U16_INVALID for a missing unsigned
 *     value (speed, roughness).
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <ctime>

namespace LogRec {

static constexpr uint8_t FORMAT_VERSION = 2;
static constexpr size_t RECORD_SIZE = 64;
static constexpr size_t TYPE_OFFSET = 30;
static constexpr size_t VERSION_OFFSET = 31;

static constexpr int16_t GRAD_INVALID = INT16_MIN;
static constexpr uint16_t U16_INVALID = 0xFFFF;

enum Type : uint8_t {
	TYPE_DATA = 0,				// every 5 s: the ride data (as in v1, plus gradient sources and road class)
	TYPE_ROAD_QUALITY = 1,		// every interval (1..10 s): road-surface metrics
	TYPE_SHOCK = 2,				// on a hard hit
	TYPE_LABEL = 3,				// manual road label (RQ-Ride screen): on change, then every 60 s
	TYPE_RIDESTATE = 4,			// ride/drive-state change (Statistics::EDrivingState): on every change
};

// Manual road label: what the rider says the surface is, as ground truth for the automatic
// road class. Added to v2 without a version bump -- a new record type and two formerly
// reserved bytes in Shock, both 0 ("not set") in older files.
//
// Surface, and the OSM surface=* values each one stands for (for comparing with OSM):
enum Surface : uint8_t {
	SURFACE_UNSET = 0,
	SURFACE_ASPHALT = 1,		// asphalt, concrete
	SURFACE_GRAVEL = 2,			// Schotter: compacted, fine_gravel, gravel
	SURFACE_FOREST = 3,			// Waldweg: ground, dirt (roots, forest floor)
	SURFACE_FIELD = 4,			// Feldweg: compacted/ground/grass track between fields
	SURFACE_PAVING = 5,			// Pflaster: paving_stones, sett, cobblestone
	SURFACE_OTHER = 6,
	SURFACE_COUNT
};
// Quality 1..4 (0 = not set), rated for cycling, roughly OSM smoothness:
//   1 excellent/good, 2 intermediate, 3 bad, 4 very_bad and worse
static constexpr uint8_t LABEL_QUALITY_MAX = 4;

enum LabelReason : uint8_t {
	LABEL_CHANGE = 0,			// surface or quality changed (including cleared)
	LABEL_REFRESH = 1,			// unchanged, repeated every 60 s so a lost record doesn't lose the label
	LABEL_CAPTURE_START = 2,	// raw capture R_*.bin started
	LABEL_CAPTURE_STOP = 3,		// ... stopped or complete
};

enum LabelFlags : uint8_t {
	LF_CAPTURING = 0x01,		// a raw capture is running (after this record, for CAPTURE_START/STOP)
	LF_GPS_VALID = 0x02,
};

// Bits of Data::gpsFlags. LOG_GPS_VALID mirrors SGpsFix::valid (a POSITION_UPDATE was
// received at all); it does NOT mean the fix is fresh -- check gpsFixAgeMs for that (see
// BikeGpsProtocol.h). The other bits mirror SGpsFix's hasXxx flags: the corresponding
// value field is 0 when its bit is clear.
// LOG_SIMULATED is not about GPS, it only shares the byte (Data has no other spare bits):
// speed, cadence and heart rate of this record come from the sensor simulator
// (src/SimSensors.h, simulator build only). Always 0 in older files -- no version bump.
enum GpsFlags : uint8_t {
	LOG_GPS_VALID        = 0x01,
	LOG_GPS_HAS_ALTITUDE = 0x02,
	LOG_GPS_HAS_SPEED    = 0x04,
	LOG_GPS_HAS_BEARING  = 0x08,
	LOG_GPS_HAS_ACCURACY = 0x10,
	LOG_SIMULATED        = 0x80,
};

// Type 0. Bytes 0..55 are v1's layout except that gpsFlags moved from 30 to 56 to make
// room for recordType.
struct Data {
	time_t timestamp;					//  0
	float speed;						//  8  km/h, wheel sensor
	float temp;							// 12  deg C
	float grad;							// 16  %, the gradient shown on the display (see gradBaro/gradImu)
	float height;						// 20  m, barometric
	float dist_m;						// 24  m, trip distance since start
	uint8_t hr;							// 28  bpm
	uint8_t cadence;					// 29  rpm
	uint8_t recordType;					// 30  = TYPE_DATA
	uint8_t formatVersion;				// 31  = FORMAT_VERSION
	int32_t gpsLatitudeE7;				// 32
	int32_t gpsLongitudeE7;				// 36
	int32_t gpsAltitudeM;				// 40
	uint32_t gpsSpeedCms;				// 44
	uint16_t gpsBearingDegX100;			// 48
	uint16_t gpsAccuracyMX10;			// 50
	uint32_t gpsFixAgeMs;				// 52
	uint8_t gpsFlags;					// 56  GpsFlags
	uint8_t roadClass;					// 57  latest road class, 0 = not rated
	uint16_t timestampMs;				// 58
	int16_t gradBaroX100;				// 60  % * 100, barometric; GRAD_INVALID if none
	int16_t gradImuX100;				// 62  % * 100, accelerometer (bias-corrected); GRAD_INVALID if none
};

// Type 1. flags = RQ::IntervalFlags (RoadQuality.h).
struct RoadQuality {
	time_t timestamp;					//  0  end of the interval
	uint16_t timestampMs;				//  8
	uint16_t intervalMs;				// 10  wall-clock length (longer than configured after a FIFO overflow)
	uint16_t sampleCount;				// 12
	uint16_t speedCms;					// 14  mean wheel (or GPS) speed; U16_INVALID if none
	uint16_t rmsVertMg;					// 16  RMS of the vertical, band-passed (2..80 Hz) acceleration
	uint16_t rmsHorizMg;				// 18
	int16_t peakVertMaxMg;				// 20
	int16_t peakVertMinMg;				// 22
	uint16_t peakTotalMg;				// 24
	uint16_t roughnessX100;				// 26  speed-normalised roughness index R; U16_INVALID if not rated
	uint8_t roadClass;					// 28  0 = not rated, 1 (smooth) .. 5 (very rough)
	uint8_t flags;						// 29  RQ::IntervalFlags
	uint8_t recordType;					// 30  = TYPE_ROAD_QUALITY
	uint8_t formatVersion;				// 31
	float vdvVert;						// 32  vibration dose value, m/s^1.75
	float distanceM;					// 36  over the interval
	uint16_t countOverT1;				// 40  crossings of 1 g (total dynamic acceleration)
	uint16_t countOverT2;				// 42  crossings of 2 g
	uint16_t eventsLogged;				// 44  shock records written during the interval
	uint16_t eventsSuppressed;			// 46  shocks dropped by the rate limit
	int32_t gpsLatitudeE7;				// 48  valid if flags & IF_GPS_VALID
	int32_t gpsLongitudeE7;				// 52
	uint32_t gpsFixAgeMs;				// 56
	uint16_t gpsAccuracyMX10;			// 60  0 = unknown
	int16_t gradImuX100;				// 62  GRAD_INVALID if none
};

// Type 2. flags = RQ::ShockFlags (RoadQuality.h).
struct Shock {
	time_t timestamp;					//  0  time of the (first) peak
	uint16_t timestampMs;				//  8
	uint16_t durationMs;				// 10  of the first peak above the release threshold
	uint16_t peakTotalMg;				// 12
	int16_t peakVertMaxMg;				// 14
	int16_t peakVertMinMg;				// 16
	uint16_t peakHorizMg;				// 18
	uint16_t preRmsMg;					// 20  RMS_v of the second before: rough ground or smooth road?
	uint16_t speedCms;					// 22  U16_INVALID if unknown
	uint16_t secondPeakMg;				// 24  rear wheel, 0 = none
	uint16_t secondPeakDelayMs;			// 26
	uint8_t severity;					// 28  1 (3..5 g), 2 (5..8 g), 3 (> 8 g or clipped)
	uint8_t flags;						// 29  RQ::ShockFlags
	uint8_t recordType;					// 30  = TYPE_SHOCK
	uint8_t formatVersion;				// 31
	float vdv;							// 32  m/s^1.75, vertical, over the first peak
	uint16_t samplesOverThr;			// 36
	uint16_t thresholdMg;				// 38  trigger threshold in effect
	int32_t gpsLatitudeE7;				// 40  valid if flags & SF_GPS_VALID
	int32_t gpsLongitudeE7;				// 44
	uint32_t gpsFixAgeMs;				// 48
	uint16_t gpsAccuracyMX10;			// 52
	uint8_t labelSurface;				// 54  manual label at the time (Surface), 0 = none
	uint8_t labelQuality;				// 55  1..4, 0 = none
	uint32_t eventSeq;					// 56  running number since boot: gaps = records lost on the way to the card
	uint32_t reserved2;					// 60
};

// Type 3. Written from the ImuTask, like the other road-quality records.
struct Label {
	time_t timestamp;					//  0
	uint16_t timestampMs;				//  8
	uint8_t surface;					// 10  Surface
	uint8_t quality;					// 11  1..4, 0 = not set
	uint8_t reason;						// 12  LabelReason
	uint8_t flags;						// 13  LabelFlags
	uint8_t prevSurface;				// 14  label before a LABEL_CHANGE
	uint8_t prevQuality;				// 15
	float prevDistanceM;				// 16  distance ridden under the previous label (LABEL_CHANGE), else so far
	uint32_t prevDurationMs;			// 20  same for the time
	uint32_t labelSeq;					// 24  running number since boot
	uint16_t reserved1;					// 28
	uint8_t recordType;					// 30  = TYPE_LABEL
	uint8_t formatVersion;				// 31
	int32_t gpsLatitudeE7;				// 32  valid if flags & LF_GPS_VALID
	int32_t gpsLongitudeE7;				// 36
	uint32_t gpsFixAgeMs;				// 40
	uint16_t gpsAccuracyMX10;			// 44
	uint16_t speedCms;					// 46  U16_INVALID if unknown
	uint8_t reserved2[16];				// 48
};

// RideState.flags
enum RideStateFlags : uint8_t {
	RSF_SESSION_OPEN = 0x01,	// a ride session is open (Start tapped, not yet ended by a long press)
	RSF_SESSION_START = 0x02,	// this record opened the session: its Start/Ride statistics begin here
	RSF_SIMULATED = 0x04,		// the sensor simulator was active (src/SimSensors.h, simulator build only)
};

// Type 4. Written by Statistics on every change of the drive state, the Ride mode or the
// ride session (Statistics::logRideState()) -- a tap at a stop or a long press while
// cruising changes only rideMode/flags, not the state. Lets a post-processing tool
// (Tools/bikelog/gpx.py) tag GPX <trkseg> sections with the ride state in effect
// (doc/design/ride-state-machine.md). The record holds until the next one.
// state/prevState = Statistics::EDrivingState (0 DS_NO_CONN, 1 DS_BREAK, 2 DS_STOP,
// 3 DS_FREE_RIDE, 4 DS_DRIVE_COASTING, 5 DS_DRIVE_POWER -- see src/Stats/Statistics.h).
// DS_FREE_RIDE with RSF_SESSION_OPEN is Cruise, without it FreeRide.
struct RideState {
	time_t timestamp;					//  0
	uint16_t timestampMs;				//  8
	uint8_t state;						// 10
	uint8_t prevState;					// 11  state of the previous RideState record
	uint8_t rideMode;					// 12  1 = Ride mode (Pause glyph), also while stopped
	uint8_t flags;						// 13  RideStateFlags
	uint8_t reserved1;					// 14
	uint8_t reserved2;					// 15
	float distM;						// 16  distance since power-on (as Data::dist_m)
	uint32_t reserved4;					// 20
	uint32_t stateSeq;					// 24  running number since boot: gaps = records lost
	uint16_t reserved5;					// 28
	uint8_t recordType;					// 30  = TYPE_RIDESTATE
	uint8_t formatVersion;				// 31
	uint8_t reserved6[32];				// 32
};

// The reader (Tools/bikelog/record.py) depends on exactly these positions.
static_assert(sizeof(time_t) == 8, "time_t size changed -- all offsets below shift");
static_assert(sizeof(Data) == RECORD_SIZE, "LogRec::Data must be 64 byte");
static_assert(sizeof(RoadQuality) == RECORD_SIZE, "LogRec::RoadQuality must be 64 byte");
static_assert(sizeof(Shock) == RECORD_SIZE, "LogRec::Shock must be 64 byte");
static_assert(sizeof(Label) == RECORD_SIZE, "LogRec::Label must be 64 byte");
static_assert(sizeof(RideState) == RECORD_SIZE, "LogRec::RideState must be 64 byte");
static_assert(offsetof(Data, recordType) == TYPE_OFFSET && offsetof(Data, formatVersion) == VERSION_OFFSET, "Data header");
static_assert(offsetof(RoadQuality, recordType) == TYPE_OFFSET && offsetof(RoadQuality, formatVersion) == VERSION_OFFSET, "RoadQuality header");
static_assert(offsetof(Shock, recordType) == TYPE_OFFSET && offsetof(Shock, formatVersion) == VERSION_OFFSET, "Shock header");
static_assert(offsetof(Label, recordType) == TYPE_OFFSET && offsetof(Label, formatVersion) == VERSION_OFFSET, "Label header");
static_assert(offsetof(RideState, recordType) == TYPE_OFFSET && offsetof(RideState, formatVersion) == VERSION_OFFSET, "RideState header");
static_assert(offsetof(Data, gpsLatitudeE7) == 32 && offsetof(Data, gpsFlags) == 56 && offsetof(Data, gradImuX100) == 62, "Data layout");
static_assert(offsetof(RoadQuality, vdvVert) == 32 && offsetof(RoadQuality, gpsLatitudeE7) == 48 && offsetof(RoadQuality, gradImuX100) == 62, "RoadQuality layout");
static_assert(offsetof(Shock, vdv) == 32 && offsetof(Shock, gpsLatitudeE7) == 40 && offsetof(Shock, eventSeq) == 56, "Shock layout");
static_assert(offsetof(Label, prevDistanceM) == 16 && offsetof(Label, gpsLatitudeE7) == 32 && offsetof(Label, speedCms) == 46, "Label layout");
static_assert(offsetof(RideState, distM) == 16 && offsetof(RideState, stateSeq) == 24, "RideState layout");

// Helpers for filling records
inline uint16_t toU16(float v) {return v <= 0 ? 0 : (v >= 65534.0f ? 65534 : (uint16_t)(v + 0.5f));}
inline int16_t toI16(float v) {return v <= -32767.0f ? -32767 : (v >= 32767.0f ? 32767 : (int16_t)(v >= 0 ? v + 0.5f : v - 0.5f));}
inline uint16_t mg(float g) {return toU16(g * 1000.0f);}
inline int16_t mgSigned(float g) {return toI16(g * 1000.0f);}
inline int16_t gradX100(float pct) {return pct != pct ? GRAD_INVALID : toI16(pct * 100.0f);}	// NaN -> invalid
inline uint16_t speedCms(float kmh) {return kmh != kmh ? U16_INVALID : toU16(kmh / 3.6f * 100.0f);}

}	// namespace LogRec
