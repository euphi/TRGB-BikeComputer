/*
 * UIFacade.h
 *
 *  Created on: 03.03.2023
 *      Author: ian
 */

#pragma once

#include <Arduino.h>
#include <Ticker.h>

class UIFacade {
public:
	UIFacade();

	void initDisplay();
	void updateHandler();

private:
	void updateFLPower(uint16_t batVoltage, uint8_t batPerc, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn);
	void updateData();
	Ticker updateTicker;

};
