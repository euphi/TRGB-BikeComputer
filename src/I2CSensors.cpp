/*
 * I2CSensors.cpp
 *
 *  Created on: 03.01.2024
 *      Author: ian
 */
#ifdef TRGBBC_SENSORS_I2C


#include <I2CSensors.h>
#include "WebPage.h"
#include <Singletons.h>
#include <LittleFS.h>

#include <math.h>		// for pow()
#include <ArduinoJson.h>


I2CSensors::I2CSensors() {

}

uint16_t I2CSensors::getHTMLPage(String &htmlresponse) {
	WebPage::begin(htmlresponse, "Sensors (I2C)");
	htmlresponse += F("<div class=\"sensor-box\"><h3>BME 280</h3>\n<table><tbody>\n");
	char buffer[320];
	snprintf(buffer, sizeof(buffer) - 1,
	         "<tr><td>Pressure</td><td>%.2f mbar</td></tr>\n"
	         "<tr><td>Height</td><td>%.2f m NHN</td></tr>\n"
	         "<tr><td>Reference pressure (0m)</td><td>%.2f mbar</td></tr>\n"
	         "<tr><td>Humidity</td><td>%.2f %%rel</td></tr>\n"
	         "<tr><td>Temperature</td><td>%.2f &deg;C</td></tr>\n",
	         press, height, bme280.getReferencePressure() / 100, humid, temp);
	htmlresponse += buffer;
	htmlresponse += F("</tbody></table>\n"
	                  "<form action=\"/sensor/submit\" method=\"post\" style=\"margin-top:12px;\">"
	                  "<label for=\"height\">Calibrate to known actual height (m):</label> "
	                  "<input type=\"text\" id=\"height\" name=\"height\" size=\"8\"> "
	                  "<input class=\"btn\" type=\"submit\" value=\"Submit\">"
	                  "</form></div>\n");
	//TODO: Add other sensors here
	WebPage::end(htmlresponse);
	return 200;
}

uint16_t I2CSensors::procHTMLHeight(String& htmlresponse, const float actHeight) {
	refPres = calculateReferencePressure(actHeight, press) * 100;
	bme280.setReferencePressure(refPres );
	sensorPreferences.begin("Sensors");
	sensorPreferences.putFloat("RefPressure", refPres );
	sensorPreferences.end();

	WebPage::begin(htmlresponse, "Calibration");
	char buffer[255];
	snprintf(buffer, sizeof(buffer) - 1,
	         "<p>New reference pressure <b>%.2f mbar</b> stored for height %.2f m.</p>\n", refPres, actHeight);
	htmlresponse += buffer;
	WebPage::end(htmlresponse);
	return 200;
}

void I2CSensors::initBME280() {
	bme280.settings.commInterface = BME280::I2C_MODE;
	bme280.settings.I2CAddress = 0x76;

	//  0, Sleep mode
	//  1 or 2, Forced mode
	//  3, Normal mode
	bme280.settings.runMode = 3;

	//tStandby can be:
	//  0, 0.5ms
	//  1, 62.5ms
	//  2, 125ms
	//  3, 250ms
	//  4, 500ms
	//  5, 1000ms
	//  6, 10ms
	//  7, 20ms
	bme280.settings.tStandby = 1;

	//filter can be off or number of FIR coefficients to use:
	//  0, filter off
	//  1, coefficients = 2
	//  2, coefficients = 4
	//  3, coefficients = 8
	//  4, coefficients = 16
	bme280.settings.filter = 4;

	//tempOverSample can be:
	//  0, skipped
	//  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
	bme280.settings.tempOverSample = 2;

	//pressOverSample can be:
	//  0, skipped
	//  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
	bme280.settings.pressOverSample = 5;

	//humidOverSample can be:
	//  0, skipped
	//  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
	bme280.settings.humidOverSample = 1;

	bme280.begin();
	sensorPreferences.begin("Sensors");
	refPres = sensorPreferences.getFloat("RefPressure", 101300.0);
	bme280.setReferencePressure(refPres);
	sensorPreferences.end();

	bme280Cycle.attach_ms(1000, +[](I2CSensors* thisInstance) { thisInstance->readBME280(); }, this);
}

