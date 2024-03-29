// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.4
// LVGL version: 8.3.6
// Project name: ScreenChart

#include "ui.h"

void ui_SChart_screen_init(void)
{
    ui_SChart = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_SChart, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Chart1 = lv_chart_create(ui_SChart);
    lv_obj_set_width(ui_Chart1, 400);
    lv_obj_set_height(ui_Chart1, 320);
    lv_obj_set_align(ui_Chart1, LV_ALIGN_CENTER);
    lv_chart_set_type(ui_Chart1, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ui_Chart1, 400);
    lv_chart_set_range(ui_Chart1, LV_CHART_AXIS_PRIMARY_Y, 0, 500);
    lv_chart_set_range(ui_Chart1, LV_CHART_AXIS_SECONDARY_Y, 0, 50);
    lv_chart_set_axis_tick(ui_Chart1, LV_CHART_AXIS_PRIMARY_X, 10, 5, 5, 2, true, 50);
    lv_chart_set_axis_tick(ui_Chart1, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 10, 2, false, 50);
    lv_chart_set_axis_tick(ui_Chart1, LV_CHART_AXIS_SECONDARY_Y, 10, 5, 5, 2, false, 25);
    ui_Chart1_series[0] = lv_chart_add_series(ui_Chart1, lv_color_hex(0xFF0000), LV_CHART_AXIS_PRIMARY_Y);
    ui_Chart1_series[1] = lv_chart_add_series(ui_Chart1, lv_color_hex(0x00FF00), LV_CHART_AXIS_PRIMARY_Y);
    ui_Chart1_series[2] = lv_chart_add_series(ui_Chart1, lv_color_hex(0x0000FF), LV_CHART_AXIS_SECONDARY_Y);
    ui_Chart1_series[3] = lv_chart_add_series(ui_Chart1, lv_color_hex(0xFFFF00), LV_CHART_AXIS_SECONDARY_Y);


    ui_LabelSpeed = lv_label_create(ui_Chart1);
    lv_obj_set_width(ui_LabelSpeed, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LabelSpeed, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_LabelSpeed, 0);
    lv_obj_set_y(ui_LabelSpeed, -120);
    lv_obj_set_align(ui_LabelSpeed, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LabelSpeed, "25,3");
    lv_obj_set_style_text_color(ui_LabelSpeed, lv_color_hex(0x00FFF7), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_LabelSpeed, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_LabelSpeed, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_PanelMode = lv_obj_create(ui_SChart);
    lv_obj_set_width(ui_PanelMode, 250);
    lv_obj_set_height(ui_PanelMode, 50);
    lv_obj_set_x(ui_PanelMode, 0);
    lv_obj_set_y(ui_PanelMode, 25);
    lv_obj_set_align(ui_PanelMode, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_PanelMode, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_PanelMode, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(ui_PanelMode, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_ButtonGrad = lv_btn_create(ui_PanelMode);
    lv_obj_set_width(ui_ButtonGrad, 100);
    lv_obj_set_height(ui_ButtonGrad, 40);
    lv_obj_set_x(ui_ButtonGrad, -4);
    lv_obj_set_y(ui_ButtonGrad, 189);
    lv_obj_set_align(ui_ButtonGrad, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonGrad, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonGrad, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_LButtonGrad = lv_label_create(ui_ButtonGrad);
    lv_obj_set_width(ui_LButtonGrad, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LButtonGrad, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LButtonGrad, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LButtonGrad, "Gradient");

    ui_ButtonPuls = lv_btn_create(ui_PanelMode);
    lv_obj_set_width(ui_ButtonPuls, 100);
    lv_obj_set_height(ui_ButtonPuls, 40);
    lv_obj_set_x(ui_ButtonPuls, -4);
    lv_obj_set_y(ui_ButtonPuls, 189);
    lv_obj_set_align(ui_ButtonPuls, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_ButtonPuls, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ButtonPuls, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_LButtonPuls = lv_label_create(ui_ButtonPuls);
    lv_obj_set_width(ui_LButtonPuls, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_LButtonPuls, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_LButtonPuls, LV_ALIGN_CENTER);
    lv_label_set_text(ui_LButtonPuls, "Puls");

    lv_obj_add_event_cb(ui_ButtonGrad, ui_event_ButtonGrad, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_ButtonPuls, ui_event_ButtonPuls, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_SChart, ui_event_SChart, LV_EVENT_ALL, NULL);

}
