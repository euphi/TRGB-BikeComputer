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
		String prefString = String(NVS_STAT_PREFIX "DIST_") + idx_str + String("_") + String(Statistics::SUM_TYPE_STRING[j]+3);
		storedDist.begin(prefString.c_str(), true);		// +3 to skip first three chars "ST_"
		distanceFromNVS[j]  = storedDist.getFloat("total", 0.0);		// Total distance in m (as float). It is only updated sporadically, so the actual total distance is total + (revs * wheel_circ).
		lostDistanceFromNVS[j] = storedDist.getFloat("lost_total", 0.0);	// Total distance lost
		if (isnan(distanceFromNVS[j])) {
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "%s: Loaded distance from NVS invalid (NAN) - setting to zero.", Statistics::SUM_TYPE_STRING[j]);
			distanceFromNVS[j] = 0.0;
		}
		revsKnown[j] = storedDist.isKey("revs");
		revsFromNVS[j] = storedDist.getULong("revs", 0);
		storedDist.end();
		curTotalDistance[j] = distanceFromNVS[j];
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "%s: Loaded %s total distance %.2fm at %d revs.",
				Statistics::SUM_TYPE_STRING[j], prefString.c_str(), distanceFromNVS[j], revsFromNVS[j]);
	}
	distanceFromNVS[Statistics::SUM_ESP_START] = 0.0;	// since power-on
	curTotalDistance[Statistics::SUM_ESP_START] = 0.0;
}

void Distance::updateRevs(uint32_t revs, uint16_t timestamp) {
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "%d revs at %d ticks", revs, timestamp);

	/* Distance vs. time: every revolution counts as distance, but only revolutions received
	 * while the sensor is connected have ride time to go with them (Statistics::cycle() counts
	 * no time in DS_NO_CONN). Revolutions made while the BC was off or the sensor disconnected
	 * are added to the distance AND to lostDistanceFromNVS[], so the "net" distance
	 * (Distance::getDistance(t, false)) stays the part that matches the recorded time -- that
	 * is what the average speeds divide.
	 *
	 * Two kinds of sensors: most keep their cumulative wheel revs across reconnects; cheap ones
	 * (e.g. CYCPLUS) restart at 0 whenever they wake up. Since connecting takes a moment, such
	 * a sensor rarely reports exactly 0 on the first message, rather a small count -- which
	 * was ridden, so it is counted (as lost distance if the BC was not connected for it). */

	// Scenario 1: first message after power-on.
	if (!revsInitialized) {
		for (uint_fast8_t j = 0; j <= Statistics::SUM_ESP_START; j++) {
			if (j == Statistics::SUM_ESP_START || !revsKnown[j]) {
				revsFromNVS[j] = revs;		// START counts from power-on; the others have nothing stored yet
			} else if (revs >= revsFromNVS[j]) {
				uint32_t lostRevs = revs - revsFromNVS[j];
				float lostDist = lostRevs * wheel_c;
				lostDistanceFromNVS[j] += lostDist;		// curTotalDistance[] below includes it via revs - revsFromNVS[j]
				bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "%s: %d revs (%.1f m) ridden while switched off --> lost total %.1f m",
						(Statistics::SUM_TYPE_STRING[j] + 3), lostRevs, lostDist, lostDistanceFromNVS[j]);
			} else {
				// Counter restarted since the value was stored: all of the current count was ridden without the BC.
				float lostDist = revs * wheel_c;
				bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "%s: stored revs (%d) greater than sensor revs (%d) - sensor restarted its counter, %.1f m counted as lost",
						(Statistics::SUM_TYPE_STRING[j] + 3), revsFromNVS[j], revs, lostDist);
				distanceFromNVS[j] += lostDist;
				lostDistanceFromNVS[j] += lostDist;
				revsFromNVS[j] = revs;
			}
			revsKnown[j] = true;
		}
		lastRevs = revs;		// no delta, no speed spike on the first message
		lastTimestamp = timestamp;
		revsInitialized = true;
	}

	uint32_t riddenRevs = 0;		// revolutions to count as ridden with the sensor connected (time and distance)
	bool speedSample = true;		// revs/timestamp delta is a real measurement interval
	if (revs < lastRevs) {
		// Scenario 2b: the sensor restarted its counter. On a reconnect the new count was
		// ridden while disconnected (lost); while connected it's just normal riding.
		const bool asLost = !stats.isConnected();
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Received revs %d smaller than last (%d) - sensor restarted its counter (%s).",
				revs, lastRevs, asLost ? "reconnect, counted as lost distance" : "while connected");
		rebaseCounterRestart(asLost, revs);
		if (!asLost) riddenRevs = revs;
		speedSample = false;		// the ticks restarted as well
	} else if (!stats.isConnected()) {
		// Scenario 2a: reconnect, counter kept running -- the gap was ridden without the BC.
		if (revs > lastRevs) updateLostRevs(revs - lastRevs);
		speedSample = false;
	} else {
		riddenRevs = revs - lastRevs;
	}
	if (!stats.isConnected()) stats.setConnected(true);

	// Scenario 3: normal update (also the tail of 1 and 2)
	for (uint_fast8_t j=0; j <= Statistics::SUM_ESP_START; j++) {
		curTotalDistance[j] = distanceFromNVS[j] + ( (revs - revsFromNVS[j]) * wheel_c);
	}
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "New distance in total: %.2fm\tTour: %.2fm\tTrip: %.2fm\tStart: %.2fm",
			curTotalDistance[Statistics::SUM_ESP_TOTAL], curTotalDistance[Statistics::SUM_ESP_TOUR], curTotalDistance[Statistics::SUM_ESP_TRIP], curTotalDistance[Statistics::SUM_ESP_START]);

	stats.checkDistance(curTotalDistance[Statistics::SUM_ESP_START]);

	// Cruise/FreeRide distance for Statistics' "without cruise" average -- same connected-only
	// revs as the speed below, so reconnect gaps (lost distance) never end up in it.
	if (riddenRevs > 0) {
		stats.addDistanceDelta(riddenRevs * wheel_c);
	}

	// If timestamp is zero, revs have been transmitted without timestamp (e.g. in FL mode) --> no speed calculation possible (here)
	if (timestamp > 0) {
		if (speedSample) {
			float newSpeed = calculateSpeed(riddenRevs, timestamp - lastTimestamp);
			if (!isnan(newSpeed)) {
				bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "New speed %.2f", newSpeed);
				stats.addSpeed(newSpeed);
			}
		}
		lastTimestamp = timestamp;
	}
	lastRevs = revs;
}

