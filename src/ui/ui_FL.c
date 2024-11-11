// SquareLine LVGL GENERATED FILE
// EDITOR VERSION: SquareLine Studio 1.1.1
// LVGL VERSION: 8.3.3
// PROJECT: FLScreen

#include "ui_FL.h"
#include "ui.h"

#include "ui_helpers.h"

///////////////////// VARIABLES ////////////////////
void ui_event_ScreenFL(lv_event_t * e);
lv_obj_t * ui_ScreenFL;
lv_obj_t * ui_Screen1_Panel1;
lv_obj_t * ui_Screen1_Panel2;
lv_obj_t * ui_SFLPStateLTitle;
lv_obj_t * ui_SFLPStateCB1;
lv_obj_t * ui_SFLPStateCB2;
lv_obj_t * ui_SFLPStateCB3;
lv_obj_t * ui_SFLPStateCB4;
lv_obj_t * ui_SFLPStateCB5;
lv_obj_t * ui_SFLPStateCB6;
lv_obj_t * ui_SFLPStateCB7;
lv_obj_t * ui_SFLPStateCB8;
lv_obj_t * ui_Screen1_Panel3;
lv_obj_t * ui_Screen1_Label2;
lv_obj_t * ui_ScreenFLCBError0;
lv_obj_t * ui_ScreenFLCBError1;
lv_obj_t * ui_ScreenFLCBError2;
lv_obj_t * ui_ScreenFLCBError3;
lv_obj_t * ui_ScreenFLCBError4;
lv_obj_t * ui_ScreenFLCBError5;
lv_obj_t * ui_ScreenFLCBError6;
lv_obj_t * ui_ScreenFLCBError7;
lv_obj_t * ui_Screen1_Label3;
lv_obj_t * ui_Screen1_Bar1;
lv_obj_t * ui_Screen1_Label4;
lv_obj_t * ui_Screen1_Label5;
lv_obj_t * ui_ScreenFL_Bar2;
lv_obj_t * ui_ScreenFL_Bar1;
lv_obj_t * ui_ScreenFL_Bar3;
lv_obj_t * ui_ScreenFL_Label5;
lv_obj_t * ui_ScreenFL_Label1;
lv_obj_t * ui_ScreenFL_Label2;
lv_obj_t * ui_ScreenFL_Label6;
lv_obj_t * ui_ScreenFL_Label4;
lv_obj_t * ui_ScreenFL_Label3;

///////////////////// TEST LVGL SETTINGS ////////////////////
#if LV_COLOR_DEPTH != 16
    #error "LV_COLOR_DEPTH should be 16bit to match SquareLine Studio's settings"
#endif
#if LV_COLOR_16_SWAP !=0
    #error "LV_COLOR_16_SWAP should be 0 to match SquareLine Studio's settings"
#endif

///////////////////// ANIMATIONS ////////////////////

///////////////////// FUNCTIONS ////////////////////
void ui_event_ScreenFL(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * target = lv_event_get_target(e);
    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_get_act()) == LV_DIR_RIGHT) {
        _ui_screen_change(ui_MainScreen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0);
    	//_ui_screen_change(ui_ScreenFL, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 500, 0);
    }
}

void updateBool(lv_obj_t* obj, bool b) {
	if (b) lv_obj_add_state(obj, LV_STATE_CHECKED); else lv_obj_clear_state(obj, LV_STATE_CHECKED);
}


