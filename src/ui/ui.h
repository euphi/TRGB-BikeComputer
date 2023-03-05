// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: SquareLine_Project

#ifndef _SQUARELINE_PROJECT_UI_H
#define _SQUARELINE_PROJECT_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

extern lv_obj_t * ui_S1Main;
extern lv_obj_t * ui_S1LabelTitle;
void ui_event_S1RollerStatMode(lv_event_t * e);
extern lv_obj_t * ui_S1RollerStatMode;
extern lv_obj_t * ui_S1PanelIcons;
void ui_event_S1ImgIconBTClassic(lv_event_t * e);
extern lv_obj_t * ui_S1ImgIconBTClassic;
void ui_event_S1ImgIconBLEhrm(lv_event_t * e);
extern lv_obj_t * ui_S1ImgIconBLEhrm;
void ui_event_S1ImgIconWifi(lv_event_t * e);
extern lv_obj_t * ui_S1ImgIconWifi;
extern lv_obj_t * ui_S1ArcCadence;
extern lv_obj_t * ui_S1ArcAvg;
extern lv_obj_t * ui_S1ArcSpeed;
extern lv_obj_t * ui_S1BarPuls;
extern lv_obj_t * ui_S1BarPulsLabel;
extern lv_obj_t * ui_S1LabelSpeed;
void ui_event_S1PanelStat(lv_event_t * e);
extern lv_obj_t * ui_S1PanelStat;
extern lv_obj_t * ui_S1PStatLTitle;
extern lv_obj_t * ui_S1PStatLAvg;
extern lv_obj_t * ui_S1PStatLAvgVar;
extern lv_obj_t * ui_S1PStatImgAvgUnit;
extern lv_obj_t * ui_S1PStatLGradientVar;
extern lv_obj_t * ui_S1PStatLGradient;
extern lv_obj_t * ui_S1PStatLspdMax;
extern lv_obj_t * ui_S1PStatLspdMaxVar;
extern lv_obj_t * ui_S1PStatImgSpdMaxUnit;
extern lv_obj_t * ui_S1PStatLDist;
extern lv_obj_t * ui_S1PStatLDistVar;
extern lv_obj_t * ui_S1PStatLTime;
extern lv_obj_t * ui_S1PStatLTimeVar;
extern lv_obj_t * ui_S1BarPowerMode;
void ui_event_S1BarBatt(lv_event_t * e);
extern lv_obj_t * ui_S1BarBatt;
extern lv_obj_t * ui_S1BarBattLabel;
extern lv_obj_t * ui_S1LabelClock;
extern lv_obj_t * ui_Label2;
void ui_event_ScreenChart(lv_event_t * e);
extern lv_obj_t * ui_ScreenChart;
extern lv_obj_t * ui_ScreenSettings_Panel2;
extern lv_obj_t * ui_ScreenChart_Chart1;

void chartModeHeartRate(lv_event_t * e);
void chartModeBatterie(lv_event_t * e);

LV_IMG_DECLARE(ui_img_bt_png);    // assets/bt.png
LV_IMG_DECLARE(ui_img_heartrate_png);    // assets/heartrate.png
LV_IMG_DECLARE(ui_img_wlan_png);    // assets/wlan.png
LV_IMG_DECLARE(ui_img_kmh_xs_png);    // assets/kmh_XS.png

LV_FONT_DECLARE(ui_font_by75_96);

void ui_S1Main_screen_init();
void ui_ScreenWifi_screen_init();
void ui_ScreenChart_screen_init();

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
