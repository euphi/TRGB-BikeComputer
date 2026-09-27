// Remainder of the old SquareLine main project (screens S1Main, Chart, Settings,
// ... all removed by 2026-09-27; the UI is EEZ Studio now, see src/ui_eez/).
// Only the main-screen pointer the rest of the firmware still loads is left.

#include "ui.h"

lv_obj_t * ui_MainScreen;	// set to RimRidge in UIFacade::initDisplay()

// The SquareLine-era fonts/images under font/ and img/ were converted for these settings
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif
