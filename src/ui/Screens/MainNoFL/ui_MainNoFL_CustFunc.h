/*
 * ui_MainNoFL_CustFunc.h
 *
 *  Created on: 17.06.2023
 *      Author: ian
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ui_SMainNoFLUpdateSpeed(float speed);
void ui_SMainNoFLUpdateCadence(int16_t cadence);
void ui_SMainNoFLUpdateHR(int16_t hr);
void ui_SMainNoFLUpdateNav(const char* navStr, uint32_t dist, uint8_t dirCode);
void ui_SMainNoFLUpdateNavDist(uint32_t dist);
void ui_SMainNoFLUpdateGrad(float grad, float height);

void ui_SMainNoFLUpdateIntBatPerc(uint8_t perc);

void ui_SMainNoFLUpdateStats(const char* typeStr, const char* avgStr, float speedAvg, float speedMax,  float temperature, uint32_t dist, uint32_t time);


void ui_SMainNoFLUpdateClock(uint32_t dist);


#ifdef __cplusplus
} /*extern "C"*/
#endif
