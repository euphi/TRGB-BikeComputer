/*
 * WifiWebserver.cpp
 *
 *  Created on: 26.02.2023
 *      Author: ian
 */

#include <WifiWebserver.h>
#include <Singletons.h>
#include <SD_MMC.h>
#include <LittleFS.h>
#include <FS.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <version.h>
#include <nvs.h>
#include <ESPmDNS.h>

//TODO: Read this from preferences
const char* ntpServer = "pool.ntp.org";
//const char* ssid = "IA216oT";
//const char* password = "SwieSecurity";

static const BCLogger::LogTag TAG = BCLogger::TAG_WIFI;

WifiWebserver::WifiWebserver():
	server(80)
{

}

void WifiWebserver::setup() {
	LittleFS.begin();		// WifiWebserver is also responsible for enabling LittleFS, because it is only used for Website storage (Logging is on SDCARD, which is maintained in BClogger).

	// Load settings from NVS
	WifiSettings.begin("WifiSettings", true);
	for (uint_fast8_t i = 0; i < WifiAPCount; i++) {
		String key = "SSID_" + i;
		StrSSID[i] = WifiSettings.getString(key.c_str(), "");
		key = "PW_" + i;
		StrPW[i] = WifiSettings.getString(key.c_str(), "");
		if (i == 0 && StrSSID[0] == "") {		//TODO: For testing only - remove
			StrSSID[0] = "IA216oT";
			StrPW[0] = "SwieSecurity";
		}
//		if (!StrSSID[i].equals("")) {
//			bclog.logf(BCLogger::Log_Info, TAG, "Adding SSID %s to WiFiMult", StrSSID[i].c_str());
//			wifiMulti.addAP(StrSSID[i].c_str(), StrPW[i].c_str());
//		}
	}
	disableAPMode = WifiSettings.getBool("disApMode", false);
	WifiSettings.end();

	// Enable WiFi
	enableWifi();
	bclog.log(BCLogger::Log_Info, TAG, "Connecting to WiFi and set timeserver");
	configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", ntpServer);
	startScan();
	WiFi.begin(StrSSID[0].c_str(), StrPW[0].c_str());
	wifiCheckTicker.attach_ms(500, +[](WifiWebserver *thisInstance) {thisInstance->checkLoop();}, this);
//	wifiMulti.addAP(ssid, password);
//	wifiMulti.addAP("IA216", "xxxxx");
}

void WifiWebserver::disableWifi() {
	ui.updateIP(String("WiFi disabled"));
	WiFi.setSleep(true);
	WiFi.mode(WIFI_MODE_NULL);
	wifiEnabled = false;
}

void WifiWebserver::enableWifi() {
	ui.updateIP(String("Enabling WiFi .."));
	WiFi.setSleep(true);
	WiFi.mode(WIFI_MODE_STA);
	WiFi.enableIpV6();
	wifiEnabled = true;
	wifiWasConnected = false;	// reset wifiWasConnected to enable check loop again
	lostConnTimeStamp = millis();
}

void WifiWebserver::enableAPMode(bool enable) {
	lostConnTimeStamp = millis();
	if (enable) {
		APModeActive = true;
		//TODO: Check if necessary (may be better to keep old value to restore it automatically when AP mode is disabled again)
		wifiEnabled = true;
		WiFi.mode(WIFI_AP);
		WiFi.softAP("TRGB-BC", "123456");
		String ipStr = WiFi.softAPIP().toString();
		ui.updateIP(ipStr);
		bclog.logf(BCLogger::Log_Debug, TAG, "Enabled AP mode with IPv4: %s .", ipStr.c_str());
	} else {
		APModeActive = false;
		ui.updateIP("Disabling AP...");
		enableWifi();
		WiFi.begin(StrSSID[0].c_str(), StrPW[0].c_str());
	}
}

void WifiWebserver::startScan() {
	bclog.log(BCLogger::Log_Debug, TAG, "Start scanning...");
	if (!wifiEnabled) enableWifi();
	WiFi.scanNetworks(true);
	scanActive = true;
}

