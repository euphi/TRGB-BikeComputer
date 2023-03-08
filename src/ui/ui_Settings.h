// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: TRGB-BC-Settings

#ifndef _TRGBBC_SETTINGS_UI_H
#define _TRGBBC_SETTINGS_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void ui_event_ScrSettings(lv_event_t * e);
extern lv_obj_t * ui_ScrSettings;
void ui_event_ScrSettButSleep(lv_event_t * e);
extern lv_obj_t * ui_ScrSettButSleep;
extern lv_obj_t * ui_ScrSettButSleepLabel;
void ui_event_ScrSettSliderBright(lv_event_t * e);
extern lv_obj_t * ui_ScrSettSliderBright;
extern lv_obj_t * ui_ScrSettSliderBrightLabel;
extern lv_obj_t * ui_ScrSettLabelClockDate;
extern lv_obj_t * ui_ScrSettLabelVersion;

void EvDeepSleep(lv_event_t * e);
void BrightChanged(lv_event_t * e);


void ui_ScrSettings_screen_init(void);




#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
