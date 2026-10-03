#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;

//
// Event handlers
//

lv_obj_t *tick_value_change_obj;

//
// Screens
//

void create_screen_rim_ridge() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.rim_ridge = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 480);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // rr_speed_arc
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.rr_speed_arc = obj;
            lv_obj_set_pos(obj, 5, 5);
            lv_obj_set_size(obj, 470, 470);
            lv_arc_set_range(obj, 0, 60);
            lv_arc_set_value(obj, 28);
            lv_arc_set_bg_start_angle(obj, 120);
            lv_arc_set_bg_end_angle(obj, 60);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_arc_color(obj, lv_color_hex(theme_colors[active_theme_index][18]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 16, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
        {
            // rr_ic_wifi
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.rr_ic_wifi = obj;
            lv_obj_set_pos(obj, 172, 32);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_rr_icon_wifi);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // rr_ic_gps
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.rr_ic_gps = obj;
            lv_obj_set_pos(obj, 224, 32);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_rr_icon_gps);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // rr_nav_pill
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_nav_pill = obj;
            lv_obj_set_pos(obj, 140, 64);
            lv_obj_set_size(obj, 200, 78);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_go_to_nav, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_nav_dist
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_nav_dist = obj;
                    lv_obj_set_pos(obj, 94, 24);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "180 m");
                }
                {
                    // rr_ic_turn
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_turn = obj;
                    lv_obj_set_pos(obj, 15, 7);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_turn);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
        {
            // rr_btn_pause
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_btn_pause = obj;
            lv_obj_set_pos(obj, 123, 383);
            lv_obj_set_size(obj, 50, 50);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_pause_long_press, LV_EVENT_LONG_PRESSED, (void *)0);
            lv_obj_add_event_cb(obj, action_pause_click, LV_EVENT_SHORT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 25, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_ic_pause
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_pause = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_pause);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_width(obj, 10193, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
        {
            // rr_tour_pill
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_tour_pill = obj;
            lv_obj_set_pos(obj, 173, 378);
            lv_obj_set_size(obj, 134, 57);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_tour_pill_gesture, LV_EVENT_GESTURE, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE|LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_distance_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_distance_val = obj;
                    lv_obj_set_pos(obj, 0, 8);
                    lv_obj_set_size(obj, 134, LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "2138.6 km");
                }
                {
                    // rr_tour_label
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_tour_label = obj;
                    lv_obj_set_pos(obj, 59, 35);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_left(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "TOUR 1");
                }
            }
        }
        {
            // rr_btn_settings
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_btn_settings = obj;
            lv_obj_set_pos(obj, 307, 383);
            lv_obj_set_size(obj, 50, 50);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_go_to_settings, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 25, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_ic_settings
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_settings = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_settings_icon);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
        {
            // rr_lane_row
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.rr_lane_row = obj;
            lv_obj_set_pos(obj, 100, 80);
            lv_obj_set_size(obj, 80, 50);
            lv_img_set_src(obj, &img_rr_icon_turn_64);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // rr_grp_cadence
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_grp_cadence = obj;
            lv_obj_set_pos(obj, 64, 177);
            lv_obj_set_size(obj, 32, 66);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_ic_cadence
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_cadence = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_cadence);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rr_cadence_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_cadence_val = obj;
                    lv_obj_set_pos(obj, 0, 5);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "82");
                }
                {
                    // rr_cadence_unit
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_cadence_unit = obj;
                    lv_obj_set_pos(obj, 0, 25);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "rpm");
                }
            }
        }
        {
            // rr_grp_power
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_grp_power = obj;
            lv_obj_set_pos(obj, 378, 174);
            lv_obj_set_size(obj, 44, 69);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_ic_power
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_power = obj;
                    lv_obj_set_pos(obj, 6, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_power);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rr_power_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_power_val = obj;
                    lv_obj_set_pos(obj, 0, 6);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "186");
                }
                {
                    // rr_power_unit
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_power_unit = obj;
                    lv_obj_set_pos(obj, 0, 26);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "W");
                }
            }
        }
        {
            // rr_grp_speed
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_grp_speed = obj;
            lv_obj_set_pos(obj, 106, 188);
            lv_obj_set_size(obj, 268, 118);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_speed_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_speed_val = obj;
                    lv_obj_set_pos(obj, 0, -14);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][20]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_by7x128, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "28.4");
                }
                {
                    // rr_speed_unit
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_speed_unit = obj;
                    lv_obj_set_pos(obj, 0, 49);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "KM/H");
                }
            }
        }
        {
            // rr_grp_temp
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_grp_temp = obj;
            lv_obj_set_pos(obj, 72, 309);
            lv_obj_set_size(obj, 63, 32);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_ic_temp
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_temp = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_temp);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rr_temp_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_temp_val = obj;
                    lv_obj_set_pos(obj, 32, 7);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat22, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "14°");
                }
            }
        }
        {
            // rr_grp_height
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_grp_height = obj;
            lv_obj_set_pos(obj, 0, 80);
            lv_obj_set_size(obj, 110, 32);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_go_to_climb, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_ic_height
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_height = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_height);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][23]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rr_height_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_height_val = obj;
                    lv_obj_set_pos(obj, 32, 5);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat22, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "231m");
                }
            }
        }
        {
            // rr_grp_gradient
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_grp_gradient = obj;
            lv_obj_set_pos(obj, 312, 309);
            lv_obj_set_size(obj, 97, 32);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_go_to_climb, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_ic_gradient
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_gradient = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_gradient);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rr_gradient_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_gradient_val = obj;
                    lv_obj_set_pos(obj, 32, 7);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat22, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "+4.2%");
                }
            }
        }
        {
            // rr_grp_heart
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_grp_heart = obj;
            lv_obj_set_pos(obj, 110, 337);
            lv_obj_set_size(obj, 260, 43);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_ic_heart
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_heart = obj;
                    lv_obj_set_pos(obj, 92, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_heart);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rr_hr_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_hr_val = obj;
                    lv_obj_set_pos(obj, 122, 6);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat22, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "142");
                }
                {
                    // rr_hr_bar
                    lv_obj_t *obj = lv_bar_create(parent_obj);
                    objects.rr_hr_bar = obj;
                    lv_obj_set_pos(obj, 0, 29);
                    lv_obj_set_size(obj, 260, 14);
                    lv_bar_set_range(obj, 40, 200);
                    lv_bar_set_value(obj, 142, LV_ANIM_OFF);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][26]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_grad_color(obj, lv_color_hex(theme_colors[active_theme_index][30]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_main_stop(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_grad_stop(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][26]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_grad_color(obj, lv_color_hex(theme_colors[active_theme_index][30]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_main_stop(obj, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_grad_stop(obj, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 7, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                }
            }
        }
        {
            // rr_grp_time
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_grp_time = obj;
            lv_obj_set_pos(obj, 164, 144);
            lv_obj_set_size(obj, 122, 32);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_ic_time
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_time = obj;
                    lv_obj_set_pos(obj, -45, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_stopwatch);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rr_time_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_time_val = obj;
                    lv_obj_set_pos(obj, 15, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_letter_space(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "1:24:07");
                }
            }
        }
        {
            // rr_grp_battery
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_grp_battery = obj;
            lv_obj_set_pos(obj, 276, 32);
            lv_obj_set_size(obj, 32, 32);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_ic_battery
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_battery = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_battery);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rr_battery_fill
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.rr_battery_fill = obj;
                    lv_obj_set_pos(obj, 9, 25);
                    lv_obj_set_size(obj, 9, 7);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][23]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
        {
            // rr_group_rq_mode
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_group_rq_mode = obj;
            lv_obj_set_pos(obj, 0, -5);
            lv_obj_set_size(obj, 197, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_go_to_rq, LV_EVENT_CLICKED, (void *)0);
            lv_obj_set_style_align(obj, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rr_ic_state
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_state = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_state_power);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE|LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rr_line_rq
                    lv_obj_t *obj = lv_line_create(parent_obj);
                    objects.rr_line_rq = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 140, 5);
                    static lv_point_t line_points[] = {
                        { 0, 0 },
                        { 140, 0 }
                    };
                    lv_line_set_points(obj, line_points, 2);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
                    lv_obj_set_style_align(obj, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_line_width(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
    }
    
    tick_screen_rim_ridge();
}

void tick_screen_rim_ridge() {
}

void create_screen_rim_ridge_nav() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.rim_ridge_nav = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 480);
    lv_obj_add_event_cb(obj, action_nav_screen_gesture, LV_EVENT_GESTURE, (void *)0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // rrnav_dist_arc
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.rrnav_dist_arc = obj;
            lv_obj_set_pos(obj, 5, 5);
            lv_obj_set_size(obj, 470, 470);
            lv_arc_set_value(obj, 55);
            lv_arc_set_bg_start_angle(obj, 120);
            lv_arc_set_bg_end_angle(obj, 60);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_arc_color(obj, lv_color_hex(theme_colors[active_theme_index][18]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 16, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
        {
            // rrnav_lane_row
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.rrnav_lane_row = obj;
            lv_obj_set_pos(obj, 100, 280);
            lv_obj_set_size(obj, 280, 50);
            lv_img_set_src(obj, &img_rr_icon_turn_64);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // rrnav_street
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrnav_street = obj;
            lv_obj_set_pos(obj, 0, 28);
            lv_obj_set_size(obj, 440, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_font_schild22, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "Feldweg");
        }
        {
            // rrnav_grp_turn
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrnav_grp_turn = obj;
            lv_obj_set_pos(obj, 144, 22);
            lv_obj_set_size(obj, 192, 262);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrnav_ic_turn
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrnav_ic_turn = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 192, 192);
                    lv_img_set_src(obj, &img_rr_icon_turn_large);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrnav_dist_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrnav_dist_val = obj;
                    lv_obj_set_pos(obj, 0, 83);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][20]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat48, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "180 m");
                }
            }
        }
        {
            // rrnav_grp_next
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrnav_grp_next = obj;
            lv_obj_set_pos(obj, 340, 86);
            lv_obj_set_size(obj, 64, 86);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrnav_ic_next
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrnav_ic_next = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_turn_64);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrnav_next_dist
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrnav_next_dist = obj;
                    lv_obj_set_pos(obj, 1, 30);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "60 m");
                }
            }
        }
        {
            // rrnav_grp_speed
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrnav_grp_speed = obj;
            lv_obj_set_pos(obj, 152, 338);
            lv_obj_set_size(obj, 100, 55);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrnav_speed_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrnav_speed_val = obj;
                    lv_obj_set_pos(obj, 0, -6);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat48, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "28.4");
                }
                {
                    // rrnav_speed_unit
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrnav_speed_unit = obj;
                    lv_obj_set_pos(obj, 0, 20);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "KM/H");
                }
            }
        }
        {
            // rrnav_grp_gradient
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrnav_grp_gradient = obj;
            lv_obj_set_pos(obj, 273, 341);
            lv_obj_set_size(obj, 112, 32);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrnav_ic_gradient
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrnav_ic_gradient = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_gradient);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrnav_gradient_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrnav_gradient_val = obj;
                    lv_obj_set_pos(obj, 29, 2);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "+4.2%");
                }
            }
        }
        {
            // rrnav_grp_heart
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrnav_grp_heart = obj;
            lv_obj_set_pos(obj, 204, 401);
            lv_obj_set_size(obj, 69, 32);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrnav_ic_heart
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrnav_ic_heart = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_heart);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrnav_hr_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrnav_hr_val = obj;
                    lv_obj_set_pos(obj, 30, 4);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "142");
                }
            }
        }
        {
            // rrnav_group_rq_mode
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrnav_group_rq_mode = obj;
            lv_obj_set_pos(obj, 0, -5);
            lv_obj_set_size(obj, 197, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_go_to_rq, LV_EVENT_CLICKED, (void *)0);
            lv_obj_set_style_align(obj, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrnav_ic_state
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrnav_ic_state = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_state_power);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE|LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrnav_line_rq
                    lv_obj_t *obj = lv_line_create(parent_obj);
                    objects.rrnav_line_rq = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 140, 5);
                    static lv_point_t line_points[] = {
                        { 0, 0 },
                        { 140, 0 }
                    };
                    lv_line_set_points(obj, line_points, 2);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
                    lv_obj_set_style_align(obj, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_line_width(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
    }
    
    tick_screen_rim_ridge_nav();
}