#ifdef BC_FL_SUPPORT
void Distance::updateRevsFL(uint32_t revs, float pulses_per_s) {
	updateRevs(revs, 0);
	float newSpeed = pulses_per_s * wheel_c * 3.6;
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "New speed from FL %.2f", newSpeed);
	stats.addSpeed(newSpeed);
}
#endif

// Sensor counter restarted (revs < lastRevs): make the current count the new base without
// losing what was counted before, and count the new revs as ridden -- as lost distance if the
// BC wasn't connected for them (asLost). curTotalDistance[] still holds the value at lastRevs.
void Distance::rebaseCounterRestart(bool asLost, uint32_t revs) {
	for (uint_fast8_t j = 0; j <= Statistics::SUM_ESP_START; j++) {
		distanceFromNVS[j] = curTotalDistance[j];
		revsFromNVS[j] = 0;		// curTotalDistance = base + revs * wheel_c from here on
	}
	if (asLost && revs > 0) updateLostRevs(revs);
	lastRevs = revs;
	for (uint_fast8_t j = 0; j <= Statistics::SUM_ESP_START; j++) {
		curTotalDistance[j] = distanceFromNVS[j] + revs * wheel_c;
	}
	storeDistanceAndResetRevs(false);		// persist right away, the stored revs would otherwise still be the old counter's
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
	if (eSummaryType == Statistics::SUM_ESP_START) {
		// START here is "since power-on" (binary log, navigation distance, gradient) and is
		// never reset; the Start/Ride view is a session on top of it, see Statistics::reset().
		stats.reset(eSummaryType);
		return;
	}
	if (eSummaryType < Statistics::SUM_ESP_TOTAL || eSummaryType > Statistics::SUM_ESP_TRIP) {
		bclog.logf(BCLogger::Log_Error, BCLogger::TAG_STAT, "Try to reset invalid Sum-Type 0x%04x", eSummaryType);
		return;
	}
	curTotalDistance[eSummaryType] = 0;
	distanceFromNVS[eSummaryType] = 0;
	lostDistanceFromNVS[eSummaryType] = 0;
	revsFromNVS[eSummaryType] = lastRevs;
	revsKnown[eSummaryType] = revsInitialized;
	if (!revsInitialized) {
		// Reset before the sensor connected: the old counter value must not be taken as the
		// new start, or the first message would count everything since then as lost.
		Preferences storedDist;
		String prefString = String(NVS_STAT_PREFIX "DIST_") + String(currentBikeIdx) + String("_") + String(Statistics::SUM_TYPE_STRING[eSummaryType]+3);
		storedDist.begin(prefString.c_str(), false);
		storedDist.remove("revs");
		storedDist.end();
	}
	storeDistanceAndResetRevs();
	stats.reset(eSummaryType);
}

