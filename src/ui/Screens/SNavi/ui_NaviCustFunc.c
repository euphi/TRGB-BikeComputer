/*
 * ui_custFunc.c
 *
 *  Created on: 05.03.2023
 *      Author: ian
 */

#include <ui/Screens/SNavi/ui.h>
#include "ui.h"
#include "ui/img/nav_icons.h"
#include "ui/font/font.h"

// Small preview of the maneuver *after* the current one, hung into the
// otherwise-empty ui_SNavPanelStat panel on the left of the screen (see
// ui_ScrNaviExtraInit()). Not part of the SquareLine-generated ui_SNavi.c.
static lv_obj_t* ui_SNavImgNextManeuver;
static lv_obj_t* ui_SNavLabelNextDist;
static lv_obj_t* ui_SNavLabelNextStreet;

// Remaining distance/time to destination, shown below the big current-maneuver
// icon (ui_SNavImgNav). Screen-level widget, not part of the left panel.
static lv_obj_t* ui_SNavLabelRemaining;

void ui_ScrNaviExtraInit(void) {
	ui_SNavImgNextManeuver = lv_img_create(ui_SNavPanelStat);
	lv_img_set_src(ui_SNavImgNextManeuver, navIcon64(NAV_MANEUVER_NONE, 0));
	lv_obj_set_width(ui_SNavImgNextManeuver, 64);
	lv_obj_set_height(ui_SNavImgNextManeuver, 64);
	lv_obj_set_align(ui_SNavImgNextManeuver, LV_ALIGN_TOP_MID);
	lv_obj_set_y(ui_SNavImgNextManeuver, 10);
	lv_obj_clear_flag(ui_SNavImgNextManeuver, LV_OBJ_FLAG_SCROLLABLE);
	// Same light backdrop as the main nav icon (ui_SNavImgNav in ui_SNavi.c) --
	// the icon artwork is dark/black, invisible on the panel's dark theme bg otherwise.
	lv_obj_set_style_bg_color(ui_SNavImgNextManeuver, lv_color_hex(0xAAAAAA), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(ui_SNavImgNextManeuver, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_add_flag(ui_SNavImgNextManeuver, LV_OBJ_FLAG_HIDDEN);

	ui_SNavLabelNextDist = lv_label_create(ui_SNavPanelStat);
	lv_obj_set_width(ui_SNavLabelNextDist, LV_SIZE_CONTENT);
	lv_obj_set_height(ui_SNavLabelNextDist, LV_SIZE_CONTENT);
	lv_obj_set_align(ui_SNavLabelNextDist, LV_ALIGN_TOP_MID);
	lv_obj_set_y(ui_SNavLabelNextDist, 10 + 64 + 4);
	lv_obj_set_style_text_font(ui_SNavLabelNextDist, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_label_set_text(ui_SNavLabelNextDist, "");
	lv_obj_add_flag(ui_SNavLabelNextDist, LV_OBJ_FLAG_HIDDEN);

	ui_SNavLabelNextStreet = lv_label_create(ui_SNavPanelStat);
	lv_obj_set_width(ui_SNavLabelNextStreet, 100);
	lv_obj_set_height(ui_SNavLabelNextStreet, LV_SIZE_CONTENT);
	lv_obj_set_align(ui_SNavLabelNextStreet, LV_ALIGN_TOP_MID);
	lv_obj_set_y(ui_SNavLabelNextStreet, 10 + 64 + 4 + 18 + 4);
	lv_label_set_long_mode(ui_SNavLabelNextStreet, LV_LABEL_LONG_WRAP);
	lv_obj_set_style_text_align(ui_SNavLabelNextStreet, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
	// SchildGrot, not montserrat: needs to render German umlauts/ß in street names,
	// which LVGL's built-in montserrat fonts don't include.
	lv_obj_set_style_text_font(ui_SNavLabelNextStreet, &ui_font_SchildGrot18, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_label_set_text(ui_SNavLabelNextStreet, "");
	lv_obj_add_flag(ui_SNavLabelNextStreet, LV_OBJ_FLAG_HIDDEN);

	ui_SNavLabelRemaining = lv_label_create(ui_SNavi);
	lv_obj_set_width(ui_SNavLabelRemaining, LV_SIZE_CONTENT);
	lv_obj_set_height(ui_SNavLabelRemaining, LV_SIZE_CONTENT);
	lv_obj_set_align(ui_SNavLabelRemaining, LV_ALIGN_CENTER);
	lv_obj_set_x(ui_SNavLabelRemaining, 0);
	lv_obj_set_y(ui_SNavLabelRemaining, 115);
	lv_obj_set_style_text_align(ui_SNavLabelRemaining, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(ui_SNavLabelRemaining, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_label_set_text(ui_SNavLabelRemaining, "");
	lv_obj_add_flag(ui_SNavLabelRemaining, LV_OBJ_FLAG_HIDDEN);
}

void ui_ScrNaviSetBackScreen(lv_obj_t* const screenBack) {
	ui_SNavi_SBack = screenBack;
}

void ui_ScrNaviGoBack(void) {
    _ui_screen_change(ui_SNavi_SBack, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0);
}


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
			lv_obj_set_style_img_recolor(ui_SNavImgNav, lv_color_hex(0x800080), LV_PART_MAIN | LV_STATE_DEFAULT);		//violett

		} else if (dist < 60) {
			lv_obj_set_style_img_recolor(ui_S1ImgNav, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);			//red
			lv_obj_set_style_img_recolor(ui_SNavImgNav, lv_color_hex(0xFF0000), LV_PART_MAIN | LV_STATE_DEFAULT);		//red
		} else if (dist < 100) {
			lv_obj_set_style_img_recolor(ui_S1ImgNav, lv_color_hex(0xFF8000), LV_PART_MAIN | LV_STATE_DEFAULT);			//orange
			lv_obj_set_style_img_recolor(ui_SNavImgNav, lv_color_hex(0xFF8000), LV_PART_MAIN | LV_STATE_DEFAULT);		//orange
		} else if (dist < 250) {
			lv_obj_set_style_img_recolor(ui_S1ImgNav, lv_color_hex(0x808000), LV_PART_MAIN | LV_STATE_DEFAULT);			//dark yellow
			lv_obj_set_style_img_recolor(ui_SNavImgNav, lv_color_hex(0x808000), LV_PART_MAIN | LV_STATE_DEFAULT);		//dark yellow
		} else {
			lv_obj_set_style_img_recolor(ui_S1ImgNav, lv_color_hex(0x008000), LV_PART_MAIN | LV_STATE_DEFAULT);			//half-dark green
			lv_obj_set_style_img_recolor(ui_SNavImgNav, lv_color_hex(0x008000), LV_PART_MAIN | LV_STATE_DEFAULT);		//half-dark green
		}
	} else {
		lv_label_set_text_fmt(ui_SNavLabelNavDist, "%.1f km", dist/1000.0);
		lv_label_set_text_fmt(ui_S1LabelNav, "%.1f km", dist/1000.0);
		lv_bar_set_value(ui_SNavBarNavDist, 100, LV_ANIM_OFF);
		lv_bar_set_value(ui_S1BarNavDist, 100, LV_ANIM_OFF);
		lv_obj_set_style_img_recolor(ui_S1ImgNav, lv_color_hex(000000), LV_PART_MAIN | LV_STATE_DEFAULT);			//black
	}
}

void ui_ScrNaviUpdateNav(const char* navStr, uint32_t dist, uint8_t maneuver, uint8_t roundaboutExit,
		uint8_t nextManeuver, uint32_t nextManeuverDist, const char* nextStreet,
		uint32_t remainingDist, uint32_t remainingTime) {
	static uint8_t maneuverLast = 255, exitLast = 0;
	static uint8_t nextManeuverLast = 255;
	lv_label_set_text(ui_SNavLabelStreet, navStr);
	ui_ScrNaviUpdateNavDist(dist);
	if (maneuver != maneuverLast || roundaboutExit != exitLast) {
		maneuverLast = maneuver;
		exitLast = roundaboutExit;
		lv_img_set_src(ui_SNavImgNav, navIconLarge(maneuver, roundaboutExit));
		lv_img_set_src(ui_S1ImgNav, navIcon64(maneuver, roundaboutExit));
		if (maneuver != NAV_MANEUVER_NONE) {
		    lv_obj_clear_flag(ui_S1PanelNav, LV_OBJ_FLAG_HIDDEN);     /// Flags
		} else {	// NONE --> also happens once navigation is finished
		    lv_obj_add_flag(ui_S1PanelNav, LV_OBJ_FLAG_HIDDEN);     /// Flags
		}
	}

	if (nextManeuver != NAV_MANEUVER_NONE && nextManeuver != NAV_MANEUVER_UNKNOWN) {
		if (nextManeuver != nextManeuverLast) {
			nextManeuverLast = nextManeuver;
			lv_img_set_src(ui_SNavImgNextManeuver, navIcon64(nextManeuver, 0));
		}
		lv_label_set_text_fmt(ui_SNavLabelNextDist, "%u m", (unsigned)nextManeuverDist);
		lv_label_set_text(ui_SNavLabelNextStreet, nextStreet);
		lv_obj_clear_flag(ui_SNavImgNextManeuver, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(ui_SNavLabelNextDist, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(ui_SNavLabelNextStreet, LV_OBJ_FLAG_HIDDEN);
	} else {
		nextManeuverLast = 255;
		lv_obj_add_flag(ui_SNavImgNextManeuver, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(ui_SNavLabelNextDist, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(ui_SNavLabelNextStreet, LV_OBJ_FLAG_HIDDEN);
	}

	if (maneuver != NAV_MANEUVER_NONE) {
		if (remainingDist < 1500) {
			lv_label_set_text_fmt(ui_SNavLabelRemaining, "%u m, %u min", (unsigned)remainingDist, (unsigned)(remainingTime / 60));
		} else {
			lv_label_set_text_fmt(ui_SNavLabelRemaining, "%.1f km, %u min", remainingDist / 1000.0, (unsigned)(remainingTime / 60));
		}
		lv_obj_clear_flag(ui_SNavLabelRemaining, LV_OBJ_FLAG_HIDDEN);
	} else {
		lv_obj_add_flag(ui_SNavLabelRemaining, LV_OBJ_FLAG_HIDDEN);
	}
}
