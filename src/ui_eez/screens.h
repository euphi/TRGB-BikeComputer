#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN_NO_FL = 1,
    SCREEN_ID_RIM_RIDGE = 2,
    SCREEN_ID_BOOT_LOGO = 3,
    _SCREEN_ID_LAST = 3
};

typedef struct _objects_t {
    lv_obj_t *main_no_fl;
    lv_obj_t *rim_ridge;
    lv_obj_t *boot_logo;
    lv_obj_t *panel_nav;
    lv_obj_t *img_nav;
    lv_obj_t *bar_nav;
    lv_obj_t *label_nav_dist;
    lv_obj_t *arc_speed;
    lv_obj_t *arc_avg;
    lv_obj_t *arc_cad;
    lv_obj_t *label_speed;
    lv_obj_t *bar_hr;
    lv_obj_t *label_dist;
    lv_obj_t *panel_clock;
    lv_obj_t *label_clock;
    lv_obj_t *label_clock_mode;
    lv_obj_t *label_cad;
    lv_obj_t *bar_batt;
    lv_obj_t *img_wifi;
    lv_obj_t *img_settings;
    lv_obj_t *label_spd_avg;
    lv_obj_t *cont_height;
    lv_obj_t *label_gradient;
    lv_obj_t *label_height;
    lv_obj_t *label_temp;
    lv_obj_t *img_state;
    lv_obj_t *rr_speed_arc;
    lv_obj_t *rr_ic_wifi;
    lv_obj_t *rr_ic_gps;
    lv_obj_t *rr_ic_battery;
    lv_obj_t *rr_nav_pill;
    lv_obj_t *rr_nav_dist;
    lv_obj_t *rr_ic_turn;
    lv_obj_t *rr_ic_cadence;
    lv_obj_t *rr_cadence_val;
    lv_obj_t *rr_cadence_unit;
    lv_obj_t *rr_ic_power;
    lv_obj_t *rr_power_val;
    lv_obj_t *rr_power_unit;
    lv_obj_t *rr_speed_val;
    lv_obj_t *rr_speed_unit;
    lv_obj_t *rr_ic_temp;
    lv_obj_t *rr_temp_val;
    lv_obj_t *rr_ic_height;
    lv_obj_t *rr_height_val;
    lv_obj_t *rr_ic_gradient;
    lv_obj_t *rr_gradient_val;
    lv_obj_t *rr_ic_heart;
    lv_obj_t *rr_hr_val;
    lv_obj_t *rr_hr_bar;
    lv_obj_t *rr_btn_pause;
    lv_obj_t *rr_ic_pause;
    lv_obj_t *rr_tour_pill;
    lv_obj_t *rr_distance_val;
    lv_obj_t *rr_tour_label;
    lv_obj_t *rr_btn_settings;
    lv_obj_t *rr_ic_settings;
    lv_obj_t *rr_battery_fill;
    lv_obj_t *img_bootlogo;
} objects_t;

extern objects_t objects;

void create_screen_main_no_fl();
void tick_screen_main_no_fl();

void create_screen_rim_ridge();
void tick_screen_rim_ridge();

void create_screen_boot_logo();
void tick_screen_boot_logo();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

// Color themes

enum Themes {
    THEME_ID_DEFAULT,
};
enum Colors {
    COLOR_ID_NAV_ICON_RECOLOR,
    COLOR_ID_TEXT_PRIMARY,
    COLOR_ID_PANEL_NAV_BG,
    COLOR_ID_ARC_SPEED_TRACK,
    COLOR_ID_ARC_SPEED_KNOB,
    COLOR_ID_ARC_AVG_TRACK,
    COLOR_ID_ARC_AVG_INDICATOR,
    COLOR_ID_ARC_AVG_KNOB,
    COLOR_ID_ARC_CAD_COLOR,
    COLOR_ID_GRADIENT_GOOD_GREEN,
    COLOR_ID_GRADIENT_DANGER_RED,
    COLOR_ID_PANEL_CLOCK_BG,
    COLOR_ID_GRADIENT_FULL_GREEN,
    COLOR_ID_HEIGHT_PANEL_BG,
    COLOR_ID_STATE_ICON_RECOLOR,
    COLOR_ID_SCREEN_BG,
    COLOR_ID_RR_BACKGROUND,
    COLOR_ID_RR_RIM_OUTLINE,
    COLOR_ID_RR_ARC_TRACK,
    COLOR_ID_RR_BRASS,
    COLOR_ID_RR_PARCHMENT_BRIGHT,
    COLOR_ID_RR_PARCHMENT,
    COLOR_ID_RR_MUTED,
    COLOR_ID_RR_SAGE,
    COLOR_ID_RR_PANEL_BG,
    COLOR_ID_RR_TOUR_BG,
    COLOR_ID_RR_ZONE_BLUE,
    COLOR_ID_RR_ZONE_GREEN,
    COLOR_ID_RR_ZONE_YELLOW,
    COLOR_ID_RR_ZONE_ORANGE,
    COLOR_ID_RR_ZONE_RED,
};
void change_color_theme(uint32_t themeIndex);
extern uint32_t theme_colors[1][31];
extern uint32_t active_theme_index;

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/