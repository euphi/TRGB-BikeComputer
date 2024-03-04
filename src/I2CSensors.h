/*
 * I2CSensors.h
 *
 *  Created on: 03.01.2024
 *      Author: ian
 */

#pragma once

#include "SparkFunBME280.h"

#include <SPI.h>        // needed so that pio can find the SPI include in BMI160 library
#include <BMI160Gen.h>
#include <Ticker.h>
#include <Preferences.h>

class I2CSensors {
public:
	I2CSensors();

	void setup() {
		initBME280();
		initBMI160();
	}

	uint16_t getHTMLPage(String& htmlresponse);
	uint16_t procHTMLHeight(String& htmlresponse, const float actHeight);

	//uint16_t procHTMLCmd(String& htmlresponse, const String& cmd, const String& arg);
	void readBME280();
	void readBMI160();

	float getHeight() const {return height;}
	float getHumid() const {return humid;}
	float getPress() const {return press;}
	float getTemp() const {return temp;}

private:
	BME280 bme280;
	void initBME280();
	void initBMI160();

	float press = NAN, humid = NAN, temp = NAN, height = NAN;
	float refPres = 101300.0;

	// Generated by ChatGPT:
	// Constants
	const double G = 9.81;        // Acceleration due to gravity in m/s^2
	const double M = 0.029;       // Molar mass of dry air in kg/mol
	const double R = 8.314;       // Specific gas constant of dry air in J/(mol·K)
	const double T0 = 288.15;     // Reference temperature at sea level in Kelvin
	const double P0_REF = 1013.25; // Reference air pressure at sea level in hPa

	// Function to calculate the reference air pressure at sea level
	double calculateReferencePressure(double h, double P) {
	    return P * exp((G * M * h) / (R * T0));
	}

	float convertRawGyro(int gRaw) {
	  // since we are using 250 degrees/seconds range
	  // -250 maps to a raw value of -32768
	  // +250 maps to a raw value of 32767
	  return (gRaw * 250.0) / 32768.0;
	}

	Ticker bme280Cycle;
	Preferences sensorPreferences;
};