void I2CSensors::readBME280() {
	BME280_SensorMeasurements measurement;

	for (uint_fast8_t i = 0 ; i < 10 ; i++) {
		if (bme280.isMeasuring()) {
			usleep(2000);
			yield();
		} else {
			bme280.readAllMeasurements(&measurement, 0);
			press = measurement.pressure / 100.0;
			humid = measurement.humidity;
			temp = measurement.temperature;
			height = -44330.77 * (pow((press / (refPres/100)), 0.190263) - 1.0);
			bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_OP, "BME280: %.2f mbar, %.2f rel%%, %.2f °C, %.2f m NN", press, humid, temp, height);
			return;
		}
	}
	bclog.log(BCLogger::Log_Warn, BCLogger::TAG_OP, "Can't read BME280 - measurement in progress (5 times, with 10ms wait)");

//	humid = bme280.readFloatHumidity();
//	press = bme280.readFloatPressure() / 100; // /100: Pa -> hPa == mbar
//	temp  = bme280.readTempC();
//	height = bme280.readFloatAltitudeMeters();
}

// ******************** BMI160 ********************

const char* const I2CSensors::CAL_STATE_STRING[] = {"not calibrated", "running", "ok", "failed: not still", "failed: implausible scale"};

void I2CSensors::initBMI160() {
	bool ok = BMI160.begin(BMI160GenClass::I2C_MODE, 0x68, -1);
	uint8_t devId = BMI160.getDeviceID();
	imuSnap.deviceId = devId;
	if (!ok || devId != 0xD1) {
		bclog.logf(BCLogger::Log_Error, BCLogger::TAG_OP, "BMI160 not found (begin: %d, device id 0x%02X, expected 0xD1) - IMU disabled", ok, devId);
		return;
	}

	// begin() soft-resets the chip and leaves it at +/-2 g, 100 Hz.
	BMI160.setFullScaleAccelRange(BMI160_ACCEL_RANGE_16G);
	BMI160.setAccelRate(BMI160_ACCEL_RATE_400HZ);
	BMI160.setAccelDLPFMode(BMI160_DLPF_MODE_NORM);		// -3 dB at ~160 Hz for 400 Hz ODR
	BMI160.setGyroFIFOEnabled(false);
	BMI160.setFIFOHeaderModeEnabled(false);
	BMI160.setAccelFIFOEnabled(true);

	// The gyro offset used to be auto-calibrated here on every boot, which silently produced a
	// wrong offset whenever the bike was moved while booting. It is now part of the explicit
	// calibration (/debug/imu) and restored from Preferences, as the soft reset clears it.
	loadIMUCalibration();

	BMI160.resetFIFO();
	imuSnap.running = true;
	// 4096 byte (ESP-IDF counts byte): FIFO chunk buffer, bclog.logf() and the NVS write of
	// a calibration. Priority above FlusherTask/BLE (5), so an SD stall can't starve the FIFO.
	xTaskCreate(+[](void* thisInstance){((I2CSensors*)thisInstance)->imuTask();}, "ImuTask", 4096, this, 6, &imuTaskHandle);
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_OP, "BMI160 running: %u Hz, +/-16 g, FIFO acc only, calibration %s",
	           IMU_ODR_HZ, imuCal.valid ? "loaded" : "missing");
}

void I2CSensors::loadIMUCalibration() {
	ImuCalibration cal;
	Preferences p;
	p.begin("RoadQ", true);
	cal.valid = p.getBool("calValid", false);
	if (cal.valid) {
		cal.g0[0] = p.getFloat("g0x", 0);
		cal.g0[1] = p.getFloat("g0y", 0);
		cal.g0[2] = p.getFloat("g0z", 0);
		cal.scale = p.getFloat("scale", 1.0f);
		cal.sigma[0] = p.getFloat("sigX", 0);
		cal.sigma[1] = p.getFloat("sigY", 0);
		cal.sigma[2] = p.getFloat("sigZ", 0);
		cal.gyroOffset[0] = p.getShort("gyrOffX", 0);
		cal.gyroOffset[1] = p.getShort("gyrOffY", 0);
		cal.gyroOffset[2] = p.getShort("gyrOffZ", 0);
		cal.calTime = p.getLong64("calTime", 0);
	}
	p.end();

	if (cal.valid) {
		BMI160.setXGyroOffset(cal.gyroOffset[0]);
		BMI160.setYGyroOffset(cal.gyroOffset[1]);
		BMI160.setZGyroOffset(cal.gyroOffset[2]);
		BMI160.setGyroOffsetEnabled(true);
		imuSnap.calState = CAL_OK;
	}
	portENTER_CRITICAL(&imuMux);
	imuCal = cal;
	portEXIT_CRITICAL(&imuMux);
}

