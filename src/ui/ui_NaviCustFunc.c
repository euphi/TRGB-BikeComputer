/*
 * ui_custFunc.c
 *
 *  Created on: 05.03.2023
 *      Author: ian
 */

#include "ui_Navi.h"

void ui_ScrNaviUpdateSpeed(float speed) {
	lv_label_set_text_fmt(ui_SNavLabelSpeed, "%.1f", speed);
	lv_arc_set_value(ui_SNavArcSpeed, (uint16_t)(speed*10.0));
}

void ui_ScrNaviUpdateCadence(int16_t cadence) {
	lv_label_set_text_fmt(ui_SNavLabelCad, (cadence >= 0) ? "%d rpm" : "-/-", cadence);
}

void ui_ScrNaviUpdateHR(int16_t hr) {
	lv_label_set_text_fmt(ui_SNavLabenHR, (hr >= 0) ? "%d bpm": "-/-", hr);
	lv_bar_set_value(ui_SNavBarHR, hr, LV_ANIM_OFF);
}


void ui_ScrNaviUpdateNav(const char* navStr, uint32_t dist, uint8_t dirCode) {
	lv_bar_set_value(ui_SNavBarNavDist, dist, LV_ANIM_OFF);
	lv_label_set_text(ui_SNavLabelStreet, navStr);
	lv_label_set_text_fmt(ui_SNavLabelNavDist, "%d m", dist);
	if (dirCode < 32) lv_img_set_src(ui_SNavImgNav, NavImgTable[dirCode]);		// FIXME: Inefficient to set it always
}
