/*
 * Statistics.cpp
 *
 *  Created on: 31.12.2022
 *      Author: ian
 */

#include "Statistics.h"
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
		"ST_FL_TOTAL",
		"ST_FL_TOUR",
		"ST_FL_TRIP",
};

const char* Statistics::AVG_TYPE_STRING[Statistics::EAvgTypeMax] = {
		"⏱ kompl",
		"⏱ fahrend",
		"⏱ Stops"
};

Statistics::Statistics() {
	timestamp_last = millis();
}

void Statistics::setup() {
	// What kind of sorcery is this?  --> See https://stackoverflow.com/questions/60985496/arduino-esp8266-esp32-ticker-callback-class-member-function
	statCycle.attach_ms(500, +[](Statistics *thisInstance) {thisInstance->cycle();}, this);
	statDataStore.attach(5, +[](Statistics *thisInstance) {thisInstance->dataStore();}, this);
	statStore.attach(5, +[](Statistics *thisInstance) {thisInstance->autoStore();}, this);
	restoreStats();
	ui.setChartArray(height_array, 0, 400);
	ui.setChartArray(hr_array, 1, 400);
	ui.setChartArray(speed_array, 2, 400);
	ui.setChartArray(speed2_array, 3, 400);

	// Serve the HTML page
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

		for (int i = 0; i < sizeof(this->hr_array) / sizeof(this->hr_array[0]); i++) {
			S_DataPoint data = timeData.distanceData[i].hr;
			htmlPage += "    <tr>\n";
			htmlPage += "      <td>" + ((hr_array_idx == i) ? String("-->") : String("")) + String(i) + "</td>\n";
			htmlPage += "      <td>" + String(hr_array[i]) + "</td>\n";
			htmlPage += "      <td>" + String(data.min) + "</td>\n";
			htmlPage += "      <td>" + String(data.avg) + "</td>\n";
			htmlPage += "      <td>" + String(data.max) + "</td>\n";
			htmlPage += "    </tr>\n";
		}
		htmlPage += "</body>\n";
		htmlPage += "</html>\n";

		request->send(200, "text/html", htmlPage);
	});

	// Serve the array data as JSON
	webserver.getServer().on("/stat/data", HTTP_GET, [this](AsyncWebServerRequest *request) {
		String jsonArray = this->generateJSONArray();
		request->send(200, "application/json", jsonArray);
	});

}


// Helper function to generate a JSON array from a float array
String Statistics::generateJSONArray() {
	time_t timenow;
	time(&timenow);		// TODO: timestamp is a bit off.
	String jsonArray = "[[";
	for (size_t i = 0; i < 400; i++) {
		jsonArray += String(timenow - (400-i) * 5);
		if (i < 399) jsonArray += ",";
	}
	jsonArray += "],[";
	for (size_t i = 0; i < 400; i++) {
		int16_t value = hr_array[(i+hr_array_idx) % 400];
		if (value == INT16_MAX) value = 0;
		jsonArray += String(value);
		if (i < 399) jsonArray += ",";
	}
	jsonArray += "]]";
	return jsonArray;
}

void Statistics::restoreStats() {
	for (uint_fast8_t c = 0 ; c < SUM_ESP_START; c++) {
		StatPreferences[c].begin(SUM_TYPE_STRING[c]);
		start_distance[c] = StatPreferences[c].getLong("START_DISTANCE", 0);
		lost_distance[c]  = StatPreferences[c].getLong("LOST_DISTANCE",  0);
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Loaded distance %d (lost: %d) from preferences: %s", start_distance[c], lost_distance[c], SUM_TYPE_STRING[c]);
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			time_in[d][c] = StatPreferences[c].getLong(PREF_TIME_STRING[d], 0);
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Loaded time in %s for %s from preferences: %d", PREF_TIME_STRING[d], SUM_TYPE_STRING[c], time_in[d][c]);
		}
		//StatPreferences[c].end();
	}
	StatPreferences[SUM_ESP_START].begin(SUM_TYPE_STRING[SUM_ESP_START]);
	start_distance[SUM_ESP_START] =StatPreferences[SUM_ESP_START].getLong("START_DISTANCE", 0);
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Loaded start distance %d from preferences.", start_distance[SUM_ESP_START]);
	distance = start_distance[SUM_ESP_START];
}

