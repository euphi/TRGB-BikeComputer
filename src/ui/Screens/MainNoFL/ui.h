// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.2.2
// LVGL version: 8.3.4
// Project name: BC_noFL_Main

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>

#include "ui/img/img.h"
#include "ui/font/font.h"


void ui_SMainNoFL_screen_init();

extern lv_obj_t * ui_SMainNoFL;
void ui_event_PanelNav(lv_event_t * e);
extern lv_obj_t * ui_PanelNav;
extern lv_obj_t * ui_ImgNav;
extern lv_obj_t * ui_BarNav;
extern lv_obj_t * ui_LabelNavDist;
extern lv_obj_t * ui_ArcSpeed;
extern lv_obj_t * ui_ArcAvg;
extern lv_obj_t * ui_ArcCad;
extern lv_obj_t * ui_LabelSpeed;
void ui_event_BarHR(lv_event_t * e);
extern lv_obj_t * ui_BarHR;
extern lv_obj_t * ui_LabelHR;
extern lv_obj_t * ui_LabelDist;
void ui_event_PanelClock(lv_event_t * e);
extern lv_obj_t * ui_PanelClock;
extern lv_obj_t * ui_LabelClock;
extern lv_obj_t * ui_LabelClockMode;
extern lv_obj_t * ui_LabelCad;
extern lv_obj_t * ui_BarBatt;
void ui_event_ImgWifi(lv_event_t * e);
extern lv_obj_t * ui_ImgWifi;
void ui_event_ImgSettings(lv_event_t * e);
extern lv_obj_t * ui_ImgSettings;
extern lv_obj_t * ui_LabelSpdAvg;
extern lv_obj_t * ui_contHeight;
extern lv_obj_t * ui_LabelHeight;
extern lv_obj_t * ui_LabelGradient;
extern lv_obj_t * ui_LabelTemp;
void ui_event_ImgState(lv_event_t * e);
extern lv_obj_t * ui_ImgState;

#ifdef __cplusplus
} /*extern "C"*/
#endif
