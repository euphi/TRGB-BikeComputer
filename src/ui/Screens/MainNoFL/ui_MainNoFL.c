// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.2.2
// LVGL version: 8.3.4
// Project name: BC_noFL_Main

#include "ui_MainNoFL.h"

#include "ui/ui_WLAN.h"
#include "ui/ui_Settings.h"

#include "ui/ui_Navi.h"

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

///////////////////// SCREENS ////////////////////
void ui_SMainNoFL_screen_init(void)
{
    ui_SMainNoFL = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_SMainNoFL, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_SMainNoFL, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_SMainNoFL, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_PanelNav = lv_obj_create(ui_SMainNoFL);
    lv_obj_set_width(ui_PanelNav, 96);
    lv_obj_set_height(ui_PanelNav, 96);
    lv_obj_set_x(ui_PanelNav, 0);
    lv_obj_set_y(ui_PanelNav, -162);
    lv_obj_set_align(ui_PanelNav, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_PanelNav, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelNav, lv_color_hex(0xC8FFF7), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelNav, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_ImgNav = lv_img_create(ui_PanelNav);
    lv_img_set_src(ui_ImgNav, &nav_64_nonav);
    lv_obj_set_width(ui_ImgNav, 64);
    lv_obj_set_height(ui_ImgNav, 64);
    lv_obj_set_x(ui_ImgNav, -8);
    lv_obj_set_y(ui_ImgNav, -10);
    lv_obj_set_align(ui_ImgNav, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ImgNav, LV_OBJ_FLAG_ADV_HITTEST | LV_OBJ_FLAG_EVENT_BUBBLE);     /// Flags
    lv_obj_clear_flag(ui_ImgNav, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_BarNav = lv_bar_create(ui_PanelNav);
    lv_bar_set_value(ui_BarNav, 100, LV_ANIM_OFF);
    lv_obj_set_width(ui_BarNav, 12);
    lv_obj_set_height(ui_BarNav, 90);
    lv_obj_set_x(ui_BarNav, 38);
    lv_obj_set_y(ui_BarNav, 0);
    lv_obj_set_align(ui_BarNav, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_BarNav, LV_OBJ_FLAG_EVENT_BUBBLE);     /// Flags

    ui_LabelNavDist = lv_label_create(ui_PanelNav);
    lv_obj_set_width(ui_LabelNavDist, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelNavDist, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelNavDist, -8);
    lv_obj_set_y(ui_LabelNavDist, 35);
    lv_obj_set_align(ui_LabelNavDist, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelNavDist, "?? m");
    lv_obj_add_flag(ui_LabelNavDist, LV_OBJ_FLAG_EVENT_BUBBLE);     /// Flags
    lv_obj_set_style_text_color(ui_LabelNavDist, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelNavDist, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelNavDist, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_ArcSpeed = lv_arc_create(ui_SMainNoFL);
    lv_obj_set_width(ui_ArcSpeed, 470);
    lv_obj_set_height(ui_ArcSpeed, 470);
    lv_obj_set_align(ui_ArcSpeed, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_ArcSpeed, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_arc_set_range(ui_ArcSpeed, 0, 600);
    lv_obj_set_style_arc_color(ui_ArcSpeed, lv_color_hex(0xE0E0E0), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui_ArcSpeed, 128, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_ArcSpeed, 15, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_width(ui_ArcSpeed, 15, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_ArcSpeed, lv_color_hex(0x00FF26), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ArcSpeed, 255, LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_ArcSpeed, LV_BORDER_SIDE_FULL, LV_PART_KNOB | LV_STATE_DEFAULT);

    ui_ArcAvg = lv_arc_create(ui_SMainNoFL);
    lv_obj_set_width(ui_ArcAvg, 480);
    lv_obj_set_height(ui_ArcAvg, 480);
    lv_obj_set_align(ui_ArcAvg, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_ArcAvg, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_arc_set_range(ui_ArcAvg, 0, 600);
    lv_obj_set_style_arc_color(ui_ArcAvg, lv_color_hex(0xE5DEAE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui_ArcAvg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_ArcAvg, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_color(ui_ArcAvg, lv_color_hex(0xFFE200), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui_ArcAvg, 200, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_ArcAvg, 5, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_ArcAvg, lv_color_hex(0xFF3A00), LV_PART_KNOB | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ArcAvg, 255, LV_PART_KNOB | LV_STATE_DEFAULT);

    ui_ArcCad = lv_arc_create(ui_SMainNoFL);
    lv_obj_set_width(ui_ArcCad, 440);
    lv_obj_set_height(ui_ArcCad, 440);
    lv_obj_set_align(ui_ArcCad, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_ArcCad, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_arc_set_range(ui_ArcCad, 0, 140);
    lv_obj_set_style_arc_color(ui_ArcCad, lv_color_hex(0x005766), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui_ArcCad, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_ArcCad, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_arc_color(ui_ArcCad, lv_color_hex(0x005766), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_opa(ui_ArcCad, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_ArcCad, 5, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ui_LabelSpeed = lv_label_create(ui_SMainNoFL);
    lv_obj_set_width(ui_LabelSpeed, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelSpeed, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelSpeed, 0);
    lv_obj_set_y(ui_LabelSpeed, -58);
    lv_obj_set_align(ui_LabelSpeed, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelSpeed, "--");
    lv_obj_set_style_text_color(ui_LabelSpeed, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelSpeed, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelSpeed, &ui_font_by7x128, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_BarHR = lv_bar_create(ui_SMainNoFL);
    lv_bar_set_range(ui_BarHR, 50, 180);
    lv_bar_set_value(ui_BarHR, 180, LV_ANIM_OFF);
    lv_obj_set_width(ui_BarHR, 340);
    lv_obj_set_height(ui_BarHR, 58);
    lv_obj_set_x(ui_BarHR, 0);
    lv_obj_set_y(ui_BarHR, 112);
    lv_obj_set_align(ui_BarHR, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_BarHR, lv_color_hex(0x3FFF00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BarHR, 32, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_BarHR, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_BarHR, 110, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_BarHR, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_BarHR, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_radius(ui_BarHR, 25, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BarHR, lv_color_hex(0x3FFF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BarHR, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_BarHR, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_BarHR, 110, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_BarHR, 200, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_BarHR, LV_GRAD_DIR_HOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ui_LabelHR = lv_label_create(ui_BarHR);
    lv_obj_set_width(ui_LabelHR, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelHR, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LabelHR, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelHR, "--");
    lv_obj_set_style_text_color(ui_LabelHR, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelHR, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelHR, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelDist = lv_label_create(ui_SMainNoFL);
    lv_obj_set_width(ui_LabelDist, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelDist, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelDist, 0);
    lv_obj_set_y(ui_LabelDist, 14);
    lv_obj_set_align(ui_LabelDist, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelDist, "-- km");
    lv_obj_set_style_text_color(ui_LabelDist, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelDist, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelDist, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_PanelClock = lv_obj_create(ui_SMainNoFL);
    lv_obj_set_width(ui_PanelClock, 220);
    lv_obj_set_height(ui_PanelClock, 103);
    lv_obj_set_x(ui_PanelClock, 0);
    lv_obj_set_y(ui_PanelClock, 194);
    lv_obj_set_align(ui_PanelClock, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_PanelClock, LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_PanelClock, lv_color_hex(0xFDFFDE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_PanelClock, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelClock = lv_label_create(ui_PanelClock);
    lv_obj_set_width(ui_LabelClock, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelClock, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelClock, 0);
    lv_obj_set_y(ui_LabelClock, -25);
    lv_obj_set_align(ui_LabelClock, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelClock, "--:--:--");
    lv_obj_add_flag(ui_LabelClock, LV_OBJ_FLAG_EVENT_BUBBLE);     /// Flags
    lv_obj_set_style_text_color(ui_LabelClock, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelClock, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelClock, &ui_font_FontSchild, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelClockMode = lv_label_create(ui_PanelClock);
    lv_obj_set_width(ui_LabelClockMode, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelClockMode, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelClockMode, 0);
    lv_obj_set_y(ui_LabelClockMode, 19);
    lv_obj_set_align(ui_LabelClockMode, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelClockMode, "- - -");
    lv_obj_add_flag(ui_LabelClockMode, LV_OBJ_FLAG_EVENT_BUBBLE);     /// Flags
    lv_obj_set_style_text_color(ui_LabelClockMode, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelClockMode, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelClockMode, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LabelCad = lv_label_create(ui_SMainNoFL);
    lv_obj_set_width(ui_LabelCad, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelCad, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelCad, 0);
    lv_obj_set_y(ui_LabelCad, 59);
    lv_obj_set_align(ui_LabelCad, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelCad, "-- rpm");
    lv_obj_set_style_text_color(ui_LabelCad, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelCad, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelCad, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_BarBatt = lv_bar_create(ui_SMainNoFL);
    lv_obj_set_width(ui_BarBatt, 16);
    lv_obj_set_height(ui_BarBatt, 55);
    lv_obj_set_x(ui_BarBatt, -125);
    lv_obj_set_y(ui_BarBatt, 174);
    lv_obj_set_align(ui_BarBatt, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_BarBatt, lv_color_hex(0x00FF00), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BarBatt, 64, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_BarBatt, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_BarBatt, 40, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_BarBatt, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_BarBatt, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_radius(ui_BarBatt, 5, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_BarBatt, lv_color_hex(0x00FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_BarBatt, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_BarBatt, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_BarBatt, 40, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_BarBatt, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_BarBatt, LV_GRAD_DIR_VER, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ui_ImgWifi = lv_img_create(ui_SMainNoFL);
    lv_img_set_src(ui_ImgWifi, &ui_img_wlan_png);
    lv_obj_set_width(ui_ImgWifi, 64);
    lv_obj_set_height(ui_ImgWifi, 64);
    lv_obj_set_x(ui_ImgWifi, -155);
    lv_obj_set_y(ui_ImgWifi, 36);
    lv_obj_set_align(ui_ImgWifi, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ImgWifi, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_ImgWifi, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_ImgSettings = lv_img_create(ui_SMainNoFL);
    lv_img_set_src(ui_ImgSettings, &ui_img_ic_settings_48px_svg_png);
    lv_obj_set_width(ui_ImgSettings, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ImgSettings, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ImgSettings, 135);
    lv_obj_set_y(ui_ImgSettings, 165);
    lv_obj_set_align(ui_ImgSettings, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ImgSettings, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_ImgSettings, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_LabelSpdAvg = lv_label_create(ui_SMainNoFL);
    lv_obj_set_width(ui_LabelSpdAvg, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelSpdAvg, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelSpdAvg, -111);
    lv_obj_set_y(ui_LabelSpdAvg, -127);
    lv_obj_set_align(ui_LabelSpdAvg, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelSpdAvg, "--,-km/h");
    lv_obj_set_style_text_color(ui_LabelSpdAvg, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelSpdAvg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelSpdAvg, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_PanelNav, ui_event_PanelNav, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_PanelClock, ui_event_PanelClock, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ImgWifi, ui_event_ImgWifi, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ImgSettings, ui_event_ImgSettings, LV_EVENT_ALL, NULL);

}
