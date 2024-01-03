/*
 * Statistics.cpp
 *
 *  Created on: 31.12.2022
 *      Author: ian
 */

#include "Statistics.h"
#include "Singletons.h"

const char* Statistics::PREF_TIME_STRING[Statistics::EDrivingStateMax] = {
		"TIME_IN_NOCONN",	//		DS_NO_CONN,
		"TIME_IN_BREAK",	//		DS_BREAK,
		"TIME_IN_STOP",		//		DS_STOP,
		"TIME_IN_COAST",	//		DS_DRIVE_COASTING,
		"TIME_IN_DRIVE"		//		DS_DRIVE_POWER,
};

const char* Statistics::SUM_TYPE_STRING[Statistics::ESummaryTypeMax] = {
		"ST_TOTAL",
		"ST_TOUR",
		"ST_TRIP",
		"ST_START",
		"ST_FL_TOTAL",
		"ST_FL_TOUR",
		"ST_FL_TRIP",
};

const char* Statistics::AVG_TYPE_STRING[Statistics::EAvgTypeMax] = {
		"⏱ kompl",
		"⏱ fahrend",
		"⏱ Stops"
};

Statistics::Statistics() {
	timestamp_last = millis();
}

void Statistics::setup() {
	// What kind of sorcery is this?  --> See https://stackoverflow.com/questions/60985496/arduino-esp8266-esp32-ticker-callback-class-member-function
	statCycle.attach_ms(500, +[](Statistics* thisInstance) { thisInstance->cycle(); }, this);
	statDataStore.attach(5, +[](Statistics* thisInstance) { thisInstance->dataStore(); }, this);
	statStore.attach(5, +[](Statistics* thisInstance) { thisInstance->autoStore(); }, this);
	restoreStats();
}

void Statistics::restoreStats() {
	for (uint_fast8_t c = 0 ; c < SUM_ESP_START; c++) {
		StatPreferences[c].begin(SUM_TYPE_STRING[c]);
		start_distance[c] = StatPreferences[c].getLong("START_DISTANCE", 0);
		lost_distance[c]  = StatPreferences[c].getLong("LOST_DISTANCE",  0);
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Loaded distance %d (lost: %d) from preferences: %s", start_distance[c], lost_distance[c], SUM_TYPE_STRING[c]);
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			time_in[d][c] = StatPreferences[c].getLong(PREF_TIME_STRING[d], 0);
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Loaded time in %s for %s from preferences: %d", PREF_TIME_STRING[d], SUM_TYPE_STRING[c], time_in[d][c]);
		}
		//StatPreferences[c].end();
	}
	StatPreferences[SUM_ESP_START].begin(SUM_TYPE_STRING[SUM_ESP_START]);
	start_distance[SUM_ESP_START] =StatPreferences[SUM_ESP_START].getLong("START_DISTANCE", 0);
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Loaded start distance %d from preferences.", start_distance[SUM_ESP_START]);
	distance = start_distance[SUM_ESP_START];
}

void Statistics::autoStore() {
	bclog.log(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Store distance to preferences");
	for (uint_fast8_t c = 0 ; c < SUM_ESP_START; c++) {
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Store distance to preferences %s", SUM_TYPE_STRING[c]);
		if (!StatPreferences[c].putLong("START_DISTANCE", start_distance[c])) {
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Can't save distance %s to preferences", SUM_TYPE_STRING[c]);
		}
		if (!StatPreferences[c].putLong("LOST_DISTANCE", lost_distance[c])) {
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Can't save lost distance %s to preferences", SUM_TYPE_STRING[c]);
		}
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			if (!StatPreferences[c].putLong(PREF_TIME_STRING[d], time_in[d][c])) {
				bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Can't save time in %s for %s to preferences", PREF_TIME_STRING[d], SUM_TYPE_STRING[c]);
			}
		}
	}
	if (!StatPreferences[SUM_ESP_START].putLong("START_DISTANCE", distance)) {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Can't save current distance (SUM_ESP_START) to preferences");
	}

}

void Statistics::dataStore() {
	bclog.appendDataLog(speed, NAN, grad, distance, height * 1.0, hr);
}

void Statistics::cycle() {
	time_t time_now = millis();
	uint32_t delta = time_now - timestamp_last;
	timestamp_last = time_now;
	time_t time_in_break = time_now - timestamp_stop;
	for (uint_fast8_t c = SUM_ESP_TOTAL; c <= SUM_ESP_START; c++) {
		time_in[curDriveState][c] += delta;
	}

	// Driving state fsm (Connected sub-state)
	switch (curDriveState) {
	case DS_STOP:
		if ( time_in_break > 120000) {
			for (uint_fast8_t c = SUM_ESP_TOTAL; c <= SUM_ESP_START; c++) {
				if (time_in[DS_STOP][c] >= time_in_break) {
					time_in[DS_STOP][c] -= time_in_break;
				} else { // else path can happen after data reset
					time_in_break = time_in[DS_STOP][c];
					time_in[DS_STOP][c] = 0;
				}
				time_in[DS_BREAK][c] += time_in_break;
			}
			setCurDriveState(DS_BREAK);
		}
		//no break
	case DS_BREAK:
		if (speed > 5.5) {
			setCurDriveState(cadence < 40  ? DS_DRIVE_COASTING : DS_DRIVE_POWER);
		}
		if (time_in_break > (5 * 60000)) {		// switch off after 5 minutes
			// TODO: Add warning on display before switch-off and add button to override
			trgb.deepSleep();
		}
		break;
	// state DRIVING
	case DS_DRIVE_COASTING:
	case DS_DRIVE_POWER:
		if (speed < 0.3) {
			setCurDriveState(DS_STOP);
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
}

void Statistics::setConnected(bool connected) {
	if (connected && (curDriveState == DS_NO_CONN)) {
		setCurDriveState(histDriveState);
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_STAT, "Connected to speed sensor - counting time");
	}
	if (!connected && curDriveState != DS_NO_CONN) {
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_STAT, "Disconnected from speed sensor - stop time counters");
		histDriveState = curDriveState;
		setCurDriveState(DS_NO_CONN);
		distance_start = false;
	}
}

