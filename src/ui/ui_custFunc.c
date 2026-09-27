/*
 * ui_custFunc.c
 *
 *  Created on: 05.03.2023
 *      Author: ian
 */

#include "ui.h"
#include <math.h> // to initialize float to NAN

lv_obj_t* msgBox = NULL;


void ui_ScrMainUpdatePower(uint16_t batVoltage, uint8_t batPerc, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn) {
	lv_bar_set_value(ui_S1BarPowerMode, powerStage, LV_ANIM_OFF);
	lv_bar_set_value(ui_S1BarBatt, batPerc, LV_ANIM_OFF);
	lv_label_set_text_fmt(ui_S1BarBattLabel, "FL %d%%", batPerc);
}

void ui_ScrMainUpdateClock(const char* clockStr, const char* dateStr) {
	lv_label_set_text(ui_S1LabelClock, clockStr);
}

void ui_ScrMainUpdateFast(float speed, float grad) {
	lv_label_set_text_fmt(ui_S1LabelSpeed, "%.1f", speed);
	lv_arc_set_value(ui_S1ArcSpeed, (uint16_t)(speed*10.0));
	lv_label_set_text_fmt(ui_S1PStatLGradientVar, "%.1f", grad);
}

void ui_ScrMainUpdateTimeMode(const char* tmStr) {
	lv_label_set_text(ui_S1PStatLTime, tmStr);
}


void ui_ScrMainUpdateStats(const char* modeStr, float avgSpd, float maxSpd, uint32_t dist, uint32_t timeInS) {
	lv_label_set_text(ui_S1PStatLTitle, modeStr);
	lv_label_set_text_fmt(ui_S1PStatLspdMaxVar, "%.1f", maxSpd);
	lv_label_set_text_fmt(ui_S1PStatLAvgVar, "%.1f", avgSpd);
	lv_arc_set_value(ui_S1ArcAvg, (int16_t)(avgSpd * 10));
	lv_label_set_text_fmt(ui_S1PStatLDistVar, "%.1f", dist/1000.0);
	lv_label_set_text_fmt(ui_S1PStatLTimeVar, "%02d:%02d:%02d", timeInS / 3600, (timeInS / 60) % 60, timeInS % 60);
}

void ui_ScrMainUpdateCadence(int16_t cadence) {
	lv_label_set_text_fmt(ui_S1LabelCad, (cadence >= 0) ? "%d rpm" : "-/-", cadence);
	lv_arc_set_value(ui_S1ArcCadence, cadence);
}

void ui_ScrMainUpdateHR(int16_t hr) {
	lv_label_set_text_fmt(ui_S1BarPulsLabel, (hr >= 0) ? "%d bpm": "-/-", hr);
	lv_bar_set_value(ui_S1BarPuls, hr, LV_ANIM_OFF);
}

static void msg_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_current_target(e);
    bool ok = lv_msgbox_get_active_btn(obj) == 0;
    msgCppCB(ok);
    lv_msgbox_close(obj);
    msgBox = 0;
}

void ui_MsgBox(const char* str) {
    static const char * btns[] = {"Apply", "Close", ""};
    if (msgBox) lv_msgbox_close(msgBox);
    msgBox = lv_msgbox_create(NULL, "Msg", str, btns, true);
    lv_obj_add_event_cb(msgBox, msg_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(msgBox);
}

void ui_MsgBoxUpdate(const char* str) {
	if (!msgBox) return;
	lv_label_set_text(lv_msgbox_get_text(msgBox), str);
}

