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

#include <lvgl.h>


void ui_ScrNaviUpdateSpeed(float speed);
void ui_ScrNaviUpdateCadence(int16_t cadence);
void ui_ScrNaviUpdateHR(int16_t hr);
void ui_ScrNaviUpdateNav(const char* navStr, uint32_t dist, uint8_t dirCode);
void ui_ScrNaviUpdateNavDist(uint32_t dist);
void ui_ScrNaviSetBackScreen( lv_obj_t* const screenBack);
void ui_ScrNaviGoBack(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
