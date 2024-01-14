/*
 * UIFacade.h
 *
 *  Created on: 03.03.2023
 *      Author: ian
 */

#pragma once

#include <Arduino.h>
#include <Ticker.h>
#include "freertos/semphr.h"

#include "ui/uiFLmodel.h"
#include <Statistics.h>

class UIFacade {
public:
	UIFacade();

	void initDisplay();
	void updateHandler();

	void updateSpeed(float speed);		// speed in km/h as float
	void updateCadence(uint16_t cad);	// cadence in revs per min
	void updateHR(uint16_t hr);       	// heartbet in beats per min
	void updateGrad(float grad, float height);	// grad(ient) in permille,

	void setStatMode(Statistics::ESummaryType mode) {statMode = mode;}
	void setStatMode(bool dir) {
		int32_t mode = statMode;
		mode += dir ? 1 : -1;
		if (mode < Statistics::SUM_ESP_TOTAL) mode = Statistics::SUM_ESP_START;
		if (mode > Statistics::SUM_ESP_START) mode = Statistics::SUM_ESP_TOTAL;
		statMode = static_cast<Statistics::ESummaryType>(mode);
		updateData();
	}
	void updateFast() {xSemaphoreGive(xUpdateFast);}

	void updateIP(const String& ipStr);
	void updateNavi(const String& navStr, uint32_t dist, uint8_t dirCode);
	void updateNaviDist(uint32_t dist);

	void updateBatInt(float voltage, uint8_t batPerc, bool charging);

	float getBatIntVoltageAvg() const {return batIntVoltageAvg;}

	Statistics::ESummaryType getStatMode() const {return statMode;}

	Statistics::EAvgType getStatTimeMode() const {return statTimeMode;}
	void setStatTimeMode(Statistics::EAvgType _statTimeMode) {statTimeMode = _statTimeMode;updateData();}


private:
	void updateData();
	void updateClock(const time_t now);
	void updateStats();
	void updateIntBatteryInt();


	UiFLModel uifl;

	Ticker updateTicker;
	Ticker dataTicker;

	//TODO: Use notify/poll mechanism instead of storing data
	int16_t hr = -1, cad = -1;
	float grad, height, speed = NAN;

	Statistics::ESummaryType statMode = Statistics::SUM_ESP_START;
	Statistics::EAvgType     statTimeMode = Statistics::AVG_ALL;

	// Model for TRGB specific data
	float batIntVoltage, batIntVoltageAvg = NAN;
	int8_t batIntPerc = -1;
	bool batIntCharging = false;

	// Use (binary) Semaphores as messaging mechanism
	//    give: Signal an information (please update)
	//    take: receive signal
	SemaphoreHandle_t xUpdateFast;
	SemaphoreHandle_t xUpdateSlow;
	SemaphoreHandle_t xUIDrawMutex;
	TaskHandle_t uiTaskHandle = NULL;

};
