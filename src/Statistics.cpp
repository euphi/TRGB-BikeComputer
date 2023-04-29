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
	statStore.attach(5, +[](Statistics* thisInstance) { thisInstance->autoStore(); }, this);
	restoreStats();
}

void Statistics::restoreStats() {
	for (uint_fast8_t c = 0 ; c < SUM_ESP_START; c++) {
		StatPreferences[c].begin(SUM_TYPE_STRING[c]);
		start_distance[c] = StatPreferences[c].getLong("START_DISTANCE", 0);
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			time_in[d][c] = StatPreferences[c].getLong(PREF_TIME_STRING[d], 0);
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Loaded time in %s for %s from preferences: %d", PREF_TIME_STRING[d], SUM_TYPE_STRING[c], time_in[d][c]);
		}
		//StatPreferences[c].end();
	}
}

void Statistics::autoStore() {
	bclog.log(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Store distance to preferences");
	for (uint_fast8_t c = 0 ; c < SUM_ESP_START; c++) {
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Store distance to preferences %s", SUM_TYPE_STRING[c]);
		if (!StatPreferences[c].putLong("START_DISTANCE", start_distance[c])) {
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Can't save distance %s to preferences", SUM_TYPE_STRING[c]);
		}
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			if (!StatPreferences[c].putLong(PREF_TIME_STRING[d], time_in[d][c])) {
				bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Can't save time in %s for %s to preferences", PREF_TIME_STRING[d], SUM_TYPE_STRING[c]);
			}
		}
	}
}

void Statistics::cycle() {
	time_t time_now = millis();
	uint32_t delta = time_now - timestamp_last;
	timestamp_last = time_now;
	for (uint_fast8_t c = SUM_ESP_TOTAL; c <= SUM_ESP_START; c++) {
		time_in[curDriveState][c] += delta;
	}

	// Driving state fsm (Connected sub-state)
	switch (curDriveState) {
	case DS_STOP:
		if (time_now - timestamp_stop > 120000) {
			for (uint_fast8_t c = SUM_ESP_TOTAL; c <= SUM_ESP_START; c++) {
				if (time_in[DS_STOP][c] > 120000) time_in[DS_STOP][c] -= 120000; else time_in[DS_STOP][c] = 0;
			}
			setCurDriveState(DS_BREAK);
		}
		//no break
	case DS_BREAK:
		if (speed > 2.5) {
			setCurDriveState(DS_DRIVE_POWER);
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
	if (connected && (curDriveState == DS_NO_CONN)) setCurDriveState(histDriveState);
	if (!connected && curDriveState != DS_NO_CONN) {
		histDriveState = curDriveState;
		setCurDriveState(DS_NO_CONN);
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
	distance = _dist;
	if (start_distance[SUM_ESP_START] == 0) addDistance(distance, SUM_ESP_START);
}

void Statistics::addGradient(int16_t _grad, int16_t _height) {
	height = _height;
	grad = _grad;
	ui.updateGrad(grad, height);
}

uint32_t Statistics::getDistance(ESummaryType type) const {
	switch (type) {
	case SUM_ESP_TOTAL:
	case SUM_ESP_TOUR:
	case SUM_ESP_TRIP:
	case SUM_ESP_START:
		return distance-start_distance[type];
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
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			time_in[d][type] = 0;
		}
	}
}

void Statistics::setCurDriveState(EDrivingState _curDriveState) {
	curDriveState = _curDriveState;
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
	return (getDistance(type) / static_cast<float>(getTime(type, avgtype))) * 3.6;

}

float Statistics::getAvgCadence(EAvgType avgtype) const {
	//FIXME: avg does not take into account distance in no connection. There should be at least a mechanism to compensate distance in NO_CONN
	//TRACE: Serial.print(getDistance(type)); Serial.print('\t');Serial.println(relevantTime);

	//        .. in m         / sec                             * 3.6 km/h / m/s..
		return (cadence_tot / static_cast<float>(getTime(SUM_ESP_START, avgtype)) ) * 3.6;

}

