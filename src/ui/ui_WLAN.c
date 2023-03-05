// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: BC_ScreenWLAN

#include "ui_WLAN.h"
#include "ui.h"
#include "ui_helpers.h"

///////////////////// VARIABLES ////////////////////
void ui_event_SWLAN(lv_event_t * e);
lv_obj_t * ui_SWLAN;
lv_obj_t * ui_SWLANDropAP;
void ui_event_SWLANButConn(lv_event_t * e);
lv_obj_t * ui_SWLANButConn;
lv_obj_t * ui_SWLANButtCBLabel;
void ui_event_SWLANCBMode(lv_event_t * e);
lv_obj_t * ui_SWLANCBMode;
lv_obj_t * ui_SWLANLabelIP;
lv_obj_t * ui_SWLANPanel1;
void ui_event_SWLANTextPW(lv_event_t * e);
lv_obj_t * ui_SWLANTextPW;
void ui_event_SWLANLabelPW(lv_event_t * e);
lv_obj_t * ui_SWLANLabelPW;
lv_obj_t * ui_SWLANPanelPlacelholderQR;
lv_obj_t * ui_SWLANKeyB;

///////////////////// TEST LVGL SETTINGS ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////
void ui_event_SWLAN(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        _ui_screen_change(ui_S1Main, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0);
    }
}
void ui_event_SWLANButConn(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
//        _ui_flag_modify(ui_SWLANKeyB, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_TOGGLE);
    }
}
void ui_event_SWLANCBMode(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_VALUE_CHANGED) {
        WLANModeToggle(e);
    }
}
void ui_event_SWLANTextPW(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_FOCUSED) {
        _ui_flag_modify(ui_SWLANKeyB, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_REMOVE);
    }
    if(event_code == LV_EVENT_DEFOCUSED) {
        _ui_flag_modify(ui_SWLANKeyB, LV_OBJ_FLAG_HIDDEN, _UI_MODIFY_FLAG_ADD);
    }
}
void ui_event_SWLANLabelPW(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_CLICKED) {
        changePwStyle(e);
    }
}

