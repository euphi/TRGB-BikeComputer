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
#include <Update.h>
//#include <PrettyOTA.h>
#include <ArduinoJson.h>
#include <version.h>
#include <nvs.h>
#include <ESPmDNS.h>
#include "WebPage.h"
#include <esp_heap_caps.h>
#include "WebInstrument.h"

//TODO: Read this from preferences
const char* ntpServer = "pool.ntp.org";
//const char* ssid = "IA216oT";
//const char* password = "SwieSecurity";

static const BCLogger::LogTag TAG = BCLogger::TAG_WIFI;

// --- path extraction for the prefix routes below ------------------------------------
// These replace three regex routes. ASYNCWEBSERVER_REGEX looks cheap but isn't:
// AsyncCallbackWebHandler::canHandle() constructs a std::regex on EVERY request for
// EVERY regex route (WebHandlers.cpp), so a plain GET /stylesheet.css used to build
// three NFAs first -- dozens of small allocations each, all from the internal heap,
// which is the pool that actually runs out here (measured: 21KB free idle, ~300 byte
// under load). Prefix matching costs a string compare.
//
// canHandle() matches a non-regex _uri when it equals the url or the url starts with
// _uri + "/", so registering "/del" catches "/del/<path>". request->url() is already
// URL-decoded and has the query string stripped (WebRequest.cpp::_parseReqHead), so
// validating it here is validating what we will actually use.

// Tail after "<prefix>/", or an empty String if the url is just the prefix itself.
static String pathTail(AsyncWebServerRequest *request, const char* prefixWithSlash) {
	const String& url = request->url();
	const size_t len = strlen(prefixWithSlash);
	if (!url.startsWith(prefixWithSlash)) return String();
	return url.substring(len);
}

