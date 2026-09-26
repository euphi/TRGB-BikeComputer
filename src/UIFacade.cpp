/*
 * UIFacade.cpp
 *
 *  Created on: 03.03.2023
 *      Author: ian
 */

#include <UIFacade.h>
#include "Singletons.h"
#include "BikeNavProtocol.h"


// Screens

#include "ui/img/state-icons.h"

#include "ui/Screens/Settings/ui_Settings.h"

#include <ui/Screens/Chart/ui.h>
#include <ui/Screens/Chart/ui_Chart_CustFunc.h>

#include "ui/ui.h"  // FL main and chart screen
#include "ui/ui_custFunc.h"

// EEZ Studio-generated "RimRidge" main screen (feature/rimridge-ui-design).
// Project lives at EEZStudio/TRGB-BikeComputer.eez-project, destinationFolder
// ../src/ui_eez (deliberately NOT ../src/ui, so an EEZ Studio re-export never
// overwrites the still-present SquareLine-generated screens below). RimRidge
// is now the permanent main screen, so this include is no longer optional -
// hand-written wiring lives in RimRidgeCustFunc.cpp/.h (kept outside
// src/ui_eez/ since that whole directory gets replaced on every EEZ export).
#include "ui_eez/screens.h"
#include "ui_eez/ui.h"
#include "ui_eez/images.h"
#include "ui/RimRidgeCustFunc.h"
#include "ui/RimRidgeNavCustFunc.h"
#include "ui/RimRidgeRQCustFunc.h"

#include <DateTime.h>

void startTaskUiUpdate(void*) {
	ui.updateHandler();
}

UIFacade::UIFacade() {
    xUpdateFast = xSemaphoreCreateBinary();
	xUpdateSlow = xSemaphoreCreateBinary();
	xUIDrawMutex = xSemaphoreCreateMutex();
}

bool UIFacade::isDrawTask() {
	if (!uiTaskHandle) return false;
	TaskHandle_t currentTaskHandle = xTaskGetCurrentTaskHandle();
//	uint32_t taskID = uxTaskGetTaskNumber(currentTaskHandle);
//	uint32_t drawTaskID = uxTaskGetTaskNumber(uiTaskHandle);
//	return taskID == drawTaskID;
	return currentTaskHandle == uiTaskHandle;
}

void UIFacade::initDisplay() {
	// Init LVGL

	// 1. Default theme/font etc.
    lv_disp_t * dispp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    // 2. Init all screens
    ui_S1Main_screen_init();
    ui_ScreenChart_screen_init();		// old chart (included in SQS main screen project)
    ui_SChart_screen_init();			// new chart (own SQS project)

    ui_ScrSettings_screen_init();

    // .. add init of new screens here

    create_screen_rim_ridge();
    ui_RimRidgeUpdateNav(nullptr, 0, NAV_MANEUVER_NONE, 0); // start on the "no nav" icon, not rr_ic_turn's EEZ-authored default placeholder
    ui_RimRidgeUpdateLanes(nullptr, 0); // rr_lane_row starts visible in the EEZ canvas - hide it and confirm rr_nav_pill is at rest
    create_screen_rim_ridge_nav();
    ui_RimRidgeNavUpdateNav(0, NAV_MANEUVER_NONE, 0, "", NAV_MANEUVER_NONE, 0); // same "no nav" boot fixup as rr_ic_turn above - otherwise this screen keeps showing its EEZ-authored example maneuver/street/distance until the first real update
    ui_RimRidgeNavUpdateLanes(nullptr, 0); // rrnav_lane_row starts visible in the EEZ canvas - hide it
    create_screen_rim_ridge_rq();
    ui_RimRidgeRQUpdateNav(0, NAV_MANEUVER_NONE, 0); // same "no nav" boot fixup as rr_ic_turn above
    ui_RimRidgeRQInitLabelControls();
    ui_RimRidgeRQUpdateLabel(0, 0, false); // nothing labeled/capturing yet - normalize away the JSON's static "Schotter/3 selected" example content
    ui_RimRidgeRQInitNavLink(); // rq_nav_pill tap -> showNavScreen(), same target as rr_nav_pill's GoToNav action, but wired in C (see RimRidgeRQCustFunc.cpp)

    // 3. set main screen
    // RimRidge is now the permanent main/boot screen (2026-09-18).
    // MainNoFL/SNavi/SOTA/SWLAN were disabled-not-deleted then, and fully
    // removed 2026-09-26 once nothing outside their own folders referenced
    // them any more (see memory ui-tooling-eez-studio-migration). Chart and
    // Settings are still initialized (Chart's ui_ScrChartUpdateBat() has a
    // live dependency - see updateIntBatteryInt(); Settings has no RimRidge
    // replacement screen yet).
    ui_MainScreen = objects.rim_ridge;

    ui_ScrChartSetBackScreen(ui_MainScreen);

    // init data model
    uifl.init();		// FL data model

    // 4. Load initial screen
    lv_disp_load_scr(ui_MainScreen);

    // 5. Start fast update Thread
    xTaskCreate(startTaskUiUpdate, "UI Task", 4096, NULL, 20, &uiTaskHandle);	// High priority task for smooth display updates

    // 6. Start Update ticker
    dataTicker.attach_ms(1000, +[](UIFacade* thisInstance) {thisInstance->updateData();}, this);
}


