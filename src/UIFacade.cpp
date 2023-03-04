/*
 * UIFacade.cpp
 *
 *  Created on: 03.03.2023
 *      Author: ian
 */

#include <UIFacade.h>
#include "Singletons.h"
#include "ui/ui_FL.h"
#include "ui/ui.h"

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
    ui_ScreenWifi_screen_init();
    ui_ScreenChart_screen_init();
    ui_ScreenFL_screen_init();

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
}

void UIFacade::updateData() {
}

void UIFacade::updateHandler() {
	updateData();
	uint32_t next_ms = lv_timer_handler();
	updateTicker.once_ms(next_ms, +[](UIFacade* thisInstance) {thisInstance->updateHandler();}, this);
}

void UIFacade::updateFLPower(uint16_t batVoltage, uint8_t batPerc, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn) {
	ui_ScrFLUpdatePower(batVoltage, batPerc, powerStage, CurBat, CurConsumer, ConsumerOn);
}