// Character classes of the old regexes: [A-Za-z0-9_./] for file paths, [A-Za-z0-9] for
// device commands. PLUS a ".." rejection the regexes did not have -- "[A-Za-z0-9_./]+"
// matched "../../etc/passwd" just fine, so /del/ and /replay/ could reach outside
// /BIKECOMP/. Keep that rejection if these routes are ever touched again.
static bool isSafeRelPath(const String& p) {
	if (p.isEmpty()) return false;
	if (p.indexOf("..") >= 0) return false;
	if (p.startsWith("/")) return false;			// must stay relative to LOGDIR
	for (size_t i = 0; i < p.length(); i++) {
		const char c = p.charAt(i);
		if (!(isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' || c == '/')) return false;
	}
	return true;
}

static bool isSafeDevCmd(const String& p) {
	if (p.isEmpty()) return false;
	for (size_t i = 0; i < p.length(); i++) {
		if (!isalnum(static_cast<unsigned char>(p.charAt(i)))) return false;
	}
	return true;
}

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
		//if (i == 0 && StrSSID[0] == "") {		//TODO: For testing only - remove
		if (true) {
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
	WiFi.enableIPv6();
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
			// Wait for every module to have registered its routes -- see startupComplete.
			// Deliberately before wifiWasConnected is set, so this retries on the next tick.
			// NTP is unaffected: configTzTime() already ran in setup().
			if (!startupComplete) return;
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
	// A successful OTA cannot restart from the upload handler -- that runs on async_tcp
	// and the client still has to receive the response. The flag is set there and acted
	// on here, one tick later, by which time the reply has gone out.
	if (otaRebootAt && millis() >= otaRebootAt) {
		bclog.log(BCLogger::Log_Info, TAG, "OTA complete - restarting");
		ESP.restart();
	}
}

void WifiWebserver::scanResult() {

}

// ---------------------------------------------------------------------------
// OTA update
// ---------------------------------------------------------------------------
// Replaces the ElegantOTA dependency. Everything it did for this project is here:
// a GET page to pick a file, a POST that streams into the Update partition, and a
// restart once the client has its answer. In exchange the page follows the rest of the
// UI (ElegantOTA ships its own gzipped HTML, which cannot be restyled) and the progress
// shown is the browser's own upload progress rather than a server-side estimate.
void WifiWebserver::setupOta() {
	server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
		String html;
		WebPage::begin(html, "Firmware Update",
			"#bar{height:10px;border-radius:5px;background:var(--rr-panel-2,#282019);"
			"border:1px solid var(--rr-line,#3A362E);overflow:hidden;margin-top:14px;display:none}"
			"#bar i{display:block;height:100%;width:0;background:var(--rr-brass,#CBA36B);transition:width .2s}");
		html += F("<p class=\"eyebrow\">Pick a firmware binary (<code>firmware.bin</code>) or a "
		          "filesystem image (a name containing <code>littlefs</code> or <code>spiffs</code> "
		          "selects the filesystem partition). The device restarts when the upload verifies.</p>\n"
		          "<div class=\"row\" style=\"margin-top:14px;gap:10px;flex-wrap:wrap;\">"
		          "<input type=\"file\" id=\"f\" accept=\".bin\">"
		          "<a class=\"btn\" id=\"go\" onclick=\"upload()\">Upload</a>"
		          "</div>\n<div id=\"bar\"><i id=\"fill\"></i></div>\n"
		          "<p class=\"eyebrow\" id=\"state\" style=\"margin-top:10px;\">Version " VERSION "</p>\n");
		WebPage::end(html,
			"function upload(){const f=document.getElementById('f').files[0];"
			"if(!f){toast('Choose a file first',true);return;}\n"
			"const fd=new FormData();fd.append('update',f,f.name);\n"
			"const x=new XMLHttpRequest();document.getElementById('bar').style.display='block';\n"
			"document.getElementById('go').style.pointerEvents='none';\n"
			"x.upload.onprogress=e=>{if(!e.lengthComputable)return;"
			"const p=Math.round(e.loaded/e.total*100);"
			"document.getElementById('fill').style.width=p+'%';"
			"document.getElementById('state').textContent='Uploading '+p+'%';};\n"
			"x.onload=()=>{if(x.status===200){document.getElementById('state').textContent="
			"'Done - the device is restarting.';toast('Update complete, restarting');}"
			"else{document.getElementById('state').textContent=x.responseText||('Failed ('+x.status+')');"
			"toast(x.responseText||'Update failed',true);"
			"document.getElementById('go').style.pointerEvents='';}};\n"
			"x.onerror=()=>{toast('Upload failed',true);document.getElementById('go').style.pointerEvents='';};\n"
			"x.open('POST','/update');x.send(fd);}\n");
		request->send(200, "text/html", html);
	});

	server.on("/update", HTTP_POST,
		// Runs once the body is fully consumed by the upload callback below.
		[this](AsyncWebServerRequest *request) {
			const bool ok = !Update.hasError();
			AsyncWebServerResponse* resp = request->beginResponse(ok ? 200 : 400, "text/plain",
			                                                      ok ? "OK" : Update.errorString());
			resp->addHeader("Connection", "close");		// the socket dies with the restart anyway
			request->send(resp);
			if (ok) {
				ui.otaProgress(100);
				otaRebootAt = millis() + 1500;			// let the response drain first
			} else {
				bclog.logf(BCLogger::Log_Error, TAG, "OTA failed: %s", Update.errorString());
			}
		},
		// Body chunks, in order, on the async_tcp task.
		[this](AsyncWebServerRequest *request, const String& filename, size_t index,
		       uint8_t* data, size_t len, bool final) {
			if (index == 0) {
				// A filesystem image has to go to the other partition; anything else is
				// treated as application firmware.
				String lower = filename;
				lower.toLowerCase();
				const int partition = (lower.indexOf("littlefs") >= 0 || lower.indexOf("spiffs") >= 0)
				                      ? U_SPIFFS : U_FLASH;
				otaTotal = request->contentLength();
				bclog.logf(BCLogger::Log_Info, TAG, "OTA start: %s -> %s partition, %u byte",
				           filename.c_str(), partition == U_SPIFFS ? "filesystem" : "app", otaTotal);
				ui.otaStart();
				if (!Update.begin(UPDATE_SIZE_UNKNOWN, partition)) {
					bclog.logf(BCLogger::Log_Error, TAG, "OTA begin failed: %s", Update.errorString());
					return;
				}
			}
			if (Update.isRunning() && len && Update.write(data, len) != len) {
				bclog.logf(BCLogger::Log_Error, TAG, "OTA write failed: %s", Update.errorString());
				return;
			}
			if (Update.isRunning() && otaTotal) {
				// Coarse, because this fires per TCP chunk and the UI redraw is not free.
				const uint8_t perc = static_cast<uint8_t>(((index + len) * 100) / otaTotal);
				if (perc != otaLastPerc) {
					otaLastPerc = perc;
					ui.otaProgress(perc);
				}
			}
			if (final && Update.isRunning() && !Update.end(true)) {
				bclog.logf(BCLogger::Log_Error, TAG, "OTA end failed: %s", Update.errorString());
			}
		});
}

