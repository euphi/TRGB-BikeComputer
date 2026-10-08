/*
 * RimRidgeNavCustFunc.cpp
 *
 * See RimRidgeNavCustFunc.h for scope notes.
 */

#include <math.h>

#include "ui_eez/screens.h"
#include "ui_eez/ui.h"
#include "ui_eez/actions.h"
#include "RimRidgeNavCustFunc.h"
#include "BikeNavProtocol.h"
#include "ui/img/nav_icons.h"
#include "ui/img/lane_icon.h"
#include "Singletons.h"	// ui (UIFacade singleton)

static void formatDist(lv_obj_t* label, uint32_t dist) {
	if (dist < 1000) {
		lv_label_set_text_fmt(label, "%dm", dist);
	} else {
		lv_label_set_text_fmt(label, "%.1fkm", dist / 1000.0);
	}
}

void ui_RimRidgeNavUpdateNav(uint32_t dist, uint8_t maneuver, uint8_t roundaboutExit,
		const char* street, uint8_t nextManeuver, uint32_t nextManeuverDist) {
	static uint8_t maneuverLast = 255, exitLast = 0;
	if (maneuver != maneuverLast || roundaboutExit != exitLast) {
		maneuverLast = maneuver;
		exitLast = roundaboutExit;
		lv_img_set_src(objects.rrnav_ic_turn, navIconLarge(maneuver, roundaboutExit));
	}

	// No active route - navIconLarge() above already resolved to the "no
	// nav" icon; blank distance/street/arc too instead of leaving whatever
	// example content EEZ Studio's canvas baked in (this screen has no
	// per-field boot-time reset otherwise, unlike RimRidge/RimRidgeRQ's own
	// rr_ic_turn/rq_ic_turn init calls - see UIFacade::initDisplay()).
	if (maneuver == NAV_MANEUVER_NONE) {
		lv_label_set_text(objects.rrnav_dist_val, "--");
		lv_label_set_text(objects.rrnav_street, "--");
		lv_arc_set_value(objects.rrnav_dist_arc, 0);
	} else {
		formatDist(objects.rrnav_dist_val, dist);
		lv_label_set_text(objects.rrnav_street, street);

		// Distance-to-maneuver arc: counts DOWN from 100 to 0 as the maneuver
		// gets closer (2026-09-19 user feedback - the earlier "fills up"
		// version, using the maneuver's first-seen distance as a dynamic
		// reference, read as unintuitive). Stays pinned at 100 beyond
		// NAV_ARC_COUNTDOWN_START_M - the countdown only means something once
		// the maneuver is close enough to matter.
		static const uint32_t NAV_ARC_COUNTDOWN_START_M = 250;
		int32_t pct = 100;
		if (dist < NAV_ARC_COUNTDOWN_START_M) {
			pct = (int32_t) (dist * 100 / NAV_ARC_COUNTDOWN_START_M);
			if (pct < 0) pct = 0;
			if (pct > 100) pct = 100;
		}
		lv_arc_set_value(objects.rrnav_dist_arc, pct);
	}

	// Next-maneuver mini badge - hidden when there is none, matching the
	// old SNavi screen's ui_ScrNaviUpdateNav precedent.
	static uint8_t nextManeuverLast = 255;
	if (nextManeuver != NAV_MANEUVER_NONE && nextManeuver != NAV_MANEUVER_UNKNOWN) {
		if (nextManeuver != nextManeuverLast) {
			nextManeuverLast = nextManeuver;
			lv_img_set_src(objects.rrnav_ic_next, navIcon64(nextManeuver, 0));
		}
		formatDist(objects.rrnav_next_dist, nextManeuverDist);
		lv_obj_clear_flag(objects.rrnav_ic_next, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(objects.rrnav_next_dist, LV_OBJ_FLAG_HIDDEN);
	} else {
		nextManeuverLast = 255;
		lv_obj_add_flag(objects.rrnav_ic_next, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(objects.rrnav_next_dist, LV_OBJ_FLAG_HIDDEN);
	}
}

void ui_RimRidgeNavUpdateSpeed(float speed) {
	if (isnan(speed)) {
		lv_label_set_text(objects.rrnav_speed_val, "-/-");
	} else {
		lv_label_set_text_fmt(objects.rrnav_speed_val, "%.1f", speed);
	}
}

void ui_RimRidgeNavUpdateGrad(float grad) {
	if (isnan(grad)) {
		lv_label_set_text(objects.rrnav_gradient_val, "-/-");
	} else {
		lv_label_set_text_fmt(objects.rrnav_gradient_val, "%+.1f%%", grad);
	}
}

void ui_RimRidgeNavUpdateHR(int16_t hr) {
	lv_label_set_text_fmt(objects.rrnav_hr_val, (hr >= 0) ? "%d" : "-/-", hr);
}

void ui_RimRidgeNavUpdateLanes(const NavLane* lanes, uint8_t laneCount) {
	static bool lanesShown = false;
	static bool initialized = false;

	if (!initialized) {
		// rrnav_lane_row starts VISIBLE in the EEZ canvas - see
		// ui_RimRidgeUpdateLanes()'s identical fix in RimRidgeCustFunc.cpp
		// for the full rationale.
		initialized = true;
		lv_obj_add_flag(objects.rrnav_lane_row, LV_OBJ_FLAG_HIDDEN);
	}

	if (laneCount > 0) {
		lv_img_set_src(objects.rrnav_lane_row, lane_row_get(lanes, laneCount, 5, 280, 50));
		if (!lanesShown) {
			lanesShown = true;
			lv_obj_clear_flag(objects.rrnav_lane_row, LV_OBJ_FLAG_HIDDEN);
			lv_obj_fade_in(objects.rrnav_lane_row, 300, 0);
		}
	} else if (lanesShown) {
		lanesShown = false;
		lv_obj_fade_out(objects.rrnav_lane_row, 300, 0); // sets HIDDEN once opacity hits 0
	}
}

// EEZ Studio action, wired to the RimRidgeNav screen root's GESTURE event
// - a left-to-right swipe (LV_DIR_RIGHT) manually returns to the main
// screen (see UIFacade::hideNavScreen()), a right-to-left one (LV_DIR_LEFT)
// opens the route overview (UIFacade::showRouteScreen()). Other directions ignored.
void action_nav_screen_gesture(lv_event_t * e) {
	(void) e;
	lv_indev_t * indev = lv_indev_get_act();
	if (!indev) return;
	lv_dir_t dir = lv_indev_get_gesture_dir(indev);
	lv_indev_wait_release(indev);
	if (dir == LV_DIR_RIGHT) {
		ui.hideNavScreen();
	} else if (dir == LV_DIR_LEFT) {
		ui.showRouteScreen();
	}
}
