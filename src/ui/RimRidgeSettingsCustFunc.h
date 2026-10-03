/*
 * RimRidgeSettingsCustFunc.h
 *
 * Hand-written logic for the EEZ Studio-generated "RimRidgeSettings" screen
 * (doc/design/settings.svg, extended by what a ride needs without web access).
 * Same pattern as RimRidgeCustFunc.h - lives outside src/ui_eez/ because that
 * whole directory gets overwritten on every EEZ Studio export.
 *
 * The settings are a hub plus one page per group (tabs would not fit the round display):
 *   RimRidgeSettings      hub: build string (and a flag for debug builds such as the simulator),
 *                         one pill per group with a one-line summary, restart and deep sleep
 *   RimRidgeSettingsWifi  IP address, WLAN on/off, hotspot and the way to the network list
 *                         (RimRidgeWifi, see RimRidgeWifiCustFunc.h)
 *   RimRidgeSettingsImu   IMU calibration and reference ride (start, progress, result)
 *   RimRidgeSettingsAlt   height calibration, RimRidgeSettingsNum its number entry
 *                         (RimRidgeAltCustFunc.h)
 *
 * Screen switching: the hub is opened by a tap on rr_btn_settings (RimRidge, action
 * GoToSettings) and closed by any swipe on it (SettingsScreenGesture), see
 * UIFacade::showSettingsScreen()/hideSettingsScreen(). Its pills open the pages; "Zurück"
 * and any swipe on a page return to the hub.
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
enum {RRSET_WIFI_OFF = 0, RRSET_WIFI_CONNECTING = 1, RRSET_WIFI_ONLINE = 2, RRSET_WIFI_AP = 3};

// Once, after create_screen_rim_ridge_settings(): build string, initial
// calibration status, and the timer that carries out restart/deep sleep.
void ui_RimRidgeSettingsInit();

// ipText: IP address, or a short status text ("WLAN aus", "Verbindung verloren", ...); in
// RRSET_WIFI_AP the hotspot password, with `caption` ("HOTSPOT <ssid>") above it instead of
// "IP-ADRESSE" (caption may be NULL/empty). The WLAN pill reads "WLAN an" when off or in AP
// mode, else "WLAN aus"; the hotspot pill "Hotspot aus" in AP mode.
void ui_RimRidgeSettingsUpdateWifi(const char* ipText, uint8_t state, const char* caption);

// Calibration/reference-ride status and button labels (and the hub's IMU summary), read fresh
// from I2CSensors::getCalibrationState(). xUIDrawMutex must be held.
void ui_RimRidgeSettingsUpdateCal();

// Once a second from the UI task: refreshes what the screen on display shows (hub summaries,
// calibration status, height). Does nothing on any other screen. xUIDrawMutex must be held.
void ui_RimRidgeSettingsTick();

#ifdef __cplusplus
} /*extern "C"*/
#endif
