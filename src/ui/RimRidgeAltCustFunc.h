/*
 * RimRidgeAltCustFunc.h
 *
 * Hand-written logic for the height calibration pages of the settings (EEZ Studio-generated,
 * src/ui_eez/ is replaced on every export). Reached from the hub (RimRidgeSettings, see
 * RimRidgeSettingsCustFunc.h). The calibration itself is I2CSensors::calibrateHeight() & co;
 * the same functions are on the web page /sensor/.
 *
 *   RimRidgeSettingsAlt  current height and pressure, three preset pills (tap = calibrate to the
 *                        preset height, long press = change the preset), "GPS" (height above sea
 *                        level from TrailBridge's fix), "Manuell"
 *   RimRidgeSettingsNum  number entry for "Manuell" -- either the known height (m) or the
 *                        reference pressure at sea level (hPa), switched by two pills -- and for
 *                        changing a preset. Own numeric keyboard map (set here), digits, sign,
 *                        decimal point, backspace and clear; Abbrechen / Übernehmen below it.
 *
 * Swipe on the Alt page goes back to the hub; the entry screen has no swipe (it would fire while
 * typing). EEZ actions (action_alt_*) are declared by the generated src/ui_eez/actions.h.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Once, after create_screen_rim_ridge_settings_alt() and _num(): keyboard map and handler,
// the canvas' example content out of the text fields.
void ui_RimRidgeAltInit();

// Height, pressure, preset labels and button states of the Alt page, and the hub's one-line
// summary. xUIDrawMutex must be held.
void ui_RimRidgeAltUpdate();

#ifdef __cplusplus
} /*extern "C"*/
#endif
