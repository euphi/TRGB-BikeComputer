/*
 * ui_custFunc.h
 *
 *  Created on: 05.03.2023
 *      Author: ian
 */


#pragma once
#ifdef __cplusplus
extern "C" {
#endif

void ui_ScrMainUpdatePower(uint16_t batVoltage, uint8_t batPerc, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn);
void ui_ScrMainUpdateClock(const char* clockStr, const char* dateStr);

void ui_ScrMainUpdateFast(float speed, float grad);
void ui_ScrMainUpdateStats(const char* modeStr, float avgSpd, float maxSpd, uint32_t dist, uint32_t timeInS);
void ui_ScrMainUpdateTimeMode(const char* tmStr);


void ui_ScrMainUpdateCadence(uint16_t cadence);
void ui_ScrMainUpdateHR(uint16_t hr);

void ui_ScrChartUpdateSpeed(float speed);
float ui_ScrChartUpdateBat(float batVolt, int8_t perc, char* batString);

void ui_MsgBox(const char* str);
void ui_MsgBoxUpdate(const char* str);

#ifdef __cplusplus
} /*extern "C"*/
#endif