void Statistics::autoStore() {
	bclog.log(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Store distance to preferences");
	for (uint_fast8_t c = 0 ; c < SUM_ESP_START; c++) {
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Store distance to preferences %s", SUM_TYPE_STRING[c]);
		if (!StatPreferences[c].putLong("START_DISTANCE", start_distance[c])) {
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Can't save distance %s to preferences", SUM_TYPE_STRING[c]);
		}
		if (!StatPreferences[c].putLong("LOST_DISTANCE", lost_distance[c])) {
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Can't save lost distance %s to preferences", SUM_TYPE_STRING[c]);
		}
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			if (!StatPreferences[c].putLong(PREF_TIME_STRING[d], time_in[d][c])) {
				bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Can't save time in %s for %s to preferences", PREF_TIME_STRING[d], SUM_TYPE_STRING[c]);
			}
		}
	}
	if (!StatPreferences[SUM_ESP_START].putLong("START_DISTANCE", distance)) {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_STAT, "Can't save current distance (SUM_ESP_START) to preferences");
	}

	updateTimeSeries();		// FIMXE: test only
	createChartArray();		// FIXME: test only


}

void Statistics::dataStore() {
	calculateGradient(-1);	// FIXME: Distance handling wrong here (old value for _revs)
	bclog.appendDataLog(speed, temperature, gradient, distance, height, hr);
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
		}
		if (time_in_break > (5 * 60000)) {		// switch off after 5 minutes
			// TODO: Add warning on display before switch-off and add button to override
			trgb.deepSleep();
		}
		break;
	// state DRIVING
	case DS_DRIVE_COASTING:
	case DS_DRIVE_POWER:
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
}

void Statistics::setConnected(bool connected) {
	if (connected && (curDriveState == DS_NO_CONN)) {
		setCurDriveState(histDriveState);
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_STAT, "Connected to speed sensor - counting time");
	}
	if (!connected && curDriveState != DS_NO_CONN) {
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_STAT, "Disconnected from speed sensor - stop time counters");
		histDriveState = curDriveState;
		setCurDriveState(DS_NO_CONN);
		distance_start = false;
	}
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
	height_array[height_array_idx++] = height;
	if (height_array_idx > 400) height_array_idx = 0;
	addFloatToDatapoint(distanceData.currentMinMax.height, height);
	distanceData.curCountGradHeight++;

	ui.updateGrad(gradient, height);
	ui.updateChart();
}

void Statistics::updateDistanceSeries() {
	if ( (distance - distanceData.curDistance) >= 100) {		// every 100m
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "100m step - distance: %d", distance);
		distanceData.curDistance = distance;

		setDatapointAvg(distanceData.currentMinMax.cadence, distanceData.curCountCadence);
		setDatapointAvg(distanceData.currentMinMax.gradient, distanceData.curCountGradHeight);
		setDatapointAvg(distanceData.currentMinMax.height, distanceData.curCountGradHeight);  //FIMXE
		setDatapointAvg(distanceData.currentMinMax.hr, distanceData.curCountHr);
		setDatapointAvg(distanceData.currentMinMax.speed, distanceData.curCountSpeed);
		time(&distanceData.currentMinMax.timestamp);	// --> write timestamp

		distanceData.distanceData[distanceData.index++] = distanceData.currentMinMax;
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
	//if ( (now - timeData.startTime) >= 5000) {		// every 100m
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "10s step @ %d", now);
		timeData.startTime = now;

		setDatapointAvg(timeData.currentMinMax.cadence, timeData.curCountCadence);
		setDatapointAvg(timeData.currentMinMax.hr, timeData.curCountHr);
		setDatapointAvg(timeData.currentMinMax.speed, timeData.curCountSpeed);
		timeData.currentMinMax.distance = distance;	// --> write distance

		timeData.distanceData[timeData.index++] = timeData.currentMinMax;
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

void Statistics::createChartArray() {
	bool modeTime = true; // true: X axis is time, false: X axis is distance
	uint8_t idx = 1;
	enum DataClass {SPEED = 0, HR, HEIGHT, GRADIENT, TEMPERATURE};

	float scale = 1.0;
	DataClass c = HR;

	// timeData.index is always the last written position + 1 - so the next value to be written. And thus it's also the first value to be shown in chart, if the newest point is the most right one.
	hr_array_idx = timeData.index;

	for (uint16_t point = 0 ; point < 400 ; point ++) {
		bool useMax = (point > 0 && timeData.distanceData[point].hr.max > timeData.distanceData[point-1].hr.max) && (point < 399 && timeData.distanceData[point].hr.max > timeData.distanceData[point+1].hr.max);
		bool useMin = (point > 0 && timeData.distanceData[point].hr.min < timeData.distanceData[point-1].hr.min) && (point < 399 && timeData.distanceData[point].hr.min < timeData.distanceData[point+1].hr.min);
		if (useMax && useMin) {
			useMax = false;
			useMin = false;
		}
		S_DataPoint lp =  timeData.distanceData[point].hr;
		float val = (useMax ? lp.max : ( useMin ? lp.min : lp.avg) ) * scale;
		hr_array[point] = isnan(val) ? INT16_MAX : static_cast<int16_t>(round(val));
	}

	ui.setChartPosFirst(hr_array_idx, 1); 	// IDX 1 is HR series. FIXME: Make more generic


//
//	switch (c) {
//	case SPEED:
//	case HR:
//	default:
//
//	}
}

