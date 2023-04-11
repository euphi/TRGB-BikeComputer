/*
 * ui_custFunc.c
 *
 *  Created on: 05.03.2023
 *      Author: ian
 */

#include "ui.h"
#include <math.h> // to initialize float to NAN


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

void ui_ScrChartUpdateSpeed(float speed) {
	lv_label_set_text_fmt(ui_ScreenChartLabelSpeed, "%.1f", speed);
}


float ui_ScrChartUpdateBat(float batVolt, int8_t perc, char *batString) {
	static uint32_t count = 0;
	float avg = NAN;
	lv_chart_set_next_value(ui_ScreenChart_Chart1, ui_ScreenChart_Chart1_series_v, (uint16_t)(batVolt * 100));		// Add voltage every call
	if (++count % 60 == 0) {	// for each 60th call, calculate avarage and set percentage
		avg = 0.0;
		int16_t cur_point = ui_ScreenChart_Chart1_series_v->start_point;
		for (uint8_t c = 0; c < 60; c++) {
			if (--cur_point < 0) cur_point = ((lv_chart_t*) ui_ScreenChart_Chart1)->point_cnt - 1;
			float v1 = ui_ScreenChart_Chart1_series_v->y_points[cur_point] / 100.0;
			avg += v1;
		}
		avg = avg / 60.0;
		lv_chart_set_next_value(ui_ScreenChart_Chart1, ui_ScreenChart_Chart1_ser_v_per_minute, (uint16_t)(avg * 100));
		lv_chart_set_next_value(ui_ScreenChart_Chart1, ui_ScreenChart_Chart1_series_perc_per_minute, perc);
	}

	lv_chart_refresh(ui_ScreenChart_Chart1);
	lv_label_set_text_fmt(ui_S1BarBattLabel, "I %s", batString);
	lv_label_set_text(ui_ScreenChartLabelInfo, batString);
	return avg;
}

