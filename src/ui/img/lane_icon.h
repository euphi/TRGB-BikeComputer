/*
 * lane_icon.h
 *
 * Parametric lane-guidance row icon (turn-lane arrows for the LANES /
 * NEXT_LANES BLE TLV data, ../TrailBridge/PROTOCOL.md "Fahrspur-
 * Informationen", already parsed into NavLane by BLEDevices.cpp and stored
 * by UIFacade::updateLanes() -- nothing renders it yet, this module is that
 * missing piece).
 *
 * Background: a lane can carry up to 3 simultaneous directions
 * (primary/secondary/tertiary) and the design study ("Lane-Widget --
 * Extremfälle" section of the Mainscreen-Studie) catalogs single arrows x4
 * angle buckets, Y-split combos x6, a 3-way trident, U-turn hooks x2, plus
 * an overflow "..." tile -- and all of that again at up to 5 different
 * pitch/scale profiles depending on how many lanes are shown at once, times
 * two different screen contexts (compact 3-tile Main chip vs up-to-5-tile
 * Nav strip). Baking that combinatorial cross product as static bitmaps
 * (like the maneuver icons in nav_icons.c) was judged not worth the flash
 * budget by the user -- this module instead draws the whole lane row fresh
 * with LVGL draw primitives onto one canvas per update, the same approach
 * roundabout-icon.c already uses for the same reason (see its header for
 * the fuller rationale).
 *
 * Geometry is a documented, deliberate *approximation* of the design study
 * (see lane_icon.c's module comment for the exact simplifications), not a
 * pixel-accurate reproduction of its SVGs -- consistent with
 * roundabout-icon.c's own "deliberately simple heuristic" precedent.
 *
 * Color format: LV_IMG_CF_TRUE_COLOR_ALPHA, ink drawn in black at full or
 * ~40% opacity (bright = recommended/route-matching, dim = present but not
 * the way to go) so the existing lv_obj_set_style_img_recolor() tinting
 * (RRBrass on both RimRidge and RimRidgeNav) recolors the ink while leaving
 * the alpha-driven bright/dim distinction intact -- exactly like
 * roundabout-icon.c and for the same reason.
 *
 * Created on: 20.09.2026
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>
#include <stdint.h>
#include "BikeNavProtocol.h"

// Hard ceiling on tiles per row (post-overflow-collapse) - both real call
// sites (RimRidge's 3-tile chip, RimRidgeNav's 5-tile strip) stay at or
// below this; also sizes the fixed-size scratch array in lane_row_get().
#define LANE_ROW_MAX_TILES 5

/**
 * Get (rendering on first use / on change, and caching afterwards) a
 * complete lane-guidance row image: up to `maxTiles` tiles evenly spaced
 * across a `width` x `height` canvas.
 *
 * Overflow rule when laneCount > maxTiles (see lane_icon.c for the full
 * walk-through): every ACTIVE lane (NAV_LANE_FLAG_ACTIVE, i.e. recommended
 * by the router for the calculated route) is guaranteed to render as a real
 * tile. Remaining tile budget fills with the nearest neighboring lanes on
 * either side; anything left beyond that collapses into a single "..."
 * tile per side instead of being silently cut off.
 *
 * Per-tile brightness (corrected 2026-09-20 -- see lane_icon.c's
 * "Brightness rule" section for the incident that prompted this):
 * PROTOCOL.md's ACTIVE flag is a per-LANE recommendation, not a per-
 * direction one, so no maneuver code is consulted here at all. A lane's
 * shared shaft and its PRIMARY branch (byte 0, "Haupt-Pfeilrichtung
 * dieser Spur" per PROTOCOL.md) share the lane's own ACTIVE-flag
 * brightness; secondary/tertiary branches are always the dimmer "also
 * possible from here" ones, regardless of ACTIVE. Lanes render purely
 * from their own content -- independent of the current navigation state,
 * exactly as PROTOCOL.md intends.
 *
 * @param lanes          left-to-right lane array, straight from
 *                        UIFacade's NavLane storage (BikeNavProtocol.h).
 * @param laneCount       number of valid entries in `lanes`. 0 returns NULL
 *                        (caller should hide the row widget entirely).
 * @param maxTiles        hard cap on rendered tiles for this call site.
 *                        Only two values are meaningful in this firmware:
 *                        3 (RimRidge's compact main-screen chip, matching
 *                        its original fixed 3-tile layout) or 5
 *                        (RimRidgeNav's full-width strip, matching the
 *                        design study's revised up-to-5-tile frame) -- the
 *                        pitch/scale profile tables in lane_icon.c are
 *                        keyed to exactly these two cases, see there.
 * @param width, height   pixel size of the destination row image. Stays
 *                        the same regardless of how many tiles are shown
 *                        this call (the frame is fixed; only the
 *                        pitch/scale of the tiles inside adapts) -- pass
 *                        the RimRidge/RimRidgeNav lane-row widget's actual
 *                        authored size here.
 * @return pointer to an lv_img_dsc_t usable with lv_img_set_src(), or NULL
 *         if laneCount is 0, width/height are invalid, or the canvas
 *         buffer could not be allocated. Owned by this module -- do not
 *         free it, and treat its contents as valid only until the next
 *         lane_row_get() call for the same (width,height) slot (it is
 *         redrawn in place, not reallocated, so the pointer itself stays
 *         stable across calls for the same size, but its pixel contents
 *         change).
 */
const lv_img_dsc_t * lane_row_get(const NavLane * lanes, uint8_t laneCount, uint8_t maxTiles, lv_coord_t width,
		lv_coord_t height);

/**
 * Free all cached canvases/buffers. Not required in normal firmware
 * operation, provided for completeness / tests -- see
 * roundabout_icon_deinit() for the equivalent on the sibling module.
 */
void lane_icon_deinit(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif
