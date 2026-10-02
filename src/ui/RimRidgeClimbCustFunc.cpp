/*
 * RimRidgeClimbCustFunc.cpp
 *
 * See RimRidgeClimbCustFunc.h for scope notes.
 */

#include <math.h>

#include "ui_eez/screens.h"
#include "ui_eez/ui.h"
#include "ui_eez/actions.h"
#include "ui_eez/images.h"
#include "RimRidgeClimbCustFunc.h"
#include "Singletons.h"	// ui, climb

namespace {

// RRZoneBlue..RRZoneRed, as in ui_RimRidgeUpdateRoadQuality() (no access to theme_colors[] here)
const uint32_t BAND_COLOR[Climb::GRADE_BANDS] = {0x6C90B0, 0x6FA98C, 0xD7B463, 0xCE8A4C, 0xC1604A};
const uint32_t COLOR_PARCHMENT = 0xE7E2D6;
const uint32_t COLOR_PARCHMENT_BRIGHT = 0xF3ECDF;
const uint32_t COLOR_BRASS = 0xCBA36B;
const uint32_t COLOR_PANEL_BG = 0x1E252B;

// Plot area inside rrclimb_profile (the box itself is laid out in the EEZ project)
const lv_coord_t PROFILE_PAD_X = 6, PROFILE_PAD_TOP = 22, PROFILE_PAD_BOTTOM = 6;
const lv_coord_t PROFILE_MIN_H = 6;				// the lowest point still gets a strip of colour
const float PROFILE_MIN_SPAN_M = 20.0f;			// altitude range of the plot at least -- a 15 m hill is not drawn like a pass
const float PROFILE_LEAD_M = 100.0f, PROFILE_TAIL_M = 150.0f;
const lv_opa_t PROFILE_DONE_OPA = 90;			// the part behind the rider

// What the profile box shows. Only the UI task touches it (update and draw event).
struct ProfileView {
	int16_t altDm[Climb::MAX_POINTS];
	uint16_t count = 0;
	uint8_t stepM = 25;
	uint32_t version = UINT32_MAX;
	bool posValid = false, summitShown = false;
	float posM = 0, summitM = 0;
	float fromM = 0, toM = 0;					// visible stretch, metres from the first point
	float bandPct[Climb::GRADE_BANDS - 1] = {};
	lv_coord_t markerX = -1;					// last drawn, relative to the plot: redraw when it moves
};
ProfileView view;

float lengthM() {
	return static_cast<float>(view.count - 1) * view.stepM;
}

// Altitude in dm and gradient in % of the step at a distance
void sample(float distM, float& altDm, float& gradePct) {
	float x = distM / view.stepM;
	if (x < 0) x = 0;
	int32_t i = static_cast<int32_t>(x);
	if (i > view.count - 2) i = view.count - 2;
	const float delta = view.altDm[i + 1] - view.altDm[i];
	float t = x - i;
	if (t > 1) t = 1;
	altDm = view.altDm[i] + t * delta;
	gradePct = delta * 10.0f / view.stepM;
}

uint8_t band(float gradePct) {
	uint8_t b = 0;
	while (b < Climb::GRADE_BANDS - 1 && gradePct >= view.bandPct[b]) b++;
	return b;
}

void profileDrawCb(lv_event_t* e) {
	if (view.count < 2 || view.toM - view.fromM < view.stepM) return;
	lv_obj_t* obj = lv_event_get_target(e);
	lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);
	lv_area_t box;
	lv_obj_get_coords(obj, &box);
	const lv_coord_t x0 = box.x1 + PROFILE_PAD_X;
	const lv_coord_t width = box.x2 - PROFILE_PAD_X - x0 + 1;
	const lv_coord_t bottom = box.y2 - PROFILE_PAD_BOTTOM;
	const lv_coord_t plotH = bottom - (box.y1 + PROFILE_PAD_TOP);
	if (width < 2 || plotH <= PROFILE_MIN_H) return;

	// Altitude range of the visible stretch
	int32_t first = static_cast<int32_t>(view.fromM / view.stepM);
	if (first > view.count - 1) first = view.count - 1;
	int32_t last = static_cast<int32_t>(ceilf(view.toM / view.stepM));
	if (last > view.count - 1) last = view.count - 1;
	int16_t minDm = view.altDm[first], maxDm = minDm;
	for (int32_t i = first + 1; i <= last; i++) {
		if (view.altDm[i] < minDm) minDm = view.altDm[i];
		if (view.altDm[i] > maxDm) maxDm = view.altDm[i];
	}
	float spanDm = maxDm - minDm;
	if (spanDm < PROFILE_MIN_SPAN_M * 10.0f) spanDm = PROFILE_MIN_SPAN_M * 10.0f;
	const float mPerPx = (view.toM - view.fromM) / width;
	const float pxPerDm = static_cast<float>(plotH - PROFILE_MIN_H) / spanDm;
	auto heightOf = [&](float altDm) -> lv_coord_t {
		return PROFILE_MIN_H + static_cast<lv_coord_t>((altDm - minDm) * pxPerDm + 0.5f);
	};