void WifiWebserver::setupWebserver() {
	// checkLoop() calls this on every WiFi (re)connect -- wifiWasConnected is reset in
	// enableWifi() and in the /wifi/connect handler. AsyncServer::begin() is idempotent
	// ("if (_pcb) return;"), but server.on() and addMiddleware() are
	// not: they new + append unconditionally. Without this guard every reconnect leaked
	// ~21 handler objects AND made _attachHandler() walk a longer list on every request.
	if (webserverStarted) {
		server.begin();
		return;
	}
	webserverStarted = true;

	// Request instrumentation. AsyncWebServer derives from AsyncMiddlewareChain and runs
	// _runChain() for EVERY request -- static files and 404s included -- so one middleware
	// covers all routes instead of a line in each of the ~20 handlers.
	{
		WebInstr::setup();
		server.addMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
			const uint32_t startMs = millis();
			const size_t internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
			WebInstr::onStart(request);
			// onDisconnect() fires after the response is done (or the client gave up), and
			// _onDisconnect() invokes it before the request object is deleted, so url() is
			// still readable in there. NOTE: _onDisconnectfn is a single slot -- a handler
			// that calls request->onDisconnect() itself silently replaces this hook.
			request->onDisconnect([request, startMs, internalFree]() {
				WebInstr::onEnd(request, startMs, internalFree);
			});
			next();
		});
	}

	// Enable file deletion
    // using DELETE method on the same URI as for "serveStatic" would be more elegant, but is not possible to create links that result in making the browser use DELETE method. So use special "del" uri
	server.on("/del", HTTP_GET, [](AsyncWebServerRequest *request) {
		// DELETE on the serveStatic URI would be more elegant, but a link can't make the
		// browser use the DELETE method -- hence this separate "/del/" prefix.
		const String rel = pathTail(request, "/del/");
		bclog.logf(BCLogger::Log_Info, TAG, "💻 Request on /del/: %s\n\tPath: %s", request->url().c_str(), rel.c_str());
		if (!isSafeRelPath(rel)) {
			request->send(400, "text/plain", "Invalid path");
			return;
		}
		const String dUri = BCLogger::LOGDIR + "/" + rel;
		uint16_t http_code = 500;
		String html_resp;
		// Normally fetched by the listing's JavaScript, which only looks at the status --
		// this page is what you get when the URL is opened directly.
		WebPage::begin(html_resp, "Delete");
		if (bclog.deleteFile(dUri)) {
			http_code = 200;
			html_resp += F("<p>OK - file deleted.</p>\n");
		} else {
			http_code = 403;
			html_resp += F("<p class=\"badge badge-error\">Forbidden - file can't be deleted (probably because it does not exist).</p>\n");
		}
		WebPage::end(html_resp);
		request->send(http_code, "text/html", html_resp.c_str());
	});
	server.on("/replay", HTTP_GET, [](AsyncWebServerRequest *request) {
		const String rel = pathTail(request, "/replay/");
		bclog.logf(BCLogger::Log_Info, TAG, "💻 Request on /replay/: %s\n\tPath: %s", request->url().c_str(), rel.c_str());
		if (!isSafeRelPath(rel)) {
			request->send(400, "text/plain", "Invalid path");
			return;
		}
		const String dUri = BCLogger::LOGDIR + "/" + rel;
		uint16_t http_code = 500;
		String html_resp;
		// Normally fetched by the listing's JavaScript, which only looks at the status --
		// this page is what you get when the URL is opened directly.
		WebPage::begin(html_resp, "Replay");
		if (bclog.replayFile(dUri)) {
			http_code = 200;
			html_resp += F("<p>OK - file replay started</p>\n");
		} else {
			http_code = 403;
			html_resp += F("<p class=\"badge badge-error\">Forbidden - file can't be openend for replay (probably because it does not exist).</p>\n");
		}
		WebPage::end(html_resp);
		request->send(http_code, "text/html", html_resp.c_str());
	});

	// -- generate Logfile Index
	server.on("/logfiles/", HTTP_GET, [this](AsyncWebServerRequest *request) {
		htmlresponse.clear();
		// No Serial.println(htmlresponse) here any more: dumping the whole ~20KB page over
		// USB CDC blocks the async_tcp task, which is watchdog-guarded with a 5s panic.
		const uint16_t code = bclog.getAllFileLinks(htmlresponse);
		request->send(code, "text/html", htmlresponse.c_str());
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
		String html;
		WebPage::begin(html, "System");
		html += F("<table><tbody>\n<tr><td>Version</td><td>" VERSION "</td></tr>\n<tr><td>Free heap</td><td>");
		html += ESP.getFreeHeap() / 1024;
		html += F(" kB</td></tr>\n<tr><td>Free internal</td><td>");
		html += heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) / 1024;
		html += F(" kB</td></tr>\n<tr><td>Free PSRAM</td><td>");
		html += ESP.getFreePsram() / 1024;
		html += F(" kB</td></tr>\n<tr><td>Open requests</td><td>");
		html += WebInstr::open();
		html += F(" (peak ");
		html += WebInstr::openPeak();
		html += F(")</td></tr>\n<tr><td>Uptime</td><td>");
		html += millis() / 1000;
		html += F(" s</td></tr>\n<tr><td>LittleFS</td><td>");
		html += LittleFS.usedBytes() / 1024;
		html += F(" kB of ");
		html += LittleFS.totalBytes() / 1024;
		html += F(" kB</td></tr>\n<tr><td>SD card</td><td>");
		const sdcard_type_t ctype = SD_MMC.cardType();
		if (ctype != CARD_UNKNOWN && ctype != CARD_NONE) {
			html += static_cast<unsigned long>(SD_MMC.usedBytes() / (1024 * 1024));
			html += F(" MB of ");
			html += static_cast<unsigned long>(SD_MMC.cardSize() / (1024 * 1024));
			html += F(" MB");
		} else {
			html += F("not detected");
		}
		html += F("</td></tr>\n</tbody></table>\n");
		WebPage::end(html);
		request->send(200, "text/html", html);
	});

	server.on("/dev/", HTTP_GET,  [this](AsyncWebServerRequest *request) {
		htmlresponse.clear();
		bleDevs.getHTMLPage(htmlresponse);
		request->send(200, "text/html", htmlresponse.c_str());
	});

	// Registered AFTER the "/dev/" index route above: _attachHandler() takes the first
	// match in registration order, and "/dev" would otherwise swallow "/dev/" as well.
	server.on("/dev", HTTP_GET, [this] (AsyncWebServerRequest *request) {
		htmlresponse.clear();
		const String cmd = pathTail(request, "/dev/");
		const AsyncWebParameter* para = request->getParam((size_t)0);
		bclog.logf(BCLogger::Log_Info, TAG, "💻 Request on /dev/: %s\n\tCmd: %s - %s", request->url().c_str(), cmd.c_str(), para ? para->value().c_str() : "n/a");
		if (!isSafeDevCmd(cmd)) {
			request->send(400, "text/plain", "Invalid command");
		} else if (!para) {
			request->send(400, "text/plain", "Missing parameter");
		} else {
			int16_t code = bleDevs.procHTMLCmd(htmlresponse, cmd, para->value());
			request->send(code, "text/plain", htmlresponse.c_str());
		}
	});

