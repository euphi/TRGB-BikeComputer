/*
 * Statistics.cpp
 *
 *  Created on: 31.12.2022
 *      Author: ian
 */

#include "Statistics.h"
#include "Distance.h"
#include "Singletons.h"
#include "WebPage.h"
#include "LogRecords.h"
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <new>				// placement new for the PSRAM-backed rb_timedata, see the constructor

const char* Statistics::PREF_TIME_STRING[Statistics::EDrivingStateMax] = {
		"TIME_IN_NOCONN",	//		DS_NO_CONN,
		"TIME_IN_BREAK",	//		DS_BREAK,
		"TIME_IN_STOP",		//		DS_STOP,
		"TIME_IN_FREE",		//		DS_FREE_RIDE,  -- new key, name-keyed NVS storage so old data stays valid (NVS keys: max. 15 chars)
		"TIME_IN_COAST",	//		DS_DRIVE_COASTING,
		"TIME_IN_DRIVE"		//		DS_DRIVE_POWER,
};

const char* Statistics::SUM_TYPE_STRING[Statistics::ESummaryTypeMax] = {
		"ST_TOTAL",
		"ST_TOUR",
		"ST_TRIP",
		"ST_START",
#ifdef BC_FL_SUPPORT
		"ST_FL_TOTAL",
		"ST_FL_TOUR",
		"ST_FL_TRIP",
#endif
};

const char* Statistics::AVG_TYPE_STRING[Statistics::EAvgTypeMax] = {
		"⏱ kompl",
		"⏱ fahrend",
		"⏱ Stops",
		"⏱ o.Cruise"
};

Statistics::Statistics(): distHandler(* new Distance())  {
	// PSRAM-backed (see S_timeComplete::data's comment in Statistics.h for why only this field,
	// not the whole struct). heap_caps_calloc (not _malloc) so it starts zeroed, matching what
	// this got for free before as a plain .bss-resident array.
	timeData.data = static_cast<S_timeData*>(heap_caps_calloc(400, sizeof(S_timeData), MALLOC_CAP_SPIRAM));
	assert(timeData.data);		// same fail-fast convention as TRGBSuppport.cpp's own PSRAM draw-buffer allocations

	// NOTE: distanceData.data and rb_timedata are deliberately NOT allocated here -- see
	// allocPsramBuffers(), called from setup(). This constructor runs during static init,
	// i.e. before trgb.init(), and anything allocated here shifts every later PSRAM
	// allocation, including LVGL's two 460KB draw buffers.
	timestamp_last = millis();
	timestamp_stop = timestamp_last;
}

// Allocated from setup(), NOT from the constructor, and this ordering is load-bearing.
//
// The constructor runs during static init, before main()'s trgb.init(). The RGB panel
// driver allocates its framebuffer with .psram_trans_align = 64, so that one is safe --
// but TRGBSuppport.cpp then allocates LVGL's two 460KB draw buffers with a plain
// heap_caps_malloc(MALLOC_CAP_SPIRAM), which carries NO alignment guarantee: their
// addresses depend entirely on what was taken out of the PSRAM heap before them.
// LVGL memcpys a full 460KB draw buffer into the framebuffer on every flush, and this
// board's panel DMAs that same framebuffer out of PSRAM with no bounce buffer. Degrading
// that copy's alignment shows up as visible tearing -- observed as a frequent, irregular
// left-shift (distinct from the right-shift-with-wraparound in commit 7051e56, which had
// a different cause).
//
// setup() runs after trgb.init() and ui.initDisplay(), so allocating here leaves the
// display buffers at exactly the addresses they had before these two fields existed.
// Keep it that way: do not move PSRAM allocations into a statically constructed object.
void Statistics::allocPsramBuffers() {
	distanceData.data = static_cast<S_distanceData*>(
			heap_caps_calloc(DISTANCE_SERIES_LEN, sizeof(S_distanceData), MALLOC_CAP_SPIRAM));
	assert(distanceData.data);

	// Ringbuffer has no state beyond its two atomics and the element array, so plain
	// placement new over zeroed PSRAM is enough -- no destructor is ever needed either,
	// this lives for the whole runtime like every other singleton member here.
	void* rbMem = heap_caps_calloc(1, sizeof(*rb_timedata), MALLOC_CAP_SPIRAM);
	assert(rbMem);
	rb_timedata = new (rbMem) jnk0le::Ringbuffer<S_timeData, 256>();
}

void Statistics::setup() {
	allocPsramBuffers();
	restoreStats();
	distHandler.setup();
	// What kind of sorcery is this?  --> See https://stackoverflow.com/questions/60985496/arduino-esp8266-esp32-ticker-callback-class-member-function
	statCycle.attach_ms(500, +[](Statistics *thisInstance) {thisInstance->cycle();}, this);
	statDataStore.attach(5, +[](Statistics *thisInstance) {thisInstance->dataStore();}, this);
	statStore.attach(15, +[](Statistics *thisInstance) {thisInstance->autoStore();}, this);

	// Serve the array data as JSON
	webserver.getServer().on("/stat/data", HTTP_GET, [this](AsyncWebServerRequest *request) {
		String jsonArray = this->generateJSONArray();
		request->send(200, "application/json", jsonArray);
	});
	setupWebserverSummary();
	setupWebserverDebug();
}

// Helper function to generate a JSON array from a float array
String Statistics::generateJSONArray() {
	time_t timenow;
	time(&timenow);		// TODO: timestamp is a bit off, because its not synced with data storage
	String jsonArray = "[[";
	for (size_t i = 0; i < 400; i++) {
		jsonArray += String(timenow - (400-i) * 5);
		if (i < 399) jsonArray += ",";
	}
	jsonArray += ']';
	for (uint_fast8_t j = 0; j < 4; j++) {
		jsonArray += ",[";
		for (size_t i = 0; i < 400; i++) {
			int16_t value = chart_array[j][(i + chart_array_startPos[j]) % 400];
			if (value == INT16_MAX) value = 0;
			jsonArray += String(value);
			if (i < 399) jsonArray += ",";
		}
		jsonArray +=']';
	}
	jsonArray += ']';
	return jsonArray;
}

// NVS keys next to PREF_TIME_STRING[] in the same namespace (max. 15 chars each)
static const char* const PREF_DIST_FREE = "DIST_FREE";
static const char* const PREF_SPEED_MAX = "SPEED_MAX";
static const char* const PREF_CAD_MREVS = "CAD_MREVS";
static const char* const PREF_CAD_MS    = "CAD_MS";

void Statistics::restoreStats() {
	for (uint_fast8_t c = 0 ; c < SUM_ESP_START; c++) {
		StatPreferences[c].begin(SUM_TYPE_STRING[c]);
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			time_in[d][c] = StatPreferences[c].getLong(PREF_TIME_STRING[d], 0);
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Loaded time in %s for %s from preferences: %d", PREF_TIME_STRING[d], SUM_TYPE_STRING[c], time_in[d][c]);
		}
		// isKey() first: Preferences logs an [E] line for every missing key otherwise (they
		// only exist after the first autoStore() that has something to write).
		Preferences& p = StatPreferences[c];
		distFree[c]     = p.isKey(PREF_DIST_FREE) ? p.getFloat(PREF_DIST_FREE, 0.0) : 0.0;
		speed_max[c]    = p.isKey(PREF_SPEED_MAX) ? p.getFloat(PREF_SPEED_MAX, 0.0) : 0.0;
		cadMilliRevs[c] = p.isKey(PREF_CAD_MREVS) ? p.getULong64(PREF_CAD_MREVS, 0) : 0;
		cadMs[c]        = p.isKey(PREF_CAD_MS)    ? p.getULong64(PREF_CAD_MS, 0) : 0;
		//StatPreferences[c].end();
	}
}

void Statistics::autoStore() {
	bclog.log(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Store distance and time to preferences");
	for (uint_fast8_t c = 0 ; c < SUM_ESP_START; c++) {
		Preferences& p = StatPreferences[c];
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			if (!p.putLong(PREF_TIME_STRING[d], time_in[d][c])) {
				bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Can't save time in %s for %s to preferences", PREF_TIME_STRING[d], SUM_TYPE_STRING[c]);
			}
		}
		// These only change while riding -- skip the flash write when nothing changed (and
		// check isKey() first, see restoreStats()).
		if (!p.isKey(PREF_DIST_FREE) || p.getFloat(PREF_DIST_FREE, 0.0) != distFree[c])      p.putFloat(PREF_DIST_FREE, distFree[c]);
		if (!p.isKey(PREF_SPEED_MAX) || p.getFloat(PREF_SPEED_MAX, 0.0) != speed_max[c])     p.putFloat(PREF_SPEED_MAX, speed_max[c]);
		if (!p.isKey(PREF_CAD_MREVS) || p.getULong64(PREF_CAD_MREVS, 0) != cadMilliRevs[c])  p.putULong64(PREF_CAD_MREVS, cadMilliRevs[c]);
		if (!p.isKey(PREF_CAD_MS)    || p.getULong64(PREF_CAD_MS, 0) != cadMs[c])            p.putULong64(PREF_CAD_MS, cadMs[c]);
	}
	updateTimeSeries();			// FIXME: test only - there may be time or distance mode so maybe move it to cycle or updateRevs.
	for (uint_fast8_t j=0; j < 4 ; j++) {
		createChartArray(j);	// FIXME: test only
	}
}

void Statistics::dataStore() {
	SGpsFix gpsFix = bleDevs.getGpsFix();
#ifdef TRGBBC_SENSORS_I2C
	tempC = sensors.getTemp();
	height = sensors.getHeight();
	ui.updateHeight(height);
	bclog.appendDataLog(speed, sensors.getTemp(), gradient, distHandler.getDistance(), height, hr, cadence, gpsFix,
	                    gradientBaro, sensors.getImuGradient(), sensors.getRoadClass());
#else
	bclog.appendDataLog(speed, tempC, gradient, distHandler.getDistance(), height, hr, cadence, gpsFix);
#endif
	updateGpsFixIcon(gpsFix);
}

/**
 * @brief Colors/shows the base screen's GPS status icon from the latest TrailBridge fix.
 *
 * Quality is judged from horizontal accuracy (SGpsFix::accuracyMX10), the only quality metric
 * the TrailBridge protocol actually carries -- there's no real DOP available (Android's Location
 * API doesn't expose classic HDOP/PDOP either). A fix is also treated as "gone" once it's older
 * than a few heartbeat intervals: TrailBridge keeps resending the last known fix with a growing
 * fixAgeMs while it can't get a new one (tunnel, no reception), it doesn't switch to
 * POSITION_NONE for that -- see PROTOCOL.md and BikeGpsProtocol.h.
 */
void Statistics::updateGpsFixIcon(const SGpsFix& fix) {
	static const uint32_t GPS_STALE_MS = 30000;			// well beyond TrailBridge's 5s heartbeat
	static const uint16_t GPS_ACCURACY_GOOD_M_X10 = 50;	// <= 5m: green
	static const uint16_t GPS_ACCURACY_OK_M_X10 = 150;		// <= 15m: amber, else red

	bool hasFix = fix.valid && fix.fixAgeMs <= GPS_STALE_MS;
	UIFacade::UIColor color = UIFacade::UI_ColorCrit;
	if (hasFix && fix.hasAccuracy) {
		if (fix.accuracyMX10 <= GPS_ACCURACY_GOOD_M_X10) {
			color = UIFacade::UI_ColorOK;
		} else if (fix.accuracyMX10 <= GPS_ACCURACY_OK_M_X10) {
			color = UIFacade::UI_ColorWarn;
		}
	}
	ui.updateGpsFix(hasFix, color);
}

void Statistics::cycle() {
	time_t time_now = millis();
	uint32_t delta = time_now - timestamp_last;
	timestamp_last = time_now;
	time_t time_in_break = time_now - timestamp_stop;
	for (uint_fast8_t c = SUM_ESP_TOTAL; c <= SUM_ESP_START; c++) {
		// SUM_ESP_START only accumulates while a ride session is open (see
		// toggleRideMode()/stopRide(), doc/design/ride-state-machine.md §4) -- before the
		// first Start tap, or after a Stop, the Start/Ride view stays at zero even though
		// curDriveState may already be DS_FREE_RIDE (device is moving, just not "riding" yet).
		if (c == SUM_ESP_START && !rideSessionOpen) continue;
		time_in[curDriveState][c] += delta;
		if (curDriveState == DS_STOP) stopEpisodeMs[c] += delta;
		if (isMoving(curDriveState) && cadence > 0) {
			cadMs[c] += delta;
			cadMilliRevs[c] += static_cast<uint64_t>(cadence) * delta / 60;	// rpm * ms / 60000 revs, in 1/1000
		}
	}

	// Driving state fsm (Connected sub-state). rideMode (manual, Pause/Start button) picks
	// DS_FREE_RIDE vs. DS_DRIVE_COASTING/POWER on resume -- see
	// doc/design/ride-state-machine.md for the full two-axis picture.
	switch (curDriveState) {
	case DS_STOP:
		if ( time_in_break > 120000) {
			// The stop turned out to be a break: move exactly this stop's time over. (Moving
			// time_in_break instead took time from older stops whenever this stop wasn't fully
			// counted as DS_STOP, e.g. after a reconnect or a reset.)
			for (uint_fast8_t c = SUM_ESP_TOTAL; c <= SUM_ESP_START; c++) {
				uint32_t moved = min(stopEpisodeMs[c], time_in[DS_STOP][c]);
				time_in[DS_STOP][c] -= moved;
				time_in[DS_BREAK][c] += moved;
				stopEpisodeMs[c] = 0;
			}
			setCurDriveState(DS_BREAK);
		}
		//no break
	case DS_BREAK:
		if (speed > 5.5) {
			setCurDriveState(rideMode ? (cadence < 40  ? DS_DRIVE_COASTING : DS_DRIVE_POWER) : DS_FREE_RIDE);
			if (offAfterMinutes != 255) offAfterMinutes = 5;	// don't silently re-enable auto-off if the user disabled it (long-press / Pause button)
			time_in_break = 0;	// Necessary so that next if is not true
		}
		// no break - also switch off in NO_CONN
	case DS_NO_CONN:
		if (time_in_break > (offAfterMinutes * 60000)) {		// auto-switch off  (default 50min, can be delayed)
			trgb.deepSleep();
		}
		break;
	// state FREE_RIDE (moving, but rideMode == false -- covers both FreeRide and Cruise,
	// see doc/design/ride-state-machine.md §2). No cadence-based sub-split here -- Coast is
	// only a Ride sub-state.
	case DS_FREE_RIDE:
		timestamp_stop = time_now;
		if (speed < 0.3) {
			setCurDriveState(DS_STOP);
		}
		break;
	// state DRIVING
	case DS_DRIVE_COASTING:
	case DS_DRIVE_POWER:
		timestamp_stop = time_now;	// Continuously update timestamp_stop if not in break/stop/disconnected (necessary so that
		if (speed < 0.3) {
			setCurDriveState(DS_STOP);
		} else if (!rideMode) {
			// Pause was tapped mid-ride -- toggleRideMode() already switches curDriveState
			// immediately (applyRideModeToCurrentMovement()), so this is only a safety net
			// against the two ever disagreeing, not the normal path.
			setCurDriveState(DS_FREE_RIDE);
		} else {
			//TODO: add speed depended cadence limits to adapt to steep gradients
			if (cadence < 40 && curDriveState == DS_DRIVE_POWER) {
				bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Speed: %f Cadence: %d", speed, cadence);
				setCurDriveState(DS_DRIVE_COASTING);
			} else if (cadence > 50 && curDriveState == DS_DRIVE_COASTING) {
				setCurDriveState(DS_DRIVE_POWER);
			}
		}
	}
	updateStateIcon();				// Always update in cycle, to support blinking and missed changes/init.
#ifdef TRGBBC_SENSORS_I2C
	sensors.readBME280();
	updateRoadQualityUi();
	// The accelerometer gradient (if selected) is current every second, unlike the
	// barometric one -- refresh the display at that rate, not only per calculateGradient().
	float g;
	if (++gradUiCycles >= 2) {
		gradUiCycles = 0;
		if (sensors.imuGradientForDisplay(g)) {
			gradient = g;
			ui.updateGrad(gradient, height);
		}
	}
#endif
}

