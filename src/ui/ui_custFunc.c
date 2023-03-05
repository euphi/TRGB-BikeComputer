/*
 * ui_custFunc.c
 *
 *  Created on: 05.03.2023
 *      Author: ian
 */

#include "ui.h"

void ui_ScrMainUpdatePower(uint16_t batVoltage, uint8_t batPerc, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn) {

	lv_bar_set_value(ui_S1BarPowerMode, powerStage, LV_ANIM_OFF);
	lv_bar_set_value(ui_S1BarBatt, batPerc, LV_ANIM_OFF);
	lv_label_set_text_fmt(ui_S1BarBattLabel, "%d%%", batPerc);
}

void ui_ScrMainUpdateClock(const char* clockStr, const char* dateStr) {
	lv_label_set_text(ui_S1LabelClock, clockStr);
}

void ui_ScrMainUpdateSpeed(float speed) {
	lv_label_set_text_fmt(ui_S1LabelSpeed, "%.2f", speed);
	lv_arc_set_value(ui_S1ArcSpeed, (uint16_t)(speed*10.0));
}

void ui_ScrMainUpdateCadence(uint16_t cadence) {
	lv_label_set_text_fmt(ui_Label2, "%d rpm", cadence);
	lv_arc_set_value(ui_S1ArcCadence, cadence);
}

void ui_ScrMainUpdateHR(uint16_t hr) {
	lv_label_set_text_fmt(ui_S1BarPulsLabel, "%d bpm", hr);
	lv_bar_set_value(ui_S1BarPuls, hr, LV_ANIM_OFF);
}