void I2CSensors::storeIMUCalibration() {
	// Called from ImuTask only, which is also the only writer of imuCal -- no lock needed to read it.
	Preferences p;
	p.begin("RoadQ", false);
	p.putFloat("g0x", imuCal.g0[0]);
	p.putFloat("g0y", imuCal.g0[1]);
	p.putFloat("g0z", imuCal.g0[2]);
	p.putFloat("scale", imuCal.scale);
	p.putFloat("sigX", imuCal.sigma[0]);
	p.putFloat("sigY", imuCal.sigma[1]);
	p.putFloat("sigZ", imuCal.sigma[2]);
	p.putShort("gyrOffX", imuCal.gyroOffset[0]);
	p.putShort("gyrOffY", imuCal.gyroOffset[1]);
	p.putShort("gyrOffZ", imuCal.gyroOffset[2]);
	p.putLong64("calTime", imuCal.calTime);
	p.putBool("calValid", imuCal.valid);		// last, so a torn write leaves the old flag
	p.end();
}

bool I2CSensors::requestIMUCalibration() {
	portENTER_CRITICAL(&imuMux);
	bool accept = imuSnap.running && imuSnap.calState != CAL_RUNNING;
	portEXIT_CRITICAL(&imuMux);
	if (accept) imuCalRequest = true;
	return accept;
}

void I2CSensors::imuTask() {
	uint8_t buf[IMU_CHUNK_BYTES];
	TickType_t lastWake = xTaskGetTickCount();
	imuWin = {};
	imuWin.startMs = millis();

	for (;;) {
		vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(IMU_POLL_MS));

		if (imuResetMinMaxRequest.exchange(false)) imuMinMaxValid = false;
		if (imuCalRequest.exchange(false) && !imuCalActive) {
			imuCalAcc = {};
			imuCalActive = true;
			portENTER_CRITICAL(&imuMux);
			imuSnap.calState = CAL_RUNNING;
			imuSnap.calProgress = 0;
			portEXIT_CRITICAL(&imuMux);
			bclog.log(BCLogger::Log_Info, BCLogger::TAG_OP, "BMI160 calibration started - keep the bike still for 3 s");
		}

		uint16_t count = BMI160.getFIFOCount() & 0x07FF;		// fifo_byte_counter is 11 bit
		uint16_t fill = count;
		if (count >= IMU_FIFO_OVERFLOW_BYTES) {
			// Frames are lost already; flush so the next read starts clean. A calibration in
			// progress is simply continued -- it only needs still samples, not contiguous ones.
			BMI160.resetFIFO();
			count = 0;
			portENTER_CRITICAL(&imuMux);
			uint32_t overflows = ++imuSnap.overflows;
			portEXIT_CRITICAL(&imuMux);
			// Throttled: logging can block on the SD mutex, which is what makes the next overflow.
			if (overflows <= 5 || overflows % 100 == 0) {
				bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_OP, "BMI160 FIFO overflow #%u (%u byte) - flushed", overflows, fill);
			}
		}

		uint16_t bytes = count - count % IMU_FRAME_BYTES;
		while (bytes > 0) {
			uint16_t n = bytes < IMU_CHUNK_BYTES ? bytes : IMU_CHUNK_BYTES;
			BMI160.getFIFOBytes(buf, n);
			for (uint16_t i = 0; i < n; i += IMU_FRAME_BYTES) {
				int16_t raw[3];
				for (uint8_t a = 0; a < 3; a++) {
					raw[a] = (int16_t)(buf[i + 2 * a] | (buf[i + 2 * a + 1] << 8));
				}
				imuProcessSample(raw);
			}
			bytes -= n;
		}
		if (imuCalActive && imuCalAcc.n >= IMU_CAL_SAMPLES) {
			imuFinishCalibration();		// outside the sample loop: may block for the gyro FOC
		}

		// Publish once per poll
		uint32_t now = millis();
		uint32_t elapsed = now - imuWin.startMs;
		portENTER_CRITICAL(&imuMux);
		imuSnap.fifoFill = fill;
		if (fill > imuSnap.fifoFillMax) imuSnap.fifoFillMax = fill;
		for (uint8_t a = 0; a < 3; a++) {
			imuSnap.last[a] = imuLast[a];
			imuSnap.min[a] = imuMin[a];
			imuSnap.max[a] = imuMax[a];
		}
		imuSnap.magMax = imuMagMax;
		imuSnap.totalSamples = imuTotalSamples;
		if (imuCalActive) imuSnap.calProgress = imuCalAcc.n;
		if (elapsed >= 1000 && imuWin.n > 0) {
			imuSnap.samplesPerSec = imuWin.n * 1000.0f / elapsed;
			for (uint8_t a = 0; a < 3; a++) {
				double mean = imuWin.sum[a] / imuWin.n;
				double var = imuWin.sumSq[a] / imuWin.n - mean * mean;
				imuSnap.mean[a] = mean;
				imuSnap.sigma[a] = var > 0 ? sqrt(var) : 0;
			}
			imuSnap.magMean = imuWin.sumMag / imuWin.n;
		}
		portEXIT_CRITICAL(&imuMux);
		if (elapsed >= 1000) {
			imuWin = {};
			imuWin.startMs = now;
		}
	}
}

