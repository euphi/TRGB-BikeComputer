/*
 * RimRidgeDevCustFunc.h
 *
 * Hand-written logic for the BLE device page of the settings (EEZ page RimRidgeSettingsDev,
 * src/ui_eez/ is replaced on every export). One row per device -- CSC 1/2 (shown as Speed or
 * Kadenz once the first data told which), heart rate, TrailBridge and, in the FL build only,
 * the Forumslader: a coloured dot for the connection (green connected, yellow found and
 * connecting, red lost, grey not found), the battery level. A long press on a row forgets the
 * sensor (BLEDevices::requestForget()), so another one can connect. Not for TrailBridge: its
 * address is never stored (no bonding, Android rotates it).
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Once, after create_screen_rim_ridge_settings_dev(): hides the Forumslader row outside the FL build.
void ui_RimRidgeDevInit();

// Rows and the hub's summary ("3 von 4"). xUIDrawMutex must be held.
void ui_RimRidgeDevUpdate();

#ifdef __cplusplus
} /*extern "C"*/
#endif
