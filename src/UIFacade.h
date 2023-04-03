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
	void setStatMode(Statistics::ESummaryType mode) {statMode = mode;}


	void updateFast() {xSemaphoreGive(xUpdateFast);}

	void updateIP(const String& ipStr);
	void updateNavi(const String& navStr, uint32_t dist, uint8_t dirCode);
	void updateBatInt(float voltage, uint8_t batPerc, bool charging);

	float getBatIntVoltageAvg() const {return batIntVoltageAvg;}

private:
	void updateData();
	void updateClock(const time_t now);
	void updateStats();
	void updateIntBatteryInt();


	UiFLModel uifl;

	Ticker updateTicker;
	Ticker dataTicker;

	//TODO: Use notify/poll mechanism instead of storing data
	int16_t hr, cad;
	float speed;

	Statistics::ESummaryType statMode = Statistics::SUM_ESP_START;

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
