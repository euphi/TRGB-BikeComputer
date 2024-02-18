/*
 * WifiWebserver.h
 *
 *  Created on: 26.02.2023
 *      Author: ian
 */

#pragma once

#include <Arduino.h>
#include <WiFi.h>
//#include <WiFiMulti.h>		//TODO: Debug why WifiMult does not connect (auth fail)
#include <Ticker.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <global_settings.h>

class WifiWebserver {
public:
	WifiWebserver();
	void setup();
	void checkLoop();
	void enableAPMode(bool enable);

	AsyncWebServer& getServer() {return server;}

private:
	void setupWebserver();
	Ticker wifiCheckTicker;
	bool wifiWasConnected = false;
	time_t lostConnTimeStamp = 0;
	AsyncWebServer server;

	String htmlresponse;		// Buffer for response

	Preferences WifiSettings;
	String StrSSID[WifiAPCount] = {""};
	String StrPW[WifiAPCount] = {""};
	bool disableAPMode = false;		// AP Mode not possible
	bool APModeActive = false;		// WiFi is in AP mode
	//WiFiMulti wifiMulti;

	void scanResult();

	bool wifiEnabled = true;
	bool scanActive = false;
	void disableWifi();
	void enableWifi();
	void startScan();

#ifdef DEBUG_APP
	void setupNvsDebug();
#endif

};

