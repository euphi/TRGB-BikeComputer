/*
 * Distance.cpp
 *
 *  Created on: 21.01.2024
 *      Author: ian
 */

/* Note on a decision to use float or uint32_t for total distance: (Text created by ChatGPT):

     Floating-point numbers, such as the float data type, provide a flexible representation of real numbers by sacrificing precision for a wider range.
     In the case of a 32-bit float, it dedicates a limited number of bits to represent the fractional part (mantissa), leading to decreasing precision
     as the magnitude of the number increases. This loss of precision becomes noticeable after a certain threshold.

     For larger distances traveled, the limited precision of a 32-bit float results in larger intervals between representable values. Consequently,
     the difference between consecutive float values larger than 650km becomes greater than 10cm, compromising the accuracy needed for precise
     measurements for gradients in a bike computer.

     Alternative data types, like uint32_t , also pose challenges, as they can't represent distances exceeding 42,950 kilometers when the least
     significant bit (LSB) corresponds to 1cm.
 */

#include <Stats/Distance.h>
#include <ArduinoJson.h>
#include <Singletons.h>

Distance::Distance() {

}

void Distance::setup() {
	loadDistanceForBikeIdx(0);
	distanceStore.attach(60, +[](Distance *thisInstance) {thisInstance->store();}, this);
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "setup DistanceHandler with stored values: Total distance: %.2fm\tRevs: %d\tCircumference: %.04fm",
			distanceFromNVS[Statistics::SUM_ESP_TOTAL], revsFromNVS[Statistics::SUM_ESP_TOTAL], wheel_c);
	setupWebserver();
}

void Distance::loadDistanceForBikeIdx(uint8_t idx) {
	if (! (idx < bikecount)) {
		bclog.logf(BCLogger::Log_Error, BCLogger::TAG_STAT, "❌ Try to load distance from NVS for invalid bikecount idx %d (count %d).", idx, bikecount);
		idx = 0;	//To avoid instable behavior due to unitialized data fallback to idx 0
	}
	currentBikeIdx = idx;
	//ESP32 NVS Storage
	Preferences params;											// wheel circumference (other params could be added later)
	Preferences storedDist; //[Statistics::SUM_ESP_TRIP + 1];		// Distance (in m and delta as revs) for TOTAL, TOUR, TRIP
	String idx_str(idx);
	params.begin((String("DIST_PARAMS_") + idx_str).c_str(), true);
	wheel_c = params.getFloat("wheel_circ", 2.220);
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Loaded wheel circumference from NVS: %.04fm", wheel_c);
	params.end();
	for (uint_fast8_t j=0; j <= Statistics::SUM_ESP_TRIP; j++) {
		String prefString = String("DIST_") + idx_str + String("_") + String(Statistics::SUM_TYPE_STRING[j]+3);
		storedDist.begin(prefString.c_str(), true);		// +3 to skip first three chars "ST_"
		distanceFromNVS[j]  = storedDist.getFloat("total", 0.0);		// Total distance in m (as float). It is only updated sporadically, so the actual total distance is total + (revs * wheel_circ).
		lostDistanceFromNVS[j] = storedDist.getFloat("lost_total", 0.0);	// Total distance lost
		if (isnan(distanceFromNVS[j])) {
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "%s: Loaded distance from NVS invalid (NAN) - setting to zero.", Statistics::SUM_TYPE_STRING[j]);
			distanceFromNVS[j] = 0.0;
		}
		revsFromNVS[j] = storedDist.getULong("revs", 0);
		storedDist.end();
		curTotalDistance[j] = distanceFromNVS[j];
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "%s: Loaded %s total distance %.2fm at %d revs.",
				Statistics::SUM_TYPE_STRING[j], prefString.c_str(), distanceFromNVS[j], revsFromNVS[j]);
	}
}

