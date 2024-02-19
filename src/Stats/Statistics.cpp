/*
 * Statistics.cpp
 *
 *  Created on: 31.12.2022
 *      Author: ian
 */

#include "Statistics.h"
#include "Distance.h"
#include "Singletons.h"

const char* Statistics::PREF_TIME_STRING[Statistics::EDrivingStateMax] = {
		"TIME_IN_NOCONN",	//		DS_NO_CONN,
		"TIME_IN_BREAK",	//		DS_BREAK,
		"TIME_IN_STOP",		//		DS_STOP,
		"TIME_IN_COAST",	//		DS_DRIVE_COASTING,
		"TIME_IN_DRIVE"		//		DS_DRIVE_POWER,
};

const char* Statistics::SUM_TYPE_STRING[Statistics::ESummaryTypeMax] = {
		"ST_TOTAL",
		"ST_TOUR",
		"ST_TRIP",
		"ST_START",
#if defined BC_FL_SUPPORT
		"ST_FL_TOTAL",
		"ST_FL_TOUR",
		"ST_FL_TRIP",
#endif
};

const char* Statistics::AVG_TYPE_STRING[Statistics::EAvgTypeMax] = {
		"⏱ kompl",
		"⏱ fahrend",
		"⏱ Stops"
};

Statistics::Statistics(): distHandler(* new Distance())  {
	timestamp_last = millis();
	timestamp_stop = timestamp_last;

}

void Statistics::setup() {
	restoreStats();
	distHandler.setup();
	// What kind of sorcery is this?  --> See https://stackoverflow.com/questions/60985496/arduino-esp8266-esp32-ticker-callback-class-member-function
	statCycle.attach_ms(500, +[](Statistics *thisInstance) {thisInstance->cycle();}, this);
	statDataStore.attach(5, +[](Statistics *thisInstance) {thisInstance->dataStore();}, this);
	statStore.attach(15, +[](Statistics *thisInstance) {thisInstance->autoStore();}, this);

	for (uint_fast8_t i = 0 ; i < chart_array_count ; i++) {
		ui.setChartArray(chart_array[i], i);
	}
	// Serve the array data as JSON
	webserver.getServer().on("/stat/data", HTTP_GET, [this](AsyncWebServerRequest *request) {
		String jsonArray = this->generateJSONArray();
		request->send(200, "application/json", jsonArray);
	});
	setupWebserverDebug();
}

// Helper function to generate a JSON array from a float array
String Statistics::generateJSONArray() {
	time_t timenow;
	time(&timenow);		// TODO: timestamp is a bit off, because its not synced with data storage
	String jsonArray = "[[";
	for (size_t i = 0; i < 400; i++) {
		jsonArray += String(timenow - (400-i) * 5);
		if (i < 399) jsonArray += ",";
	}
	jsonArray += ']';
	for (uint_fast8_t j = 0; j < 4; j++) {
		jsonArray += ",[";
		for (size_t i = 0; i < 400; i++) {
			int16_t value = chart_array[j][(i + chart_array_startPos[j]) % 400];
			if (value == INT16_MAX) value = 0;
			jsonArray += String(value);
			if (i < 399) jsonArray += ",";
		}
		jsonArray +=']';
	}
	jsonArray += ']';
	return jsonArray;
}

void Statistics::restoreStats() {
	for (uint_fast8_t c = 0 ; c < SUM_ESP_START; c++) {
		StatPreferences[c].begin(SUM_TYPE_STRING[c]);
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			time_in[d][c] = StatPreferences[c].getLong(PREF_TIME_STRING[d], 0);
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Loaded time in %s for %s from preferences: %d", PREF_TIME_STRING[d], SUM_TYPE_STRING[c], time_in[d][c]);
		}
		//StatPreferences[c].end();
	}
}

