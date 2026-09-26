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
#include <sys/time.h>
#include "LogRecords.h"
#include "RawCapture.h"


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

static int64_t epochMs() {
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

const char* const I2CSensors::CAL_STATE_STRING[] = {"not calibrated", "running", "ok", "failed: not still", "failed: implausible scale"};

// Burst read of BMI160 registers with a repeated start, so no other task's I2C transaction
// can come between address and data, and with the length checked -- BMI160Gen's
// serial_buffer_transfer() does neither and hands back stale buffer bytes on a short read.
bool I2CSensors::imuRead(uint8_t reg, uint8_t* buf, uint16_t n) {
	Wire.beginTransmission(IMU_I2C_ADDR);
	Wire.write(reg);
	if (Wire.endTransmission(false) != 0) return false;
	if (Wire.requestFrom((uint16_t)IMU_I2C_ADDR, (size_t)n) != n) return false;
	for (uint16_t i = 0; i < n; i++) {
		int v = Wire.read();
		if (v < 0) return false;
		buf[i] = (uint8_t)v;
	}
	return true;
}

void I2CSensors::initBMI160() {
	bool ok = BMI160.begin(BMI160GenClass::I2C_MODE, IMU_I2C_ADDR, -1);
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
	rqLoad();

	rqCmd = cli.addCmd("rq", +[](cmd* c) {sensors.handleRqCommand(Command(c));});
	rqCmd.addPositionalArgument("action", "status");
	rqCmd.addPositionalArgument("value", "");
	rqCmd.setDescription("Road quality: rq [status | interval <1..10 s> | shock <g> | wheelbase <m> | gradsrc <baro|imu> | ref <start|stop> | pitchreset | raw <1..600 s|stop>]");

	BMI160.resetFIFO();
	imuSnap.running = true;
	// 4096 byte (ESP-IDF counts byte): FIFO chunk buffer, bclog.logf(), NVS writes (calibration,
	// learned pitch state), the 64-byte log records and a 482-byte snippet piece -- 5120 left
	// 3368 byte free. Internal RAM is scarce, check "mem" after changes here. Priority above
	// FlusherTask/BLE (5), so an SD stall can't starve the FIFO.
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
		const uint32_t pollMs = millis();
		rqBeforeBurst(pollMs);
		rawBeforeBurst();

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

		// FIFO fill level. Read with imuRead(), which checks the length -- the library's
		// getFIFOCount() doesn't, and a partial I2C read left stack garbage in the high byte:
		// a count of ~80 frames with only 2 in the FIFO, and the empty FIFO then reads as
		// 0x8000 frames = -16 g on all axes = phantom 31 g "shocks" (2026-09-26).
		uint8_t cnt[2];
		if (!imuRead(BMI160_RA_FIFO_LENGTH_0, cnt, 2)) {
			imuI2cErrors++;
			continue;							// data stays in the FIFO for the next poll
		}
		uint16_t count = ((cnt[1] & 0x07) << 8) | cnt[0];		// fifo_byte_counter is 11 bit
		uint16_t fill = count;
		if (count >= IMU_FIFO_OVERFLOW_BYTES) {
			// Frames are lost already; flush so the next read starts clean. A calibration in
			// progress is simply continued -- it only needs still samples, not contiguous ones.
			BMI160.resetFIFO();
			count = 0;
			roadq.notifyDataGap();
			rawGap = true;
			portENTER_CRITICAL(&imuMux);
			uint32_t overflows = ++imuSnap.overflows;
			portEXIT_CRITICAL(&imuMux);
			// Throttled: logging can block on the SD mutex, which is what makes the next overflow.
			if (overflows <= 5 || overflows % 100 == 0) {
				bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_OP, "BMI160 FIFO overflow #%u (%u byte) - flushed", overflows, fill);
			}
		}

		uint16_t bytes = count - count % IMU_FRAME_BYTES;
		// Wall-clock time of the newest frame ~ now; each older one is one sample period earlier.
		// Good to a few ms -- plenty for placing a shock or an interval on the map.
		const uint16_t frames = bytes / IMU_FRAME_BYTES;
		const int64_t pollEpochMs = epochMs();
		uint16_t frame = 0;
		while (bytes > 0) {
			uint16_t n = bytes < IMU_CHUNK_BYTES ? bytes : IMU_CHUNK_BYTES;
			if (!imuRead(BMI160_RA_FIFO_DATA, buf, n)) {
				// How much of the FIFO this consumed is unknown: treat as lost samples.
				imuI2cErrors++;
				roadq.notifyDataGap();
				rawGap = true;
				break;
			}
			bytes -= n;
			// Drop "FIFO empty" frames (0x8000 on all three axes -- not a possible measurement,
			// even clipped). With a correct count there are none; count them if there are.
			uint16_t valid = 0;
			for (uint16_t i = 0; i < n; i += IMU_FRAME_BYTES) {
				if (buf[i] == 0x00 && buf[i + 1] == 0x80 && buf[i + 2] == 0x00 && buf[i + 3] == 0x80 && buf[i + 4] == 0x00 && buf[i + 5] == 0x80) {
					imuInvalidFrames++;
					continue;
				}
				if (valid * IMU_FRAME_BYTES != i) memmove(buf + valid * IMU_FRAME_BYTES, buf + i, IMU_FRAME_BYTES);
				valid++;
			}
			if (valid && rawCapturing) {
				rawCaptureChunk(buf, valid, pollEpochMs - (int64_t)(frames - 1 - frame) * 1000 / IMU_ODR_HZ);
			}
			for (uint16_t k = 0; k < valid; k++) {
				const uint8_t* f = buf + k * IMU_FRAME_BYTES;
				int16_t raw[3];
				for (uint8_t a = 0; a < 3; a++) {
					raw[a] = (int16_t)(f[2 * a] | (f[2 * a + 1] << 8));
				}
				imuProcessSample(raw, pollEpochMs - (int64_t)(frames - 1 - frame) * 1000 / IMU_ODR_HZ);
				frame++;
			}
		}
		rqAfterBurst(pollMs);
		rawPublish();
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
		imuSnap.i2cErrors = imuI2cErrors;
		imuSnap.invalidFrames = imuInvalidFrames;
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

void I2CSensors::imuProcessSample(const int16_t raw[3], int64_t sampleEpochMs) {
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

	for (uint8_t i = 0; i < 3; i++) blockSum[i] += a[i];
	blockN++;
	uint8_t ready = roadq.process(a);
	if (ready) rqHandleResults(ready, sampleEpochMs);
	rawAfterSample(raw, sampleEpochMs);

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
		rqSettingsChanged = true;	// noise floor feeds the standstill detection
		float g[3] = {cal.g0[0] * cal.scale, cal.g0[1] * cal.scale, cal.g0[2] * cal.scale};
		pitch.setGravityHint(g);
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

// ******************** Road quality, shocks, accelerometer gradient ********************

RQ::Config I2CSensors::rqConfig(const RqSettings& s) const {
	RQ::Config c = roadq.config();			// keeps the baseline (reference ride) and anything not set here
	c.odrHz = IMU_ODR_HZ;
	c.intervalS = s.intervalS;
	c.shockAbsG = s.shockAbsG;
	c.wheelbaseM = s.wheelbaseM;
	if (imuCal.valid) {
		float n = sqrtf(imuCal.sigma[0] * imuCal.sigma[0] + imuCal.sigma[1] * imuCal.sigma[1] + imuCal.sigma[2] * imuCal.sigma[2]);
		if (n > 0) c.noiseG = n;
	}
	return c;
}

void I2CSensors::rqLoad() {
	RqSettings s;
	RQ::PitchEstimator::State st;
	Preferences p;
	p.begin("RoadQ", true);
	s.intervalS = p.getUChar("rqIntv", s.intervalS);
	s.shockAbsG = p.getFloat("shockG", s.shockAbsG);
	s.wheelbaseM = p.getFloat("wheelbase", s.wheelbaseM);
	s.gradSrc = p.getUChar("gradSrc", s.gradSrc);
	float baseline = p.getFloat("baseG", 0);
	bool baselineCal = p.getBool("baseCal", false);
	baselineTime = p.getLong64("baseTime", 0);
	st.ef[0] = p.getFloat("pEfX", 0);
	st.ef[1] = p.getFloat("pEfY", 0);
	st.ef[2] = p.getFloat("pEfZ", 0);
	st.efWeight = p.getFloat("pEfW", 0);
	st.biasRad = p.getFloat("pBias", 0);
	st.biasDistM = p.getFloat("pBiasD", 0);
	p.end();

	portENTER_CRITICAL(&imuMux);
	rqSettings = s;
	portEXIT_CRITICAL(&imuMux);

	RQ::Config c = rqConfig(s);
	if (baselineCal && baseline > 0) {
		c.baselineG = baseline;
		c.baselineCalibrated = true;
	}
	roadq.configure(c);
	pitch.setState(st);
	pitchSaved = pitch.state();
	if (imuCal.valid) {
		float g[3] = {imuCal.g0[0] * imuCal.scale, imuCal.g0[1] * imuCal.scale, imuCal.g0[2] * imuCal.scale};
		pitch.setGravityHint(g);
	}
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_OP, "Road quality: interval %u s, shock %.1f g, baseline %.0f mg (%s), forward axis %s, bias %.2f deg over %.0f m, gradient source %s",
	           c.intervalS, c.shockAbsG, c.baselineG * 1000, c.baselineCalibrated ? "reference ride" : "default",
	           pitch.forwardValid() ? "learned" : "unknown", pitch.biasDeg(), pitch.biasDistance(), s.gradSrc == GRAD_IMU ? "accelerometer" : "barometer");
}

void I2CSensors::rqSaveBaseline() {
	time_t now;
	time(&now);
	baselineTime = now > 1672531200 ? now : 0;
	Preferences p;
	p.begin("RoadQ", false);
	p.putFloat("baseG", roadq.config().baselineG);
	p.putLong64("baseTime", baselineTime);
	p.putBool("baseCal", true);
	p.end();
}

void I2CSensors::rqSavePitch(uint32_t nowMs) {
	RQ::PitchEstimator::State st = pitch.state();
	Preferences p;
	p.begin("RoadQ", false);
	p.putFloat("pEfX", st.ef[0]);
	p.putFloat("pEfY", st.ef[1]);
	p.putFloat("pEfZ", st.ef[2]);
	p.putFloat("pEfW", st.efWeight);
	p.putFloat("pBias", st.biasRad);
	p.putFloat("pBiasD", st.biasDistM);
	p.end();
	pitchSaved = st;
	pitchSavedMs = nowMs;
}

void I2CSensors::rqBeforeBurst(uint32_t nowMs) {
	if (rqSettingsChanged.exchange(false)) {
		portENTER_CRITICAL(&imuMux);
		RqSettings s = rqSettings;
		portEXIT_CRITICAL(&imuMux);
		roadq.configure(rqConfig(s));
	}
	switch (refRequest.exchange(0)) {
	case 1:
		roadq.startReference();
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_OP, "Road quality: reference ride started - ride %u s on smooth asphalt at >= %.0f km/h",
		           roadq.config().refSeconds, roadq.config().refMinKmh);
		break;
	case 2:
		roadq.cancelReference();
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_OP, "Road quality: reference ride cancelled");
		break;
	}
	if (pitchResetRequest.exchange(false)) {
		pitch.resetLearning();
		rqSavePitch(nowMs);
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_OP, "Accelerometer gradient: learned forward axis and bias reset");
	}

	bool baro = false;
	float baroPct = 0, baroDist = 0;
	portENTER_CRITICAL(&imuMux);
	if (baroMail.pending) {
		baro = true;
		baroPct = baroMail.pct;
		baroDist = baroMail.distM;
		baroMail.pending = false;
	}
	portEXIT_CRITICAL(&imuMux);
	if (baro) pitch.onBaroGradient(baroPct, baroDist);

	// Speed: wheel sensor if it is current. A stopped wheel sends no new revolutions, and
	// Distance::calculateSpeed() reports 0 after 1.2 s -- a stale 0 is still a valid 0.
	const float v = stats.getSpeed();
	const uint32_t upd = stats.getSpeedUpdateMs();
	const bool wheel = stats.isConnected() && !isnan(v) && upd != 0 && (v < 0.5f || nowMs - upd < 3000);
	if (wheel) {
		roadq.setSpeed(v, RQ::SpeedSource::WHEEL);
		if (upd != lastSpeedUpdateSeen) {
			pitch.onSpeed(upd, v);
			lastSpeedUpdateSeen = upd;
		}
	} else {
		// Phone GPS speed as fallback -- too laggy for dv/dt, so it only feeds the normalisation.
		// getGpsFix() isn't synchronised with the BLE task (same as Statistics::dataStore()).
		SGpsFix gps = bleDevs.getGpsFix();
		if (gps.valid && gps.hasSpeed && gps.fixAgeMs < 5000) {
			roadq.setSpeed(gps.speedCms * 0.036f, RQ::SpeedSource::GPS);
			roadq.setLongitudinal(pitch.forward(), pitch.longitudinalAccelG(), pitch.longitudinalValid(nowMs));
			curSpeedKmh = gps.speedCms * 0.036f;
			curSpeedSrc = (uint8_t)RQ::SpeedSource::GPS;
			curSpeedUpdMs = 0;
			portENTER_CRITICAL(&imuMux);
			rqSnap.speedKmh = gps.speedCms * 0.036f;
			rqSnap.speedSrc = (uint8_t)RQ::SpeedSource::GPS;
			portEXIT_CRITICAL(&imuMux);
			return;
		} else {
			roadq.setSpeed(NAN, RQ::SpeedSource::NONE);
		}
	}
	roadq.setLongitudinal(pitch.forward(), pitch.longitudinalAccelG(), pitch.longitudinalValid(nowMs));
	curSpeedKmh = wheel ? v : NAN;
	curSpeedSrc = (uint8_t)(wheel ? RQ::SpeedSource::WHEEL : RQ::SpeedSource::NONE);
	curSpeedUpdMs = wheel ? upd : 0;
	portENTER_CRITICAL(&imuMux);
	rqSnap.speedKmh = curSpeedKmh;
	rqSnap.speedSrc = curSpeedSrc;
	portEXIT_CRITICAL(&imuMux);
}