void Distance::updateRevs(uint32_t revs, uint16_t timestamp) {
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "%d revs at %d ticks", revs, timestamp);

	/* Scenario:
	 * 1. First called after startup, revs > 0
	 *    --> Start must initialize to revs. If no value (=0) is stored for TOTAL/TOUR/TRIP, also set to revs
	 *    --> Also the lastRevs must be updated to revs, to avoid a large spike in speed for first calculation --> done in Scenerio 2 which is also triggered
	 *    --> Also lost revs must be calculated, but different to scenario 2.
	 *
	 *    === Trigger: lastRevs == 0
	 *    */
	if (lastRevs == 0) {  // Scenario 1, in Scen1 also Scenario 2 is triggered, so it shall not interfere with Scen2 calculations
		bclog.log(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Initialize revs counters");
		for (uint_fast8_t j = 0; j <= Statistics::SUM_ESP_START; j++) {
			// If stored rev value is zero and the last known value is also zero, assume it is a reset.
			//    --> After startup, this is always the case for SUM_ESP_START
			if (revsFromNVS[j] == 0) {
				revsFromNVS[j] = revs;
			} else if (revsFromNVS[j] < revs) {
				uint32_t lostRevs = revs - revsFromNVS[j];
				float lostDist = lostRevs * wheel_c;
				lostDistanceFromNVS[j] += lostDist;
				bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Lost distance for %s while switched off: %d revs -> %.1f m --> total %.1f",
						(Statistics::SUM_TYPE_STRING[j] + 3), lostRevs, lostDist, lostDistanceFromNVS[j]);
			} else {
				bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "No distance lost in disconnect: %d = %d", revs, revsFromNVS[j]);
			}
		}
	}
	/* Scenario:
	 * 2. Reconnect after connection loss
	 *    a) revs do not reset
	 *       --> TOTAL/TOUR/TRIP shall be updated with "lost" distance
	 *
	 *    b) revs are reset
	 *       --> Total distance must be updated with old values
	 *
	 *    === TRIGGER: revs received, but not connected
 	 */
	if (revs < lastRevs) {	// Scenario 2 b
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Received revs %d smaller than stored (%d). New CSC sensor?", revs, lastRevs);
		// My cheap CYCPLUS CSC sensor does not store cumulative wheel revs. Every reconnect it starts with 0.. To mitigate impact, store the distance and ignore the update.
		// Note: The Magene CSC sensors (that cost a little bit more, but are still quite cheap) do not show this behaviour.
		lastRevs = revs;
		storeDistanceAndResetRevs(true);
		//TODO: Show message box and let user decide if to update
	}
	// Scenario 2 a & b (in case of b, lastrevs is already set to revs, so delta is 0.
	if (!stats.isConnected()) {
		if (lastRevs > 0) {		// Not in Scenario 1, because Scenario 1 has own logic for lost distance
			int32_t lostRevs = revs - lastRevs;
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Reconnect: Lost %d revs of distance.", lostRevs);
			if (lostRevs < 0) lostRevs = 0;
			updateLostRevs(lostRevs);
		}
		lastRevs = revs;	// no spike in speed
		stats.setConnected(true);
	}

	/* Scenario
	 * 3. Normal Update
	 */
	for (uint_fast8_t j=0; j <= Statistics::SUM_ESP_START; j++) {
		curTotalDistance[j] = ( (j == Statistics::SUM_ESP_START) ? 0.0 : distanceFromNVS[j] ) + ( (revs - revsFromNVS[j]) * wheel_c);
	}
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "New distance in total: %.2fm\tTour: %.2fm\tTrip: %.2fm\tStart: %.2fm",
			curTotalDistance[Statistics::SUM_ESP_TOTAL], curTotalDistance[Statistics::SUM_ESP_TOUR], curTotalDistance[Statistics::SUM_ESP_TRIP], curTotalDistance[Statistics::SUM_ESP_START]);

	float newSpeed = calculateSpeed(revs-lastRevs, timestamp-lastTimestamp);
	if (!isnan(newSpeed)) {
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "New speed %.2f", newSpeed);
		stats.addSpeed(newSpeed);
	}

	stats.checkDistance(curTotalDistance[Statistics::SUM_ESP_START]);
	lastTimestamp = timestamp;
	lastRevs = revs;
}

void Distance::updateLostRevs(const uint32_t lostRevs) {
	float lostDist = lostRevs * wheel_c;
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Lost distance while disconnects %d revs -> %.1f m", lostRevs, lostDist);
	for (uint_fast8_t j=0; j <= Statistics::SUM_ESP_START; j++) {
		lostDistanceFromNVS[j] += lostDist;
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Lost distance %s now %.1f m", (Statistics::SUM_TYPE_STRING[j] + 3), lostDistanceFromNVS[j]);
	}
}

void Distance::store() {
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Ticker: Store in Preferences: %d revs.", lastRevs);
	// To minimize added up floating point error, just store, but continue to use values as loaded at startup.
	//    --> Error only adds up once per restart. Error is smaller than half wheel_c till approx 6500km total distance.
	storeDistanceAndResetRevs(false);
}

