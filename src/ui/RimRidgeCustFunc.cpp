/*
 * RimRidgeCustFunc.cpp
 *
 * See RimRidgeCustFunc.h for scope notes.
 */

#include "ui_eez/screens.h"
#include "ui_eez/ui.h"
#include "RimRidgeCustFunc.h"
#include "BikeNavProtocol.h"
#include "ui/img/nav_icons.h"

void ui_RimRidgeUpdateSpeed(float speed) {
	lv_label_set_text_fmt(objects.rr_speed_val, "%.1f", speed);
	lv_arc_set_value(objects.rr_speed_arc, (int16_t) speed);
}

void ui_RimRidgeUpdateCadence(int16_t cadence) {
	lv_label_set_text_fmt(objects.rr_cadence_val, (cadence >= 0) ? "%d" : "-/-", cadence);
}

void ui_RimRidgeUpdateHR(int16_t hr) {
	lv_label_set_text_fmt(objects.rr_hr_val, (hr >= 0) ? "%d" : "-/-", hr);
	lv_bar_set_value(objects.rr_hr_bar, hr, LV_ANIM_OFF);
}

void ui_RimRidgeUpdateGrad(float grad, float height) {
	lv_label_set_text_fmt(objects.rr_gradient_val, "%+.1f%%", grad);
	lv_label_set_text_fmt(objects.rr_height_val, "%.0fm", height);
}

void ui_RimRidgeUpdateIntBatPerc(uint8_t perc) {
	// rr_battery_fill is a plain rect (not an lv_bar) sized to sit inside
	// the battery icon's outline - scale its width 0..9px for 0..100%,
	// left/top/height stay fixed (see build_rimridge.py / patch_rimridge_3).
	if (perc > 100) perc = 100;
	lv_obj_set_width(objects.rr_battery_fill, (perc * 9 + 50) / 100);
}

void ui_RimRidgeUpdateStats(const char* modeStr, const char* avgStr, float avgSpd, float maxSpd,
		float temperature, uint32_t dist, uint32_t timeInS) {
	(void) avgStr; (void) avgSpd; (void) maxSpd; (void) timeInS; // no widgets for these on RimRidge yet
	lv_label_set_text(objects.rr_tour_label, modeStr);
	lv_label_set_text_fmt(objects.rr_distance_val, "%.1f km", dist / 1000.0);
	lv_label_set_text_fmt(objects.rr_temp_val, "%.0f\xc2\xb0", temperature); // UTF-8 degree sign
}

void ui_RimRidgeUpdateNavDist(uint32_t dist) {
	if (dist < 1000) {
		lv_label_set_text_fmt(objects.rr_nav_dist, "%dm", dist);
	} else {
		lv_label_set_text_fmt(objects.rr_nav_dist, "%.1fkm", dist / 1000.0);
	}
}

void ui_RimRidgeUpdateNav(const char* navStr, uint32_t dist, uint8_t maneuver, uint8_t roundaboutExit) {
	(void) navStr;
	static uint8_t maneuverLast = 255, exitLast = 0;
	ui_RimRidgeUpdateNavDist(dist);
	// Reuses the same maneuver icon set as MainNoFL's ui_ImgNav
	// (navIcon64()) instead of RimRidge's own placeholder turn glyph -
	// navIcon64(NAV_MANEUVER_NONE, ...) already returns the "no nav" icon,
	// so no separate hide/show or special-casing is needed here.
	if (maneuver != maneuverLast || roundaboutExit != exitLast) {
		maneuverLast = maneuver;
		exitLast = roundaboutExit;
		lv_img_set_src(objects.rr_ic_turn, navIcon64(maneuver, roundaboutExit));
	}
}

void ui_RimRidgeUpdateGpsFix(bool hasFix, lv_color_t color) {
	if (hasFix) {
		lv_obj_set_style_img_recolor(objects.rr_ic_gps, color, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_img_recolor_opa(objects.rr_ic_gps, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	} else {
		lv_obj_add_flag(objects.rr_ic_gps, LV_OBJ_FLAG_HIDDEN);
		return;
	}
	lv_obj_clear_flag(objects.rr_ic_gps, LV_OBJ_FLAG_HIDDEN);
}

void ui_RimRidgeUpdateWiFiState(bool wifiEnabled, bool APModeActive, bool disableAPMode, uint8_t apStaCount) {
	// Only a simple show/hide for now - no RimRidge equivalent yet for
	// MainNoFL's AP-mode/client-count distinction (that lived on SWLAN only).
	(void) APModeActive; (void) disableAPMode; (void) apStaCount;
	if (wifiEnabled) {
		lv_obj_clear_flag(objects.rr_ic_wifi, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_add_flag(objects.rr_ic_wifi, LV_OBJ_FLAG_HIDDEN);
	}
}
