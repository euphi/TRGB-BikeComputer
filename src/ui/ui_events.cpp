// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: SquareLine_Project

#include "ui.h"
#include "Singletons.h"
#include "Stats/Statistics.h"
#include "Stats/Distance.h"
#include "ui_custFunc.h"


#include <Arduino.h>

void statModeChanged(uint8_t mode) {
/*
Selected Mode 0: Auto
Selected Mode 1: Trip
Selected Mode 2: Tour
Selected Mode 3: Total
Selected Mode 4: FLTour
Selected Mode 5: FLTrip
Selected Mode 6: FL-Tot
 */
	switch (mode) {
	case 0:
		ui.setStatMode(Statistics::SUM_ESP_START);
		break;
	case 1:
		ui.setStatMode(Statistics::SUM_ESP_TRIP);
		break;
	case 2:
		ui.setStatMode(Statistics::SUM_ESP_TOUR);
		break;
	case 3:
		ui.setStatMode(Statistics::SUM_ESP_TOTAL);
		break;
	case 4:
		ui.setStatMode(Statistics::SUM_FL_TOUR);
		break;
	case 5:
		ui.setStatMode(Statistics::SUM_FL_TRIP);
		break;
	case 6:
		ui.setStatMode(Statistics::SUM_FL_TOTAL);
		break;
	}

}

void statModeNext(bool dir) {
//	Serial.println("UI Event: Stats mode next");
	ui.setStatMode(dir);
}

void resetStats()
{
//	Serial.println("UI Event: Reset stats");
	stats.getDistHandler().resetDistToZero(ui.getStatMode());
}

void statsTimeMode(bool dir)
{
//	Serial.println("UI Event: Change time mode");
	Statistics::EAvgType statTimeMode = ui.getStatTimeMode();
	ui.setStatTimeMode(Statistics::getNextTimeMode(statTimeMode, dir));
}


void driveStateUpdate(const UIDriveStateEvent op)
{
	switch (op) {
	case DSE_delayStandby:
		stats.delayStandby();
		break;
	case DSE_toggleStandbyMode:
		stats.toggleStandbyMode();
		break;
	default:
		break;
	}
}

void msgCppCB(bool ok) {
	ui.msgCBFct(ok);
}