	lv_draw_rect_dsc_t rect;
	lv_draw_rect_dsc_init(&rect);
	rect.border_width = 0;

	// One column per pixel; neighbours of the same height and colour go out as one rectangle
	lv_coord_t runStart = 0, runH = -1;
	uint8_t runBand = 0;
	bool runDone = false;
	auto flush = [&](lv_coord_t endCol) {
		if (runH < 0) return;
		lv_area_t a = {static_cast<lv_coord_t>(x0 + runStart), static_cast<lv_coord_t>(bottom - runH + 1),
				static_cast<lv_coord_t>(x0 + endCol - 1), bottom};
		rect.bg_color = lv_color_hex(BAND_COLOR[runBand]);
		rect.bg_opa = runDone ? PROFILE_DONE_OPA : LV_OPA_COVER;
		lv_draw_rect(ctx, &rect, &a);
	};
	for (lv_coord_t col = 0; col < width; col++) {
		if (x0 + col < ctx->clip_area->x1 || x0 + col > ctx->clip_area->x2) {		// outside what is being redrawn
			flush(col);
			runH = -1;
			continue;
		}
		const float d = view.fromM + (col + 0.5f) * mPerPx;
		float altDm, gradePct;
		sample(d, altDm, gradePct);
		const lv_coord_t h = heightOf(altDm);
		const uint8_t b = band(gradePct);
		const bool done = view.posValid && d < view.posM;
		if (h != runH || b != runBand || done != runDone) {
			flush(col);
			runStart = col;
			runH = h;
			runBand = b;
			runDone = done;
		}
	}
	flush(width);

	float altDm, gradePct;
	if (view.summitShown) {		// flag on the summit
		sample(view.summitM, altDm, gradePct);
		const lv_coord_t sx = x0 + static_cast<lv_coord_t>((view.summitM - view.fromM) / mPerPx);
		const lv_coord_t sy = bottom - heightOf(altDm);
		rect.bg_color = lv_color_hex(COLOR_BRASS);
		rect.bg_opa = LV_OPA_COVER;
		lv_area_t pole = {sx, static_cast<lv_coord_t>(sy - 13), static_cast<lv_coord_t>(sx + 1), sy};
		lv_draw_rect(ctx, &rect, &pole);
		lv_area_t cloth = {static_cast<lv_coord_t>(sx + 2), static_cast<lv_coord_t>(sy - 13), static_cast<lv_coord_t>(sx + 8), static_cast<lv_coord_t>(sy - 8)};
		lv_draw_rect(ctx, &rect, &cloth);
	}
	if (view.posValid && view.posM >= view.fromM && view.posM <= view.toM) {		// the rider
		sample(view.posM, altDm, gradePct);
		const lv_coord_t mx = x0 + view.markerX;
		const lv_coord_t my = bottom - heightOf(altDm);
		rect.bg_color = lv_color_hex(COLOR_PARCHMENT_BRIGHT);
		rect.bg_opa = LV_OPA_COVER;
		lv_area_t line = {mx, static_cast<lv_coord_t>(box.y1 + PROFILE_PAD_TOP - 4), static_cast<lv_coord_t>(mx + 1), bottom};
		lv_draw_rect(ctx, &rect, &line);
		rect.radius = LV_RADIUS_CIRCLE;
		rect.border_width = 2;
		rect.border_color = lv_color_hex(COLOR_PANEL_BG);
		rect.border_opa = LV_OPA_COVER;
		lv_area_t dot = {static_cast<lv_coord_t>(mx - 5), static_cast<lv_coord_t>(my - 5), static_cast<lv_coord_t>(mx + 6), static_cast<lv_coord_t>(my + 6)};
		lv_draw_rect(ctx, &rect, &dot);
	}
}

