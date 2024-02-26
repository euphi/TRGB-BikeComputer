// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: SquareLine_Project

#include "ui.h"
#include "Screens/Settings/ui_Settings.h"
#include "Singletons.h"
#include "Stats/Statistics.h"
#include "Stats/Distance.h"
#include "ui_custFunc.h"


#include <Arduino.h>

void chartModeHeartRate(lv_event_t * e)
{
	// Your code here
}

void chartModeBatterie(lv_event_t * e)
{
	// Your code here
}

void EvDeepSleep(lv_event_t * e)
{
	trgb.deepSleep();
}

void EvRestart(lv_event_t * e)
{
	trgb.restart();
}

void BrightChanged(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
	//int sliderValue = ( ( (int)lv_slider_get_value(slider)) << 8 ) / 100 - 1;
	int sliderValue = (int)lv_slider_get_value(slider);

	bclog.logf( BCLogger::Log_Info, BCLogger::TAG_OP, "New brightness value %d.\n", sliderValue);
	if (sliderValue > 250) {
		analogWrite(EXAMPLE_PIN_NUM_BK_LIGHT, 255);
		delay(5);
		digitalWrite(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
	} else {
		analogWrite(EXAMPLE_PIN_NUM_BK_LIGHT, sliderValue);
	}
}

void chart_up(lv_event_t * e)
{
	// Your code here
}

void chart_dn(lv_event_t * e)
{
	// Your code here
}

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
	ui_ScrMainUpdateTimeMode(Statistics::AVG_TYPE_STRING[ui.getStatTimeMode()]);
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