#ifdef TRGBBC_SENSORS_I2C
void Statistics::updateRoadQualityUi() {
	uint8_t cls = sensors.getRoadClass();
	uint32_t shocks = sensors.getShockCount();
	if (cls != roadClassShown || shocks != shockCountShown || ++roadUiCycles >= 20) {
		roadUiCycles = 0;
		roadClassShown = cls;
		shockCountShown = shocks;
		ui.updateRoadQuality(cls, sensors.getRoughness(), shocks);
	}
	// Manual road label (RQ-Ride-Screen) - refreshed every cycle, not just on
	// change, since capturing/captureS keep moving on their own even while
	// surface/quality stay put.
	I2CSensors::RoadLabelState label = sensors.getRoadLabelState();
	ui.updateRoadLabel(label.surface, label.quality, label.capturing);
}
#endif

void Statistics::delayStandby() {
	time_t time_in_break = millis() - timestamp_stop;
	if ((offAfterMinutes * 60000 - time_in_break ) < 60000 ) {
		offAfterMinutes++;
		updateStateIcon();	// Immediate update (user feedback)
	}
}

void Statistics::toggleStandbyMode() {
	if (offAfterMinutes == 255)	offAfterMinutes = ((millis()-timestamp_stop)/60000) + 5;
	else offAfterMinutes = 255;
	updateStateIcon();	// Immediate update (user feedback)
}

// ******************** Ride-state FSM (Pause/Start button) ********************
// See doc/design/ride-state-machine.md for the full model.

void Statistics::applyRideModeToCurrentMovement() {
	// Only re-decides the leaf state while actually moving -- while stopped/on a break
	// (or disconnected), rideMode's new value simply takes effect once cycle() resumes
	// movement (its speed>5.5 branch already reads rideMode), nothing to do here.
	switch (curDriveState) {
	case DS_FREE_RIDE:
	case DS_DRIVE_COASTING:
	case DS_DRIVE_POWER:
		setCurDriveState(rideMode ? (cadence < 40 ? DS_DRIVE_COASTING : DS_DRIVE_POWER) : DS_FREE_RIDE);
		break;
	default:
		break;
	}
}

void Statistics::toggleRideMode() {
	if (!rideMode) {
		if (!rideSessionOpen) {
			// First Start tap (not a resume from Cruise): open a new ride session and zero
			// the Start/Ride view (reset() also takes the session's distance base).
			rideSessionOpen = true;
			reset(SUM_ESP_START);
			bclog.log(BCLogger::Log_Info, BCLogger::TAG_STAT, "Ride session started");
		}
		rideMode = true;
	} else {
		rideMode = false;	// Pause tapped -> Cruise. Session stays open (rideSessionOpen unchanged).
	}
	applyRideModeToCurrentMovement();
	updateStateIcon();	// Immediate update (user feedback)
}

void Statistics::stopRide() {
	// Keep the finished session's distance on show; the time already stops accumulating.
	sessionEndNet = distHandler.getDistance(SUM_ESP_START, false) - sessionBaseNet;
	sessionEndGross = distHandler.getDistance(SUM_ESP_START, true) - sessionBaseGross;
	rideMode = false;
	rideSessionOpen = false;
	applyRideModeToCurrentMovement();
	bclog.log(BCLogger::Log_Info, BCLogger::TAG_STAT, "Ride session stopped (long-press)");
	updateStateIcon();	// Immediate update (user feedback)
}

void Statistics::handlePauseButtonHold() {
	if (rideSessionOpen) {
		stopRide();
	} else {
		// No ride session to stop -- long-press keeps its pre-existing meaning, RimRidge's
		// only control for the auto-standby timer.
		toggleStandbyMode();
	}
}

void Statistics::setConnected(bool connected) {
	if (connected && (curDriveState == DS_NO_CONN)) {
		// Always resume as a fresh stop: there's no speed yet, and the state from before the
		// disconnect (possibly "moving" an hour ago) would count time as moving until the first
		// speed arrives. The usual >5.5 km/h check then picks the moving state.
		setCurDriveState(DS_STOP);
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_STAT, "Connected to speed sensor - counting time");
	}
	if (!connected && curDriveState != DS_NO_CONN) {
		addSpeed(NAN);
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_STAT, "Disconnected from speed sensor - stop time counters");
		setCurDriveState(DS_NO_CONN);
	}
}

void Statistics::updateStateIcon() {
	UIFacade::UIColor color = UIFacade::UI_ColorNeutral;
	if (curDriveState == DS_NO_CONN || curDriveState == DS_BREAK) {
		if (offAfterMinutes == 255)	color = UIFacade::UI_ColorOK;
		else {
			time_t time_in_break = millis() - timestamp_stop;
			time_t remaining = offAfterMinutes * 60000 - time_in_break ;
			if (( remaining < 60000) && (((time_in_break / 1000) % 2) == 1)) { // part after '&&' enables blinking - TODO: Have blink function(s) based on cycle() count
				if (remaining > 10000) {
					color = UIFacade::UI_ColorWarn;
				} else {
					color = UIFacade::UI_ColorCrit;
					char buffer[30];
					snprintf(buffer, 30, "Switch off in %02d sec",(remaining/1000));
					if (!shutdownMsg) {
						shutdownMsg = true;
						ui.showMsgBox(String(buffer), [this](bool ok) {
							shutdownMsg = false;
							if (ok) {
								trgb.deepSleep();
							} else {
								delayStandby();
							}
						});
					} else {
						ui.updateMsgBox(String(buffer));
					}
				}

			}
		}
	}
	ui.updateStateIcon(curDriveState, color, rideMode);
}

// ******************** Helper functions (static) ********************
//static
void Statistics::addFloatToDatapoint(S_DataPoint& data, const float val) {
	// Use negated comparison, so statement is true if stored value is NAN
	if (! (data.min < val)) data.min = val;
	if (! (data.max > val)) data.max = val;
	if (isnan(data.avg)) data.avg = 0;
	data.avg += val;
}

//static
float Statistics::setDatapointAvg(S_DataPoint &data, uint8_t& count) {
	data.avg = data.avg / count;
	count = 0;
	return data.avg;
}

// ******************** Add data to statistics ********************
void Statistics::addCadence(int16_t _cadence, int16_t _total) {
	cadence = _cadence;
	cadence_tot = _total;

	addFloatToDatapoint(distanceData.currentMinMax.cadence, (cadence * 1.0));
	distanceData.curCountCadence++;

	addFloatToDatapoint(timeData.currentMinMax.cadence, (cadence * 1.0));
	timeData.curCountCadence++;

	ui.updateCadence(cadence);
}

void Statistics::addHR(int16_t _hr) {
	hr = _hr;
	float hr_float = hr > 0 ? hr * 1.0 : NAN;

	addFloatToDatapoint(distanceData.currentMinMax.hr, hr_float );
	distanceData.curCountHr++;

	addFloatToDatapoint(timeData.currentMinMax.hr, hr_float);
	timeData.curCountHr++;

	ui.updateHR(hr);
}

void Statistics::addSpeed(float _speed) {
	speed = _speed;
	speedUpdateMs = millis();
	// Plausibility cap: a single bad sample (e.g. the 16-bit 1/1024s event time wrapping after a
	// long gap between notifications) would otherwise stick as the max forever -- it's persisted.
	if (speed <= 120.0) {
		for (uint_fast8_t i = 0; i<= SUM_ESP_START; i++) {
			if (i == SUM_ESP_START && !rideSessionOpen) continue;
			if (speed_max[i] < speed) speed_max[i] = speed;
		}
	}

	addFloatToDatapoint(distanceData.currentMinMax.speed, speed);
	distanceData.curCountSpeed++;

	addFloatToDatapoint(timeData.currentMinMax.speed, speed);
	timeData.curCountSpeed++;

	ui.updateSpeed(speed);
}

void Statistics::addDistanceDelta(float deltaM) {
	// Same SUM_ESP_START gate as cycle()'s time_in[][] (§4 of the design doc).
	if (!(deltaM > 0) || curDriveState != DS_FREE_RIDE) return;	// also excludes NAN
	for (uint_fast8_t c = SUM_ESP_TOTAL; c <= SUM_ESP_START; c++) {
		if (c == SUM_ESP_START && !rideSessionOpen) continue;
		distFree[c] += deltaM;
	}
}

void Statistics::addGradientHeight(float _grad, float _height) {
	gradient = _grad;
	addFloatToDatapoint(distanceData.currentMinMax.gradient, gradient);
	height = _height;
	addFloatToDatapoint(distanceData.currentMinMax.height, height);
	distanceData.curCountGradHeight++;
	ui.updateGrad(gradient, height);
}

void Statistics::updateDistanceSeries() {
	float distance = distHandler.getDistance();
	if ( (distance - distanceData.curDistance) >= 100) {		// every 100m
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "100m step - distance: %.1fm", distance);
		distanceData.curDistance = distance;

		setDatapointAvg(distanceData.currentMinMax.cadence, distanceData.curCountCadence);
		setDatapointAvg(distanceData.currentMinMax.gradient, distanceData.curCountGradHeight);
		setDatapointAvg(distanceData.currentMinMax.height, distanceData.curCountGradHeight);  //FIMXE
		setDatapointAvg(distanceData.currentMinMax.hr, distanceData.curCountHr);
		setDatapointAvg(distanceData.currentMinMax.speed, distanceData.curCountSpeed);
		time(&distanceData.currentMinMax.timestamp);	// --> write timestamp

		distanceData.data[distanceData.index++] = distanceData.currentMinMax;
		if (distanceData.index >= DISTANCE_SERIES_LEN) {
			bclog.log(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Statistics DistanceData overflow - resetting index to 0");
			distanceData.index = 0;
		}
		distanceData.currentMinMax.cadence.min = NAN;
		distanceData.currentMinMax.cadence.avg = NAN;
		distanceData.currentMinMax.cadence.max = NAN;
		distanceData.curCountCadence = 0;

		distanceData.currentMinMax.gradient.min = NAN;
		distanceData.currentMinMax.gradient.avg = NAN;
		distanceData.currentMinMax.gradient.max = NAN;
		distanceData.currentMinMax.height.min = NAN;
		distanceData.currentMinMax.height.avg = NAN;
		distanceData.currentMinMax.height.max = NAN;
		distanceData.curCountGradHeight = 0;

		distanceData.currentMinMax.hr.min = NAN;
		distanceData.currentMinMax.hr.avg = NAN;
		distanceData.currentMinMax.hr.max = NAN;
		distanceData.curCountHr = 0;

		distanceData.currentMinMax.speed.min = NAN;
		distanceData.currentMinMax.speed.avg = NAN;
		distanceData.currentMinMax.speed.max = NAN;
		distanceData.curCountSpeed = 0;
	}
}

