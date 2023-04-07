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
FLClassicParser flparser(14, 2155);  // TODO: Load parameters from preferences
UIFacade ui;
TRGBSuppport trgb;
WifiWebserver webserver;
BLEDevices bleDevs;