void tick_screen_rim_ridge_nav() {
}

void create_screen_rim_ridge_rq() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.rim_ridge_rq = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 480);
    lv_obj_add_event_cb(obj, action_rq_screen_gesture, LV_EVENT_GESTURE, (void *)0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // rq_nav_pill
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_nav_pill = obj;
            lv_obj_set_pos(obj, 155, 30);
            lv_obj_set_size(obj, 170, 64);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_ic_turn
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rq_ic_turn = obj;
                    lv_obj_set_pos(obj, 12, 1);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_turn);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rq_nav_dist
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rq_nav_dist = obj;
                    lv_obj_set_pos(obj, 76, 22);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "180 m");
                }
            }
        }
        {
            // rq_speed_val
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rq_speed_val = obj;
            lv_obj_set_pos(obj, -90, -118);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][20]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "24.6");
        }
        {
            // rq_speed_unit
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rq_speed_unit = obj;
            lv_obj_set_pos(obj, -90, -81);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "KM/H");
        }
        {
            // rq_ic_heart
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.rq_ic_heart = obj;
            lv_obj_set_pos(obj, 134, 169);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_rr_icon_heart);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // rq_hr_val
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rq_hr_val = obj;
            lv_obj_set_pos(obj, -90, -32);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat22, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "138");
        }
        {
            // rq_ic_dist
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.rq_ic_dist = obj;
            lv_obj_set_pos(obj, 314, 168);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_rr_icon_ruler);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // rq_dist_val
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rq_dist_val = obj;
            lv_obj_set_pos(obj, 90, -31);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat22, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "3.2 km");
        }
        {
            // rq_rq_val
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rq_rq_val = obj;
            lv_obj_set_pos(obj, 90, -118);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][28]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "3");
        }
        {
            // rq_rq_caption
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rq_rq_caption = obj;
            lv_obj_set_pos(obj, 90, -81);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "RQ-KLASSE");
        }
        {
            // rq_surf_caption
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rq_surf_caption = obj;
            lv_obj_set_pos(obj, 1, 7);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "UNTERGRUND");
        }
        {
            // rq_surf_asphalt
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_surf_asphalt = obj;
            lv_obj_set_pos(obj, 64, 260);
            lv_obj_set_size(obj, 120, 48);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_surf_asphalt_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rq_surf_asphalt_lbl = obj;
                    lv_obj_set_pos(obj, 0, 1);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "ASPHALT");
                }
            }
        }
        {
            // rq_surf_schotter
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_surf_schotter = obj;
            lv_obj_set_pos(obj, 191, 260);
            lv_obj_set_size(obj, 120, 48);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_surf_schotter_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rq_surf_schotter_lbl = obj;
                    lv_obj_set_pos(obj, 0, 1);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "SCHOTTER");
                }
            }
        }
        {
            // rq_surf_waldweg
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_surf_waldweg = obj;
            lv_obj_set_pos(obj, 320, 260);
            lv_obj_set_size(obj, 120, 48);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_surf_waldweg_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rq_surf_waldweg_lbl = obj;
                    lv_obj_set_pos(obj, 0, 1);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "WALDWEG");
                }
            }
        }
        {
            // rq_surf_feldweg
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_surf_feldweg = obj;
            lv_obj_set_pos(obj, 64, 316);
            lv_obj_set_size(obj, 120, 48);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_surf_feldweg_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rq_surf_feldweg_lbl = obj;
                    lv_obj_set_pos(obj, 0, 1);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "FELDWEG");
                }
            }
        }
        {
            // rq_surf_pflaster
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_surf_pflaster = obj;
            lv_obj_set_pos(obj, 191, 316);
            lv_obj_set_size(obj, 120, 48);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_surf_pflaster_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rq_surf_pflaster_lbl = obj;
                    lv_obj_set_pos(obj, 0, 1);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "PFLASTER");
                }
            }
        }
        {
            // rq_surf_sonstiges
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_surf_sonstiges = obj;
            lv_obj_set_pos(obj, 320, 316);
            lv_obj_set_size(obj, 120, 48);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_surf_sonstiges_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rq_surf_sonstiges_lbl = obj;
                    lv_obj_set_pos(obj, 0, 1);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "SONSTIGES");
                }
            }
        }
        {
            // rq_qual_caption
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rq_qual_caption = obj;
            lv_obj_set_pos(obj, 0, 133);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text_static(obj, "QUALITÄT");
        }
        {
            // rq_qual_track
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_qual_track = obj;
            lv_obj_set_pos(obj, 150, 397);
            lv_obj_set_size(obj, 180, 3);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 64, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // rq_qual_1
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_qual_1 = obj;
            lv_obj_set_pos(obj, 130, 378);
            lv_obj_set_size(obj, 40, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_qual_1_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rq_qual_1_lbl = obj;
                    lv_obj_set_pos(obj, 0, 1);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "1");
                }
            }
        }
        {
            // rq_qual_2
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_qual_2 = obj;
            lv_obj_set_pos(obj, 190, 378);
            lv_obj_set_size(obj, 40, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_qual_2_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rq_qual_2_lbl = obj;
                    lv_obj_set_pos(obj, 0, 1);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "2");
                }
            }
        }
        {
            // rq_qual_3
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_qual_3 = obj;
            lv_obj_set_pos(obj, 250, 378);
            lv_obj_set_size(obj, 40, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_qual_3_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rq_qual_3_lbl = obj;
                    lv_obj_set_pos(obj, 0, 1);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "3");
                }
            }
        }
        {
            // rq_qual_4
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_qual_4 = obj;
            lv_obj_set_pos(obj, 310, 378);
            lv_obj_set_size(obj, 40, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_qual_4_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rq_qual_4_lbl = obj;
                    lv_obj_set_pos(obj, 0, 1);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "4");
                }
            }
        }
        {
            // rq_btn_record
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_btn_record = obj;
            lv_obj_set_pos(obj, 212, 138);
            lv_obj_set_size(obj, 56, 56);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 28, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_btn_record_ring
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.rq_btn_record_ring = obj;
                    lv_obj_set_pos(obj, 13, 13);
                    lv_obj_set_size(obj, 26, 26);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 13, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rq_btn_record_dot
                    lv_obj_t *obj = lv_obj_create(parent_obj);
                    objects.rq_btn_record_dot = obj;
                    lv_obj_set_pos(obj, 21, 21);
                    lv_obj_set_size(obj, 14, 14);
                    lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][30]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_radius(obj, 7, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
        {
            // rq_group_rq_mode
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rq_group_rq_mode = obj;
            lv_obj_set_pos(obj, 0, -5);
            lv_obj_set_size(obj, 197, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_go_to_rq, LV_EVENT_CLICKED, (void *)0);
            lv_obj_set_style_align(obj, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rq_ic_state
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rq_ic_state = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_state_power);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE|LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rq_line_rq
                    lv_obj_t *obj = lv_line_create(parent_obj);
                    objects.rq_line_rq = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 140, 5);
                    static lv_point_t line_points[] = {
                        { 0, 0 },
                        { 140, 0 }
                    };
                    lv_line_set_points(obj, line_points, 2);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
                    lv_obj_set_style_align(obj, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_line_width(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
    }
    
    tick_screen_rim_ridge_rq();
}

void tick_screen_rim_ridge_rq() {
}

void create_screen_rim_ridge_settings() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.rim_ridge_settings = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 480);
    lv_obj_add_event_cb(obj, action_settings_screen_gesture, LV_EVENT_GESTURE, (void *)0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // rrset_ic_gear
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.rrset_ic_gear = obj;
            lv_obj_set_pos(obj, 216, 24);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_settings_icon);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // rrset_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrset_title = obj;
            lv_obj_set_pos(obj, 0, 78);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_letter_space(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "EINSTELLUNGEN");
        }
        {
            // rrset_build_caption
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrset_build_caption = obj;
            lv_obj_set_pos(obj, 0, 108);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_letter_space(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "BUILD");
        }
        {
            // rrset_build_val
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrset_build_val = obj;
            lv_obj_set_pos(obj, 0, 126);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "v0.0.2-88 (51c4300) #1272");
        }
        {
            // rrset_ip_caption
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrset_ip_caption = obj;
            lv_obj_set_pos(obj, 0, 156);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_letter_space(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "IP-ADRESSE");
        }
        {
            // rrset_ip_val
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrset_ip_val = obj;
            lv_obj_set_pos(obj, 0, 174);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat22, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "192.168.178.47");
        }
        {
            // rrset_btn_wifi
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrset_btn_wifi = obj;
            lv_obj_set_pos(obj, 45, 208);
            lv_obj_set_size(obj, 130, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_settings_wifi, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_opa(obj, 40, LV_PART_MAIN | LV_STATE_DISABLED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrset_btn_wifi_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrset_btn_wifi_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
                    lv_label_set_text(obj, "WLAN an");
                }
            }
        }
        {
            // rrset_btn_ap
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrset_btn_ap = obj;
            lv_obj_set_pos(obj, 179, 208);
            lv_obj_set_size(obj, 130, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_settings_ap, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_opa(obj, 40, LV_PART_MAIN | LV_STATE_DISABLED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrset_btn_ap_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrset_btn_ap_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
                    lv_label_set_text(obj, "Hotspot");
                }
            }
        }
        {
            // rrset_btn_wifisetup
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrset_btn_wifisetup = obj;
            lv_obj_set_pos(obj, 313, 208);
            lv_obj_set_size(obj, 130, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_go_to_wifi, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_opa(obj, 40, LV_PART_MAIN | LV_STATE_DISABLED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrset_btn_wifisetup_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrset_btn_wifisetup_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
                    lv_label_set_text(obj, "Netzwerke");
                }
            }
        }
        {
            // rrset_btn_cal
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrset_btn_cal = obj;
            lv_obj_set_pos(obj, 50, 258);
            lv_obj_set_size(obj, 185, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_settings_cal, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_opa(obj, 40, LV_PART_MAIN | LV_STATE_DISABLED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrset_btn_cal_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrset_btn_cal_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
                    lv_label_set_text(obj, "Kalibrieren");
                }
            }
        }
        {
            // rrset_btn_ref
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrset_btn_ref = obj;
            lv_obj_set_pos(obj, 245, 258);
            lv_obj_set_size(obj, 185, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_settings_ref, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_opa(obj, 40, LV_PART_MAIN | LV_STATE_DISABLED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrset_btn_ref_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrset_btn_ref_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
                    lv_label_set_text(obj, "Referenzfahrt");
                }
            }
        }
        {
            // rrset_cal_status
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrset_cal_status = obj;
            lv_obj_set_pos(obj, 50, 302);
            lv_obj_set_size(obj, 185, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "kalibriert 27.09.");
        }
        {
            // rrset_ref_status
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrset_ref_status = obj;
            lv_obj_set_pos(obj, 245, 302);
            lv_obj_set_size(obj, 185, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Standard (150 mg)");
        }
        {
            // rrset_btn_reset
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrset_btn_reset = obj;
            lv_obj_set_pos(obj, 85, 346);
            lv_obj_set_size(obj, 150, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_settings_reset, LV_EVENT_LONG_PRESSED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrset_ic_reset
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrset_ic_reset = obj;
                    lv_obj_set_pos(obj, 16, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_cadence);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_LEFT_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrset_btn_reset_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrset_btn_reset_lbl = obj;
                    lv_obj_set_pos(obj, 54, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_LEFT_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Neustart");
                }
            }
        }
        {
            // rrset_btn_sleep
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrset_btn_sleep = obj;
            lv_obj_set_pos(obj, 245, 346);
            lv_obj_set_size(obj, 150, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_settings_sleep, LV_EVENT_LONG_PRESSED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrset_ic_sleep
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrset_ic_sleep = obj;
                    lv_obj_set_pos(obj, 13, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_moon);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_LEFT_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrset_btn_sleep_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrset_btn_sleep_lbl = obj;
                    lv_obj_set_pos(obj, 51, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_LEFT_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Tiefschlaf");
                }
            }
        }
        {
            // rrset_hint
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrset_hint = obj;
            lv_obj_set_pos(obj, 0, 392);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "lange drücken");
        }
        {
            // rrset_group_rq_mode
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrset_group_rq_mode = obj;
            lv_obj_set_pos(obj, 0, -5);
            lv_obj_set_size(obj, 197, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_go_to_rq, LV_EVENT_CLICKED, (void *)0);
            lv_obj_set_style_align(obj, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrset_ic_state
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrset_ic_state = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_state_power);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE|LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrset_line_rq
                    lv_obj_t *obj = lv_line_create(parent_obj);
                    objects.rrset_line_rq = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 140, 5);
                    static lv_point_t line_points[] = {
                        { 0, 0 },
                        { 140, 0 }
                    };
                    lv_line_set_points(obj, line_points, 2);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
                    lv_obj_set_style_align(obj, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_line_width(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
    }
    
    tick_screen_rim_ridge_settings();
}

