/*
 * I2CSensors.cpp
 *
 *  Created on: 03.01.2024
 *      Author: ian
 */

#include <I2CSensors.h>
#include <Singletons.h>
#include <LittleFS.h>

I2CSensors::I2CSensors() {

}

uint16_t I2CSensors::getHTMLPage(String &htmlresponse) {
	uint16_t rc = 200;
	htmlresponse += "<html><head><meta charset=\"UTF-8\"><title>Sensors</title><link rel=\"stylesheet\" href=\"/stylesheet.css\"></head><body>\n <div class=\"container\"><h1>Sensors</h1>\n";
	//BME 280
	htmlresponse += " <div class=\"sensor-box\"><h2>BME 280</h2>\n<p><ul>\n";
	char buffer[255];
	snprintf(buffer, 254,
			"      <li>Luftdruck: %.2f mbar</li>\n<li>Height: %.2f m NHN</li>\n<li>Reference-Luftdruck (0m): %.2f mbar</li>\n<li>Luftfeuchte: %.2f rel%%</li>\n<li>Temperatur: %.2f °C</li>",
			press,height, bme280.getReferencePressure()/100, humid, temp);
	htmlresponse += buffer;
	htmlresponse += "</ul>\n  <form action='/sensor/submit' method='post'>Calibrate to known actual height: <input type='text' name='height'><input type='submit' value='Submit'></form></p></div>\n";
	// Add other sensors here

	htmlresponse += "</div></body></html>";
	return rc;
}

uint16_t I2CSensors::procHTMLHeight(String& htmlresponse, const float actHeight) {
	float newRefPress = calculateReferencePressure(actHeight, press) * 100;
	bme280.setReferencePressure(newRefPress);
	htmlresponse += "<html><body>";
	char buffer[255];
	snprintf(buffer, 254, "Ok - new pressure %.2f mbar for height %.2f.<br /><button onclick=\"window.history.back()\">Go Back</button>", newRefPress, actHeight);
	htmlresponse += buffer;
	htmlresponse += "</body></html>";
	return 200;
}


//uint16_t I2CSensors::procHTMLCmd(String &htmlresponse, const String &cmd, const String &arg) {
//	if (cmd == "UpdRefFromHeight") {
//		float actHeight = 256.0;
//		float newRefPress = calculateReferencePressure(actHeight, press) * 100;
//		char buffer[255];
//		snprintf(buffer, 254, "Ok - new pressure %.2f mbar for height %.2f", newRefPress, actHeight);
//		bme280.setReferencePressure(newRefPress);
//		return 200;
//	}
//	return 501;
//}

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
	bme280.settings.filter = 3;

	//tempOverSample can be:
	//  0, skipped
	//  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
	bme280.settings.tempOverSample = 4;

	//pressOverSample can be:
	//  0, skipped
	//  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
	bme280.settings.pressOverSample = 5;

	//humidOverSample can be:
	//  0, skipped
	//  1 through 5, oversampling *1, *2, *4, *8, *16 respectively
	bme280.settings.humidOverSample = 4;

	bme280.begin();
	bme280.setReferencePressure(100184.0);		// TODO: Adapt

		//       Idea is to use the template processor, however the processor gets confused and cannot handle more than the first two variables.
	webserver.getServer().on("/sensor_t/", HTTP_GET, [this](AsyncWebServerRequest *request) {
		Serial.println("Request for /sensor_t/");
		request->send(LittleFS, "/site/sensor/index.html", String(), false, [](const String &var) -> String {
			//FIXME: We need to capture the outer this, but this crashes?
			if (var == "bmeTemperature") {
				return (String("22") /*String(temp)*/);
			} else if (var == "bmeHumidity") {
				return (String("53")/*String(humid)*/);
			} else if (var == "bmePressure") {
				return (String("1008.2") /*String(press)*/);
			} else {
				Serial.printf("WARNING: Unknown var %s\n",
						var.c_str());
			}
			return String("-/-");
		});
	});

	bme280Cycle.attach_ms(1000, +[](I2CSensors* thisInstance) { thisInstance->readBME280(); }, this);
}

void I2CSensors::readBME280() {
	//FIXME: Quick test
	press = bme280.readFloatPressure() / 100; // /100: Pa -> hPa == mbar
	humid = bme280.readFloatHumidity();
	temp  = bme280.readTempC();
	height = bme280.readFloatAltitudeMeters();
	Serial.printf("BME280: %.2f mbar, %.2f rel%%, %.2f °C, %.2f m NN\n", press, humid, temp, height);
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
	Serial.print("g:\t");
	Serial.print(gx);
	Serial.print("\t");
	Serial.print(gy);
	Serial.print("\t");
	Serial.print(gz);
	Serial.println();

	Serial.print("a:\t");
	Serial.print(ax);
	Serial.print("\t");
	Serial.print(ay);
	Serial.print("\t");
	Serial.print(az);
	Serial.println();


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
