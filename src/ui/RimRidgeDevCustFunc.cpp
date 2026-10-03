/*
 * RimRidgeDevCustFunc.cpp
 *
 * See RimRidgeDevCustFunc.h for scope notes.
 */

#include <stdio.h>
#include <string.h>

#include "ui_eez/screens.h"
#include "ui_eez/ui.h"
#include "ui_eez/actions.h"
#include "RimRidgeDevCustFunc.h"
#include "Singletons.h"		// bleDevs

namespace {

// Hardcoded like in RimRidgeSettingsCustFunc.cpp: RRZoneGreen, RRZoneYellow, RRZoneRed, RRMuted.
const uint32_t COLOR_GREEN = 0x6FA98C;
const uint32_t COLOR_YELLOW = 0xD7B463;
const uint32_t COLOR_RED = 0xC1604A;
const uint32_t COLOR_MUTED = 0x9BA097;

struct Row {
	lv_obj_t* row;
	lv_obj_t* dot;
	lv_obj_t* name;
	lv_obj_t* battery;
	BLEDevices::EDevType slot;
	bool shown;
};

Row rows[5];

void setText(lv_obj_t* label, const char* text) {
	if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

}	// namespace

void ui_RimRidgeDevInit() {
	rows[0] = {objects.rrsd_row_csc1, objects.rrsd_row_csc1_dot, objects.rrsd_row_csc1_lbl, objects.rrsd_row_csc1_bat, BLEDevices::DEV_CSC_1, true};
	rows[1] = {objects.rrsd_row_csc2, objects.rrsd_row_csc2_dot, objects.rrsd_row_csc2_lbl, objects.rrsd_row_csc2_bat, BLEDevices::DEV_CSC_2, true};
	rows[2] = {objects.rrsd_row_hr, objects.rrsd_row_hr_dot, objects.rrsd_row_hr_lbl, objects.rrsd_row_hr_bat, BLEDevices::DEV_HRM, true};
	rows[3] = {objects.rrsd_row_tb, objects.rrsd_row_tb_dot, objects.rrsd_row_tb_lbl, objects.rrsd_row_tb_bat, BLEDevices::DEV_NAV, true};
	rows[4] = {objects.rrsd_row_fl, objects.rrsd_row_fl_dot, objects.rrsd_row_fl_lbl, objects.rrsd_row_fl_bat, BLEDevices::DEV_FL, false};
#ifdef BC_FL_SUPPORT
	rows[4].shown = true;
#else
	lv_obj_add_flag(rows[4].row, LV_OBJ_FLAG_HIDDEN);
#endif
	ui_RimRidgeDevUpdate();
}

void ui_RimRidgeDevUpdate() {
	uint8_t connected = 0, total = 0;
	for (Row& r : rows) {
		if (!r.shown) continue;
		const BLEDevices::DevStatus s = bleDevs.getDevStatus(r.slot);
		total++;

		uint32_t color = COLOR_MUTED;
		switch (s.state) {
		case BLEDevices::CONN_CONNECTED: color = COLOR_GREEN; connected++; break;
		case BLEDevices::CONN_ADVERTISED: color = COLOR_YELLOW; break;
		case BLEDevices::CONN_LOST: color = COLOR_RED; break;
		default: break;
		}
		lv_obj_set_style_bg_color(r.dot, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);

		const char* name = "";
		switch (r.slot) {
		case BLEDevices::DEV_CSC_1: name = s.cscKind == 1 ? "Speed" : s.cscKind == 2 ? "Kadenz" : "CSC 1"; break;
		case BLEDevices::DEV_CSC_2: name = s.cscKind == 1 ? "Speed" : s.cscKind == 2 ? "Kadenz" : "CSC 2"; break;
		case BLEDevices::DEV_HRM: name = "Puls"; break;
		case BLEDevices::DEV_NAV: name = "TrailBridge"; break;
		default: name = "Forumslader";
		}
		setText(r.name, name);

		char buf[8] = "";
		if (s.state == BLEDevices::CONN_CONNECTED && s.battery >= 0) snprintf(buf, sizeof(buf), "%d %%", s.battery);
		setText(r.battery, buf);
	}
	char sum[16];
	snprintf(sum, sizeof(sum), "%u von %u", connected, total);
	setText(objects.rrset_nav_dev_val, sum);
}

void action_go_to_settings_dev(lv_event_t* e) {
	(void) e;
	ui_RimRidgeDevUpdate();
	lv_disp_load_scr(objects.rim_ridge_settings_dev);
}

void action_dev_forget(lv_event_t* e) {
	const BLEDevices::EDevType slot = static_cast<BLEDevices::EDevType>((intptr_t) lv_event_get_user_data(e));
	if (bleDevs.requestForget(slot)) {
		// The dot goes grey with the next update, once the disconnect has gone through
		lv_label_set_text(objects.rrsd_hint, "vergessen - neues Gerät wird gesucht");
	}
}
