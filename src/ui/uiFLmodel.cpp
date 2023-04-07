/*
 * uiFLmodel.cpp
 *
 *  Created on: 09.03.2023
 *      Author: ian
 */

#include <ui/uiFLmodel.h>

#include "Singletons.h"

#include "ui/ui_FL.h"

UiFLModel::UiFLModel() {

}

void UiFLModel::init() {
	ui_ScreenFL_screen_init();

	// Glue oberserver/listeners to data model
	flparser.setBatCb([this](uint16_t batVoltage, uint8_t batPerc, uint16_t batFullCap, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn) {
		updateFLPower(batVoltage, batPerc, batFullCap, powerStage, CurBat, CurConsumer, ConsumerOn);
	});
	flparser.setStateCb([this](FLClassicParser::EFLConnState cstate, uint32_t flag, int16_t timeout) {
		updateFLState(cstate, flag, timeout);
	});
}

bool UiFLModel::redraw() {
	bool updateDone = false;
	if (updateState) {
		ui_ScrFLUpdateFlags(flags);
		updateState = false;
		updateDone = true;
	}
	if (updatePower) {
		ui_ScrFLUpdatePower(batVoltage, batPerc, batFullCap, powerStage, CurBat, CurConsumer, ConsumerOn);
		//ui_ScrMainUpdatePower(batVoltage, batPerc, powerStage, CurBat, CurConsumer, ConsumerOn);
		updatePower = false;updateDone = true;
	}
	return updateDone;
}

void UiFLModel::updateClock(const String &clock, const String &date) {
	ui_ScrFLUpdateClock(clock.c_str(), date.c_str());
}

void UiFLModel::updateFLState(FLClassicParser::EFLConnState _cstate, uint32_t _flags, int16_t _timeout) {
	cstate = _cstate;
	flags = _flags;
	timeout = _timeout;
	updateState = true;
}

void UiFLModel::updateFLPower(uint16_t _batVoltage, uint8_t _batPerc, uint16_t _batFullCap, int8_t _powerStage, int16_t _CurBat, int16_t _CurConsumer, bool _ConsumerOn) {
	batVoltage = _batVoltage;
	batPerc = _batPerc;
	batFullCap = _batFullCap;
	powerStage = _powerStage;
	CurBat = _CurBat;
	CurConsumer = _CurConsumer;
	ConsumerOn = _ConsumerOn;

	updatePower = true;
}

