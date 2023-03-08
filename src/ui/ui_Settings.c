// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: TRGB-BC-Settings

#include "ui_Settings.h"
#include "ui_helpers.h"
#include "ui.h"

///////////////////// VARIABLES ////////////////////
void ui_event_ScrSettings(lv_event_t * e);
lv_obj_t * ui_ScrSettings;
void ui_event_ScrSettButSleep(lv_event_t * e);
lv_obj_t * ui_ScrSettButSleep;
lv_obj_t * ui_ScrSettButSleepLabel;
void ui_event_ScrSettSliderBright(lv_event_t * e);
lv_obj_t * ui_ScrSettSliderBright;
lv_obj_t * ui_ScrSettSliderBrightLabel;
lv_obj_t * ui_ScrSettLabelClockDate;
lv_obj_t * ui_ScrSettLabelVersion;

///////////////////// TEST LVGL SETTINGS ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////
void ui_event_ScrSettings(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        _ui_screen_change(ui_S1Main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0);
    }
}
void ui_event_ScrSettButSleep(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_LONG_PRESSED) {
        EvDeepSleep(e);
    }
}
void ui_event_ScrSettSliderBright(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_VALUE_CHANGED) {
        BrightChanged(e);
    }
}

///////////////////// SCREENS ////////////////////
void ui_ScrSettings_screen_init(void)
{
    ui_ScrSettings = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScrSettings, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_ScrSettButSleep = lv_btn_create(ui_ScrSettings);
    lv_obj_set_width(ui_ScrSettButSleep, 180);
    lv_obj_set_height(ui_ScrSettButSleep, 50);
    lv_obj_set_x(ui_ScrSettButSleep, 0);
    lv_obj_set_y(ui_ScrSettButSleep, 100);
    lv_obj_set_align(ui_ScrSettButSleep, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ScrSettButSleep, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ScrSettButSleep, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_ScrSettButSleep, lv_color_hex(0xB88E00), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(ui_ScrSettButSleep, 255, LV_PART_MAIN | LV_STATE_PRESSED);

    ui_ScrSettButSleepLabel = lv_label_create(ui_ScrSettButSleep);
    lv_obj_set_width(ui_ScrSettButSleepLabel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScrSettButSleepLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_ScrSettButSleepLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ScrSettButSleepLabel, "Deep Sleep");
    lv_obj_set_style_text_font(ui_ScrSettButSleepLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_ScrSettSliderBright = lv_slider_create(ui_ScrSettings);
    lv_slider_set_range(ui_ScrSettSliderBright, 35, 255);
    lv_slider_set_value(ui_ScrSettSliderBright, 255, LV_ANIM_OFF);
    if(lv_slider_get_mode(ui_ScrSettSliderBright) == LV_SLIDER_MODE_RANGE) lv_slider_set_left_value(ui_ScrSettSliderBright,
                                                                                                        0, LV_ANIM_OFF);
    lv_obj_set_width(ui_ScrSettSliderBright, 400);
    lv_obj_set_height(ui_ScrSettSliderBright, 32);
    lv_obj_set_align(ui_ScrSettSliderBright, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_ScrSettSliderBright, LV_OBJ_FLAG_GESTURE_BUBBLE);      /// Flags

    lv_obj_set_style_bg_color(ui_ScrSettSliderBright, lv_color_hex(0x524600), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ScrSettSliderBright, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_ScrSettSliderBright, lv_color_hex(0xFFE65A), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_ScrSettSliderBright, LV_GRAD_DIR_HOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ui_ScrSettSliderBrightLabel = lv_label_create(ui_ScrSettSliderBright);
    lv_obj_set_width(ui_ScrSettSliderBrightLabel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScrSettSliderBrightLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_ScrSettSliderBrightLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ScrSettSliderBrightLabel, "Helligkeit");
    lv_obj_set_style_text_color(ui_ScrSettSliderBrightLabel, lv_color_hex(0xD8D8D8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ScrSettSliderBrightLabel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_ScrSettSliderBrightLabel, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_ScrSettLabelClockDate = lv_label_create(ui_ScrSettings);
    lv_obj_set_width(ui_ScrSettLabelClockDate, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScrSettLabelClockDate, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScrSettLabelClockDate, 0);
    lv_obj_set_y(ui_ScrSettLabelClockDate, -200);
    lv_obj_set_align(ui_ScrSettLabelClockDate, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ScrSettLabelClockDate, "--:--:--\ndd.mm.YYYY");
    lv_obj_set_style_text_align(ui_ScrSettLabelClockDate, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_ScrSettLabelClockDate, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_ScrSettLabelVersion = lv_label_create(ui_ScrSettings);
    lv_obj_set_width(ui_ScrSettLabelVersion, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScrSettLabelVersion, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScrSettLabelVersion, 0);
    lv_obj_set_y(ui_ScrSettLabelVersion, -100);
    lv_obj_set_align(ui_ScrSettLabelVersion, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ScrSettLabelVersion, "TRGB-Bikecomputer\nV0.0.1");
    lv_obj_set_style_text_align(ui_ScrSettLabelVersion, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_ScrSettLabelVersion, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_ScrSettButSleep, ui_event_ScrSettButSleep, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ScrSettSliderBright, ui_event_ScrSettSliderBright, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ScrSettings, ui_event_ScrSettings, LV_EVENT_ALL, NULL);

}
