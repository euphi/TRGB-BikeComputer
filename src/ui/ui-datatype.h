/*
 * ui-datatype.h
 *
 *  Created on: 05.03.2023
 *      Author: ian
 */

#pragma once

enum EUIDataType {
	UIDT_CLOCK = 0,
	UIDT_SPEED,
	UIDT_SPEED_AVG,
	UIDT_CADENCE,
	UIDT_HR,
	UIDT_DIST,
	UIDT_GRADIENT,
	UIDT_HEIGHT,
	UIDT_TEMP,

	UI_DT_NAV_DIST = 32,
	UI_DT_NAV_TEXT,
	UI_DT_NAV_DIR,


	UIDT_FL_BAT=64,
	UIDT_FL_DYN_STAGE,
	UIDT_FL_DYN_POWER,
	UIDT_FL_CON_POWER

};