// ---------------- Internal (private) ticker handlers (automatic update) ----------------

// data updater
void UIFacade::updateData() {
	xSemaphoreGive(xUpdateSlow);
}

// Internal task handler

/* updateHandler(): Update and Redraw Screens
 *
 * lvgl is not thread-safe, so semaphores and mutexes must be used here.
 *
 * There are 3 different update mechanism;
 *
 * "fast" - signaled by binary semaphore xUpdateFast
 * 		   - this redraws all quickly changing data like speed, cadence, heart rate
 *
 * "slow"  - signaled by binary semaphore xUpdateSlow.
 *         - it isn't that slow: a timer is used to give the semaphore every 250ms (4 Hz)
 *         - here, data that slowly changes is polled from data model. (`Statistics stats`)
 *
 * The mechanism for fast and slow is the same. The difference is that "slow" is allowed to take more time, because it is
 * handled AFTER the scheduled draw updated and the next scheduled draw is calculated accounting the time needed for slow update.
 *
 * The fast update mechanism is handled directly before the draw and thus delays drawing the UI.
 *
 * "direct" - data that is rarely updated may directly update lvgl objects. This MUST not be done during refresh, so a mutex (xUIDrawMutex) is used to protect refresh.
 *          - this may block refreshing and thus may lead to less smooth display animations or even some flickering. So use only for rarely updated labels (like IP adress).
 *
 * "unprotected direct" - if an update changes only values at a fixed adress, it is not necessary to protect it with a mutex. This is the case for array
 */