void ui_ScrFLUpdatePower(uint16_t batVoltage, uint8_t batPerc, uint16_t batFullCap, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn) {
	// Label5: Dynamo-Leistung Pos_mitte: (-50,135);
	// Label2: Verbraucher-Leistung Pos_mitte: (7,-135);
	// Label1: Batterie-Strom Pos_mitte: (59,135);

	// Bar1: Dynamo 0 - 10'000mW - Pos_mitte: (-10%,0)
	// Bar2: Verbraucher 0-10'000mW - Pos_mitte: (0,0)
	// Bar3: Strom -1'000mA - 1'000mA - Pos_mitte: (10%,0)
	char txtBuffer[32];

	//Dynamo (in W)
	float dynPow = (CurConsumer+CurBat)/ 1000.0 * (batVoltage/1000.0);
	if (dynPow < 0) dynPow = 0;
	snprintf(txtBuffer, 31, "%.01f W", dynPow);
	lv_label_set_text(ui_ScreenFL_Label5, txtBuffer);
    lv_bar_set_value(ui_ScreenFL_Bar1, (int16_t) (dynPow*1000), LV_ANIM_ON);
    lv_bar_set_value(ui_S1BarPowerMode, powerStage, LV_ANIM_ON);
    if (powerStage < 0 ) {
        lv_obj_add_state(ui_S1BarPowerMode, LV_STATE_DISABLED);       /// States
    	powerStage = 0;
    } else {
        lv_obj_clear_state(ui_S1BarPowerMode, LV_STATE_DISABLED);       /// States
    }



	//Verbraucher
	float consPow = CurConsumer / 1000.0 * (batVoltage/1000.0);
	snprintf(txtBuffer, 31, "%.01f W", consPow);
	lv_label_set_text(ui_ScreenFL_Label2, txtBuffer);
    lv_bar_set_value(ui_ScreenFL_Bar2, (int16_t)(consPow*1000), LV_ANIM_ON);

	//Batterie
	snprintf(txtBuffer, 31, "%.02f A", CurBat/1000.0);
    lv_label_set_text(ui_ScreenFL_Label1, txtBuffer);
    lv_bar_set_value(ui_ScreenFL_Bar3, CurBat, LV_ANIM_ON);
    lv_bar_set_value(ui_Screen1_Bar1, batPerc, LV_ANIM_ON);
	snprintf(txtBuffer, 31, "%d%% of %d mAh", batPerc, batFullCap);
    lv_label_set_text(ui_Screen1_Label4, txtBuffer);
}

void ui_ScrFLUpdateClock(const char* clockStr, const char* dateStr) {
	lv_label_set_text_fmt(ui_Screen1_Label5, "%s\n%s", clockStr, dateStr);
}


void ui_ScrFLUpdateFlags(uint32_t flags) {
	lv_obj_t* const checkbox[FL_FLAG_MAX] = {
			0, 0, 0, 0, 0, 0, 0, ui_SFLPStateCB4,
			0, 					0, 						0,					ui_SFLPStateCB4,	0, 					ui_SFLPStateCB6,		ui_SFLPStateCB7,	ui_SFLPStateCB8,
			ui_ScreenFLCBError0, ui_ScreenFLCBError1, ui_ScreenFLCBError2, ui_ScreenFLCBError3, ui_ScreenFLCBError4, ui_ScreenFLCBError5, ui_ScreenFLCBError6, ui_ScreenFLCBError7
	};
	for (uint8_t f=0; f < FL_FLAG_MAX; f++) {
		//lv_obj_t* cb=0;
		lv_obj_t* cb=checkbox[f];
		switch (f) {
		case FL_FLAG_BALANCE1:
		case FL_FLAG_BALANCE2:
		case FL_FLAG_BALANCE3:
		case FL_FLAG_SHORTCIRCUIT:
		case FL_FLAG_DISCHARGE_OVR:
		case FL_FLAG_CHARGE_OVR:
		case FL_FLAG_DISCHARGE_HGH:
		case FL_FLAG_CHARGE_HGH:
			cb = ui_SFLPStateCB4;
			break;
		case FL_FLAG_OVPWRRED:
		case FL_FLAG_OVERLOAD:
		case FL_FLAG_IN_DUVR:
		case FL_FLAG_CHARGE_INH:
			cb = ui_SFLPStateCB4;
			break;
		case FL_FLAG_DISCHARGE_INH:
		case FL_FLAG_FULL_DISCHARGE:
			cb = ui_SFLPStateCB6;
			break;
		case FL_FLAG_CAPACITY_ACC:
			cb = ui_SFLPStateCB7;
			break;
		case FL_FLAG_DISCHARGE:
			cb = ui_SFLPStateCB8;
			break;
		case FL_ERR_CRITICAL:
			cb = ui_ScreenFLCBError0;
			break;
		case FL_ERR_CELL_TEMP_LOW:
			cb = ui_ScreenFLCBError1;
			break;
		case FL_ERR_CELL_TEMP_HIGH:
			cb = ui_ScreenFLCBError2;
			break;
		case FL_ERR_VOLTAGE_HIGH:
			cb = ui_ScreenFLCBError3;
			break;
		case FL_ERR_VOLTAGE_LOW:
			cb = ui_ScreenFLCBError4;
			break;

		case FL_ERR_CHARGE_PROT:
			cb = ui_ScreenFLCBError5;
			break;
		case FL_ERR_CHECKSUM:
		case FL_ERR_SYSTEM_IRQ:
			break;
		// no default:
		}
		if (cb) updateBool(cb, flags & (1<<f));
	}

}