// Takes over what is to be drawn; invalidates the box if the picture changes.
void updateProfileView(const Climb::Status& st, const Climb::Config& cfg, uint32_t version) {
	bool changed = false;
	if (version != view.version) {
		view.version = version;
		view.count = climb.copyProfile(view.altDm, Climb::MAX_POINTS, view.stepM);
		changed = true;
	}
	for (uint8_t i = 0; i < Climb::GRADE_BANDS - 1; i++) {
		if (view.bandPct[i] != cfg.gradeBandPct[i]) {
			view.bandPct[i] = cfg.gradeBandPct[i];
			changed = true;
		}
	}
	if (view.count < 2) {
		if (changed) lv_obj_invalidate(objects.rrclimb_profile);
		return;
	}

	float fromM = 0, toM = lengthM();
	const bool summitShown = st.active && !st.summitOpen;
	if (st.active) {
		fromM = st.footM - PROFILE_LEAD_M;
		if (st.posM < fromM) fromM = st.posM;		// approaching: the rider is in the picture
		if (fromM < 0) fromM = 0;
		if (summitShown && st.summitM + PROFILE_TAIL_M < toM) toM = st.summitM + PROFILE_TAIL_M;
	}
	const lv_coord_t width = lv_obj_get_width(objects.rrclimb_profile) - 2 * PROFILE_PAD_X;
	lv_coord_t markerX = -1;
	if (st.positionValid && toM > fromM) markerX = static_cast<lv_coord_t>((st.posM - fromM) / (toM - fromM) * width);

	if (fromM != view.fromM || toM != view.toM || summitShown != view.summitShown || st.summitM != view.summitM
			|| st.positionValid != view.posValid || markerX != view.markerX) changed = true;
	view.fromM = fromM;
	view.toM = toM;
	view.summitShown = summitShown;
	view.summitM = st.summitM;
	view.posValid = st.positionValid;
	view.posM = st.posM;
	view.markerX = markerX;
	if (changed) lv_obj_invalidate(objects.rrclimb_profile);
}

// ---------- rotating info field ----------
enum InfoItem : uint8_t {INFO_CLOCK, INFO_DIST, INFO_TEMP, INFO_CADENCE, INFO_ALT, INFO_COUNT};

bool infoAvailable(uint8_t item, const RimRidgeClimbInfo& info) {
	switch (item) {
	case INFO_TEMP: return !isnan(info.temperature);
	case INFO_CADENCE: return info.cadence >= 0;
	case INFO_ALT: return !isnan(info.heightM);
	default: return true;
	}
}

void updateInfo(const RimRidgeClimbInfo& info, uint8_t cycleS) {
	static uint8_t item = INFO_CLOCK, shownS = 0;
	static int8_t iconOf = -1;
	if (++shownS >= cycleS || !infoAvailable(item, info)) {
		shownS = 0;
		for (uint8_t i = 0; i < INFO_COUNT; i++) {
			item = (item + 1) % INFO_COUNT;
			if (infoAvailable(item, info)) break;
		}
	}
	if (item != iconOf) {
		static const lv_img_dsc_t* const ICON[INFO_COUNT] = {&img_rr_icon_clock, &img_rr_icon_ruler, &img_rr_icon_temp, &img_rr_icon_cadence, &img_rr_icon_height};
		const bool first = iconOf < 0;
		iconOf = item;
		lv_img_set_src(objects.rrclimb_ic_info, ICON[item]);
		if (!first) lv_obj_fade_in(objects.rrclimb_grp_info, 400, 0);
	}
	switch (item) {
	case INFO_CLOCK: lv_label_set_text(objects.rrclimb_info_val, info.clock); break;
	case INFO_DIST: lv_label_set_text_fmt(objects.rrclimb_info_val, "%.1f km", info.distM / 1000.0); break;
	case INFO_TEMP: lv_label_set_text_fmt(objects.rrclimb_info_val, "%.0f\xc2\xb0", info.temperature); break;		// UTF-8 degree sign
	case INFO_CADENCE: lv_label_set_text_fmt(objects.rrclimb_info_val, "%d rpm", info.cadence); break;
	case INFO_ALT: lv_label_set_text_fmt(objects.rrclimb_info_val, "%.0f m", info.heightM); break;
	}
}

}	// anonymous namespace

void ui_RimRidgeClimbInit() {
	// After the box's own background and border, before its children (the summit label)
	lv_obj_add_event_cb(objects.rrclimb_profile, profileDrawCb, LV_EVENT_DRAW_MAIN_END, nullptr);
	// The EEZ canvas shows example values: start from "no climb"
	const RimRidgeClimbInfo none = {"--:--", 0, NAN, -1, NAN};
	ui_RimRidgeClimbUpdate(Climb::Status(), Climb::Config(), 0, none);
	ui_RimRidgeClimbUpdateSpeed(NAN);
	ui_RimRidgeClimbUpdateHR(-1);
	ui_RimRidgeClimbUpdateGrad(NAN);
}