void Statistics::updateTimeSeries() {
	time_t now;
	time(&now);
	//if ( (now - timeData.startTime) >= 5000) {
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "10s step @ %d", now);
		timeData.startTime = now;

		setDatapointAvg(timeData.currentMinMax.cadence, timeData.curCountCadence);
		setDatapointAvg(timeData.currentMinMax.hr, timeData.curCountHr);
		setDatapointAvg(timeData.currentMinMax.speed, timeData.curCountSpeed);
		timeData.currentMinMax.distance = distHandler.getDistance();

		timeData.data[timeData.index++] = timeData.currentMinMax;
		if (timeData.index >= 400) {
			bclog.log(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Statistics TimeData overflow - resetting index to 0");
			timeData.index = 0;
		}
		timeData.currentMinMax.cadence.min = NAN;
		timeData.currentMinMax.cadence.avg = NAN;
		timeData.currentMinMax.cadence.max = NAN;
		timeData.curCountCadence = 0;

		timeData.currentMinMax.hr.min = NAN;
		timeData.currentMinMax.hr.avg = NAN;
		timeData.currentMinMax.hr.max = NAN;
		timeData.curCountHr = 0;

		timeData.currentMinMax.speed.min = NAN;
		timeData.currentMinMax.speed.avg = NAN;
		timeData.currentMinMax.speed.max = NAN;
		timeData.curCountSpeed = 0;
//	}
}

void Statistics::createChartArray(uint8_t idx) {
	uint32_t startcount = xthal_get_ccount();
	bool modeTime = true; // true: X axis is time, false: X axis is distance
	float scale = 1.0;
	DataClass dc = chart_array_type[idx];

	// timeData.index is always the last written position + 1 - so the next value to be written. And thus it's also the first value to be shown in chart, if the newest point is the most right one.
	chart_array_startPos[idx] = timeData.index;

	for (uint16_t point = 0 ; point < 400 ; point ++) {
		S_DataPoint dp;
		switch (dc) {
		case SPEED:
			dp = timeData.data[point].speed;
			break;
		case HR:
			dp = timeData.data[point].hr;
			break;
		case CADENCE:
			dp = timeData.data[point].cadence;
			break;
		case DISTANCE:
			timeData.data[point].distance;
			break;
		case HEIGHT:
		case GRADIENT:
		case TEMPERATURE:
			//FIXME: in distance only
			break;
		};
		//FIXME: Finalize min/max evalution
//		bool useMax = (point > 0 && dp.max > timeData.data[point-1].hr.max) && (point < 399 && dp.max > timeData.data[point+1].hr.max);
//		bool useMin = (point > 0 && dp.min < timeData.data[point-1].hr.min) && (point < 399 && dp.min < timeData.data[point+1].hr.min);
//		if (useMax && useMin) {
//			useMax = false;
//			useMin = false;
//		}
		float val = NAN;
		if (dc == DISTANCE) {
			val = timeData.data[point].distance;
		} else {
			//val = (useMax ? dp.max : ( useMin ? dp.min : dp.avg) ) * scale;
			val = dp.avg * scale;
		}
		chart_array[idx][point] = isnan(val) ? INT16_MAX : static_cast<int16_t>(round(val));
	}
	uint32_t endcount = xthal_get_ccount();
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Took %d cycles to update chart", endcount - startcount);
}

void Statistics::checkDistance(const float dist) {
	if ( (dist - distSeries_last) > 500) {
		distSeries_last = dist;
		updateDistanceSeries();
	}
	calculateGradient(dist);
}

// ******************** internal calculations ********************

void Statistics::calculateGradient(float newDist) {
/* Don't compile own gradient calculation if an external gradient calculation is done (like in Forumslader) */
#ifndef BC_FL_SUPPORT
	time_t time_now = millis();
	uint32_t delta = time_now - gradient_timestamp;  // [msec]
	float delta_dist = newDist - gradient_dist;
	if ( ( delta_dist >= 8 && delta > 2500) || delta >= 5000 /*|| delta_height > 0.5*/ ) {   // Using delta_height seems to cause more harm than good
		// Get Height information, so it's synchronized with distance update (improves accuracy)
		sensors.readBME280();
		float height_new = sensors.getHeight();		// [m]
		float delta_height = height_new - gradient_height;
		if (delta_dist < 0 || delta_dist > 200) {
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Gradient calculation: New distance %.2fm implausible (delta: %.2fm) ", newDist, delta_dist);
			delta_dist = NAN;
		}
		gradient_height = height_new;
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Update gradient with deltas time: %dms, distance: %.2fm, height: %.2fm", delta, delta_dist, delta_height);
		gradient_timestamp = time_now;

		//Note: Keep it outside the next if. Otherwise large delta in height will result in extreme spikes in gradient. ("Elevator case").
		//      It does not make sense to do a calculation for this, because it will be wrong in many cases anyhow.
		gradient_dist = newDist;

		float gradient_new = NAN; // division by 0 should not lead not +Inf or -Inf, because delta_height is also close to zero. Ideally it would be zero, so 0/0 --> NAN.
		if (delta_dist > 0.2) {
			gradient_new = delta_height / (delta_dist) * 100.0;		// simplified gradient calculation. Accurate enough for smaller gradients.
		}
		gradientBaro = gradient_new;
		float gradient_shown = gradient_new;
#ifdef TRGBBC_SENSORS_I2C
		// The barometric gradient is the reference the accelerometer learns its mounting
		// offset against; the accelerometer's is shown instead only if selected ("rq gradsrc imu").
		sensors.feedBaroGradient(gradient_new, delta_dist);
		sensors.imuGradientForDisplay(gradient_shown);
#endif
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Gradient: %.2f (barometric %.2f)", gradient_shown, gradient_new);
		addGradientHeight(gradient_shown, height_new);
	}
#endif
}

