/*
 * RimRidgeWifiCustFunc.cpp
 *
 * See RimRidgeWifiCustFunc.h for scope notes.
 */

#include <stdio.h>
#include <string.h>

#include "ui_eez/screens.h"
#include "ui_eez/ui.h"
#include "ui_eez/actions.h"
#include "ui_eez/fonts.h"
#include "RimRidgeWifiCustFunc.h"
#include "Singletons.h"	// webserver

namespace {

// Hardcoded for the same reason as in RimRidgeSettingsCustFunc.cpp (no access to theme_colors[]):
// RRPanelBg, RRTourBg, RRBrass, RRParchment, RRMuted, RRZoneRed.
const uint32_t COLOR_PANEL = 0x1E252B;
const uint32_t COLOR_TOUR = 0x282019;
const uint32_t COLOR_BRASS = 0xCBA36B;
const uint32_t COLOR_PARCHMENT = 0xE7E2D6;
const uint32_t COLOR_MUTED = 0x9BA097;
const uint32_t COLOR_ERROR = 0xC1604A;

const lv_coord_t ROW_W = 330, ROW_H = 40, ROW_PITCH = 46;		// inside rrwifi_list (330 x 200)
const uint32_t SCAN_PENDING_MS = 2000;							// "suche ..." until the 500 ms tick has seen the request

// The last scan as shown; static because the UI task has little stack (doc/PITFALLS.md).
WifiScanEntry entries[WifiWebserver::SCAN_MAX];
size_t entryCount = 0;
uint32_t shownVersion = UINT32_MAX;			// forces a rebuild on the first update
uint32_t scanTappedAt = 0;					// lv_tick of the Scan tap, 0 = none

// The network the password screen is about.
char pwSsid[WifiCfg::SSID_MAX + 1];
bool pwOpen = false, pwKnown = false;

lv_style_t rowStyle, rowPressedStyle;
bool stylesReady = false;

void setText(lv_obj_t* label, const char* text) {
	if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void setHint(const char* text, uint32_t color) {
	lv_label_set_text(objects.rrwpw_hint, text);
	lv_obj_set_style_text_color(objects.rrwpw_hint, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void setPillEnabled(lv_obj_t* pill, lv_obj_t* label, bool enabled) {
	if (enabled) {
		lv_obj_clear_state(pill, LV_STATE_DISABLED);
		lv_obj_clear_state(label, LV_STATE_DISABLED);
	} else {
		lv_obj_add_state(pill, LV_STATE_DISABLED);
		lv_obj_add_state(label, LV_STATE_DISABLED);
	}
}

// ---------------- password screen ----------------

void openPasswordScreen() {
	lv_label_set_text(objects.rrwpw_ssid, pwSsid);
	lv_textarea_set_text(objects.rrwpw_ta, "");
	lv_textarea_set_placeholder_text(objects.rrwpw_ta, pwOpen ? "offen: leer lassen" : pwKnown ? "unverändert" : "Passwort");
	lv_textarea_set_password_mode(objects.rrwpw_ta, true);
	lv_label_set_text(objects.rrwpw_btn_eye_lbl, "abc");
	lv_keyboard_set_mode(objects.rrwpw_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
	setHint(pwOpen ? "offenes Netz" : "mind. 8 Zeichen", COLOR_MUTED);
	lv_disp_load_scr(objects.rim_ridge_wifi_pw);
}

void closePasswordScreen() {
	lv_textarea_set_text(objects.rrwpw_ta, "");		// don't leave the password in the widget
	shownVersion = UINT32_MAX;						// "saved" marks may have changed
	lv_disp_load_scr(objects.rim_ridge_wifi);
}

void savePassword() {
	const char* pw = lv_textarea_get_text(objects.rrwpw_ta);
	const size_t n = strlen(pw);
	if (n == 0 && !pwOpen && !pwKnown) {
		setHint("Passwort fehlt", COLOR_ERROR);
		return;
	}
	if (n != 0 && n < WifiCfg::PW_MIN) {
		setHint("mind. 8 Zeichen", COLOR_ERROR);
		return;
	}
	if (!webserver.addNetworkAsync(pwSsid, pw)) {
		setHint("Fehler - bitte erneut", COLOR_ERROR);
		return;
	}
	closePasswordScreen();
}

// ---------------- keyboard ----------------
// 10 keys per row. Control keys carry CHECKED, which the EEZ style draws as TourBg/brass.
#define KEY(w) (LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_CLICK_TRIG | (w))
#define CTL(w) (LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_CLICK_TRIG | LV_BTNMATRIX_CTRL_CHECKED | (w))
#define BKSP(w) (LV_BTNMATRIX_CTRL_CHECKED | (w))		// no NO_REPEAT: held down it deletes on

const char* KB_LOWER[] = {"q", "w", "e", "r", "t", "z", "u", "i", "o", "p", "\n",
                          "a", "s", "d", "f", "g", "h", "j", "k", "l", "\n",
                          "ABC", "y", "x", "c", "v", "b", "n", "m", LV_SYMBOL_BACKSPACE, "\n",
                          "1#", ".", " ", ",", LV_SYMBOL_OK, ""};
const lv_btnmatrix_ctrl_t KB_LOWER_CTRL[] = {
	KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4),
	KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4),
	CTL(6), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), BKSP(6),
	CTL(5), KEY(3), KEY(12), KEY(3), CTL(5)};

const char* KB_UPPER[] = {"Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P", "\n",
                          "A", "S", "D", "F", "G", "H", "J", "K", "L", "\n",
                          "abc", "Y", "X", "C", "V", "B", "N", "M", LV_SYMBOL_BACKSPACE, "\n",
                          "1#", ".", " ", ",", LV_SYMBOL_OK, ""};

const char* KB_SYMBOLS[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
                            "@", "#", "$", "%", "&", "*", "-", "+", "=", "_", "\n",
                            "#+=", "!", "?", ":", ";", "\"", "'", "/", LV_SYMBOL_BACKSPACE, "\n",
                            "abc", ".", " ", ",", LV_SYMBOL_OK, ""};
const lv_btnmatrix_ctrl_t KB_SYMBOLS_CTRL[] = {
	KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4),
	KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4),
	CTL(6), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), KEY(4), BKSP(6),
	CTL(5), KEY(3), KEY(12), KEY(3), CTL(5)};

