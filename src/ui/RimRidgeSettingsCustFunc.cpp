/*
 * RimRidgeSettingsCustFunc.cpp
 *
 * See RimRidgeSettingsCustFunc.h for scope notes.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "ui_eez/screens.h"
#include "ui_eez/ui.h"
#include "ui_eez/actions.h"
#include "RimRidgeSettingsCustFunc.h"
#include "Singletons.h"	// ui, webserver, stats, bclog, trgb, sensors (TRGBBC_SENSORS_I2C only)
#include "Stats/Distance.h"
#include <version.h>
#ifdef TRGBBC_SENSORS_I2C
#include "I2CSensors.h"
#endif

// Status text colors - hardcoded for the same reason as in RimRidgeCustFunc.cpp
// (no access to the generated theme_colors[] here): RRMuted, RRParchment, RRZoneRed.
static const uint32_t COLOR_MUTED = 0x9BA097;
static const uint32_t COLOR_ACTIVE = 0xE7E2D6;
static const uint32_t COLOR_ERROR = 0xC1604A;

// ---------------- restart / deep sleep ----------------
// Both pills fire on LONG_PRESSED, but the action only arms the request: deep sleep
// wakes on the touch controller's interrupt, so going to sleep while the finger is
// still on the glass would wake the device right away. rrsetShutdownTimer() starts it
// once the touch is released.
enum PendingShutdown : uint8_t {SHUTDOWN_NONE = 0, SHUTDOWN_RESTART, SHUTDOWN_SLEEP};
static PendingShutdown pendingShutdown = SHUTDOWN_NONE;
static bool shutdownStarted = false;

static bool touchReleased() {
	for (lv_indev_t* indev = lv_indev_get_next(NULL); indev; indev = lv_indev_get_next(indev)) {
		if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER && indev->proc.state == LV_INDEV_STATE_PRESSED) {
			return false;
		}
	}
	return true;
}

// Own task: NVS, SD flush and trgb.deepSleep()'s 2 s delay don't belong on the UI task,
// which has less than 1 KB of stack to spare (STACK UI Task in the debug log).
static void shutdownTask(void* arg) {
	const bool sleep = (arg != nullptr);
	bclog.log(BCLogger::Log_Info, BCLogger::TAG_OP, sleep ? "Deep sleep requested on the settings screen"
	                                                    : "Restart requested on the settings screen");
	stats.persistNow();		// otherwise up to 5 min of statistics are lost (Statistics::PERSIST_INTERVAL_MS)
	bclog.flushFiles();
	if (sleep) {
		trgb.deepSleep();
	} else {
		trgb.restart();
	}
	vTaskDelete(NULL);		// not reached
}

static void rrsetShutdownTimer(lv_timer_t* t) {
	(void) t;
	if (pendingShutdown == SHUTDOWN_NONE || shutdownStarted || !touchReleased()) return;
	const bool sleep = (pendingShutdown == SHUTDOWN_SLEEP);
	shutdownStarted = true;
	lv_label_set_text(objects.rrset_hint, sleep ? "Tiefschlaf ..." : "Neustart ...");
	if (xTaskCreate(shutdownTask, "Shutdown", 6144, sleep ? (void*) 1 : nullptr, 5, nullptr) != pdPASS) {
		bclog.log(BCLogger::Log_Error, BCLogger::TAG_OP, "Could not start the shutdown task");
		lv_label_set_text(objects.rrset_hint, "Fehler - bitte erneut");
		shutdownStarted = false;
		pendingShutdown = SHUTDOWN_NONE;
	}
}

static void armShutdown(PendingShutdown what) {
	pendingShutdown = what;
	lv_label_set_text(objects.rrset_hint, what == SHUTDOWN_SLEEP ? "loslassen: Tiefschlaf" : "loslassen: Neustart");
	lv_obj_set_style_text_color(objects.rrset_hint, lv_color_hex(COLOR_ACTIVE), LV_PART_MAIN | LV_STATE_DEFAULT);
}

void action_settings_reset(lv_event_t* e) {
	(void) e;
	armShutdown(SHUTDOWN_RESTART);
}

void action_settings_sleep(lv_event_t* e) {
	(void) e;
	armShutdown(SHUTDOWN_SLEEP);
}

// ---------------- screen switching ----------------
void action_go_to_settings(lv_event_t* e) {
	(void) e;
	ui.showSettingsScreen();
}

// Any swipe returns to the main screen, same as on RimRidgeRQ.
void action_settings_screen_gesture(lv_event_t* e) {
	(void) e;
	lv_indev_t* indev = lv_indev_get_act();
	if (!indev) return;
	lv_indev_wait_release(indev);
	ui.hideSettingsScreen();
}

// ---------------- build / WLAN ----------------
void ui_RimRidgeSettingsInit() {
	char build[48];
#if defined(COMMIT_VERSION) && defined(GIT_HASH)
	const char* hash = GIT_HASH;
	if (hash[0] == 'g') hash++;		// git describe prefixes the abbreviated hash with 'g'
	snprintf(build, sizeof(build), "%s-%s (%s) #%s", GIT_VERSION, COMMIT_VERSION, hash, BUILD_NUMBER);
#else
	snprintf(build, sizeof(build), "%s #%s", GIT_VERSION, BUILD_NUMBER);
#endif
	lv_label_set_text(objects.rrset_build_val, build);
	ui_RimRidgeSettingsUpdateCal();
	lv_timer_create(rrsetShutdownTimer, 100, NULL);
}

static uint8_t wifiState = RRSET_WIFI_OFF;		// what the pills show, for the actions below

void ui_RimRidgeSettingsUpdateWifi(const char* ipText, uint8_t state, const char* caption) {
	if (state > RRSET_WIFI_AP) state = RRSET_WIFI_OFF;
	wifiState = state;
	lv_label_set_text(objects.rrset_ip_val, ipText);
	lv_label_set_text(objects.rrset_ip_caption, (caption && caption[0]) ? caption : "IP-ADRESSE");
	const bool radioOn = (state == RRSET_WIFI_CONNECTING || state == RRSET_WIFI_ONLINE);
	lv_label_set_text(objects.rrset_btn_wifi_lbl, radioOn ? "WLAN aus" : "WLAN an");
	lv_label_set_text(objects.rrset_btn_ap_lbl, state == RRSET_WIFI_AP ? "Hotspot aus" : "Hotspot");
}

// WifiWebserver::checkLoop() reports back via UIFacade::updateIP()
void action_settings_wifi(lv_event_t* e) {
	(void) e;
	if (wifiState == RRSET_WIFI_CONNECTING || wifiState == RRSET_WIFI_ONLINE) {
		webserver.requestDisable();
	} else {
		webserver.requestReconnect();
	}
}

void action_settings_ap(lv_event_t* e) {
	(void) e;
	webserver.requestAccessPoint(wifiState != RRSET_WIFI_AP);
}

// ---------------- calibration / reference ride ----------------
static void setStatus(lv_obj_t* label, const char* text, uint32_t color) {
	lv_label_set_text(label, text);
	lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
}

#ifdef TRGBBC_SENSORS_I2C

// " 27.09." for a known timestamp, "" if the clock wasn't set back then.
static void appendDate(char* buf, size_t len, time_t t) {
	if (t == 0) return;
	struct tm lt;
	localtime_r(&t, &lt);
	size_t n = strlen(buf);
	snprintf(buf + n, len - n, " %02d.%02d.", lt.tm_mday, lt.tm_mon + 1);
}

void ui_RimRidgeSettingsUpdateCal() {
	const I2CSensors::CalibrationState c = sensors.getCalibrationState();
	char buf[48];

	if (!c.imuRunning) {
		setStatus(objects.rrset_cal_status, "IMU nicht aktiv", COLOR_ERROR);
		setStatus(objects.rrset_ref_status, "IMU nicht aktiv", COLOR_ERROR);
		lv_obj_add_state(objects.rrset_btn_cal, LV_STATE_DISABLED);
		lv_obj_add_state(objects.rrset_btn_cal_lbl, LV_STATE_DISABLED);
		lv_obj_add_state(objects.rrset_btn_ref, LV_STATE_DISABLED);
		lv_obj_add_state(objects.rrset_btn_ref_lbl, LV_STATE_DISABLED);
		return;
	}
	lv_obj_clear_state(objects.rrset_btn_ref, LV_STATE_DISABLED);
	lv_obj_clear_state(objects.rrset_btn_ref_lbl, LV_STATE_DISABLED);

	// IMU calibration: 3 s standing still
	const bool calRunning = (c.calState == I2CSensors::CAL_RUNNING);
	if (calRunning) {
		lv_obj_add_state(objects.rrset_btn_cal, LV_STATE_DISABLED);
		lv_obj_add_state(objects.rrset_btn_cal_lbl, LV_STATE_DISABLED);
	} else {
		lv_obj_clear_state(objects.rrset_btn_cal, LV_STATE_DISABLED);
		lv_obj_clear_state(objects.rrset_btn_cal_lbl, LV_STATE_DISABLED);
	}
	switch (c.calState) {
	case I2CSensors::CAL_RUNNING:
		snprintf(buf, sizeof(buf), "still halten ... %u %%", c.calPercent);
		setStatus(objects.rrset_cal_status, buf, COLOR_ACTIVE);
		break;
	case I2CSensors::CAL_ERR_MOTION:
		setStatus(objects.rrset_cal_status, "Fehler: bewegt", COLOR_ERROR);
		break;
	case I2CSensors::CAL_ERR_SCALE:
		setStatus(objects.rrset_cal_status, "Fehler: Skalierung", COLOR_ERROR);
		break;
	default:	// CAL_NONE / CAL_OK (also set when a stored calibration is loaded at boot)
		if (c.calValid) {
			strcpy(buf, "kalibriert");
			appendDate(buf, sizeof(buf), c.calTime);
			setStatus(objects.rrset_cal_status, buf, COLOR_MUTED);
		} else {
			setStatus(objects.rrset_cal_status, "nicht kalibriert", COLOR_ERROR);
		}
	}

	// Reference ride: refTargetS accepted seconds at >= refMinKmh on smooth asphalt
	switch (c.refState) {
	case RQ::RoadQuality::RefState::RUNNING:
		lv_label_set_text(objects.rrset_btn_ref_lbl, "Abbrechen");
		snprintf(buf, sizeof(buf), "%u / %u s ab %.0f km/h", c.refProgressS, c.refTargetS, c.refMinKmh);
		setStatus(objects.rrset_ref_status, buf, COLOR_ACTIVE);
		break;
	case RQ::RoadQuality::RefState::FAILED:
		lv_label_set_text(objects.rrset_btn_ref_lbl, "Referenzfahrt");
		setStatus(objects.rrset_ref_status, "fehlgeschlagen", COLOR_ERROR);
		break;
	default:	// IDLE / DONE: show the baseline in use
		lv_label_set_text(objects.rrset_btn_ref_lbl, "Referenzfahrt");
		if (c.baselineCal) {
			snprintf(buf, sizeof(buf), "%.0f mg", c.baselineG * 1000);
			appendDate(buf, sizeof(buf), c.baselineTime);
			setStatus(objects.rrset_ref_status, buf,
			          c.refState == RQ::RoadQuality::RefState::DONE ? COLOR_ACTIVE : COLOR_MUTED);
		} else {
			snprintf(buf, sizeof(buf), "Standard (%.0f mg)", c.baselineG * 1000);
			setStatus(objects.rrset_ref_status, buf, COLOR_MUTED);
		}
	}
}

void action_settings_cal(lv_event_t* e) {
	(void) e;
	if (sensors.requestIMUCalibration()) {
		// The ImuTask picks the request up within one poll - show it right away.
		setStatus(objects.rrset_cal_status, "still halten ...", COLOR_ACTIVE);
	}
}

void action_settings_ref(lv_event_t* e) {
	(void) e;
	const bool running = sensors.getCalibrationState().refState == RQ::RoadQuality::RefState::RUNNING;
	if (sensors.requestRefRide(!running) && !running) {
		lv_label_set_text(objects.rrset_btn_ref_lbl, "Abbrechen");
		setStatus(objects.rrset_ref_status, "gestartet", COLOR_ACTIVE);
	}
}

#else // !TRGBBC_SENSORS_I2C - FL variant, no BMI160

void ui_RimRidgeSettingsUpdateCal() {
	setStatus(objects.rrset_cal_status, "nicht verfügbar", COLOR_MUTED);
	setStatus(objects.rrset_ref_status, "nicht verfügbar", COLOR_MUTED);
	lv_obj_add_state(objects.rrset_btn_cal, LV_STATE_DISABLED);
	lv_obj_add_state(objects.rrset_btn_cal_lbl, LV_STATE_DISABLED);
	lv_obj_add_state(objects.rrset_btn_ref, LV_STATE_DISABLED);
	lv_obj_add_state(objects.rrset_btn_ref_lbl, LV_STATE_DISABLED);
}

void action_settings_cal(lv_event_t* e) {(void) e;}
void action_settings_ref(lv_event_t* e) {(void) e;}

#endif
