/*
 * RimRidgeCustFunc.h
 *
 * Hand-written data wiring for the EEZ Studio-generated "RimRidge" main
 * screen (src/ui_eez/, feature/rimridge-ui-design). Mirrors the pattern of
 * ui_MainNoFL_CustFunc.h for the SquareLine-generated MainNoFL screen it
 * replaces - lives outside src/ui_eez/ because that whole directory gets
 * overwritten on every EEZ Studio export.
 *
 * Deliberately NOT wired yet (confirmed with the user 2026-09-18, see
 * memory ui-tooling-eez-studio-migration - restore once RimRidge grows a
 * widget for it):
 *   - rr_power_val (no data source in the firmware at all yet)
 *   - average speed + clock/time (no widgets on RimRidge for these)
 *   - driving-state icon (coasting/power/braking/stopped - no widget)
 */

#pragma once

#include "BikeNavProtocol.h"	// NavLane, used by ui_RimRidgeUpdateLanes()'s signature below

#ifdef __cplusplus
extern "C" {
#endif

void ui_RimRidgeUpdateSpeed(float speed);
void ui_RimRidgeUpdateCadence(int16_t cadence);
void ui_RimRidgeUpdateHR(int16_t hr);
void ui_RimRidgeUpdateGrad(float grad, float height);

void ui_RimRidgeUpdateIntBatPerc(uint8_t perc);

// modeStr/avgStr mirror ui_SMainNoFLUpdateStats' signature so callers don't
// need to change, but RimRidge only has a place for the mode text right
// now (rr_tour_label) - avgSpd/maxSpd are accepted and ignored.
void ui_RimRidgeUpdateStats(const char* modeStr, const char* avgStr, float avgSpd, float maxSpd,
		float temperature, uint32_t dist, uint32_t timeInS);

void ui_RimRidgeUpdateNavDist(uint32_t dist);
void ui_RimRidgeUpdateNav(const char* navStr, uint32_t dist, uint8_t maneuver, uint8_t roundaboutExit);

void ui_RimRidgeUpdateGpsFix(bool hasFix, lv_color_t color);
void ui_RimRidgeUpdateWiFiState(bool wifiEnabled, bool APModeActive, bool disableAPMode, uint8_t apStaCount);

// Driving-state icon (rr_ic_state, added 2026-09-19 per the "Mainscreen-
// Studie" follow-up artifact). pIcon NULL hides the widget - used for
// DS_NO_CONN, which has no icon in the artifact (only Stop/Break/
// Coasting/Power are shown). The icon<->state mapping lives in
// UIFacade::updateStateIcon(), matching the pre-RimRidge precedent
// (ui_SMainNoFLUpdateStateIcon) of resolving the enum to a bitmap pointer
// there rather than pulling Statistics.h into this extern "C" header.
void ui_RimRidgeUpdateStateIcon(const lv_img_dsc_t* pIcon, lv_color_t color);

// Lane-guidance row (rr_lane_row) + rr_nav_pill's slide-right-and-shrink
// animation, per the "Rim & Ridge - Mainscreen" design study section.
// laneCount==0 fades out the row and animates the pill back to its
// EEZ-authored resting position (140,85,200,78 - unchanged in the
// .eez-project, always the canvas truth); laneCount>0 shows/updates the
// row and animates the pill to its "shown" position/size, which exists
// ONLY in this C code, not in the JSON - see patch_rimridge_lane_row.py's
// module comment for why (EEZ Studio's schema has no per-state override
// for a widget's own left/top/width/height) and the pixel-math worksheet
// behind the exact numbers. No maneuver code is needed here - lane
// brightness comes entirely from each lane's own content, see
// lane_icon.h's lane_row_get() doc comment.
void ui_RimRidgeUpdateLanes(const NavLane* lanes, uint8_t laneCount);

#ifdef __cplusplus
} /*extern "C"*/
#endif
