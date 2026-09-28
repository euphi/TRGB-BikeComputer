/*
 * Distance.h
 *
 *  Created on: 21.01.2024
 *      Author: ian
 */

#pragma once

//#include <Arduino.h>
#include <Preferences.h>
#include <global_settings.h>
#include <Ticker.h>
#include <Stats/Statistics.h>

class Distance {
public:
	Distance();
	void setup();
	void store();

	void updateRevs(uint32_t revs, uint16_t timestamp);
#ifdef BC_FL_SUPPORT
	void updateRevsFL(uint32_t revs, float pulses_per_s);
#endif
	bool updateWheelCirc(const float circ_in_m); // return true on success
	void resetDistToZero(Statistics::ESummaryType);

	float revsToDistance(uint32_t revs) {return revs * wheel_c;}
	const float getDistance(Statistics::ESummaryType t = Statistics::SUM_ESP_START, bool includeLost = false) const {
		return ( curTotalDistance[t] - ( includeLost ? 0 : lostDistanceFromNVS[t]) );
	}

private:
	float wheel_c = NAN;											// wheel_c as loaded from NVS
	// Base distance at revsFromNVS[] -- loaded from NVS for TOTAL, TOUR, TRIP; for START (since
	// power-on, never stored) it starts at 0 and only moves when the sensor counter is rebased.
	float distanceFromNVS[Statistics::SUM_ESP_START + 1] = {0};
	float lostDistanceFromNVS[Statistics::SUM_ESP_START + 1] = {0};	// part of the distance ridden without the BC connected (no time for it)
	uint32_t revsFromNVS[Statistics::SUM_ESP_START + 1] = {0};	// sensor revs at distanceFromNVS[]
	bool revsKnown[Statistics::SUM_ESP_START + 1] = {false};	// revsFromNVS[] is a real sensor value (stored in NVS or received), not just "nothing stored yet"

	float curTotalDistance[Statistics::SUM_ESP_START + 1] = {NAN};	// current, actual distance for TOTAL, TOUR, TRIP (stored distance + (current revs - stored revs) * wheel_c)

	// Internal data to manage updates
	uint32_t lastRevs = 0;
	bool revsInitialized = false;	// lastRevs holds a received value -- 0 is a legal counter value for sensors that restart at 0
	uint32_t last_speedUpdate = 0;
	uint16_t lastTimestamp = 0;
	uint8_t currentBikeIdx=0;

	Ticker distanceStore;

	float calculateSpeed(const uint32_t revs, const uint16_t duration);
	void loadDistanceForBikeIdx(uint8_t idx);

	void storeDistanceAndResetRevs(bool resetRevs=false);		// stores current distance and reset rev count (needed for change of wheel circumference)
	void updateLostRevs(const uint32_t lostRevs);
	void rebaseCounterRestart(bool asLost, uint32_t revs);

	void setupWebserver();
};
