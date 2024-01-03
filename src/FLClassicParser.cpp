/*
 * FLClassicParser.cpp
 *
 *  Created on: 19.02.2023
 *      Author: ian
 */

#include <FLClassicParser.h>
#include "Singletons.h"


FLClassicParser::FLClassicParser(uint8_t polePair, uint16_t circum_mm):
dist_m_per_pulse((float)circum_mm/(float)polePair / 1000.0),
kmh_per_pulse_per_s(dist_m_per_pulse * 3600 / 1000.0),
lastUpdate(millis())
{

}

void FLClassicParser::setConnState(EFLConnState state) {
	cstate = state;
	if (stateCB) stateCB(cstate, flags_last, 0);
}


void FLClassicParser::updateFromString(const String &flStr) {
#if BC_FL_SUPPORT
	lastUpdate = millis();
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_FL, "Rvcd string: %s", flStr.c_str());

	int8_t scanCt = 0;
	float speed_f = NAN;
	uint8_t scanFLC_id;
	uint32_t scanFLC_buffer[5];

	if (flStr.startsWith("$FL")) {
		uint16_t pulses_per_s, temperature, pressure, height, gradient;

		int8_t cons_on_off;
		int8_t	stufe;			// -1: Dauer-Aus, 0: aus, 1-4: nach Geschwindigkeit

		int32_t timecounter;
		int32_t pulsecounter;
		int32_t micropulsecounter;
		uint16_t batterie[3];	// in mV

		int16_t  batt_current;	// in mA
		int16_t  cons_current;  // in mA
		int16_t int_temp;		// in 1/10°C (or K?)
		int16_t timeout;

		uint32_t flags;
		uint32_t hm_total;
		uint32_t dist_total;

		//Forumslader V5 uses $FL5, $FLB and $FLC only
		switch(flStr.charAt(3)) { //                                                                                                                 Strom in mA? // Verbraucherstrom int. Temp Verbraucher Timeout
		case '5':  // $FL5,08c800,0,0,4158,4161,4162,-18,0,294,1,233,3679,13231,25897;                        $FL5,08c800,     0,        0,       4158,        4161,        4162,       -18,         0,           294,        1,          233,        3679,             13231,         25897;
			scanCt = sscanf(flStr.c_str(), "$FL5,%lx,%hhd,%hd,%hd,%hd,%hd,%hd,%hd,%hd,%hhd,%hd,%d,%d,%d\n", &flags, &stufe, &pulses_per_s, &batterie[0],&batterie[1],&batterie[2],&batt_current,&cons_current,&int_temp,&cons_on_off,&timeout,&micropulsecounter,&pulsecounter,&timecounter);
			if (scanCt != 14) Serial.println("❌ Not all fields scanned");

			if (flags != flags_last) {
				Serial.printf("Flags changed: %x\n", flags);
				if (stateCB) stateCB(FL_STATE_CONNECTED, flags, timeout);
				flags_last=flags;
			}
			if (batCB) {
				//Note batt_perc and batt_fullCap are not updated in $FL5 string, but change slowly, so use last value
				//    (uint16_t batVoltage, uint8_t batPerc, int8_t powerStage, int16_t CurBat, int16_t CurConsumer, bool ConsumerOn)> FLBatUpdateHandler;
				batCB(batterie[0]+batterie[1]+batterie[2], batt_perc, batt_fullCap, stufe, batt_current, cons_current, cons_on_off);
			}
			speed_f = pulses_per_s * kmh_per_pulse_per_s;
			dist_total = ceil((pulsecounter * 4096 + micropulsecounter) * dist_m_per_pulse);
			stats.addSpeed(speed_f);
			stats.updateDistance(dist_total, pulsecounter * 4096 + micropulsecounter);
			stats.addDistance(dist_total, Statistics::SUM_FL_TOTAL);
			if (speedCB) speedCB(speed_f, dist_total);
			break;
		case 'B': // $FLB,850,98591,2731,0;
			scanCt = sscanf(flStr.c_str(), "$FLB,%hd,%d,%hd,%hd\n", &temperature, &pressure,&height,&gradient);
			if (scanCt != 4) Serial.println("❌ Not all fields scanned");
			if (envCB) {
				envCB(temperature, pressure, height, gradient);
			}
			stats.addGradient(gradient, height);
			//Serial.printf("[%x] Temp: %.02f\tPressure: %.01f\tHeight: %.01f\tGradient: %d\n", scanCt, temperature/10.0, pressure/100.0, height/10.0, gradient);
			break;
		case 'C':
			scanCt = sscanf(flStr.c_str(), "$FLC,%hhd,%d,%d,%d,%d,%d\n", &scanFLC_id, &scanFLC_buffer[0],&scanFLC_buffer[1],&scanFLC_buffer[2],&scanFLC_buffer[3],&scanFLC_buffer[4]);
			switch (scanFLC_id) {
			case 0: // $FLC,0,0,0,200,3798,26;
				//Tour: Höhenmeter Total, Tour Steigung Max, Tour Temperatur Max, Tour Höhe Max, Tour Pulse Max
				stats.addDistance(scanFLC_buffer[4], Statistics::SUM_FL_TOUR);
				//stats.updateFLStats();
				break;
			case 1: // $FLC,1,0,0,200,3798,26;
				//Trip: Höhenmeter Total, Tour Steigung Max, Tour Temperatur Max, Tour Höhe Max, Tour Pulse Max
				stats.addDistance(scanFLC_buffer[4], Statistics::SUM_FL_TRIP);
				break;
			case 2: // $FLC,2,478561,0,200,0,200;
				// Mixed: Höhenmeter Total Toal, Tour Steigung Min, Tour Temperatur Min, Trip Steigung Min, Trip Temperatur Min
				break;
			case 3: // $FLC,3,8126,0,0,1799,0;
				// Total Energy, Tour Energy, Trip Energy, BT Save Count, -reserved-
				break;
			case 4: // $FLC,4,48,48,0,0,0;
				// Averages: Trip, Tour, Trip Climb, Tour Clim, -reserved-
				break;
			case 5: // $FLC,5,1870,100,1667,36,73053775;   $FLC,5,1921,99,1667,37,74684176;  // StartCounter, Batt%, fullCap in mAh, CycleCount, Acc CCADCvalue
				batt_perc = scanFLC_buffer[1];
				batt_fullCap = scanFLC_buffer[2];
				break;
			}
			break;
		case 'V':	// $FLV,500290515,5.01;  // Version Info
			break;
		default:
			Serial.printf("❌ Unknown FL-ID '%c'\n", flStr.charAt(3));
		}
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_RAW_NMEA, flStr);

	} else {
		Serial.println("❌ Unknown String identifier");
	}
#endif
}