void Statistics::autoStore() {
	bclog.log(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Store distance and time to preferences");
	for (uint_fast8_t c = 0 ; c < SUM_ESP_START; c++) {
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			if (!StatPreferences[c].putLong(PREF_TIME_STRING[d], time_in[d][c])) {
				bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Can't save time in %s for %s to preferences", PREF_TIME_STRING[d], SUM_TYPE_STRING[c]);
			}
		}
	}
	updateTimeSeries();			// FIXME: test only - there may be time or distance mode so maybe move it to cycle or updateRevs.
	for (uint_fast8_t j=0; j < 4 ; j++) {
		createChartArray(j);	// FIXME: test only
	}
}

void Statistics::dataStore() {
	bclog.appendDataLog(speed, sensors.getTemp(), gradient, distHandler.getDistance(), height, hr, cadence);
}

void Statistics::cycle() {
	time_t time_now = millis();
	uint32_t delta = time_now - timestamp_last;
	timestamp_last = time_now;
	time_t time_in_break = time_now - timestamp_stop;
	for (uint_fast8_t c = SUM_ESP_TOTAL; c <= SUM_ESP_START; c++) {
		time_in[curDriveState][c] += delta;
	}

	// Driving state fsm (Connected sub-state)
	switch (curDriveState) {
	case DS_STOP:
		if ( time_in_break > 120000) {
			for (uint_fast8_t c = SUM_ESP_TOTAL; c <= SUM_ESP_START; c++) {
				if (time_in[DS_STOP][c] >= time_in_break) {
					time_in[DS_STOP][c] -= time_in_break;
				} else { // else path can happen after data reset
					time_in_break = time_in[DS_STOP][c];
					time_in[DS_STOP][c] = 0;
				}
				time_in[DS_BREAK][c] += time_in_break;
			}
			setCurDriveState(DS_BREAK);
		}
		//no break
	case DS_BREAK:
		if (speed > 5.5) {
			setCurDriveState(cadence < 40  ? DS_DRIVE_COASTING : DS_DRIVE_POWER);
			offAfterMinutes = 5;
			time_in_break = 0;	// Necessary so that next if is not true
		}
		// no break - also switch off in NO_CONN
	case DS_NO_CONN:
		if (time_in_break > (offAfterMinutes * 60000)) {		// auto-switch off  (default 50min, can be delayed)
			trgb.deepSleep();
		}
		break;
	// state DRIVING
	case DS_DRIVE_COASTING:
	case DS_DRIVE_POWER:
		timestamp_stop = time_now;	// Continuously update timestamp_stop if not in break/stop/disconnected (necessary so that
		if (speed < 0.3) {
			setCurDriveState(DS_STOP);
		} else {
			//TODO: add speed depended cadence limits to adapt to steep gradients
			if (cadence < 40 && curDriveState == DS_DRIVE_POWER) {
				bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Speed: %f Cadence: %d", speed, cadence);
				setCurDriveState(DS_DRIVE_COASTING);
			} else if (cadence > 50 && curDriveState == DS_DRIVE_COASTING) {
				setCurDriveState(DS_DRIVE_POWER);
			}
		}
	}
	updateStateIcon();				// Always update in cycle, to support blinking and missed changes/init.
	sensors.readBME280();
}

void Statistics::delayStandby() {
	time_t time_in_break = millis() - timestamp_stop;
	if ((offAfterMinutes * 60000 - time_in_break ) < 60000 ) {
		offAfterMinutes++;
		updateStateIcon();	// Immediate update (user feedback)
	}
}

void Statistics::toggleStandbyMode() {
	if (offAfterMinutes == 255)	offAfterMinutes = ((millis()-timestamp_stop)/60000) + 5;
	else offAfterMinutes = 255;
	updateStateIcon();	// Immediate update (user feedback)
}

void Statistics::setConnected(bool connected) {
	if (connected && (curDriveState == DS_NO_CONN)) {
		setCurDriveState(histDriveState);
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_STAT, "Connected to speed sensor - counting time");
	}
	if (!connected && curDriveState != DS_NO_CONN) {
		addSpeed(NAN);
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_STAT, "Disconnected from speed sensor - stop time counters");
		histDriveState = curDriveState;
		setCurDriveState(DS_NO_CONN);
	}
}