void WifiWebserver::checkLoop() {
	if (scanActive) {
		bclog.log(BCLogger::Log_Debug, TAG, "Wifi check loop - scan active");
		int16_t result = WiFi.scanComplete();
		if (result == WIFI_SCAN_RUNNING) return;
		if (result > 0) {
			String allSSID = "";
			for (uint16_t i = 0 ; i < result ; i ++) {
				allSSID += WiFi.SSID(i);
				allSSID += "\n";
			}
			bclog.logf(BCLogger::Log_Debug, TAG, "WiFi Scan: %d SSID found: %s", result, allSSID.c_str());
			ui.updateSSIDList(allSSID);
			scanActive = false;
		} else if (result == 0) {
			bclog.log(BCLogger::Log_Info, TAG, "WiFi Scan: No SSID found");
			scanActive = false;
		} else if (result == WIFI_SCAN_FAILED) {
			bclog.log(BCLogger::Log_Warn, TAG, "WiFi Scan: failed");
		} else  {
			bclog.logf(BCLogger::Log_Warn, TAG, "WiFi Scan: Invalid result 0x%02x", result);
		}
		scanActive = false;

	}
	if (APModeActive) {
		bclog.log(BCLogger::Log_Debug, TAG, "Wifi check loop - AP mode active");
		uint8_t apStaCount = WiFi.softAPgetStationNum();
		ui.updateWiFiState(wifiEnabled, APModeActive, disableAPMode, apStaCount);
		return;
	}
    // Handle WiFi - the current logic is:
	// - immediately start a connection to a known Access point (from preferences)
	// - if no connection is established within 10seconds, WiFi is disabled completely
	// - if established connection  is lost, WiFi is disabled completely
	// --> this is done to save power consumption of WiFi radio.
	if (!wifiWasConnected) {
		bclog.log(BCLogger::Log_Debug, TAG, "Wifi check loop - try to connect");
		if (WiFi.status() == WL_CONNECTED) {
//		if (WiFi.status() != WL_CONNECTED && wifiMulti.run(10000) == WL_CONNECTED) {
			wifiWasConnected = true;
			bclog.logf(BCLogger::Log_Info, TAG, "Wifi connected. IPv4: %s", WiFi.localIP().toString());
			//bclog.logf(BCLogger::Log_Info, TAG, "Wifi connected. IPv4: %s IPv6: %s", WiFi.localIP().toString(), WiFi.localIPv6().toString());
			// Setup Web Server
			setupWebserver();
			delay(1000);	//TODO: Wifi reconnection after reset can be very fast, so IP can be available BEFORE UI has been initialized. So wait a second...
			ui.updateIP(WiFi.localIP().toString());
			MDNS.begin("TRGB-BC");
		    MDNS.addService("http", "tcp", 80);
		} else if (millis() - lostConnTimeStamp > 100000) { //disable Wifi if no connection was established during the first 10 seconds
			wifiWasConnected = true;
			bclog.log(BCLogger::Log_Warn, TAG, "Not connected to Wifi - disabling it");
			disableWifi();
		} else {
			bclog.log(BCLogger::Log_Debug, TAG, "Wifi check loop - can't connect (yet)");
		}
	} else if (WiFi.status() == WL_CONNECTION_LOST) {
		lostConnTimeStamp = millis();
		ui.updateIP(String("connection lost"));
		bclog.log(BCLogger::Log_Warn, TAG, "Wifi connection lost - disabling it to save power");
		disableWifi();
	}
	ElegantOTA.loop();		// check loop for ElegantOTA (seems to only check for reboot-flag for now)
}

void WifiWebserver::scanResult() {

}

