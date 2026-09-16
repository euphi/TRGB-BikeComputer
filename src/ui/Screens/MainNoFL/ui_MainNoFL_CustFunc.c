/*
 * ui_MainNoFL_CustFunc.c
 *
 *  Created on: 17.06.2023
 *      Author: ian
 */


#include <ui/Screens/MainNoFL/ui.h>
#include "ui_MainNoFL_CustFunc.h"

#include <ui/Screens/SNavi/ui.h>		// for nav images
#include "ui/img/nav_icons.h"
#include "ui/img/gps_icon.h"

// GPS fix status icon, left column above the driving-state icon (ui_ImgState sits at
// x=-180,y=-36; this continues that same column/spacing one slot up). Not part of the
// SquareLine project/ui_SMainNoFL.c -- created by hand in ui_SMainNoFLExtraInit() below,
// like the next-maneuver preview on the Navi screen (see ui_NaviCustFunc.c).
static lv_obj_t* ui_ImgGps;

void ui_SMainNoFLExtraInit(void) {
	ui_ImgGps = lv_img_create(ui_SMainNoFL);
	lv_img_set_src(ui_ImgGps, &gpsSatDish);
	lv_obj_set_width(ui_ImgGps, 64);
	lv_obj_set_height(ui_ImgGps, 64);
	lv_obj_set_x(ui_ImgGps, -180);
	lv_obj_set_y(ui_ImgGps, -108);
	lv_obj_set_align(ui_ImgGps, LV_ALIGN_CENTER);
	lv_obj_clear_flag(ui_ImgGps, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_set_style_img_recolor_opa(ui_ImgGps, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_add_flag(ui_ImgGps, LV_OBJ_FLAG_HIDDEN);		// hidden until the first GPS fix arrives
}

void ui_SMainNoFLUpdateGpsFix(bool hasFix, lv_color_t color) {
	if (hasFix) {
		lv_obj_set_style_img_recolor(ui_ImgGps, color, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_clear_flag(ui_ImgGps, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_add_flag(ui_ImgGps, LV_OBJ_FLAG_HIDDEN);
	}
}


void ui_SMainNoFLUpdateSpeed(float speed) {
	lv_label_set_text_fmt(ui_LabelSpeed, "%.1f", speed);
	lv_arc_set_value(ui_ArcSpeed, (uint16_t)(speed*10.0));

}
void ui_SMainNoFLUpdateCadence(int16_t cadence) {
	lv_label_set_text_fmt(ui_LabelCad, (cadence >= 0) ? "%drpm" : "-/-", cadence);
	lv_arc_set_value(ui_ArcCad, cadence);
}

void ui_SMainNoFLUpdateHR(int16_t hr) {
	lv_label_set_text_fmt(ui_LabelHR, (hr >= 0) ? "%d": "-/-", hr);
	lv_bar_set_value(ui_BarHR, hr, LV_ANIM_OFF);
}

void ui_SMainNoFLUpdateGrad(float grad, float height) {
	lv_label_set_text_fmt(ui_LabelGradient, "%.1f%%", grad);
	lv_label_set_text_fmt(ui_LabelHeight, "%.1fm", height);
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

void ui_SMainNoFLUpdateNav(const char* navStr, uint32_t dist, uint8_t maneuver, uint8_t roundaboutExit) {
	static uint8_t maneuverLast = 255, exitLast = 0;
	//lv_label_set_text(ui_SNavLabelStreet, navStr);
	ui_SMainNoFLUpdateNavDist(dist);
	if (maneuver != maneuverLast || roundaboutExit != exitLast) {
		maneuverLast = maneuver;
		exitLast = roundaboutExit;
		lv_img_set_src(ui_ImgNav, navIcon64(maneuver, roundaboutExit));
		if (maneuver != NAV_MANEUVER_NONE) {
		    lv_obj_clear_flag(ui_PanelNav, LV_OBJ_FLAG_HIDDEN);     /// Flags
		} else {	// NONE --> also happens once navigation is finished
		    lv_obj_add_flag(ui_PanelNav, LV_OBJ_FLAG_HIDDEN);     /// Flags
		}
	}
}

void ui_SMainNoFLUpdateIntBatPerc(uint8_t perc) {
	lv_bar_set_value(ui_BarBatt, perc, LV_ANIM_OFF);
}

void ui_SMainNoFLUpdateStats(const char* modeStr, const char* avgStr, float avgSpd, float maxSpd, float temperature, uint32_t dist, uint32_t timeInS) {
	lv_label_set_text_fmt(ui_LabelClockMode, "%s - %s", modeStr, avgStr);
	//lv_label_set_text_fmt(ui_S1PStatLspdMaxVar, "%.1f", maxSpd);
	lv_label_set_text_fmt(ui_LabelSpdAvg, "%.1fkm/h", avgSpd);
	lv_arc_set_value(ui_ArcAvg, (int16_t)(avgSpd * 10));
	lv_label_set_text_fmt(ui_LabelDist, "%.1fkm", dist/1000.0);
	lv_label_set_text_fmt(ui_LabelClock, "%02d:%02d:%02d", timeInS / 3600, (timeInS / 60) % 60, timeInS % 60);
	lv_label_set_text_fmt(ui_LabelTemp, "%.1f°C", temperature);
}

void ui_SMainNoFLUpdateStateIcon(const lv_img_dsc_t * pStateIcon, lv_color_t color) {
	lv_img_set_src(ui_ImgState, pStateIcon);
	lv_obj_set_style_img_recolor(ui_ImgState, color, LV_PART_MAIN | LV_STATE_DEFAULT);
}