const char* KB_MORE[] = {"(", ")", "<", ">", "[", "]", "{", "}", "|", "\\", "\n",
                         "^", "`", "~", "-", "_", "=", "+", "*", "#", "@", "\n",
                         "123", "!", "?", ":", ";", "\"", "'", "/", LV_SYMBOL_BACKSPACE, "\n",
                         "abc", ".", " ", ",", LV_SYMBOL_OK, ""};

void keyboardEvent(lv_event_t* e) {
	lv_obj_t* kb = lv_event_get_target(e);
	const uint16_t id = lv_keyboard_get_selected_btn(kb);
	if (id == LV_BTNMATRIX_BTN_NONE) return;
	const char* txt = lv_keyboard_get_btn_text(kb, id);
	if (!txt) return;
	lv_obj_t* ta = lv_keyboard_get_textarea(kb);

	if (!strcmp(txt, "ABC")) {
		lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_UPPER);
	} else if (!strcmp(txt, "abc")) {
		lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
	} else if (!strcmp(txt, "1#") || !strcmp(txt, "123")) {
		lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_SPECIAL);
	} else if (!strcmp(txt, "#+=")) {
		lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_USER_1);
	} else if (!strcmp(txt, LV_SYMBOL_OK)) {
		savePassword();
	} else if (ta && !strcmp(txt, LV_SYMBOL_BACKSPACE)) {
		lv_textarea_del_char(ta);
	} else if (ta) {
		lv_textarea_add_text(ta, txt);
		// Shift is one-shot, as on a phone
		if (lv_keyboard_get_mode(kb) == LV_KEYBOARD_MODE_TEXT_UPPER) lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
	}
}

