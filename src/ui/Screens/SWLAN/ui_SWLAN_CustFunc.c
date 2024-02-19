/*
 * ui_SWLAN_CustFunc.c
 *
 *  Created on: 17.02.2024
 *      Author: ian
 */

#include "ui_SWLAN_CustFunc.h"
#include <ui/Screens/SWLAN/ui.h>

lv_obj_t * ui_SWLANQR;

void ui_SWLAN_extra_init() {
    ui_SWLANQR = lv_qrcode_create(ui_SWLANPanelPlacelholderQR, 190, lv_color_hex3(0x33f), lv_color_hex3(0xeef));
    lv_obj_set_align(ui_SWLANPanelPlacelholderQR, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_SWLANPanelPlacelholderQR, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    const char* qrstr = "https://github.com/euphi/TRGB-BikeComputer";
    lv_qrcode_update(ui_SWLANQR, qrstr, strlen(qrstr));
}

void ui_SWLANUpdateIP(const char * ipStr) {
	if (ui_SWLANLabelIP == NULL) return;		// Can happen in init
	lv_label_set_text_fmt(ui_SWLANLabelIP, "IP: %s", ipStr);
	char qrBuff[128];
	snprintf(qrBuff, 127, "http://%s/", ipStr);
	lv_qrcode_update(ui_SWLANQR, qrBuff, strlen(qrBuff));
}

void ui_SWLANUpdateSSIDList(const char * ssidList) {
	lv_dropdown_set_options(ui_SWLANDropAP, ssidList);
	lv_dropdown_set_selected(ui_SWLANDropAP, 0);
}

void ui_SWLANUpdateWiFiState(bool wifiEnabled, bool APModeActive, bool disableAPMode, short apStaCount) {
	//TODO: wifiEnabled unused yet
	if (APModeActive) lv_obj_add_state(ui_SWLANCBModeAP, LV_STATE_CHECKED); else lv_obj_clear_state(ui_SWLANCBModeAP, LV_STATE_CHECKED);
	if (!disableAPMode) lv_obj_add_state(ui_SWLANCBModeAutoAP, LV_STATE_CHECKED); else lv_obj_clear_state(ui_SWLANCBModeAutoAP, LV_STATE_CHECKED);
	if (apStaCount > 0) {
		char buffer[30];
		snprintf(buffer, 30, "AP Mode (%d clients)", apStaCount);
		lv_checkbox_set_text(ui_SWLANCBModeAP, buffer);
	} else {
		lv_checkbox_set_text_static(ui_SWLANCBModeAP, "AP Mode");
	}
}
