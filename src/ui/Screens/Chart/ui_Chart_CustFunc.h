
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>
#include "ui.h"

extern lv_obj_t* ui_SChart_SBack;

void ui_ScrChartSetBackScreen( lv_obj_t* const screenBack);

void ui_ScrChartSetExtArray1(uint16_t array[], uint8_t index, uint16_t size);


#ifdef __cplusplus
} /*extern "C"*/
#endif
