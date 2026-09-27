/*
 * RimRidgeRQCustFunc.h
 *
 * Hand-written data wiring for the EEZ Studio-generated "RimRidgeRQ"
 * (RQ-Ride-Screen) page (src/ui_eez/, feature/rimridge-ui-design).
 * Mirrors the RimRidgeCustFunc.h/RimRidgeNavCustFunc.h pattern - lives
 * outside src/ui_eez/ because that whole directory gets overwritten on
 * every EEZ Studio export.
 *
 * Scope as of 2026-09-26 (see doc/design/rim-ridge-design-system.md §6):
 * only the plain telemetry widgets this header wires up - speed, pulse
 * (HR), trip counter, nav pill - are connected to real data. The
 * RQ-index digit, the 6-pill surface picker, the 4-stop quality selector
 * and the record button all exist as widgets already but are deliberately
 * NOT wired here yet - no event handlers, no data, per the user
 * ("erstmal nur mit den bekannten Daten").
 *
 * Screen switching: shown via a tap on the state icon/RQ line group
 * (rr_group_rq_mode and its copies on RimRidgeNav/RimRidgeSettings, see
 * action_go_to_rq() in RimRidgeCustFunc.cpp calling UIFacade::showRQScreen()),
 * returned from via a tap on this screen's own copy (rq_group_rq_mode, same
 * action) or a swipe gesture on the screen (action_rq_screen_gesture() below
 * calling UIFacade::hideRQScreen()) - same manual-only pattern as
 * RimRidgeNav's tap/swipe, but with no auto-popup logic at all.
 *
 * Manual road label controls (surface pills, quality selector, record
 * button - added 2026-09-26 once I2CSensors grew setRoadLabel*()/
 * startRoadCapture()/stopRoadCapture()/getRoadLabelState(), see
 * I2CSensors.h and doc/design/rim-ridge-design-system.md's RQ-Ride-Screen
 * section): wired with plain lv_obj_add_event_cb() calls, not EEZ actions -
 * 11 click targets would mean 11 near-identical actions in the JSON for no
 * benefit, and no navigation/screen-load is involved. Gravel variant
 * (TRGBBC_SENSORS_I2C) only - a no-op on the FL variant, which has no
 * BMI160 and thus no I2CSensors instance at all.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Fan-out from the same data already pushed to RimRidge's own
// rr_speed_val/rr_hr_val - called from the same call sites in
// UIFacade.cpp so all screens stay in sync from one update.
void ui_RimRidgeRQUpdateSpeed(float speed);
void ui_RimRidgeRQUpdateHR(int16_t hr);

// distM in meters, same source as ui_RimRidgeUpdateStats()'s dist
// parameter - formatted here as "X.X km" like rr_distance_val.
void ui_RimRidgeRQUpdateDist(uint32_t distM);

// Same data/semantics as ui_RimRidgeUpdateNav()/ui_RimRidgeUpdateNavDist()
// (navStr intentionally not shown here - no widget for it, matches this
// screen's compact nav pill which only has room for icon+distance).
void ui_RimRidgeRQUpdateNav(uint32_t dist, uint8_t maneuver, uint8_t roundaboutExit);
void ui_RimRidgeRQUpdateNavDist(uint32_t dist);

// action_rq_screen_gesture() (wired to the RimRidgeRQ screen root's
// GESTURE event) is defined in the .cpp and declared by the generated
// src/ui_eez/actions.h, same as every other EEZ action in this project -
// not redeclared here.

// Registers the CLICKED handlers on the 6 surface pills, 4 quality circles
// and the record button (see the .cpp for the tap-toggle behavior) - call
// once from UIFacade::initDisplay() after create_screen_rim_ridge_rq().
void ui_RimRidgeRQInitLabelControls();

// Tap on rq_nav_pill opens the full nav screen (UIFacade::showNavScreen()) -
// same target as RimRidge's rr_nav_pill/GoToNav action, just reached via a
// direct lv_obj_add_event_cb() instead of a second near-identical EEZ
// action. Manual only, like every other screen switch in this app - does
// NOT touch UIFacade's auto-show/auto-hide bookkeeping (evaluateNaviAuto
// Switch()), so it never pops up or closes on its own from here.
void ui_RimRidgeRQInitNavLink();

// Refreshes the label controls' visual state - which pill/circle is
// highlighted, the record button's idle/recording look - from
// I2CSensors::getRoadLabelState(), never from the tap itself: a capture can
// stop by itself (30 min cap) and the label is gone after a restart, so the
// display has to reflect the sensor's own state, not the last click. Called
// at the same cadence as ui_RimRidgeUpdateRoadQuality() (Statistics::
// updateRoadQualityUi(), via UIFacade::updateRoadLabel()).
//   surface/quality  0 = none, same encoding as I2CSensors::setRoadLabel*()
void ui_RimRidgeRQUpdateLabel(uint8_t surface, uint8_t quality, bool capturing);

#ifdef __cplusplus
} /*extern "C"*/
#endif