void ui_RimRidgeClimbUpdateSpeed(float speed) {
	if (isnan(speed)) {
		lv_label_set_text(objects.rrclimb_speed_val, "-/-");
	} else {
		lv_label_set_text_fmt(objects.rrclimb_speed_val, "%.1f", speed);
	}
}

void ui_RimRidgeClimbUpdateHR(int16_t hr) {
	lv_label_set_text_fmt(objects.rrclimb_hr_val, (hr >= 0) ? "%d" : "-/-", hr);
}

void ui_RimRidgeClimbUpdateGrad(float grad) {
	if (isnan(grad)) {
		lv_label_set_text(objects.rrclimb_grad_val, "-/-");
	} else {
		lv_label_set_text_fmt(objects.rrclimb_grad_val, "%+.1f%%", grad);
	}
}

void ui_RimRidgeClimbUpdate(const Climb::Status& st, const Climb::Config& cfg, uint32_t version, const RimRidgeClimbInfo& info) {
	const bool onProfile = st.hasProfile && st.positionValid;
	const bool active = onProfile && st.active;
	const char* atLeast = (active && st.summitOpen) ? ">" : "";

	if (!active) {
		lv_label_set_text(objects.rrclimb_cat, "KEIN ANSTIEG");
		lv_label_set_text(objects.rrclimb_rem_val, "--");
		lv_label_set_text(objects.rrclimb_total_val, "--");
		lv_label_set_text(objects.rrclimb_dist_val, "--");
		lv_label_set_text(objects.rrclimb_summit_alt, st.hasProfile ? "" : "kein H\xc3\xb6henprofil");
		lv_arc_set_value(objects.rrclimb_arc, 0);
	} else {
		if (st.rank == Climb::RANK_NONE) {
			lv_label_set_text(objects.rrclimb_cat, "ANSTIEG");
		} else {
			lv_label_set_text_fmt(objects.rrclimb_cat, "KAT. %s", Climb::rankLabel(st.rank));
		}
		lv_label_set_text_fmt(objects.rrclimb_rem_val, "%s%.0f m", atLeast, st.remainingAscentM);
		lv_label_set_text_fmt(objects.rrclimb_total_val, "von %s%.0f m", atLeast, st.totalAscentM);
		if (st.toSummitM < 1000) {
			lv_label_set_text_fmt(objects.rrclimb_dist_val, "%s%.0f m", atLeast, st.toSummitM);
		} else {
			lv_label_set_text_fmt(objects.rrclimb_dist_val, "%s%.1f km", atLeast, st.toSummitM / 1000.0);
		}
		lv_label_set_text_fmt(objects.rrclimb_summit_alt, "Gipfel %s%.0f m", atLeast, st.summitAltM);
		lv_arc_set_value(objects.rrclimb_arc, static_cast<int16_t>(st.doneFraction * 100.0f + 0.5f));
	}

	static uint16_t lookAheadShown = 0;
	if (cfg.lookAheadM != lookAheadShown) {
		lookAheadShown = cfg.lookAheadM;
		lv_label_set_text_fmt(objects.rrclimb_ahead_cap, "N\xc3\x84""CHSTE %u m", cfg.lookAheadM);
	}
	if (!onProfile || isnan(st.aheadGradePct)) {
		lv_label_set_text(objects.rrclimb_ahead_val, "-/-");
		lv_obj_set_style_text_color(objects.rrclimb_ahead_val, lv_color_hex(COLOR_PARCHMENT), LV_PART_MAIN | LV_STATE_DEFAULT);
	} else {
		lv_label_set_text_fmt(objects.rrclimb_ahead_val, "%+.1f%%", st.aheadGradePct);
		lv_obj_set_style_text_color(objects.rrclimb_ahead_val, lv_color_hex(BAND_COLOR[Climb::gradeBand(cfg, st.aheadGradePct)]), LV_PART_MAIN | LV_STATE_DEFAULT);
	}

	updateProfileView(st, cfg, version);
	updateInfo(info, cfg.infoCycleS);
}

// EEZ Studio action, wired to the CLICKED event of rr_grp_height and rr_grp_gradient on
// RimRidge: opens the climb screen by hand (also without a climb).
void action_go_to_climb(lv_event_t * e) {
	(void) e;
	ui.showClimbScreen();
}

// EEZ Studio action, wired to the RimRidgeClimb screen root's GESTURE event: any swipe goes
// back to the main screen, as on the settings and RQ screens.
void action_climb_screen_gesture(lv_event_t * e) {
	(void) e;
	lv_indev_t * indev = lv_indev_get_act();
	if (!indev) return;
	lv_indev_wait_release(indev);
	ui.hideClimbScreen();
}