void I2CSensors::imuProcessSample(const int16_t raw[3]) {
	float unscaled[3], a[3];
	float mag2 = 0;
	for (uint8_t i = 0; i < 3; i++) {
		unscaled[i] = raw[i] / IMU_LSB_PER_G;
		a[i] = unscaled[i] * imuCal.scale;
		mag2 += a[i] * a[i];
		imuWin.sum[i] += a[i];
		imuWin.sumSq[i] += (double)a[i] * a[i];
		imuLast[i] = a[i];
		if (!imuMinMaxValid || a[i] < imuMin[i]) imuMin[i] = a[i];
		if (!imuMinMaxValid || a[i] > imuMax[i]) imuMax[i] = a[i];
	}
	float mag = sqrtf(mag2);
	if (!imuMinMaxValid || mag > imuMagMax) imuMagMax = mag;
	imuMinMaxValid = true;
	imuWin.sumMag += mag;
	imuWin.n++;

	imuTotalSamples++;

	// Calibration works on the unscaled values: it determines the scale itself.
	if (imuCalActive && imuCalAcc.n < IMU_CAL_SAMPLES) {
		for (uint8_t i = 0; i < 3; i++) {
			imuCalAcc.sum[i] += unscaled[i];
			imuCalAcc.sumSq[i] += (double)unscaled[i] * unscaled[i];
		}
		imuCalAcc.n++;
	}
}

void I2CSensors::imuFinishCalibration() {
	imuCalActive = false;
	ImuCalibration cal;
	float sigma[3];
	bool still = true;
	double mag2 = 0;
	for (uint8_t i = 0; i < 3; i++) {
		double mean = imuCalAcc.sum[i] / imuCalAcc.n;
		double var = imuCalAcc.sumSq[i] / imuCalAcc.n - mean * mean;
		sigma[i] = var > 0 ? sqrt(var) : 0;
		cal.g0[i] = mean;
		cal.sigma[i] = sigma[i];
		mag2 += mean * mean;
		if (sigma[i] > IMU_CAL_MAX_SIGMA_G) still = false;
	}
	float g0mag = sqrt(mag2);
	cal.scale = g0mag > 0 ? 1.0f / g0mag : 0;

	ImuCalState result = CAL_OK;
	if (!still) {
		result = CAL_ERR_MOTION;
	} else if (cal.scale < IMU_CAL_SCALE_MIN || cal.scale > IMU_CAL_SCALE_MAX) {
		result = CAL_ERR_SCALE;
	}

	if (result == CAL_OK) {
		// Gyro fast offset compensation needs the same stillness just verified. Takes up to
		// ~250 ms; the FIFO (425 ms) covers it.
		BMI160.autoCalibrateGyroOffset();
		BMI160.setGyroOffsetEnabled(true);
		cal.gyroOffset[0] = BMI160.getXGyroOffset();
		cal.gyroOffset[1] = BMI160.getYGyroOffset();
		cal.gyroOffset[2] = BMI160.getZGyroOffset();
		time_t now;
		time(&now);
		cal.calTime = now > 1672531200 ? now : 0;		// 2023-01-01: clock not set before that
		cal.valid = true;
		portENTER_CRITICAL(&imuMux);
		imuCal = cal;
		portEXIT_CRITICAL(&imuMux);
		storeIMUCalibration();
		imuMinMaxValid = false;		// old min/max were taken with the previous scale
	}

	portENTER_CRITICAL(&imuMux);
	imuSnap.calState = result;
	for (uint8_t i = 0; i < 3; i++) imuSnap.calMeasuredSigma[i] = sigma[i];
	portEXIT_CRITICAL(&imuMux);

	bclog.logf(result == CAL_OK ? BCLogger::Log_Info : BCLogger::Log_Warn, BCLogger::TAG_OP,
	           "BMI160 calibration %s: g0=(%.4f, %.4f, %.4f) g, |g0|=%.4f, scale=%.4f, sigma=(%.1f, %.1f, %.1f) mg, gyro offset=(%d, %d, %d)",
	           CAL_STATE_STRING[result], cal.g0[0], cal.g0[1], cal.g0[2], g0mag, cal.scale,
	           sigma[0] * 1000, sigma[1] * 1000, sigma[2] * 1000,
	           cal.gyroOffset[0], cal.gyroOffset[1], cal.gyroOffset[2]);
}

