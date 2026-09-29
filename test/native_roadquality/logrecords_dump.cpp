/*
 * Writes one record of each type from src/LogRecords.h, filled with known values, to the
 * file given as argv[1]. Tools/tests/test_logformat.py compiles and runs this on the host
 * and reads the result with bikelog.record -- so a change to the firmware structs that the
 * Python layouts don't follow fails a test instead of silently corrupting exports.
 *
 * Host and ESP32-S3 agree on what matters here: little-endian, IEEE floats, 8-byte time_t
 * (asserted in LogRecords.h), natural alignment.
 */

#include "LogRecords.h"

#include <cmath>
#include <cstdio>

using namespace LogRec;

int main(int argc, char** argv) {
	if (argc < 2) return 2;

	Data d = {};
	d.timestamp = 1790000000;
	d.speed = 21.5f;
	d.temp = 17.25f;
	d.grad = -3.5f;
	d.height = 123.5f;
	d.dist_m = 4567.0f;
	d.hr = 140;
	d.cadence = 85;
	d.recordType = TYPE_DATA;
	d.formatVersion = FORMAT_VERSION;
	d.gpsLatitudeE7 = 524000000;
	d.gpsLongitudeE7 = 87000000;
	d.gpsAltitudeM = 95;
	d.gpsSpeedCms = 600;
	d.gpsBearingDegX100 = 4500;
	d.gpsAccuracyMX10 = 45;
	d.gpsFixAgeMs = 350;
	d.gpsFlags = 0x1f;
	d.roadClass = 3;
	d.timestampMs = 789;
	d.gradBaroX100 = gradX100(-3.25f);
	d.gradImuX100 = gradX100(NAN);

	RoadQuality r = {};
	r.timestamp = 1790000002;
	r.timestampMs = 12;
	r.intervalMs = 2003;
	r.sampleCount = 800;
	r.speedCms = speedCms(20.0f);
	r.rmsVertMg = 234;
	r.rmsHorizMg = 99;
	r.peakVertMaxMg = 1500;
	r.peakVertMinMg = -1400;
	r.peakTotalMg = 1800;
	r.roughnessX100 = 275;
	r.roadClass = 3;
	r.flags = 0x88;
	r.recordType = TYPE_ROAD_QUALITY;
	r.formatVersion = FORMAT_VERSION;
	r.vdvVert = 1.25f;
	r.distanceM = 11.5f;
	r.countOverT1 = 7;
	r.countOverT2 = 2;
	r.eventsLogged = 1;
	r.eventsSuppressed = 4;
	r.gpsLatitudeE7 = 524000100;
	r.gpsLongitudeE7 = 87000100;
	r.gpsFixAgeMs = 400;
	r.gpsAccuracyMX10 = 50;
	r.gradImuX100 = gradX100(4.2f);

	Shock s = {};
	s.timestamp = 1790000003;
	s.timestampMs = 456;
	s.durationMs = 8;
	s.peakTotalMg = 5790;
	s.peakVertMaxMg = 5700;
	s.peakVertMinMg = -2100;
	s.peakHorizMg = 800;
	s.preRmsMg = 90;
	s.speedCms = speedCms(NAN);
	s.secondPeakMg = 2820;
	s.secondPeakDelayMs = 188;
	s.severity = 2;
	s.flags = 0x0a;
	s.recordType = TYPE_SHOCK;
	s.formatVersion = FORMAT_VERSION;
	s.vdv = 0.5f;
	s.samplesOverThr = 3;
	s.thresholdMg = 3000;
	s.gpsLatitudeE7 = 524000200;
	s.gpsLongitudeE7 = 87000200;
	s.gpsFixAgeMs = 500;
	s.gpsAccuracyMX10 = 60;
	s.labelSurface = SURFACE_PAVING;
	s.labelQuality = 3;
	s.eventSeq = 42;

	Label l = {};
	l.timestamp = 1790000004;
	l.timestampMs = 321;
	l.surface = SURFACE_GRAVEL;
	l.quality = 2;
	l.reason = LABEL_CHANGE;
	l.flags = LF_CAPTURING | LF_GPS_VALID;
	l.prevSurface = SURFACE_ASPHALT;
	l.prevQuality = 1;
	l.prevDistanceM = 1234.5f;
	l.prevDurationMs = 180000;
	l.labelSeq = 7;
	l.recordType = TYPE_LABEL;
	l.formatVersion = FORMAT_VERSION;
	l.gpsLatitudeE7 = 524000300;
	l.gpsLongitudeE7 = 87000300;
	l.gpsFixAgeMs = 600;
	l.gpsAccuracyMX10 = 70;
	l.speedCms = speedCms(18.0f);

	RideState rs = {};
	rs.timestamp = 1790000005;
	rs.timestampMs = 654;
	rs.state = 4;			// DS_DRIVE_COASTING (Statistics::EDrivingState)
	rs.prevState = 3;		// DS_FREE_RIDE
	rs.rideMode = 1;
	rs.stateSeq = 9;
	rs.recordType = TYPE_RIDESTATE;
	rs.formatVersion = FORMAT_VERSION;

	FILE* f = fopen(argv[1], "wb");
	if (!f) return 1;
	fwrite(&d, sizeof(d), 1, f);
	fwrite(&r, sizeof(r), 1, f);
	fwrite(&s, sizeof(s), 1, f);
	fwrite(&l, sizeof(l), 1, f);
	fwrite(&rs, sizeof(rs), 1, f);
	fclose(f);
	return 0;
}
