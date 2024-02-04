/*
 * Distance.h
 *
 *  Created on: 21.01.2024
 *      Author: ian
 */

#pragma once

#include <Arduino.h>
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
	bool updateWheelCirc(float circ_in_m); // return true on success
	void resetDistToZero(Statistics::ESummaryType);

	float revsToDistance(uint32_t revs) {return revs * wheel_c;}
	const float getDistance(Statistics::ESummaryType t = Statistics::SUM_ESP_START) const {return curTotalDistance[t];}

private:
	float wheel_c = NAN;											// wheel_c as loaded from NVS
	float distanceFromNVS[Statistics::SUM_ESP_TRIP + 1] = {NAN};		// total distance as loaded from NVS
	uint32_t revsFromNVS[Statistics::SUM_ESP_START + 1] = {0};	// total revs as loaded from NVS

	float curTotalDistance[Statistics::SUM_ESP_START + 1] = {NAN};	// current, actual distance for TOTAL, TOUR, TRIP (stored distance + (current revs - stored revs) * wheel_c)

	// Internal data to manage updates
	uint32_t lastRevs = 0;
	uint32_t last_speedUpdate = 0;
	uint16_t lastTimestamp = 0;
	uint8_t currentBikeIdx=0;

	Ticker distanceStore;

	float calculateSpeed(uint32_t revs, uint16_t duration);
	void loadDistanceForBikeIdx(uint8_t idx);

	void storeDistanceAndResetRevs(bool resetRevs=false);		// stores current distance and reset rev count (needed for change of wheel circumference)

	void setupWebserver();
};
