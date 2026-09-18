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

#include "ui/Screens/SNavi/ui.h"
#include "ui/Screens/SNavi/ui_NaviCustFunc.h"

#include "ui/img/state-icons.h"

#include "ui/Screens/Settings/ui_Settings.h"

#include <ui/Screens/MainNoFL/ui.h>
#include <ui/Screens/MainNoFL/ui_MainNoFL_CustFunc.h>

#include <ui/Screens/SWLAN/ui.h>
#include <ui/Screens/SWLAN/ui_SWLAN_CustFunc.h>

#include <ui/Screens/Chart/ui.h>
#include <ui/Screens/Chart/ui_Chart_CustFunc.h>

#include <ui/Screens/SOTA/ui.h>

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
#include "ui/RimRidgeCustFunc.h"

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

    ui_SMainNoFL_screen_init();
    ui_SMainNoFLExtraInit(); // GPS fix status icon
    ui_SWLAN_screen_init();
    ui_SWLAN_extra_init(); // QR Code
    ui_SNavi_screen_init();
    ui_ScrNaviExtraInit(); // next-maneuver preview
    ui_ScrSettings_screen_init();

    ui_SOTA_screen_init();

    // .. add init of new screens here

    create_screen_rim_ridge();
    ui_RimRidgeUpdateNav(nullptr, 0, NAV_MANEUVER_NONE, 0); // start on the "no nav" icon, not rr_ic_turn's EEZ-authored default placeholder

    // 3. set main screen
    // RimRidge is now the permanent main/boot screen (2026-09-18). The old
    // SquareLine screens above are still initialized - some of their update
    // functions are still called from the update loop below for hidden
    // dependencies (see updateIntBatteryInt()) - but none of them are ever
    // shown anymore; see memory ui-tooling-eez-studio-migration for the
    // full list of what's disabled-not-deleted and needs restoring once
    // RimRidge-style replacements exist for OTA/Nav/WLAN/Settings/Chart.
    ui_MainScreen = objects.rim_ridge;

    ui_ScrNaviSetBackScreen(ui_MainScreen);
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
			// Old-screen fan-out (ui_ScrMain*/ui_ScrNavi*/ui_ScrChart*/ui_SMainNoFL*)
			// removed 2026-09-18 - those screens are disabled, not deleted, see
			// memory ui-tooling-eez-studio-migration for the restore list.
			ui_RimRidgeUpdateSpeed(speed);
			ui_RimRidgeUpdateCadence(cad);
			ui_RimRidgeUpdateHR(hr);
			ui_RimRidgeUpdateGrad(grad, height);
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
	// ui_ScrMainUpdateClock removed 2026-09-18 (old S1Main screen disabled,
	// RimRidge has no clock widget - confirmed leave-out for this step).
	uifl.updateClock(strClock, strDate);
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
	// SWLAN screen disabled 2026-09-18, no RimRidge equivalent (no IP
	// display) - restore once WLAN gets a RimRidge-style screen.
	(void) ipStr;
}

void UIFacade::updateSSIDList(const String& ssidStr) {
	// SWLAN screen disabled 2026-09-18, no RimRidge equivalent - restore
	// once WLAN gets a RimRidge-style screen.
	(void) ssidStr;
}

void UIFacade::updateWiFiState(bool wifiEnabled, bool APModeActive, bool disableAPMode, uint8_t apStaCount) {
	// SWLAN-specific update removed 2026-09-18 (screen disabled, not
	// deleted) - RimRidge only gets the simple show/hide for now, the
	// AP-mode/client-count detail lived on SWLAN only and has no home yet.
	bool uiTask = isDrawTask();
	if (uiTask || xSemaphoreTake(xUIDrawMutex, 250 / portTICK_PERIOD_MS) == pdTRUE) {
		ui_RimRidgeUpdateWiFiState(wifiEnabled, APModeActive, disableAPMode, apStaCount);
		if (!uiTask) xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Update Wifi State blocked by mutex");
	}
}

void UIFacade::updateStateIcon(Statistics::EDrivingState state, UIColor col) {
	// No-op since 2026-09-18: driving-state icon (coasting/power/braking/
	// stopped, was ui_ImgState on MainNoFL) has no RimRidge widget yet -
	// confirmed leave-out for this step. Statistics.cpp still calls this
	// every cycle, so keep the function (just without a widget to update);
	// restore the icon/color selection logic (see git history of this
	// function) once RimRidge grows a place for it.
	(void) state; (void) col;
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

void UIFacade::updateNavi(const String& navStr, uint32_t dist, uint8_t maneuver, uint8_t roundaboutExit,
		uint8_t nextManeuver, uint32_t nextManeuverDist, const String& nextStreet,
		uint32_t remainingDist, uint32_t remainingTime) {
	static uint8_t oldManeuver = 0;
	static bool distAnn = false;
	static bool avoidBack = false;
	static uint8_t maneuver_old = 0;
	static uint32_t maneuver_timestamp = 0;
	bool loadScreen=false;
	bool unloadScreen=false;
	if ( maneuver != oldManeuver) {
		oldManeuver = maneuver;
	    loadScreen = true;
	}
	if (!distAnn && dist < 200) {
		distAnn = true;
		loadScreen = true;
	}
	if (distAnn && dist > 250) {
		distAnn = false;
	}

	if (maneuver != maneuver_old) {
		maneuver_old = maneuver;
		avoidBack = true;
		maneuver_timestamp = millis();
	}
	if (avoidBack && ((millis() - maneuver_timestamp) > 5000)) {
		avoidBack = false;
	}

	// Full-screen SNavi popup disabled 2026-09-18 (confirmed with user) -
	// loadScreen/unloadScreen/avoidBack bookkeeping above is now inert
	// (nothing acts on it anymore) but left in place since it's harmless
	// and documents the original intent for when RimRidge grows a
	// full-screen nav view. RESTORE lv_disp_load_scr(ui_SNavi) /
	// ui_ScrNaviGoBack() below once that exists.
	(void) loadScreen; (void) unloadScreen;
	if (xSemaphoreTake(xUIDrawMutex, 150 / portTICK_PERIOD_MS) == pdTRUE) {
		ui_RimRidgeUpdateNav(navStr.c_str(), dist, maneuver, roundaboutExit);
		xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Nav blocked by mutex");
	}
}

void UIFacade::updateNaviDist(uint32_t dist) {
	if (xSemaphoreTake(xUIDrawMutex, 250 / portTICK_PERIOD_MS) == pdTRUE) {
		// ui_ScrNaviUpdateNavDist removed 2026-09-18 (SNavi popup disabled).
		ui_RimRidgeUpdateNavDist(dist);
		xSemaphoreGive(xUIDrawMutex);
	} else {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_UI, "Nav dist blocked by mutex");
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
	// SOTA screen disabled 2026-09-18 (confirmed with user) - RimRidge stays
	// visible during an OTA update, no progress feedback on screen until
	// SOTA gets a RimRidge-style replacement. RESTORE lv_disp_load_scr(ui_SOTA)
	// below once that exists.
}

void UIFacade::otaProgress(uint8_t perc) {
	// ui_SOTA_updatePerc removed 2026-09-18 (SOTA screen disabled, see otaStart()).
	(void) perc;
}