void UIFacade::updateHandler() {
	static unsigned long next_millis = 0;
	while (true) {
		// Fast update - use this only for data that should be shown with no (further) delay
		if (xSemaphoreTake(xUpdateFast, static_cast<TickType_t>(0) ) == pdTRUE) {		// Semaphore is used for message "please update" only. So there is no reason to wait.
			// Old-screen fan-out (ui_ScrMain*/ui_ScrNavi*/ui_SMainNoFL*) removed
			// 2026-09-18 when those screens were disabled, then the screens
			// themselves removed 2026-09-26 (see memory
			// ui-tooling-eez-studio-migration). ui_ScrChart* stays - Chart is
			// still initialized (see updateIntBatteryInt()).
			ui_RimRidgeUpdateSpeed(speed);
			ui_RimRidgeUpdateCadence(cad);
			ui_RimRidgeUpdateHR(hr);
			ui_RimRidgeUpdateGrad(grad, height);
			// RimRidgeNav shows the same speed/gradient/HR as RimRidge -
			// fan out from the same data here rather than duplicating the
			// NaN/formatting logic in a second call site.
			ui_RimRidgeNavUpdateSpeed(speed);
			ui_RimRidgeNavUpdateGrad(grad);
			ui_RimRidgeNavUpdateHR(hr);
			// RimRidgeRQ shows the same speed/HR too (no gradient widget there).
			ui_RimRidgeRQUpdateSpeed(speed);
			ui_RimRidgeRQUpdateHR(hr);
		}

		int32_t next_ms = 20; // wait 20ms if Mutex can't be taken within 100ms (this should never happen)
		if (xSemaphoreTake(xUIDrawMutex, static_cast<TickType_t>(100 / portTICK_PERIOD_MS)) == pdTRUE) {
			next_ms = lv_timer_handler();	// --> call lvgl main loop
			xSemaphoreGive(xUIDrawMutex);
		} else {
			//TaskHandle_t t = xSemaphoreGetMutexHolder(xUIDrawMutex);
			bclog.log(BCLogger::Log_Error, BCLogger::TAG_UI, "!!!!! UI draw task blocked !!!!!");
			printf("%d: !!!!! UI draw task blocked !!!!!", millis());
		}

		unsigned long mil_start = millis();
		// Slow update is used for more expensive updates
		if (xSemaphoreTake(xUpdateSlow, static_cast<TickType_t>(0) ) == pdTRUE) {		// Semaphore is used for message "please update" only. So there is no reason to wait.
			updateStats();
			updateIntBatteryInt();
		}
		if (millis() > next_millis) {
			timeval tv;
			gettimeofday(&tv, NULL);
			updateClock(tv.tv_sec);
			next_millis = millis() + (1005 - ( (tv.tv_usec / 1000) % 1000) ) ;		// Update clock only 5ms after full second
			//TRACE:printf("millis: %d\tclock:%d - %d -> %d\n", millis(), tv.tv_sec, tv.tv_usec, next_millis);
			uifl.redraw();	//Redraw FL screens

			// 1Hz fallback for evaluateNaviAutoSwitch()'s delayed switch-
			// back timer - updateNavi()/updateNaviDist() also call it, but
			// neither fires on its own (e.g. stopped at a light, no wheel
			// revs and no fresh BLE frame), so the pending 3s check could
			// otherwise sit armed indefinitely.
			//
			// Deliberately NOT the isDrawTask() bypass used elsewhere in
			// this file: that bypass is only correct for code running
			// SYNCHRONOUSLY NESTED inside lv_timer_handler() above, i.e.
			// while THIS function's own mutex block already holds
			// xUIDrawMutex. By this point in the loop that block has
			// already released it - isDrawTask() would still (correctly,
			// but misleadingly) say "yes this is the draw task", making
			// the bypass skip locking even though nothing is held here.
			// Found 2026-09-20: this was silently racing against the BLE
			// task's own properly-locked updateNavi()/updateNaviDist()
			// calls, a very plausible cause of the auto-switch state
			// machine being reported unreliable. A plain blocking take is
			// both correct and safe here (nothing to self-deadlock against).
			if (xSemaphoreTake(xUIDrawMutex, 50 / portTICK_PERIOD_MS) == pdTRUE) {
				evaluateNaviAutoSwitch();
				xSemaphoreGive(xUIDrawMutex);
			}
		}
		next_ms -= (millis() - mil_start);
		if (next_ms < 0) next_ms = 0;
		if (next_ms > 100) next_ms = 100; // minimum refresh rate 10Hz
		vTaskDelay(next_ms / portTICK_PERIOD_MS);
	}
}


// ---------------- Internal (private) data updater ----------------
void UIFacade::updateClock(const time_t now) {
	String strClock = DateFormatter::format(DateFormatter::TIME_ONLY,now);
	String strDate = DateFormatter::format(DateFormatter::DATE_ONLY,now);
	// ui_ScrMainUpdateClock removed 2026-09-18 (old S1Main screen disabled).
	uifl.updateClock(strClock, strDate);

	// Ride time / clock widget (rr_ic_time + rr_time_val), added 2026-09-25
	// per doc/design/mainscreen.svg's timeGroup. Mode switch rule (this
	// firmware's own choice - the design doc explicitly leaves it open):
	// stopwatch showing elapsed ride time while a ride is actually
	// connected/running (same isConnected() signal updateStateIcon()'s
	// caller uses to decide DS_NO_CONN), clock-face showing time-of-day the
	// rest of the time (e.g. before a ride starts). elapsedS uses AVG_ALL
	// so it counts wall-clock time since ride start including stops -
	// that's "Fahrzeit" here, not moving time.
	bool stopwatchMode = stats.isConnected();
	uint32_t elapsedS = stats.getTime(Statistics::SUM_ESP_START, Statistics::AVG_ALL);
	struct tm *lt = localtime(&now);
	char clockStr[6];
	snprintf(clockStr, sizeof(clockStr), "%02d:%02d", lt->tm_hour, lt->tm_min);

	// Not the isDrawTask() bypass: updateClock() runs from updateHandler()'s
	// loop AFTER that iteration's own lv_timer_handler() mutex block already
	// released xUIDrawMutex (same reasoning as evaluateNaviAutoSwitch()'s
	// 1Hz-fallback call just below in this file) - a plain blocking take is
	// correct and safe here.
	if (xSemaphoreTake(xUIDrawMutex, 50 / portTICK_PERIOD_MS) == pdTRUE) {
		ui_RimRidgeUpdateTime(stopwatchMode, elapsedS, clockStr);
		xSemaphoreGive(xUIDrawMutex);
	}
}

