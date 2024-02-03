
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>
#include "ui.h"

extern lv_obj_t* ui_SChart_SBack;

void ui_ScrChartSetBackScreen( lv_obj_t* const screenBack);

void ui_ScrChartSetExtArray1(int16_t array[], uint8_t index);
void ui_ScrChartSetPostFirst(uint16_t pos, uint8_t idx);

void ui_ScrChartRefresh();

#ifdef __cplusplus
} /*extern "C"*/
#endif
