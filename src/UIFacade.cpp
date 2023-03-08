/*
 * UIFacade.cpp
 *
 *  Created on: 03.03.2023
 *      Author: ian
 */

#include <UIFacade.h>
#include "Singletons.h"
#include "ui/ui_FL.h"
#include "ui/ui_WLAN.h"
#include "ui/ui_Navi.h"
#include "ui/ui_NaviCustFunc.h"
#include "ui/ui_Settings.h"
#include "ui/ui.h"
#include "ui/ui_custFunc.h"

#include <DateTime.h>

void startTaskUiUpdate(void*) {
	ui.updateHandler();
}

UIFacade::UIFacade() {
    xUpdateFast = xSemaphoreCreateBinary();
	xUpdateSlow = xSemaphoreCreateBinary();
}

void UIFacade::initDisplay() {
	// Init LVGL

	// 1. Default theme/font etc.
    lv_disp_t * dispp = lv_disp_get_default();
    lv_theme_t * theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), true, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    // 2. Init all screens
    ui_S1Main_screen_init();
    //ui_ScreenWifi_screen_init();
    ui_ScreenChart_screen_init();
    ui_ScreenFL_screen_init();
    ui_SWLAN_screen_init();
    ui_SWLAN_extra_init(); // QR Code
    ui_SNavi_screen_init();
    ui_ScrSettings_screen_init();
    // .. add init of new screens here


    // Glue oberserver/listeners to data model
	flparser.setBatCb([this](uint16_t batVoltage, uint8_t batPerc, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn) {
		updateFLPower(batVoltage, batPerc, powerStage, CurBat, CurConsumer, ConsumerOn);
	});
	flparser.setStateCb([](FLClassicParser::EFLConnState cstate, uint32_t flag, int16_t timeout) {
		//TODO: check thread safety ui_ScrFLUpdateFlags(flag);
	});

    // 4. Load initial screen
    lv_disp_load_scr(ui_S1Main);

    // 5. Start fast update Thread
    xTaskCreate(startTaskUiUpdate, "UI Task", 4096, NULL, 20, &uiTaskHandle);	// High priority task for smooth display updates

    // 6. Start Update ticker
    dataTicker.attach_ms(250, +[](UIFacade* thisInstance) {thisInstance->updateData();}, this);
}


// ---------------- Internal (private) ticker handlers (automatic update) ----------------

// data updater
void UIFacade::updateData() {
	xSemaphoreGive(xUpdateSlow);
}


// Internal task handler

// redraw screen
void UIFacade::updateHandler() {

	//updateData();
	unsigned long next_millis = 0;
	while (true) {
		uint32_t next_ms = lv_timer_handler();	// --> call lvgl main loop
		unsigned long mil_start = millis();
		// Fast update - use this only for data that should be shown with no (further) delay
		if (xSemaphoreTake(xUpdateFast, static_cast<TickType_t>(0) ) == pdTRUE) {		// Semaphore is used for message "please update" only. So there is no reason to wait.
			ui_ScrMainUpdateSpeed(speed);
			ui_ScrNaviUpdateSpeed(speed);
			ui_ScrMainUpdateCadence(cad);
			ui_ScrNaviUpdateCadence(cad);
			ui_ScrMainUpdateHR(hr);
			ui_ScrNaviUpdateHR(hr);
		}
		// Slow update is used for more expensive updates
		if (xSemaphoreTake(xUpdateSlow, static_cast<TickType_t>(0) ) == pdTRUE) {		// Semaphore is used for message "please update" only. So there is no reason to wait.
			updateStats();
		}
		if (millis() > next_millis) {
			timeval tv;
			gettimeofday(&tv, NULL);
			updateClock(tv.tv_sec);
			next_millis = millis() + (1005 - ( (tv.tv_usec / 1000) % 1000) ) ;		// Update clock only 5ms after full second
			//TRACE:printf("millis: %d\tclock:%d - %d -> %d\n", millis(), tv.tv_sec, tv.tv_usec, next_millis);
		}

		next_ms -= (millis()-mil_start);
		if (next_ms > 100) next_ms = 100; // minimum refresh rate + catches overflow
		vTaskDelay(next_ms / portTICK_PERIOD_MS);
	}
}


// ---------------- Internal (private) data updater ----------------

void UIFacade::updateClock(const time_t now) {
	String strClock = DateFormatter::format(DateFormatter::TIME_ONLY,now);
	String strDate = DateFormatter::format(DateFormatter::DATE_ONLY,now);

	ui_ScrMainUpdateClock(strClock.c_str(), strDate.c_str());
	ui_ScrFLUpdateClock(strClock.c_str(), strDate.c_str());
}

void UIFacade::updateStats() {
	ui_ScrMainUpdateStats(stats.getAvg(Statistics::SUM_ESP_START, Statistics::AVG_DRIVE), stats.getSpeedMax(Statistics::SUM_ESP_START), stats.getDistance(Statistics::SUM_ESP_START));

}

// ---------------- external (public) data updater ----------------

void UIFacade::updateSpeed(float _speed) {
//	ui_ScrMainUpdateSpeed(speed);
//	ui_ScrNaviUpdateSpeed(speed);
	speed=_speed;
	update = true;
}

void UIFacade::updateCadence(uint16_t _cad) {
//	ui_ScrMainUpdateCadence(cad);
//	ui_ScrNaviUpdateCadence(cad);
	cad=_cad;
	update = true;
}

void UIFacade::updateHR(uint16_t _hr) {
//	ui_ScrMainUpdateHR(hr);
//	ui_ScrNaviUpdateHR(hr);
	hr=_hr;
	update = true;
}

void UIFacade::updateIP(const String& ipStr) {
	//ui_SWLANUpdateIP(ipStr.c_str());
}

void UIFacade::updateNavi(const String& navStr, uint32_t dist, uint8_t dirCode) {
	static uint8_t oldDirCode = 0;
	static bool distAnn = false;
	if ( dirCode != oldDirCode) {
		oldDirCode = dirCode;
	    lv_disp_load_scr(ui_SNavi);
	}
	if (!distAnn && dist < 200) {
		distAnn = true;
		lv_disp_load_scr(ui_SNavi);
	}
	if (distAnn && dist > 250) {
		distAnn = false;
	}
	//ui_ScrNaviUpdateNav(navStr.c_str(), dist, dirCode);

}

// ----------- Forumslader-specific -----------
void UIFacade::updateFLPower(uint16_t batVoltage, uint8_t batPerc, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn) {
//	ui_ScrFLUpdatePower(batVoltage, batPerc, powerStage, CurBat, CurConsumer, ConsumerOn);
//	ui_ScrMainUpdatePower(batVoltage, batPerc, powerStage, CurBat, CurConsumer, ConsumerOn);
}

