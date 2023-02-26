/*
 * FLClassicFlags.h
 *
 *  Created on: 19.02.2023
 *      Author: ian
 */

#pragma once


enum FL_flags {
		FL_FLAG_BALANCE1       =  0 , // (1 << 7) << 16,
		FL_FLAG_BALANCE2       =  1 , // (1 << 6) << 16,
		FL_FLAG_BALANCE3       =  2 , // (1 << 5) << 16,
		FL_FLAG_SHORTCIRCUIT   =  3 , // (1 << 4) << 16,
		FL_FLAG_DISCHARGE_OVR  =  4 , // (1 << 3) << 16,
		FL_FLAG_CHARGE_OVR     =  5 , // (1 << 2) << 16,
		FL_FLAG_DISCHARGE_HGH  =  6 , // (1 << 1) << 16,
		FL_FLAG_CHARGE_HGH     =  7 , // (1 << 0) << 16,

		FL_FLAG_OVPWRRED       =  8 , // (1 << 7) << 8,
		FL_FLAG_OVERLOAD       =  9 , // (1 << 6) << 8, --> 2
		FL_FLAG_IN_DUVR        = 10 , // (1 << 5) << 8,
		FL_FLAG_CHARGE_INH     = 11 , // (1 << 4) << 8,
		FL_FLAG_DISCHARGE_INH  = 12 , // (1 << 3) << 8,
		FL_FLAG_FULL_DISCHARGE = 13 , // (1 << 2) << 8,
		FL_FLAG_CAPACITY_ACC   = 14 , // (1 << 1) << 8,
		FL_FLAG_DISCHARGE      = 15 , // (1 << 0) << 8,

		FL_ERR_CRITICAL        = 16, // (1 << 7),
		FL_ERR_CELL_TEMP_LOW   = 17, // (1 << 6),
		FL_ERR_CELL_TEMP_HIGH  = 18, // (1 << 5),
		FL_ERR_VOLTAGE_HIGH    = 19, // (1 << 4),
		FL_ERR_VOLTAGE_LOW     = 20, // (1 << 3),
		FL_ERR_CHARGE_PROT     = 21, // (1 << 2),
		FL_ERR_CHECKSUM        = 22, // (1 << 1),
		FL_ERR_SYSTEM_IRQ      = 23, // (1 << 0)
		FL_FLAG_MAX = 24
};


/* old wrong byte order
enum FL_flags {
		FL_FLAG_MAX = 24,
		FL_FLAG_BALANCE1       =  23 , // (1 << 7) << 16,
		FL_FLAG_BALANCE2       =  22  , // (1 << 6) << 16,
		FL_FLAG_BALANCE3       =  21  , // (1 << 5) << 16,
		FL_FLAG_SHORTCIRCUIT   =  20  , // (1 << 4) << 16,
		FL_FLAG_DISCHARGE_OVR  =  19  , // (1 << 3) << 16,
		FL_FLAG_CHARGE_OVR     =  18  , // (1 << 2) << 16,
		FL_FLAG_DISCHARGE_HGH  =  17  , // (1 << 1) << 16,
		FL_FLAG_CHARGE_HGH     =  16  , // (1 << 0) << 16,

		FL_FLAG_OVPWRRED       =  15  , // (1 << 7) << 8,
		FL_FLAG_OVERLOAD       =  14  , // (1 << 6) << 8, --> 2
		FL_FLAG_IN_DUVR        =  13  , // (1 << 5) << 8,
		FL_FLAG_CHARGE_INH     =  12  , // (1 << 4) << 8,
		FL_FLAG_DISCHARGE_INH  =  11  , // (1 << 3) << 8,
		FL_FLAG_FULL_DISCHARGE =  10  , // (1 << 2) << 8,
		FL_FLAG_CAPACITY_ACC   =  9   , // (1 << 1) << 8,
		FL_FLAG_DISCHARGE      =  8   , // (1 << 0) << 8,

		FL_ERR_CRITICAL        =  7  , // (1 << 7),
		FL_ERR_CELL_TEMP_LOW   =  6  , // (1 << 6),
		FL_ERR_CELL_TEMP_HIGH  =  5  , // (1 << 5),
		FL_ERR_VOLTAGE_HIGH    =  4  , // (1 << 4),
		FL_ERR_VOLTAGE_LOW     =  3  , // (1 << 3),
		FL_ERR_CHARGE_PROT     =  2  , // (1 << 2),
		FL_ERR_CHECKSUM        =  1  , // (1 << 1),
		FL_ERR_SYSTEM_IRQ      =  0  , // (1 << 0)
};
*/