void WifiWebserver::setupWebserver() {
	// Enable file deletion
    // using DELETE method on the same URI as for "serveStatic" would be more elegant, but is not possible to create links that result in making the browser use DELETE method. So use special "del" uri
	server.on("^\\/del\\/([A-Za-z0-9_\\.\\/]+)$", HTTP_GET, [](AsyncWebServerRequest *request) {
		// using DELETE method on the same URI as for "serveStatic" would be more elegant, but is not possible to create links that result in making the browser use DELETE method. So use special "del" uri
		bclog.logf(BCLogger::Log_Info, TAG, "💻 Request on /del/: %s\n\tPath-Arg: %s", request->url().c_str(), request->pathArg(0).c_str());
		String dUri = String("/BIKECOMP/") + request->pathArg(0);
		uint16_t http_code = 500;
		String html_resp("<html><body>");
		if (bclog.deleteFile(dUri)) {
			http_code = 200;
			html_resp += "OK - file deleted.";
		} else {
			http_code = 403;
			html_resp += "Forbidden - file can't be deleted (probably because it does not exist).";
		}
		html_resp += "</body></html>";
		request->send(http_code, "text/html", html_resp.c_str());
	});
	server.on("^\\/replay\\/([A-Za-z0-9_\\.\\/]+)$", HTTP_GET, [](AsyncWebServerRequest *request) {
		bclog.logf(BCLogger::Log_Info, TAG, "💻 Request on /replay/: %s\n\tPath-Arg: %s", request->url().c_str(), request->pathArg(0).c_str());
		String dUri = String("/BIKECOMP/") + request->pathArg(0);
		uint16_t http_code = 500;
		String html_resp("<html><body>");
		if (bclog.replayFile(dUri)) {
			http_code = 200;
			html_resp += "OK - file replay started";
		} else {
			http_code = 403;
			html_resp += "Forbidden - file can't be openend for replay (probably because it does not exist).";
		}
		html_resp += "</body></html>";
		request->send(http_code, "text/html", html_resp.c_str());
	});

	// -- generate Logfile Index
	server.on("/logfiles/", HTTP_GET, [this](AsyncWebServerRequest *request) {
		htmlresponse.clear();
		bclog.getAllFileLinks(htmlresponse);
		Serial.println(htmlresponse);
		request->send(200, "text/html", htmlresponse.c_str());
	});
	// -- offer cleanup
	server.on("/cleanup", HTTP_GET, [](AsyncWebServerRequest *request) {
		bclog.autoCleanUp("/BIKECOMP");
		request->send(200, "text/plain", "Cleanup done.");
	});

	server.on("/coredump_now", HTTP_GET, [](AsyncWebServerRequest *request) {
		request->send(200, "text/plain", "Crash for coredump");
		delay(1000);
		abort();
	});

	// Serve the system resources page
	server.on("/system", HTTP_GET, [](AsyncWebServerRequest *request) {
	    String html = "<html><head><link rel='stylesheet' type='text/css' href='/stylesheet.css'></head>\n<body>\n<div class='container'>\n<h1>ESP32 System Resources</h1>";

		// Get free heap memory
		html += "<p>Free Heap : " + String(ESP.getFreeHeap() / 1024 ) + " kilobytes</p>";
		html += "<p>Free PSRAM: " + String(ESP.getFreePsram() / 1024 ) + " kilobytes</p>";

		// Calculate free space on LittleFS
		size_t totalBytes = LittleFS.totalBytes();
		size_t usedBytes = LittleFS.usedBytes();
		html += "<p>Used space on LitteFS: " + String(usedBytes / (1024) ) + " kB of " + String(totalBytes / (1024)) + " kB.</p>";


		// Check if an SD card is present
		sdcard_type_t ctype = SD_MMC.cardType();
		if (ctype != CARD_UNKNOWN && ctype != CARD_NONE) {
			// Get free space on the SD card
			uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
			uint64_t totalSpace = SD_MMC.usedBytes() / (1024 * 1024);
			html += "<p>Used space on SD card: " + String(totalSpace) + " MB of " + String(cardSize) + " MB.</p>";
		} else {
			html += "<p>No SD card detected</p>";
		}
		html += "<p>Version: " VERSION "</p>";
		html += "</div></body></html>";
		request->send(200, "text/html", html);
	});

	server.on("/dev/", HTTP_GET,  [this](AsyncWebServerRequest *request) {
		htmlresponse.clear();
		bleDevs.getHTMLPage(htmlresponse);
		request->send(200, "text/html", htmlresponse.c_str());
	});

	server.on("^\\/dev\\/([A-Za-z0-9]+)$", HTTP_GET, [this] (AsyncWebServerRequest *request) {
		htmlresponse.clear();
		const AsyncWebParameter* para = request->getParam(0);
		bclog.logf(BCLogger::Log_Info, TAG, "💻 Request on /dev/: %s\n\tPath-Arg: %s - %s", request->url().c_str(), request->pathArg(0).c_str(), para ? para->value().c_str() : "n/a");
		if (!para) {
			request->send(400, "text/plain", "Missing parameter");
		} else {
			int16_t code = bleDevs.procHTMLCmd(htmlresponse, request->pathArg(0), para->value());
			request->send(code, "text/plain", htmlresponse.c_str());
		}
	});

#if TRGBBC_SENSORS_I2C
	server.on("/sensor/", HTTP_GET,  [this](AsyncWebServerRequest *request) {
		htmlresponse.clear();
		sensors.getHTMLPage(htmlresponse);
		request->send(200, "text/html", htmlresponse.c_str());
	});

	server.on("/sensor/submit", HTTP_POST, [this](AsyncWebServerRequest *request) {
		String height = request->arg("height");
		double heightValue = height.toDouble();
		htmlresponse.clear();
		int16_t code = sensors.procHTMLHeight(htmlresponse, heightValue);
		request->send(200, "text/html", htmlresponse);
	});
#endif		//TODO: Add height (pressure) adjustment for FL


	server.on("/log/set", HTTP_GET, [](AsyncWebServerRequest *request) {
		String tag = request->getParam("tag")->value();
		String level = request->getParam("level")->value();
		String output = request->getParam("output")->value();

		bool toSerialWeb = output.equalsIgnoreCase("Serial");

		// Validate tag and level values
		if (tag.length() > 0 && level.length() > 0) {
			// Set the log level for the specified tag
			// You can implement your own logic to set the log level based on the parameters
			// For example, you can use conditional statements or a mapping structure.

			// Replace the following line with your actual logic
			// setLogLevel(tag, level);

			// Respond with a success message
			uint16_t t = BCLogger::TAG_RAW_NMEA;
			for (; t < BCLogger::LogTagMax; t++) {
				if (tag.equalsIgnoreCase(BCLogger::TAG_STRING[t])) break;
			}
			if (t == BCLogger::LogTagMax) {
				bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_OP, "Invalid log tag %s", tag.c_str());
				request->send(400, "text/plain", "Invalid tag.");
				return;
			}
			uint16_t l = BCLogger::Log_Debug;
			for (; l < BCLogger::LogTypeMax; l++) {
				if (level.equalsIgnoreCase(BCLogger::LEVEL_STRING[l])) break;
			}
			if (l == BCLogger::LogTypeMax) {
				bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_OP, "Invalid log level %s", level.c_str());
				request->send(400, "text/plain", "Invalid level.");
				return;
			}
			bclog.setLogLevel(static_cast<BCLogger::LogType>(l), static_cast<BCLogger::LogTag>(t), !toSerialWeb, toSerialWeb);
			request->send(200, "text/plain", "Log level set successfully.");
		} else {
			// Respond with an error message for invalid parameters
			request->send(400, "text/plain", "Invalid parameters.");
		}
	});

	server.on("/log/get", HTTP_GET, [](AsyncWebServerRequest *request) {
		// Create a JSON document
		//DynamicJsonDocument doc(1024);  // deprecated
		JsonDocument doc;

		for (uint16_t t = BCLogger::TAG_RAW_NMEA; t < BCLogger::LogTagMax; t++) {
			BCLogger::LogType ll = bclog.getLogLevel(static_cast<BCLogger::LogTag>(t));
			doc[BCLogger::TAG_STRING[t]] = BCLogger::LEVEL_STRING[ll];
		}
		// Serialize JSON document to a string
		String jsonString;
		serializeJson(doc, jsonString);

		// Respond with the JSON string
		request->send(200, "application/json", jsonString);
	});
