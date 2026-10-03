#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_pause_long_press(lv_event_t * e);
extern void action_tour_pill_gesture(lv_event_t * e);
extern void action_go_to_nav(lv_event_t * e);
extern void action_nav_screen_gesture(lv_event_t * e);
extern void action_go_to_rq(lv_event_t * e);
extern void action_rq_screen_gesture(lv_event_t * e);
extern void action_go_to_settings(lv_event_t * e);
extern void action_settings_screen_gesture(lv_event_t * e);
extern void action_settings_wifi(lv_event_t * e);
extern void action_settings_cal(lv_event_t * e);
extern void action_settings_ref(lv_event_t * e);
extern void action_settings_reset(lv_event_t * e);
extern void action_settings_sleep(lv_event_t * e);
extern void action_pause_click(lv_event_t * e);
extern void action_go_to_climb(lv_event_t * e);
extern void action_climb_screen_gesture(lv_event_t * e);
extern void action_go_to_wifi(lv_event_t * e);
extern void action_wifi_screen_gesture(lv_event_t * e);
extern void action_wifi_scan(lv_event_t * e);
extern void action_wifi_back(lv_event_t * e);
extern void action_settings_ap(lv_event_t * e);
extern void action_wifi_pw_cancel(lv_event_t * e);
extern void action_wifi_pw_save(lv_event_t * e);
extern void action_wifi_pw_eye(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/