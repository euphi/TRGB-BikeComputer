/*
 * RimRidgeRQCustFunc.cpp
 *
 * See RimRidgeRQCustFunc.h for scope notes.
 */

#include <math.h>
#include <stdint.h>

#include "ui_eez/screens.h"
#include "ui_eez/ui.h"
#include "ui_eez/actions.h"
#include "RimRidgeRQCustFunc.h"
#include "ui/img/nav_icons.h"
#include "Singletons.h"	// ui (UIFacade singleton), sensors (TRGBBC_SENSORS_I2C only)

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

// EEZ Studio makes every widget clickable by default, so a child sitting on
// top of a clickable container (label text, an icon, ...) would otherwise
// swallow the tap itself instead of it reaching the container's own event
// handler - same bug and fix as rr_group_rq_mode's rr_ic_state/rr_line_rq
// (RimRidgeCustFunc.cpp), just applied here at init time in C instead of
// via an EEZ Studio JSON edit (LV_OBJ_FLAG_EVENT_BUBBLE is a runtime flag,
// not layout - setting it here doesn't diverge from what the canvas shows).
static void rqBubble(lv_obj_t* child) {
	lv_obj_add_flag(child, LV_OBJ_FLAG_EVENT_BUBBLE);
}

static void rqNavPillClickedCb(lv_event_t* e) {
	(void) e;
	ui.showNavScreen();
}

void ui_RimRidgeRQInitNavLink() {
	rqBubble(objects.rq_ic_turn);
	rqBubble(objects.rq_nav_dist);
	lv_obj_add_event_cb(objects.rq_nav_pill, rqNavPillClickedCb, LV_EVENT_CLICKED, nullptr);
}

// ---------------- Manual road label controls ----------------
// Gravel variant only - the FL variant has no BMI160/I2CSensors at all, so
// the buttons stay inert there (stub bodies below the #else).
#ifdef TRGBBC_SENSORS_I2C
#include "I2CSensors.h"

// A second tap on the already-active pill/circle clears it (0 = none) -
// read the current value fresh from getRoadLabelState() rather than
// tracking it locally, so this stays correct even if the label changes from
// somewhere else. setRoadLabelSurface()/setRoadLabelQuality() are safe to
// call from an LVGL event callback (see I2CSensors.h) - they only set a
// request, the ImuTask does the actual logging.
static void rqSurfClickedCb(lv_event_t* e) {
	uint8_t surface = (uint8_t)(uintptr_t) lv_event_get_user_data(e);
	uint8_t current = sensors.getRoadLabelState().surface;
	sensors.setRoadLabelSurface((current == surface) ? 0 : surface);
}

static void rqQualClickedCb(lv_event_t* e) {
	uint8_t quality = (uint8_t)(uintptr_t) lv_event_get_user_data(e);
	uint8_t current = sensors.getRoadLabelState().quality;
	sensors.setRoadLabelQuality((current == quality) ? 0 : quality);
}

static void rqBtnRecordClickedCb(lv_event_t* e) {
	(void) e;
	if (sensors.getRoadLabelState().capturing) {
		sensors.stopRoadCapture();
	} else {
		sensors.startRoadCapture();
	}
}

void ui_RimRidgeRQInitLabelControls() {
	struct { lv_obj_t* pill; lv_obj_t* lbl; uint8_t surface; } surf[] = {
		{objects.rq_surf_asphalt,   objects.rq_surf_asphalt_lbl,   1},
		{objects.rq_surf_schotter,  objects.rq_surf_schotter_lbl,  2},
		{objects.rq_surf_waldweg,   objects.rq_surf_waldweg_lbl,   3},
		{objects.rq_surf_feldweg,   objects.rq_surf_feldweg_lbl,   4},
		{objects.rq_surf_pflaster,  objects.rq_surf_pflaster_lbl,  5},
		{objects.rq_surf_sonstiges, objects.rq_surf_sonstiges_lbl, 6},
	};
	for (auto& s : surf) {
		rqBubble(s.lbl);
		lv_obj_add_event_cb(s.pill, rqSurfClickedCb, LV_EVENT_CLICKED, (void*)(uintptr_t) s.surface);
	}

	struct { lv_obj_t* circle; lv_obj_t* lbl; uint8_t quality; } qual[] = {
		{objects.rq_qual_1, objects.rq_qual_1_lbl, 1},
		{objects.rq_qual_2, objects.rq_qual_2_lbl, 2},
		{objects.rq_qual_3, objects.rq_qual_3_lbl, 3},
		{objects.rq_qual_4, objects.rq_qual_4_lbl, 4},
	};
	for (auto& q : qual) {
		rqBubble(q.lbl);
		lv_obj_add_event_cb(q.circle, rqQualClickedCb, LV_EVENT_CLICKED, (void*)(uintptr_t) q.quality);
	}

	rqBubble(objects.rq_btn_record_ring);
	rqBubble(objects.rq_btn_record_dot);
	lv_obj_add_event_cb(objects.rq_btn_record, rqBtnRecordClickedCb, LV_EVENT_CLICKED, nullptr);
}