void Statistics::addTemperature(float _temperature) {
	tempC = _temperature;
}

uint32_t Statistics::getDistance(ESummaryType type, bool includeLost) const {
	switch (type) {
	case SUM_ESP_TOTAL:
	case SUM_ESP_TOUR:
	case SUM_ESP_TRIP:
		return distHandler.getDistance(type, includeLost);
	case SUM_ESP_START: {
		// Ride session window on Distance's since-power-on counter (see sessionBaseNet).
		if (!rideSessionOpen) return includeLost ? sessionEndGross : sessionEndNet;
		float d = distHandler.getDistance(type, includeLost) - (includeLost ? sessionBaseGross : sessionBaseNet);
		return d > 0 ? d : 0;
	}
#ifdef BC_FL_SUPPORT
	//FIXME FL_ Distance handling
	case SUM_FL_TRIP:
	case SUM_FL_TOUR:
		//return start_distance[type];
		return 0;
	case SUM_FL_TOTAL:
		//return distance;
		return 0;
#endif
	}
	return 0;
}

void Statistics::reset(ESummaryType type) {	//TODO: Move to DistanceHandler
	for (uint_fast8_t d = 0; d < EDrivingStateMax; d++) {
		time_in[d][type] = 0;
	}
	stopEpisodeMs[type] = 0;
	distFree[type] = 0;
	speed_max[type] = 0;
	cadMilliRevs[type] = 0;
	cadMs[type] = 0;
	if (type == SUM_ESP_START) {
		sessionBaseNet = distHandler.getDistance(SUM_ESP_START, false);
		sessionBaseGross = distHandler.getDistance(SUM_ESP_START, true);
		sessionEndNet = sessionEndGross = 0;
	}
}

void Statistics::setCurDriveState(EDrivingState _curDriveState) {
	if (_curDriveState != curDriveState) {
		// Logged here, not in cycle()'s callers, so every transition is caught regardless
		// of which branch triggered it -- see doc/design/ride-state-machine.md and
		// LogRec::RideState's comment for why this feeds the GPX <trkseg> export.
		LogRec::RideState rec = {};
		BCLogger::nowEpoch(rec.timestamp, rec.timestampMs);
		rec.state = _curDriveState;
		rec.prevState = curDriveState;
		rec.rideMode = rideMode ? 1 : 0;
		rec.stateSeq = ++rideStateSeq;
		rec.recordType = LogRec::TYPE_RIDESTATE;
		rec.formatVersion = LogRec::FORMAT_VERSION;
		bclog.appendRecord(rec);
	}
	curDriveState = _curDriveState;
	if (_curDriveState == DS_STOP) {
		timestamp_stop = millis();
		for (uint_fast8_t c = 0; c < ESummaryTypeMax; c++) stopEpisodeMs[c] = 0;
	}
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Driving state changed to %s", (PREF_TIME_STRING[curDriveState]+8));
}

uint32_t Statistics::getTime(ESummaryType type, EAvgType avgtype) const {
	// Only time with a connected speed sensor counts -- DS_NO_CONN never does (the distance
	// ridden then is "lost" distance, see Distance::updateRevs(), and not in the net distance
	// getAvg() divides). Coast vs. Power is display-only and makes no difference here.
	// Summed in 64 bit: each bucket is a uint32_t of ms (~1193 h), the sum would wrap earlier.
	const uint64_t ride = static_cast<uint64_t>(time_in[DS_DRIVE_COASTING][type]) + time_in[DS_DRIVE_POWER][type];
	const uint64_t moving = ride + time_in[DS_FREE_RIDE][type];
	uint64_t t = 0;
	switch (avgtype) {
	case AVG_NOCRUISE: t = ride; break;
	case AVG_DRIVE:    t = moving; break;
	case AVG_NOBREAK:  t = moving + time_in[DS_STOP][type]; break;
	case AVG_ALL:      t = moving + time_in[DS_STOP][type] + time_in[DS_BREAK][type]; break;
	default: break;
	}
	return static_cast<uint32_t>(t / 1000);		//msec to sec
}