void Statistics::updateStateIcon() {
	UIFacade::UIColor color = UIFacade::UI_ColorNeutral;
	if (curDriveState == DS_NO_CONN || curDriveState == DS_BREAK) {
		if (offAfterMinutes == 255)	color = UIFacade::UI_ColorOK;
		else {
			time_t time_in_break = millis() - timestamp_stop;
			time_t remaining = offAfterMinutes * 60000 - time_in_break ;
			if (( remaining < 60000) && (((time_in_break / 1000) % 2) == 1)) { // part after '&&' enables blinking - TODO: Have blink function(s) based on cycle() count
				color = (remaining < 10000) ? UIFacade::UI_ColorCrit : UIFacade::UI_ColorWarn;
			}
		}
	}
	ui.updateStateIcon(curDriveState, color);
}

// ******************** Helper functions (static) ********************
//static
void Statistics::addFloatToDatapoint(S_DataPoint& data, const float val) {
	// Use negated comparison, so statement is true if stored value is NAN
	if (! (data.min < val)) data.min = val;
	if (! (data.max > val)) data.max = val;
	if (isnan(data.avg)) data.avg = 0;
	data.avg += val;
}

//static
float Statistics::setDatapointAvg(S_DataPoint &data, uint8_t& count) {
	data.avg = data.avg / count;
	count = 0;
	return data.avg;
}

// ******************** Add data to statistics ********************
void Statistics::addCadence(int16_t _cadence, int16_t _total) {
	cadence = _cadence;
	cadence_tot = _total;

	addFloatToDatapoint(distanceData.currentMinMax.cadence, (cadence * 1.0));
	distanceData.curCountCadence++;

	addFloatToDatapoint(timeData.currentMinMax.cadence, (cadence * 1.0));
	timeData.curCountCadence++;

	ui.updateCadence(cadence);
}

void Statistics::addHR(int16_t _hr) {
	hr = _hr;
	float hr_float = hr > 0 ? hr * 1.0 : NAN;

	addFloatToDatapoint(distanceData.currentMinMax.hr, hr_float );
	distanceData.curCountHr++;

	addFloatToDatapoint(timeData.currentMinMax.hr, hr_float);
	timeData.curCountHr++;

	ui.updateHR(hr);
}

void Statistics::addSpeed(float _speed) {
	speed = _speed;
	for (uint_fast8_t i = 0; i<= SUM_ESP_START; i++) {
		if (speed_max[i] < speed) speed_max[i] = speed;
	}

	addFloatToDatapoint(distanceData.currentMinMax.speed, speed);
	distanceData.curCountSpeed++;

	addFloatToDatapoint(timeData.currentMinMax.speed, speed);
	timeData.curCountSpeed++;

	ui.updateSpeed(speed);
}

void Statistics::addGradientHeight(float _grad, float _height) {
	gradient = _grad;
	addFloatToDatapoint(distanceData.currentMinMax.gradient, gradient);
	height = _height;
	addFloatToDatapoint(distanceData.currentMinMax.height, height);
	distanceData.curCountGradHeight++;
	ui.updateGrad(gradient, height);
	ui.updateChart();
}