void UIFacade::updateStats() {
	Statistics::ESummaryType t = statMode;

	uint32_t timeTot = stats.getTime(t, statTimeMode);
	if (statMode == Statistics::SUM_ESP_TOTAL) {
		time_t now = time(nullptr);
		struct tm *lt = localtime(&now);
		timeTot = lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec;
	}
	// ui_ScrMainUpdateStats removed 2026-09-18 (old S1Main screen disabled).

//TODO: Check if heigt is also updated in non-FL mode at standstill (no gradient calculation)
	ui_RimRidgeUpdateStats(Statistics::SUM_TYPE_STRING[t] + 3, Statistics::AVG_TYPE_STRING[statTimeMode] + 3,
			stats.getAvg(t, statTimeMode), stats.getSpeedMax(t), stats.getTemp(), stats.getDistance(t), timeTot);
	ui_RimRidgeRQUpdateDist(stats.getDistance(t));
}

void UIFacade::updateIntBatteryInt() {
	char batStr[32];
	snprintf(batStr, 31, "Volt: %.02fV - %d%% %s", batIntVoltage, batIntPerc, batIntCharging?"- C": "");
	ui_RimRidgeUpdateIntBatPerc(batIntPerc);
	// ui_ScrChartUpdateBat kept even though the Chart screen is disabled:
	// it also computes the rolling battery-voltage average (batIntVoltageAvg)
	// by reading back samples from the Chart's own lv_chart widget storage -
	// removing this call would silently freeze that average. See memory
	// ui-tooling-eez-studio-migration for the full disabled-screens list.
	float avg = ui_ScrChartUpdateBat(batIntVoltage, batIntPerc, batStr);
	if (! isnan(avg)) batIntVoltageAvg = avg;
}

// ---------------- external (public) data updater ----------------

void UIFacade::updateSpeed(float _speed) {
	speed=_speed;
	xSemaphoreGive(xUpdateFast);
}

void UIFacade::updateCadence(uint16_t _cad) {
	cad=_cad;
	xSemaphoreGive(xUpdateFast);
}

void UIFacade::updateHR(uint16_t _hr) {
	hr=_hr;
	xSemaphoreGive(xUpdateFast);
}

void UIFacade::updateGrad(float _grad, float _height) {
	height = _height;
	grad   = _grad;
	xSemaphoreGive(xUpdateFast);
}

void UIFacade::updateHeight(float _height) { // height only update,
	height = _height;
	xSemaphoreGive(xUpdateFast);
}



void UIFacade::updateIP(const String& ipStr) {
	// SWLAN screen removed 2026-09-26 (was already disabled since
	// 2026-09-18) - no RimRidge equivalent yet (no IP display), wire this
	// up once WLAN gets a RimRidge-style screen.
	(void) ipStr;
}

void UIFacade::updateSSIDList(const String& ssidStr) {
	// SWLAN screen removed 2026-09-26 - no RimRidge equivalent yet, wire
	// this up once WLAN gets a RimRidge-style screen.
	(void) ssidStr;
}

void UIFacade::updateWiFiState(bool wifiEnabled, bool APModeActive, bool disableAPMode, uint8_t apStaCount) {
	// SWLAN-specific update removed 2026-09-18, screen itself removed
	// 2026-09-26 - RimRidge only gets the simple show/hide for now, the
	// AP-mode/client-count detail had no RimRidge home and is gone with it.
	bool uiTask = isDrawTask();
	if (uiTask || xSemaphoreTake(xUIDrawMutex, 250 / portTICK_PERIOD_MS) == pdTRUE) {
		ui_RimRidgeUpdateWiFiState(wifiEnabled, APModeActive, disableAPMode, apStaCount);
		if (!uiTask) xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Update Wifi State blocked by mutex");
	}
}

