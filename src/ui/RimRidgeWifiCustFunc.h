/*
 * RimRidgeWifiCustFunc.h
 *
 * Hand-written logic for the two EEZ Studio-generated WLAN screens (src/ui_eez/ is replaced on
 * every export, so everything dynamic lives here). Reached from the WLAN page of the settings ("Netzwerke", RimRidgeSettingsWifi).
 *
 *   RimRidgeWifi    status line (what WifiWebserver is doing), the networks of the last scan as a
 *                   scrollable list (rows are created here, rrwifi_list is only their frame),
 *                   Scan and Back. A tap on a row opens
 *   RimRidgeWifiPw  password field (password mode, show/hide toggle), keyboard, Cancel/Save. Save
 *                   stores the network via WifiWebserver::addNetworkAsync() (NVS, own task) and goes
 *                   back; WifiWebserver connects if the radio is idle.
 *
 * The order of the saved networks (= priority) and removing one are not on the display, only on the
 * web page (/wifi). Networks already saved are shown in brass in the list.
 *
 * Keyboard: QWERTZ with 10 keys per row; the default LVGL layout has 12 and its keys would be 30 px
 * wide here. Own maps (lower, UPPER, symbols, more symbols) and an own key handler replace the
 * default one; the layout differs from the one the EEZ canvas shows (it renders the LVGL default).
 * Shift is one-shot. The password never goes into a log line.
 *
 * Swipe on RimRidgeWifi goes back to the WLAN settings page; the password screen has no swipe (it would fire
 * while typing) -- Cancel instead. EEZ actions (action_wifi_*, action_go_to_wifi) are declared by
 * the generated src/ui_eez/actions.h.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Once, after create_screen_rim_ridge_wifi() and _wifi_pw(): keyboard layout and handler, row styles, timer.
void ui_RimRidgeWifiInit();

#ifdef __cplusplus
} /*extern "C"*/
#endif
