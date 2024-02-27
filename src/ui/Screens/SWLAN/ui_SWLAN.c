// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.4
// LVGL version: 8.3.4
// Project name: BC_ScreenWLAN

#include "ui.h"

void ui_SWLAN_screen_init(void)
{
    ui_SWLAN = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_SWLAN, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_SWLANDropAP = lv_dropdown_create(ui_SWLAN);
    lv_dropdown_set_options(ui_SWLANDropAP, "** no scan **");
    lv_obj_set_width(ui_SWLANDropAP, 240);
    lv_obj_set_height(ui_SWLANDropAP, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SWLANDropAP, -75);
    lv_obj_set_y(ui_SWLANDropAP, -130);
    lv_obj_set_align(ui_SWLANDropAP, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_SWLANDropAP, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags

    ui_SWLANButConn = lv_btn_create(ui_SWLAN);
    lv_obj_set_width(ui_SWLANButConn, 100);
    lv_obj_set_height(ui_SWLANButConn, 45);
    lv_obj_set_x(ui_SWLANButConn, 105);
    lv_obj_set_y(ui_SWLANButConn, -130);
    lv_obj_set_align(ui_SWLANButConn, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_SWLANButConn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_SWLANButConn, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_SWLANButtCBLabel = lv_label_create(ui_SWLANButConn);
    lv_obj_set_width(ui_SWLANButtCBLabel, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SWLANButtCBLabel, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_SWLANButtCBLabel, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SWLANButtCBLabel, "Store");
    lv_obj_set_style_text_font(ui_SWLANButtCBLabel, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SWLANCBModeAP = lv_checkbox_create(ui_SWLAN);
    lv_checkbox_set_text(ui_SWLANCBModeAP, "AP Mode");
    lv_obj_set_width(ui_SWLANCBModeAP, 150);
    lv_obj_set_height(ui_SWLANCBModeAP, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SWLANCBModeAP, -12);
    lv_obj_set_y(ui_SWLANCBModeAP, -170);
    lv_obj_set_align(ui_SWLANCBModeAP, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_SWLANCBModeAP, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_set_style_text_font(ui_SWLANCBModeAP, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SWLANCBModeAutoAP = lv_checkbox_create(ui_SWLAN);
    lv_checkbox_set_text(ui_SWLANCBModeAutoAP, "Auto AP");
    lv_obj_set_width(ui_SWLANCBModeAutoAP, 150);
    lv_obj_set_height(ui_SWLANCBModeAutoAP, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SWLANCBModeAutoAP, -12);
    lv_obj_set_y(ui_SWLANCBModeAutoAP, -210);
    lv_obj_set_align(ui_SWLANCBModeAutoAP, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_SWLANCBModeAutoAP, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_set_style_text_font(ui_SWLANCBModeAutoAP, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SWLANLabelIP = lv_label_create(ui_SWLAN);
    lv_obj_set_width(ui_SWLANLabelIP, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SWLANLabelIP, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SWLANLabelIP, -3);
    lv_obj_set_y(ui_SWLANLabelIP, -30);
    lv_obj_set_align(ui_SWLANLabelIP, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SWLANLabelIP, "IP: not connected");
    lv_obj_set_style_text_font(ui_SWLANLabelIP, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SWLANPanel1 = lv_obj_create(ui_SWLAN);
    lv_obj_set_width(ui_SWLANPanel1, 360);
    lv_obj_set_height(ui_SWLANPanel1, 50);
    lv_obj_set_x(ui_SWLANPanel1, 3);
    lv_obj_set_y(ui_SWLANPanel1, -80);
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
    lv_obj_set_y(ui_SWLANPanelPlacelholderQR, 110);
    lv_obj_set_align(ui_SWLANPanelPlacelholderQR, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_SWLANPanelPlacelholderQR, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_SWLANRollerSSID = lv_roller_create(ui_SWLAN);
    lv_roller_set_options(ui_SWLANRollerSSID, "SSID 1\nSSID 2\nSSID 3", LV_ROLLER_MODE_INFINITE);
    lv_obj_set_height(ui_SWLANRollerSSID, 100);
    lv_obj_set_width(ui_SWLANRollerSSID, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_x(ui_SWLANRollerSSID, -160);
    lv_obj_set_y(ui_SWLANRollerSSID, 50);
    lv_obj_set_align(ui_SWLANRollerSSID, LV_ALIGN_CENTER);

    ui_SWLANKeyB = lv_keyboard_create(ui_SWLAN);
    lv_obj_set_width(ui_SWLANKeyB, 383);
    lv_obj_set_height(ui_SWLANKeyB, 180);
    lv_obj_set_x(ui_SWLANKeyB, 0);
    lv_obj_set_y(ui_SWLANKeyB, 80);
    lv_obj_set_align(ui_SWLANKeyB, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_SWLANKeyB, LV_OBJ_FLAG_HIDDEN);     /// Flags

    lv_obj_add_event_cb(ui_SWLANButConn, ui_event_SWLANButConn, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_SWLANCBModeAP, ui_event_SWLANCBModeAP, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_SWLANCBModeAutoAP, ui_event_SWLANCBModeAutoAP, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_SWLANTextPW, ui_event_SWLANTextPW, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_SWLANLabelPW, ui_event_SWLANLabelPW, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_SWLANRollerSSID, ui_event_SWLANRollerSSID, LV_EVENT_ALL, NULL);
    lv_keyboard_set_textarea(ui_SWLANKeyB, ui_SWLANTextPW);
    lv_obj_add_event_cb(ui_SWLAN, ui_event_SWLAN, LV_EVENT_ALL, NULL);
}