#ifdef DEBUG_APP
	setupNvsDebug();
#endif

	server.on("/wifi/connect", HTTP_GET, [this](AsyncWebServerRequest *request) {
		if ((request->hasParam("ssid", true) || request->hasParam("manualSSID", true)) && request->hasParam("password", true)) {
			if (request->hasParam("ssid", true)) {
				StrSSID[0] = request->getParam("ssid", true)->value();
			} else {
				StrSSID[0] = request->getParam("manualSSID", true)->value();
			}
			StrPW[0] = request->getParam("password", true)->value();
			WifiSettings.begin("WifiSettings", false);
			WifiSettings.putString("SSID_0", StrSSID[0]);
			WifiSettings.putString("PW_0", StrPW[0]);
			//WifiSettings.putBool("disApMode", disableAPMode);
			WifiSettings.end();
			wifiWasConnected = false;
			lostConnTimeStamp = millis();
			request->send(200);
		} else {
			request->send(400);
		}
	});

	server.on("/wifi/ssids", HTTP_GET, [](AsyncWebServerRequest *request) {
		int numNetworks = WiFi.scanNetworks();
		String json = "[";
		if (numNetworks > 0) {
			for (int i = 0; i < numNetworks; i++) {
				json += "\"" + WiFi.SSID(i) + "\"";
				if (i < numNetworks - 1) {
					json += ",";
				}
			}
		}
		json += "]";
		request->send(200, "application/json", json);
	});
	server.on("/wifi/enableAP", HTTP_GET, [this](AsyncWebServerRequest *request) {
		enableAPMode(true);
		request->send(200);
	});

	// -- Allow OTA via Web ("ElegantOTA" library)
	ElegantOTA.begin(&server); // Start ElegantOTA - it listens on "/update/"
	ElegantOTA.setAutoReboot(true);
	ElegantOTA.onStart([]() {
		bclog.log(BCLogger::Log_Info, TAG, "Start OTA Update");
		ui.otaStart();
	});
	ElegantOTA.onProgress([](size_t current, size_t total) {
		uint8_t perc = (current * 100) / total;
		bclog.logf(BCLogger::Log_Debug, TAG, "OTA Update: %d %% [%d byte from %d byte].", perc, current, total);
		ui.otaProgress(perc);
	});


	// -- download Binary Logfile
	server.serveStatic("/log/", SD_MMC, "/BIKECOMP/");
	server.serveStatic("/", LittleFS, "/site/").setCacheControl("max-age=31536000").setDefaultFile("index.html");

	// URI not found
	server.onNotFound([](AsyncWebServerRequest *request) {
		bclog.logf(BCLogger::Log_Info, TAG, "💻 Can't handle request on : %s\n", request->url().c_str());
		String responsetext = request->url() + " not found!\n";
		request->send(404, "text/plain", responsetext.c_str());
	});
	server.begin();
	bclog.logf(BCLogger::Log_Debug, TAG, "💻 HTTP server started at %s.\n", WiFi.localIP().toString().c_str());
}

