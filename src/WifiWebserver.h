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
	void enableWebserver() {startupComplete = true;}	// call once, after all routes are registered

private:
	void setupWebserver();
	void setupOta();					// own OTA, replaces the ElegantOTA dependency
	uint32_t otaRebootAt = 0;			// millis() deadline, 0 = no restart pending
	size_t   otaTotal = 0;				// Content-Length of the running upload, for progress
	uint8_t  otaLastPerc = 255;			// throttles ui.otaProgress() to actual changes
	Ticker wifiCheckTicker;
	bool wifiWasConnected = false;
	bool webserverStarted = false;
	// Routes are registered by several modules from main's setup() (stats, distance,
	// bclog), but the server is started from checkLoop() as soon as WiFi is up -- which
	// after a warm reboot is ~1s, long before setup() has got that far. Requests arriving
	// in that window 404 on routes that simply do not exist yet. enableWebserver() closes
	// that window; see the call at the end of main's setup().
	volatile bool startupComplete = false;	// routes are registered exactly once, see setupWebserver()
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