///////////////////// SCREENS ////////////////////
void ui_ScreenFL_screen_init(void)
{
    ui_ScreenFL = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_ScreenFL, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Screen1_Panel1 = lv_obj_create(ui_ScreenFL);
    lv_obj_set_width(ui_Screen1_Panel1, lv_pct(100));
    lv_obj_set_height(ui_Screen1_Panel1, lv_pct(100));
    lv_obj_set_align(ui_Screen1_Panel1, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Screen1_Panel1,
                      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_SCROLLABLE);     /// Flags
    lv_obj_set_style_radius(ui_Screen1_Panel1, 240, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Screen1_Panel1, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen1_Panel1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Screen1_Panel2 = lv_obj_create(ui_ScreenFL);
    lv_obj_set_width(ui_Screen1_Panel2, 130);
    lv_obj_set_height(ui_Screen1_Panel2, 254);
    lv_obj_set_y(ui_Screen1_Panel2, 0);
    lv_obj_set_x(ui_Screen1_Panel2, lv_pct(-33));
    lv_obj_set_align(ui_Screen1_Panel2, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Screen1_Panel2, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_SFLPStateLTitle = lv_label_create(ui_Screen1_Panel2);
    lv_obj_set_width(ui_SFLPStateLTitle, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SFLPStateLTitle, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SFLPStateLTitle, 0);
    lv_obj_set_y(ui_SFLPStateLTitle, lv_pct(-50));
    lv_obj_set_align(ui_SFLPStateLTitle, LV_ALIGN_CENTER);
    lv_label_set_text(ui_SFLPStateLTitle, "State");
    lv_obj_set_style_text_font(ui_SFLPStateLTitle, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SFLPStateCB1 = lv_checkbox_create(ui_Screen1_Panel2);
    lv_checkbox_set_text(ui_SFLPStateCB1, "Ovl Pwr Red");
    lv_obj_set_width(ui_SFLPStateCB1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SFLPStateCB1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SFLPStateCB1, lv_pct(-18));
    lv_obj_set_y(ui_SFLPStateCB1, lv_pct(-37));
    lv_obj_set_align(ui_SFLPStateCB1, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_SFLPStateCB1, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_SFLPStateCB1, LV_OBJ_FLAG_CLICKABLE);      /// Flags

    ui_SFLPStateCB2 = lv_checkbox_create(ui_Screen1_Panel2);
    lv_checkbox_set_text(ui_SFLPStateCB2, "Overload");
    lv_obj_set_width(ui_SFLPStateCB2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SFLPStateCB2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SFLPStateCB2, lv_pct(-18));
    lv_obj_set_y(ui_SFLPStateCB2, lv_pct(-25));
    lv_obj_set_align(ui_SFLPStateCB2, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_SFLPStateCB2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_SFLPStateCB2, LV_OBJ_FLAG_CLICKABLE);      /// Flags

    ui_SFLPStateCB3 = lv_checkbox_create(ui_Screen1_Panel2);
    lv_checkbox_set_text(ui_SFLPStateCB3, "in DUVR");
    lv_obj_set_width(ui_SFLPStateCB3, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SFLPStateCB3, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SFLPStateCB3, lv_pct(-18));
    lv_obj_set_y(ui_SFLPStateCB3, lv_pct(-12));
    lv_obj_set_align(ui_SFLPStateCB3, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_SFLPStateCB3, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_SFLPStateCB3, LV_OBJ_FLAG_CLICKABLE);      /// Flags

    ui_SFLPStateCB4 = lv_checkbox_create(ui_Screen1_Panel2);
    lv_checkbox_set_text(ui_SFLPStateCB4, "ChargeProh.");
    lv_obj_set_width(ui_SFLPStateCB4, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SFLPStateCB4, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SFLPStateCB4, lv_pct(-18));
    lv_obj_set_y(ui_SFLPStateCB4, lv_pct(0));
    lv_obj_set_align(ui_SFLPStateCB4, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_SFLPStateCB4, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_SFLPStateCB4, LV_OBJ_FLAG_CLICKABLE);      /// Flags

    ui_SFLPStateCB5 = lv_checkbox_create(ui_Screen1_Panel2);
    lv_checkbox_set_text(ui_SFLPStateCB5, "dischrgProh.");
    lv_obj_set_width(ui_SFLPStateCB5, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SFLPStateCB5, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SFLPStateCB5, lv_pct(-18));
    lv_obj_set_y(ui_SFLPStateCB5, lv_pct(12));
    lv_obj_set_align(ui_SFLPStateCB5, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_SFLPStateCB5, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_SFLPStateCB5, LV_OBJ_FLAG_CLICKABLE);      /// Flags

    ui_SFLPStateCB6 = lv_checkbox_create(ui_Screen1_Panel2);
    lv_checkbox_set_text(ui_SFLPStateCB6, "full Dischrg");
    lv_obj_set_width(ui_SFLPStateCB6, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SFLPStateCB6, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SFLPStateCB6, lv_pct(-18));
    lv_obj_set_y(ui_SFLPStateCB6, lv_pct(25));
    lv_obj_set_align(ui_SFLPStateCB6, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_SFLPStateCB6, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_SFLPStateCB6, LV_OBJ_FLAG_CLICKABLE);      /// Flags

    ui_SFLPStateCB7 = lv_checkbox_create(ui_Screen1_Panel2);
    lv_checkbox_set_text(ui_SFLPStateCB7, "CapAccurate");
    lv_obj_set_width(ui_SFLPStateCB7, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SFLPStateCB7, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SFLPStateCB7, lv_pct(-18));
    lv_obj_set_y(ui_SFLPStateCB7, lv_pct(37));
    lv_obj_set_align(ui_SFLPStateCB7, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_SFLPStateCB7, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_SFLPStateCB7, LV_OBJ_FLAG_CLICKABLE);      /// Flags

    ui_SFLPStateCB8 = lv_checkbox_create(ui_Screen1_Panel2);
    lv_checkbox_set_text(ui_SFLPStateCB8, "Discharge");
    lv_obj_set_width(ui_SFLPStateCB8, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_SFLPStateCB8, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_SFLPStateCB8, lv_pct(-18));
    lv_obj_set_y(ui_SFLPStateCB8, lv_pct(50));
    lv_obj_set_align(ui_SFLPStateCB8, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_SFLPStateCB8, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_SFLPStateCB8, LV_OBJ_FLAG_CLICKABLE);      /// Flags

    ui_Screen1_Panel3 = lv_obj_create(ui_ScreenFL);
    lv_obj_set_width(ui_Screen1_Panel3, 130);
    lv_obj_set_height(ui_Screen1_Panel3, 254);
    lv_obj_set_y(ui_Screen1_Panel3, 0);
    lv_obj_set_x(ui_Screen1_Panel3, lv_pct(33));
    lv_obj_set_align(ui_Screen1_Panel3, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Screen1_Panel3, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Screen1_Label2 = lv_label_create(ui_Screen1_Panel3);
    lv_obj_set_width(ui_Screen1_Label2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Screen1_Label2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Screen1_Label2, 0);
    lv_obj_set_y(ui_Screen1_Label2, lv_pct(-50));
    lv_obj_set_align(ui_Screen1_Label2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Screen1_Label2, "Error");
    lv_obj_set_style_text_font(ui_Screen1_Label2, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_ScreenFLCBError0 = lv_checkbox_create(ui_Screen1_Panel3);
    lv_checkbox_set_text(ui_ScreenFLCBError0, "Critical");
    lv_obj_set_width(ui_ScreenFLCBError0, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFLCBError0, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScreenFLCBError0, lv_pct(-18));
    lv_obj_set_y(ui_ScreenFLCBError0, lv_pct(-37));
    lv_obj_set_align(ui_ScreenFLCBError0, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_ScreenFLCBError0, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ScreenFLCBError0, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_obj_set_style_text_color(ui_ScreenFLCBError0, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError0, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_ScreenFLCBError0, lv_color_hex(0xB50000), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError0, 255, LV_PART_MAIN | LV_STATE_CHECKED);

    ui_ScreenFLCBError1 = lv_checkbox_create(ui_Screen1_Panel3);
    lv_checkbox_set_text(ui_ScreenFLCBError1, "Low Temp");
    lv_obj_set_width(ui_ScreenFLCBError1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFLCBError1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScreenFLCBError1, lv_pct(-18));
    lv_obj_set_y(ui_ScreenFLCBError1, lv_pct(-25));
    lv_obj_set_align(ui_ScreenFLCBError1, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_ScreenFLCBError1, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ScreenFLCBError1, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_obj_set_style_text_color(ui_ScreenFLCBError1, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_ScreenFLCBError1, lv_color_hex(0x00E8FF), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError1, 255, LV_PART_MAIN | LV_STATE_CHECKED);

    ui_ScreenFLCBError2 = lv_checkbox_create(ui_Screen1_Panel3);
    lv_checkbox_set_text(ui_ScreenFLCBError2, "High Temp");
    lv_obj_set_width(ui_ScreenFLCBError2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFLCBError2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScreenFLCBError2, lv_pct(-18));
    lv_obj_set_y(ui_ScreenFLCBError2, lv_pct(-12));
    lv_obj_set_align(ui_ScreenFLCBError2, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_ScreenFLCBError2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ScreenFLCBError2, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_obj_set_style_text_color(ui_ScreenFLCBError2, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_ScreenFLCBError2, lv_color_hex(0xFF5E00), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError2, 255, LV_PART_MAIN | LV_STATE_CHECKED);

    ui_ScreenFLCBError3 = lv_checkbox_create(ui_Screen1_Panel3);
    lv_checkbox_set_text(ui_ScreenFLCBError3, "High Volt");
    lv_obj_set_width(ui_ScreenFLCBError3, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFLCBError3, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScreenFLCBError3, lv_pct(-18));
    lv_obj_set_y(ui_ScreenFLCBError3, lv_pct(0));
    lv_obj_set_align(ui_ScreenFLCBError3, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_ScreenFLCBError3, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ScreenFLCBError3, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_obj_set_style_text_color(ui_ScreenFLCBError3, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError3, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_ScreenFLCBError3, lv_color_hex(0xA92700), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError3, 255, LV_PART_MAIN | LV_STATE_CHECKED);

    ui_ScreenFLCBError4 = lv_checkbox_create(ui_Screen1_Panel3);
    lv_checkbox_set_text(ui_ScreenFLCBError4, "Low Volt");
    lv_obj_set_width(ui_ScreenFLCBError4, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFLCBError4, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScreenFLCBError4, lv_pct(-18));
    lv_obj_set_y(ui_ScreenFLCBError4, lv_pct(12));
    lv_obj_set_align(ui_ScreenFLCBError4, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_ScreenFLCBError4, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ScreenFLCBError4, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_obj_set_style_text_color(ui_ScreenFLCBError4, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_ScreenFLCBError4, lv_color_hex(0x76471E), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError4, 255, LV_PART_MAIN | LV_STATE_CHECKED);

    ui_ScreenFLCBError5 = lv_checkbox_create(ui_Screen1_Panel3);
    lv_checkbox_set_text(ui_ScreenFLCBError5, "ChargeProt");
    lv_obj_set_width(ui_ScreenFLCBError5, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFLCBError5, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScreenFLCBError5, lv_pct(-18));
    lv_obj_set_y(ui_ScreenFLCBError5, lv_pct(25));
    lv_obj_set_align(ui_ScreenFLCBError5, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_ScreenFLCBError5, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ScreenFLCBError5, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_obj_set_style_text_color(ui_ScreenFLCBError5, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError5, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_ScreenFLCBError5, lv_color_hex(0xB83F7F), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError5, 255, LV_PART_MAIN | LV_STATE_CHECKED);

    ui_ScreenFLCBError6 = lv_checkbox_create(ui_Screen1_Panel3);
    lv_checkbox_set_text(ui_ScreenFLCBError6, "Checksum");
    lv_obj_set_width(ui_ScreenFLCBError6, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFLCBError6, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScreenFLCBError6, lv_pct(-18));
    lv_obj_set_y(ui_ScreenFLCBError6, lv_pct(37));
    lv_obj_set_align(ui_ScreenFLCBError6, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_ScreenFLCBError6, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ScreenFLCBError6, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_obj_set_style_text_color(ui_ScreenFLCBError6, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError6, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_ScreenFLCBError6, lv_color_hex(0xFF8C00), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError6, 255, LV_PART_MAIN | LV_STATE_CHECKED);

    ui_ScreenFLCBError7 = lv_checkbox_create(ui_Screen1_Panel3);
    lv_checkbox_set_text(ui_ScreenFLCBError7, "Interrupt");
    lv_obj_set_width(ui_ScreenFLCBError7, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFLCBError7, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScreenFLCBError7, lv_pct(-18));
    lv_obj_set_y(ui_ScreenFLCBError7, lv_pct(50));
    lv_obj_set_align(ui_ScreenFLCBError7, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_ScreenFLCBError7, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_ScreenFLCBError7, LV_OBJ_FLAG_CLICKABLE);      /// Flags
    lv_obj_set_style_text_color(ui_ScreenFLCBError7, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError7, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_ScreenFLCBError7, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_text_opa(ui_ScreenFLCBError7, 255, LV_PART_MAIN | LV_STATE_CHECKED);

    ui_Screen1_Label3 = lv_label_create(ui_ScreenFL);
    lv_obj_set_width(ui_Screen1_Label3, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Screen1_Label3, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Screen1_Label3, lv_pct(0));
    lv_obj_set_y(ui_Screen1_Label3, lv_pct(-34));
    lv_obj_set_align(ui_Screen1_Label3, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Screen1_Label3, "Forumslader");
    lv_obj_set_style_text_font(ui_Screen1_Label3, &lv_font_montserrat_36, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Screen1_Bar1 = lv_bar_create(ui_ScreenFL);
    lv_bar_set_value(ui_Screen1_Bar1, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_Screen1_Bar1, 340);
    lv_obj_set_height(ui_Screen1_Bar1, 20);
    lv_obj_set_x(ui_Screen1_Bar1, 0);
    lv_obj_set_y(ui_Screen1_Bar1, 160);
    lv_obj_set_align(ui_Screen1_Bar1, LV_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_Screen1_Bar1, lv_color_hex(0x331111), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen1_Bar1, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Screen1_Bar1, lv_color_hex(0x113311), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_Screen1_Bar1, LV_GRAD_DIR_HOR, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_bg_color(ui_Screen1_Bar1, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen1_Bar1, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_Screen1_Bar1, lv_color_hex(0x00FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_Screen1_Bar1, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_Screen1_Bar1, 220, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_Screen1_Bar1, LV_GRAD_DIR_HOR, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    ui_Screen1_Label4 = lv_label_create(ui_ScreenFL);
    lv_obj_set_width(ui_Screen1_Label4, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Screen1_Label4, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Screen1_Label4, lv_pct(0));
    lv_obj_set_y(ui_Screen1_Label4, lv_pct(42));
    lv_obj_set_align(ui_Screen1_Label4, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Screen1_Label4, "n/a");
    lv_obj_set_style_text_font(ui_Screen1_Label4, &lv_font_montserrat_28, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Screen1_Label5 = lv_label_create(ui_ScreenFL);
    lv_obj_set_width(ui_Screen1_Label5, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Screen1_Label5, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Screen1_Label5, lv_pct(0));
    lv_obj_set_y(ui_Screen1_Label5, lv_pct(-43));
    lv_obj_set_align(ui_Screen1_Label5, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Screen1_Label5, "--:--:--\nxxxx-xx-xx");
    lv_obj_set_style_text_align(ui_Screen1_Label5, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_Screen1_Label5, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Bar2: Verbraucher 0-10'000mW - Pos_mitte: (0,0)
    ui_ScreenFL_Bar2 = lv_bar_create(ui_ScreenFL);
    lv_bar_set_range(ui_ScreenFL_Bar2, 0, 10000);
    lv_bar_set_value(ui_ScreenFL_Bar2, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_ScreenFL_Bar2, 20);
    lv_obj_set_height(ui_ScreenFL_Bar2, lv_pct(50));
    lv_obj_set_align(ui_ScreenFL_Bar2, LV_ALIGN_CENTER);

    lv_obj_set_style_bg_color(ui_ScreenFL_Bar2, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ScreenFL_Bar2, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_ScreenFL_Bar2, lv_color_hex(0xA5FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_ScreenFL_Bar2, LV_GRAD_DIR_VER, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    // Bar1: Dynamo 0 - 10'000mW - Pos_mitte: (-10%,0)
    ui_ScreenFL_Bar1 = lv_bar_create(ui_ScreenFL);
    lv_bar_set_range(ui_ScreenFL_Bar1, 0, 10000);
    lv_bar_set_value(ui_ScreenFL_Bar1, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_ScreenFL_Bar1, 20);
    lv_obj_set_height(ui_ScreenFL_Bar1, lv_pct(50));
    lv_obj_set_y(ui_ScreenFL_Bar1, 0);
    lv_obj_set_x(ui_ScreenFL_Bar1, lv_pct(-10));
    lv_obj_set_align(ui_ScreenFL_Bar1, LV_ALIGN_CENTER);

    lv_obj_set_style_bg_color(ui_ScreenFL_Bar1, lv_color_hex(0x2BFF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ScreenFL_Bar1, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_ScreenFL_Bar1, lv_color_hex(0x0059FF), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_main_stop(ui_ScreenFL_Bar1, 100, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_stop(ui_ScreenFL_Bar1, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_ScreenFL_Bar1, LV_GRAD_DIR_VER, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    // Bar3: Strom -1'000mA - 1'000mA - Pos_mitte: (10%,0)
    ui_ScreenFL_Bar3 = lv_bar_create(ui_ScreenFL);
    lv_bar_set_mode(ui_ScreenFL_Bar3, LV_BAR_MODE_SYMMETRICAL);
    lv_bar_set_range(ui_ScreenFL_Bar3, -1000, 1000);
    lv_bar_set_value(ui_ScreenFL_Bar3, 0, LV_ANIM_OFF);
    lv_obj_set_width(ui_ScreenFL_Bar3, 20);
    lv_obj_set_height(ui_ScreenFL_Bar3, lv_pct(50));
    lv_obj_set_y(ui_ScreenFL_Bar3, 0);
    lv_obj_set_x(ui_ScreenFL_Bar3, lv_pct(10));
    lv_obj_set_align(ui_ScreenFL_Bar3, LV_ALIGN_CENTER);

    lv_obj_set_style_bg_color(ui_ScreenFL_Bar3, lv_color_hex(0x00FF00), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ScreenFL_Bar3, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(ui_ScreenFL_Bar3, lv_color_hex(0xFF0000), LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(ui_ScreenFL_Bar3, LV_GRAD_DIR_VER, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    // Label5: Dynamo-Leistung Pos_mitte: (-50,135);
    ui_ScreenFL_Label5 = lv_label_create(ui_ScreenFL);
    lv_obj_set_width(ui_ScreenFL_Label5, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFL_Label5, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScreenFL_Label5, -50);
    lv_obj_set_y(ui_ScreenFL_Label5, 135);
    lv_obj_set_align(ui_ScreenFL_Label5, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ScreenFL_Label5, "n/a");
    lv_obj_set_style_text_align(ui_ScreenFL_Label5, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_ScreenFL_Label5, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Label1: Batterie-Strom Pos_mitte: (59,135);
    ui_ScreenFL_Label1 = lv_label_create(ui_ScreenFL);
    lv_obj_set_width(ui_ScreenFL_Label1, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFL_Label1, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScreenFL_Label1, 59);
    lv_obj_set_y(ui_ScreenFL_Label1, 135);
    lv_obj_set_align(ui_ScreenFL_Label1, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ScreenFL_Label1, "n/a");
    lv_obj_set_style_text_align(ui_ScreenFL_Label1, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_ScreenFL_Label1, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Label2: Verbraucher-Leistung Pos_mitte: (7,-135);
    ui_ScreenFL_Label2 = lv_label_create(ui_ScreenFL);
    lv_obj_set_width(ui_ScreenFL_Label2, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFL_Label2, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_ScreenFL_Label2, 7);
    lv_obj_set_y(ui_ScreenFL_Label2, -135);
    lv_obj_set_align(ui_ScreenFL_Label2, LV_ALIGN_CENTER);
    lv_label_set_text(ui_ScreenFL_Label2, "n/a");
    lv_obj_set_style_text_align(ui_ScreenFL_Label2, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_ScreenFL_Label2, &lv_font_montserrat_24, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_ScreenFL_Label6 = lv_label_create(ui_ScreenFL);
    lv_obj_set_width(ui_ScreenFL_Label6, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFL_Label6, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_y(ui_ScreenFL_Label6, 0);
    lv_obj_set_x(ui_ScreenFL_Label6, lv_pct(-4));
    lv_obj_set_align(ui_ScreenFL_Label6, LV_ALIGN_CENTER);
    lv_obj_set_style_transform_angle(ui_ScreenFL_Label6, 900, 0);
    lv_label_set_text(ui_ScreenFL_Label6, "Dynamo-W");


    ui_ScreenFL_Label4 = lv_label_create(ui_ScreenFL);
    lv_obj_set_width(ui_ScreenFL_Label4, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFL_Label4, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_y(ui_ScreenFL_Label4, 0);
    lv_obj_set_x(ui_ScreenFL_Label4, lv_pct(8));
    lv_obj_set_align(ui_ScreenFL_Label4, LV_ALIGN_CENTER);
    lv_obj_set_style_transform_angle(ui_ScreenFL_Label4, 900, 0);
    lv_label_set_text(ui_ScreenFL_Label4, "Verbraucher-W");


    ui_ScreenFL_Label3 = lv_label_create(ui_ScreenFL);
    lv_obj_set_width(ui_ScreenFL_Label3, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_ScreenFL_Label3, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_y(ui_ScreenFL_Label3, 0);
    lv_obj_set_x(ui_ScreenFL_Label3, lv_pct(14));
    lv_obj_set_align(ui_ScreenFL_Label3, LV_ALIGN_CENTER);
    lv_obj_set_style_transform_angle(ui_ScreenFL_Label3, 900, 0);
    lv_label_set_text(ui_ScreenFL_Label3, "Batterie-A");

    lv_obj_add_event_cb(ui_ScreenFL, ui_event_ScreenFL, LV_EVENT_ALL, NULL);

}