float Distance::calculateSpeed(const uint32_t revs, const uint16_t duration) {
	if (revs > 0) {
		last_speedUpdate = millis();
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Calculate new speed: %d * %.4f * 1024 * 3.6 / %d", revs, wheel_c, duration);
		//Timestamp (duration) is 1024 Ticks per sec
		// km/h     1        m  * tick/s * (km/h / m/s)  / tick
		return (revs * wheel_c * 1024 * 3.6) / duration;
	} else {
		if (millis() - last_speedUpdate > 1200) {
			return 0.0;
		} else {
			bclog.log(BCLogger::Log_Info, BCLogger::TAG_STAT, "Ignore 0 revolutions (speed) since last update is less than 1200ms");
			return NAN;
		}
	}
}

bool Distance::updateWheelCirc(const float circ_in_m) {
	// Gracious Plausibility check
	if (circ_in_m < 0.2 || circ_in_m > 5) {
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Try to set implausible value for wheel circumference: %.4f", circ_in_m);
		return false;
	}
	wheel_c = circ_in_m;
	Preferences params;											// wheel circumference (other params could be added later)
	String idx_str(currentBikeIdx);
	params.begin((String("DIST_PARAMS_") + idx_str).c_str(), false);
	bool ok = true;
	if (params.putFloat("wheel_circ", wheel_c) < 4) {
		bclog.logf(BCLogger::Log_Error, BCLogger::TAG_STAT, "Cannot write to NVS to store wheel circ for %s", idx_str.c_str());
		ok = false;
	} else {
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Wheel circumference for bike index %d updated to %.4f", currentBikeIdx, circ_in_m);
	}
	params.end();
	return ok;
}

void Distance::resetDistToZero(Statistics::ESummaryType eSummaryType) {
	if (eSummaryType < Statistics::SUM_ESP_TOTAL || eSummaryType > Statistics::SUM_ESP_START) {
		bool expected = (eSummaryType == Statistics::SUM_ESP_TOTAL) || (eSummaryType == Statistics::SUM_ESP_START); 	//  if user presses reset in TOTAL or START
		bclog.logf(expected ? BCLogger::Log_Debug : BCLogger::Log_Error, BCLogger::TAG_STAT, "Try to reset invalid Sum-Type 0x%04x", eSummaryType);
		return;
	}
	curTotalDistance[eSummaryType] = 0;
	distanceFromNVS[eSummaryType] = 0;
	revsFromNVS[eSummaryType] = lastRevs;
	storeDistanceAndResetRevs();
	stats.reset(eSummaryType);
}

void Distance::storeDistanceAndResetRevs(bool resetRevs) {
	Preferences storedDist; // Distance (in m and delta as revs) for TOTAL, TOUR, TRIP
	String idx_str(currentBikeIdx);
	for (uint_fast8_t j=0; j <= Statistics::SUM_ESP_TRIP; j++) {
		String prefString = String("DIST_") + idx_str + String("_") + String(Statistics::SUM_TYPE_STRING[j]+3); // +3 to skip first three chars "ST_"
		storedDist.begin(prefString.c_str(), false);
		size_t bytes = storedDist.putFloat("total", curTotalDistance[j]);		// Total distance in m (as float). It is only updated sporadically, so the actual total distance is total + (revs * wheel_circ).
		bytes += storedDist.putFloat("lost_total", lostDistanceFromNVS[j]);
		bytes += (lastRevs > 0)  ? storedDist.putULong("revs", lastRevs) : 4;	// only save lastRevs if there are received from sensor (and thus > 0).
		storedDist.end();
		if (bytes < 12) {
			bclog.logf(BCLogger::Log_Error, BCLogger::TAG_STAT, "Cannot write to NVS to store wheel circ for %s", prefString.c_str());
		} else {
			bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Updated NVS for %s", prefString.c_str());
		}
		//	It seems better to not reset revs, so floating point addition errors do not sum up. However, in some case it is necessary, e.g. if CSC resets its revs
		if (resetRevs) {
			bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "%s: Reset revs", Statistics::SUM_TYPE_STRING[j]);
			distanceFromNVS[j] = curTotalDistance[j];
			revsFromNVS[j] = lastRevs;
		}
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "%s: Stored total distance %.2fm at %d revs", Statistics::SUM_TYPE_STRING[j], curTotalDistance[j], lastRevs);
	}
	if (resetRevs) {
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "%s: Reset revs", Statistics::SUM_TYPE_STRING[Statistics::SUM_ESP_START]);
		revsFromNVS[Statistics::SUM_ESP_START] = lastRevs;
	}
}


