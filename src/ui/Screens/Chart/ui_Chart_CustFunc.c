#include <lvgl.h>
#include <ui/Screens/Chart/ui_Chart_CustFunc.h>

lv_obj_t* ui_SChart_SBack;

void ui_ScrChartSetBackScreen(lv_obj_t* const screenBack) {
	ui_SChart_SBack = screenBack;
}

void ui_ScrChartSetExtArray1(int16_t array[], uint8_t index) {
	// Note that size is set in init to 400 (it must be same for all series so it not useful to change it)
	lv_chart_set_ext_y_array(ui_Chart1, ui_Chart1_series[index], array);
}

void ui_ScrChartSetPostFirst(uint16_t pos, uint8_t idx) {
	lv_chart_set_x_start_point(ui_Chart1, ui_Chart1_series[idx], pos);
}

void ui_ScrChartRefresh() {
	lv_chart_refresh(ui_Chart1);
}
