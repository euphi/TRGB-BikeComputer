// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.1.1
// LVGL VERSION: 8.3.3
// PROJECT: FLScreen

#pragma once

#include "FLClassicFlags.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void ui_event_ScreenFL(lv_event_t * e);
extern lv_obj_t * ui_ScreenFL;
extern lv_obj_t * ui_Screen1_Panel1;
extern lv_obj_t * ui_Screen1_Panel2;
extern lv_obj_t * ui_SFLPStateLTitle;
extern lv_obj_t * ui_SFLPStateCB1;
extern lv_obj_t * ui_SFLPStateCB2;
extern lv_obj_t * ui_SFLPStateCB3;
extern lv_obj_t * ui_SFLPStateCB4;
extern lv_obj_t * ui_SFLPStateCB5;
extern lv_obj_t * ui_SFLPStateCB6;
extern lv_obj_t * ui_SFLPStateCB7;
extern lv_obj_t * ui_SFLPStateCB8;
extern lv_obj_t * ui_Screen1_Panel3;
extern lv_obj_t * ui_Screen1_Label2;
extern lv_obj_t * ui_ScreenFLCBError0;
extern lv_obj_t * ui_ScreenFLCBError1;
extern lv_obj_t * ui_ScreenFLCBError2;
extern lv_obj_t * ui_ScreenFLCBError3;
extern lv_obj_t * ui_ScreenFLCBError4;
extern lv_obj_t * ui_ScreenFLCBError5;
extern lv_obj_t * ui_ScreenFLCBError6;
extern lv_obj_t * ui_ScreenFLCBError7;
extern lv_obj_t * ui_Screen1_Label3;
extern lv_obj_t * ui_Screen1_Bar1;
extern lv_obj_t * ui_Screen1_Label4;
extern lv_obj_t * ui_Screen1_Label5;
extern lv_obj_t * ui_ScreenFL_Bar2;
extern lv_obj_t * ui_ScreenFL_Bar1;
extern lv_obj_t * ui_ScreenFL_Bar3;
extern lv_obj_t * ui_ScreenFL_Label5;
extern lv_obj_t * ui_ScreenFL_Label1;
extern lv_obj_t * ui_ScreenFL_Label2;
extern lv_obj_t * ui_ScreenFL_Label6;
extern lv_obj_t * ui_ScreenFL_Label4;
extern lv_obj_t * ui_ScreenFL_Label3;

void ui_ScrFLUpdateFlags(uint32_t flags);
void ui_ScrFLUpdatePower(uint16_t batVoltage, uint8_t batPerc, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn);
void ui_ScrFLUpdateClock(const char* clockStr, const char* dateStr);

void ui_ScreenFL_screen_init(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
