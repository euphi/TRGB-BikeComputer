// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: BC_ScreenWLAN

#ifndef _BC_SCREENWLAN_UI_H
#define _BC_SCREENWLAN_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void ui_event_SWLAN(lv_event_t * e);
extern lv_obj_t * ui_SWLAN;
extern lv_obj_t * ui_SWLANDropAP;
void ui_event_SWLANButConn(lv_event_t * e);
extern lv_obj_t * ui_SWLANButConn;
extern lv_obj_t * ui_SWLANButtCBLabel;
void ui_event_SWLANCBMode(lv_event_t * e);
extern lv_obj_t * ui_SWLANCBMode;
extern lv_obj_t * ui_SWLANLabelIP;
extern lv_obj_t * ui_SWLANPanel1;
void ui_event_SWLANTextPW(lv_event_t * e);
extern lv_obj_t * ui_SWLANTextPW;
void ui_event_SWLANLabelPW(lv_event_t * e);
extern lv_obj_t * ui_SWLANLabelPW;
extern lv_obj_t * ui_SWLANPanelPlacelholderQR;
extern lv_obj_t * ui_SWLANKeyB;

void WLANModeToggle(lv_event_t * e);
void changePwStyle(lv_event_t * e);

void ui_SWLAN_screen_init();
void ui_SWLAN_extra_init();
void ui_SWLANUpdateIP(const char * ipStr);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
