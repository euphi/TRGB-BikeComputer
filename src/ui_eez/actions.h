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

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/