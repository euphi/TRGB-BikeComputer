/*
 * ui_MainNoFL_CustFunc.c
 *
 *  Created on: 17.06.2023
 *      Author: ian
 */


#include "ui_MainNoFL.h"

#include "ui_MainNoFL_CustFunc.h"

#include "ui/ui_Navi.h"  // for nav images

void ui_SMainNoFLUpdateSpeed(float speed) {
	lv_label_set_text_fmt(ui_LabelSpeed, "%.1f", speed);
	lv_arc_set_value(ui_ArcSpeed, (uint16_t)(speed*10.0));

}
void ui_SMainNoFLUpdateCadence(int16_t cadence) {
	lv_label_set_text_fmt(ui_LabelCad, (cadence >= 0) ? "%d rpm" : "-/-", cadence);
	lv_arc_set_value(ui_ArcCad, cadence);
}

void ui_SMainNoFLUpdateHR(int16_t hr) {
	lv_label_set_text_fmt(ui_LabelHR, (hr >= 0) ? "%d": "-/-", hr);
	lv_bar_set_value(ui_BarHR, hr, LV_ANIM_OFF);
}

void ui_SMainNoFLUpdateNavDist(uint32_t dist) {
	if (dist < 1500) {
		lv_label_set_text_fmt(ui_LabelNavDist, "%dm", dist);
		uint8_t perc = (dist > 250) ? 100 : ((dist * 100) / 250);
		lv_bar_set_value(ui_BarNav, perc, LV_ANIM_ON);
		if (dist < 15) {
			lv_obj_set_style_img_recolor(ui_ImgNav, lv_color_hex(0x800080), LV_PART_MAIN | LV_STATE_DEFAULT);			//violett
		} else if (dist < 60) {
			lv_obj_set_style_img_recolor(ui_ImgNav, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);			//red
		} else if (dist < 100) {
			lv_obj_set_style_img_recolor(ui_ImgNav, lv_color_hex(0xFF8000), LV_PART_MAIN | LV_STATE_DEFAULT);			//orange
		} else if (dist < 250) {
			lv_obj_set_style_img_recolor(ui_ImgNav, lv_color_hex(0x808000), LV_PART_MAIN | LV_STATE_DEFAULT);			//dark yellow
		} else {
			lv_obj_set_style_img_recolor(ui_ImgNav, lv_color_hex(0x008000), LV_PART_MAIN | LV_STATE_DEFAULT);			//half-dark green
		}
	} else {
		lv_label_set_text_fmt(ui_LabelNavDist, "%.1fkm", dist/1000.0);
		lv_bar_set_value(ui_BarNav, 100, LV_ANIM_OFF);
		lv_obj_set_style_img_recolor(ui_ImgNav, lv_color_hex(000000), LV_PART_MAIN | LV_STATE_DEFAULT);			//black
	}
}

void ui_SMainNoFLUpdateNav(const char* navStr, uint32_t dist, uint8_t dirCode) {
	static uint8_t dirCodeLast = 255;
	//lv_label_set_text(ui_SNavLabelStreet, navStr);
	ui_SMainNoFLUpdateNavDist(dist);
	if (dirCode != dirCodeLast) {
		dirCodeLast = dirCode;
		if (dirCode < 32) {
			lv_img_set_src(ui_ImgNav, Nav64ImgTable[dirCode]);
			if (dirCode > 0) {
			    lv_obj_clear_flag(ui_PanelNav, LV_OBJ_FLAG_HIDDEN);     /// Flags
			} else {	 // Code = 0 --> unknown, also happens if Navigation is finished
			    lv_obj_add_flag(ui_PanelNav, LV_OBJ_FLAG_HIDDEN);     /// Flags
			}
		}
	}
}

void ui_SMainNoFLUpdateIntBatPerc(uint8_t perc) {
	lv_bar_set_value(ui_BarBatt, perc, LV_ANIM_OFF);
}


void ui_SMainNoFLUpdateStats(const char* modeStr, const char* avgStr, float avgSpd, float maxSpd, uint32_t dist, uint32_t timeInS) {
	lv_label_set_text_fmt(ui_LabelClockMode, "%s - %s", modeStr, avgStr);
	//lv_label_set_text_fmt(ui_S1PStatLspdMaxVar, "%.1f", maxSpd);
	//lv_label_set_text_fmt(ui_S1PStatLAvgVar, "%.1f", avgSpd);
	lv_arc_set_value(ui_ArcAvg, (int16_t)(avgSpd * 10));
	lv_label_set_text_fmt(ui_LabelDist, "%.1fkm", dist/1000.0);
	lv_label_set_text_fmt(ui_LabelClock, "%02d:%02d:%02d", timeInS / 3600, (timeInS / 60) % 60, timeInS % 60);
}