void Distance::storeDistanceAndResetRevs(bool resetRevs) {
	Preferences storedDist; // Distance (in m and delta as revs) for TOTAL, TOUR, TRIP
	String idx_str(currentBikeIdx);
	for (uint_fast8_t j=0; j <= Statistics::SUM_ESP_TRIP; j++) {
		String prefString = String(NVS_STAT_PREFIX "DIST_") + idx_str + String("_") + String(Statistics::SUM_TYPE_STRING[j]+3); // +3 to skip first three chars "ST_"
		storedDist.begin(prefString.c_str(), false);
		size_t bytes = storedDist.putFloat("total", curTotalDistance[j]);		// Total distance in m (as float). It is only updated sporadically, so the actual total distance is total + (revs * wheel_circ).
		bytes += storedDist.putFloat("lost_total", lostDistanceFromNVS[j]);
		bytes += revsInitialized ? storedDist.putULong("revs", lastRevs) : 4;	// only save lastRevs once received from the sensor
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
		distanceFromNVS[Statistics::SUM_ESP_START] = curTotalDistance[Statistics::SUM_ESP_START];
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
			String prefString = String(NVS_STAT_PREFIX "DIST_0_") + String(Statistics::SUM_TYPE_STRING[j]+3);
			storedDist.begin(prefString.c_str(), true);		// +3 to skip first three chars "ST_"
			float sDist = storedDist.getFloat("total", 0.0);		// Total distance in m (as float). It is only updated sporadically, so the actual total distance is total + (revs * wheel_circ).
			uint32_t sRev = storedDist.getULong("revs", 0);
			storedDist.end();
			jsonDoc[typeString]["storedTotal"] = sDist;
			jsonDoc[typeString]["storedRevs"] = sRev;
			jsonDoc[typeString]["actualDistance"] = curTotalDistance[j];
			jsonDoc[typeString]["totalDistance"] = distanceFromNVS[j];
			jsonDoc[typeString]["deltaRevs"] = lastRevs - revsFromNVS[j];
		}
		String jsonData;
		serializeJson(jsonDoc, jsonData);
		request->send(200, "application/json", jsonData);
	});
	// Current calibration values, so the odometry page can pre-fill its inputs instead of
	// showing empty boxes that silently overwrite whatever is stored when submitted.
	webserver.getServer().on("/stat/calibration", HTTP_GET, [this](AsyncWebServerRequest *request) {
		JsonDocument jsonDoc;
		jsonDoc["wheelData"] = wheel_c;
		jsonDoc["totalDistance"] = curTotalDistance[Statistics::SUM_ESP_TOTAL];
		jsonDoc["totalDistanceLost"] = lostDistanceFromNVS[Statistics::SUM_ESP_TOTAL];
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
			// NOTE: this was "} if (...)" -- a missing else. Every totalDistance or
			// totalDistanceLost update therefore fell through into the wheelData test,
			// failed it, and called request->send() a SECOND time with 400, after having
			// already answered 200. The client saw the 400 and the update looked broken.
			} else if (dataType == "wheelData") {
				// updateWheelCirc() takes METRES and rejects anything outside 0.2..5. The
				// old page labelled its input "mm" and sent e.g. 2155, so every wheel
				// calibration was silently refused by that plausibility check while the
				// handler still answered "updated". The page converts mm->m now; this log
				// line claimed mm as well and was just as wrong.
				bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Received new wheel circumference: %.4f m", dataValue);
				if (updateWheelCirc(dataValue)) {
					request->send(200, "text/plain", "Wheel data updated");
				} else {
					request->send(400, "text/plain", "Implausible wheel circumference (expected 0.2..5 m)");
				}
			} else {
				request->send(400, "text/plain", "Invalid data type");
			}
		} else {
			request->send(400, "text/plain", "Invalid request");
		}
	});

}