void Statistics::updateDistanceSeries() {
	float distance = distHandler.getDistance();
	if ( (distance - distanceData.curDistance) >= 100) {		// every 100m
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "100m step - distance: %.1fm", distance);
		distanceData.curDistance = distance;

		setDatapointAvg(distanceData.currentMinMax.cadence, distanceData.curCountCadence);
		setDatapointAvg(distanceData.currentMinMax.gradient, distanceData.curCountGradHeight);
		setDatapointAvg(distanceData.currentMinMax.height, distanceData.curCountGradHeight);  //FIMXE
		setDatapointAvg(distanceData.currentMinMax.hr, distanceData.curCountHr);
		setDatapointAvg(distanceData.currentMinMax.speed, distanceData.curCountSpeed);
		time(&distanceData.currentMinMax.timestamp);	// --> write timestamp

		distanceData.data[distanceData.index++] = distanceData.currentMinMax;
		if (distanceData.index > 400) {
			bclog.log(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Statistics DistanceData overflow - resetting index to 0");
			distanceData.index = 0;
		}
		distanceData.currentMinMax.cadence.min = NAN;
		distanceData.currentMinMax.cadence.avg = NAN;
		distanceData.currentMinMax.cadence.max = NAN;
		distanceData.curCountCadence = 0;

		distanceData.currentMinMax.gradient.min = NAN;
		distanceData.currentMinMax.gradient.avg = NAN;
		distanceData.currentMinMax.gradient.max = NAN;
		distanceData.currentMinMax.height.min = NAN;
		distanceData.currentMinMax.height.avg = NAN;
		distanceData.currentMinMax.height.max = NAN;
		distanceData.curCountGradHeight = 0;

		distanceData.currentMinMax.hr.min = NAN;
		distanceData.currentMinMax.hr.avg = NAN;
		distanceData.currentMinMax.hr.max = NAN;
		distanceData.curCountHr = 0;

		distanceData.currentMinMax.speed.min = NAN;
		distanceData.currentMinMax.speed.avg = NAN;
		distanceData.currentMinMax.speed.max = NAN;
		distanceData.curCountSpeed = 0;
	}
}

void Statistics::updateTimeSeries() {
	time_t now;
	time(&now);
	//if ( (now - timeData.startTime) >= 5000) {
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "10s step @ %d", now);
		timeData.startTime = now;

		setDatapointAvg(timeData.currentMinMax.cadence, timeData.curCountCadence);
		setDatapointAvg(timeData.currentMinMax.hr, timeData.curCountHr);
		setDatapointAvg(timeData.currentMinMax.speed, timeData.curCountSpeed);
		timeData.currentMinMax.distance = distHandler.getDistance();

		timeData.data[timeData.index++] = timeData.currentMinMax;
		if (timeData.index >= 400) {
			bclog.log(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Statistics TimeData overflow - resetting index to 0");
			timeData.index = 0;
		}
		timeData.currentMinMax.cadence.min = NAN;
		timeData.currentMinMax.cadence.avg = NAN;
		timeData.currentMinMax.cadence.max = NAN;
		timeData.curCountCadence = 0;

		timeData.currentMinMax.hr.min = NAN;
		timeData.currentMinMax.hr.avg = NAN;
		timeData.currentMinMax.hr.max = NAN;
		timeData.curCountHr = 0;

		timeData.currentMinMax.speed.min = NAN;
		timeData.currentMinMax.speed.avg = NAN;
		timeData.currentMinMax.speed.max = NAN;
		timeData.curCountSpeed = 0;
//	}
}

void Statistics::createChartArray(uint8_t idx) {
	uint32_t startcount = xthal_get_ccount();
	bool modeTime = true; // true: X axis is time, false: X axis is distance
	float scale = 1.0;
	DataClass dc = chart_array_type[idx];

	// timeData.index is always the last written position + 1 - so the next value to be written. And thus it's also the first value to be shown in chart, if the newest point is the most right one.
	chart_array_startPos[idx] = timeData.index;

	for (uint16_t point = 0 ; point < 400 ; point ++) {
		S_DataPoint dp;
		switch (dc) {
		case SPEED:
			dp = timeData.data[point].speed;
			break;
		case HR:
			dp = timeData.data[point].hr;
			break;
		case CADENCE:
			dp = timeData.data[point].cadence;
			break;
		case DISTANCE:
			timeData.data[point].distance;
			break;
		case HEIGHT:
		case GRADIENT:
		case TEMPERATURE:
			//FIXME: in distance only
			break;
		};
		//FIXME: Finalize min/max evalution
//		bool useMax = (point > 0 && dp.max > timeData.data[point-1].hr.max) && (point < 399 && dp.max > timeData.data[point+1].hr.max);
//		bool useMin = (point > 0 && dp.min < timeData.data[point-1].hr.min) && (point < 399 && dp.min < timeData.data[point+1].hr.min);
//		if (useMax && useMin) {
//			useMax = false;
//			useMin = false;
//		}
		float val = NAN;
		if (dc == DISTANCE) {
			val = timeData.data[point].distance;
		} else {
			//val = (useMax ? dp.max : ( useMin ? dp.min : dp.avg) ) * scale;
			val = dp.avg * scale;
		}
		chart_array[idx][point] = isnan(val) ? INT16_MAX : static_cast<int16_t>(round(val));
	}
	ui.setChartPosFirst(chart_array_startPos[idx], idx);
	uint32_t endcount = xthal_get_ccount();
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Took %d cycles to update chart", endcount - startcount);
}

