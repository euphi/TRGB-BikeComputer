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
		EAvgTypeMax
	};
	enum EDrivingState {	// Various driving states
		DS_NO_CONN,
		DS_BREAK,
		DS_STOP,
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
		S_distanceData data[480];				// per 100m -> 48km
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
		S_timeData data[400];
		S_timeData currentMinMax;
		uint16_t index = 0;
		uint8_t curCountSpeed;
		uint8_t curCountCadence;
		uint8_t curCountHr;
	};

	S_timeComplete timeData;

	// Variables to calculate gradient (Forumslader calculates gradient on its own)
#ifndef BC_FL_SUPPORT
	time_t gradient_timestamp = 0;		//Timestamp of last gradient calculation
	float gradient_dist = 0;			//Distance of last gradient calculation
	float gradient_height = NAN;		//Height of last gradient calculation
#endif
	Distance& distHandler;				// also the distance handler is only used without Forumslader because FL calculates distance (partly) on its own.

	float gradient = 0.0;				//calculated gradient
	float height   = NAN;
	float tempC    = NAN;

	jnk0le::Ringbuffer<S_timeData, 256> rb_timedata;


	// use int instead of uint, so -1 can be used as "invalid".
	int16_t hr = -1;
	int16_t cadence = 0, cadence_tot = -1;
	float speed=0.0;

	uint8_t offAfterMinutes = 5;

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
	void addSpeed(float speed);  // in 0,1km/h
	//void addDistance(uint32_t dist, ESummaryType type = SUM_ESP_TOUR);
	bool isConnected() {return (curDriveState != DS_NO_CONN);}
	void delayStandby();
	void toggleStandbyMode();
private:
	void updateStateIcon();
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


	uint32_t getTime(ESummaryType type, EAvgType avgtype) const;

	float getAvg(ESummaryType type, EAvgType avgtype) const;
	float getAvgCadence(EAvgType avgtype) const;
	int16_t getHr() const {return hr;}
	const float getSpeedMax(ESummaryType type) const {return speed_max[type];}
	uint32_t getDistance(ESummaryType type, bool includeLost = true) const;
	float getTemp() const {return tempC;}

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

	void createChartArray(uint8_t idx);

	Distance& getDistHandler() {return distHandler;}

	EDrivingState getCurDriveState() const {return curDriveState;}
};

