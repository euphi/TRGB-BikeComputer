// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.2.1
// LVGL VERSION: 8.3.4
// PROJECT: SquareLine_Project

#include "ui.h"
#include "ui_Settings.h"
#include "Singletons.h"


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

void BrightChanged(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
	//int sliderValue = ( ( (int)lv_slider_get_value(slider)) << 8 ) / 100 - 1;
	int sliderValue = (int)lv_slider_get_value(slider);

	bclog.logf( BCLogger::Log_Info, BCLogger::TAG_OP, "New brightness value %d.\n", sliderValue);
	if (sliderValue > 250) {
		digitalWrite(EXAMPLE_PIN_NUM_BK_LIGHT, EXAMPLE_LCD_BK_LIGHT_ON_LEVEL);
	} else {
		analogWrite(EXAMPLE_PIN_NUM_BK_LIGHT, sliderValue);
	}
}

