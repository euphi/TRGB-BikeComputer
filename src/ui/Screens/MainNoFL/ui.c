// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.2.2
// LVGL version: 8.3.4
// Project name: BC_noFL_Main

#include <ui/Screens/MainNoFL/ui.h>
#include "ui/ui_WLAN.h"
#include "ui/Screens/Settings/ui_Settings.h"

#include "ui/ui_Navi.h"
#include "ui/Screens/Chart/ui.h"

#include "ui/ui_helpers.h"

///////////////////// VARIABLES ////////////////////
lv_obj_t * ui_SMainNoFL;
void ui_event_PanelNav(lv_event_t * e);
lv_obj_t * ui_PanelNav;
lv_obj_t * ui_ImgNav;
lv_obj_t * ui_BarNav;
lv_obj_t * ui_LabelNavDist;
lv_obj_t * ui_ArcSpeed;
lv_obj_t * ui_ArcAvg;
lv_obj_t * ui_ArcCad;
lv_obj_t * ui_LabelSpeed;
lv_obj_t * ui_BarHR;
lv_obj_t * ui_LabelHR;
lv_obj_t * ui_LabelDist;
void ui_event_PanelClock(lv_event_t * e);
lv_obj_t * ui_PanelClock;
lv_obj_t * ui_LabelClock;
lv_obj_t * ui_LabelClockMode;
lv_obj_t * ui_LabelCad;
lv_obj_t * ui_BarBatt;
void ui_event_ImgWifi(lv_event_t * e);
lv_obj_t * ui_ImgWifi;
void ui_event_ImgSettings(lv_event_t * e);
lv_obj_t * ui_ImgSettings;
lv_obj_t * ui_LabelSpdAvg;
lv_obj_t * ui_contHeight;
lv_obj_t * ui_LabelHeight;
lv_obj_t * ui_LabelGradient;
lv_obj_t * ui_LabelTemp;
void ui_event_ImgState(lv_event_t * e);
lv_obj_t * ui_ImgState;
lv_obj_t * ui____initial_actions0;


///////////////////// TEST LVGL SETTINGS ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////
void ui_event_PanelNav(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
        _ui_screen_change(ui_SNavi, LV_SCR_LOAD_ANIM_MOVE_TOP, 500, 0);
    }
}
void ui_event_PanelClock(lv_event_t * e)
{
	lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);

    if(event_code == LV_EVENT_LONG_PRESSED) {
        resetStats(e);
    }
    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_TOP) {
        statsTimeMode(true);
    }
    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_BOTTOM) {
        statsTimeMode(false);
    }

    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
    	statModeNext(true);
    }
    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_LEFT) {
    	statModeNext(false);
    }
}
void ui_event_ImgWifi(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
        _ui_screen_change(ui_SWLAN, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0);
    }
}
void ui_event_ImgSettings(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
        _ui_screen_change(ui_ScrSettings, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0);
    }
}

void ui_event_ImgState(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
        _ui_screen_change(ui_SChart, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0);
    }
}
