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

#ifdef __cplusplus
} /*extern "C"*/
#endif
