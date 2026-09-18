#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_go_to_navi(lv_event_t * e);
extern void action_go_to_chart(lv_event_t * e);
extern void action_go_to_fl_screen(lv_event_t * e);
extern void action_go_to_wlan(lv_event_t * e);
extern void action_go_to_settings(lv_event_t * e);
extern void action_reload_main_screen(lv_event_t * e);
extern void action_drive_state_short_press(lv_event_t * e);
extern void action_drive_state_long_press(lv_event_t * e);
extern void action_clock_gesture(lv_event_t * e);
extern void action_reset_stats(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/