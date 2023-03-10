// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: BC_ScreenNavspj

#include "ui_Navi.h"
#include "ui.h"
#include "ui_helpers.h"

///////////////////// VARIABLES ////////////////////
void ui_event_SNavi(lv_event_t * e);
lv_obj_t * ui_SNavi;
lv_obj_t * ui_SNavPanelStat;
lv_obj_t * ui_SNavArcSpeed;
lv_obj_t * ui_SNavImgNav;
lv_obj_t * ui_SNavBarNavDist;
lv_obj_t * ui_SNavLabelStreet;
lv_obj_t * ui_SNavLabelNavDist;
lv_obj_t * ui_SNavLabelSpeed;
lv_obj_t * ui_SNavBarHR;
lv_obj_t * ui_SNavLabenHR;
lv_obj_t * ui_SNavLabelCad;
lv_obj_t * ui_SNavBarBat;

///////////////////// TEST LVGL SETTINGS ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////
void ui_event_SNavi(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        _ui_screen_change(ui_S1Main, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0);
    }
}

///////////////////// SCREENS ////////////////////
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

    ui_SNavArcSpeed = lv_arc_create(ui_SNavi);
    lv_obj_set_width(ui_SNavArcSpeed, 480);
    lv_obj_set_height(ui_SNavArcSpeed, 480);
    lv_obj_set_align(ui_SNavArcSpeed, LV_ALIGN_CENTER);

    ui_SNavImgNav = lv_img_create(ui_SNavi);
    lv_obj_set_width(ui_SNavImgNav, LV_SIZE_CONTENT);   /// 200
    lv_obj_set_height(ui_SNavImgNav, LV_SIZE_CONTENT);    /// 200
    lv_obj_set_align(ui_SNavImgNav, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_SNavImgNav, LV_OBJ_FLAG_ADV_HITTEST);     /// Flags
    lv_obj_clear_flag(ui_SNavImgNav, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_SNavImgNav, lv_color_hex(0xAAAAAA), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_SNavImgNav, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SNavBarNavDist = lv_bar_create(ui_SNavi);
    lv_bar_set_value(ui_SNavBarNavDist, 25, LV_ANIM_OFF);
    lv_obj_set_width(ui_SNavBarNavDist, 20);
    lv_obj_set_height(ui_SNavBarNavDist, 200);
    lv_obj_set_y(ui_SNavBarNavDist, 0);
    lv_obj_set_x(ui_SNavBarNavDist, lv_pct(25));
    lv_obj_set_align(ui_SNavBarNavDist, LV_ALIGN_CENTER);

    ui_SNavLabelStreet = lv_label_create(ui_SNavi);
    lv_obj_set_width(ui_SNavLabelStreet, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SNavLabelStreet, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SNavLabelStreet, 0);
    lv_obj_set_y(ui_SNavLabelStreet, lv_pct(-30));
    lv_obj_set_align(ui_SNavLabelStreet, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SNavLabelStreet, "Badstra??e");
    lv_obj_set_style_text_font(ui_SNavLabelStreet, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SNavLabelNavDist = lv_label_create(ui_SNavi);
    lv_obj_set_width(ui_SNavLabelNavDist, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SNavLabelNavDist, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SNavLabelNavDist, 175);
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
    lv_obj_set_width(ui_SNavBarHR, 150);
    lv_obj_set_height(ui_SNavBarHR, 12);
    lv_obj_set_x(ui_SNavBarHR, 0);
    lv_obj_set_y(ui_SNavBarHR, lv_pct(-39));
    lv_obj_set_align(ui_SNavBarHR, LV_ALIGN_CENTER);

    ui_SNavLabenHR = lv_label_create(ui_SNavi);
    lv_obj_set_width(ui_SNavLabenHR, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SNavLabenHR, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SNavLabenHR, 0);
    lv_obj_set_y(ui_SNavLabenHR, lv_pct(-43));
    lv_obj_set_align(ui_SNavLabenHR, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SNavLabenHR, "125bpm");
    lv_obj_set_style_text_font(ui_SNavLabenHR, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SNavLabelCad = lv_label_create(ui_SNavi);
    lv_obj_set_width(ui_SNavLabelCad, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SNavLabelCad, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SNavLabelCad, 0);
    lv_obj_set_y(ui_SNavLabelCad, 197);
    lv_obj_set_align(ui_SNavLabelCad, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SNavLabelCad, "101 rpm");
    lv_obj_set_style_text_font(ui_SNavLabelCad, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SNavBarBat = lv_bar_create(ui_SNavi);
    lv_bar_set_value(ui_SNavBarBat, 25, LV_ANIM_OFF);
    lv_obj_set_width(ui_SNavBarBat, 120);
    lv_obj_set_height(ui_SNavBarBat, 10);
    lv_obj_set_x(ui_SNavBarBat, 0);
    lv_obj_set_y(ui_SNavBarBat, 229);
    lv_obj_set_align(ui_SNavBarBat, LV_ALIGN_CENTER);

    lv_obj_add_event_cb(ui_SNavi, ui_event_SNavi, LV_EVENT_ALL, NULL);

}