void I2CSensors::getIMUJson(String& json) {
	ImuSnapshot s;
	ImuCalibration c;
	portENTER_CRITICAL(&imuMux);
	s = imuSnap;
	c = imuCal;
	portEXIT_CRITICAL(&imuMux);

	JsonDocument doc;
	doc["running"] = s.running;
	doc["devId"] = s.deviceId;
	doc["sps"] = s.samplesPerSec;
	doc["fifo"] = s.fifoFill;
	doc["fifoMax"] = s.fifoFillMax;
	doc["ovf"] = s.overflows;
	doc["total"] = s.totalSamples;
	doc["mag"] = s.magMean;
	doc["magMax"] = s.magMax;
	for (uint8_t i = 0; i < 3; i++) {
		doc["last"][i] = s.last[i];
		doc["mean"][i] = s.mean[i];
		doc["sigma"][i] = s.sigma[i];
		doc["min"][i] = s.min[i];
		doc["max"][i] = s.max[i];
	}
	JsonObject cal = doc["cal"].to<JsonObject>();
	cal["state"] = CAL_STATE_STRING[s.calState];
	cal["running"] = s.calState == CAL_RUNNING;
	cal["progress"] = s.calProgress;
	cal["samples"] = IMU_CAL_SAMPLES;
	cal["maxSigma"] = IMU_CAL_MAX_SIGMA_G;
	cal["valid"] = c.valid;
	cal["scale"] = c.scale;
	cal["time"] = (int64_t)c.calTime;
	for (uint8_t i = 0; i < 3; i++) {
		cal["attemptSigma"][i] = s.calMeasuredSigma[i];
		cal["g0"][i] = c.g0[i];
		cal["sigma"][i] = c.sigma[i];
		cal["gyro"][i] = c.gyroOffset[i];
	}
	serializeJson(doc, json);
}

