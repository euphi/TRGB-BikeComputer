/*
 * RimRidgeNavCustFunc.h
 *
 * Hand-written data wiring for the EEZ Studio-generated "RimRidgeNav"
 * full-screen auto-popup nav view (src/ui_eez/, feature/rimridge-ui-design).
 * Mirrors the RimRidgeCustFunc.h pattern - lives outside src/ui_eez/
 * because that whole directory gets overwritten on every EEZ Studio export.
 *
 * This screen is shown/hidden automatically by UIFacade::updateNavi() (the
 * pre-RimRidge SNavi screen worked the same way) - there is no manual
 * navigation to/from it.
 *
 * Deliberately NOT built yet (see build_rimridge_nav.py's module
 * docstring): the lane-guidance row. TrailBridge doesn't send LANES/
 * NEXT_LANES bytes yet either, so there's nothing real to wire up against.
 */

#pragma once

#include "BikeNavProtocol.h"	// NavLane, used by ui_RimRidgeNavUpdateLanes()'s signature below

#ifdef __cplusplus
extern "C" {
#endif

// dist/maneuver/roundaboutExit drive the big icon + distance-to-maneuver
// arc + distance text; street is the CURRENT maneuver's street name
// (UIFacade::updateNavi()'s navStr - NOT nextStreet, a mix-up found
// 2026-09-19: the call site briefly passed nextStreet here, showing the
// maneuver-after-next's street where the current one belonged).
// nextManeuver/nextManeuverDist drive the small top-right preview badge
// (hidden when nextManeuver is NAV_MANEUVER_NONE/UNKNOWN, matching the
// old SNavi screen's ui_ScrNaviUpdateNav precedent) - nextStreet itself
// is deliberately not shown anywhere on this screen at all (not enough
// room for two street names at once, per the user).
void ui_RimRidgeNavUpdateNav(uint32_t dist, uint8_t maneuver, uint8_t roundaboutExit,
		const char* street, uint8_t nextManeuver, uint32_t nextManeuverDist);

// Fan-out from the same data already pushed to RimRidge's own
// rr_speed_val/rr_gradient_val/rr_hr_val - called from the same call
// sites in UIFacade.cpp so both screens stay in sync from one update.
void ui_RimRidgeNavUpdateSpeed(float speed);
void ui_RimRidgeNavUpdateGrad(float grad);
void ui_RimRidgeNavUpdateHR(int16_t hr);

// rrnav_lane_row - the same lane data shown on RimRidge's rr_lane_row
// (current maneuver's LANES, not NEXT_LANES - see UIFacade::updateLanes()),
// just at this screen's own 5-tile-cap, 280x50 box (see lane_icon.c's
// NAV_PROFILE_BY_COUNT for why it's a very different shape from the main
// screen's chip). laneCount==0 hides the row; no distance gating at all
// (LANE_DISTANCE_M is explicitly unused per the user - "das Protokoll
// behauptet, sie ist eh nicht korrekt") - shown purely on laneCount>0. No
// maneuver code needed - lane brightness comes entirely from each lane's
// own content, see lane_icon.h's lane_row_get() doc comment.
void ui_RimRidgeNavUpdateLanes(const NavLane* lanes, uint8_t laneCount);

#ifdef __cplusplus
} /*extern "C"*/
#endif