void tick_screen_rim_ridge_settings() {
}

void create_screen_rim_ridge_climb() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.rim_ridge_climb = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 480);
    lv_obj_add_event_cb(obj, action_climb_screen_gesture, LV_EVENT_GESTURE, (void *)0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // rrclimb_arc
            lv_obj_t *obj = lv_arc_create(parent_obj);
            objects.rrclimb_arc = obj;
            lv_obj_set_pos(obj, 5, 5);
            lv_obj_set_size(obj, 470, 470);
            lv_arc_set_value(obj, 38);
            lv_arc_set_bg_start_angle(obj, 120);
            lv_arc_set_bg_end_angle(obj, 60);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN_HOR|LV_OBJ_FLAG_SCROLL_CHAIN_VER);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_arc_color(obj, lv_color_hex(theme_colors[active_theme_index][18]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
            lv_obj_set_style_arc_width(obj, 16, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        }
        {
            // rrclimb_cat
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrclimb_cat = obj;
            lv_obj_set_pos(obj, 0, 46);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_letter_space(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "KAT. 3");
        }
        {
            // rrclimb_rem_val
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrclimb_rem_val = obj;
            lv_obj_set_pos(obj, 0, 62);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][20]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat48, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "212 m");
        }
        {
            // rrclimb_grp_total
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrclimb_grp_total = obj;
            lv_obj_set_pos(obj, 86, 112);
            lv_obj_set_size(obj, 150, 32);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrclimb_ic_total
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrclimb_ic_total = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_height);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][23]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrclimb_total_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrclimb_total_val = obj;
                    lv_obj_set_pos(obj, 34, 6);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "von 340 m");
                }
            }
        }
        {
            // rrclimb_grp_dist
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrclimb_grp_dist = obj;
            lv_obj_set_pos(obj, 250, 112);
            lv_obj_set_size(obj, 144, 32);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrclimb_ic_dist
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrclimb_ic_dist = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_ruler);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrclimb_dist_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrclimb_dist_val = obj;
                    lv_obj_set_pos(obj, 36, 6);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "2.4 km");
                }
            }
        }
        {
            // rrclimb_profile
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrclimb_profile = obj;
            lv_obj_set_pos(obj, 60, 150);
            lv_obj_set_size(obj, 360, 126);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 9, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrclimb_summit_alt
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrclimb_summit_alt = obj;
                    lv_obj_set_pos(obj, 8, 3);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Gipfel 812 m");
                }
            }
        }
        {
            // rrclimb_grp_ahead
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrclimb_grp_ahead = obj;
            lv_obj_set_pos(obj, 70, 282);
            lv_obj_set_size(obj, 160, 46);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrclimb_ahead_cap
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrclimb_ahead_cap = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_letter_space(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "NÄCHSTE 25 m");
                }
                {
                    // rrclimb_ahead_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrclimb_ahead_val = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][28]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "+6.5%");
                }
            }
        }
        {
            // rrclimb_grp_grad
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrclimb_grp_grad = obj;
            lv_obj_set_pos(obj, 250, 282);
            lv_obj_set_size(obj, 160, 46);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrclimb_grad_cap
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrclimb_grad_cap = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_letter_space(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "GEMESSEN");
                }
                {
                    // rrclimb_grad_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrclimb_grad_val = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "+5.8%");
                }
            }
        }
        {
            // rrclimb_grp_speed
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrclimb_grp_speed = obj;
            lv_obj_set_pos(obj, 96, 334);
            lv_obj_set_size(obj, 136, 32);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrclimb_speed_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrclimb_speed_val = obj;
                    lv_obj_set_pos(obj, 0, 2);
                    lv_obj_set_size(obj, 84, LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "18.4");
                }
                {
                    // rrclimb_speed_unit
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrclimb_speed_unit = obj;
                    lv_obj_set_pos(obj, 88, 12);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "km/h");
                }
            }
        }
        {
            // rrclimb_grp_heart
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrclimb_grp_heart = obj;
            lv_obj_set_pos(obj, 262, 334);
            lv_obj_set_size(obj, 110, 32);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrclimb_ic_heart
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrclimb_ic_heart = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_heart);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrclimb_hr_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrclimb_hr_val = obj;
                    lv_obj_set_pos(obj, 36, 2);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat26, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "142");
                }
            }
        }
        {
            // rrclimb_grp_info
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrclimb_grp_info = obj;
            lv_obj_set_pos(obj, 188, 370);
            lv_obj_set_size(obj, 150, 32);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrclimb_ic_info
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrclimb_ic_info = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_clock);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrclimb_info_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrclimb_info_val = obj;
                    lv_obj_set_pos(obj, 36, 4);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat22, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "14:32");
                }
            }
        }
        {
            // rrclimb_group_rq_mode
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrclimb_group_rq_mode = obj;
            lv_obj_set_pos(obj, 0, -5);
            lv_obj_set_size(obj, 197, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_go_to_rq, LV_EVENT_CLICKED, (void *)0);
            lv_obj_set_style_align(obj, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrclimb_ic_state
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rrclimb_ic_state = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_state_power);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE|LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rrclimb_line_rq
                    lv_obj_t *obj = lv_line_create(parent_obj);
                    objects.rrclimb_line_rq = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, 140, 5);
                    static lv_point_t line_points[] = {
                        { 0, 0 },
                        { 140, 0 }
                    };
                    lv_line_set_points(obj, line_points, 2);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
                    lv_obj_set_style_align(obj, LV_ALIGN_BOTTOM_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_line_width(obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_line_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                }
            }
        }
    }
    
    tick_screen_rim_ridge_climb();
}

void tick_screen_rim_ridge_climb() {
}

void create_screen_rim_ridge_wifi() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.rim_ridge_wifi = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 480);
    lv_obj_add_event_cb(obj, action_wifi_screen_gesture, LV_EVENT_GESTURE, (void *)0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // rrwifi_ic
            lv_obj_t *obj = lv_img_create(parent_obj);
            objects.rrwifi_ic = obj;
            lv_obj_set_pos(obj, 224, 30);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_img_set_src(obj, &img_rr_icon_wifi);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
        {
            // rrwifi_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrwifi_title = obj;
            lv_obj_set_pos(obj, 0, 68);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_letter_space(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "WLAN");
        }
        {
            // rrwifi_status
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrwifi_status = obj;
            lv_obj_set_pos(obj, 90, 90);
            lv_obj_set_size(obj, 300, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "verbunden");
        }
        {
            // rrwifi_list
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrwifi_list = obj;
            lv_obj_set_pos(obj, 75, 124);
            lv_obj_set_size(obj, 330, 200);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
            lv_obj_set_scroll_dir(obj, LV_DIR_VER);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 140, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
            lv_obj_set_style_width(obj, 3, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 2, LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrwifi_empty
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrwifi_empty = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Suchen antippen");
                }
            }
        }
        {
            // rrwifi_btn_scan
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrwifi_btn_scan = obj;
            lv_obj_set_pos(obj, 85, 334);
            lv_obj_set_size(obj, 150, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_wifi_scan, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_opa(obj, 40, LV_PART_MAIN | LV_STATE_DISABLED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrwifi_btn_scan_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrwifi_btn_scan_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
                    lv_label_set_text(obj, "Suchen");
                }
            }
        }
        {
            // rrwifi_btn_back
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrwifi_btn_back = obj;
            lv_obj_set_pos(obj, 245, 334);
            lv_obj_set_size(obj, 150, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_wifi_back, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_opa(obj, 40, LV_PART_MAIN | LV_STATE_DISABLED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrwifi_btn_back_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrwifi_btn_back_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
                    lv_label_set_text(obj, "Zurück");
                }
            }
        }
        {
            // rrwifi_hint
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrwifi_hint = obj;
            lv_obj_set_pos(obj, 0, 384);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "Reihenfolge: Webseite");
        }
    }
    
    tick_screen_rim_ridge_wifi();
}