float Statistics::getAvg(ESummaryType type, EAvgType avgtype) const {
	// Net distance: what the sensor counted while connected, i.e. exactly the distance the
	// recorded time belongs to. Stops and breaks add time but (almost) no distance, so the
	// variants only differ in the time -- except AVG_NOCRUISE, which leaves out the
	// FreeRide/Cruise distance as well as its time.
	const uint32_t t = getTime(type, avgtype);
	if (t == 0) return NAN;
	float distM = getDistance(type, false);
	if (avgtype == AVG_NOCRUISE) {
		distM -= distFree[type];
		if (distM < 0) distM = 0;
	}
	//        .. in m / sec  * 3.6 km/h / m/s..
	return (distM / static_cast<float>(t)) * 3.6;
}

float Statistics::getAvgCadence(ESummaryType type) const {
	if (cadMs[type] == 0) return NAN;
	// milli-revs / ms = revs/s * ... -> rpm = revs / min = (mrevs/1000) / (ms/60000)
	return static_cast<float>(cadMilliRevs[type]) * 60.0f / static_cast<float>(cadMs[type]);
}

void Statistics::setupWebserverSummary() {
	// Ride statistics as one document, read by /stat/statistics.html. Times in seconds,
	// distances in m, speeds in km/h; NAN (no time yet) serializes as null.
	webserver.getServer().on("/stat/summary", HTTP_GET, [this](AsyncWebServerRequest *request) {
		JsonDocument doc;
		JsonObject st = doc["state"].to<JsonObject>();
		st["drive"] = PREF_TIME_STRING[curDriveState] + 8;		// +8 skips "TIME_IN_"
		st["connected"] = isConnected();
		st["rideMode"] = rideMode;
		st["session"] = rideSessionOpen;
		static const char* const kAvg[EAvgTypeMax] = { "ALL", "DRIVE", "NOBREAK", "NOCRUISE" };
		for (uint_fast8_t t = 0; t <= SUM_ESP_START; t++) {
			const ESummaryType sType = static_cast<ESummaryType>(t);
			JsonObject o = doc[SUM_TYPE_STRING[t] + 3].to<JsonObject>();	// +3 skips "ST_"
			const uint32_t net = getDistance(sType, false);
			const uint32_t gross = getDistance(sType, true);
			JsonObject d = o["dist"].to<JsonObject>();
			d["net"] = net;
			d["lost"] = gross > net ? gross - net : 0;
			d["cruise"] = static_cast<uint32_t>(distFree[t]);
			JsonObject tm = o["time"].to<JsonObject>();
			tm["moving"] = getTime(sType, AVG_DRIVE);
			tm["ride"]   = getTime(sType, AVG_NOCRUISE);
			tm["cruise"] = time_in[DS_FREE_RIDE][t] / 1000;
			tm["coast"]  = time_in[DS_DRIVE_COASTING][t] / 1000;
			tm["stops"]  = time_in[DS_STOP][t] / 1000;
			tm["breaks"] = time_in[DS_BREAK][t] / 1000;
			tm["total"]  = getTime(sType, AVG_ALL);
			tm["noconn"] = time_in[DS_NO_CONN][t] / 1000;		// BC on without speed sensor -- not ride time
			JsonObject a = o["avg"].to<JsonObject>();
			for (uint_fast8_t k = 0; k < EAvgTypeMax; k++) {
				a[kAvg[k]] = getAvg(sType, static_cast<EAvgType>(k));
			}
			o["maxSpeed"] = getSpeedMax(sType);
			o["cadence"] = getAvgCadence(sType);
		}
		String json;
		serializeJson(doc, json);
		request->send(200, "application/json", json);
	});
}

void Statistics::setupWebserverDebug() {
	webserver.getServer().on("/stat/debugarray", HTTP_GET, [this](AsyncWebServerRequest *request) {
		String htmlPage;
		htmlPage.reserve(65536);		// ~400 rows; see WebPage::begin() on why this is reserved up front
		WebPage::begin(htmlPage, "Chart Array");
		htmlPage += F("<p class=\"eyebrow\">Raw contents of the heart-rate chart ring buffer. "
		              "The arrow marks the current write position.</p>\n"
		              "<table><thead><tr><th>Index</th><th>Chart</th><th>Min</th><th>Avg</th><th>Max</th>"
		              "</tr></thead><tbody>\n<tr><td>Current</td><td>Count: ");
		htmlPage += timeData.curCountHr;
		htmlPage += F("</td><td>");
		htmlPage += timeData.currentMinMax.hr.min;
		htmlPage += F("</td><td>");
		htmlPage += timeData.currentMinMax.hr.avg;
		htmlPage += F("</td><td>");
		htmlPage += timeData.currentMinMax.hr.max;
		htmlPage += F("</td></tr>\n");
		for (size_t i = 0; i < sizeof(this->chart_array[1]) / sizeof(this->chart_array[1][0]); i++) {
			const S_DataPoint& data = timeData.data[i].hr;
			htmlPage += F("<tr><td>");
			if (chart_array_startPos[1] == static_cast<int>(i)) htmlPage += F("&rarr; ");
			htmlPage += i;
			htmlPage += F("</td><td>");
			htmlPage += chart_array[1][i];
			htmlPage += F("</td><td>");
			htmlPage += data.min;
			htmlPage += F("</td><td>");
			htmlPage += data.avg;
			htmlPage += F("</td><td>");
			htmlPage += data.max;
			htmlPage += F("</td></tr>\n");
		}
		htmlPage += F("</tbody></table>\n");
		WebPage::end(htmlPage);
		request->send(200, "text/html", htmlPage);
	});
}

