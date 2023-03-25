// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: BC_ScreenNavspj

#ifndef _BC_SCREENNAVSPJ_UI_H
#define _BC_SCREENNAVSPJ_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void ui_event_SNavi(lv_event_t * e);
extern lv_obj_t * ui_SNavi;
extern lv_obj_t * ui_SNavPanelStat;
extern lv_obj_t * ui_SNavArcSpeed;
extern lv_obj_t * ui_SNavImgNav;
extern lv_obj_t * ui_SNavBarNavDist;
extern lv_obj_t * ui_SNavLabelStreet;
extern lv_obj_t * ui_SNavLabelNavDist;
extern lv_obj_t * ui_SNavLabelSpeed;
extern lv_obj_t * ui_SNavBarHR;
extern lv_obj_t * ui_SNavLabenHR;
extern lv_obj_t * ui_SNavLabelCad;
extern lv_obj_t * ui_SNavBarBat;

extern const lv_img_dsc_t* NavImgTable[];

void ui_SNavi_screen_init(void);

LV_IMG_DECLARE(nav_reserved)
LV_IMG_DECLARE(nav_straight )
LV_IMG_DECLARE(nav_start )
LV_IMG_DECLARE(nav_finish )
LV_IMG_DECLARE(nav_left45 )
LV_IMG_DECLARE(nav_left90 )
LV_IMG_DECLARE(nav_left135 )
LV_IMG_DECLARE(nav_right135 )
LV_IMG_DECLARE(nav_right90 )
LV_IMG_DECLARE(nav_right45 )
LV_IMG_DECLARE(nav_fork_r )
LV_IMG_DECLARE(nav_fork_l )
LV_IMG_DECLARE(nav_uturn )
LV_IMG_DECLARE(nav_Kreisel_3_3 )
LV_IMG_DECLARE(nav_nonav )

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