void UIFacade::updateStateIcon(Statistics::EDrivingState state, UIColor col) {
	// Restored 2026-09-19 for the RimRidge "Mainscreen-Studie" follow-up
	// (rr_ic_state) - mirrors the pre-RimRidge mapping (see git history of
	// this function) with 4 distinct icons instead of the old 2 shared
	// ones (stateCyclePower/stateStop). DS_NO_CONN has no icon in the
	// artifact -> NULL, which ui_RimRidgeUpdateStateIcon() hides.
	const lv_img_dsc_t *pCurStateIcon = NULL;
	switch (state) {
	case Statistics::DS_DRIVE_POWER:
		pCurStateIcon = &img_rr_icon_state_power;
		break;
	case Statistics::DS_DRIVE_COASTING:
		pCurStateIcon = &img_rr_icon_state_coasting;
		break;
	case Statistics::DS_BREAK:
		pCurStateIcon = &img_rr_icon_state_break;
		break;
	case Statistics::DS_STOP:
		pCurStateIcon = &img_rr_icon_state_stop;
		break;
	case Statistics::DS_NO_CONN:
	default:
		pCurStateIcon = NULL;
	}

	// TODO(2026-09-19): provisional override for visual QA while no speed
	// sensor is connected (real state would be DS_NO_CONN, hidden) - forces
	// the "Cruise"/DS_DRIVE_COASTING icon so the user can check its on-
	// device look. Remove this override once confirmed, restoring the
	// switch's real DS_NO_CONN result above.
	pCurStateIcon = &img_rr_icon_state_coasting;

	lv_color_t lvcol = lv_color_hex(0xCBA36B);	// RRBrass - matches the rest of the RimRidge icon set
	switch (col) {
	case UI_ColorWarn:
		lvcol = lv_palette_main(LV_PALETTE_AMBER);
		break;
	case UI_ColorCrit:
		lvcol = lv_palette_main(LV_PALETTE_RED);
		break;
	case UI_ColorOK:
		lvcol = lv_palette_main(LV_PALETTE_GREEN);
		break;
	default:
		break;
	}

	bool uiTask = isDrawTask();
	if (uiTask || xSemaphoreTake(xUIDrawMutex, 150 / portTICK_PERIOD_MS) == pdTRUE) {
		ui_RimRidgeUpdateStateIcon(pCurStateIcon, lvcol);
		if (!uiTask) xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Update state icon blocked by mutex");
	}
}

void UIFacade::updateGpsFix(bool hasFix, UIColor col) {
	lv_color_t lvcol = lv_color_black();
	switch (col) {
	case UI_ColorWarn:
		lvcol = lv_palette_main(LV_PALETTE_AMBER);
		break;
	case UI_ColorCrit:
		lvcol = lv_palette_main(LV_PALETTE_RED);
		break;
	case UI_ColorOK:
		lvcol = lv_palette_main(LV_PALETTE_GREEN);
		break;
	}

	bool uiTask = isDrawTask();
	if (uiTask || xSemaphoreTake(xUIDrawMutex, 150 / portTICK_PERIOD_MS) == pdTRUE) {
		ui_RimRidgeUpdateGpsFix(hasFix, lvcol);
		if (!uiTask) xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Update GPS fix icon blocked by mutex");
	}
}

void UIFacade::updateRoadQuality(uint8_t roadClass, float roughness, uint32_t shockCount) {
	bool uiTask = isDrawTask();
	if (uiTask || xSemaphoreTake(xUIDrawMutex, 150 / portTICK_PERIOD_MS) == pdTRUE) {
		ui_RimRidgeUpdateRoadQuality(roadClass, roughness, shockCount);
		if (!uiTask) xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Update road quality blocked by mutex");
	}
}

void UIFacade::updateRoadLabel(uint8_t surface, uint8_t quality, bool capturing) {
	bool uiTask = isDrawTask();
	if (uiTask || xSemaphoreTake(xUIDrawMutex, 150 / portTICK_PERIOD_MS) == pdTRUE) {
		ui_RimRidgeRQUpdateLabel(surface, quality, capturing);
		if (!uiTask) xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Update road label blocked by mutex");
	}
}

// Distance thresholds for evaluateNaviAutoSwitch() (2026-09-20, exact
// values from the user): auto-show only once within this close to the
// maneuver...
static const uint32_t NAV_AUTO_SHOW_DIST_M = 300;
// ...and, once a NEW instruction arrives while already showing, only
// switch back to Main if that new instruction turns out to be at least
// this far away (a deliberate 50m gap above the show threshold, not the
// same number, so a maneuver sitting right around 300-350m doesn't flicker
// the screen open/closed every update as distance jitters slightly around
// one single boundary).
static const uint32_t NAV_AUTO_HIDE_DIST_M = 350;
// ...and only after this long, so there's time to actually see the "you
// just made that turn" confirmation instead of an instant jump back.
static const uint32_t NAV_AUTO_HIDE_DELAY_MS = 3000;

