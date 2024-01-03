/*
 * WifiWebserver.h
 *
 *  Created on: 26.02.2023
 *      Author: ian
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Ticker.h>
#include <ESPAsyncWebServer.h>


//#include <AsyncTCP.h>


class WifiWebserver {
public:
	WifiWebserver();
	void setup();
	void checkLoop();
	bool addHandler(const char* uri, ArRequestHandlerFunction onRequest);

	AsyncWebServer& getServer() {
		return server;
	}

private:
	void setupWebserver();
	Ticker wifiCheckTicker;
	bool wifiWasConnected = false;
	AsyncWebServer server;

	String htmlresponse; // Reserve buffer for HTML response

};