void I2CSensors::rqHandleResults(uint8_t ready, int64_t sampleEpochMs) {
	const SGpsFix gps = bleDevs.getGpsFix();
	float gradImu = NAN;
	float g;
	if (pitch.gradient(g, millis(), 3000)) gradImu = g;

	if (ready & RQ::RoadQuality::READY_INTERVAL) {
		const RQ::IntervalResult& r = roadq.interval();
		LogRec::RoadQuality rec = {};
		rec.timestamp = sampleEpochMs / 1000;
		rec.timestampMs = sampleEpochMs % 1000;
		rec.intervalMs = intervalStartEpochMs ? LogRec::toU16(sampleEpochMs - intervalStartEpochMs) : r.sampleCount * 1000 / IMU_ODR_HZ;
		intervalStartEpochMs = sampleEpochMs;
		rec.sampleCount = r.sampleCount;
		rec.speedCms = LogRec::speedCms(r.speedKmh);
		rec.rmsVertMg = LogRec::mg(r.rmsVertG);
		rec.rmsHorizMg = LogRec::mg(r.rmsHorizG);
		rec.peakVertMaxMg = LogRec::mgSigned(r.peakVertMaxG);
		rec.peakVertMinMg = LogRec::mgSigned(r.peakVertMinG);
		rec.peakTotalMg = LogRec::mg(r.peakTotalG);
		rec.roughnessX100 = isnan(r.roughness) ? LogRec::U16_INVALID : LogRec::toU16(r.roughness * 100);
		rec.roadClass = r.roadClass;
		rec.flags = r.flags | (gps.valid ? RQ::IF_GPS_VALID : 0);
		rec.recordType = LogRec::TYPE_ROAD_QUALITY;
		rec.formatVersion = LogRec::FORMAT_VERSION;
		rec.vdvVert = r.vdvVert;
		rec.distanceM = r.distanceM;
		rec.countOverT1 = r.countOverT1;
		rec.countOverT2 = r.countOverT2;
		rec.eventsLogged = r.eventsLogged;
		rec.eventsSuppressed = r.eventsSuppressed;
		if (gps.valid) {
			rec.gpsLatitudeE7 = gps.latitudeE7;
			rec.gpsLongitudeE7 = gps.longitudeE7;
			rec.gpsFixAgeMs = gps.fixAgeMs;
			rec.gpsAccuracyMX10 = gps.hasAccuracy ? gps.accuracyMX10 : 0;
		}
		rec.gradImuX100 = LogRec::gradX100(gradImu);
		bclog.appendRecord(rec);
		roadClass = r.roadClass;

		portENTER_CRITICAL(&imuMux);
		rqSnap.last = r;
		rqSnap.haveInterval = true;
		rqSnap.rqRecords++;
		portEXIT_CRITICAL(&imuMux);
	}

	if (ready & RQ::RoadQuality::READY_SHOCK) {
		const RQ::ShockResult& sh = roadq.shock();
		const int64_t peakMs = sampleEpochMs - (int64_t)sh.samplesAgo * 1000 / IMU_ODR_HZ;
		LogRec::Shock rec = {};
		rec.timestamp = peakMs / 1000;
		rec.timestampMs = peakMs % 1000;
		rec.durationMs = LogRec::toU16(sh.durationMs);
		rec.peakTotalMg = LogRec::mg(sh.peakTotalG);
		rec.peakVertMaxMg = LogRec::mgSigned(sh.peakVertMaxG);
		rec.peakVertMinMg = LogRec::mgSigned(sh.peakVertMinG);
		rec.peakHorizMg = LogRec::mg(sh.peakHorizG);
		rec.preRmsMg = LogRec::mg(sh.preRmsG);
		rec.speedCms = LogRec::speedCms(sh.speedKmh);
		rec.secondPeakMg = LogRec::mg(sh.secondPeakG);
		rec.secondPeakDelayMs = LogRec::toU16(sh.secondPeakDelayMs);
		rec.severity = sh.severity;
		rec.flags = sh.flags | (gps.valid ? RQ::SF_GPS_VALID : 0);
		rec.recordType = LogRec::TYPE_SHOCK;
		rec.formatVersion = LogRec::FORMAT_VERSION;
		rec.vdv = sh.vdv;
		rec.samplesOverThr = sh.samplesOverThr;
		rec.thresholdMg = LogRec::mg(sh.thresholdG);
		if (gps.valid) {
			rec.gpsLatitudeE7 = gps.latitudeE7;
			rec.gpsLongitudeE7 = gps.longitudeE7;
			rec.gpsFixAgeMs = gps.fixAgeMs;
			rec.gpsAccuracyMX10 = gps.hasAccuracy ? gps.accuracyMX10 : 0;
		}
		rec.eventSeq = sh.seq;
		bclog.appendRecord(rec);

		// Raw snippet around the peak, written once SNIP_POST frames after it are in (see
		// rawAfterSample()). The shock is reported ~0.4 s after its peak, so that is soon.
		if (!snipPending) {
			snipPending = true;
			snipPeakIdx = imuTotalSamples - sh.samplesAgo;
			snipSeq = sh.seq;
			snipSpeedKmh = sh.speedKmh;
		} else {
			snippetsDropped++;			// can't happen with the 400 ms second-peak window, but be safe
		}

		portENTER_CRITICAL(&imuMux);
		rqSnap.lastShock = sh;
		rqSnap.lastShockTime = rec.timestamp;
		rqSnap.haveShock = true;
		portEXIT_CRITICAL(&imuMux);
		// Rare (rate-limited to 20/min), so a log line is affordable even from this task
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_OP, "Shock #%u: %.1f g (severity %u), second peak %.1f g after %.0f ms, %.1f km/h, flags 0x%02X",
		           sh.seq, sh.peakTotalG, sh.severity, sh.secondPeakG, sh.secondPeakDelayMs, sh.speedKmh, rec.flags);
	}
}

void I2CSensors::rqAfterBurst(uint32_t nowMs) {
	if (blockN) {
		pitch.addBlock(blockSum, blockN, nowMs);
		blockSum[0] = blockSum[1] = blockSum[2] = 0;
		blockN = 0;
	}

	const RQ::RoadQuality::RefState refNow = roadq.refState();
	if (refNow != refPrev) {
		if (refNow == RQ::RoadQuality::RefState::DONE) {
			rqSaveBaseline();
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_OP, "Road quality: reference ride done - baseline %.0f mg at %.0f km/h", roadq.config().baselineG * 1000, roadq.config().vRefKmh);
		} else if (refNow == RQ::RoadQuality::RefState::FAILED) {
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_OP, "Road quality: reference ride failed - only %u of %u s at >= %.0f km/h within %u s",
			           roadq.refProgressS(), roadq.config().refSeconds, roadq.config().refMinKmh, roadq.config().refTimeoutS);
		}
		refPrev = refNow;
	}

	// Persist the learned pitch state now and then, if it moved noticeably
	if (nowMs - pitchSavedMs > PITCH_SAVE_INTERVAL_MS) {
		const RQ::PitchEstimator::State st = pitch.state();
		const float d = st.ef[0] * pitchSaved.ef[0] + st.ef[1] * pitchSaved.ef[1] + st.ef[2] * pitchSaved.ef[2];
		if ((pitch.forwardValid() && d < 0.9998f) || fabsf(st.biasRad - pitchSaved.biasRad) > 0.001f || st.biasDistM - pitchSaved.biasDistM > 1000) {
			rqSavePitch(nowMs);
		} else {
			pitchSavedMs = nowMs;
		}
	}
	rqPublish(nowMs);
}

void I2CSensors::rqPublish(uint32_t nowMs) {
	float gi = NAN;
	bool gradValid = pitch.gradient(gi, nowMs, 3000);
	const RQ::Config& c = roadq.config();
	portENTER_CRITICAL(&imuMux);
	rqSnap.shocks = roadq.shocksLogged();
	rqSnap.suppressed = roadq.shocksSuppressed();
	rqSnap.rms1sG = roadq.rmsVert1sG();
	rqSnap.still = roadq.isStill();
	rqSnap.refState = roadq.refState();
	rqSnap.refProgressS = roadq.refProgressS();
	rqSnap.refElapsedS = roadq.refElapsedS();
	rqSnap.baselineG = c.baselineG;
	rqSnap.baselineCal = c.baselineCalibrated;
	rqSnap.baselineTime = baselineTime;
	rqSnap.efValid = pitch.forwardValid();
	for (uint8_t i = 0; i < 3; i++) rqSnap.ef[i] = pitch.forward()[i];
	rqSnap.slope = pitch.regressionSlope();
	rqSnap.biasDeg = pitch.biasDeg();
	rqSnap.biasDistM = pitch.biasDistance();
	rqSnap.biasConverged = pitch.biasConverged();
	rqSnap.gradValid = gradValid;
	rqSnap.gradImu = gi;
	rqSnap.gradRaw = pitch.rawGradientPct();
	rqSnap.frozen = pitch.frozen();
	rqSnap.pitchUpdates = pitch.updates();
	portEXIT_CRITICAL(&imuMux);
}

float I2CSensors::getRoughness() {
	portENTER_CRITICAL(&imuMux);
	float r = rqSnap.haveInterval ? rqSnap.last.roughness : NAN;
	portEXIT_CRITICAL(&imuMux);
	return r;
}

uint32_t I2CSensors::getShockCount() {
	portENTER_CRITICAL(&imuMux);
	uint32_t n = rqSnap.shocks;
	portEXIT_CRITICAL(&imuMux);
	return n;
}

float I2CSensors::getImuGradient() {
	portENTER_CRITICAL(&imuMux);
	float g = rqSnap.gradValid ? rqSnap.gradImu : NAN;
	portEXIT_CRITICAL(&imuMux);
	return g;
}

bool I2CSensors::imuGradientForDisplay(float& pct) {
	portENTER_CRITICAL(&imuMux);
	bool use = rqSettings.gradSrc == GRAD_IMU && rqSnap.gradValid && rqSnap.biasConverged;
	float g = rqSnap.gradImu;
	portEXIT_CRITICAL(&imuMux);
	if (use) pct = g;
	return use;
}

void I2CSensors::feedBaroGradient(float pct, float deltaDistM) {
	portENTER_CRITICAL(&imuMux);
	baroMail.pending = true;
	baroMail.pct = pct;
	baroMail.distM = deltaDistM;
	rqSnap.gradBaro = pct;
	portEXIT_CRITICAL(&imuMux);
}

bool I2CSensors::requestRefRide(bool start) {
	portENTER_CRITICAL(&imuMux);
	bool running = imuSnap.running;
	portEXIT_CRITICAL(&imuMux);
	if (running) refRequest = start ? 1 : 2;
	return running;
}

void I2CSensors::printRqStatus() {
	RqSnapshot r;
	RqSettings s;
	portENTER_CRITICAL(&imuMux);
	r = rqSnap;
	s = rqSettings;
	portEXIT_CRITICAL(&imuMux);
	Serial.printf("Road quality: interval %u s, shock threshold %.1f g, wheelbase %.2f m, gradient source %s\n",
	              s.intervalS, s.shockAbsG, s.wheelbaseM, s.gradSrc == GRAD_IMU ? "imu" : "baro");
	Serial.printf("  baseline %.0f mg (%s), reference ride %s (%u/%u s)\n", r.baselineG * 1000, r.baselineCal ? "calibrated" : "default",
	              RQ::RoadQuality::refStateString(r.refState), r.refProgressS, r.refElapsedS);
	if (r.haveInterval) {
		Serial.printf("  last interval: class %u, R %.2f, rms_v %.0f mg, rms_h %.0f mg, peak %.2f g, %.1f km/h, flags 0x%02X\n",
		              r.last.roadClass, r.last.roughness, r.last.rmsVertG * 1000, r.last.rmsHorizG * 1000, r.last.peakTotalG, r.last.speedKmh, r.last.flags);
	}
	Serial.printf("  shocks %u logged, %u suppressed; records written %u, dropped %u\n", r.shocks, r.suppressed, bclog.getRecordsWritten(), bclog.getRecordsDropped());
	RawSnapshot w;
	portENTER_CRITICAL(&imuMux);
	w = rawSnap;
	portEXIT_CRITICAL(&imuMux);
	char capName[40], snipName[40];
	bclog.getRawFileName(BCLogger::RAW_CAPTURE, capName, sizeof(capName));
	bclog.getRawFileName(BCLogger::RAW_SNIPPETS, snipName, sizeof(snipName));
	Serial.printf("  raw capture %s %.1f/%.1f s (%s, %u byte); shock snippets %u (%s, %u byte), %u dropped; raw bytes dropped %u\n",
	              w.capturing ? "running" : "idle", w.captureFrames / (float)IMU_ODR_HZ, w.captureTarget / (float)IMU_ODR_HZ,
	              capName[0] ? capName : "-", bclog.getRawBytes(BCLogger::RAW_CAPTURE), w.snippets, snipName[0] ? snipName : "-",
	              bclog.getRawBytes(BCLogger::RAW_SNIPPETS), w.snippetsDropped, bclog.getRawDropped());
	Serial.printf("  gradient: imu %.1f %% (%s%s), raw %.1f %%, baro %.1f %%, forward axis %s (slope %.2f), bias %.2f deg over %.0f m%s\n",
	              r.gradImu, r.gradValid ? "valid" : "invalid", r.frozen ? ", held" : "", r.gradRaw, r.gradBaro,
	              r.efValid ? "learned" : "unknown", r.slope, r.biasDeg, r.biasDistM, r.biasConverged ? "" : " (not converged)");
}

void I2CSensors::handleRqCommand(const Command& cmd) {
	const String action = cmd.getArgument("action").getValue();
	const String value = cmd.getArgument("value").getValue();
	portENTER_CRITICAL(&imuMux);
	RqSettings s = rqSettings;
	bool running = imuSnap.running;
	portEXIT_CRITICAL(&imuMux);
	if (!running) {
		Serial.println("IMU not running");
		return;
	}

	bool changed = false;
	if (action.equalsIgnoreCase("status")) {
		printRqStatus();
	} else if (action.equalsIgnoreCase("interval")) {
		long v = value.toInt();
		if (v >= 1 && v <= 10) {s.intervalS = v; changed = true;}
		else Serial.println("interval: 1..10 s");
	} else if (action.equalsIgnoreCase("shock")) {
		float v = value.toFloat();
		if (v >= 1.5f && v <= 15.0f) {s.shockAbsG = v; changed = true;}
		else Serial.println("shock: 1.5..15 g");
	} else if (action.equalsIgnoreCase("wheelbase")) {
		float v = value.toFloat();
		if (v >= 0.8f && v <= 1.6f) {s.wheelbaseM = v; changed = true;}
		else Serial.println("wheelbase: 0.8..1.6 m");
	} else if (action.equalsIgnoreCase("gradsrc")) {
		if (value.equalsIgnoreCase("baro")) {s.gradSrc = GRAD_BARO; changed = true;}
		else if (value.equalsIgnoreCase("imu")) {s.gradSrc = GRAD_IMU; changed = true;}
		else Serial.println("gradsrc: baro | imu");
	} else if (action.equalsIgnoreCase("ref")) {
		if (value.equalsIgnoreCase("start")) requestRefRide(true);
		else if (value.equalsIgnoreCase("stop")) requestRefRide(false);
		else Serial.println("ref: start | stop");
	} else if (action.equalsIgnoreCase("pitchreset")) {
		requestPitchReset();
	} else if (action.equalsIgnoreCase("raw")) {
		if (value.equalsIgnoreCase("stop")) requestRawCapture(0);
		else {
			long sec = value.length() ? value.toInt() : 60;
			if (sec >= 1 && sec <= RAW_MAX_CAPTURE_S) requestRawCapture(sec);
			else Serial.printf("raw: 1..%u s | stop\n", RAW_MAX_CAPTURE_S);
		}
	} else {
		Serial.println(rqCmd.getDescription());
	}

	if (changed) {
		portENTER_CRITICAL(&imuMux);
		rqSettings = s;
		portEXIT_CRITICAL(&imuMux);
		rqSettingsChanged = true;
		Preferences p;
		p.begin("RoadQ", false);
		p.putUChar("rqIntv", s.intervalS);
		p.putFloat("shockG", s.shockAbsG);
		p.putFloat("wheelbase", s.wheelbaseM);
		p.putUChar("gradSrc", s.gradSrc);
		p.end();
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_CLI, "Road quality settings: interval %u s, shock %.1f g, wheelbase %.2f m, gradient source %s",
		           s.intervalS, s.shockAbsG, s.wheelbaseM, s.gradSrc == GRAD_IMU ? "imu" : "baro");
	}
}

// ******************** Raw data: capture on demand, shock snippets (RawCapture.h) ********************

bool I2CSensors::requestRawCapture(uint16_t seconds) {
	portENTER_CRITICAL(&imuMux);
	bool running = imuSnap.running;
	portEXIT_CRITICAL(&imuMux);
	if (!running) return false;
	rawRequest = seconds > RAW_MAX_CAPTURE_S ? RAW_MAX_CAPTURE_S : seconds;
	return true;
}

void I2CSensors::rawFileHeader(uint8_t kind, uint8_t* out) {
	RawCap::FileHeader h = {};
	h.magic[0] = 'B'; h.magic[1] = 'C'; h.magic[2] = 'R'; h.magic[3] = 'W';
	h.version = RawCap::VERSION;
	h.kind = kind;
	h.odrHz = IMU_ODR_HZ;
	h.lsbPerG = IMU_LSB_PER_G;
	h.scale = imuCal.scale;					// ImuTask is imuCal's only writer
	for (uint8_t i = 0; i < 3; i++) h.g0[i] = imuCal.g0[i];
	h.noiseG = roadq.config().noiseG;
	h.startEpochMs = epochMs();
	h.wheelbaseM = roadq.config().wheelbaseM;
	h.intervalS = roadq.config().intervalS;
	h.rangeG = 16;
	h.preMs = SNIP_PRE_MS;
	h.postMs = SNIP_POST_MS;
	memcpy(out, &h, sizeof(h));
}

void I2CSensors::rawBeforeBurst() {
	const int32_t req = rawRequest.exchange(-1);
	if (req < 0) return;
	if (rawCapturing) {
		bclog.rawClose(BCLogger::RAW_CAPTURE);
		rawCapturing = false;
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_OP, "Raw capture stopped after %.1f s", rawCaptureFrames / (float)IMU_ODR_HZ);
	}
	if (req == 0) return;
	uint8_t msg[BCLogger::RAW_PREFIX + sizeof(RawCap::FileHeader)];
	rawFileHeader(RawCap::KIND_CAPTURE, msg + BCLogger::RAW_PREFIX);
	if (!bclog.rawOpen(BCLogger::RAW_CAPTURE, msg, sizeof(msg))) {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_OP, "Raw capture not started - raw buffer full");
		return;
	}
	rawCapturing = true;
	rawCaptureTarget = rawCaptureLeft = (uint32_t)req * IMU_ODR_HZ;
	rawCaptureFrames = 0;
	rawBlockNo = 0;
	rawGap = false;
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_OP, "Raw capture started: %d s, ~%u KB", req, (unsigned)(req * IMU_ODR_HZ * 6 / 1000 * 21 / 20));
}

void I2CSensors::rawCaptureChunk(const uint8_t* frames, uint16_t n, int64_t firstEpochMs) {
	const uint16_t take = n < rawCaptureLeft ? n : rawCaptureLeft;
	uint8_t msg[BCLogger::RAW_PREFIX + sizeof(RawCap::BlockHeader) + IMU_CHUNK_BYTES];
	RawCap::BlockHeader h = {};
	h.sync = RawCap::SYNC;
	h.type = RawCap::BLOCK_CONTINUOUS;
	h.flags = (rawGap ? RawCap::BF_GAP_BEFORE : 0) | (curSpeedSrc == (uint8_t)RQ::SpeedSource::GPS ? RawCap::BF_SPEED_FROM_GPS : 0);
	h.count = take;
	h.speedCms = LogRec::speedCms(curSpeedKmh);		// NAN -> 0xFFFF = RawCap::SPEED_UNKNOWN
	h.epochMs = firstEpochMs;
	h.ref = rawBlockNo++;
	if (curSpeedSrc == (uint8_t)RQ::SpeedSource::WHEEL && curSpeedUpdMs) {
		uint32_t age = millis() - curSpeedUpdMs;
		h.speedAgeMs = age < 0xFFFE ? age : 0xFFFE;
	} else {
		h.speedAgeMs = RawCap::SPEED_AGE_UNKNOWN;
	}
	memcpy(msg + BCLogger::RAW_PREFIX, &h, sizeof(h));
	memcpy(msg + BCLogger::RAW_PREFIX + sizeof(h), frames, take * IMU_FRAME_BYTES);
	// A dropped block (card too slow for > 3 s) is marked on the next one
	rawGap = !bclog.rawWrite(BCLogger::RAW_CAPTURE, msg, BCLogger::RAW_PREFIX + sizeof(h) + take * IMU_FRAME_BYTES);
	rawCaptureLeft -= take;
	rawCaptureFrames += take;
	if (rawCaptureLeft == 0) {
		bclog.rawClose(BCLogger::RAW_CAPTURE);
		rawCapturing = false;
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_OP, "Raw capture complete: %.1f s", rawCaptureFrames / (float)IMU_ODR_HZ);
	}
}

void I2CSensors::rawAfterSample(const int16_t raw[3], int64_t sampleEpochMs) {
	const uint32_t idx = imuTotalSamples;			// this sample's number, as used for snipPeakIdx
	int16_t* slot = rawRing[idx % RAW_RING];
	slot[0] = raw[0];
	slot[1] = raw[1];
	slot[2] = raw[2];
	if (snipPending && idx - snipPeakIdx >= SNIP_POST - 1 && (int32_t)(idx - snipPeakIdx) >= 0) {
		snipPending = false;
		rawWriteSnippet(sampleEpochMs);
	}
}

