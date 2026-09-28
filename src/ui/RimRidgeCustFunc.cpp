/*
 * RimRidgeCustFunc.cpp
 *
 * See RimRidgeCustFunc.h for scope notes.
 */

#include <math.h>

#include "ui_eez/screens.h"
#include "ui_eez/ui.h"
#include "ui_eez/actions.h"
#include "ui_eez/images.h"	// img_rr_icon_stopwatch/img_rr_icon_clock, referenced directly below
#include "RimRidgeCustFunc.h"
#include "BikeNavProtocol.h"
#include "ui/img/nav_icons.h"
#include "ui/img/lane_icon.h"
#include "ui/ui.h"	// driveStateUpdate()/UIDriveStateEvent/statModeNext()/statsTimeMode()
#include "Singletons.h"	// bclog, ui (UIFacade singleton)

void ui_RimRidgeUpdateSpeed(float speed) {
	// speed can be NAN (no speed sensor connected yet) - rr_speed_val uses
	// the digit-only by7x128 font (range 0x2B-0x39), which has no glyphs
	// for the letters in "nan" and would otherwise log LVGL glyph-missing
	// warnings every update. "-/-" only uses '-'/'/' (0x2D/0x2F), both in
	// range, matching the fallback convention MainNoFL used for cadence/HR.
	if (isnan(speed)) {
		lv_label_set_text(objects.rr_speed_val, "-/-");
		lv_arc_set_value(objects.rr_speed_arc, 0);
	} else {
		lv_label_set_text_fmt(objects.rr_speed_val, "%.1f", speed);
		lv_arc_set_value(objects.rr_speed_arc, (int16_t) speed);
	}
}

void ui_RimRidgeUpdateCadence(int16_t cadence) {
	lv_label_set_text_fmt(objects.rr_cadence_val, (cadence >= 0) ? "%d" : "-/-", cadence);
}

void ui_RimRidgeUpdateHR(int16_t hr) {
	lv_label_set_text_fmt(objects.rr_hr_val, (hr >= 0) ? "%d" : "-/-", hr);
	lv_bar_set_value(objects.rr_hr_bar, hr, LV_ANIM_OFF);
}

