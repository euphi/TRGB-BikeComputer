/*
 * RimRidgeAltCustFunc.cpp
 *
 * See RimRidgeAltCustFunc.h for scope notes.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_eez/screens.h"
#include "ui_eez/ui.h"
#include "ui_eez/actions.h"
#include "RimRidgeAltCustFunc.h"
#include "Singletons.h"		// sensors (TRGBBC_SENSORS_I2C only)
#ifdef TRGBBC_SENSORS_I2C
#include "I2CSensors.h"
#endif

namespace {

// Hardcoded for the same reason as in RimRidgeSettingsCustFunc.cpp (no access to theme_colors[]):
// RRMuted, RRParchment, RRZoneRed.
const uint32_t COLOR_MUTED = 0x9BA097;
const uint32_t COLOR_ACTIVE = 0xE7E2D6;
const uint32_t COLOR_ERROR = 0xC1604A;

const uint32_t STATUS_SHOWN_MS = 8000;			// how long a result stays on the Alt page

// "123.4" by integer arithmetic: the actions below run inside lv_timer_handler() on the UI task,
// which has less than 1 KB of stack to spare, and float printf takes several hundred bytes of it.
void fmtTenths(char* buf, size_t n, float v) {
	const long t = lroundf(v * 10.0f);
	snprintf(buf, n, "%s%ld.%ld", t < 0 ? "-" : "", labs(t) / 10, labs(t) % 10);
}

void setText(lv_obj_t* label, const char* text) {
	if (strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void setColor(lv_obj_t* label, uint32_t color) {
	lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
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

#ifdef TRGBBC_SENSORS_I2C

// Result line of the Alt page: set by an action, shown for STATUS_SHOWN_MS.
char statusText[40];
uint32_t statusColor = COLOR_MUTED;
uint32_t statusAt = 0;			// lv_tick of the last message, 0 = none

void setStatus(const char* text, uint32_t color) {
	snprintf(statusText, sizeof(statusText), "%s", text);
	statusColor = color;
	statusAt = lv_tick_get() | 1;		// never 0 = "none"
}

const char* resultText(I2CSensors::HeightCalResult r) {
	switch (r) {
	case I2CSensors::HeightCalResult::OK: return "";
	case I2CSensors::HeightCalResult::NO_PRESSURE: return "kein Druckwert";
	case I2CSensors::HeightCalResult::OUT_OF_RANGE: return "Wert außerhalb des Bereichs";
	case I2CSensors::HeightCalResult::NO_GPS_HEIGHT: return "keine GPS-Höhe von TrailBridge";
	}
	return "";
}

void reportCalibrated(I2CSensors::HeightCalResult r, float heightM, const char* via) {
	char buf[40];
	if (r == I2CSensors::HeightCalResult::OK) {
		snprintf(buf, sizeof(buf), "%s: %ld m", via, lroundf(heightM));
		setStatus(buf, COLOR_ACTIVE);
	} else {
		setStatus(resultText(r), COLOR_ERROR);
	}
	ui_RimRidgeAltUpdate();
}

// ---------------- number entry ----------------
enum NumTarget : uint8_t {NUM_HEIGHT = 0, NUM_SEALEVEL = 1, NUM_PRESET = 2};
NumTarget numTarget = NUM_HEIGHT;
uint8_t numPreset = 0;

void setHint(const char* text, uint32_t color) {
	lv_label_set_text(objects.rrsn_hint, text);
	setColor(objects.rrsn_hint, color);
}

// Title, unit, hint and mode pills for the current target. The text field is cleared.
void showNumTarget() {
	char buf[24];
	const bool manual = (numTarget != NUM_PRESET);
	if (manual) {
		lv_obj_clear_flag(objects.rrsn_btn_mode_h, LV_OBJ_FLAG_HIDDEN);
		lv_obj_clear_flag(objects.rrsn_btn_mode_p, LV_OBJ_FLAG_HIDDEN);
		if (numTarget == NUM_HEIGHT) {
			lv_obj_add_state(objects.rrsn_btn_mode_h, LV_STATE_CHECKED);
			lv_obj_clear_state(objects.rrsn_btn_mode_p, LV_STATE_CHECKED);
		} else {
			lv_obj_clear_state(objects.rrsn_btn_mode_h, LV_STATE_CHECKED);
			lv_obj_add_state(objects.rrsn_btn_mode_p, LV_STATE_CHECKED);
		}
	} else {
		lv_obj_add_flag(objects.rrsn_btn_mode_h, LV_OBJ_FLAG_HIDDEN);
		lv_obj_add_flag(objects.rrsn_btn_mode_p, LV_OBJ_FLAG_HIDDEN);
	}

	const I2CSensors::HeightCalState c = sensors.getHeightCalState();
	char placeholder[16];
	if (numTarget == NUM_SEALEVEL) {
		lv_label_set_text(objects.rrsn_title, "NN-DRUCK");
		lv_label_set_text(objects.rrsn_unit, "hPa");
		snprintf(buf, sizeof(buf), "%d bis %d hPa", (int) I2CSensors::SEA_LEVEL_MIN_HPA, (int) I2CSensors::SEA_LEVEL_MAX_HPA);
		fmtTenths(placeholder, sizeof(placeholder), c.seaLevelHPa);
	} else {
		if (numTarget == NUM_PRESET) {
			snprintf(buf, sizeof(buf), "PRESET %u", numPreset + 1);
			lv_label_set_text(objects.rrsn_title, buf);
			snprintf(placeholder, sizeof(placeholder), "%ld", lroundf(c.presetM[numPreset]));
		} else {
			lv_label_set_text(objects.rrsn_title, "HÖHE");
			if (isnan(c.heightM)) strcpy(placeholder, "0");
			else snprintf(placeholder, sizeof(placeholder), "%ld", lroundf(c.heightM));
		}
		lv_label_set_text(objects.rrsn_unit, "m");
		snprintf(buf, sizeof(buf), "%d bis %d m", (int) I2CSensors::HEIGHT_MIN_M, (int) I2CSensors::HEIGHT_MAX_M);
	}
	lv_textarea_set_placeholder_text(objects.rrsn_ta, placeholder);
	lv_textarea_set_text(objects.rrsn_ta, "");
	setHint(buf, COLOR_MUTED);
}

void openNumScreen(NumTarget target, uint8_t preset) {
	numTarget = target;
	numPreset = preset;
	showNumTarget();
	lv_disp_load_scr(objects.rim_ridge_settings_num);
}

void closeNumScreen() {
	ui_RimRidgeAltUpdate();
	lv_disp_load_scr(objects.rim_ridge_settings_alt);
}

void saveNumber() {
	const char* text = lv_textarea_get_text(objects.rrsn_ta);
	char* end = nullptr;
	const float v = strtof(text, &end);
	if (end == text || *end != '\0') {		// empty, or only "-" / "."
		setHint("Wert fehlt", COLOR_ERROR);
		return;
	}

	I2CSensors::HeightCalResult r;
	switch (numTarget) {
	case NUM_SEALEVEL: r = sensors.setSeaLevelPressure(v); break;
	case NUM_PRESET: r = sensors.setHeightPreset(numPreset, v); break;
	default: r = sensors.calibrateHeight(v);
	}
	if (r != I2CSensors::HeightCalResult::OK) {
		setHint(resultText(r), COLOR_ERROR);
		return;
	}

	char buf[40];
	if (numTarget == NUM_SEALEVEL) {
		char val[16];
		fmtTenths(val, sizeof(val), v);
		snprintf(buf, sizeof(buf), "NN-Druck: %s hPa", val);
	} else if (numTarget == NUM_PRESET) {
		snprintf(buf, sizeof(buf), "Preset %u: %ld m", numPreset + 1, lroundf(v));
	} else {
		snprintf(buf, sizeof(buf), "manuell: %ld m", lroundf(v));
	}
	setStatus(buf, COLOR_ACTIVE);
	lv_textarea_set_text(objects.rrsn_ta, "");
	closeNumScreen();
}

// 4 columns, rows of 3 digits + one control key; the last row is a wide 0 and clear.
// Control keys carry CHECKED, which the EEZ style draws as TourBg/brass.
#define KEY(w) (LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_CLICK_TRIG | (w))
#define BKSP(w) (LV_BTNMATRIX_CTRL_CHECKED | (w))		// no NO_REPEAT: held down it deletes on
#define CTL(w) (LV_BTNMATRIX_CTRL_NO_REPEAT | LV_BTNMATRIX_CTRL_CLICK_TRIG | LV_BTNMATRIX_CTRL_CHECKED | (w))

const char* KB_NUM[] = {"1", "2", "3", LV_SYMBOL_BACKSPACE, "\n",
                        "4", "5", "6", "-", "\n",
                        "7", "8", "9", ".", "\n",
                        "0", "C", ""};
const lv_btnmatrix_ctrl_t KB_NUM_CTRL[] = {
	KEY(3), KEY(3), KEY(3), BKSP(3),
	KEY(3), KEY(3), KEY(3), CTL(3),
	KEY(3), KEY(3), KEY(3), CTL(3),
	KEY(9), CTL(3)};

void keyboardEvent(lv_event_t* e) {
	lv_obj_t* kb = lv_event_get_target(e);
	const uint16_t id = lv_keyboard_get_selected_btn(kb);
	if (id == LV_BTNMATRIX_BTN_NONE) return;
	const char* txt = lv_keyboard_get_btn_text(kb, id);
	lv_obj_t* ta = lv_keyboard_get_textarea(kb);
	if (!txt || !ta) return;

	const char* cur = lv_textarea_get_text(ta);
	if (!strcmp(txt, LV_SYMBOL_BACKSPACE)) {
		lv_textarea_del_char(ta);
	} else if (!strcmp(txt, "C")) {
		lv_textarea_set_text(ta, "");
	} else if (!strcmp(txt, "-")) {			// sign toggle: a minus is only valid in front
		char buf[16];
		if (cur[0] == '-') snprintf(buf, sizeof(buf), "%s", cur + 1);
		else snprintf(buf, sizeof(buf), "-%s", cur);
		lv_textarea_set_text(ta, buf);
	} else if (!strcmp(txt, ".")) {
		if (cur[0] != '\0' && strchr(cur, '.') == nullptr) lv_textarea_add_text(ta, ".");
	} else {
		lv_textarea_add_text(ta, txt);
	}
}

#endif // TRGBBC_SENSORS_I2C

}	// namespace

void ui_RimRidgeAltInit() {
#ifdef TRGBBC_SENSORS_I2C
	lv_obj_t* kb = objects.rrsn_kb;
	lv_obj_remove_event_cb(kb, lv_keyboard_def_event_cb);
	lv_obj_add_event_cb(kb, keyboardEvent, LV_EVENT_VALUE_CHANGED, NULL);
	lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_NUMBER, KB_NUM, KB_NUM_CTRL);
	lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_NUMBER);
	lv_textarea_set_text(objects.rrsn_ta, "");
	lv_label_set_text(objects.rrsa_status, "");
#endif
}

void ui_RimRidgeAltUpdate() {
#ifdef TRGBBC_SENSORS_I2C
	const I2CSensors::HeightCalState c = sensors.getHeightCalState();
	char buf[48];

	if (isnan(c.heightM)) strcpy(buf, "-- m");
	else snprintf(buf, sizeof(buf), "%ld m", lroundf(c.heightM));
	setText(objects.rrsa_height, buf);
	setText(objects.rrset_nav_alt_val, buf);

	char sea[16], pressure[16];
	fmtTenths(sea, sizeof(sea), c.seaLevelHPa);
	if (isnan(c.pressHPa)) {
		snprintf(buf, sizeof(buf), "kein Druckwert  |  NN %s hPa", sea);
	} else {
		fmtTenths(pressure, sizeof(pressure), c.pressHPa);
		snprintf(buf, sizeof(buf), "%s hPa  |  NN %s hPa", pressure, sea);
	}
	setText(objects.rrsa_info, buf);

	lv_obj_t* presetLabels[] = {objects.rrsa_btn_p1_lbl, objects.rrsa_btn_p2_lbl, objects.rrsa_btn_p3_lbl};
	lv_obj_t* presetPills[] = {objects.rrsa_btn_p1, objects.rrsa_btn_p2, objects.rrsa_btn_p3};
	const bool haveP = !isnan(c.pressHPa);
	for (uint8_t i = 0; i < I2CSensors::HEIGHT_PRESET_COUNT; i++) {
		snprintf(buf, sizeof(buf), "%ld m", lroundf(c.presetM[i]));
		setText(presetLabels[i], buf);
		setPillEnabled(presetPills[i], presetLabels[i], haveP);
	}
	// The GPS pill shows the height it would calibrate to, as the presets do.
	float gpsM;
	const bool haveGps = sensors.getGpsHeight(gpsM);
	if (haveGps) snprintf(buf, sizeof(buf), "GPS %ld m", lroundf(gpsM));
	else strcpy(buf, "GPS");
	setText(objects.rrsa_btn_gps_lbl, buf);
	setPillEnabled(objects.rrsa_btn_gps, objects.rrsa_btn_gps_lbl, haveP && haveGps);

	if (statusAt && lv_tick_elaps(statusAt) < STATUS_SHOWN_MS) {
		setText(objects.rrsa_status, statusText);
		setColor(objects.rrsa_status, statusColor);
	} else {
		statusAt = 0;
		setText(objects.rrsa_status, "");
	}
#else
	setText(objects.rrset_nav_alt_val, "nicht verfügbar");
	setColor(objects.rrset_nav_alt_val, COLOR_MUTED);
#endif
}

// ---------------- EEZ actions ----------------
#ifdef TRGBBC_SENSORS_I2C

void action_alt_preset(lv_event_t* e) {
	const uint8_t i = (uint8_t) (intptr_t) lv_event_get_user_data(e);
	const float h = sensors.getHeightPreset(i);
	char via[16];
	snprintf(via, sizeof(via), "Preset %u", i + 1);
	reportCalibrated(sensors.calibrateHeight(h), h, via);
}

void action_alt_preset_edit(lv_event_t* e) {
	const uint8_t i = (uint8_t) (intptr_t) lv_event_get_user_data(e);
	if (i < I2CSensors::HEIGHT_PRESET_COUNT) openNumScreen(NUM_PRESET, i);
}

void action_alt_gps(lv_event_t* e) {
	(void) e;
	float h = NAN;
	const I2CSensors::HeightCalResult r = sensors.calibrateHeightFromGps(&h);
	reportCalibrated(r, h, "GPS");
}

void action_alt_manual(lv_event_t* e) {
	(void) e;
	openNumScreen(NUM_HEIGHT, 0);
}

void action_alt_num_mode(lv_event_t* e) {
	const NumTarget t = ((intptr_t) lv_event_get_user_data(e) == 1) ? NUM_SEALEVEL : NUM_HEIGHT;
	if (t == numTarget) return;
	numTarget = t;
	showNumTarget();
}

void action_alt_num_cancel(lv_event_t* e) {
	(void) e;
	lv_textarea_set_text(objects.rrsn_ta, "");
	closeNumScreen();
}

void action_alt_num_save(lv_event_t* e) {
	(void) e;
	saveNumber();
}

#else // !TRGBBC_SENSORS_I2C - FL variant, no BME280

void action_alt_preset(lv_event_t* e) {(void) e;}
void action_alt_preset_edit(lv_event_t* e) {(void) e;}
void action_alt_gps(lv_event_t* e) {(void) e;}
void action_alt_manual(lv_event_t* e) {(void) e;}
void action_alt_num_mode(lv_event_t* e) {(void) e;}
void action_alt_num_cancel(lv_event_t* e) {(void) e;}
void action_alt_num_save(lv_event_t* e) {(void) e;}

#endif
