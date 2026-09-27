/*
 * Statistics.h
 *
 *  Created on: 31.12.2022
 *      Author: ian
 */
#pragma once

#include <Arduino.h>
#include <Ticker.h>
#include <Preferences.h>
//#include <Stats/Distance.h>
class Distance;
#include "BikeGpsProtocol.h"

#include <ringbuffer.hpp>

class Statistics {
public:
	enum ESummaryType{
		SUM_ESP_TOTAL,				// Distance as stored forever
		SUM_ESP_TOUR,               // Tour Distance as stored locally
		SUM_ESP_TRIP,               // Trip Distance as stored locally
		SUM_ESP_START,              // Distance as stored locally (since start)
		SUM_FL_TOTAL,               // Total Distance as stored in external device, e. g. FL
		SUM_FL_TOUR,    			// Tour Distance as stored in external device, e. g. FL
		SUM_FL_TRIP,                // Trip Distance as stored in external device, e. g. FL
		ESummaryTypeMax
	};
	enum EAvgType {
		AVG_ALL,
		AVG_DRIVE,
		AVG_NOBREAK,   // but Pause/Stop
		AVG_NOCRUISE,  // Drive + Stop + Break, but without FreeRide/Cruise time -- see
		               // doc/design/ride-state-machine.md §5. Only SUM_ESP_START normally
		               // has any DS_FREE_RIDE time to exclude (see the rideSessionOpen gate
		               // in cycle()); for the other summary types this is close to AVG_ALL.
		EAvgTypeMax
	};
	// Ride-state FSM -- see doc/design/ride-state-machine.md for the full picture (two
	// independent axes: rideMode, manual via the Pause/Start button, selects
	// DS_FREE_RIDE vs. DS_DRIVE_COASTING/POWER; DS_STOP/DS_BREAK are automatic, from
	// speed, and apply regardless of rideMode).
	enum EDrivingState {	// Various driving states
		DS_NO_CONN,
		DS_BREAK,
		DS_STOP,
		DS_FREE_RIDE,		// rideMode == false, in Bewegung (deckt "FreeRide" UND "Cruise" ab)
		DS_DRIVE_COASTING,
		DS_DRIVE_POWER,
		EDrivingStateMax
	};

private:
	enum EStatDataType {
		DT_SPEED,
		DT_HR,
		DT_CAD,
		DT_TEMP,
		EStatDataTypeMax
	};

	float speed_max[ESummaryTypeMax] = {0.0,0.0,0.0};

	EDrivingState curDriveState = DS_NO_CONN;
	EDrivingState histDriveState = DS_STOP;

	// Manual ride-mode toggle (Pause/Start button short tap) and the ride-session flag it
	// opens/closes (long-press "Stop") -- see doc/design/ride-state-machine.md §2/§4.
	// Both start false/false: initial state is FreeRide, no session open, matching the
	// "Initialer Zustand: FreeRide" requirement without any extra setup code.
	bool rideMode = false;
	bool rideSessionOpen = false;
	void applyRideModeToCurrentMovement();	// re-evaluates curDriveState right after rideMode changes, instead of waiting for the next cycle()

	time_t timestamp_last;
	uint32_t time_in[EDrivingStateMax][ESummaryTypeMax];
	// Per-state distance, mirrors time_in[][] -- needed so AVG_NOBREAK/AVG_NOCRUISE can
	// exclude the distance covered while stopped/cruising, not just the time (see §5 of
	// the design doc). NOT NVS-persisted, unlike time_in[][] -- resets on reboot.
	float dist_in[EDrivingStateMax][ESummaryTypeMax] = {};
	time_t timestamp_stop;

	struct S_DataPoint {
		float min;
		float avg;
		float max;
	};

	// Data storage per distance
	struct S_distanceData {	 // 64 Byte
		time_t	timestamp;		//   4 Byte
		S_DataPoint speed; 		//  12 Byte
		S_DataPoint cadence;	// +12 Byte
		S_DataPoint gradient;	// +12 Byte
		S_DataPoint height;	    // +12 Byte
		S_DataPoint hr;			// +12 Byte
		uint8_t coastShare;		//  +1 Byte + 3Byte alignment
	};
	static void addFloatToDatapoint(S_DataPoint& data, const float val);
	static float setDatapointAvg(S_DataPoint& data, uint8_t& count);

	// Number of 100m buckets kept in S_distanceComplete::data -> 48km of history.
	// Used both for the allocation and for the index wrap in updateDistanceSeries();
	// those two used to disagree (array of 480, wrap at 400), leaving 79 slots dead.
	static const uint16_t DISTANCE_SERIES_LEN = 480;