void I2CSensors::rawWriteSnippet(int64_t lastEpochMs) {
	if (!snipFileOpen) {
		uint8_t msg[BCLogger::RAW_PREFIX + sizeof(RawCap::FileHeader)];
		rawFileHeader(RawCap::KIND_SNIPPETS, msg + BCLogger::RAW_PREFIX);
		if (!bclog.rawOpen(BCLogger::RAW_SNIPPETS, msg, sizeof(msg))) {
			snippetsDropped++;
			return;
		}
		snipFileOpen = true;
	}
	// Sent in pieces of <= RAW_MAX_MSG, the header first. Only if all of it fits right now:
	// a half-written block would cost the reader a resync. Single producer, so the space
	// can only grow while the pieces go in.
	static constexpr uint16_t PIECE = 80;		// frames per message: 2 + 480 byte
	const size_t needed = (4 + BCLogger::RAW_PREFIX + sizeof(RawCap::BlockHeader))
	                    + ((SNIP_FRAMES + PIECE - 1) / PIECE) * (4 + BCLogger::RAW_PREFIX + PIECE * IMU_FRAME_BYTES)
	                    + (4 + BCLogger::RAW_PREFIX);		// the close

	if (bclog.rawSpace() < needed) {
		snippetsDropped++;
		return;
	}
	uint8_t msg[BCLogger::RAW_PREFIX + PIECE * IMU_FRAME_BYTES];
	static_assert(sizeof(msg) <= BCLogger::RAW_MAX_MSG, "snippet piece too big");
	RawCap::BlockHeader h = {};
	h.sync = RawCap::SYNC;
	h.type = RawCap::BLOCK_SHOCK;
	h.flags = curSpeedSrc == (uint8_t)RQ::SpeedSource::GPS ? RawCap::BF_SPEED_FROM_GPS : 0;
	h.count = SNIP_FRAMES;
	h.speedCms = LogRec::speedCms(snipSpeedKmh);
	h.epochMs = lastEpochMs - (int64_t)(SNIP_FRAMES - 1) * 1000 / IMU_ODR_HZ;
	h.ref = snipSeq;
	h.speedAgeMs = RawCap::SPEED_AGE_UNKNOWN;
	memcpy(msg + BCLogger::RAW_PREFIX, &h, sizeof(h));
	bool ok = bclog.rawWrite(BCLogger::RAW_SNIPPETS, msg, BCLogger::RAW_PREFIX + sizeof(h));
	const uint32_t first = imuTotalSamples - (SNIP_FRAMES - 1);
	for (uint16_t k0 = 0; ok && k0 < SNIP_FRAMES; k0 += PIECE) {
		const uint16_t cnt = SNIP_FRAMES - k0 < PIECE ? SNIP_FRAMES - k0 : PIECE;
		int16_t* frames = reinterpret_cast<int16_t*>(msg + BCLogger::RAW_PREFIX);		// 2-byte aligned
		for (uint16_t k = 0; k < cnt; k++) {
			const int16_t* src = rawRing[(first + k0 + k) % RAW_RING];
			frames[3 * k] = src[0];
			frames[3 * k + 1] = src[1];
			frames[3 * k + 2] = src[2];
		}
		ok = bclog.rawWrite(BCLogger::RAW_SNIPPETS, msg, BCLogger::RAW_PREFIX + cnt * IMU_FRAME_BYTES);
	}
	// Close again: an open file holds ~4 KB of internal RAM for the whole session otherwise.
	// The next snippet's first WRITE reopens it for appending (BCLogger::rawHandle()).
	bclog.rawClose(BCLogger::RAW_SNIPPETS);
	if (ok) snippets++;
	else snippetsDropped++;
}

