/*
 * ui_custFunc.c
 *
 *  Created on: 05.03.2023
 *      Author: ian
 */

#include "ui_Navi.h"
#include "ui.h"

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

void ui_ScrNaviUpdateNavDist(uint32_t dist) {
	if (dist < 1500) {
		lv_label_set_text_fmt(ui_SNavLabelNavDist, "%d m", dist);
		lv_label_set_text_fmt(ui_S1LabelNav, "%d m", dist);
		uint8_t perc = (dist > 250) ? 100 : ((dist * 100) / 250);
		lv_bar_set_value(ui_SNavBarNavDist, perc, LV_ANIM_ON);
		lv_bar_set_value(ui_S1BarNavDist, perc, LV_ANIM_ON);
		if (dist < 15) {
			lv_obj_set_style_img_recolor(ui_S1ImgNav, lv_color_hex(0x800080), LV_PART_MAIN | LV_STATE_DEFAULT);			//violett
		} else if (dist < 60) {
			lv_obj_set_style_img_recolor(ui_S1ImgNav, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);			//red
		} else if (dist < 100) {
			lv_obj_set_style_img_recolor(ui_S1ImgNav, lv_color_hex(0xFF8000), LV_PART_MAIN | LV_STATE_DEFAULT);			//orange
		} else if (dist < 250) {
			lv_obj_set_style_img_recolor(ui_S1ImgNav, lv_color_hex(0x808000), LV_PART_MAIN | LV_STATE_DEFAULT);			//dark yellow
		} else {
			lv_obj_set_style_img_recolor(ui_S1ImgNav, lv_color_hex(0x008000), LV_PART_MAIN | LV_STATE_DEFAULT);			//half-dark green
		}
	} else {
		lv_label_set_text_fmt(ui_SNavLabelNavDist, "%.1f km", dist/1000.0);
		lv_label_set_text_fmt(ui_S1LabelNav, "%.1f km", dist/1000.0);
		lv_bar_set_value(ui_SNavBarNavDist, 100, LV_ANIM_OFF);
		lv_bar_set_value(ui_S1BarNavDist, 100, LV_ANIM_OFF);
		lv_obj_set_style_img_recolor(ui_S1ImgNav, lv_color_hex(000000), LV_PART_MAIN | LV_STATE_DEFAULT);			//black
	}
}

void ui_ScrNaviUpdateNav(const char* navStr, uint32_t dist, uint8_t dirCode) {
	static uint8_t dirCodeLast = 255;
	lv_label_set_text(ui_SNavLabelStreet, navStr);
	ui_ScrNaviUpdateNavDist(dist);
	if (dirCode != dirCodeLast) {
		dirCodeLast = dirCode;
		if (dirCode < 32) {
			lv_img_set_src(ui_SNavImgNav, NavImgTable[dirCode]);
			lv_img_set_src(ui_S1ImgNav, Nav64ImgTable[dirCode]);
			if (dirCode > 0) {
			    lv_obj_clear_flag(ui_S1PanelNav, LV_OBJ_FLAG_HIDDEN);     /// Flags
			} else {	 // Code = 0 --> unknown, also happens if Navigation is finished
			    lv_obj_add_flag(ui_S1PanelNav, LV_OBJ_FLAG_HIDDEN);     /// Flags
			}
		}
	}
}