	struct S_distanceComplete {
		uint32_t startDistance = 0;
		// PSRAM-backed, same reasoning and same fail-fast convention as S_timeComplete::data
		// below: 480 * 72 byte = ~34.5KB that used to sit in internal .bss, which is the pool
		// WiFi/lwIP/BLE/SD_MMC compete for (measured: 21KB free when idle, ~300 byte under
		// web load).
		// Safe against the panel-tearing constraint from 7051e56 by TRIGGER, which is the
		// test that matters, not by size: the single write below fires once per 100m ridden
		// (updateDistanceSeries()) -- not from a BLE notify callback, not from an LVGL draw,
		// and not at all while the bike is standing still. Contrast currentMinMax, which is
		// written on every sensor notification and therefore stays internal, and chart_array,
		// which LVGL reads on every chart refresh and likewise stays internal.
		// If flicker ever needs re-diagnosing, check triggers before reverting this: a
		// periodic heap_caps_get_info()/get_largest_free_block() on MALLOC_CAP_SPIRAM walks
		// every block of the PSRAM heap and is a far bigger offender than this field.
		S_distanceData* data = nullptr;			// DISTANCE_SERIES_LEN entries, per 100m
		S_distanceData currentMinMax;
		float curDistance = 0;
		uint16_t index = 0;
		uint8_t curCountSpeed;
		uint8_t curCountCadence;
		uint8_t curCountHr;
		uint8_t curCountGradHeight;
	};

	S_distanceComplete distanceData;

	float distSeries_last = 0.0;

	// Data storage per time
	struct S_timeData {	// 46 Byte
		float distance;		//   4 Byte
		float tempC;		//   4 Byte
		S_DataPoint speed;	//  12 Byte
		S_DataPoint cadence;//  12 Byte
		S_DataPoint hr;		//  12 Byte
		uint8_t coastShare:8;
		uint8_t pauseShare:8;
	};

	struct S_timeComplete {		// 18459 Byte
		time_t startTime;
		// 400-entry history ring -- PSRAM-backed (allocated in the constructor, see
		// Statistics.cpp), 18.4KB is too much to tie up in internal DRAM permanently, which
		// BLE/SD_MMC need DMA-capable room in (see the SD-logging DMA starvation fix). Only
		// touched every ~5-15s (updateTimeSeries()/createChartArray()), never from the hot path
		// below, so PSRAM's extra access latency doesn't matter here.
		S_timeData* data;
		// Hot accumulator: addCadence()/addHR()/addSpeed() write into this on EVERY BLE sensor
		// notify (so potentially several times/sec) -- kept in plain internal RAM on purpose.
		// Moving this alongside `data` above (as part of one PSRAM-backed struct) is what
		// caused the display to flicker/tear: the RGB panel has no bounce buffer
		// (TRGBSuppport.cpp: fb_in_psram=1, no bounce_buffer_size_px), so it DMAs the
		// framebuffer straight from PSRAM with zero tolerance for other PSRAM traffic sharing
		// the bus (2026-09-19 investigation).
		S_timeData currentMinMax;
		uint16_t index = 0;
		uint8_t curCountSpeed;
		uint8_t curCountCadence;
		uint8_t curCountHr;
	};

	S_timeComplete timeData;	// plain internal-RAM member; only its `data` field is PSRAM-backed (see constructor)

	// Variables to calculate gradient (Forumslader calculates gradient on its own)
#ifndef BC_FL_SUPPORT
	time_t gradient_timestamp = 0;		//Timestamp of last gradient calculation
	float gradient_dist = 0;			//Distance of last gradient calculation
	float gradient_height = NAN;		//Height of last gradient calculation
#endif
	Distance& distHandler;				// also the distance handler is only used without Forumslader because FL calculates distance (partly) on its own.

	float gradient = 0.0;				//gradient shown on the display (barometric, or the accelerometer's if selected, see calculateGradient())
	float gradientBaro = NAN;			//last barometric gradient, logged next to the accelerometer's
	float height   = NAN;
	float tempC    = NAN;

	// PSRAM-backed for the same reason as distanceData.data (~12.3KB of internal .bss).
	// Currently unused -- it belongs to the charts that are to come back; kept, not deleted.
	// Allocated with placement new in the constructor because Ringbuffer is a real object
	// (two atomics + the element array), not a plain buffer. When the charts do return, do
	// NOT hand a PSRAM pointer straight to LVGL (no lv_chart_set_ext_y_array on this) --
	// copy into an internal staging buffer first, or the panel tearing from 7051e56 is back.
	jnk0le::Ringbuffer<S_timeData, 256>* rb_timedata = nullptr;


	// use int instead of uint, so -1 can be used as "invalid".
	int16_t hr = -1;
	int16_t cadence = 0, cadence_tot = -1;
	float speed=0.0;
	uint32_t speedUpdateMs = 0;			// millis() of the last addSpeed() -- the ImuTask derives dv/dt from these updates

	// Road quality on the UI: pushed when it changes, and every 10 s regardless
	uint8_t roadClassShown = 255;
	uint32_t shockCountShown = 0;
	uint8_t roadUiCycles = 0;
	uint8_t gradUiCycles = 0;
	void updateRoadQualityUi();

	// Minutes stopped/disconnected before auto-off (deepSleep()); 255 = disabled. Starts
	// disabled -- toggle via long-press on the drive-state icon (toggleStandbyMode()).
	uint8_t offAfterMinutes = 255;

	Ticker statCycle;
	Ticker statStore;
	Ticker statDataStore;
	Preferences StatPreferences[SUM_ESP_START + 1]; // FL values are not stored (because they are stored in FL). SUM_ESP_START value stores last know value

