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
#include "ui/ui.h"
#include "ui/ui_custFunc.h"

#include <DateTime.h>

UIFacade::UIFacade() {
	// TODO Auto-generated constructor stub

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


    // .. add init of new screens here


    // Glue oberserver/listeners to data model
	flparser.setBatCb([this](uint16_t batVoltage, uint8_t batPerc, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn) {
		updateFLPower(batVoltage, batPerc, powerStage, CurBat, CurConsumer, ConsumerOn);
	});
	flparser.setStateCb([](FLClassicParser::EFLConnState cstate, uint32_t flag, int16_t timeout) {
		ui_ScrFLUpdateFlags(flag);
	});

    // 4. Load initial screen
    lv_disp_load_scr(ui_S1Main);
    //lv_disp_load_scr(ui_ScreenFL); // TODO: this should not be the first screen

    // 5. Start Update ticker
    updateHandler();
    dataTicker.attach_ms(250, +[](UIFacade* thisInstance) {thisInstance->updateData();}, this);
}


// ---------------- Internal (private) ticker handlers (automatic update) ----------------

// data updater
void UIFacade::updateData() {
    time_t now;
    time(&now);
	updateClock(now);
}

// redraw screen
void UIFacade::updateHandler() {
	updateData();
	uint32_t next_ms = lv_timer_handler();
	updateTicker.once_ms(next_ms, +[](UIFacade* thisInstance) {thisInstance->updateHandler();}, this);
}


// ---------------- Internal (private) data updater ----------------

void UIFacade::updateClock(const time_t now) {
	String strClock = DateFormatter::format(DateFormatter::TIME_ONLY,now);
	String strDate = DateFormatter::format(DateFormatter::DATE_ONLY,now);

	ui_ScrMainUpdateClock(strClock.c_str(), strDate.c_str());
	ui_ScrFLUpdateClock(strClock.c_str(), strDate.c_str());
}

// ---------------- external (public) data updater ----------------

void UIFacade::updateSpeed(float speed) {
	ui_ScrMainUpdateSpeed(speed);
	ui_ScrNaviUpdateSpeed(speed);
}

void UIFacade::updateCadence(uint16_t cad) {
	ui_ScrMainUpdateCadence(cad);
	ui_ScrNaviUpdateCadence(cad);
}

void UIFacade::updateHR(uint16_t hr) {
	ui_ScrMainUpdateHR(hr);
	ui_ScrNaviUpdateHR(hr);
}

void UIFacade::updateIP(const String& ipStr) {
	ui_SWLANUpdateIP(ipStr.c_str());
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
	ui_ScrNaviUpdateNav(navStr.c_str(), dist, dirCode);

}

// ----------- Forumslader-specific -----------
void UIFacade::updateFLPower(uint16_t batVoltage, uint8_t batPerc, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn) {
	ui_ScrFLUpdatePower(batVoltage, batPerc, powerStage, CurBat, CurConsumer, ConsumerOn);
	ui_ScrMainUpdatePower(batVoltage, batPerc, powerStage, CurBat, CurConsumer, ConsumerOn);
}