// ---------------- list screen ----------------

void rowClicked(lv_event_t* e) {
	const size_t idx = (size_t) (intptr_t) lv_event_get_user_data(e);
	if (idx >= entryCount) return;
	strlcpy(pwSsid, entries[idx].ssid, sizeof(pwSsid));
	pwOpen = entries[idx].open;
	pwKnown = entries[idx].known;
	openPasswordScreen();
}

void makeRow(size_t i, const WifiScanEntry& e) {
	lv_obj_t* row = lv_obj_create(objects.rrwifi_list);
	lv_obj_set_pos(row, 0, (lv_coord_t) (i * ROW_PITCH));
	lv_obj_set_size(row, ROW_W, ROW_H);
	lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_style(row, &rowStyle, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_add_style(row, &rowPressedStyle, LV_PART_MAIN | LV_STATE_PRESSED);
	lv_obj_add_event_cb(row, rowClicked, LV_EVENT_CLICKED, (void*) (intptr_t) i);

	lv_obj_t* name = lv_label_create(row);
	lv_obj_set_width(name, 200);
	lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
	lv_obj_set_style_text_font(name, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_set_style_text_color(name, lv_color_hex(e.known ? COLOR_BRASS : COLOR_PARCHMENT), LV_PART_MAIN | LV_STATE_DEFAULT);
	lv_obj_align(name, LV_ALIGN_LEFT_MID, 16, 0);
	lv_label_set_text(name, e.ssid);

	if (e.known || e.open) {
		lv_obj_t* tag = lv_label_create(row);
		lv_obj_set_style_text_font(tag, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_set_style_text_color(tag, lv_color_hex(COLOR_MUTED), LV_PART_MAIN | LV_STATE_DEFAULT);
		lv_obj_align(tag, LV_ALIGN_RIGHT_MID, -16, 0);
		lv_label_set_text(tag, e.known ? "gespeichert" : "offen");
	}
}

// Rebuilds the rows. The only child that stays is rrwifi_empty (the canvas' placeholder).
void rebuildList() {
	for (int32_t c = (int32_t) lv_obj_get_child_cnt(objects.rrwifi_list) - 1; c >= 0; c--) {
		lv_obj_t* child = lv_obj_get_child(objects.rrwifi_list, c);
		if (child != objects.rrwifi_empty) lv_obj_del(child);
	}
	entryCount = webserver.getScan(entries, WifiWebserver::SCAN_MAX);
	for (size_t i = 0; i < entryCount; i++) makeRow(i, entries[i]);
	lv_obj_scroll_to_y(objects.rrwifi_list, 0, LV_ANIM_OFF);
	if (entryCount) lv_obj_add_flag(objects.rrwifi_empty, LV_OBJ_FLAG_HIDDEN);
	else lv_obj_clear_flag(objects.rrwifi_empty, LV_OBJ_FLAG_HIDDEN);
}

void updateList() {
	WifiStatus st;
	webserver.getStatus(st);

	char text[96];
	switch (st.phase) {
	case WifiPhase::OFF:          snprintf(text, sizeof(text), "WLAN aus"); break;
	case WifiPhase::SCANNING:     snprintf(text, sizeof(text), "suche Netze ..."); break;
	case WifiPhase::CONNECTING:   snprintf(text, sizeof(text), "verbinde: %s", st.ssid); break;
	case WifiPhase::WAITING:      snprintf(text, sizeof(text), "kein bekanntes Netz"); break;
	case WifiPhase::ONLINE:       snprintf(text, sizeof(text), "%s  %s", st.ssid, st.ip); break;
	case WifiPhase::ACCESS_POINT: snprintf(text, sizeof(text), "Hotspot %s", st.ssid); break;
	default:                      text[0] = '\0';
	}
	setText(objects.rrwifi_status, text);

	// "suche ..." from the tap until the tick has taken the request over and the scan has ended
	if (st.scanning) {
		scanTappedAt = 0;
	} else if (scanTappedAt && lv_tick_elaps(scanTappedAt) > SCAN_PENDING_MS) {
		scanTappedAt = 0;
	}
	const bool scanning = st.scanning || scanTappedAt;
	setText(objects.rrwifi_btn_scan_lbl, scanning ? "suche ..." : "Suchen");
	setPillEnabled(objects.rrwifi_btn_scan, objects.rrwifi_btn_scan_lbl, !scanning);
	setText(objects.rrwifi_empty, scanning ? "suche ..." : "Suchen antippen");

	if (st.scanVersion != shownVersion) {
		shownVersion = st.scanVersion;
		rebuildList();
	}
}

void timerCb(lv_timer_t* t) {
	(void) t;
	if (lv_scr_act() == objects.rim_ridge_wifi) updateList();
}

}	// namespace

void ui_RimRidgeWifiInit() {
	if (!stylesReady) {
		lv_style_init(&rowStyle);
		lv_style_set_bg_color(&rowStyle, lv_color_hex(COLOR_PANEL));
		lv_style_set_bg_opa(&rowStyle, LV_OPA_COVER);
		lv_style_set_radius(&rowStyle, 20);
		lv_style_set_border_color(&rowStyle, lv_color_hex(COLOR_BRASS));
		lv_style_set_border_width(&rowStyle, 2);
		lv_style_set_border_opa(&rowStyle, 90);
		lv_style_set_pad_all(&rowStyle, 0);
		lv_style_init(&rowPressedStyle);
		lv_style_set_bg_color(&rowPressedStyle, lv_color_hex(COLOR_TOUR));
		lv_style_set_border_opa(&rowPressedStyle, LV_OPA_COVER);
		stylesReady = true;
	}

	lv_obj_t* kb = objects.rrwpw_kb;
	lv_obj_remove_event_cb(kb, lv_keyboard_def_event_cb);
	lv_obj_add_event_cb(kb, keyboardEvent, LV_EVENT_VALUE_CHANGED, NULL);
	lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, KB_LOWER, KB_LOWER_CTRL);
	lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER, KB_UPPER, KB_LOWER_CTRL);
	lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_SPECIAL, KB_SYMBOLS, KB_SYMBOLS_CTRL);
	lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_USER_1, KB_MORE, KB_SYMBOLS_CTRL);
	lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);

	// The canvas' example content
	lv_textarea_set_text(objects.rrwpw_ta, "");
	lv_label_set_text(objects.rrwifi_status, "");
	lv_timer_create(timerCb, 400, NULL);
}