void UIFacade::evaluateNaviAutoSwitch() {
	bool hasActiveNav = (currentManeuver != NAV_MANEUVER_NONE);

	if (hadActiveNav && !hasActiveNav) {
		// Route just ended - force back to Main regardless of how the
		// screen got shown (auto or manual preview). Only fires on the
		// actual active->NONE edge, not on every subsequent call while
		// already inactive - otherwise this would kill a manual preview
		// opened via showNavScreen() with no active route at all (see its
		// own doc comment).
		if (navScreenActive) {
			navScreenActive = false;
			bclog.log(BCLogger::Log_Info, BCLogger::TAG_UI, "Nav auto-hide: route ended");
			lv_disp_load_scr(ui_MainScreen);
		}
		autoDecidedForManeuver = NAV_MANEUVER_NONE;
		pendingHideAtMs = 0;
	}
	hadActiveNav = hasActiveNav;
	if (!hasActiveNav) return;

	bool maneuverChanged = (currentManeuver != lastManeuverSeen);
	lastManeuverSeen = currentManeuver;

	if (maneuverChanged) {
		if (navScreenActive) {
			// Already showing (mid the previous turn) and a new
			// instruction just arrived - don't snap back to Main
			// instantly, that's jarring and gives no time to register
			// "you just made that turn". Arm a delayed re-check instead;
			// see below. The fresh maneuver still gets its own
			// auto-show/suppress decision, just not before the delay
			// resolves (it's already showing, so there's nothing to
			// auto-show anyway).
			pendingHideAtMs = millis() + NAV_AUTO_HIDE_DELAY_MS;
			pendingHideForManeuver = currentManeuver;
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_UI,
					"Nav pending-hide armed: new maneuver=%s dist=%u, checking in %ums",
					navManeuverToString(currentManeuver), (unsigned) currentManeuverDist, NAV_AUTO_HIDE_DELAY_MS);
		} else {
			pendingHideAtMs = 0;
		}
		autoDecidedForManeuver = NAV_MANEUVER_NONE; // fresh instruction, no decision made for it yet
	}

	if (!navScreenActive && autoDecidedForManeuver != currentManeuver
			&& currentManeuverDist < NAV_AUTO_SHOW_DIST_M) {
		navScreenActive = true;
		autoDecidedForManeuver = currentManeuver;
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_UI, "Nav auto-show: maneuver=%s dist=%u",
				navManeuverToString(currentManeuver), (unsigned) currentManeuverDist);
		lv_disp_load_scr(objects.rim_ridge_nav);
	}

	if (pendingHideAtMs != 0 && pendingHideForManeuver == currentManeuver && millis() >= pendingHideAtMs) {
		pendingHideAtMs = 0;
		// Uses whatever the freshest known distance is right now, not the
		// distance from when the timer was armed - it keeps decreasing as
		// you ride, and a stale snapshot could hide a turn that's since
		// become imminent again.
		bool hide = currentManeuverDist > NAV_AUTO_HIDE_DIST_M;
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_UI, "Nav pending-hide check: maneuver=%s dist=%u -> %s",
				navManeuverToString(currentManeuver), (unsigned) currentManeuverDist, hide ? "HIDE" : "STAY");
		if (hide) {
			navScreenActive = false;
			lv_disp_load_scr(ui_MainScreen);
		}
		// Deliberately NOT setting autoDecidedForManeuver here. This used
		// to say "autoDecidedForManeuver = currentManeuver; // decided
		// either way" - which permanently blocked the auto-show check
		// above from ever firing again for THIS SAME maneuver, since it
		// only fires when autoDecidedForManeuver != currentManeuver. Found
		// 2026-09-20 via a live serial log: a KEEP_LEFT instruction got
		// hidden here at 366m (correctly, >350), and then NEVER
		// auto-showed again even as distance fell to 202/51/12m - the
		// exact bug the user reported ("nicht gewechselt... wenn die
		// Distanz < 300m wird"). pendingHideAtMs is already reset to 0
		// above, which is all that's needed to stop this SAME timer from
		// re-firing - autoDecidedForManeuver should only ever be touched
		// by an explicit MANUAL dismiss (hideNavScreen()) or a fresh
		// instruction arriving (the maneuverChanged block above, which
		// resets it to NONE so the NEXT maneuver gets its own decision).
	}
}

