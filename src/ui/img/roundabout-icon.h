/*
 * roundabout-icon.h
 *
 * Parametric "take the Nth roundabout exit" navigation icon.
 *
 * Background: the BikeNavRelay TLV protocol (../BikeNavRelay/PROTOCOL.md,
 * tag 0x03 ROUNDABOUT_EXIT) only ever hands us a raw 1-based exit number --
 * unlike the old Komoot-era icon set in doc/Resources/Images/
 * (nav_64_rounda_<exit>_<totalExits>.png), there is no total-exit-count to
 * key a combinatorial (exit, totalExits) image lookup off. That old set was
 * also hand-baked and capped at 3 total exits, which is too low for a lot
 * of real-world roundabouts.
 *
 * Baking a new static image per exit number would cost flash (already at
 * ~91% on the esp32s3 build targets) for comparatively little benefit, so
 * this module instead draws a single generic roundabout glyph on the fly
 * with LVGL draw primitives (a ring, a fixed entry stub, a rotated exit
 * arrow, and the exit number as a digit in the middle) onto an
 * LV_IMG_CF_TRUE_COLOR_ALPHA canvas. Rendering onto a real alpha channel
 * (rather than a baked opaque PNG) keeps the existing
 * lv_obj_set_style_img_recolor() distance-based tinting in
 * ui_NaviCustFunc.c working exactly like it does for the other nav icons --
 * only the ink pixels get tinted, not a solid background block.
 *
 * The returned lv_img_dsc_t* is a normal image source, usable anywhere
 * lv_img_set_src() is. It is wired into the NAV_MANEUVER_ROUNDABOUT case of
 * navIcon64()/navIconLarge() in src/ui/img/nav_icons.c, which is the actual
 * maneuver-code -> icon lookup used by ui_NaviCustFunc.c and
 * ui_MainNoFL_CustFunc.c (see nav_icons.h).
 *
 * Angle convention (see roundabout-icon.c for the full rationale): the
 * entry is drawn fixed at the bottom of the icon, and the exit arrow is
 * rotated by a fixed 45 degree step per exit index, going counter-clockwise
 * to match right-hand-traffic roundabouts (mainland Europe). This is a
 * deliberately simple heuristic -- the protocol gives us no real bearing
 * data to render a geometrically accurate exit angle -- but it reproduces
 * the familiar "how far around do I go" shape used by common turn-by-turn
 * UIs, which matters more for a quick glance while riding than exactness.
 *
 * Created on: 14.09.2026
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>
#include <stdint.h>

/**
 * Get (rendering on first use / on change, and caching afterwards) the
 * roundabout icon for the given 1-based exit number at the given pixel
 * size (the icon is always square).
 *
 * @param exit_number  1-based exit index, straight from ROUNDABOUT_EXIT
 *                      (TLV tag 0x03). 0 is treated the same as 1
 *                      (defensive fallback, should not occur per protocol).
 *                      Not capped: any value up to 255 renders (the digit
 *                      label always shows the real number), only the
 *                      rotation angle wraps every 8 exits (45 degrees each)
 *                      since real roundabouts essentially never have that
 *                      many exits and the wrap keeps the arrow direction
 *                      meaningful (a full lap back to the entry angle for
 *                      exit 8 reads as "the exit right before your own
 *                      entry", i.e. a near-U-turn, which happens to be
 *                      correct).
 * @param size         icon edge length in px, e.g. 64 (small nav panel) or
 *                      192 (main nav panel, matching the existing
 *                      Nav64ImgTable / NavImgTable icon sizes). Only a
 *                      small number of distinct sizes are cached
 *                      (see ROUNDABOUT_ICON_CACHE_SLOTS in the .c file) --
 *                      calling with more distinct sizes than that still
 *                      works, just evicts and re-renders more often.
 * @return pointer to an lv_img_dsc_t usable with lv_img_set_src(), or NULL
 *         if size is invalid or the canvas buffer could not be allocated.
 *         The returned pointer is owned by this module -- do not free it,
 *         and treat its contents as valid only until the next call to
 *         roundabout_icon_get() with the same size (it is redrawn in
 *         place, not reallocated, so the pointer itself stays stable
 *         across calls with the same size, but its pixel contents change).
 */
const lv_img_dsc_t * roundabout_icon_get(uint8_t exit_number, lv_coord_t size);

/**
 * Free all cached canvases/buffers. Not required in normal firmware
 * operation (the small, bounded cache is meant to just live for the
 * lifetime of the app, like any other static icon table), but provided
 * for completeness / tests.
 */
void roundabout_icon_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
