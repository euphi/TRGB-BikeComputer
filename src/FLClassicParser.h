/*
 * FLClassicParser.h
 *
 *  Created on: 19.02.2023
 *      Author: ian
 */


#pragma once
#include <functional>
#include <Arduino.h>
#include "FLClassicFlags.h"

class FLClassicParser {
public:
	typedef enum {
		FL_STATE_UNKNOWN = -1,
		FL_STATE_INIT = 0,
		FL_STATE_LOST,
		FL_STATE_CONNECTING,
		FL_STATE_CONNECTED
	} EFLConnState;

	//Types
	typedef std::function<void(uint16_t batVoltage, uint8_t batPerc, uint32_t batFullCap, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn)> FLBatUpdateHandler;
	typedef std::function<void(uint16_t temperature, uint16_t pressure, uint16_t height, uint16_t gradient)> FLEnvUpdateHandler;
	typedef std::function<void(EFLConnState cstate, uint32_t flags, int16_t timeout)> FLStateUpdateHandler;

	//Methods
	FLClassicParser(uint8_t polePair, uint16_t circum_mm);

	void setConnState(EFLConnState state);
	void updateFromString(const String& flStr);

	void setBatCb(const FLBatUpdateHandler &batCb) {batCB = batCb;}
	void setStateCb(const FLStateUpdateHandler &stateCb) {stateCB = stateCb;}
	//FIXME: FL: void setEnvCb(const FLEnvUpdateHandler &envCb) {envCB = envCb;}

private:
	// Fields
	EFLConnState cstate = FL_STATE_UNKNOWN;

	FLBatUpdateHandler batCB = nullptr;
	//FLEnvUpdateHandler envCB = nullptr;
	FLStateUpdateHandler stateCB = nullptr;

	const uint8_t polePairs = 14;

	unsigned long lastUpdate;
	uint32_t flags_last = 0;
	uint8_t batt_perc=0;
	uint16_t batt_fullCap = 0;
};