void tick_screen_rim_ridge_wifi() {
}

void create_screen_rim_ridge_wifi_pw() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.rim_ridge_wifi_pw = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, 480, 480);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
    {
        lv_obj_t *parent_obj = obj;
        {
            // rrwpw_title
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrwpw_title = obj;
            lv_obj_set_pos(obj, 0, 44);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_letter_space(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "PASSWORT");
        }
        {
            // rrwpw_ssid
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrwpw_ssid = obj;
            lv_obj_set_pos(obj, 115, 64);
            lv_obj_set_size(obj, 250, LV_SIZE_CONTENT);
            lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat22, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "MeinNetzwerk");
        }
        {
            // rrwpw_ta
            lv_obj_t *obj = lv_textarea_create(parent_obj);
            objects.rrwpw_ta = obj;
            lv_obj_set_pos(obj, 90, 98);
            lv_obj_set_size(obj, 256, 40);
            lv_textarea_set_max_length(obj, 63);
            lv_textarea_set_placeholder_text(obj, "Passwort");
            lv_textarea_set_one_line(obj, true);
            lv_textarea_set_password_mode(obj, true);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_SCROLL_ON_FOCUS);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat22, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_left(obj, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_FOCUSED);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_TEXTAREA_PLACEHOLDER | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_CURSOR | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_CURSOR | LV_STATE_DEFAULT);
            lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_CURSOR | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_CURSOR | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_CURSOR | LV_STATE_FOCUSED);
            lv_obj_set_style_border_width(obj, 2, LV_PART_CURSOR | LV_STATE_FOCUSED);
            lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR | LV_STATE_FOCUSED);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_CURSOR | LV_STATE_FOCUSED);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_CURSOR | LV_STATE_FOCUSED);
        }
        {
            // rrwpw_btn_eye
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrwpw_btn_eye = obj;
            lv_obj_set_pos(obj, 352, 98);
            lv_obj_set_size(obj, 40, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_wifi_pw_eye, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_opa(obj, 40, LV_PART_MAIN | LV_STATE_DISABLED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrwpw_btn_eye_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrwpw_btn_eye_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
                    lv_label_set_text(obj, "abc");
                }
            }
        }
        {
            // rrwpw_kb
            lv_obj_t *obj = lv_keyboard_create(parent_obj);
            objects.rrwpw_kb = obj;
            lv_obj_set_pos(obj, 0, 10);
            lv_obj_set_size(obj, 470, 194);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICK_FOCUSABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_min_width(obj, 400, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_max_width(obj, 460, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_min_height(obj, 220, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_max_height(obj, 220, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_row(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_column(obj, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 8, LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 1, LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &lv_font_montserrat_22, LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_shadow_width(obj, 0, LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_ITEMS | LV_STATE_PRESSED);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_ITEMS | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_ITEMS | LV_STATE_CHECKED);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_ITEMS | LV_STATE_CHECKED);
        }
        {
            // rrwpw_btn_cancel
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrwpw_btn_cancel = obj;
            lv_obj_set_pos(obj, 95, 367);
            lv_obj_set_size(obj, 135, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_wifi_pw_cancel, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_opa(obj, 40, LV_PART_MAIN | LV_STATE_DISABLED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrwpw_btn_cancel_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrwpw_btn_cancel_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
                    lv_label_set_text(obj, "Abbrechen");
                }
            }
        }
        {
            // rrwpw_btn_save
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrwpw_btn_save = obj;
            lv_obj_set_pos(obj, 250, 367);
            lv_obj_set_size(obj, 135, 40);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_add_event_cb(obj, action_wifi_pw_save, LV_EVENT_CLICKED, (void *)0);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 90, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_opa(obj, 255, LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
            lv_obj_set_style_border_opa(obj, 40, LV_PART_MAIN | LV_STATE_DISABLED);
            {
                lv_obj_t *parent_obj = obj;
                {
                    // rrwpw_btn_save_lbl
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrwpw_btn_save_lbl = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_montserrat18, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
                    lv_label_set_text(obj, "Speichern");
                }
            }
        }
        {
            // rrwpw_hint
            lv_obj_t *obj = lv_label_create(parent_obj);
            objects.rrwpw_hint = obj;
            lv_obj_set_pos(obj, 0, 409);
            lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(obj, &ui_font_montserrat14, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_TOP_MID, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(obj, "mind. 8 Zeichen");
        }
    }
    lv_keyboard_set_textarea(objects.rrwpw_kb, objects.rrwpw_ta);
    
    tick_screen_rim_ridge_wifi_pw();
}

void tick_screen_rim_ridge_wifi_pw() {
}

typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_rim_ridge,
    tick_screen_rim_ridge_nav,
    tick_screen_rim_ridge_rq,
    tick_screen_rim_ridge_settings,
    tick_screen_rim_ridge_climb,
    tick_screen_rim_ridge_wifi,
    tick_screen_rim_ridge_wifi_pw,
};
void tick_screen(int screen_index) {
    if (screen_index >= 0 && screen_index < 7) {
        tick_screen_funcs[screen_index]();
    }
}
void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen(screenId - 1);
}

//
// Fonts
//

ext_font_desc_t fonts[] = {
    { "by7x128", &ui_font_by7x128 },
    { "FontSchild22", &ui_font_font_schild22 },
    { "Montserrat14", &ui_font_montserrat14 },
    { "Montserrat18", &ui_font_montserrat18 },
    { "Montserrat22", &ui_font_montserrat22 },
    { "Montserrat26", &ui_font_montserrat26 },
    { "Montserrat48", &ui_font_montserrat48 },
#if LV_FONT_MONTSERRAT_8
    { "MONTSERRAT_8", &lv_font_montserrat_8 },
#endif
#if LV_FONT_MONTSERRAT_10
    { "MONTSERRAT_10", &lv_font_montserrat_10 },
#endif
#if LV_FONT_MONTSERRAT_12
    { "MONTSERRAT_12", &lv_font_montserrat_12 },
#endif
#if LV_FONT_MONTSERRAT_14
    { "MONTSERRAT_14", &lv_font_montserrat_14 },
#endif
#if LV_FONT_MONTSERRAT_16
    { "MONTSERRAT_16", &lv_font_montserrat_16 },
#endif
#if LV_FONT_MONTSERRAT_18
    { "MONTSERRAT_18", &lv_font_montserrat_18 },
#endif
#if LV_FONT_MONTSERRAT_20
    { "MONTSERRAT_20", &lv_font_montserrat_20 },
#endif
#if LV_FONT_MONTSERRAT_22
    { "MONTSERRAT_22", &lv_font_montserrat_22 },
#endif
#if LV_FONT_MONTSERRAT_24
    { "MONTSERRAT_24", &lv_font_montserrat_24 },
#endif
#if LV_FONT_MONTSERRAT_26
    { "MONTSERRAT_26", &lv_font_montserrat_26 },
#endif
#if LV_FONT_MONTSERRAT_28
    { "MONTSERRAT_28", &lv_font_montserrat_28 },
#endif
#if LV_FONT_MONTSERRAT_30
    { "MONTSERRAT_30", &lv_font_montserrat_30 },
#endif
#if LV_FONT_MONTSERRAT_32
    { "MONTSERRAT_32", &lv_font_montserrat_32 },
#endif
#if LV_FONT_MONTSERRAT_34
    { "MONTSERRAT_34", &lv_font_montserrat_34 },
#endif
#if LV_FONT_MONTSERRAT_36
    { "MONTSERRAT_36", &lv_font_montserrat_36 },
#endif
#if LV_FONT_MONTSERRAT_38
    { "MONTSERRAT_38", &lv_font_montserrat_38 },
#endif
#if LV_FONT_MONTSERRAT_40
    { "MONTSERRAT_40", &lv_font_montserrat_40 },
#endif
#if LV_FONT_MONTSERRAT_42
    { "MONTSERRAT_42", &lv_font_montserrat_42 },
#endif
#if LV_FONT_MONTSERRAT_44
    { "MONTSERRAT_44", &lv_font_montserrat_44 },
#endif
#if LV_FONT_MONTSERRAT_46
    { "MONTSERRAT_46", &lv_font_montserrat_46 },
#endif
#if LV_FONT_MONTSERRAT_48
    { "MONTSERRAT_48", &lv_font_montserrat_48 },
#endif
};

//
// Color themes
//

uint32_t active_theme_index = 0;
void change_color_theme(uint32_t theme_index) {
    active_theme_index = theme_index;
    
    {
        lv_obj_set_style_bg_color(objects.rim_ridge, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(objects.rr_speed_arc, lv_color_hex(theme_colors[theme_index][18]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(objects.rr_speed_arc, lv_color_hex(theme_colors[theme_index][19]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_wifi, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_gps, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rr_nav_pill, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rr_nav_pill, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_nav_dist, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_turn, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rr_btn_pause, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rr_btn_pause, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_pause, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_distance_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_tour_label, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rr_tour_label, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rr_btn_settings, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rr_btn_settings, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_settings, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_lane_row, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_cadence, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_cadence_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_cadence_unit, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_power, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_power_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_power_unit, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_speed_val, lv_color_hex(theme_colors[theme_index][20]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_speed_unit, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_temp, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_temp_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_height, lv_color_hex(theme_colors[theme_index][23]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_height_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_gradient, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_gradient_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_heart, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_hr_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rr_hr_bar, lv_color_hex(theme_colors[theme_index][26]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(objects.rr_hr_bar, lv_color_hex(theme_colors[theme_index][30]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rr_hr_bar, lv_color_hex(theme_colors[theme_index][26]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(objects.rr_hr_bar, lv_color_hex(theme_colors[theme_index][30]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_time, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rr_time_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_battery, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rr_battery_fill, lv_color_hex(theme_colors[theme_index][23]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rr_ic_state, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_color(objects.rr_line_rq, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.rim_ridge_nav, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(objects.rrnav_dist_arc, lv_color_hex(theme_colors[theme_index][18]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(objects.rrnav_dist_arc, lv_color_hex(theme_colors[theme_index][19]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrnav_lane_row, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_street, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrnav_ic_turn, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_dist_val, lv_color_hex(theme_colors[theme_index][20]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrnav_ic_next, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_next_dist, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_speed_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_speed_unit, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrnav_ic_gradient, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_gradient_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrnav_ic_heart, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_hr_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrnav_ic_state, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_color(objects.rrnav_line_rq, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.rim_ridge_rq, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_nav_pill, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_nav_pill, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rq_ic_turn, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_nav_dist, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_speed_val, lv_color_hex(theme_colors[theme_index][20]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_speed_unit, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rq_ic_heart, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_hr_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rq_ic_dist, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_dist_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_rq_val, lv_color_hex(theme_colors[theme_index][28]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_rq_caption, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_surf_caption, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_surf_asphalt, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_surf_asphalt, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_surf_asphalt_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_surf_schotter, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_surf_schotter, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_surf_schotter_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_surf_waldweg, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_surf_waldweg, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_surf_waldweg_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_surf_feldweg, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_surf_feldweg, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_surf_feldweg_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_surf_pflaster, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_surf_pflaster, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_surf_pflaster_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_surf_sonstiges, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_surf_sonstiges, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_surf_sonstiges_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_qual_caption, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_qual_track, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_qual_1, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_qual_1, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_qual_1_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_qual_2, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_qual_2, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_qual_2_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_qual_3, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_qual_3, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_qual_3_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_qual_4, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_qual_4, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rq_qual_4_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_btn_record, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_btn_record, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rq_btn_record_ring, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rq_btn_record_dot, lv_color_hex(theme_colors[theme_index][30]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rq_ic_state, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_color(objects.rq_line_rq, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.rim_ridge_settings, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrset_ic_gear, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_title, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_build_caption, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_build_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_ip_caption, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_ip_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrset_btn_wifi, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrset_btn_wifi, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrset_btn_wifi, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(objects.rrset_btn_wifi_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_btn_wifi_lbl, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(objects.rrset_btn_ap, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrset_btn_ap, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrset_btn_ap, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(objects.rrset_btn_ap_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_btn_ap_lbl, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(objects.rrset_btn_wifisetup, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrset_btn_wifisetup, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrset_btn_wifisetup, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(objects.rrset_btn_wifisetup_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_btn_wifisetup_lbl, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(objects.rrset_btn_cal, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrset_btn_cal, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrset_btn_cal, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(objects.rrset_btn_cal_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_btn_cal_lbl, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(objects.rrset_btn_ref, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrset_btn_ref, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrset_btn_ref, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(objects.rrset_btn_ref_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_btn_ref_lbl, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_text_color(objects.rrset_cal_status, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_ref_status, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrset_btn_reset, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrset_btn_reset, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrset_btn_reset, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_img_recolor(objects.rrset_ic_reset, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_btn_reset_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrset_btn_sleep, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrset_btn_sleep, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrset_btn_sleep, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_img_recolor(objects.rrset_ic_sleep, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_btn_sleep_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrset_hint, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrset_ic_state, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_color(objects.rrset_line_rq, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.rim_ridge_climb, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(objects.rrclimb_arc, lv_color_hex(theme_colors[theme_index][18]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(objects.rrclimb_arc, lv_color_hex(theme_colors[theme_index][19]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_cat, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_rem_val, lv_color_hex(theme_colors[theme_index][20]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrclimb_ic_total, lv_color_hex(theme_colors[theme_index][23]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_total_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrclimb_ic_dist, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_dist_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrclimb_profile, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrclimb_profile, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_summit_alt, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_ahead_cap, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_ahead_val, lv_color_hex(theme_colors[theme_index][28]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_grad_cap, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_grad_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_speed_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_speed_unit, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrclimb_ic_heart, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_hr_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrclimb_ic_info, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrclimb_info_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrclimb_ic_state, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_line_color(objects.rrclimb_line_rq, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.rim_ridge_wifi, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrwifi_ic, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwifi_title, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwifi_status, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrwifi_list, lv_color_hex(theme_colors[theme_index][19]), LV_PART_SCROLLBAR | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwifi_empty, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrwifi_btn_scan, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrwifi_btn_scan, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrwifi_btn_scan, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(objects.rrwifi_btn_scan_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwifi_btn_scan_lbl, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(objects.rrwifi_btn_back, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrwifi_btn_back, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrwifi_btn_back, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(objects.rrwifi_btn_back_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwifi_btn_back_lbl, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_text_color(objects.rrwifi_hint, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    {
        lv_obj_set_style_bg_color(objects.rim_ridge_wifi_pw, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwpw_title, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwpw_ssid, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrwpw_ta, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrwpw_ta, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwpw_ta, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwpw_ta, lv_color_hex(theme_colors[theme_index][22]), LV_PART_TEXTAREA_PLACEHOLDER | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrwpw_ta, lv_color_hex(theme_colors[theme_index][19]), LV_PART_CURSOR | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrwpw_ta, lv_color_hex(theme_colors[theme_index][19]), LV_PART_CURSOR | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(objects.rrwpw_btn_eye, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrwpw_btn_eye, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrwpw_btn_eye, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(objects.rrwpw_btn_eye_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwpw_btn_eye_lbl, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(objects.rrwpw_kb, lv_color_hex(theme_colors[theme_index][24]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrwpw_kb, lv_color_hex(theme_colors[theme_index][19]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwpw_kb, lv_color_hex(theme_colors[theme_index][21]), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrwpw_kb, lv_color_hex(theme_colors[theme_index][25]), LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(objects.rrwpw_kb, lv_color_hex(theme_colors[theme_index][25]), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(objects.rrwpw_kb, lv_color_hex(theme_colors[theme_index][19]), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(objects.rrwpw_btn_cancel, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrwpw_btn_cancel, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrwpw_btn_cancel, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(objects.rrwpw_btn_cancel_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwpw_btn_cancel_lbl, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(objects.rrwpw_btn_save, lv_color_hex(theme_colors[theme_index][24]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(objects.rrwpw_btn_save, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(objects.rrwpw_btn_save, lv_color_hex(theme_colors[theme_index][25]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(objects.rrwpw_btn_save_lbl, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrwpw_btn_save_lbl, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DISABLED);
        lv_obj_set_style_text_color(objects.rrwpw_hint, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_invalidate(objects.rim_ridge);
    lv_obj_invalidate(objects.rim_ridge_nav);
    lv_obj_invalidate(objects.rim_ridge_rq);
    lv_obj_invalidate(objects.rim_ridge_settings);
    lv_obj_invalidate(objects.rim_ridge_climb);
    lv_obj_invalidate(objects.rim_ridge_wifi);
    lv_obj_invalidate(objects.rim_ridge_wifi_pw);
}
uint32_t theme_colors[1][31] = {
    { 0xff00ff80, 0xff000000, 0xffc8fff7, 0xffe0e0e0, 0xff00ff26, 0xffe5deae, 0xffffe200, 0xffff3a00, 0xff005766, 0xff3fff00, 0xffff0000, 0xfffdffde, 0xff00ff00, 0xff85ebff, 0xff0000ff, 0xffffffff, 0xff161b1f, 0xff3a362e, 0xff332f28, 0xffcba36b, 0xfff3ecdf, 0xffe7e2d6, 0xff9ba097, 0xff7fa08f, 0xff1e252b, 0xff282019, 0xff6c90b0, 0xff6fa98c, 0xffd7b463, 0xffce8a4c, 0xffc1604a },
};

//
//
//

void create_screens() {

// Set default LVGL theme
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp, lv_palette_main(LV_PALETTE_BLUE), lv_palette_main(LV_PALETTE_RED), false, LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);
    
    // Initialize screens
    // Create screens
    create_screen_rim_ridge();
    create_screen_rim_ridge_nav();
    create_screen_rim_ridge_rq();
    create_screen_rim_ridge_settings();
    create_screen_rim_ridge_climb();
    create_screen_rim_ridge_wifi();
    create_screen_rim_ridge_wifi_pw();
}