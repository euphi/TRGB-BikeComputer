#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_RIM_RIDGE = 1,
    SCREEN_ID_RIM_RIDGE_NAV = 2,
    SCREEN_ID_RIM_RIDGE_RQ = 3,
    SCREEN_ID_RIM_RIDGE_SETTINGS = 4,
    SCREEN_ID_RIM_RIDGE_CLIMB = 5,
    SCREEN_ID_RIM_RIDGE_WIFI = 6,
    SCREEN_ID_RIM_RIDGE_WIFI_PW = 7,
    SCREEN_ID_RIM_RIDGE_SETTINGS_WIFI = 8,
    SCREEN_ID_RIM_RIDGE_SETTINGS_IMU = 9,
    SCREEN_ID_RIM_RIDGE_SETTINGS_ALT = 10,
    SCREEN_ID_RIM_RIDGE_SETTINGS_NUM = 11,
    SCREEN_ID_RIM_RIDGE_SETTINGS_DEV = 12,
    SCREEN_ID_RIM_RIDGE_ROUTE = 13,
    _SCREEN_ID_LAST = 13
};

typedef struct _objects_t {
    lv_obj_t *rim_ridge;
    lv_obj_t *rim_ridge_nav;
    lv_obj_t *rim_ridge_rq;
    lv_obj_t *rim_ridge_settings;
    lv_obj_t *rim_ridge_climb;
    lv_obj_t *rim_ridge_wifi;
    lv_obj_t *rim_ridge_wifi_pw;
    lv_obj_t *rim_ridge_settings_wifi;
    lv_obj_t *rim_ridge_settings_imu;
    lv_obj_t *rim_ridge_settings_alt;
    lv_obj_t *rim_ridge_settings_num;
    lv_obj_t *rim_ridge_settings_dev;
    lv_obj_t *rim_ridge_route;
    lv_obj_t *rr_speed_arc;
    lv_obj_t *rr_ic_wifi;
    lv_obj_t *rr_ic_gps;
    lv_obj_t *rr_nav_pill;
    lv_obj_t *rr_nav_dist;
    lv_obj_t *rr_ic_turn;
    lv_obj_t *rr_btn_pause;
    lv_obj_t *rr_ic_pause;
    lv_obj_t *rr_tour_pill;
    lv_obj_t *rr_distance_val;
    lv_obj_t *rr_tour_label;
    lv_obj_t *rr_btn_settings;
    lv_obj_t *rr_ic_settings;
    lv_obj_t *rr_lane_row;
    lv_obj_t *rr_grp_cadence;
    lv_obj_t *rr_ic_cadence;
    lv_obj_t *rr_cadence_val;
    lv_obj_t *rr_cadence_unit;
    lv_obj_t *rr_grp_power;
    lv_obj_t *rr_ic_power;
    lv_obj_t *rr_power_val;
    lv_obj_t *rr_power_unit;
    lv_obj_t *rr_grp_speed;
    lv_obj_t *rr_speed_val;
    lv_obj_t *rr_speed_unit;
    lv_obj_t *rr_grp_temp;
    lv_obj_t *rr_ic_temp;
    lv_obj_t *rr_temp_val;
    lv_obj_t *rr_grp_height;
    lv_obj_t *rr_ic_height;
    lv_obj_t *rr_height_val;
    lv_obj_t *rr_grp_gradient;
    lv_obj_t *rr_ic_gradient;
    lv_obj_t *rr_gradient_val;
    lv_obj_t *rr_grp_heart;
    lv_obj_t *rr_ic_heart;
    lv_obj_t *rr_hr_val;
    lv_obj_t *rr_hr_bar;
    lv_obj_t *rr_grp_time;
    lv_obj_t *rr_ic_time;
    lv_obj_t *rr_time_val;
    lv_obj_t *rr_grp_battery;
    lv_obj_t *rr_ic_battery;
    lv_obj_t *rr_battery_fill;
    lv_obj_t *rr_group_rq_mode;
    lv_obj_t *rr_ic_state;
    lv_obj_t *rr_line_rq;
    lv_obj_t *rrnav_dist_arc;
    lv_obj_t *rrnav_lane_row;
    lv_obj_t *rrnav_street;
    lv_obj_t *rrnav_grp_turn;
    lv_obj_t *rrnav_ic_turn;
    lv_obj_t *rrnav_dist_val;
    lv_obj_t *rrnav_grp_next;
    lv_obj_t *rrnav_ic_next;
    lv_obj_t *rrnav_next_dist;
    lv_obj_t *rrnav_grp_speed;
    lv_obj_t *rrnav_speed_val;
    lv_obj_t *rrnav_speed_unit;
    lv_obj_t *rrnav_grp_gradient;
    lv_obj_t *rrnav_ic_gradient;
    lv_obj_t *rrnav_gradient_val;
    lv_obj_t *rrnav_grp_heart;
    lv_obj_t *rrnav_ic_heart;
    lv_obj_t *rrnav_hr_val;
    lv_obj_t *rrnav_group_rq_mode;
    lv_obj_t *rrnav_ic_state;
    lv_obj_t *rrnav_line_rq;
    lv_obj_t *rq_nav_pill;
    lv_obj_t *rq_ic_turn;
    lv_obj_t *rq_nav_dist;
    lv_obj_t *rq_speed_val;
    lv_obj_t *rq_speed_unit;
    lv_obj_t *rq_ic_heart;
    lv_obj_t *rq_hr_val;
    lv_obj_t *rq_ic_dist;
    lv_obj_t *rq_dist_val;
    lv_obj_t *rq_rq_val;
    lv_obj_t *rq_rq_caption;
    lv_obj_t *rq_surf_caption;
    lv_obj_t *rq_surf_asphalt;
    lv_obj_t *rq_surf_asphalt_lbl;
    lv_obj_t *rq_surf_schotter;
    lv_obj_t *rq_surf_schotter_lbl;
    lv_obj_t *rq_surf_waldweg;
    lv_obj_t *rq_surf_waldweg_lbl;
    lv_obj_t *rq_surf_feldweg;
    lv_obj_t *rq_surf_feldweg_lbl;
    lv_obj_t *rq_surf_pflaster;
    lv_obj_t *rq_surf_pflaster_lbl;
    lv_obj_t *rq_surf_sonstiges;
    lv_obj_t *rq_surf_sonstiges_lbl;
    lv_obj_t *rq_qual_caption;
    lv_obj_t *rq_qual_track;
    lv_obj_t *rq_qual_1;
    lv_obj_t *rq_qual_1_lbl;
    lv_obj_t *rq_qual_2;
    lv_obj_t *rq_qual_2_lbl;
    lv_obj_t *rq_qual_3;
    lv_obj_t *rq_qual_3_lbl;
    lv_obj_t *rq_qual_4;
    lv_obj_t *rq_qual_4_lbl;
    lv_obj_t *rq_btn_record;
    lv_obj_t *rq_btn_record_ring;
    lv_obj_t *rq_btn_record_dot;
    lv_obj_t *rq_group_rq_mode;
    lv_obj_t *rq_ic_state;
    lv_obj_t *rq_line_rq;
    lv_obj_t *rrset_ic_gear;
    lv_obj_t *rrset_title;
    lv_obj_t *rrset_build_caption;
    lv_obj_t *rrset_build_val;
    lv_obj_t *rrset_build_flag;
    lv_obj_t *rrset_nav_wifi;
    lv_obj_t *rrset_nav_wifi_ic;
    lv_obj_t *rrset_nav_wifi_lbl;
    lv_obj_t *rrset_nav_wifi_val;
    lv_obj_t *rrset_nav_dev;
    lv_obj_t *rrset_nav_dev_ic;
    lv_obj_t *rrset_nav_dev_lbl;
    lv_obj_t *rrset_nav_dev_val;
    lv_obj_t *rrset_nav_imu;
    lv_obj_t *rrset_nav_imu_ic;
    lv_obj_t *rrset_nav_imu_lbl;
    lv_obj_t *rrset_nav_imu_val;
    lv_obj_t *rrset_nav_alt;
    lv_obj_t *rrset_nav_alt_ic;
    lv_obj_t *rrset_nav_alt_lbl;
    lv_obj_t *rrset_nav_alt_val;
    lv_obj_t *rrset_btn_reset;
    lv_obj_t *rrset_ic_reset;
    lv_obj_t *rrset_btn_reset_lbl;
    lv_obj_t *rrset_btn_sleep;
    lv_obj_t *rrset_ic_sleep;
    lv_obj_t *rrset_btn_sleep_lbl;
    lv_obj_t *rrset_hint;
    lv_obj_t *rrset_group_rq_mode;
    lv_obj_t *rrset_ic_state;
    lv_obj_t *rrset_line_rq;
    lv_obj_t *rrclimb_arc;
    lv_obj_t *rrclimb_cat;
    lv_obj_t *rrclimb_rem_val;
    lv_obj_t *rrclimb_grp_total;
    lv_obj_t *rrclimb_ic_total;
    lv_obj_t *rrclimb_total_val;
    lv_obj_t *rrclimb_grp_dist;
    lv_obj_t *rrclimb_ic_dist;
    lv_obj_t *rrclimb_dist_val;
    lv_obj_t *rrclimb_profile;
    lv_obj_t *rrclimb_summit_alt;
    lv_obj_t *rrclimb_grp_ahead;
    lv_obj_t *rrclimb_ahead_cap;
    lv_obj_t *rrclimb_ahead_val;
    lv_obj_t *rrclimb_grp_grad;
    lv_obj_t *rrclimb_grad_cap;
    lv_obj_t *rrclimb_grad_val;
    lv_obj_t *rrclimb_grp_speed;
    lv_obj_t *rrclimb_speed_val;
    lv_obj_t *rrclimb_speed_unit;
    lv_obj_t *rrclimb_grp_heart;
    lv_obj_t *rrclimb_ic_heart;
    lv_obj_t *rrclimb_hr_val;
    lv_obj_t *rrclimb_grp_info;
    lv_obj_t *rrclimb_ic_info;
    lv_obj_t *rrclimb_info_val;
    lv_obj_t *rrclimb_group_rq_mode;
    lv_obj_t *rrclimb_ic_state;
    lv_obj_t *rrclimb_line_rq;
    lv_obj_t *rrwifi_ic;
    lv_obj_t *rrwifi_title;
    lv_obj_t *rrwifi_status;
    lv_obj_t *rrwifi_list;
    lv_obj_t *rrwifi_empty;
    lv_obj_t *rrwifi_btn_scan;
    lv_obj_t *rrwifi_btn_scan_lbl;
    lv_obj_t *rrwifi_btn_back;
    lv_obj_t *rrwifi_btn_back_lbl;
    lv_obj_t *rrwifi_hint;
    lv_obj_t *rrwpw_title;
    lv_obj_t *rrwpw_ssid;
    lv_obj_t *rrwpw_ta;
    lv_obj_t *rrwpw_btn_eye;
    lv_obj_t *rrwpw_btn_eye_lbl;
    lv_obj_t *rrwpw_kb;
    lv_obj_t *rrwpw_btn_cancel;
    lv_obj_t *rrwpw_btn_cancel_lbl;
    lv_obj_t *rrwpw_btn_save;
    lv_obj_t *rrwpw_btn_save_lbl;
    lv_obj_t *rrwpw_hint;
    lv_obj_t *rrsw_ic;
    lv_obj_t *rrsw_title;
    lv_obj_t *rrset_ip_caption;
    lv_obj_t *rrset_ip_val;
    lv_obj_t *rrset_btn_wifi;
    lv_obj_t *rrset_btn_wifi_lbl;
    lv_obj_t *rrset_btn_ap;
    lv_obj_t *rrset_btn_ap_lbl;
    lv_obj_t *rrset_btn_wifisetup;
    lv_obj_t *rrset_btn_wifisetup_lbl;
    lv_obj_t *rrsw_btn_back;
    lv_obj_t *rrsw_btn_back_lbl;
    lv_obj_t *rrsi_ic;
    lv_obj_t *rrsi_title;
    lv_obj_t *rrset_btn_cal;
    lv_obj_t *rrset_btn_cal_lbl;
    lv_obj_t *rrset_cal_status;
    lv_obj_t *rrset_btn_ref;
    lv_obj_t *rrset_btn_ref_lbl;
    lv_obj_t *rrset_ref_status;
    lv_obj_t *rrsi_btn_back;
    lv_obj_t *rrsi_btn_back_lbl;
    lv_obj_t *rrsa_ic;
    lv_obj_t *rrsa_title;
    lv_obj_t *rrsa_height;
    lv_obj_t *rrsa_info;
    lv_obj_t *rrsa_btn_p1;
    lv_obj_t *rrsa_btn_p1_lbl;
    lv_obj_t *rrsa_btn_p2;
    lv_obj_t *rrsa_btn_p2_lbl;
    lv_obj_t *rrsa_btn_p3;
    lv_obj_t *rrsa_btn_p3_lbl;
    lv_obj_t *rrsa_btn_gps;
    lv_obj_t *rrsa_btn_gps_lbl;
    lv_obj_t *rrsa_btn_manual;
    lv_obj_t *rrsa_btn_manual_lbl;
    lv_obj_t *rrsa_status;
    lv_obj_t *rrsa_hint;
    lv_obj_t *rrsa_btn_back;
    lv_obj_t *rrsa_btn_back_lbl;
    lv_obj_t *rrsn_title;
    lv_obj_t *rrsn_btn_mode_h;
    lv_obj_t *rrsn_btn_mode_h_lbl;
    lv_obj_t *rrsn_btn_mode_p;
    lv_obj_t *rrsn_btn_mode_p_lbl;
    lv_obj_t *rrsn_ta;
    lv_obj_t *rrsn_unit;
    lv_obj_t *rrsn_kb;
    lv_obj_t *rrsn_btn_cancel;
    lv_obj_t *rrsn_btn_cancel_lbl;
    lv_obj_t *rrsn_btn_save;
    lv_obj_t *rrsn_btn_save_lbl;
    lv_obj_t *rrsn_hint;
    lv_obj_t *rrsd_ic;
    lv_obj_t *rrsd_title;
    lv_obj_t *rrsd_row_csc1;
    lv_obj_t *rrsd_row_csc1_dot;
    lv_obj_t *rrsd_row_csc1_lbl;
    lv_obj_t *rrsd_row_csc1_bat;
    lv_obj_t *rrsd_row_csc2;
    lv_obj_t *rrsd_row_csc2_dot;
    lv_obj_t *rrsd_row_csc2_lbl;
    lv_obj_t *rrsd_row_csc2_bat;
    lv_obj_t *rrsd_row_hr;
    lv_obj_t *rrsd_row_hr_dot;
    lv_obj_t *rrsd_row_hr_lbl;
    lv_obj_t *rrsd_row_hr_bat;
    lv_obj_t *rrsd_row_tb;
    lv_obj_t *rrsd_row_tb_dot;
    lv_obj_t *rrsd_row_tb_lbl;
    lv_obj_t *rrsd_row_tb_bat;
    lv_obj_t *rrsd_row_fl;
    lv_obj_t *rrsd_row_fl_dot;
    lv_obj_t *rrsd_row_fl_lbl;
    lv_obj_t *rrsd_row_fl_bat;
    lv_obj_t *rrsd_hint;
    lv_obj_t *rrsd_btn_back;
    lv_obj_t *rrsd_btn_back_lbl;
    lv_obj_t *rrroute_ic;
    lv_obj_t *rrroute_title;
    lv_obj_t *rrroute_dest;
    lv_obj_t *rrroute_list;
    lv_obj_t *rrroute_empty;
    lv_obj_t *rrroute_empty2;
    lv_obj_t *rrroute_foot;
} objects_t;

extern objects_t objects;

void create_screen_rim_ridge();
void tick_screen_rim_ridge();

void create_screen_rim_ridge_nav();
void tick_screen_rim_ridge_nav();

void create_screen_rim_ridge_rq();
void tick_screen_rim_ridge_rq();

void create_screen_rim_ridge_settings();
void tick_screen_rim_ridge_settings();

void create_screen_rim_ridge_climb();
void tick_screen_rim_ridge_climb();

void create_screen_rim_ridge_wifi();
void tick_screen_rim_ridge_wifi();

void create_screen_rim_ridge_wifi_pw();
void tick_screen_rim_ridge_wifi_pw();

void create_screen_rim_ridge_settings_wifi();
void tick_screen_rim_ridge_settings_wifi();

void create_screen_rim_ridge_settings_imu();
void tick_screen_rim_ridge_settings_imu();

void create_screen_rim_ridge_settings_alt();
void tick_screen_rim_ridge_settings_alt();

void create_screen_rim_ridge_settings_num();
void tick_screen_rim_ridge_settings_num();

void create_screen_rim_ridge_settings_dev();
void tick_screen_rim_ridge_settings_dev();

void create_screen_rim_ridge_route();
void tick_screen_rim_ridge_route();

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