void I2CSensors::getIMUDebugPage(String& html) {
	WebPage::begin(html, "IMU (BMI160)", "td.n{text-align:right;font-variant-numeric:tabular-nums}");
	html += F("<p class=\"eyebrow\">Accelerometer 400 Hz, &plusmn;16 g, read from the FIFO every 50 ms. "
	          "Values in g, scale-corrected. Updated every second.</p>\n"
	          "<p id=\"status\" class=\"badge\">loading&hellip;</p>\n"
	          "<h3>Acquisition</h3>\n<table><tbody>\n"
	          "<tr><td>Samples/s</td><td class=\"n\" id=\"sps\"></td></tr>\n"
	          "<tr><td>FIFO fill / max (byte)</td><td class=\"n\" id=\"fifo\"></td></tr>\n"
	          "<tr><td>FIFO overflows</td><td class=\"n\" id=\"ovf\"></td></tr>\n"
	          "<tr><td>Samples total</td><td class=\"n\" id=\"total\"></td></tr>\n"
	          "</tbody></table>\n"
	          "<h3>Live</h3>\n<table><thead><tr><th></th><th>x</th><th>y</th><th>z</th><th>|a|</th></tr></thead><tbody>\n"
	          "<tr><td>Last sample</td><td class=\"n\" id=\"last0\"></td><td class=\"n\" id=\"last1\"></td><td class=\"n\" id=\"last2\"></td><td></td></tr>\n"
	          "<tr><td>Mean (1 s)</td><td class=\"n\" id=\"mean0\"></td><td class=\"n\" id=\"mean1\"></td><td class=\"n\" id=\"mean2\"></td><td class=\"n\" id=\"mag\"></td></tr>\n"
	          "<tr><td>&sigma; (1 s, mg)</td><td class=\"n\" id=\"sigma0\"></td><td class=\"n\" id=\"sigma1\"></td><td class=\"n\" id=\"sigma2\"></td><td></td></tr>\n"
	          "<tr><td>Min</td><td class=\"n\" id=\"min0\"></td><td class=\"n\" id=\"min1\"></td><td class=\"n\" id=\"min2\"></td><td></td></tr>\n"
	          "<tr><td>Max</td><td class=\"n\" id=\"max0\"></td><td class=\"n\" id=\"max1\"></td><td class=\"n\" id=\"max2\"></td><td class=\"n\" id=\"magMax\"></td></tr>\n"
	          "</tbody></table>\n"
	          "<div class=\"row\" style=\"margin-top:8px;\"><a class=\"btn btn-ghost\" href=\"#\" "
	          "onclick=\"req('/debug/imu/reset','Min/max reset');return false;\">Reset min/max</a></div>\n"
	          "<h3>Calibration</h3>\n"
	          "<p class=\"eyebrow\">Keep the bike completely still for 3 s. Determines the gravity vector g0, "
	          "the scale correction, the noise floor and the gyro offset. The ground does not need to be level.</p>\n"
	          "<table><tbody>\n"
	          "<tr><td>State</td><td id=\"calState\"></td></tr>\n"
	          "<tr><td>Last attempt &sigma; (mg)</td><td class=\"n\" id=\"calAttempt\"></td></tr>\n"
	          "<tr><td>Stored</td><td id=\"calTime\"></td></tr>\n"
	          "<tr><td>g0 (g)</td><td class=\"n\" id=\"calG0\"></td></tr>\n"
	          "<tr><td>Scale</td><td class=\"n\" id=\"calScale\"></td></tr>\n"
	          "<tr><td>Noise floor &sigma; (mg)</td><td class=\"n\" id=\"calSigma\"></td></tr>\n"
	          "<tr><td>Gyro offset (LSB, 0.061&deg;/s)</td><td class=\"n\" id=\"calGyro\"></td></tr>\n"
	          "</tbody></table>\n"
	          "<div class=\"row\" style=\"margin-top:8px;\"><a class=\"btn\" id=\"calBtn\" href=\"#\" "
	          "onclick=\"req('/debug/imu/cal','Calibration started - keep still');return false;\">Calibrate (keep bike still)</a></div>\n");
	WebPage::end(html,
		"const $=i=>document.getElementById(i);\n"
		"const f=(v,d)=>Number(v).toFixed(d);\n"
		"const v3=(a,d,m)=>a.map(x=>f(x*(m||1),d)).join(' / ');\n"
		"async function upd(){try{const r=await fetch('/debug/imu.json');if(!r.ok)return;const j=await r.json();\n"
		" const st=$('status');\n"
		" if(!j.running){st.textContent='BMI160 not running (device id 0x'+j.devId.toString(16)+')';st.className='badge badge-error';}\n"
		" else{st.textContent='running';st.className='badge badge-info';}\n"
		" $('sps').textContent=f(j.sps,1);$('fifo').textContent=j.fifo+' / '+j.fifoMax;\n"
		" $('ovf').textContent=j.ovf;$('total').textContent=j.total;\n"
		" for(let i=0;i<3;i++){$('last'+i).textContent=f(j.last[i],3);$('mean'+i).textContent=f(j.mean[i],4);\n"
		"  $('sigma'+i).textContent=f(j.sigma[i]*1000,1);$('min'+i).textContent=f(j.min[i],3);$('max'+i).textContent=f(j.max[i],3);}\n"
		" $('mag').textContent=f(j.mag,4);$('magMax').textContent=f(j.magMax,3);\n"
		" const c=j.cal;\n"
		" $('calState').textContent=c.running?('running '+Math.round(100*c.progress/c.samples)+' %'):c.state;\n"
		" $('calAttempt').textContent=v3(c.attemptSigma,1,1000)+'  (limit '+f(c.maxSigma*1000,0)+')';\n"
		" $('calTime').textContent=c.valid?(c.time?new Date(c.time*1000).toLocaleString():'yes (no clock)'):'no';\n"
		" $('calG0').textContent=c.valid?v3(c.g0,4):'-';$('calScale').textContent=c.valid?f(c.scale,4):'-';\n"
		" $('calSigma').textContent=c.valid?v3(c.sigma,1,1000):'-';$('calGyro').textContent=c.valid?c.gyro.join(' / '):'-';\n"
		"}catch(x){}}\n"
		"upd();setInterval(upd,1000);\n");
}
#endif

