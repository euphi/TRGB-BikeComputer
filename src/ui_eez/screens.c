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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
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
            lv_obj_set_pos(obj, 173, 383);
            lv_obj_set_size(obj, 134, 92);
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
                    // rr_ic_state
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_state = obj;
                    lv_obj_set_pos(obj, 51, 58);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_state_power);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_26, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_18, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "14°");
                }
            }
        }
        {
            // rr_grp_height
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rr_grp_height = obj;
            lv_obj_set_pos(obj, 192, 309);
            lv_obj_set_size(obj, 90, 32);
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
                    // rr_ic_height
                    lv_obj_t *obj = lv_img_create(parent_obj);
                    objects.rr_ic_height = obj;
                    lv_obj_set_pos(obj, 0, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_img_set_src(obj, &img_rr_icon_height);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_ADV_HITTEST|LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_img_recolor(obj, lv_color_hex(theme_colors[active_theme_index][23]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_img_recolor_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                }
                {
                    // rr_height_val
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rr_height_val = obj;
                    lv_obj_set_pos(obj, 32, 7);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "612m");
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
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
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
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_20, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_48, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "180 m");
                }
                {
                    // rrnav_street
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.rrnav_street = obj;
                    lv_obj_set_pos(obj, 0, 113);
                    lv_obj_set_size(obj, LV_PCT(90), LV_SIZE_CONTENT);
                    lv_label_set_long_mode(obj, LV_LABEL_LONG_SCROLL_CIRCULAR);
                    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
                    lv_obj_set_style_text_color(obj, lv_color_hex(theme_colors[active_theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_font(obj, &ui_font_font_schild22, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "Feldweg");
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "60 m");
                }
            }
        }
        {
            // rrnav_grp_speed
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrnav_grp_speed = obj;
            lv_obj_set_pos(obj, 152, 332);
            lv_obj_set_size(obj, 86, 55);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_40, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "+4.2%");
                }
            }
        }
        {
            // rrnav_grp_heart
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.rrnav_grp_heart = obj;
            lv_obj_set_pos(obj, 204, 392);
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
                    lv_obj_set_style_text_font(obj, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, "142");
                }
            }
        }
    }
    
    tick_screen_rim_ridge_nav();
}

void tick_screen_rim_ridge_nav() {
}

typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_rim_ridge,
    tick_screen_rim_ridge_nav,
};
void tick_screen(int screen_index) {
    if (screen_index >= 0 && screen_index < 2) {
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
        lv_obj_set_style_img_recolor(objects.rr_ic_state, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
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
    }
    {
        lv_obj_set_style_bg_color(objects.rim_ridge_nav, lv_color_hex(theme_colors[theme_index][16]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(objects.rrnav_dist_arc, lv_color_hex(theme_colors[theme_index][18]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_arc_color(objects.rrnav_dist_arc, lv_color_hex(theme_colors[theme_index][19]), LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrnav_lane_row, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrnav_ic_turn, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_dist_val, lv_color_hex(theme_colors[theme_index][20]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_street, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrnav_ic_next, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_next_dist, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_speed_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_speed_unit, lv_color_hex(theme_colors[theme_index][22]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrnav_ic_gradient, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_gradient_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_img_recolor(objects.rrnav_ic_heart, lv_color_hex(theme_colors[theme_index][19]), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(objects.rrnav_hr_val, lv_color_hex(theme_colors[theme_index][21]), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_invalidate(objects.rim_ridge);
    lv_obj_invalidate(objects.rim_ridge_nav);
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
}