void Statistics::checkDistance(const float dist) {
	if ( (dist - distSeries_last) > 500) {
		distSeries_last = dist;
		updateDistanceSeries();
	}
	calculateGradient(dist);
}

// ******************** internal calculations ********************

void Statistics::calculateGradient(float newDist) {
/* Don't compile own gradient calculation if an external gradient calculation is done (like in Forumslader) */
#ifndef BC_FL_SUPPORT
	time_t time_now = millis();
	uint32_t delta = time_now - gradient_timestamp;  // [msec]
	float delta_dist = newDist - gradient_dist;
	if ( ( delta_dist >= 8 && delta > 2500) || delta >= 5000 /*|| delta_height > 0.5*/ ) {   // Using delta_height seems to cause more harm than good
		// Get Height information, so it's synchronized with distance update (improves accuracy)
		sensors.readBME280();
		float height_new = sensors.getHeight();		// [m]
		float delta_height = height_new - gradient_height;
		if (delta_dist < 0 || delta_dist > 200) {
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Gradient calculation: New distance %.2fm implausible (delta: %.2fm) ", newDist, delta_dist);
			delta_dist = NAN;
		}
		gradient_height = height_new;
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Update gradient with deltas time: %dms, distance: %.2fm, height: %.2fm", delta, delta_dist, delta_height);
		gradient_timestamp = time_now;

		//Note: Keep it outside the next if. Otherwise large delta in height will result in extreme spikes in gradient. ("Elevator case").
		//      It does not make sense to do a calculation for this, because it will be wrong in many cases anyhow.
		gradient_dist = newDist;

		float gradient_new = NAN; // division by 0 should not lead not +Inf or -Inf, because delta_height is also close to zero. Ideally it would be zero, so 0/0 --> NAN.
		if (delta_dist > 0.2) {
			gradient = delta_height / (delta_dist) * 100.0;		// simplified gradient calculation. Accurate enough for smaller gradients.
		}
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Gradient: %.2f", gradient);
		addGradientHeight(gradient_new, height_new);
	}
#endif
}

void Statistics::addGradientFL(int16_t _grad, int16_t _height, int16_t _temp) {
#ifdef BC_FL_SUPPORT
	// Convert to float
	height = _height * 1.0;
	gradient = _grad / 10.0;
	temperature = _temp / 10.0;
	ui.updateGrad(gradient, height);
#endif
}

uint32_t Statistics::getDistance(ESummaryType type, bool includeLost) const {
	switch (type) {
	case SUM_ESP_TOTAL:
	case SUM_ESP_TOUR:
	case SUM_ESP_TRIP:
	case SUM_ESP_START:
		return distHandler.getDistance(type);
#ifdef BC_FL_SUPPORT
	case SUM_FL_TRIP:
	case SUM_FL_TOUR:
		return start_distance[type];
	case SUM_FL_TOTAL:
		return distance;
#endif
	}
	return 0;
}

void Statistics::reset(ESummaryType type) {	//TODO: Move to DistanceHandler
	for (uint_fast8_t d = 0; d < EDrivingStateMax; d++) {
		time_in[d][type] = 0;
	}
}

void Statistics::setCurDriveState(EDrivingState _curDriveState) {
	curDriveState = _curDriveState;
	if (_curDriveState == DS_STOP) timestamp_stop = millis();
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Driving state changed to %s", (PREF_TIME_STRING[curDriveState]+8));
}

