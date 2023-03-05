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
#include <AsyncElegantOTA.h>



//FIXME: #include <UIFacade.h>

//TODO: Read this from preferences
const char* ntpServer = "pool.ntp.org";
const char* ssid = "IA216oT";
const char* password = "SwieSecurity";

static const BCLogger::LogTag TAG = BCLogger::TAG_WIFI;

WifiWebserver::WifiWebserver():
	server(80)
{

}

void WifiWebserver::setup() {
	  WiFi.mode(WIFI_MODE_STA);
	  WiFi.enableIpV6();
	  WiFi.begin(ssid, password);
	  bclog.log(BCLogger::Log_Info, TAG, "Connecting to WiFi and set timeserver");
	  configTime(3600, 3600, ntpServer);
	  wifiCheckTicker.attach_ms(500, +[](WifiWebserver* thisInstance) {thisInstance->checkLoop();}, this);
	  LittleFS.begin();
}

void WifiWebserver::checkLoop() {
    // Handle WiFi - the current logic is:
	// - immediately start a connection to a known Access point (from preferences)
	// - if no connection is established within 10seconds, WiFi is disabled completely
	// - if established connection  is lost, WiFi is disabled completely
	// --> this is done to save power consumption of WiFi radio.
	if (!wifiWasConnected) {
		if (WiFi.status() == WL_CONNECTED) {
			wifiWasConnected = true;
			bclog.logf(BCLogger::Log_Info, TAG, "Wifi connected. IPv4: %s IPv6: %s", WiFi.localIP().toString(), WiFi.localIPv6().toString());
			ui.updateIP(WiFi.localIP().toString());
			//ui.updateIP(WiFi.localIP().toString());
			// Setup Web Server
			setupWebserver();
		} else if (millis() > 100000) { //disable Wifi if no connection was established during the first 10 seconds
			wifiWasConnected = true;
			bclog.log(BCLogger::Log_Warn, TAG, "Not connected to Wifi - disabling it");
			//ui.updateIP(String("WiFi disabled"));
			WiFi.mode(WIFI_MODE_NULL);
			WiFi.setSleep(WIFI_PS_MAX_MODEM);
			WiFi.setSleep(true);
		}
	} else if (WiFi.status() == WL_CONNECTION_LOST) {
		//ui.updateIP(String("connection lost"));
		bclog.log(BCLogger::Log_Warn, TAG, "Wifi connection lost - disabling it to save power");
		WiFi.mode(WIFI_MODE_NULL);
		WiFi.setSleep(WIFI_PS_MAX_MODEM);
		WiFi.setSleep(true);
	}
}



//
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
			html_resp += "Forbidden - file can't be deleted (probably because it does not exist";
		}
		html_resp += "</body></html>";
		request->send(http_code, "text/html", html_resp.c_str());
	});

	// -- generate Logfile Index
	server.on("/logfiles/", HTTP_GET, [this](AsyncWebServerRequest *request) {
		htmlresponse.clear();
		bclog.getAllFileLinks(htmlresponse);
		Serial.println(htmlresponse);
		request->send(200, "text/html", htmlresponse.c_str()); // TODO: Check if the webserver handles the String. It's on stack, so there may be a use-after-free issue here.
	});
	// -- offer cleanup
	server.on("/cleanup", HTTP_GET, [](AsyncWebServerRequest *request) {
		bclog.autoCleanUp("/BIKECOMP");
		request->send(200, "text/plain", "Cleanup done.");
	});

	// -- download Binary Logfile
	server.serveStatic("/log/", SD_MMC, "/BIKECOMP/");
	server.serveStatic("/", LittleFS, "/site/").setDefaultFile("index.html");


//	// -- Allow OTA via Web ("ElegantOTA" library)
	AsyncElegantOTA.begin(&server); // Start ElegantOTA - it listens on "/update/"
	server.onNotFound([](AsyncWebServerRequest *request) {
		bclog.logf(BCLogger::Log_Info, TAG, "💻 Can't handle request on : %s\n", request->url().c_str());
		String responsetext = request->url() + " not found!\n";
		request->send(404, "text/plain", responsetext.c_str());
	});
	server.begin();
	bclog.logf(BCLogger::Log_Debug, TAG, "💻 HTTP server started at %s.\n", WiFi.localIP().toString().c_str());
}
