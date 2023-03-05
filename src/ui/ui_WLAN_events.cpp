// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: BC_ScreenWLAN

#include "ui_WLAN.h"

lv_obj_t * ui_SWLANQR;

void connectToWifi(lv_event_t * e)
{
	// Your code here
}

void changePwStyle(lv_event_t * e)
{
	// Your code here
}

void WLANModeToggle(lv_event_t * e)
{
	// Your code here
}


void ui_SWLAN_extra_init() {
    ui_SWLANQR = lv_qrcode_create(ui_SWLANPanelPlacelholderQR, 180, lv_color_hex3(0x33f), lv_color_hex3(0xeef));
    lv_obj_set_align(ui_SWLANPanelPlacelholderQR, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_SWLANPanelPlacelholderQR, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    const char* qrstr = "https://github.com/euphi/TRGB-BikeComputer";
    lv_qrcode_update(ui_SWLANQR, qrstr, strlen(qrstr));
}


void ui_SWLANUpdateIP(const char * ipStr) {
	lv_label_set_text_fmt(ui_SWLANLabelIP, "IP: %s", ipStr);
	char qrBuff[128];
	snprintf(qrBuff, 127, "http://%s/", ipStr);
	lv_qrcode_update(ui_SWLANQR, qrBuff, strlen(qrBuff));
}