uint32_t Statistics::getTime(ESummaryType type, EAvgType avgtype) const {
	//TODO: uint32_t is too short for time in msec (roll-over every 7 weeks)
	uint32_t relevantTime = time_in[DS_DRIVE_COASTING][type] + time_in[DS_DRIVE_POWER][type];
	switch (avgtype) {
	case AVG_ALL:
		relevantTime += time_in[DS_BREAK][type];
		//no break
	case AVG_NOBREAK:
		relevantTime += time_in[DS_STOP][type];
	}
	return relevantTime/1000;		//msec to sec
}

float Statistics::getAvg(ESummaryType type, EAvgType avgtype) const {
	//FIXME: avg does not take into account distance in no connection. There should be at least a mechanism to compensate distance in NO_CONN
	//TRACE: Serial.print(getDistance(type)); Serial.print('\t');Serial.println(relevantTime);

	//        .. in m         / sec                     * 3.6 km/h / m/s..
	return (getDistance(type, false) / static_cast<float>(getTime(type, avgtype))) * 3.6;

}

float Statistics::getAvgCadence(EAvgType avgtype) const {
	//FIXME: Calculation is totally wrong...
	//			Start total cadence needs to be stored per summary type.
	//			Time: Depends, but cadence sensors counts even if disconnected, so it should be accurate to use. Meaningful is value only during driving (including and excluding coasting).
	//TRACE: Serial.print(getDistance(type)); Serial.print('\t');Serial.println(relevantTime);

	//        .. in m         / sec                             * 3.6 km/h / m/s..
		return (cadence_tot / static_cast<float>(getTime(SUM_ESP_START, avgtype)) ) * 3.6;

}

void Statistics::setupWebserverDebug() {
	webserver.getServer().on("/stat/debugarray", HTTP_GET, [this](AsyncWebServerRequest *request) {
		String htmlPage = "<!DOCTYPE html>\n";
		htmlPage += "<html lang=\"en\">\n";
		htmlPage += "<head>\n";
		htmlPage += "  <meta charset=\"UTF-8\">\n";
		htmlPage += "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
		htmlPage += "  <title>Array Viewer</title>\n";
		htmlPage += "</head>\n";
		htmlPage += "<body>\n";
		htmlPage += "  <h1>Array Viewer</h1>\n";

		htmlPage += "  <table>\n";
		htmlPage += "    <tr>\n";
		htmlPage += "      <th>Index</th>\n";
		htmlPage += "      <th>DataArray</th>\n";
		htmlPage += "      <th>Min</th>\n";
		htmlPage += "      <th>Avg</th>\n";
		htmlPage += "      <th>Max</th>\n";
		htmlPage += "    </tr>\n";

		htmlPage += "    <tr>\n";
		htmlPage += "      <td>Current</td>\n";
		htmlPage += "      <td>Count: " + String(timeData.curCountHr) + "</td>\n";
		htmlPage += "      <td>" + String(timeData.currentMinMax.hr.min) + "</td>\n";
		htmlPage += "      <td>" + String(timeData.currentMinMax.hr.avg) + "</td>\n";
		htmlPage += "      <td>" + String(timeData.currentMinMax.hr.max) + "</td>\n";
		htmlPage += "    </tr>\n";

		for (int i = 0; i < sizeof(this->chart_array[1]) / sizeof(this->chart_array[1][0]); i++) {
			S_DataPoint data = timeData.data[i].hr;
			htmlPage += "    <tr>\n";
			htmlPage += "      <td>" + ((chart_array_startPos[1] == i) ? String("-->") : String("")) + String(i) + "</td>\n";
			htmlPage += "      <td>" + String(chart_array[1][i]) + "</td>\n";
			htmlPage += "      <td>" + String(data.min) + "</td>\n";
			htmlPage += "      <td>" + String(data.avg) + "</td>\n";
			htmlPage += "      <td>" + String(data.max) + "</td>\n";
			htmlPage += "    </tr>\n";
		}
		htmlPage += "</body>\n";
		htmlPage += "</html>\n";

		request->send(200, "text/html", htmlPage);
	});
}

