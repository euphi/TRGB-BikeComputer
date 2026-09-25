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

void I2CSensors::readBMI160() {
	int gxRaw, gyRaw, gzRaw;         // raw gyro values
	int axRaw, ayRaw, azRaw;         // raw gyro values
	float gx, gy, gz;
	float ax, ay, az;

	// read raw gyro measurements from device
	//BMI160.readGyro(gxRaw, gyRaw, gzRaw);
	BMI160.readMotionSensor(axRaw, ayRaw, azRaw, gxRaw, gyRaw, gzRaw);


	// convert the raw gyro data to degrees/second
	gx = convertRawGyro(gxRaw);
	gy = convertRawGyro(gyRaw);
	gz = convertRawGyro(gzRaw);

	ax = axRaw / 8192.0;
	ay = ayRaw / 8192.0;
	az = azRaw / 8192.0;

	// display tab-separated gyro x/y/z values
//	Serial.print("g:\t");
//	Serial.print(gx);
//	Serial.print("\t");
//	Serial.print(gy);
//	Serial.print("\t");
//	Serial.print(gz);
//	Serial.println();
//
//	Serial.print("a:\t");
//	Serial.print(ax);
//	Serial.print("\t");
//	Serial.print(ay);
//	Serial.print("\t");
//	Serial.print(az);
//	Serial.println();


}

void I2CSensors::initBMI160() {
    BMI160.begin(BMI160GenClass::I2C_MODE, 0x68, -1);
    Serial.println("Now getting device ID");
    uint8_t dev_id = BMI160.getDeviceID();
    Serial.print("DEVICE ID: ");
    Serial.println(dev_id, HEX);

    // Calibrate - must be standstill! //FIXME: Its better to trigger this manually - use menu or webserver
    BMI160.autoCalibrateGyroOffset();
    BMI160.setGyroOffsetEnabled(true);

}

#endif

