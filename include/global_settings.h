/*
 * global_settings.h
 *
 *  Created on: 28.01.2024
 *      Author: ian
 */

#pragma once

#ifndef SETTING_BIKECOUNT
#define SETTING_BIKECOUNT 2
#endif

#ifndef SETTING_APCOUNT
#define SETTING_APCOUNT 3
#endif

// Prefix of the NVS namespaces holding the ride statistics (Statistics: ST_*, Distance:
// DIST_<bike>_*). The simulator build (BC_SIM, src/SimSensors.h) keeps its own, so fake rides
// never reach the real odometer. Namespace names: max. 15 chars ("S_DIST_0_TOTAL" is 14).
#ifdef BC_SIM
#define NVS_STAT_PREFIX "S_"
#else
#define NVS_STAT_PREFIX ""
#endif

const uint8_t bikecount = SETTING_BIKECOUNT;
const uint8_t WifiAPCount = SETTING_APCOUNT;
