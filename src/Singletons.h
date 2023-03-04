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
