/*
 * RimRidgeSettingsCustFunc.h
 *
 * Hand-written logic for the EEZ Studio-generated "RimRidgeSettings" screen
 * (doc/design/settings.svg, extended 2026-09-27 by doc/USABILITY-TODO.md #2/#3).
 * Same pattern as RimRidgeCustFunc.h - lives outside src/ui_eez/ because that
 * whole directory gets overwritten on every EEZ Studio export.
 *
 * Content: build string, IP address + WLAN reconnect, IMU calibration and
 * reference ride (start, progress, result), restart and deep sleep.
 *
 * Screen switching: opened by a tap on rr_btn_settings (RimRidge, action
 * GoToSettings), closed by any swipe on the screen itself (SettingsScreenGesture),
 * see UIFacade::showSettingsScreen()/hideSettingsScreen().
 *
 * The EEZ actions (action_settings_*, action_go_to_settings) are declared by the
 * generated src/ui_eez/actions.h and defined in the .cpp.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// WLAN state for ui_RimRidgeSettingsUpdateWifi(), same values as UIFacade::WifiUiState.
enum {RRSET_WIFI_OFF = 0, RRSET_WIFI_CONNECTING = 1, RRSET_WIFI_ONLINE = 2};

// Once, after create_screen_rim_ridge_settings(): build string, initial
// calibration status, and the timer that carries out restart/deep sleep.
void ui_RimRidgeSettingsInit();

// ipText: IP address (with " (AP)" in AP mode) or a short status text ("WLAN aus",
// "Verbindung verloren", ...). The reconnect pill is only active in RRSET_WIFI_OFF.
void ui_RimRidgeSettingsUpdateWifi(const char* ipText, uint8_t state);

// Calibration/reference-ride status and button labels, read fresh from
// I2CSensors::getCalibrationState(). Called once a second while the screen is
// shown, and right after a tap. xUIDrawMutex must be held.
void ui_RimRidgeSettingsUpdateCal();

#ifdef __cplusplus
} /*extern "C"*/
#endif
