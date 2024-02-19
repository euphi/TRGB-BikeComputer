/*
 * ui_SWLAN_CustFunc.h
 *
 *  Created on: 17.02.2024
 *      Author: ian
 */

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_SWLAN_extra_init();
void ui_SWLANUpdateIP(const char * ipStr);
void ui_SWLANUpdateSSIDList(const char * ssidList);
void ui_SWLANUpdateWiFiState(bool wifiEnabled, bool APModeActive, bool disableAPMode, short apStaCount);


#ifdef __cplusplus
} /*extern "C"*/
#endif