void Distance::setupWebserver() {
	webserver.getServer().on("/stat/getDistanceInfo", HTTP_GET, [this](AsyncWebServerRequest *request) {
		JsonDocument jsonDoc;
		jsonDoc["overall"]["revs"] = lastRevs;
		for (uint_fast8_t j = 0; j <= Statistics::SUM_ESP_START; j++) {
			Preferences storedDist;
			String typeString = String(Statistics::SUM_TYPE_STRING[j] + 3);
			String prefString = String("DIST_0_") + String(Statistics::SUM_TYPE_STRING[j]+3);
			storedDist.begin(prefString.c_str(), true);		// +3 to skip first three chars "ST_"
			float sDist = storedDist.getFloat("total", 0.0);		// Total distance in m (as float). It is only updated sporadically, so the actual total distance is total + (revs * wheel_circ).
			uint32_t sRev = storedDist.getULong("revs", 0);
			storedDist.end();
			jsonDoc[typeString]["storedTotal"] = sDist;
			jsonDoc[typeString]["storedRevs"] = sRev;
			jsonDoc[typeString]["actualDistance"] = curTotalDistance[j];
			jsonDoc[typeString]["totalDistance"] = (j == Statistics::SUM_ESP_START) ? 0.0 : distanceFromNVS[j];
			jsonDoc[typeString]["deltaRevs"] = lastRevs - revsFromNVS[j];
		}
		String jsonData;
		serializeJson(jsonDoc, jsonData);
		request->send(200, "application/json", jsonData);
	});
	// Endpoint for getting distance
	webserver.getServer().on("/stat/getDistance", HTTP_GET, [this](AsyncWebServerRequest *request) {
		JsonDocument jsonDoc;
		for (uint_fast8_t j = 0; j <= Statistics::SUM_ESP_TRIP; j++) {
			String typeString = String(Statistics::SUM_TYPE_STRING[j] + 3);
			jsonDoc[typeString]["actualDistance"] = curTotalDistance[j];
		}
		String jsonData;
		serializeJson(jsonDoc, jsonData);
		request->send(200, "application/json", jsonData);
	});
	// Endpoint for resetting distance
	webserver.getServer().on("/stat/resetDistance", HTTP_GET, [this](AsyncWebServerRequest *request) {
		String mode = request->arg("mode");
		if (mode == "TOUR") {
			resetDistToZero(Statistics::SUM_ESP_TOUR);
		} else if (mode == "TRIP") {
			resetDistToZero(Statistics::SUM_ESP_TRIP);
#ifdef DEBUG_APP
		} else if (mode=="TOTAL") {
			resetDistToZero(Statistics::SUM_ESP_TOTAL);
#endif
		} else {
			request->send(400, "text/plain", "Unknown distance type");
			return;
		}
		request->send(200, "text/plain", "Distance reset successful");
	});
	  // Handler for updating total distance and wheel data
	webserver.getServer().on("/stat/setDistanceData", HTTP_GET, [this](AsyncWebServerRequest *request) {
		if (request->hasParam("type") && request->hasParam("value")) {
			String dataType = request->getParam("type")->value();
			float dataValue = request->getParam("value")->value().toFloat();
			if (dataType == "totalDistance") {
				bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Received new total distance: %.3f km", dataValue / 1000.0);
				curTotalDistance[Statistics::SUM_ESP_TOTAL] = dataValue;
				storeDistanceAndResetRevs(true);
				request->send(200, "text/plain", "Total distance updated");
			} else if (dataType == "totalDistanceLost") {
				bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Received new total LOST distance: %.3f km", dataValue / 1000.0);
				lostDistanceFromNVS[Statistics::SUM_ESP_TOTAL] = dataValue;
				storeDistanceAndResetRevs(true);
				request->send(200, "text/plain", "Total LOST distance updated");
			} if (dataType == "wheelData") {
				bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Received new wheel circumference: %f mm", dataValue);
				updateWheelCirc(dataValue);
				request->send(200, "text/plain", "Wheel data updated");
			} else {
				request->send(400, "text/plain", "Invalid data type");
			}
		} else {
			request->send(400, "text/plain", "Invalid request");
		}
	});

}