// ---------------- EEZ actions ----------------

void action_go_to_wifi(lv_event_t* e) {
	(void) e;
	shownVersion = UINT32_MAX;
	updateList();
	lv_disp_load_scr(objects.rim_ridge_wifi);
}

void action_wifi_back(lv_event_t* e) {
	(void) e;
	lv_disp_load_scr(objects.rim_ridge_settings);
}

// Any swipe returns to the settings, like on the other sub screens.
void action_wifi_screen_gesture(lv_event_t* e) {
	(void) e;
	lv_indev_t* indev = lv_indev_get_act();
	if (!indev) return;
	lv_indev_wait_release(indev);
	lv_disp_load_scr(objects.rim_ridge_settings);
}

void action_wifi_scan(lv_event_t* e) {
	(void) e;
	if (webserver.requestScan()) {
		scanTappedAt = lv_tick_get() | 1;		// never 0 = "no tap"
		updateList();
	}
}

void action_wifi_pw_cancel(lv_event_t* e) {
	(void) e;
	closePasswordScreen();
}

void action_wifi_pw_save(lv_event_t* e) {
	(void) e;
	savePassword();
}

void action_wifi_pw_eye(lv_event_t* e) {
	(void) e;
	const bool hidden = lv_textarea_get_password_mode(objects.rrwpw_ta);
	lv_textarea_set_password_mode(objects.rrwpw_ta, !hidden);
	lv_label_set_text(objects.rrwpw_btn_eye_lbl, hidden ? "***" : "abc");
}
