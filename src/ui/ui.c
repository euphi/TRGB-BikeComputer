// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.2.0
// PROJECT: SquareLine_Project

#include "ui.h"
#include "ui_FL.h"
#include "ui_WLAN.h"
#include "ui_Settings.h"
#include "ui_helpers.h"

///////////////////// VARIABLES ////////////////////
lv_obj_t * ui_S1Main;
lv_obj_t * ui_S1LabelTitle;
void ui_event_S1RollerStatMode(lv_event_t * e);
lv_obj_t * ui_S1RollerStatMode;
lv_obj_t * ui_S1PanelIcons;
void ui_event_S1ImgIconBTClassic(lv_event_t * e);
lv_obj_t * ui_S1ImgIconBTClassic;
void ui_event_S1ImgIconBLEhrm(lv_event_t * e);
lv_obj_t * ui_S1ImgIconBLEhrm;
void ui_event_S1ImgIconWifi(lv_event_t * e);
lv_obj_t * ui_S1ImgIconWifi;
lv_obj_t * ui_S1ArcCadence;
lv_obj_t * ui_S1ArcAvg;
lv_obj_t * ui_S1ArcSpeed;
lv_obj_t * ui_S1BarPuls;
lv_obj_t * ui_S1BarPulsLabel;
lv_obj_t * ui_S1LabelSpeed;
void ui_event_S1PanelStat(lv_event_t * e);
lv_obj_t * ui_S1PanelStat;
lv_obj_t * ui_S1PStatLTitle;
lv_obj_t * ui_S1PStatLAvg;
lv_obj_t * ui_S1PStatLAvgVar;
lv_obj_t * ui_S1PStatImgAvgUnit;
lv_obj_t * ui_S1PStatLGradientVar;
lv_obj_t * ui_S1PStatLGradient;
lv_obj_t * ui_S1PStatLspdMax;
lv_obj_t * ui_S1PStatLspdMaxVar;
lv_obj_t * ui_S1PStatImgSpdMaxUnit;
lv_obj_t * ui_S1PStatLDist;
lv_obj_t * ui_S1PStatLDistVar;
lv_obj_t * ui_S1PStatLTime;
lv_obj_t * ui_S1PStatLTimeVar;
lv_obj_t * ui_S1BarPowerMode;
void ui_event_S1BarBatt(lv_event_t * e);
lv_obj_t * ui_S1BarBatt;
lv_obj_t * ui_S1BarBattLabel;
void ui_event_S1LabelClock(lv_event_t * e);
lv_obj_t * ui_S1LabelClock;
lv_obj_t * ui_Label2;
void ui_event_ScreenChart(lv_event_t * e);
lv_obj_t * ui_ScreenChart;
lv_obj_t * ui_ScreenSettings_Panel2;
lv_obj_t * ui_ScreenChart_Chart1;

///////////////////// TEST LVGL SETTINGS ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////
void ui_event_S1RollerStatMode(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_VALUE_CHANGED) {
        _ui_arc_increment(ui_S1ArcSpeed, 10);
    }
}
void ui_event_S1ImgIconBTClassic(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
        _ui_screen_change(ui_ScreenFL, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
    }
}
void ui_event_S1ImgIconBLEhrm(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
        _ui_screen_change(ui_ScreenChart, LV_SCR_LOAD_ANIM_OVER_BOTTOM, 500, 0);
        chartModeHeartRate(e);
    }
}
void ui_event_S1ImgIconWifi(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
        _ui_screen_change(ui_SWLAN, LV_SCR_LOAD_ANIM_OVER_BOTTOM, 500, 0);
    }
}
void ui_event_S1PanelStat(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_BOTTOM) {
        _ui_bar_increment(ui_S1BarPowerMode, 1, LV_ANIM_ON);
    }
}
void ui_event_S1BarBatt(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_LONG_PRESSED) {
        chartModeBatterie(e);
        _ui_screen_change(ui_ScreenChart, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
    }
}
void ui_event_S1LabelClock(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_LONG_PRESSED) {
        _ui_screen_change(ui_ScrSettings, LV_SCR_LOAD_ANIM_MOVE_TOP, 500, 0);
    }
}
void ui_event_ScreenChart(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        _ui_screen_change(ui_S1Main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0);
    }
}

