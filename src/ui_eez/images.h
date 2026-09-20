#ifndef EEZ_LVGL_UI_IMAGES_H
#define EEZ_LVGL_UI_IMAGES_H

#include <lvgl/lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t img_settings_icon;
extern const lv_img_dsc_t img_rr_icon_wifi;
extern const lv_img_dsc_t img_rr_icon_gps;
extern const lv_img_dsc_t img_rr_icon_battery;
extern const lv_img_dsc_t img_rr_icon_turn;
extern const lv_img_dsc_t img_rr_icon_cadence;
extern const lv_img_dsc_t img_rr_icon_power;
extern const lv_img_dsc_t img_rr_icon_temp;
extern const lv_img_dsc_t img_rr_icon_height;
extern const lv_img_dsc_t img_rr_icon_gradient;
extern const lv_img_dsc_t img_rr_icon_heart;
extern const lv_img_dsc_t img_rr_icon_pause;
extern const lv_img_dsc_t img_rr_icon_state_stop;
extern const lv_img_dsc_t img_rr_icon_state_break;
extern const lv_img_dsc_t img_rr_icon_state_power;
extern const lv_img_dsc_t img_rr_icon_state_coasting;
extern const lv_img_dsc_t img_rr_icon_turn_large;
extern const lv_img_dsc_t img_rr_icon_turn_64;

#ifndef EXT_IMG_DESC_T
#define EXT_IMG_DESC_T
typedef struct _ext_img_desc_t {
    const char *name;
    const lv_img_dsc_t *img_dsc;
} ext_img_desc_t;
#endif

extern const ext_img_desc_t images[18];

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_IMAGES_H*/