void Statistics::addCadence(int16_t _cadence, int16_t _total) {
	cadence = _cadence;
	cadence_tot = _total;
	ui.updateCadence(cadence);
}

void Statistics::addHR(int16_t _hr) {
	hr = _hr;
	//TODO: add HR
	ui.updateHR(hr);
}

void Statistics::addSpeed(float _speed) {
	speed = _speed;
	for (uint_fast8_t i = 0; i<= SUM_ESP_START; i++) {
		if (speed_max[i] < speed) speed_max[i] = speed;
	}
	ui.updateSpeed(speed);

}

void Statistics::addDistance(uint32_t dist, ESummaryType type) {
	start_distance[type] = dist;
}

void Statistics::updateDistance(uint32_t _dist) {
	if (curDriveState == DS_NO_CONN) {
		uint32_t dist_lost_new = _dist - distance;
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Lost %d pulses of distance.", dist_lost_new);
		updateLostDistance(dist_lost_new);
	}
	distance = _dist;
	if (!distance_start) {
		distance_start = true;
		addDistance(_dist, SUM_ESP_START);
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Initial distance: %d pulses.", _dist);
	}
}

void Statistics::updateLostDistance(uint32_t _dist_lost) {
	// distance_start == true --> initial start. Don't set lost_distance for SUM_ESP_START then.
	for (uint_fast8_t i = 0; i <= (distance_start ? SUM_ESP_START : SUM_ESP_TRIP); i++) {
		lost_distance[i] += _dist_lost;
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Lost distance for %s now %d pulses", SUM_TYPE_STRING[i], lost_distance[i]);
	}
}

void Statistics::addGradient(int16_t _grad, int16_t _height) {
	height = _height;
	grad = _grad;
	ui.updateGrad(grad, height);
}

uint32_t Statistics::getDistance(ESummaryType type, bool includeLost) const {
	switch (type) {
	case SUM_ESP_TOTAL:
	case SUM_ESP_TOUR:
	case SUM_ESP_TRIP:
	case SUM_ESP_START:
		return distance - start_distance[type] - (includeLost ? 0 : lost_distance[type]);
	case SUM_FL_TRIP:
	case SUM_FL_TOUR:
		return start_distance[type];
	case SUM_FL_TOTAL:
		return distance;
	}
	return 0;
}

void Statistics::reset(ESummaryType type) {
	if (type==SUM_ESP_TOUR || type == SUM_ESP_TRIP) {
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Reset statistic for %s.", SUM_TYPE_STRING[type]);
		start_distance[type] = distance;
		lost_distance[type] = 0;
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			time_in[d][type] = 0;
		}
	}
}

void Statistics::setCurDriveState(EDrivingState _curDriveState) {
	curDriveState = _curDriveState;
	if (_curDriveState == DS_STOP) timestamp_stop = millis();
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Driving state changed to %s", (PREF_TIME_STRING[curDriveState]+8));
}

uint32_t Statistics::getTime(ESummaryType type, EAvgType avgtype) const {
	//TODO: uint32_t is too short for time in msec (roll-over every 7 weeks)
	uint32_t relevantTime = time_in[DS_DRIVE_COASTING][type] + time_in[DS_DRIVE_POWER][type];
	switch (avgtype) {
	case AVG_ALL:
		relevantTime += time_in[DS_BREAK][type];
		//no break
	case AVG_NOBREAK:
		relevantTime += time_in[DS_STOP][type];
	}
	return relevantTime/1000;		//msec to sec
}

float Statistics::getAvg(ESummaryType type, EAvgType avgtype) const {
	//FIXME: avg does not take into account distance in no connection. There should be at least a mechanism to compensate distance in NO_CONN
	//TRACE: Serial.print(getDistance(type)); Serial.print('\t');Serial.println(relevantTime);

	//        .. in m         / sec                     * 3.6 km/h / m/s..
	return (getDistance(type, false) / static_cast<float>(getTime(type, avgtype))) * 3.6;

}

float Statistics::getAvgCadence(EAvgType avgtype) const {
	//FIXME: avg does not take into account distance in no connection. There should be at least a mechanism to compensate distance in NO_CONN
	//TRACE: Serial.print(getDistance(type)); Serial.print('\t');Serial.println(relevantTime);

	//        .. in m         / sec                             * 3.6 km/h / m/s..
		return (cadence_tot / static_cast<float>(getTime(SUM_ESP_START, avgtype)) ) * 3.6;

}