void ui_RimRidgeUpdateGrad(float grad, float height) {
	// rr_gradient_val/rr_height_val use a full-ASCII Montserrat font, so a
	// literal "nan%"/"nanm" wouldn't warn like rr_speed_val did, but it's
	// still not a fallback worth showing to the user.
	if (isnan(grad)) {
		lv_label_set_text(objects.rr_gradient_val, "-/-");
	} else {
		lv_label_set_text_fmt(objects.rr_gradient_val, "%+.1f%%", grad);
	}
	if (isnan(height)) {
		lv_label_set_text(objects.rr_height_val, "-/-");
	} else {
		lv_label_set_text_fmt(objects.rr_height_val, "%.0fm", height);
	}
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
	if (isnan(temperature)) {	// sensor not read yet right after boot
		lv_label_set_text(objects.rr_temp_val, "-/-");
	} else {
		lv_label_set_text_fmt(objects.rr_temp_val, "%.0f\xc2\xb0", temperature); // UTF-8 degree sign
	}
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
	// No route: "--" like RimRidgeNav, not "0m".
	if (maneuver == NAV_MANEUVER_NONE) {
		lv_label_set_text(objects.rr_nav_dist, "--");
	} else {
		ui_RimRidgeUpdateNavDist(dist);
	}
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

void ui_RimRidgeUpdateRoadQuality(uint8_t roadClass, float roughness, uint32_t shockCount) {
	// roughness/shockCount have no widget yet (see RimRidgeCustFunc.h) - only
	// the class drives rr_line_rq, a plain LVGLLineWidget the user added
	// directly in EEZ Studio (replaces an earlier "5 small squares" design
	// that never got built - see doc/design/rim-ridge-design-system.md).
	(void) roughness; (void) shockCount;
	// RRZoneBlue..RRZoneRed (theme_colors[26..30]) - same 5 tokens as the HR
	// zone band, reused here for the same calm->harsh color meaning. Hardcoded
	// hex rather than indexing theme_colors[] directly, matching how
	// ui_RimRidgeUpdateStateIcon()'s caller resolves RRBrass - this file has
	// no access to the generated theme_colors[]/active_theme_index globals.
	// Class 0 (standstill/no data) is NOT hidden - it shows the same RRBrass
	// neutral as the widget's own JSON default, so the line stays a constant
	// presence rather than flickering in and out with every red light.
	static const uint32_t ZONE_COLOR[5] = { 0x6C90B0, 0x6FA98C, 0xD7B463, 0xCE8A4C, 0xC1604A };
	uint32_t color = (roadClass >= 1 && roadClass <= 5) ? ZONE_COLOR[roadClass - 1] : 0xCBA36B;
	// Same line on every screen that carries the state icon/RQ line group.
	lv_obj_t* const lines[] = {objects.rr_line_rq, objects.rq_line_rq, objects.rrnav_line_rq, objects.rrset_line_rq};
	for (lv_obj_t* line : lines) {
		lv_obj_set_style_line_color(line, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_clear_flag(line, LV_OBJ_FLAG_HIDDEN);
	}

	// Same zone color/class drives the RQ-index digit on the RQ-Ride-Screen
	// (rq_rq_val) - "-" in the same neutral RRBrass for class 0, matching
	// rr_line_rq's "not hidden, shown neutral" convention above.
	if (roadClass >= 1 && roadClass <= 5) {
		lv_label_set_text_fmt(objects.rq_rq_val, "%d", roadClass);
	} else {
		lv_label_set_text(objects.rq_rq_val, "-");
	}
	lv_obj_set_style_text_color(objects.rq_rq_val, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
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

// EEZ Studio action, wired to rr_btn_pause's LONG_PRESSED event (see
// patch_rimridge_4_actions.py). Originally the fix for RimRidge's previously-
// missing way to suppress the auto-standby timeout (MainNoFL had this on
// ui_ImgState's long-press; RimRidge had no tappable element for it at all
// until now). Since 2026-09-28 (see doc/design/ride-state-machine.md §3) it
// ALSO ends an open ride session ("Stop") -- Statistics::
// handlePauseButtonHold() picks whichever meaning applies right now.
void action_pause_long_press(lv_event_t * e) {
	(void) e;
	driveStateUpdate(DSE_pauseButtonHold);
}

// EEZ Studio action, wired to rr_btn_pause's SHORT_CLICKED event (not CLICKED:
// LVGL 8 sends CLICKED after LONG_PRESSED too, which re-opened the session the
// long-press had just stopped) -- short tap
// toggles Ride<->FreeRide/Cruise (Statistics::toggleRideMode()), see
// doc/design/ride-state-machine.md §3/§4.
void action_pause_click(lv_event_t * e) {
	(void) e;
	driveStateUpdate(DSE_pauseButtonTap);
}

void ui_RimRidgeUpdateStateIcon(const lv_img_dsc_t* pIcon, lv_color_t color, bool rideMode) {
	// Same icon on every screen that carries the state icon/RQ line group.
	lv_obj_t* const icons[] = {objects.rr_ic_state, objects.rq_ic_state, objects.rrnav_ic_state, objects.rrset_ic_state};
	for (lv_obj_t* icon : icons) {
		if (pIcon == nullptr) {
			// DS_NO_CONN - no icon for it in the "Mainscreen-Studie" artifact
			lv_obj_add_flag(icon, LV_OBJ_FLAG_HIDDEN);
			continue;
		}
		lv_img_set_src(icon, pIcon);
		lv_obj_set_style_img_recolor(icon, color, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_img_recolor_opa(icon, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_clear_flag(icon, LV_OBJ_FLAG_HIDDEN);
	}

	// Pause/Start button glyph follows rideMode, not curDriveState directly -- Start-arrow
	// while not actively riding (FreeRide/Cruise, tap starts/resumes Ride), Pause glyph
	// while Ride/Coast (tap pauses into Cruise). See doc/design/ride-state-machine.md §3.
	static int8_t rideModeLast = -1;
	if ((int8_t) rideMode != rideModeLast) {
		rideModeLast = rideMode;
		lv_img_set_src(objects.rr_ic_pause, rideMode ? &img_rr_icon_pause : &img_rr_icon_start);
	}
}

void ui_RimRidgeUpdateTime(bool stopwatchMode, uint32_t elapsedS, const char* clockStr) {
	// img_recolor (brass) is baked into rr_ic_time's own JSON style - unlike
	// rr_ic_state above, the color here never varies, so no C override for
	// it (see add_rimridge_time_widget.py).
	static int8_t modeLast = -1;
	if ((int8_t) stopwatchMode != modeLast) {
		modeLast = stopwatchMode;
		lv_img_set_src(objects.rr_ic_time, stopwatchMode ? &img_rr_icon_stopwatch : &img_rr_icon_clock);
	}
	if (stopwatchMode) {
		uint32_t h = elapsedS / 3600, m = (elapsedS % 3600) / 60, s = elapsedS % 60;
		if (h > 0) {
			lv_label_set_text_fmt(objects.rr_time_val, "%u:%02u:%02u", (unsigned) h, (unsigned) m, (unsigned) s);
		} else {
			lv_label_set_text_fmt(objects.rr_time_val, "%u:%02u", (unsigned) m, (unsigned) s);
		}
	} else {
		lv_label_set_text(objects.rr_time_val, clockStr);
	}
}

// EEZ Studio action, wired to rr_tour_pill's GESTURE event (see
// patch_rimridge_5_state_container.py). Reuses statModeNext()/
// statsTimeMode() from ui_events.cpp - same direction mapping MainNoFL's
// ui_PanelClock used (see ui/Screens/MainNoFL/ui.c): left/right cycles the
// distance-summary mode (Tour/Trip/...), top/bottom cycles the time-average
// mode (stop/pause/total).
// EEZ Studio action, wired to rr_nav_pill's CLICKED event - manually
// opens RimRidgeNav (independent of updateNavi()'s own auto-popup, works
// even with no active route - see UIFacade::showNavScreen()).
void action_go_to_nav(lv_event_t * e) {
	(void) e;
	ui.showNavScreen();
}

// EEZ Studio action, wired to the CLICKED event of the state icon/RQ line
// group - rr_group_rq_mode and its copies on RimRidgeRQ, RimRidgeNav and
// RimRidgeSettings. Opens the RQ-Ride-Screen (see UIFacade::showRQScreen());
// on the RQ screen itself it toggles back to the main screen, a tap being
// easier to hit than a swipe on a bumpy road.
void action_go_to_rq(lv_event_t * e) {
	(void) e;
	if (lv_scr_act() == objects.rim_ridge_rq) {
		ui.hideRQScreen();
	} else {
		ui.showRQScreen();
	}
}

// rr_nav_pill's two positions - REST is this project's shipped, EEZ-
// authored (140,85,200,78); SHOWN only exists here in C, see
// ui_RimRidgeUpdateLanes()'s doc comment in RimRidgeCustFunc.h for why.
#define RR_NAV_PILL_REST_X   140
#define RR_NAV_PILL_REST_W   200
#define RR_NAV_PILL_SHOWN_X  184
#define RR_NAV_PILL_SHOWN_W  185
#define RR_NAV_PILL_SLIDE_MS 300

static void rrAnimatePillTo(lv_coord_t targetX, lv_coord_t targetW) {
	lv_anim_t ax;
	lv_anim_init(&ax);
	lv_anim_set_var(&ax, objects.rr_nav_pill);
	lv_anim_set_exec_cb(&ax, (lv_anim_exec_xcb_t) lv_obj_set_x);
	lv_anim_set_values(&ax, lv_obj_get_x(objects.rr_nav_pill), targetX);
	lv_anim_set_time(&ax, RR_NAV_PILL_SLIDE_MS);
	lv_anim_set_path_cb(&ax, lv_anim_path_ease_out);
	lv_anim_start(&ax);

	lv_anim_t aw;
	lv_anim_init(&aw);
	lv_anim_set_var(&aw, objects.rr_nav_pill);
	lv_anim_set_exec_cb(&aw, (lv_anim_exec_xcb_t) lv_obj_set_width);
	lv_anim_set_values(&aw, lv_obj_get_width(objects.rr_nav_pill), targetW);
	lv_anim_set_time(&aw, RR_NAV_PILL_SLIDE_MS);
	lv_anim_set_path_cb(&aw, lv_anim_path_ease_out);
	lv_anim_start(&aw);
}

void ui_RimRidgeUpdateLanes(const NavLane* lanes, uint8_t laneCount) {
	static bool lanesShown = false;
	static bool initialized = false;

	if (!initialized) {
		// rr_lane_row starts VISIBLE in the EEZ canvas (showing its inert
		// placeholder bitmap) - the laneCount==0 branch below only reacts
		// to a shown->not-shown TRANSITION (lanesShown was already true),
		// so without this it would never actually get hidden if no real
		// lane data ever arrives to trigger that transition. Runs once,
		// regardless of the very first call's laneCount, so the widget
		// always starts from a genuinely hidden state before anything
		// else below decides whether to show it again.
		initialized = true;
		lv_obj_add_flag(objects.rr_lane_row, LV_OBJ_FLAG_HIDDEN);
	}

	if (laneCount > 0) {
		// RimRidge's compact chip caps at 3 tiles, 80x50 box (widened
		// vertically from 80x40 2026-09-20, user's own call, to allow
		// bigger tiles without needing to stay square - see MAIN_PROFILE
		// in lane_icon.c) - see lane_icon.h's lane_row_get() doc comment
		// and patch_rimridge_lane_row.py's pixel-math worksheet for the
		// box's position.
		lv_img_set_src(objects.rr_lane_row, lane_row_get(lanes, laneCount, 3, 80, 50));
		if (!lanesShown) {
			lanesShown = true;
			lv_obj_clear_flag(objects.rr_lane_row, LV_OBJ_FLAG_HIDDEN);
			lv_obj_fade_in(objects.rr_lane_row, RR_NAV_PILL_SLIDE_MS, 0);
			rrAnimatePillTo(RR_NAV_PILL_SHOWN_X, RR_NAV_PILL_SHOWN_W);
		}
	} else if (lanesShown) {
		lanesShown = false;
		lv_obj_fade_out(objects.rr_lane_row, RR_NAV_PILL_SLIDE_MS, 0); // sets HIDDEN once opacity hits 0
		rrAnimatePillTo(RR_NAV_PILL_REST_X, RR_NAV_PILL_REST_W);
	}
}

void action_tour_pill_gesture(lv_event_t * e) {
	(void) e;
	lv_indev_t * indev = lv_indev_get_act();
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_UI, "rr_tour_pill GESTURE fired, indev=%p", (void*) indev);
	if (!indev) return;
	lv_dir_t dir = lv_indev_get_gesture_dir(indev);
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_UI, "rr_tour_pill gesture dir=%d", (int) dir);
	lv_indev_wait_release(indev);
	switch (dir) {
	case LV_DIR_LEFT:
		statModeNext(false);
		break;
	case LV_DIR_RIGHT:
		statModeNext(true);
		break;
	case LV_DIR_TOP:
		statsTimeMode(true);
		break;
	case LV_DIR_BOTTOM:
		statsTimeMode(false);
		break;
	default:
		break;
	}
}
