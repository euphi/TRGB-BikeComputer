/*
 * RimRidgeRQCustFunc.cpp
 *
 * See RimRidgeRQCustFunc.h for scope notes.
 */

#include <math.h>

#include "ui_eez/screens.h"
#include "ui_eez/ui.h"
#include "ui_eez/actions.h"
#include "RimRidgeRQCustFunc.h"
#include "ui/img/nav_icons.h"
#include "Singletons.h"	// ui (UIFacade singleton)

void ui_RimRidgeRQUpdateSpeed(float speed) {
	// Same NAN convention as ui_RimRidgeUpdateSpeed() - rq_speed_val is a
	// full-ASCII Montserrat font here (not the digit-only by7x128), so
	// "-/-" isn't strictly required to avoid glyph warnings, but kept for
	// consistency with the rest of the UI's fallback display.
	if (isnan(speed)) {
		lv_label_set_text(objects.rq_speed_val, "-/-");
	} else {
		lv_label_set_text_fmt(objects.rq_speed_val, "%.1f", speed);
	}
}

void ui_RimRidgeRQUpdateHR(int16_t hr) {
	lv_label_set_text_fmt(objects.rq_hr_val, (hr >= 0) ? "%d" : "-/-", hr);
}

void ui_RimRidgeRQUpdateDist(uint32_t distM) {
	lv_label_set_text_fmt(objects.rq_dist_val, "%.1f km", distM / 1000.0);
}

void ui_RimRidgeRQUpdateNavDist(uint32_t dist) {
	if (dist < 1000) {
		lv_label_set_text_fmt(objects.rq_nav_dist, "%dm", dist);
	} else {
		lv_label_set_text_fmt(objects.rq_nav_dist, "%.1fkm", dist / 1000.0);
	}
}

void ui_RimRidgeRQUpdateNav(uint32_t dist, uint8_t maneuver, uint8_t roundaboutExit) {
	static uint8_t maneuverLast = 255, exitLast = 0;
	ui_RimRidgeRQUpdateNavDist(dist);
	if (maneuver != maneuverLast || roundaboutExit != exitLast) {
		maneuverLast = maneuver;
		exitLast = roundaboutExit;
		lv_img_set_src(objects.rq_ic_turn, navIcon64(maneuver, roundaboutExit));
	}
}

// EEZ Studio action, wired to the RimRidgeRQ screen root's GESTURE event -
// any swipe returns to the main screen (see UIFacade::hideRQScreen()).
// Unlike RimRidgeNav's action_nav_screen_gesture(), no direction check:
// this screen has no other gesture-driven behavior to avoid colliding
// with, so any swipe is unambiguous.
void action_rq_screen_gesture(lv_event_t * e) {
	(void) e;
	lv_indev_t * indev = lv_indev_get_act();
	if (!indev) return;
	lv_indev_wait_release(indev);
	ui.hideRQScreen();
}
