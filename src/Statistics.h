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
private:
	enum EDrivingState {	// Various driving states
		DS_NO_CONN,
		DS_BREAK,
		DS_STOP,
		DS_DRIVE_COASTING,
		DS_DRIVE_POWER,
		EDrivingStateMax
	};

	enum EAvgType {
		AVG_ALL,
		AVG_DRIVE,
		AVG_NOBREAK,   // but Pause/Stop
		EAvgTypeMax
	};

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

	time_t timestamp_last;
	uint32_t time_in[EDrivingStateMax][ESummaryTypeMax];
	time_t timestamp_stop;

	//uint32_t timestamp_last;

	uint16_t hr;
	uint16_t cadence;
	float speed=0.0;

	uint32_t start_distance[ESummaryTypeMax];   // start_distance: For locally stored distances, this is the distance the counter was reset. For extern stored (FL) distance this is the actual distance
	uint32_t distance;							// current distance counter

	Ticker statCycle;
	Ticker statStore;
	Preferences StatPreferences[SUM_ESP_START];


	void cycle();			// 500ms ticker
	void autoStore();		// 5s ticker
	void restoreStats();
	void setCurDriveState(EDrivingState curDriveState);


public:
	Statistics();
	void setup();
	void addSpeed(float speed);  // in 0,1km/h
	void addDistance(uint16_t dist, ESummaryType type = SUM_ESP_TOUR);
	void reset(ESummaryType type);
	void updateDistance(uint16_t dist);
	void addHR(uint16_t heartrate);
	void addCadence(uint16_t cadence);
	void setConnected(bool connected);

	float getAvg(ESummaryType type, EAvgType avgtype) const {
		return 0.0;
		//return dist_total[type] / (10.0 * speed_avg_count);
	};

	uint16_t getHr() const {return hr;}
	//uint16_t getSpeed() {speed += (rand() % 21) - 10;return speed;}  // FIXME: make const again when simulation is removed
	uint16_t getSpeed() {return speed;}  // FIXME: make const again when simulation is removed

	const float getSpeedMax(ESummaryType type) const {
		return speed_max[type];
	}

	uint32_t getDistance(ESummaryType type) const;

	static const char* PREF_TIME_STRING[Statistics::EDrivingStateMax];
	static const char* SUM_TYPE_STRING[Statistics::ESummaryTypeMax];

};

