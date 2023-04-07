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

#include "Statistics.h"
extern Statistics stats;

#include "FLClassicParser.h"
extern FLClassicParser flparser;

#include "UIFacade.h"
extern UIFacade ui;

#include "TRGBSuppport.h"
extern TRGBSuppport trgb;

#include <WifiWebserver.h>
extern WifiWebserver webserver;

#include "BLEDevices.h"
extern BLEDevices bleDevs;
