// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.4
// LVGL version: 8.3.6
// Project name: ScreenChart

#include "ui.h"
#include "ui_Chart_CustFunc.h"
#include "ui/ui_helpers.h"

///////////////////// VARIABLES ////////////////////

// SCREEN: ui_SChart
void ui_SChart_screen_init(void);
void ui_event_SChart(lv_event_t * e);
lv_obj_t * ui_SChart;
lv_obj_t * ui_Chart1;
lv_chart_series_t * ui_Chart1_series[4];
lv_obj_t * ui_LabelSpeed;
lv_obj_t * ui_PanelMode;
void ui_event_ButtonGrad(lv_event_t * e);
lv_obj_t * ui_ButtonGrad;
lv_obj_t * ui_LButtonGrad;
void ui_event_ButtonPuls(lv_event_t * e);
lv_obj_t * ui_ButtonPuls;
lv_obj_t * ui_LButtonPuls;
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
void ui_event_SChart(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        lv_indev_wait_release(lv_indev_get_act());
        _ui_screen_change(ui_SChart_SBack, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0);
    }
}
void ui_event_ButtonGrad(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
        EvButton1(e);
    }
}
void ui_event_ButtonPuls(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
        EvButton2(e);
    }
}

///////////////////// SCREENS ////////////////////
