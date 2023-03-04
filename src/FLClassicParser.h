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
	typedef std::function<void(float speed, float distance)> FLSpeedDistUpdateHandler;
	typedef std::function<void(uint16_t batVoltage, uint8_t batPerc, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn)> FLBatUpdateHandler;


	typedef std::function<void(uint16_t temperature, uint16_t pressure, uint16_t height, uint16_t gradient)> FLEnvUpdateHandler;

	typedef std::function<void(EFLConnState cstate, uint32_t flags, int16_t timeout)> FLStateUpdateHandler;

	//Methods
	FLClassicParser(uint8_t polePair, uint16_t circum_mm);

	void setConnState(EFLConnState state);
	void updateFromString(const String& flStr);

	void setBatCb(const FLBatUpdateHandler &batCb) {batCB = batCb;}
	void setEnvCb(const FLEnvUpdateHandler &envCb) {envCB = envCb;}
	void setSpeedCb(const FLSpeedDistUpdateHandler &speedCb) {speedCB = speedCb;}
	void setStateCb(const FLStateUpdateHandler &stateCb) {stateCB = stateCb;}

private:
	// Fields
	EFLConnState cstate = FL_STATE_UNKNOWN;

	const float dist_m_per_pulse, kmh_per_pulse_per_s;

	FLSpeedDistUpdateHandler speedCB = nullptr;
	FLBatUpdateHandler batCB = nullptr;
	FLEnvUpdateHandler envCB = nullptr;
	FLStateUpdateHandler stateCB = nullptr;

	unsigned long lastUpdate;
	uint32_t flags_last = 0;
	uint8_t batt_perc=0;


	// Methods


//	static constexpr float dist_per_pulse = 2.155 / 14; // Umfang / Polpaare
//	static constexpr float hmh_per_pulse = 2.155 / 14 * 3600 * 0.001 * 10; // Umfang / Polpaare * 3600 sec/hour * 0.001 m/km * 10 (100m/km) --> [0,1 km/h]
//	static constexpr float m_per_pulse = 2.155 / 14 / 1000; // Umfang / Polpaare / 1000mm/m --> [m]


};

