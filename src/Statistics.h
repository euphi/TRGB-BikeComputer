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
	enum EAvgType {
		AVG_ALL,
		AVG_DRIVE,
		AVG_NOBREAK,   // but Pause/Stop
		EAvgTypeMax
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

	struct S_distanceComplete {		// >20856 Byte
		uint32_t startDistance = 0;
		S_distanceData distanceData[480];				// per 100m -> 48km
		S_distanceData currentMinMax;
		uint32_t curDistance = 0;
		uint16_t index = 0;
		uint8_t curCountSpeed;
		uint8_t curCountCadence;
		uint8_t curCountHr;
		uint8_t curCountGradHeight;

	};

	S_distanceComplete distanceData;


	// Data storage per time
	struct S_timeData {	// 48 Byte
		uint32_t distance;		//   4 Byte
		S_DataPoint speed;
		S_DataPoint cadence;
		S_DataPoint hr;
		uint8_t coastShare:8;
		uint8_t pauseShare:8;

	};

	struct S_timeComplete {		// 19252 Byte
		time_t startTime;
		S_timeData distanceData[400];
		S_timeData currentMinMax;
		uint16_t index = 0;
		uint8_t curCountSpeed;
		uint8_t curCountCadence;
		uint8_t curCountHr;
	};

	S_timeComplete timeData;

	// Variables to calculate gradient (Forumslader calculates gradient on its own)
#if !BC_FL_SUPPORT
	time_t gradient_timestamp = 0;		//Timestamp of last gradient calculation
	uint32_t gradient_revs = 0;			//Distance of last gradient calculation
	float gradient_height = NAN;		//Height of last gradient calculation
#endif

	float gradient = 0.0;				//calculated gradient
	float height = NAN;
	float temperature = NAN;

	// use int instead of uint, so -1 can be used as "invalid".
	int16_t hr = -1;
	int16_t cadence, cadence_tot = -1;
	float speed=0.0;

	//int16_t grad = 0, height = 0;

	uint32_t start_distance[ESummaryTypeMax];   // start_distance: For locally stored distances, this is the distance the counter was reset. For extern stored (FL) distance this is the actual distance
	uint32_t lost_distance[ESummaryTypeMax];   // start_distance: For locally stored distances, this is the distance the counter was reset. For extern stored (FL) distance this is the actual distance
	uint32_t distance;							// current distance counter
	bool distance_start = false;

	Ticker statCycle;
	Ticker statStore;
	Ticker statDataStore;
	Preferences StatPreferences[SUM_ESP_START + 1]; // FL values are not stored (because they are stored in FL). SUM_ESP_START value stores last know value

	void cycle();			// 500ms ticker
	void autoStore();		// 5s ticker
	void dataStore();		// 5s ticker

	void restoreStats();
	void setCurDriveState(EDrivingState curDriveState);

	void updateLostDistance(uint32_t _dist_lost);
	void updateDistanceSeries();
	void updateTimeSeries();
	void calculateGradient(uint32_t _revs);


public:
	Statistics();
	void setup();
	void addSpeed(float speed);  // in 0,1km/h
	void addDistance(uint32_t dist, ESummaryType type = SUM_ESP_TOUR);
	void reset(ESummaryType type);
	void updateDistance(uint32_t dist, uint32_t revs);
	void addHR(int16_t heartrate);
	void addCadence(int16_t cadence, int16_t total);
	void setConnected(bool connected);
	void addGradientFL(int16_t grad, int16_t height, int16_t temp);
	void addGradientHeight(float _grad, float _height);

	uint32_t getTime(ESummaryType type, EAvgType avgtype) const;

	float getAvg(ESummaryType type, EAvgType avgtype) const;
	float getAvgCadence(EAvgType avgtype) const;
	int16_t getHr() const {return hr;}
	const float getSpeedMax(ESummaryType type) const {
		return speed_max[type];
	}
	float getTemperature() const {return temperature;}
	uint32_t getDistance(ESummaryType type, bool includeLost = true) const;

	static const char* PREF_TIME_STRING[Statistics::EDrivingStateMax];
	static const char* AVG_TYPE_STRING[Statistics::EAvgTypeMax];
	static const char* SUM_TYPE_STRING[Statistics::ESummaryTypeMax];

	static EAvgType getNextTimeMode(EAvgType type, bool dir) {
		int32_t rc = static_cast<int32_t>(type);
		rc += dir ? 1:-1;
		if (rc < Statistics::AVG_ALL) rc = Statistics::AVG_NOBREAK;
		if (rc >= Statistics::EAvgTypeMax) rc = Statistics::AVG_ALL;
		return static_cast<EAvgType>(rc);

	}

};