void UIFacade::updateNavi(const String& navStr, uint32_t dist, uint8_t maneuver, uint8_t roundaboutExit,
		uint8_t nextManeuver, uint32_t nextManeuverDist, const String& nextStreet,
		uint32_t remainingDist, uint32_t remainingTime) {
	(void) remainingDist; (void) remainingTime; // no RimRidgeNav widget for these yet

	if (xSemaphoreTake(xUIDrawMutex, 150 / portTICK_PERIOD_MS) == pdTRUE) {
		currentManeuver = maneuver;
		currentManeuverDist = dist;
		evaluateNaviAutoSwitch();

		ui_RimRidgeUpdateNav(navStr.c_str(), dist, maneuver, roundaboutExit);
		// navStr (not nextStreet!) is the street for the CURRENT maneuver -
		// nextStreet belongs to the maneuver after that and isn't shown
		// anywhere on this screen (see ui_RimRidgeNavUpdateNav's doc comment).
		ui_RimRidgeNavUpdateNav(dist, maneuver, roundaboutExit, navStr.c_str(), nextManeuver, nextManeuverDist);
		ui_RimRidgeRQUpdateNav(dist, maneuver, roundaboutExit);
		xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Nav blocked by mutex");
	}
}

void UIFacade::showNavScreen() {
	// Manual open (rr_nav_pill tap on RimRidge) - works even with no
	// active route, e.g. to check the screen's look without real nav data.
	//
	// Called synchronously from LVGL's own event dispatch (the EEZ action
	// callback), which runs *inside* lv_timer_handler() - itself called
	// by updateHandler() while it already holds xUIDrawMutex. Re-taking a
	// non-recursive FreeRTOS mutex from the same task that already holds
	// it blocks (logged as "blocked by mutex" - not a contention issue,
	// a same-task re-entrancy issue), so this needs the same isDrawTask()
	// bypass every other direct-from-LVGL-callback path in this file uses.
	bool uiTask = isDrawTask();
	if (uiTask || xSemaphoreTake(xUIDrawMutex, 150 / portTICK_PERIOD_MS) == pdTRUE) {
		navScreenActive = true;
		lv_disp_load_scr(objects.rim_ridge_nav);
		if (!uiTask) xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Show nav screen blocked by mutex");
	}
}

void UIFacade::showRQScreen() {
	// Manual open (rr_line_rq tap on RimRidge) - no auto-popup logic on
	// this screen at all, so unlike showNavScreen() there's no extra state
	// to keep in sync. Same same-task mutex re-entrancy note applies (see
	// showNavScreen() above).
	bool uiTask = isDrawTask();
	if (uiTask || xSemaphoreTake(xUIDrawMutex, 150 / portTICK_PERIOD_MS) == pdTRUE) {
		lv_disp_load_scr(objects.rim_ridge_rq);
		if (!uiTask) xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Show RQ screen blocked by mutex");
	}
}

void UIFacade::hideRQScreen() {
	// Manual close (swipe on RimRidgeRQ).
	bool uiTask = isDrawTask();
	if (uiTask || xSemaphoreTake(xUIDrawMutex, 150 / portTICK_PERIOD_MS) == pdTRUE) {
		lv_disp_load_scr(ui_MainScreen);
		if (!uiTask) xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Hide RQ screen blocked by mutex");
	}
}

void UIFacade::hideNavScreen() {
	// Manual close (swipe right on RimRidgeNav). Marks the current
	// maneuver as already-decided so evaluateNaviAutoSwitch() won't
	// immediately auto-re-show it on the very next distance update (it
	// could easily still be under NAV_AUTO_SHOW_DIST_M) - a manual
	// dismiss should stick until the next real turn instruction. This
	// does NOT block manually reopening via showNavScreen() (unconditional,
	// doesn't consult this state at all), only the automatic re-trigger.
	//
	// Same same-task mutex re-entrancy note as showNavScreen() above.
	bool uiTask = isDrawTask();
	if (uiTask || xSemaphoreTake(xUIDrawMutex, 150 / portTICK_PERIOD_MS) == pdTRUE) {
		navScreenActive = false;
		autoDecidedForManeuver = currentManeuver;
		pendingHideAtMs = 0;
		lv_disp_load_scr(ui_MainScreen);
		if (!uiTask) xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Hide nav screen blocked by mutex");
	}
}

