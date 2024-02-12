/*
 * Singletons.cpp
 *
 *  Created on: 25.02.2023
 *      Author: ian
 */


#include "Singletons.h"

SimpleCLI cli;
BCLogger bclog;
Statistics stats;
#ifdef BC_FL_SUPPORT
FLClassicParser flparser(14, 2155);  // TODO: Load parameters from preferences
#endif
UIFacade ui;
//TRGBSuppport trgb;
WifiWebserver webserver;
BLEDevices bleDevs;
I2CSensors sensors;

