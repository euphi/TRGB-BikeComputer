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
 * Screen switching: shown via a tap on rr_line_rq (RimRidge's road-quality
 * indicator, see action_go_to_rq() in RimRidgeCustFunc.cpp calling
 * UIFacade::showRQScreen()), returned from via a swipe gesture on this
 * screen itself (action_rq_screen_gesture() below calling
 * UIFacade::hideRQScreen()) - same manual-only pattern as RimRidgeNav's
 * tap/swipe, but with no auto-popup logic at all.
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

#ifdef __cplusplus
} /*extern "C"*/
#endif
