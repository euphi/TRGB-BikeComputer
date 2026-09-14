/*
 * nav_icons.h
 *
 * Maneuver-code -> icon lookup for BikeNavRelay navigation (../BikeNavRelay/PROTOCOL.md),
 * replacing the old 32-slot Komoot dirCode tables (NavImgTable/Nav64ImgTable, since removed).
 */

#pragma once

#include <lvgl.h>
#include "BikeNavProtocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// Small (64px) icon for the mini nav panels on the main screens.
// roundaboutExit (1-based) is only meaningful when maneuver == NAV_MANEUVER_ROUNDABOUT.
const lv_img_dsc_t* navIcon64(uint8_t maneuver, uint8_t roundaboutExit);

// Large icon for the dedicated full-screen Navi view. Same semantics as navIcon64().
const lv_img_dsc_t* navIconLarge(uint8_t maneuver, uint8_t roundaboutExit);

#ifdef __cplusplus
} /*extern "C"*/
#endif