#ifdef DEBUG_APP

typedef struct {
    nvs_type_t type;
    const char *str;
} type_str_pair_t;

static const type_str_pair_t type_str_pair[] = {
    { NVS_TYPE_I8, "i8" },
    { NVS_TYPE_U8, "u8" },
    { NVS_TYPE_U16, "u16" },
    { NVS_TYPE_I16, "i16" },
    { NVS_TYPE_U32, "u32" },
    { NVS_TYPE_I32, "i32" },
    { NVS_TYPE_U64, "u64" },
    { NVS_TYPE_I64, "i64" },
    { NVS_TYPE_STR, "str" },
    { NVS_TYPE_BLOB, "blob" },
    { NVS_TYPE_ANY, "any" },
};

static const size_t TYPE_STR_PAIR_SIZE = sizeof(type_str_pair) / sizeof(type_str_pair[0]);

static const char *type_to_str(nvs_type_t type)
{
    for (int i = 0; i < TYPE_STR_PAIR_SIZE; i++) {
        const type_str_pair_t *p = &type_str_pair[i];
        if (p->type == type) {
            return  p->str;
        }
    }
    return "Unknown";
}

void WifiWebserver::setupNvsDebug() {
	server.on("/debug/nvs", HTTP_GET, [](AsyncWebServerRequest *request) {
		String respString("NVS Iterator\n\n");
		nvs_iterator_t it = nvs_entry_find("nvs", NULL, NVS_TYPE_ANY);
		if (it == NULL) {
			bclog.log(BCLogger::Log_Warn, TAG, "Can't iterate over NVS");
			request->send(500, "text/plain", "Can't iterate over NVS");
			return;
		}
		do {
			nvs_entry_info_t info;
			nvs_entry_info(it, &info);
			it = nvs_entry_next(it);
			char buffer[400];
			snprintf(buffer, sizeof(buffer) - 1, "namespace '%s', key '%s', type '%s' \n", info.namespace_name, info.key, type_to_str(info.type));
			respString += buffer;
		} while (it != NULL);
		request->send(200, "text/plain", respString);
	});
}

#endif