///////////////////// SCREENS ////////////////////
void ui_S1Main_screen_init(void)
{
    ui_S1Main = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_S1Main, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_S1LabelTitle = lv_label_create(ui_S1Main);
    lv_obj_set_width(ui_S1LabelTitle, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_S1LabelTitle, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1LabelTitle, 0);
    lv_obj_set_y(ui_S1LabelTitle, -200);
    lv_obj_set_align(ui_S1LabelTitle, LV_ALIGN_CENTER);
    lv_label_set_text(ui_S1LabelTitle, "Overview");
    lv_obj_set_style_text_font(ui_S1LabelTitle, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1RollerStatMode = lv_roller_create(ui_S1Main);
    lv_roller_set_options(ui_S1RollerStatMode, "Auto\nTrip\nTour\nTotal\nFLTour\nFLTrip\nFL-Tot", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_height(ui_S1RollerStatMode, 100);
    lv_obj_set_width(ui_S1RollerStatMode, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_x(ui_S1RollerStatMode, 370);
    lv_obj_set_y(ui_S1RollerStatMode, 200);
    lv_obj_clear_flag(ui_S1RollerStatMode,
                      LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE);    /// Flags
    lv_obj_set_style_text_font(ui_S1RollerStatMode, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PanelIcons = lv_obj_create(ui_S1Main);
    lv_obj_set_width(ui_S1PanelIcons, 72);
    lv_obj_set_height(ui_S1PanelIcons, 216);
    lv_obj_set_x(ui_S1PanelIcons, -161);
    lv_obj_set_y(ui_S1PanelIcons, -3);
    lv_obj_set_align(ui_S1PanelIcons, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_S1PanelIcons, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_S1ImgIconBTClassic = lv_img_create(ui_S1PanelIcons);
    lv_img_set_src(ui_S1ImgIconBTClassic, &ui_img_bt_png);
    lv_obj_set_width(ui_S1ImgIconBTClassic, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_S1ImgIconBTClassic, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1ImgIconBTClassic, 0);
    lv_obj_set_y(ui_S1ImgIconBTClassic, lv_pct(-35));
    lv_obj_set_align(ui_S1ImgIconBTClassic, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_S1ImgIconBTClassic, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_S1ImgIconBTClassic, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_S1ImgIconBLEhrm = lv_img_create(ui_S1PanelIcons);
    lv_img_set_src(ui_S1ImgIconBLEhrm, &ui_img_heartrate_png);
    lv_obj_set_width(ui_S1ImgIconBLEhrm, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_S1ImgIconBLEhrm, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_S1ImgIconBLEhrm, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_S1ImgIconBLEhrm, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_S1ImgIconBLEhrm, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_S1ImgIconWifi = lv_img_create(ui_S1PanelIcons);
    lv_img_set_src(ui_S1ImgIconWifi, &ui_img_wlan_png);
    lv_obj_set_width(ui_S1ImgIconWifi, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_S1ImgIconWifi, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1ImgIconWifi, 0);
    lv_obj_set_y(ui_S1ImgIconWifi, lv_pct(35));
    lv_obj_set_align(ui_S1ImgIconWifi, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_S1ImgIconWifi, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_S1ImgIconWifi, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_S1ArcCadence = lv_arc_create(ui_S1Main);
    lv_obj_set_width(ui_S1ArcCadence, 480);
    lv_obj_set_height(ui_S1ArcCadence, 480);
    lv_obj_set_align(ui_S1ArcCadence, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_S1ArcCadence, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE |
                      LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC);     /// Flags
    lv_arc_set_range(ui_S1ArcCadence, 0, 200);
    lv_obj_set_style_blend_mode(ui_S1ArcCadence, LV_BLEND_MODE_ADDITIVE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_S1ArcCadence, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_color(ui_S1ArcCadence, lv_color_hex(0xE2FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui_S1ArcCadence, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_S1ArcCadence, 4, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_S1ArcCadence, lv_color_hex(0xFF2D2D), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_S1ArcCadence, 255, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_S1ArcCadence, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_S1ArcCadence, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_S1ArcCadence, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_S1ArcCadence, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

    ui_S1ArcAvg = lv_arc_create(ui_S1Main);
    lv_obj_set_width(ui_S1ArcAvg, 449);
    lv_obj_set_height(ui_S1ArcAvg, 449);
    lv_obj_set_align(ui_S1ArcAvg, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_S1ArcAvg, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE |
                      LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ELASTIC);     /// Flags
    lv_arc_set_range(ui_S1ArcAvg, 0, 600);
    lv_obj_set_style_blend_mode(ui_S1ArcAvg, LV_BLEND_MODE_ADDITIVE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_S1ArcAvg, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_color(ui_S1ArcAvg, lv_color_hex(0x4040FF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui_S1ArcAvg, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_S1ArcAvg, 5, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_S1ArcAvg, lv_color_hex(0xFF2D2D), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_S1ArcAvg, 255, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_S1ArcAvg, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_S1ArcAvg, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_S1ArcAvg, 0, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_S1ArcAvg, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

    ui_S1ArcSpeed = lv_arc_create(ui_S1Main);
    lv_obj_set_width(ui_S1ArcSpeed, 475);
    lv_obj_set_height(ui_S1ArcSpeed, 475);
    lv_obj_set_align(ui_S1ArcSpeed, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_S1ArcSpeed, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE |
                      LV_OBJ_FLAG_SNAPPABLE);     /// Flags
    lv_arc_set_range(ui_S1ArcSpeed, 0, 600);

    lv_obj_set_style_arc_width(ui_S1ArcSpeed, 14, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_S1ArcSpeed, lv_color_hex(0x2DFF55), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_S1ArcSpeed, 255, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_S1ArcSpeed, 1, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_S1ArcSpeed, 1, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_S1ArcSpeed, 1, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_S1ArcSpeed, 1, LV_PART_KNOB | LV_STATE_DEFAULT);

    ui_S1BarPuls = lv_bar_create(ui_S1Main);
    lv_bar_set_range(ui_S1BarPuls, 60, 180);
    lv_bar_set_value(ui_S1BarPuls, 130, LV_ANIM_OFF);
    lv_obj_set_width(ui_S1BarPuls, 280);
    lv_obj_set_height(ui_S1BarPuls, 24);
    lv_obj_set_x(ui_S1BarPuls, 0);
    lv_obj_set_y(ui_S1BarPuls, -162);
    lv_obj_set_align(ui_S1BarPuls, LV_ALIGN_CENTER);

    lv_obj_set_style_bg_color(ui_S1BarPuls, lv_color_hex(0x00FF64), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_S1BarPuls, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_S1BarPuls, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_S1BarPuls, 90, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_S1BarPuls, 200, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_S1BarPuls, LV_GRAD_DIR_HOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ui_S1BarPulsLabel = lv_label_create(ui_S1BarPuls);
    lv_obj_set_width(ui_S1BarPulsLabel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_S1BarPulsLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_S1BarPulsLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_S1BarPulsLabel, "n/a");
    lv_obj_set_style_text_color(ui_S1BarPulsLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_S1BarPulsLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1BarPulsLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1LabelSpeed = lv_label_create(ui_S1Main);
    lv_obj_set_width(ui_S1LabelSpeed, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_S1LabelSpeed, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1LabelSpeed, 140);
    lv_obj_set_y(ui_S1LabelSpeed, -110);
    lv_obj_set_align(ui_S1LabelSpeed, LV_ALIGN_LEFT_MID);
    lv_label_set_text(ui_S1LabelSpeed, "0.0");
    lv_obj_set_style_text_font(ui_S1LabelSpeed, &ui_font_by75_96, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PanelStat = lv_obj_create(ui_S1Main);
    lv_obj_set_width(ui_S1PanelStat, 240);
    lv_obj_set_height(ui_S1PanelStat, 201);
    lv_obj_set_x(ui_S1PanelStat, 0);
    lv_obj_set_y(ui_S1PanelStat, 200);
    lv_obj_set_align(ui_S1PanelStat, LV_ALIGN_TOP_MID);
    lv_obj_clear_flag(ui_S1PanelStat, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_S1PStatLTitle = lv_label_create(ui_S1PanelStat);
    lv_obj_set_width(ui_S1PStatLTitle, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_S1PStatLTitle, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatLTitle, 0);
    lv_obj_set_y(ui_S1PStatLTitle, lv_pct(-50));
    lv_obj_set_align(ui_S1PStatLTitle, LV_ALIGN_CENTER);
    lv_label_set_text(ui_S1PStatLTitle, "n/a");
    lv_obj_set_style_text_font(ui_S1PStatLTitle, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PStatLAvg = lv_label_create(ui_S1PanelStat);
    lv_obj_set_width(ui_S1PStatLAvg, 115);
    lv_obj_set_height(ui_S1PStatLAvg, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatLAvg, lv_pct(-5));
    lv_obj_set_y(ui_S1PStatLAvg, lv_pct(10));
    lv_label_set_text(ui_S1PStatLAvg, "Schnitt");
    lv_obj_set_style_text_align(ui_S1PStatLAvg, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1PStatLAvg, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PStatLAvgVar = lv_label_create(ui_S1PanelStat);
    lv_obj_set_width(ui_S1PStatLAvgVar, 60);
    lv_obj_set_height(ui_S1PStatLAvgVar, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatLAvgVar, -6);
    lv_obj_set_y(ui_S1PStatLAvgVar, lv_pct(10));
    lv_obj_set_align(ui_S1PStatLAvgVar, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(ui_S1PStatLAvgVar, "0,0");
    lv_obj_set_style_text_color(ui_S1PStatLAvgVar, lv_color_hex(0x7F8A4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_S1PStatLAvgVar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_S1PStatLAvgVar, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1PStatLAvgVar, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PStatImgAvgUnit = lv_img_create(ui_S1PanelStat);
    lv_img_set_src(ui_S1PStatImgAvgUnit, &ui_img_kmh_xs_png);
    lv_obj_set_width(ui_S1PStatImgAvgUnit, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_S1PStatImgAvgUnit, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatImgAvgUnit, 105);
    lv_obj_set_y(ui_S1PStatImgAvgUnit, -53);
    lv_obj_set_align(ui_S1PStatImgAvgUnit, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_S1PStatImgAvgUnit, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_S1PStatImgAvgUnit, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_outline_color(ui_S1PStatImgAvgUnit, lv_color_hex(0xF60707), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(ui_S1PStatImgAvgUnit, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui_S1PStatImgAvgUnit, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_S1PStatImgAvgUnit, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PStatLGradientVar = lv_label_create(ui_S1PanelStat);
    lv_obj_set_width(ui_S1PStatLGradientVar, 80);
    lv_obj_set_height(ui_S1PStatLGradientVar, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatLGradientVar, 14);
    lv_obj_set_y(ui_S1PStatLGradientVar, lv_pct(30));
    lv_obj_set_align(ui_S1PStatLGradientVar, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(ui_S1PStatLGradientVar, "0,0%");
    lv_obj_set_style_text_color(ui_S1PStatLGradientVar, lv_color_hex(0x7F8A4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_S1PStatLGradientVar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_S1PStatLGradientVar, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1PStatLGradientVar, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PStatLGradient = lv_label_create(ui_S1PanelStat);
    lv_obj_set_width(ui_S1PStatLGradient, 115);
    lv_obj_set_height(ui_S1PStatLGradient, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatLGradient, lv_pct(-5));
    lv_obj_set_y(ui_S1PStatLGradient, lv_pct(30));
    lv_label_set_long_mode(ui_S1PStatLGradient, LV_LABEL_LONG_SCROLL);
    lv_label_set_text(ui_S1PStatLGradient, "Steigung");
    lv_obj_set_style_text_align(ui_S1PStatLGradient, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1PStatLGradient, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PStatLspdMax = lv_label_create(ui_S1PanelStat);
    lv_obj_set_width(ui_S1PStatLspdMax, 115);
    lv_obj_set_height(ui_S1PStatLspdMax, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatLspdMax, lv_pct(-5));
    lv_obj_set_y(ui_S1PStatLspdMax, lv_pct(50));
    lv_label_set_text(ui_S1PStatLspdMax, "Max");
    lv_obj_set_style_text_align(ui_S1PStatLspdMax, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1PStatLspdMax, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PStatLspdMaxVar = lv_label_create(ui_S1PanelStat);
    lv_obj_set_width(ui_S1PStatLspdMaxVar, 60);
    lv_obj_set_height(ui_S1PStatLspdMaxVar, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatLspdMaxVar, -6);
    lv_obj_set_y(ui_S1PStatLspdMaxVar, lv_pct(50));
    lv_obj_set_align(ui_S1PStatLspdMaxVar, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(ui_S1PStatLspdMaxVar, "0,0");
    lv_obj_set_style_text_color(ui_S1PStatLspdMaxVar, lv_color_hex(0x7F8A4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_S1PStatLspdMaxVar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_S1PStatLspdMaxVar, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1PStatLspdMaxVar, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PStatImgSpdMaxUnit = lv_img_create(ui_S1PanelStat);
    lv_img_set_src(ui_S1PStatImgSpdMaxUnit, &ui_img_kmh_xs_png);
    lv_obj_set_width(ui_S1PStatImgSpdMaxUnit, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_S1PStatImgSpdMaxUnit, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatImgSpdMaxUnit, 105);
    lv_obj_set_y(ui_S1PStatImgSpdMaxUnit, lv_pct(8));
    lv_obj_set_align(ui_S1PStatImgSpdMaxUnit, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_S1PStatImgSpdMaxUnit, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_S1PStatImgSpdMaxUnit, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_outline_color(ui_S1PStatImgSpdMaxUnit, lv_color_hex(0xF60707), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_opa(ui_S1PStatImgSpdMaxUnit, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(ui_S1PStatImgSpdMaxUnit, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_S1PStatImgSpdMaxUnit, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PStatLDist = lv_label_create(ui_S1PanelStat);
    lv_obj_set_width(ui_S1PStatLDist, 115);
    lv_obj_set_height(ui_S1PStatLDist, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatLDist, lv_pct(-5));
    lv_obj_set_y(ui_S1PStatLDist, lv_pct(70));
    lv_label_set_long_mode(ui_S1PStatLDist, LV_LABEL_LONG_SCROLL);
    lv_label_set_text(ui_S1PStatLDist, "Distance");
    lv_obj_set_style_text_align(ui_S1PStatLDist, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1PStatLDist, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PStatLDistVar = lv_label_create(ui_S1PanelStat);
    lv_obj_set_width(ui_S1PStatLDistVar, 130);
    lv_obj_set_height(ui_S1PStatLDistVar, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatLDistVar, -6);
    lv_obj_set_y(ui_S1PStatLDistVar, lv_pct(70));
    lv_obj_set_align(ui_S1PStatLDistVar, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(ui_S1PStatLDistVar, "0,0");
    lv_obj_set_style_text_color(ui_S1PStatLDistVar, lv_color_hex(0x7F8A4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_S1PStatLDistVar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_S1PStatLDistVar, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1PStatLDistVar, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PStatLTime = lv_label_create(ui_S1PanelStat);
    lv_obj_set_width(ui_S1PStatLTime, 115);
    lv_obj_set_height(ui_S1PStatLTime, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatLTime, lv_pct(-5));
    lv_obj_set_y(ui_S1PStatLTime, lv_pct(90));
    lv_label_set_text(ui_S1PStatLTime, "Time");
    lv_obj_set_style_text_align(ui_S1PStatLTime, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1PStatLTime, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1PStatLTimeVar = lv_label_create(ui_S1PanelStat);
    lv_obj_set_width(ui_S1PStatLTimeVar, 96);
    lv_obj_set_height(ui_S1PStatLTimeVar, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1PStatLTimeVar, 14);
    lv_obj_set_y(ui_S1PStatLTimeVar, lv_pct(90));
    lv_obj_set_align(ui_S1PStatLTimeVar, LV_ALIGN_TOP_RIGHT);
    lv_label_set_text(ui_S1PStatLTimeVar, "--:--:--");
    lv_obj_set_style_text_color(ui_S1PStatLTimeVar, lv_color_hex(0x7F8A4A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_S1PStatLTimeVar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_S1PStatLTimeVar, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1PStatLTimeVar, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1BarPowerMode = lv_bar_create(ui_S1Main);
    lv_bar_set_range(ui_S1BarPowerMode, 0, 4);
    lv_bar_set_value(ui_S1BarPowerMode, 2, LV_ANIM_OFF);
    lv_obj_set_width(ui_S1BarPowerMode, 10);
    lv_obj_set_height(ui_S1BarPowerMode, 75);
    lv_obj_set_x(ui_S1BarPowerMode, 134);
    lv_obj_set_y(ui_S1BarPowerMode, lv_pct(25));
    lv_obj_set_align(ui_S1BarPowerMode, LV_ALIGN_CENTER);

    ui_S1BarBatt = lv_bar_create(ui_S1Main);
    lv_obj_set_width(ui_S1BarBatt, 310);
    lv_obj_set_height(ui_S1BarBatt, 20);
    lv_obj_set_x(ui_S1BarBatt, 0);
    lv_obj_set_y(ui_S1BarBatt, 178);
    lv_obj_set_align(ui_S1BarBatt, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_S1BarBatt, LV_OBJ_FLAG_CLICK_FOCUSABLE);      /// Flags

    lv_obj_set_style_bg_color(ui_S1BarBatt, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_S1BarBatt, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_S1BarBatt, lv_color_hex(0x00FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_S1BarBatt, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_S1BarBatt, 200, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_S1BarBatt, LV_GRAD_DIR_HOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ui_S1BarBattLabel = lv_label_create(ui_S1BarBatt);
    lv_obj_set_width(ui_S1BarBattLabel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_S1BarBattLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1BarBattLabel, 0);
    lv_obj_set_y(ui_S1BarBattLabel, -1);
    lv_obj_set_align(ui_S1BarBattLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_S1BarBattLabel, "? %");
    lv_obj_set_style_text_color(ui_S1BarBattLabel, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_S1BarBattLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1BarBattLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S1LabelClock = lv_label_create(ui_S1Main);
    lv_obj_set_width(ui_S1LabelClock, 160);
    lv_obj_set_height(ui_S1LabelClock, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_S1LabelClock, 0);
    lv_obj_set_y(ui_S1LabelClock, 209);
    lv_obj_set_align(ui_S1LabelClock, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_S1LabelClock, LV_LABEL_LONG_DOT);
    lv_label_set_text(ui_S1LabelClock, "--:--:--");
    lv_obj_add_flag(ui_S1LabelClock, LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_set_style_text_align(ui_S1LabelClock, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S1LabelClock, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label2 = lv_label_create(ui_S1Main);
    lv_obj_set_width(ui_Label2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label2, 0);
    lv_obj_set_y(ui_Label2, -58);
    lv_obj_set_align(ui_Label2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label2, "? rpm");
    lv_obj_set_style_text_font(ui_Label2, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_S1RollerStatMode, ui_event_S1RollerStatMode, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_S1ImgIconBTClassic, ui_event_S1ImgIconBTClassic, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_S1ImgIconBLEhrm, ui_event_S1ImgIconBLEhrm, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_S1ImgIconWifi, ui_event_S1ImgIconWifi, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_S1PanelStat, ui_event_S1PanelStat, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_S1BarBatt, ui_event_S1BarBatt, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_S1LabelClock, ui_event_S1LabelClock, LV_EVENT_ALL, NULL);

}
void ui_ScreenChart_screen_init(void)
{
    ui_ScreenChart = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenChart, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_ScreenSettings_Panel2 = lv_obj_create(ui_ScreenChart);
    lv_obj_set_width(ui_ScreenSettings_Panel2, lv_pct(100));
    lv_obj_set_height(ui_ScreenSettings_Panel2, lv_pct(100));
    lv_obj_set_align(ui_ScreenSettings_Panel2, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_ScreenSettings_Panel2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_ScreenSettings_Panel2, 480, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_ScreenChart_Chart1 = lv_chart_create(ui_ScreenChart);
    lv_obj_set_width(ui_ScreenChart_Chart1, lv_pct(80));
    lv_obj_set_height(ui_ScreenChart_Chart1, lv_pct(66));
    lv_obj_set_align(ui_ScreenChart_Chart1, LV_ALIGN_CENTER);
    lv_chart_set_type(ui_ScreenChart_Chart1, LV_CHART_TYPE_LINE);
    lv_chart_set_axis_tick(ui_ScreenChart_Chart1, LV_CHART_AXIS_PRIMARY_X, 10, 5, 5, 2, true, 50);
    lv_chart_set_axis_tick(ui_ScreenChart_Chart1, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 5, 2, true, 50);
    lv_chart_series_t * ui_ScreenChart_Chart1_series_1 = lv_chart_add_series(ui_ScreenChart_Chart1, lv_color_hex(0x808080),
                                                                             LV_CHART_AXIS_PRIMARY_Y);
    static lv_coord_t ui_ScreenChart_Chart1_series_1_array[] = { 0, 10, 14, 40, 80, 80, 40, 20, 12, 0 };
    lv_chart_set_ext_y_array(ui_ScreenChart_Chart1, ui_ScreenChart_Chart1_series_1, ui_ScreenChart_Chart1_series_1_array);

    lv_obj_set_style_line_color(ui_ScreenChart_Chart1, lv_color_hex(0x4040FF), LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui_ScreenChart_Chart1, 255, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(ui_ScreenChart_Chart1, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_ScreenChart, ui_event_ScreenChart, LV_EVENT_ALL, NULL);

}

