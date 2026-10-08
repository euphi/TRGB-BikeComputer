/*
 * RimRidgeRouteCustFunc.cpp
 *
 * See RimRidgeRouteCustFunc.h for scope notes.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ui_eez/screens.h"
#include "ui_eez/ui.h"
#include "ui_eez/actions.h"
#include "ui_eez/fonts.h"
#include "RimRidgeRouteCustFunc.h"
#include "RouteOverview.h"
#include "Singletons.h"	// ui, routeMon

namespace {

// Hardcoded for the same reason as in RimRidgeWifiCustFunc.cpp (no access to theme_colors[]):
// RRPanelBg, RRBrass, RRParchment, RRMuted, and the zone colours of the climb screen's profile.
const uint32_t COLOR_PANEL = 0x1E252B;
const uint32_t COLOR_BRASS = 0xCBA36B;
const uint32_t COLOR_PARCHMENT = 0xE7E2D6;
const uint32_t COLOR_MUTED = 0x9BA097;
const uint32_t BAND_COLOR[] = {0x6C90B0, 0x6FA98C, 0xD7B463, 0xCE8A4C, 0xE8392A};
const uint32_t BAND_PCT[] = {1, 4, 7, 10};				// Climb::Config::gradeBandPct, as shipped

const lv_coord_t ROW_W = 356, ROW_H = 52, ROW_PITCH = 58;	// inside rrroute_list (356 x 216): three rows and most of a fourth
const lv_coord_t DIST_W = 140;							// the right-hand figure of a row
const uint8_t MAX_ROWS = RouteOv::MAX_VIEW_ROWS;

struct RowWidgets {
	lv_obj_t* box;
	lv_obj_t* stripe;
	lv_obj_t* name;
	lv_obj_t* dist;
	lv_obj_t* info;
};

// What is shown. Static: the UI task has little stack (doc/PITFALLS.md), and a View is 1.6 kB.
RouteOv::View view;
RowWidgets rows[MAX_ROWS];
uint8_t rowCount = 0;
uint32_t shownVersion = UINT32_MAX;
lv_style_t rowStyle;
bool stylesReady = false;

void setText(lv_obj_t* label, const char* text) {
	if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void setHidden(lv_obj_t* obj, bool hidden) {
	if (hidden == lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
	if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
	else lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

// "850 m", "5.5 km", "123 km". Integers only: the UI task has next to no stack for libc's float printing.
void formatDist(char* out, size_t size, int32_t m) {
	if (m < 0) m = 0;
	if (m < 100) {
		snprintf(out, size, "%d m", static_cast<int>(m));
	} else if (m < 1000) {
		snprintf(out, size, "%d m", static_cast<int>((m + 5) / 10 * 10));
	} else if (m < 100000) {
		const int hm = (m + 50) / 100;		// hectometres, rounded
		snprintf(out, size, "%d.%d km", hm / 10, hm % 10);
	} else {
		snprintf(out, size, "%d km", static_cast<int>((m + 500) / 1000));
	}
}

// "18 min", "1:05 h"
void formatDuration(char* out, size_t size, uint32_t s) {
	const uint32_t min = (s + 30) / 60;
	if (min < 1) snprintf(out, size, "<1 min");
	else if (min < 60) snprintf(out, size, "%u min", static_cast<unsigned>(min));
	else snprintf(out, size, "%u:%02u h", static_cast<unsigned>(min / 60), static_cast<unsigned>(min % 60));
}

// "14:32"; false if the clock is not set (the time of the nav frame is unknown)
bool formatArrival(char* out, size_t size, uint32_t timeS) {
	if (view.navEpoch == 0) {
		out[0] = '\0';
		return false;
	}
	const time_t t = static_cast<time_t>(view.navEpoch + timeS);
	struct tm lt;
	localtime_r(&t, &lt);
	snprintf(out, size, "%02d:%02d", lt.tm_hour, lt.tm_min);
	return true;
}

// The text cut to maxW pixels with "..." at the end -- LV_LABEL_LONG_DOT would wrap at a word first and
// dot the last line that fits ("Kreuzung Alte..."), but a row has one line. Cuts on a UTF-8 boundary.
// out holds OVERVIEW_NAME_MAX + 4 byte.
void fitText(char* out, const char* text, lv_coord_t maxW) {
	const lv_font_t* font = &ui_font_montserrat22;
	size_t len = strlen(text);
	if (len > OVERVIEW_NAME_MAX) len = OVERVIEW_NAME_MAX;
	memcpy(out, text, len);
	out[len] = '\0';
	if (lv_txt_get_width(out, len, font, 0, LV_TEXT_FLAG_NONE) <= maxW) return;
	const lv_coord_t dotsW = lv_txt_get_width("...", 3, font, 0, LV_TEXT_FLAG_NONE);
	while (len > 0) {
		len--;
		while (len > 0 && (out[len] & 0xC0) == 0x80) len--;
		if (lv_txt_get_width(out, len, font, 0, LV_TEXT_FLAG_NONE) + dotsW <= maxW) break;
	}
	while (len > 0 && out[len - 1] == ' ') len--;
	strcpy(out + len, "...");
}

// Mean gradient of the whole climb in tenths of a percent
uint32_t climbGradeTenths(const RouteOv::Row& r) {
	return r.lengthM ? static_cast<uint32_t>((static_cast<uint64_t>(r.gainM) * 1000 + r.lengthM / 2) / r.lengthM) : 0;
}

uint32_t climbColor(const RouteOv::Row& r) {
	const uint32_t tenths = climbGradeTenths(r);
	uint8_t band = 0;
	while (band < 4 && tenths >= BAND_PCT[band] * 10) band++;
	return BAND_COLOR[band];
}

void makeRow(uint8_t i) {
	RowWidgets& w = rows[i];
	w.box = lv_obj_create(objects.rrroute_list);
	lv_obj_set_pos(w.box, 0, static_cast<lv_coord_t>(i * ROW_PITCH));
	lv_obj_set_size(w.box, ROW_W, ROW_H);
	lv_obj_clear_flag(w.box, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));		// touches go to the list: scroll, swipe
	lv_obj_add_style(w.box, &rowStyle, LV_PART_MAIN | LV_STATE_DEFAULT);

	w.stripe = lv_obj_create(w.box);
	lv_obj_clear_flag(w.stripe, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
	lv_obj_set_style_border_width(w.stripe, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_bg_opa(w.stripe, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

	w.name = lv_label_create(w.box);
	lv_label_set_long_mode(w.name, LV_LABEL_LONG_CLIP);		// the text is cut by fitText(); CLIP keeps it on one line
	lv_obj_set_height(w.name, 24);
	lv_obj_set_style_text_font(w.name, &ui_font_montserrat22, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(w.name, lv_color_hex(COLOR_PARCHMENT), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_align(w.name, LV_ALIGN_TOP_LEFT, 26, 2);

	w.dist = lv_label_create(w.box);
	lv_obj_set_width(w.dist, DIST_W);
	lv_label_set_long_mode(w.dist, LV_LABEL_LONG_CLIP);
	lv_obj_set_style_text_align(w.dist, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_font(w.dist, &ui_font_montserrat22, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(w.dist, lv_color_hex(COLOR_BRASS), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_align(w.dist, LV_ALIGN_TOP_RIGHT, -14, 2);
	lv_obj_set_height(w.dist, 24);

	w.info = lv_label_create(w.box);
	lv_obj_set_width(w.info, ROW_W - 26 - 14);
	lv_label_set_long_mode(w.info, LV_LABEL_LONG_CLIP);
	lv_obj_set_height(w.info, 21);
	lv_obj_set_style_text_font(w.info, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(w.info, lv_color_hex(COLOR_MUTED), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_align(w.info, LV_ALIGN_TOP_LEFT, 26, 28);
}

void fillRow(uint8_t i, const RouteOv::Row& r) {
	RowWidgets& w = rows[i];
	// static: the UI task has next to no stack left, and only the UI task gets here
	static char name[OVERVIEW_NAME_MAX + 4], fitted[OVERVIEW_NAME_MAX + 4], dist[24], info[56], when[8], span[16], left[16], len[16];
	uint32_t stripe;
	if (r.kind == RouteOv::KIND_CLIMB) {
		stripe = climbColor(r);
		if (view.climbsTotal >= r.number) snprintf(name, sizeof(name), "Anstieg %u/%u", r.number, view.climbsTotal);
		else snprintf(name, sizeof(name), "Anstieg %u", r.number);
		if (r.distM > 0) {
			formatDist(dist, sizeof(dist), r.distM);
		} else {		// in it: what is left to the summit
			formatDist(left, sizeof(left), r.summitM);
			snprintf(dist, sizeof(dist), "noch %s", left);
		}
		formatDist(len, sizeof(len), static_cast<int32_t>(r.lengthM));
		const uint32_t tenths = climbGradeTenths(r);
		snprintf(info, sizeof(info), "%u Hm, %s, %u.%u %%", r.gainM, len, static_cast<unsigned>(tenths / 10), static_cast<unsigned>(tenths % 10));
	} else {
		stripe = COLOR_BRASS;
		snprintf(name, sizeof(name), "%s", r.name);
		formatDist(dist, sizeof(dist), r.distM);
		formatDuration(span, sizeof(span), r.timeS);
		if (formatArrival(when, sizeof(when), r.timeS)) snprintf(info, sizeof(info), "Ankunft %s (%s)", when, span);
		else snprintf(info, sizeof(info), "in %s", span);
	}
	// A dot for a waypoint, a bar for a climb (in the colour of its gradient): told apart by shape, not only by colour
	if (r.kind == RouteOv::KIND_CLIMB) {
		lv_obj_set_size(w.stripe, 5, ROW_H - 16);
		lv_obj_set_style_radius(w.stripe, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_align(w.stripe, LV_ALIGN_LEFT_MID, 9, 0);
	} else {
		lv_obj_set_size(w.stripe, 10, 10);
		lv_obj_set_style_radius(w.stripe, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_align(w.stripe, LV_ALIGN_LEFT_MID, 8, 0);
	}
	lv_obj_set_style_bg_color(w.stripe, lv_color_hex(stripe), LV_PART_MAIN | LV_STATE_DEFAULT);
	setText(w.dist, dist);
	setText(w.info, info);
	// The name gets what the figure on the right leaves
	const lv_coord_t distW = lv_txt_get_width(dist, strlen(dist), &ui_font_montserrat22, 0, LV_TEXT_FLAG_NONE);
	const lv_coord_t nameW = ROW_W - 26 - 14 - distW - 8;
	lv_obj_set_width(w.name, nameW);
	fitText(fitted, name, nameW);
	setText(w.name, fitted);
}

void update() {
	uint32_t version;
	const bool ok = routeMon.poll(view, version);

	setHidden(objects.rrroute_dest, !ok || !view.hasDestination);
	setHidden(objects.rrroute_foot, !ok);
	if (!ok) {
		setText(objects.rrroute_empty, "Keine Strecke");
		setText(objects.rrroute_empty2, "GPX-Route in TrailBridge");
		setHidden(objects.rrroute_empty, false);
		setHidden(objects.rrroute_empty2, false);
		while (rowCount > 0) {
			lv_obj_del(rows[--rowCount].box);
		}
		shownVersion = UINT32_MAX;
		return;
	}

	static char text[64], dist[24], when[8], span[16];		// static: see fillRow()
	if (view.hasDestination) {
		formatDist(dist, sizeof(dist), view.destination.distM);
		if (formatArrival(when, sizeof(when), view.destination.timeS)) {
			snprintf(text, sizeof(text), "Ziel %s  %s", dist, when);
		} else {
			formatDuration(span, sizeof(span), view.destination.timeS);
			snprintf(text, sizeof(text), "Ziel %s  %s", dist, span);
		}
		setText(objects.rrroute_dest, text);
	}

	const uint8_t n = view.count;
	while (rowCount < n) makeRow(rowCount++);
	while (rowCount > n) lv_obj_del(rows[--rowCount].box);
	if (shownVersion != version) {		// a new overview: from the top again
		shownVersion = version;
		lv_obj_scroll_to_y(objects.rrroute_list, 0, LV_ANIM_OFF);
	}
	for (uint8_t i = 0; i < n; i++) fillRow(i, view.rows[i]);

	setText(objects.rrroute_empty, "Nichts mehr voraus");
	setText(objects.rrroute_empty2, "bis zum Ziel");
	setHidden(objects.rrroute_empty, n != 0);
	setHidden(objects.rrroute_empty2, n != 0);

	const bool cut = view.shortened;
	if (view.waypointsLeft == 0 && !cut) {
		setHidden(objects.rrroute_foot, true);
	} else {
		if (view.waypointsLeft == 255) snprintf(text, sizeof(text), "255+ Wegpunkte voraus%s", cut ? ", gekürzt" : "");
		else if (view.waypointsLeft == 0) snprintf(text, sizeof(text), "gekürzt");
		else snprintf(text, sizeof(text), "%u Wegpunkt%s voraus%s", view.waypointsLeft, view.waypointsLeft == 1 ? "" : "e", cut ? ", gekürzt" : "");
		setText(objects.rrroute_foot, text);
		setHidden(objects.rrroute_foot, false);
	}
}

void timerCb(lv_timer_t* t) {
	(void) t;
	if (lv_scr_act() == objects.rim_ridge_route) update();
}

}	// namespace

void ui_RimRidgeRouteInit() {
	if (!stylesReady) {
		lv_style_init(&rowStyle);
		lv_style_set_bg_color(&rowStyle, lv_color_hex(COLOR_PANEL));
		lv_style_set_bg_opa(&rowStyle, LV_OPA_COVER);
		lv_style_set_radius(&rowStyle, 14);
		lv_style_set_border_color(&rowStyle, lv_color_hex(COLOR_BRASS));
		lv_style_set_border_width(&rowStyle, 1);
		lv_style_set_border_opa(&rowStyle, 70);
		lv_style_set_pad_all(&rowStyle, 0);
		stylesReady = true;
	}
	// The canvas' example content: "no route" until the first poll
	setText(objects.rrroute_dest, "");
	setText(objects.rrroute_foot, "");
	setHidden(objects.rrroute_dest, true);
	setHidden(objects.rrroute_foot, true);
	lv_timer_create(timerCb, 1000, NULL);
}

void ui_RimRidgeRouteRefresh() {
	update();
}

// ---------------- EEZ actions ----------------

// Any swipe on RimRidgeRoute goes back, as on the other sub screens.
void action_route_screen_gesture(lv_event_t* e) {
	(void) e;
	lv_indev_t* indev = lv_indev_get_act();
	if (!indev) return;
	lv_indev_wait_release(indev);
	ui.hideRouteScreen();
}