void ui_RimRidgeRQUpdateLabel(uint8_t surface, uint8_t quality, bool capturing) {
	// Same 4 tokens EEZ Studio's own PANEL_SEL/PANEL_UNSEL styles use for
	// these widgets' two states (add_rimridge_rq_controls.py) - hardcoded
	// hex for the same reason as ui_RimRidgeUpdateRoadQuality()'s zone
	// colors: this file has no access to the generated theme_colors[].
	static const lv_color_t SEL_BG = lv_color_hex(0xCBA36B);    // RRBrass - unselected stays at bg_opa 0, its own JSON-authored border does the outline
	static const lv_color_t SEL_TXT = lv_color_hex(0x161B1F);   // RRBackground
	static const lv_color_t UNSEL_TXT = lv_color_hex(0xE7E2D6); // RRParchment
	static const lv_color_t REC_COLOR = lv_color_hex(0xC1604A); // RRZoneRed, brighter than the idle ring's brass

	struct { lv_obj_t* pill; lv_obj_t* lbl; uint8_t value; } surf[] = {
		{objects.rq_surf_asphalt,   objects.rq_surf_asphalt_lbl,   1},
		{objects.rq_surf_schotter,  objects.rq_surf_schotter_lbl,  2},
		{objects.rq_surf_waldweg,   objects.rq_surf_waldweg_lbl,   3},
		{objects.rq_surf_feldweg,   objects.rq_surf_feldweg_lbl,   4},
		{objects.rq_surf_pflaster,  objects.rq_surf_pflaster_lbl,  5},
		{objects.rq_surf_sonstiges, objects.rq_surf_sonstiges_lbl, 6},
	};
	for (auto& s : surf) {
		bool sel = (s.value == surface);
		// bg_opa is 0 in the exported JSON for every one of these pills
		// (EEZ Studio's "default" useStyle, never overridden with a fill) -
		// setting bg_color alone is invisible without also raising the
		// opacity here; that's why the highlight didn't read as selected.
		lv_obj_set_style_bg_opa(s.pill, sel ? LV_OPA_COVER : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_color(s.pill, SEL_BG, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_color(s.lbl, sel ? SEL_TXT : UNSEL_TXT, LV_PART_MAIN | LV_STATE_DEFAULT);
	}

	struct { lv_obj_t* circle; lv_obj_t* lbl; uint8_t value; } qual[] = {
		{objects.rq_qual_1, objects.rq_qual_1_lbl, 1},
		{objects.rq_qual_2, objects.rq_qual_2_lbl, 2},
		{objects.rq_qual_3, objects.rq_qual_3_lbl, 3},
		{objects.rq_qual_4, objects.rq_qual_4_lbl, 4},
	};
	for (auto& q : qual) {
		bool sel = (q.value == quality);
		lv_obj_set_style_bg_opa(q.circle, sel ? LV_OPA_COVER : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_bg_color(q.circle, SEL_BG, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_color(q.lbl, sel ? SEL_TXT : UNSEL_TXT, LV_PART_MAIN | LV_STATE_DEFAULT);
	}

	// Recording: brass -> red ring, dot goes solid to read as "live" (no
	// separate recording-state widget exists yet, see
	// add_rimridge_rq_controls.py's comment on rq_btn_record).
	lv_obj_set_style_border_color(objects.rq_btn_record, capturing ? REC_COLOR : SEL_BG, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_border_color(objects.rq_btn_record_ring, capturing ? REC_COLOR : SEL_BG, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(objects.rq_btn_record_dot, capturing ? 255 : 128, LV_PART_MAIN | LV_STATE_DEFAULT);
}

#else // !TRGBBC_SENSORS_I2C - FL variant, no BMI160

void ui_RimRidgeRQInitLabelControls() {}
void ui_RimRidgeRQUpdateLabel(uint8_t surface, uint8_t quality, bool capturing) {
	(void) surface; (void) quality; (void) capturing;
}

#endif
