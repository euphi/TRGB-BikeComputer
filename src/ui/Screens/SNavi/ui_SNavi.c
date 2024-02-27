// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.3.4
// LVGL version: 8.3.6
// Project name: BC_ScreenNavspj

#include "ui.h"
#include <ui/img/img.h>

void ui_SNavi_screen_init(void)
{
    ui_SNavi = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_SNavi, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_SNavPanelStat = lv_obj_create(ui_SNavi);
    lv_obj_set_height(ui_SNavPanelStat, 200);
    lv_obj_set_width(ui_SNavPanelStat, lv_pct(24));
    lv_obj_set_y(ui_SNavPanelStat, 0);
    lv_obj_set_x(ui_SNavPanelStat, lv_pct(-34));
    lv_obj_set_align(ui_SNavPanelStat, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_SNavPanelStat, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_SNavLabelStreet = lv_label_create(ui_SNavi);
    lv_obj_set_width(ui_SNavLabelStreet, 380);
    lv_obj_set_height(ui_SNavLabelStreet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SNavLabelStreet, 0);
    lv_obj_set_y(ui_SNavLabelStreet, lv_pct(-30));
    lv_obj_set_align(ui_SNavLabelStreet, LV_ALIGN_CENTER);
    lv_label_set_long_mode(ui_SNavLabelStreet, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(ui_SNavLabelStreet, "Badstraße - FÜ 234");
    lv_obj_set_style_text_align(ui_SNavLabelStreet, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_SNavLabelStreet, &ui_font_SchildGrot36, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SNavArcSpeed = lv_arc_create(ui_SNavi);
    lv_obj_set_width(ui_SNavArcSpeed, 480);
    lv_obj_set_height(ui_SNavArcSpeed, 480);
    lv_obj_set_align(ui_SNavArcSpeed, LV_ALIGN_CENTER);


    ui_SNavImgNav = lv_img_create(ui_SNavi);
    lv_img_set_src(ui_SNavImgNav, &nav_finish);
    lv_obj_set_width(ui_SNavImgNav, 192);
    lv_obj_set_height(ui_SNavImgNav, 192);
    lv_obj_set_align(ui_SNavImgNav, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_SNavImgNav, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_SNavImgNav, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_SNavImgNav, lv_color_hex(0xAAAAAA), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_SNavImgNav, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SNavBarNavDist = lv_bar_create(ui_SNavi);
    lv_bar_set_value(ui_SNavBarNavDist, 25, LV_ANIM_OFF);
    lv_bar_set_start_value(ui_SNavBarNavDist, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_SNavBarNavDist, 20);
    lv_obj_set_height(ui_SNavBarNavDist, 200);
    lv_obj_set_y(ui_SNavBarNavDist, 0);
    lv_obj_set_x(ui_SNavBarNavDist, lv_pct(25));
    lv_obj_set_align(ui_SNavBarNavDist, LV_ALIGN_CENTER);

    ui_SNavLabelNavDist = lv_label_create(ui_SNavi);
    lv_obj_set_width(ui_SNavLabelNavDist, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SNavLabelNavDist, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SNavLabelNavDist, lv_pct(35));
    lv_obj_set_y(ui_SNavLabelNavDist, lv_pct(0));
    lv_obj_set_align(ui_SNavLabelNavDist, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SNavLabelNavDist, "200m");
    lv_obj_set_style_text_font(ui_SNavLabelNavDist, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SNavLabelSpeed = lv_label_create(ui_SNavi);
    lv_obj_set_width(ui_SNavLabelSpeed, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SNavLabelSpeed, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SNavLabelSpeed, 0);
    lv_obj_set_y(ui_SNavLabelSpeed, lv_pct(33));
    lv_obj_set_align(ui_SNavLabelSpeed, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SNavLabelSpeed, "22,3");
    lv_obj_set_style_text_font(ui_SNavLabelSpeed, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SNavBarHR = lv_bar_create(ui_SNavi);
    lv_bar_set_range(ui_SNavBarHR, 60, 180);
    lv_bar_set_value(ui_SNavBarHR, 25, LV_ANIM_OFF);
    lv_bar_set_start_value(ui_SNavBarHR, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_SNavBarHR, 150);
    lv_obj_set_height(ui_SNavBarHR, 12);
    lv_obj_set_x(ui_SNavBarHR, 0);
    lv_obj_set_y(ui_SNavBarHR, 220);
    lv_obj_set_align(ui_SNavBarHR, LV_ALIGN_CENTER);

    ui_SNavLabenHR = lv_label_create(ui_SNavi);
    lv_obj_set_width(ui_SNavLabenHR, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SNavLabenHR, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SNavLabenHR, 0);
    lv_obj_set_y(ui_SNavLabenHR, 200);
    lv_obj_set_align(ui_SNavLabenHR, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SNavLabenHR, "125bpm");
    lv_obj_set_style_text_font(ui_SNavLabenHR, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SNavLabelCad = lv_label_create(ui_SNavi);
    lv_obj_set_width(ui_SNavLabelCad, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SNavLabelCad, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SNavLabelCad, 0);
    lv_obj_set_y(ui_SNavLabelCad, -180);
    lv_obj_set_align(ui_SNavLabelCad, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SNavLabelCad, "101 rpm");
    lv_obj_set_style_text_font(ui_SNavLabelCad, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SNavBarBat = lv_bar_create(ui_SNavi);
    lv_bar_set_value(ui_SNavBarBat, 25, LV_ANIM_OFF);
    lv_bar_set_start_value(ui_SNavBarBat, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_SNavBarBat, 120);
    lv_obj_set_height(ui_SNavBarBat, 10);
    lv_obj_set_x(ui_SNavBarBat, 0);
    lv_obj_set_y(ui_SNavBarBat, -205);
    lv_obj_set_align(ui_SNavBarBat, LV_ALIGN_CENTER);

    ui_ImgButton1 = lv_imgbtn_create(ui_SNavi);
    lv_imgbtn_set_src(ui_ImgButton1, LV_IMGBTN_STATE_RELEASED, NULL, &ui_img_ic_settings_48px_svg_png, NULL);
    lv_obj_set_height(ui_ImgButton1, 64);
    lv_obj_set_width(ui_ImgButton1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_x(ui_ImgButton1, 166);
    lv_obj_set_y(ui_ImgButton1, 60);
    lv_obj_set_align(ui_ImgButton1, LV_ALIGN_CENTER);
    lv_obj_set_style_img_recolor(ui_ImgButton1, lv_color_hex(0x00FFE8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor_opa(ui_ImgButton1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_SNavi, ui_event_SNavi, LV_EVENT_ALL, NULL);

}