#ifdef TRGBBC_SENSORS_I2C
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

	// BMI160 debug page. The action routes are registered before "/debug/imu" on purpose:
	// a plain route also matches "<uri>/..." (AsyncCallbackWebHandler::canHandle), so the
	// page handler would otherwise swallow them. The handlers only raise a flag -- the
	// calibration itself runs in the ImuTask, not on async_tcp's tight stack.
	server.on("/debug/imu/cal", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (sensors.requestIMUCalibration()) {
			request->send(200, "text/plain", "Calibration started");
		} else {
			request->send(409, "text/plain", "IMU not running or calibration already in progress");
		}
	});
	server.on("/debug/imu/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
		sensors.requestIMUMinMaxReset();
		request->send(200, "text/plain", "Min/max reset");
	});
	server.on("/debug/imu.json", HTTP_GET, [](AsyncWebServerRequest *request) {
		String json;
		sensors.getIMUJson(json);
		request->send(200, "application/json", json);
	});
	server.on("/debug/imu", HTTP_GET, [](AsyncWebServerRequest *request) {
		String html;
		sensors.getIMUDebugPage(html);
		request->send(200, "text/html", html);
	});
#endif		//TODO: Add height (pressure) adjustment for FL


	// Debug menu: these pages exist but were reachable only by typing the URL.
	// Registered as "/debug/menu" because "/debug/" is the static live-log page.
	server.on("/debug/menu", HTTP_GET, [](AsyncWebServerRequest *request) {
		struct Entry { const char* href; const char* name; const char* desc; };
		static const Entry kEntries[] = {
			{ "/debug/",              "Live Log",        "Log stream over SSE, as it happens" },
			{ "/debug/nvs",           "NVS Contents",    "Every key stored in non-volatile storage" },
#ifdef TRGBBC_SENSORS_I2C
			{ "/debug/imu",           "IMU (BMI160)",    "Live accelerometer data and calibration" },
#endif
			{ "/stat/debugarray",     "Chart Array",     "Raw heart-rate chart ring buffer" },
			{ "/stat/dist_debug.html","Distance Debug",  "Raw distance/wheel-revolution data" },
			{ "/log/",                "Raw SD Browser",  "Unformatted directory listing of the SD card" },
		};
		String html;
		WebPage::begin(html, "Debug");
		html += F("<div class=\"tiles\">\n");
		for (const Entry& e : kEntries) {
			html += F("<a class=\"link-box\" href=\"");
			html += e.href;
			html += F("\">");
			html += e.name;
			html += F("<span class=\"eyebrow\" style=\"display:block;margin-top:4px;\">");
			html += e.desc;
			html += F("</span></a>\n");
		}
		html += F("</div>\n<h3 style=\"margin-top:24px;\">Danger zone</h3>\n"
		          "<p class=\"eyebrow\">Forces a crash so a coredump is written. The device reboots.</p>\n"
		          "<a class=\"btn btn-ghost\" href=\"#\" style=\"border-color:var(--rr-err,#C1604A);\" "
		          "onclick=\"crash();return false;\">Force coredump</a>\n");
		WebPage::end(html,
			"function crash(){if(!confirm('Crash the device now to write a coredump?\\n\\n"
			"It will reboot and any unflushed log data is lost.'))return;"
			"req('/coredump_now','Crashing...');}\n");
		request->send(200, "text/html", html);
	});

	server.on("/log/set", HTTP_GET, [](AsyncWebServerRequest *request) {
		// getParam() returns nullptr for a missing param -- log.html didn't send "output" at all
		// until this fix, which made this an unconditional null-pointer crash on every use.
		String tag = request->hasParam("tag") ? request->getParam("tag")->value() : "";
		String level = request->hasParam("level") ? request->getParam("level")->value() : "";
		String output = request->hasParam("output") ? request->getParam("output")->value() : "";

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

		// Serial and File levels can differ (bclog.getLogLevel()'s "serial" param, default false,
		// used to mean this only ever reported the File level) -- report both.
		for (uint16_t t = BCLogger::TAG_RAW_NMEA; t < BCLogger::LogTagMax; t++) {
			JsonObject tagLevels = doc[BCLogger::TAG_STRING[t]].to<JsonObject>();
			tagLevels["file"] = BCLogger::LEVEL_STRING[bclog.getLogLevel(static_cast<BCLogger::LogTag>(t), false)];
			tagLevels["serial"] = BCLogger::LEVEL_STRING[bclog.getLogLevel(static_cast<BCLogger::LogTag>(t), true)];
			// The live log shows only the icon per line to keep the columns aligned, so its
			// filter chips carry the icon/name pairing and act as the legend. Serving the
			// icon here means every tag has one from the start, instead of only those that
			// happened to emit a line already. Additive: log.html reads file/serial only.
			tagLevels["icon"] = BCLogger::TAG_SYMBOL[t];
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

//
//	PrettyOTA       OTAUpdates;
//	OTAUpdates.Begin(&server);
//	OTAUpdates.OnStart([](NSPrettyOTA::UPDATE_MODE updateMode) {
//		bclog.log(BCLogger::Log_Info, TAG, "Start OTA Update");
//		ui.otaStart();
//	});
//	OTAUpdates.OnProgress([](size_t current, size_t total) {
//		uint8_t perc = (current * 100) / total;
//		bclog.logf(BCLogger::Log_Debug, TAG, "OTA Update: %d %% [%d byte from %d byte].", perc, current, total);
//		ui.otaProgress(perc);
//	});

	setupOta();

	// -- download Binary Logfile
	server.serveStatic("/log/", SD_MMC, "/BIKECOMP/");
	server.serveStatic("/", LittleFS, "/site/").setCacheControl("max-age=31536000").setDefaultFile("index.html");
	//server.serveStatic("/core/", LittleFS, "/core/").setCacheControl("max-age=31536000");

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
		String resp;
		WebPage::begin(resp, "NVS Contents");
		nvs_iterator_t it = NULL;
		esp_err_t res = nvs_entry_find("nvs", NULL, NVS_TYPE_ANY, &it);
		if (res != ESP_OK) {
			bclog.log(BCLogger::Log_Warn, TAG, "Can't iterate over NVS");
			resp += F("<p class=\"badge badge-error\">Cannot iterate over NVS</p>\n");
			WebPage::end(resp);
			request->send(500, "text/html", resp);
			return;
		}
		resp += F("<table><thead><tr><th>Namespace</th><th>Key</th><th>Type</th></tr></thead><tbody>\n");
		uint16_t entries = 0;
		while (res == ESP_OK) {
			nvs_entry_info_t info;
			nvs_entry_info(it, &info);
			resp += F("<tr><td>");
			resp += info.namespace_name;
			resp += F("</td><td>");
			resp += info.key;
			resp += F("</td><td><span class=\"badge badge-debug\">");
			resp += type_to_str(info.type);
			resp += F("</span></td></tr>\n");
			entries++;
			res = nvs_entry_next(&it);
		}
		nvs_release_iterator(it);
		resp += F("</tbody></table>\n<p class=\"eyebrow\" style=\"margin-top:12px;\">");
		resp += entries;
		resp += F(" entries</p>\n");
		WebPage::end(resp);
		request->send(200, "text/html", resp);
	});

}

#endif