/* ******************** handle distance ********************
    handling distance is more special than other data becauseTh it is (in parallel to time) a clock for data handling
    Also, storing total distance is part of the Statistics class
   TODO: Distance handling is not clean yet:
       * Single point to handle wheel diameter
       * Keep track of distance over wheel diameter changes
*/
void Statistics::addDistance(uint32_t dist, ESummaryType type) {
	start_distance[type] = dist;
}

void Statistics::updateDistance(uint32_t _dist, uint32_t _revs) {
	if (curDriveState == DS_NO_CONN) {
		uint32_t dist_lost_new = _dist - distance;
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Lost %d pulses of distance.", dist_lost_new);
		updateLostDistance(dist_lost_new);
	}
	distance = _dist;
	if (!distance_start) {
		distance_start = true;
		addDistance(distance, SUM_ESP_START);
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Initial distance: %d meter.", distance);
		distanceData.startDistance = distance;
		distanceData.curDistance = distance;
		distanceData.index = 0;		// FIXME: Handle reconnect case better
	}
	calculateGradient(_revs);
	updateDistanceSeries();
}

void Statistics::updateLostDistance(uint32_t _dist_lost) {
	// distance_start == true --> initial start. Don't set lost_distance for SUM_ESP_START then.
	for (uint_fast8_t i = 0; i <= (distance_start ? SUM_ESP_START : SUM_ESP_TRIP); i++) {
		lost_distance[i] += _dist_lost;
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Lost distance for %s now %d pulses", SUM_TYPE_STRING[i], lost_distance[i]);
	}
}

// ******************** internal calculations ********************

void Statistics::calculateGradient(int32_t _revs) {
/* Don't compile own gradient calculation if an external gradient calculation is done (like in Forumslader) */
#if !BC_FL_SUPPORT
	// Get Height information, so it's synchronized with distance update (improves accuracy)
	if (_revs == -1) _revs = gradient_revs;
	int32_t revs_since_last = _revs - gradient_revs ;   // [Pulses]
	time_t time_now = millis();
	uint32_t delta = time_now - gradient_timestamp;  // [msec]
	if (revs_since_last >= 4 || delta >= 5000 /*|| delta_height > 0.5*/ ) {   // Using delta_height seems to cause more harm than good
		sensors.readBME280();
		height = sensors.getHeight();		// [m]
		temperature = sensors.getTemp();
		float delta_height = height - gradient_height;
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_STAT, "Update gradient with deltas time: %dms, revs: %d, height: %.2fm", delta, revs_since_last, delta_height);
		gradient_timestamp = time_now;
		gradient_revs = _revs;
		gradient_height = height;
		gradient = delta_height / (revs_since_last * 2.22) * 100;	//FIXME: Handle distances correctly. (Only one place to convert pulses to distance etc.)
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Gradient: %.2f", gradient);
		addGradientHeight(gradient, height);  //FIXME: also copies gradient to gradient again (and height to height)
		//ui.updateGrad(gradient, height);
	}
#endif
}

void Statistics::addGradientFL(int16_t _grad, int16_t _height, int16_t _temp) {
	// Convert to float
	height = _height * 1.0;
	gradient = _grad / 10.0;
	temperature = _temp / 10.0;
	ui.updateGrad(gradient, height);
}


uint32_t Statistics::getDistance(ESummaryType type, bool includeLost) const {
	switch (type) {
	case SUM_ESP_TOTAL:
	case SUM_ESP_TOUR:
	case SUM_ESP_TRIP:
	case SUM_ESP_START:
		return distance - start_distance[type] - (includeLost ? 0 : lost_distance[type]);
	case SUM_FL_TRIP:
	case SUM_FL_TOUR:
		return start_distance[type];
	case SUM_FL_TOTAL:
		return distance;
	}
	return 0;
}

void Statistics::reset(ESummaryType type) {
	if (type==SUM_ESP_TOUR || type == SUM_ESP_TRIP) {
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Reset statistic for %s.", SUM_TYPE_STRING[type]);
		start_distance[type] = distance;
		lost_distance[type] = 0;
		for (uint_fast8_t d = 0 ; d < EDrivingStateMax ; d++) {
			time_in[d][type] = 0;
		}
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


