/*
 * uiFLmodel.h
 *
 *  Created on: 09.03.2023
 *      Author: ian
 */

#pragma once

#include <Arduino.h>
#include "FLClassicParser.h"


class UiFLModel {
public:
	UiFLModel();

	// Methods for UIFacade
	void init();
	bool redraw();

	void updateClock(const String& clock, const String& date);

	// Callback-Methods for FLClassicParser
	void updateFLPower(uint16_t batVoltage, uint8_t batPerc, uint16_t _batFullCap, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn);
	void updateFLState(FLClassicParser::EFLConnState cstate, uint32_t flags, int16_t timeout);

private:

	bool updatePower = false;
	uint16_t batVoltage, batFullCap;
	uint8_t batPerc;
	int8_t powerStage;
	int16_t CurBat;
	int16_t CurConsumer;
	bool ConsumerOn;

	bool updateState = false;
	FLClassicParser::EFLConnState cstate;
	uint32_t flags;
	int16_t timeout;

};

