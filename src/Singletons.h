/*
 * Singletons.h
 *
 *  Created on: 25.02.2023
 *      Author: ian
 */

#pragma once


#include <SimpleCLI.h>
extern SimpleCLI cli;

#include "BCLogger.h"
extern BCLogger bclog;

#include "Stats/Statistics.h"
extern Statistics stats;

#ifdef BC_FL_SUPPORT
#include "FLClassicParser.h"
extern FLClassicParser flparser;
#endif

#include "UIFacade.h"
extern UIFacade ui;

#include "TRGBSuppport.h"
extern TRGBSuppport trgb;

#include <WifiWebserver.h>
extern WifiWebserver webserver;

#include "BLEDevices.h"
extern BLEDevices bleDevs;

#ifdef TRGBBC_SENSORS_I2C
#include "I2CSensors.h"
extern I2CSensors sensors;
#endif
