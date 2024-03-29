// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.4
// LVGL version: 8.3.6
// Project name: ScreenChart

#ifndef _SCREENCHART_UI_H
#define _SCREENCHART_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

#include "ui/ui_helpers.h"
#include "ui_events.h"
// SCREEN: ui_SChart
void ui_SChart_screen_init(void);
void ui_event_SChart(lv_event_t * e);
extern lv_obj_t * ui_SChart;
extern lv_obj_t * ui_Chart1;
extern lv_chart_series_t * ui_Chart1_series[4];
extern lv_obj_t * ui_LabelSpeed;
extern lv_obj_t * ui_PanelMode;
void ui_event_ButtonGrad(lv_event_t * e);
extern lv_obj_t * ui_ButtonGrad;
extern lv_obj_t * ui_LButtonGrad;
void ui_event_ButtonPuls(lv_event_t * e);
extern lv_obj_t * ui_ButtonPuls;
extern lv_obj_t * ui_LButtonPuls;

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