void I2CSensors::rawPublish() {
	portENTER_CRITICAL(&imuMux);
	rawSnap.capturing = rawCapturing;
	rawSnap.captureFrames = rawCaptureFrames;
	rawSnap.captureTarget = rawCaptureTarget;
	rawSnap.captureBlocks = rawBlockNo;
	rawSnap.snippets = snippets;
	rawSnap.snippetsDropped = snippetsDropped;
	portEXIT_CRITICAL(&imuMux);
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
	doc["i2cErr"] = s.i2cErrors;
	doc["invalid"] = s.invalidFrames;
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

	// Road quality, shocks, gradient. NaN serialises as null (ArduinoJson default).
	RqSnapshot r;
	RqSettings rs;
	portENTER_CRITICAL(&imuMux);
	r = rqSnap;
	rs = rqSettings;
	portEXIT_CRITICAL(&imuMux);
	JsonObject rq = doc["rq"].to<JsonObject>();
	rq["interval"] = rs.intervalS;
	rq["shockG"] = rs.shockAbsG;
	rq["wheelbase"] = rs.wheelbaseM;
	rq["gradSrc"] = rs.gradSrc == GRAD_IMU ? "imu" : "baro";
	rq["rms1s"] = r.rms1sG;
	rq["still"] = r.still;
	rq["speed"] = r.speedKmh;
	rq["speedSrc"] = r.speedSrc == (uint8_t)RQ::SpeedSource::WHEEL ? "wheel" : (r.speedSrc == (uint8_t)RQ::SpeedSource::GPS ? "gps" : "none");
	rq["shocks"] = r.shocks;
	rq["suppressed"] = r.suppressed;
	rq["records"] = r.rqRecords;
	rq["written"] = bclog.getRecordsWritten();
	rq["dropped"] = bclog.getRecordsDropped();
	if (r.haveInterval) {
		JsonObject l = rq["last"].to<JsonObject>();
		l["cls"] = r.last.roadClass;
		l["R"] = r.last.roughness;
		l["rmsV"] = r.last.rmsVertG;
		l["rmsH"] = r.last.rmsHorizG;
		l["pMax"] = r.last.peakVertMaxG;
		l["pMin"] = r.last.peakVertMinG;
		l["pT"] = r.last.peakTotalG;
		l["vdv"] = r.last.vdvVert;
		l["speed"] = r.last.speedKmh;
		l["dist"] = r.last.distanceM;
		l["c1"] = r.last.countOverT1;
		l["c2"] = r.last.countOverT2;
		l["flags"] = r.last.flags;
	}
	if (r.haveShock) {
		JsonObject k = rq["shock"].to<JsonObject>();
		k["seq"] = r.lastShock.seq;
		k["time"] = (int64_t)r.lastShockTime;
		k["peak"] = r.lastShock.peakTotalG;
		k["vMax"] = r.lastShock.peakVertMaxG;
		k["vMin"] = r.lastShock.peakVertMinG;
		k["sev"] = r.lastShock.severity;
		k["dur"] = r.lastShock.durationMs;
		k["second"] = r.lastShock.secondPeakG;
		k["delay"] = r.lastShock.secondPeakDelayMs;
		k["thr"] = r.lastShock.thresholdG;
		k["pre"] = r.lastShock.preRmsG;
		k["speed"] = r.lastShock.speedKmh;
		k["flags"] = r.lastShock.flags;
	}
	JsonObject ref = rq["ref"].to<JsonObject>();
	ref["state"] = RQ::RoadQuality::refStateString(r.refState);
	ref["running"] = r.refState == RQ::RoadQuality::RefState::RUNNING;
	ref["progress"] = r.refProgressS;
	ref["elapsed"] = r.refElapsedS;
	ref["target"] = roadq.config().refSeconds;			// compile-time defaults, never changed at runtime
	ref["minKmh"] = roadq.config().refMinKmh;
	ref["baseline"] = r.baselineG;
	ref["cal"] = r.baselineCal;
	ref["time"] = (int64_t)r.baselineTime;
	RawSnapshot w;
	portENTER_CRITICAL(&imuMux);
	w = rawSnap;
	portEXIT_CRITICAL(&imuMux);
	char capName[40], snipName[40];
	bclog.getRawFileName(BCLogger::RAW_CAPTURE, capName, sizeof(capName));
	bclog.getRawFileName(BCLogger::RAW_SNIPPETS, snipName, sizeof(snipName));
	JsonObject rw = doc["raw"].to<JsonObject>();
	rw["capturing"] = w.capturing;
	rw["seconds"] = w.captureFrames / (float)IMU_ODR_HZ;
	rw["target"] = w.captureTarget / (float)IMU_ODR_HZ;
	rw["file"] = capName;
	rw["bytes"] = bclog.getRawBytes(BCLogger::RAW_CAPTURE);
	rw["snippets"] = w.snippets;
	rw["snipDropped"] = w.snippetsDropped;
	rw["snipFile"] = snipName;
	rw["snipBytes"] = bclog.getRawBytes(BCLogger::RAW_SNIPPETS);
	rw["dropped"] = bclog.getRawDropped();
	JsonObject pj = doc["pitch"].to<JsonObject>();
	pj["efValid"] = r.efValid;
	for (uint8_t i = 0; i < 3; i++) pj["ef"][i] = r.ef[i];
	pj["slope"] = r.slope;
	pj["biasDeg"] = r.biasDeg;
	pj["biasDist"] = r.biasDistM;
	pj["conv"] = r.biasConverged;
	pj["valid"] = r.gradValid;
	pj["grad"] = r.gradImu;
	pj["raw"] = r.gradRaw;
	pj["baro"] = r.gradBaro;
	pj["frozen"] = r.frozen;
	pj["updates"] = r.pitchUpdates;
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
	          "<tr><td>I2C read errors / invalid frames dropped</td><td class=\"n\" id=\"i2cErr\"></td></tr>\n"
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
	          "onclick=\"req('/debug/imu/cal','Calibration started - keep still');return false;\">Calibrate (keep bike still)</a></div>\n"
	          "<h3>Road quality</h3>\n"
	          "<p class=\"eyebrow\">Band-pass 2..80 Hz, vertical along gravity. R = RMS<sub>v</sub> / (baseline &middot; (v/20 km/h)<sup>0.8</sup>), "
	          "class 1 (smooth) .. 5 (very rough), 0 = not rated (too slow or no speed). Settings: serial CLI <code>rq</code>.</p>\n"
	          "<table><tbody>\n"
	          "<tr><td>Settings</td><td id=\"rqSet\"></td></tr>\n"
	          "<tr><td>Speed</td><td class=\"n\" id=\"rqSpeed\"></td></tr>\n"
	          "<tr><td>RMS<sub>v</sub> live (1 s, mg)</td><td class=\"n\" id=\"rqRms1\"></td></tr>\n"
	          "<tr><td>Last interval: class / R</td><td class=\"n\" id=\"rqCls\"></td></tr>\n"
	          "<tr><td>RMS vertical / horizontal (mg)</td><td class=\"n\" id=\"rqRms\"></td></tr>\n"
	          "<tr><td>Peak vertical max / min, total (g)</td><td class=\"n\" id=\"rqPeak\"></td></tr>\n"
	          "<tr><td>VDV (m/s<sup>1.75</sup>)</td><td class=\"n\" id=\"rqVdv\"></td></tr>\n"
	          "<tr><td>Crossings &gt;1 g / &gt;2 g</td><td class=\"n\" id=\"rqCnt\"></td></tr>\n"
	          "<tr><td>Flags</td><td id=\"rqFlags\"></td></tr>\n"
	          "<tr><td>Log records written / dropped</td><td class=\"n\" id=\"rqRec\"></td></tr>\n"
	          "</tbody></table>\n"
	          "<h3>Shocks</h3>\n<table><tbody>\n"
	          "<tr><td>Logged / suppressed (rate limit)</td><td class=\"n\" id=\"shCnt\"></td></tr>\n"
	          "<tr><td>Last shock</td><td id=\"shLast\"></td></tr>\n"
	          "<tr><td>Peak total / vertical max / min (g)</td><td class=\"n\" id=\"shPeak\"></td></tr>\n"
	          "<tr><td>Second peak (rear wheel)</td><td class=\"n\" id=\"shSecond\"></td></tr>\n"
	          "<tr><td>Duration / threshold / RMS before</td><td class=\"n\" id=\"shDur\"></td></tr>\n"
	          "<tr><td>Flags</td><td id=\"shFlags\"></td></tr>\n"
	          "</tbody></table>\n"
	          "<h3>Reference ride</h3>\n"
	          "<p class=\"eyebrow\">Ride about a minute on smooth asphalt, steadily, at 12 km/h or more. "
	          "The median of the speed-normalised RMS becomes the baseline (= class 1 road).</p>\n"
	          "<table><tbody>\n"
	          "<tr><td>State</td><td id=\"refState\"></td></tr>\n"
	          "<tr><td>Baseline (mg at 20 km/h)</td><td class=\"n\" id=\"refBase\"></td></tr>\n"
	          "</tbody></table>\n"
	          "<div class=\"row\" style=\"margin-top:8px;\"><a class=\"btn\" href=\"#\" "
	          "onclick=\"req('/debug/imu/ref?start=1','Reference ride started');return false;\">Start reference ride</a> "
	          "<a class=\"btn btn-ghost\" href=\"#\" onclick=\"req('/debug/imu/ref?start=0','Reference ride cancelled');return false;\">Cancel</a></div>\n"
	          "<h3>Gradient (accelerometer)</h3>\n"
	          "<p class=\"eyebrow\">Along the forward axis the sensor reads dv/dt + g&middot;sin(&theta;). The forward axis is learned "
	          "from accelerating and braking, the mounting offset against the barometer over a few hundred metres. "
	          "Shown on the display only after <code>rq gradsrc imu</code> and once the offset has converged.</p>\n"
	          "<table><tbody>\n"
	          "<tr><td>Gradient accelerometer / raw / barometer (%)</td><td class=\"n\" id=\"pGrad\"></td></tr>\n"
	          "<tr><td>Forward axis (sensor frame)</td><td class=\"n\" id=\"pEf\"></td></tr>\n"
	          "<tr><td>Mounting offset</td><td class=\"n\" id=\"pBias\"></td></tr>\n"
	          "<tr><td>Updates</td><td class=\"n\" id=\"pUpd\"></td></tr>\n"
	          "</tbody></table>\n"
	          "<div class=\"row\" style=\"margin-top:8px;\"><a class=\"btn btn-ghost\" href=\"#\" "
	          "onclick=\"if(confirm('Forget the learned forward axis and mounting offset?'))req('/debug/imu/pitchreset','Gradient learning reset');return false;\">Reset gradient learning</a></div>\n"
	          "<h3>Raw data</h3>\n"
	          "<p class=\"eyebrow\">400 Hz raw frames for tuning, next to the data log of this session: a capture on demand (R_*.bin, ~150 KB per minute) and, always on, 0.75 s around every logged shock (S_*.bin, ~1.8 KB each). Read with <code>bikelog raw</code>.</p>\n"
	          "<table><tbody>\n"
	          "<tr><td>Capture</td><td id=\"rawCap\"></td></tr>\n"
	          "<tr><td>Shock snippets</td><td id=\"rawSnip\"></td></tr>\n"
	          "</tbody></table>\n"
	          "<div class=\"row\" style=\"margin-top:8px;\"><a class=\"btn\" href=\"#\" "
	          "onclick=\"req('/debug/imu/raw?s=60','Capture started: 60 s');return false;\">Capture 60 s</a> "
	          "<a class=\"btn\" href=\"#\" onclick=\"req('/debug/imu/raw?s=300','Capture started: 5 min');return false;\">Capture 5 min</a> "
	          "<a class=\"btn btn-ghost\" href=\"#\" onclick=\"req('/debug/imu/raw?s=0','Capture stopped');return false;\">Stop</a></div>\n");
	WebPage::end(html,
		"const $=i=>document.getElementById(i);\n"
		"const f=(v,d)=>Number(v).toFixed(d);\n"
		"const v3=(a,d,m)=>a.map(x=>f(x*(m||1),d)).join(' / ');\n"
		"const n=(v,d)=>v===null||v===undefined?'-':Number(v).toFixed(d);\n"
		"const RQF=['too slow','data gap','clipped','uncalibrated','reference ride','GPS speed','no speed','GPS'];\n"
		"const SHF=['clipped','wheelbase match','standstill','GPS','after suppressed','no speed'];\n"
		"const fl=(v,names)=>names.filter((x,i)=>v&(1<<i)).join(', ')||'-';\n"
		"async function upd(){try{const r=await fetch('/debug/imu.json');if(!r.ok)return;const j=await r.json();\n"
		" const st=$('status');\n"
		" if(!j.running){st.textContent='BMI160 not running (device id 0x'+j.devId.toString(16)+')';st.className='badge badge-error';}\n"
		" else{st.textContent='running';st.className='badge badge-info';}\n"
		" $('sps').textContent=f(j.sps,1);$('fifo').textContent=j.fifo+' / '+j.fifoMax;\n"
		" $('ovf').textContent=j.ovf;$('total').textContent=j.total;$('i2cErr').textContent=j.i2cErr+' / '+j.invalid;\n"
		" for(let i=0;i<3;i++){$('last'+i).textContent=f(j.last[i],3);$('mean'+i).textContent=f(j.mean[i],4);\n"
		"  $('sigma'+i).textContent=f(j.sigma[i]*1000,1);$('min'+i).textContent=f(j.min[i],3);$('max'+i).textContent=f(j.max[i],3);}\n"
		" $('mag').textContent=f(j.mag,4);$('magMax').textContent=f(j.magMax,3);\n"
		" const c=j.cal;\n"
		" $('calState').textContent=c.running?('running '+Math.round(100*c.progress/c.samples)+' %'):c.state;\n"
		" $('calAttempt').textContent=v3(c.attemptSigma,1,1000)+'  (limit '+f(c.maxSigma*1000,0)+')';\n"
		" $('calTime').textContent=c.valid?(c.time?new Date(c.time*1000).toLocaleString():'yes (no clock)'):'no';\n"
		" $('calG0').textContent=c.valid?v3(c.g0,4):'-';$('calScale').textContent=c.valid?f(c.scale,4):'-';\n"
		" $('calSigma').textContent=c.valid?v3(c.sigma,1,1000):'-';$('calGyro').textContent=c.valid?c.gyro.join(' / '):'-';\n"
		" const q=j.rq,p=j.pitch;if(!q)return;\n"
		" $('rqSet').textContent=q.interval+' s interval, shock '+f(q.shockG,1)+' g, wheelbase '+f(q.wheelbase,2)+' m, gradient shown: '+q.gradSrc;\n"
		" $('rqSpeed').textContent=n(q.speed,1)+' km/h ('+q.speedSrc+')'+(q.still?', still':'');\n"
		" $('rqRms1').textContent=f(q.rms1s*1000,0);\n"
		" const l=q.last;\n"
		" if(l){$('rqCls').textContent=(l.cls||'not rated')+' / '+n(l.R,2);$('rqRms').textContent=f(l.rmsV*1000,0)+' / '+f(l.rmsH*1000,0);\n"
		"  $('rqPeak').textContent=f(l.pMax,2)+' / '+f(l.pMin,2)+', '+f(l.pT,2);$('rqVdv').textContent=f(l.vdv,3);\n"
		"  $('rqCnt').textContent=l.c1+' / '+l.c2;$('rqFlags').textContent=fl(l.flags,RQF);}\n"
		" $('rqRec').textContent=q.written+' / '+q.dropped;\n"
		" $('shCnt').textContent=q.shocks+' / '+q.suppressed;\n"
		" const k=q.shock;\n"
		" if(k){$('shLast').textContent='#'+k.seq+', severity '+k.sev+', '+(k.time?new Date(k.time*1000).toLocaleTimeString():'?')+', '+n(k.speed,1)+' km/h';\n"
		"  $('shPeak').textContent=f(k.peak,2)+' / '+f(k.vMax,2)+' / '+f(k.vMin,2);\n"
		"  $('shSecond').textContent=k.second>0?(f(k.second,2)+' g after '+f(k.delay,0)+' ms'):'none';\n"
		"  $('shDur').textContent=f(k.dur,1)+' ms / '+f(k.thr,2)+' g / '+f(k.pre*1000,0)+' mg';$('shFlags').textContent=fl(k.flags,SHF);}\n"
		" const rf=q.ref;\n"
		" $('refState').textContent=rf.running?('running: '+rf.progress+' of '+rf.target+' s at >= '+f(rf.minKmh,0)+' km/h ('+rf.elapsed+' s elapsed)'):rf.state;\n"
		" $('refBase').textContent=f(rf.baseline*1000,0)+(rf.cal?(' (reference ride'+(rf.time?', '+new Date(rf.time*1000).toLocaleDateString():'')+')'):' (default, no reference ride yet)');\n"
		" $('pGrad').textContent=(p.valid?n(p.grad,1):'-')+' / '+n(p.raw,1)+' / '+n(p.baro,1)+(p.frozen?' (held)':'');\n"
		" $('pEf').textContent=p.efValid?(v3(p.ef,3)+' (consistency '+f(p.slope,2)+')'):'not learned yet: accelerate and brake a few times';\n"
		" $('pBias').textContent=f(p.biasDeg,2)+' deg, learned over '+f(p.biasDist,0)+' m'+(p.conv?'':' (not converged)');\n"
		" $('pUpd').textContent=p.updates;\n"
		" const w=j.raw;\n"
		" if(w){$('rawCap').textContent=(w.capturing?('running '+f(w.seconds,0)+' / '+f(w.target,0)+' s'):'idle')+(w.file?(', '+w.file+', '+f(w.bytes/1024,1)+' KB'):'');\n"
		"  $('rawSnip').textContent=w.snippets+(w.snipFile?(' in '+w.snipFile+', '+f(w.snipBytes/1024,1)+' KB'):'')+(w.snipDropped?(', '+w.snipDropped+' dropped'):'')+(w.dropped?('; raw bytes lost: '+w.dropped):'');}\n"
		"}catch(x){}}\n"
		"upd();setInterval(upd,1000);\n");
}
#endif