	void cycle();			// 500ms ticker
	void autoStore();		// 5s ticker
	void dataStore();		// 5s ticker

	void restoreStats();
	void setCurDriveState(EDrivingState curDriveState);

	void updateDistanceSeries();
	void updateTimeSeries();
	String generateJSONArray();
	void setupWebserverDebug();
	void calculateGradient(float newDist);

	//TODO: Move into separate class
	enum DataClass {SPEED = 0, HR, HEIGHT, GRADIENT, TEMPERATURE, CADENCE, DISTANCE};
	static const uint8_t chart_array_count = 4;
	// Internal RAM (only 3.2KB). No display chart since 2026-09-27 (old SquareLine chart
	// removed, to be redesigned) -- still served on /stat/debugarray and /stat/data.
	int16_t chart_array[chart_array_count][400];
	uint16_t chart_array_startPos[chart_array_count];
	DataClass chart_array_type[chart_array_count] = {SPEED, HR, CADENCE, DISTANCE};

//	int16_t height_array[400] = {};
//	uint16_t height_array_idx = 0;
//
//	int16_t hr_array[400] = {};
//	uint16_t hr_array_idx = 0;
//
//	int16_t speed_array[400] = {};
//	uint16_t speed_array_idx = 0;
//
//	int16_t speed2_array[400] = {};
//	uint16_t speed2_array_idx = 0;


public:
	Statistics();
	void setup();
private:
	void allocPsramBuffers();	// see the definition -- must run after the display is up
public:
	void addSpeed(float speed);  // in 0,1km/h
	//void addDistance(uint32_t dist, ESummaryType type = SUM_ESP_TOUR);
	bool isConnected() {return (curDriveState != DS_NO_CONN);}
	void delayStandby();
	void toggleStandbyMode();

	// Pause/Start button (rr_btn_pause), see doc/design/ride-state-machine.md §3/§4.
	// Short tap: Start (opens a new ride session, resets SUM_ESP_START) the first time,
	// Resume/Cruise-toggle (Ride<->FreeRide/Cruise) every time after, session stays open.
	void toggleRideMode();
	// Long press: ends the ride session (-> true FreeRide, SUM_ESP_START stats stop
	// accumulating) if one is open; otherwise falls back to the pre-existing
	// toggleStandbyMode() behaviour, since that's RimRidge's only control for it.
	void handlePauseButtonHold();
	bool getRideMode() const {return rideMode;}
	bool isRideSessionOpen() const {return rideSessionOpen;}
private:
	void stopRide();
	void updateStateIcon();
	void updateGpsFixIcon(const SGpsFix& fix);
	bool shutdownMsg = false;

public:
	void reset(ESummaryType type);
	//void updateDistance(uint32_t dist, uint32_t revs);
	void checkDistance(const float dist);
	void addHR(int16_t heartrate);
	void addCadence(int16_t cadence, int16_t total);
	void setConnected(bool connected);
	void addGradientHeight(float _grad, float _height);
	void addTemperature(float _temperature);
	// Per-drive-state distance bucketing (see dist_in[][] above) -- called from
	// Distance::updateRevs() with the same rev-delta-derived distance it uses for
	// addSpeed(), so it shares that call's Scenario-1/2 (re)connect handling.
	void addDistanceDelta(float deltaM);


	uint32_t getTime(ESummaryType type, EAvgType avgtype) const;

	float getAvg(ESummaryType type, EAvgType avgtype) const;
	float getAvgCadence(EAvgType avgtype) const;
	int16_t getHr() const {return hr;}
	float getSpeed() const {return speed;}						// km/h, NAN while disconnected
	uint32_t getSpeedUpdateMs() const {return speedUpdateMs;}
	const float getSpeedMax(ESummaryType type) const {return speed_max[type];}
	uint32_t getDistance(ESummaryType type, bool includeLost = true) const;
	float getTemp() const {return tempC;}
private:
	// dist_in[][]-based distance for the AVG_DRIVE/AVG_NOBREAK/AVG_NOCRUISE filters --
	// see getAvg()'s doc comment for why AVG_ALL uses getDistance() instead.
	float getDistanceFiltered(ESummaryType type, EAvgType avgtype) const;
public:

	static const char* PREF_TIME_STRING[Statistics::EDrivingStateMax];
	static const char* AVG_TYPE_STRING[Statistics::EAvgTypeMax];
	static const char* SUM_TYPE_STRING[Statistics::ESummaryTypeMax];

	static EAvgType getNextTimeMode(EAvgType type, bool dir) {
		int32_t rc = static_cast<int32_t>(type);
		rc += dir ? 1:-1;
		if (rc < Statistics::AVG_ALL) rc = Statistics::AVG_NOCRUISE;
		if (rc >= Statistics::EAvgTypeMax) rc = Statistics::AVG_ALL;
		return static_cast<EAvgType>(rc);
	}

	void createChartArray(uint8_t idx);

	Distance& getDistHandler() {return distHandler;}

	EDrivingState getCurDriveState() const {return curDriveState;}
};