///////////////////// SCREENS ////////////////////
void ui_SWLAN_screen_init(void)
{
    ui_SWLAN = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_SWLAN, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_SWLANDropAP = lv_dropdown_create(ui_SWLAN);
    lv_dropdown_set_options(ui_SWLANDropAP, "IA216\nIA216oT\nIA216mobil\nfablabnbg");
    lv_obj_set_width(ui_SWLANDropAP, 204);
    lv_obj_set_height(ui_SWLANDropAP, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SWLANDropAP, -58);
    lv_obj_set_y(ui_SWLANDropAP, -155);
    lv_obj_set_align(ui_SWLANDropAP, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_SWLANDropAP, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags

    ui_SWLANButConn = lv_btn_create(ui_SWLAN);
    lv_obj_set_width(ui_SWLANButConn, 100);
    lv_obj_set_height(ui_SWLANButConn, 35);
    lv_obj_set_x(ui_SWLANButConn, 105);
    lv_obj_set_y(ui_SWLANButConn, -158);
    lv_obj_set_align(ui_SWLANButConn, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_SWLANButConn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_SWLANButConn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_SWLANButtCBLabel = lv_label_create(ui_SWLANButConn);
    lv_obj_set_width(ui_SWLANButtCBLabel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SWLANButtCBLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_SWLANButtCBLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SWLANButtCBLabel, "Connect");

    ui_SWLANCBMode = lv_checkbox_create(ui_SWLAN);
    lv_checkbox_set_text(ui_SWLANCBMode, "AP Mode");
    lv_obj_set_width(ui_SWLANCBMode, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SWLANCBMode, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SWLANCBMode, -12);
    lv_obj_set_y(ui_SWLANCBMode, -201);
    lv_obj_set_align(ui_SWLANCBMode, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_SWLANCBMode, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_set_style_text_font(ui_SWLANCBMode, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SWLANLabelIP = lv_label_create(ui_SWLAN);
    lv_obj_set_width(ui_SWLANLabelIP, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SWLANLabelIP, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SWLANLabelIP, -3);
    lv_obj_set_y(ui_SWLANLabelIP, -57);
    lv_obj_set_align(ui_SWLANLabelIP, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SWLANLabelIP, "IP: not connected");
    lv_obj_set_style_text_font(ui_SWLANLabelIP, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SWLANPanel1 = lv_obj_create(ui_SWLAN);
    lv_obj_set_width(ui_SWLANPanel1, 360);
    lv_obj_set_height(ui_SWLANPanel1, 50);
    lv_obj_set_x(ui_SWLANPanel1, 3);
    lv_obj_set_y(ui_SWLANPanel1, -106);
    lv_obj_set_align(ui_SWLANPanel1, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_SWLANPanel1, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_SWLANPanel1, lv_color_hex(0x00004D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_SWLANPanel1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SWLANTextPW = lv_textarea_create(ui_SWLANPanel1);
    lv_obj_set_width(ui_SWLANTextPW, 220);
    lv_obj_set_height(ui_SWLANTextPW, LV_SIZE_CONTENT);    /// 39
    lv_obj_set_x(ui_SWLANTextPW, 60);
    lv_obj_set_y(ui_SWLANTextPW, 0);
    lv_obj_set_align(ui_SWLANTextPW, LV_ALIGN_CENTER);
    lv_textarea_set_max_length(ui_SWLANTextPW, 24);
    lv_textarea_set_placeholder_text(ui_SWLANTextPW, "**** Wifi PW ****");
    lv_textarea_set_one_line(ui_SWLANTextPW, true);
    lv_textarea_set_password_mode(ui_SWLANTextPW, true);

    ui_SWLANLabelPW = lv_label_create(ui_SWLANPanel1);
    lv_obj_set_width(ui_SWLANLabelPW, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SWLANLabelPW, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SWLANLabelPW, -116);
    lv_obj_set_y(ui_SWLANLabelPW, 0);
    lv_obj_set_align(ui_SWLANLabelPW, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SWLANLabelPW, "Passwort");
    lv_obj_set_style_text_font(ui_SWLANLabelPW, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SWLANPanelPlacelholderQR = lv_obj_create(ui_SWLAN);
    lv_obj_set_width(ui_SWLANPanelPlacelholderQR, 220);
    lv_obj_set_height(ui_SWLANPanelPlacelholderQR, 220);
    lv_obj_set_x(ui_SWLANPanelPlacelholderQR, 0);
    lv_obj_set_y(ui_SWLANPanelPlacelholderQR, 75);
    lv_obj_set_align(ui_SWLANPanelPlacelholderQR, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_SWLANPanelPlacelholderQR, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_SWLANKeyB = lv_keyboard_create(ui_SWLAN);
    lv_obj_set_width(ui_SWLANKeyB, 383);
    lv_obj_set_height(ui_SWLANKeyB, 180);
    lv_obj_set_x(ui_SWLANKeyB, 0);
    lv_obj_set_y(ui_SWLANKeyB, 80);
    lv_obj_set_align(ui_SWLANKeyB, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_SWLANKeyB, LV_OBJ_FLAG_HIDDEN);     /// Flags

    lv_obj_add_event_cb(ui_SWLANButConn, ui_event_SWLANButConn, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_SWLANCBMode, ui_event_SWLANCBMode, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_SWLANTextPW, ui_event_SWLANTextPW, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_SWLANLabelPW, ui_event_SWLANLabelPW, LV_EVENT_ALL, NULL);
    lv_keyboard_set_textarea(ui_SWLANKeyB, ui_SWLANTextPW);
    lv_obj_add_event_cb(ui_SWLAN, ui_event_SWLAN, LV_EVENT_ALL, NULL);

}