void UIFacade::updateNaviDist(uint32_t dist) {
	if (xSemaphoreTake(xUIDrawMutex, 250 / portTICK_PERIOD_MS) == pdTRUE) {
		// Wheel-interpolated estimate (see BLEDevices.cpp's DEV_CSC_1/2
		// handler) between full NAV_UPDATE frames - also feeds
		// evaluateNaviAutoSwitch() so the distance thresholds react as
		// fast as the speed sensor allows, not just on the ~1-5s BLE nav
		// cadence.
		currentManeuverDist = dist;
		evaluateNaviAutoSwitch();
		// ui_ScrNaviUpdateNavDist removed 2026-09-18 (SNavi popup disabled).
		ui_RimRidgeUpdateNavDist(dist);
		ui_RimRidgeRQUpdateNavDist(dist);
		xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Nav dist blocked by mutex");
	}
}

void UIFacade::updateLanes(const NavLane* _lanes, uint8_t _laneCount, uint32_t _laneDist, const NavLane* _nextLanes,
		uint8_t _nextLaneCount, uint32_t _nextLaneDist) {
	laneCount = (_laneCount > NAV_LANES_MAX) ? NAV_LANES_MAX : _laneCount;
	nextLaneCount = (_nextLaneCount > NAV_LANES_MAX) ? NAV_LANES_MAX : _nextLaneCount;
	for (uint8_t i = 0; i < laneCount; i++) lanes[i] = _lanes[i];
	for (uint8_t i = 0; i < nextLaneCount; i++) nextLanes[i] = _nextLanes[i];
	laneDist = _laneDist;
	nextLaneDist = _nextLaneDist;

	bool uiTask = isDrawTask();
	if (uiTask || xSemaphoreTake(xUIDrawMutex, 150 / portTICK_PERIOD_MS) == pdTRUE) {
		// Shown on BOTH screens whenever there's lane information at all -
		// no distance gating (LANE_DISTANCE_M is explicitly unused per the
		// user, "das Protokoll behauptet, sie ist eh nicht korrekt") and no
		// maneuver code needed (brightness comes entirely from each lane's
		// own ACTIVE flag/primary direction, see lane_icon.h).
		ui_RimRidgeUpdateLanes(laneCount ? lanes : nullptr, laneCount);
		ui_RimRidgeNavUpdateLanes(laneCount ? lanes : nullptr, laneCount);
		if (!uiTask) xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Lanes blocked by mutex");
	}
}

void UIFacade::updateBatInt(float voltage, uint8_t batPerc, bool charging) {
	batIntVoltage = voltage;
	batIntPerc = batPerc;
	batIntCharging = charging;
}

void UIFacade::setChartArray(int16_t a[], uint8_t idx) {
	if (idx>4) {
		bclog.log(BCLogger::Log_Error, BCLogger::TAG_UI, "Invalid chart series index");
		return;
	}
	ui_ScrChartSetExtArray1(a, idx);
}

void UIFacade::setChartPosFirst(uint16_t pos, uint8_t idx) {
	ui_ScrChartSetPostFirst(pos, idx);

}
void UIFacade::updateChart() {
	ui_ScrChartRefresh();
}

void UIFacade::showMsgBox(const String &msgText, const MsgBoxCallBack &cb) {
	if (msgCB != NULL) {
		msgCB(false);
	}
	msgCB = cb;
	ui_MsgBox(msgText.c_str());
}

void UIFacade::updateMsgBox(const String& msgText) {
	ui_MsgBoxUpdate(msgText.c_str());
}

void UIFacade::msgCBFct(bool ok) {
	if (msgCB != NULL) {
		msgCB(ok);
		msgCB = NULL;
	}
}

void UIFacade::otaStart() {
	// SOTA screen removed 2026-09-26 (was already disabled since 2026-09-18)
	// - RimRidge stays visible during an OTA update, no on-device progress
	// feedback. The web-based OTA flow (WifiWebserver.cpp, replaced
	// ElegantOTA) has its own progress UI in the browser; a RimRidge screen
	// for this would need building fresh, not restoring the old SOTA code.
}

void UIFacade::otaProgress(uint8_t perc) {
	(void) perc; // no on-device widget - see otaStart()'s comment
}
