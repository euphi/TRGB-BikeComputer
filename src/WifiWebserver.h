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
#include <atomic>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <global_settings.h>
#include "WifiConfig.h"

// One entry of the last scan, as shown on the display and on /wifi.
struct WifiScanEntry {
	char ssid[WifiCfg::SSID_MAX + 1];
	int8_t rssi;
	bool open;
	bool known;		// stored in the network list
};

// What the device is doing with the radio, for the UI and /wifi/state.
enum class WifiPhase : uint8_t {
	OFF = 0,
	SCANNING,		// autoconnect: looking for a stored network
	CONNECTING,		// autoconnect: associating with one
	WAITING,		// autoconnect: nothing worked, next scan in a moment
	ONLINE,
	ACCESS_POINT,
};

struct WifiStatus {
	WifiPhase phase = WifiPhase::OFF;
	char ssid[WifiCfg::SSID_MAX + 1] = "";		// network joined / being tried, or the AP's own
	char apPassword[WifiCfg::PW_MAX + 1] = "";	// only filled in the ACCESS_POINT phase (for the display)
	char ip[16] = "";
	int8_t rssi = 0;
	uint8_t stations = 0;						// clients of the AP
	bool scanning = false;
	uint32_t scanVersion = 0;					// changes when the set of visible networks changes
};

/*
 * WiFi and the web server.
 *
 * Networks (up to WifiCfg::MAX_NETWORKS) live in the NVS as one blob, their order is the
 * priority (see WifiConfig.h; no credentials in the sources). After boot, and whenever the
 * settings screen switches WiFi on, checkLoop() scans, tries the stored networks that are
 * in range in that order and keeps going until WifiWebserver::OFFLINE_TIMEOUT_MS have passed
 * without a connection -- then WiFi is switched off to save power. It only comes back via
 * the settings screen (requestReconnect(), requestAccessPoint()) or the serial console.
 * The same 5 minutes apply to the access point without a client.
 */
class WifiWebserver {
public:
	static constexpr uint32_t OFFLINE_TIMEOUT_MS = 5 * 60 * 1000;
	static constexpr uint32_t CONNECT_TIMEOUT_MS = 15 * 1000;		// per network
	static constexpr uint32_t RESCAN_PAUSE_MS = 15 * 1000;			// nothing in range / everything failed
	static constexpr size_t SCAN_MAX = 20;

	WifiWebserver();
	void setup();
	void checkLoop();

	// Settings screen / CLI. They only raise a flag (safe from any task, including LVGL event
	// callbacks); checkLoop() does the work and reports back through UIFacade::updateIP().
	void requestReconnect() {switchRequest = REQ_ON;}			// WiFi on, autoconnect (leaves the AP)
	void requestDisable() {switchRequest = REQ_OFF;}
	void requestRestart() {switchRequest = REQ_RESTART;}		// drop the connection and autoconnect from scratch
	void requestAccessPoint(bool on) {switchRequest = on ? REQ_AP_ON : REQ_AP_OFF;}
	// Starts a scan, switching WiFi on if needed. False if one is running or the radio is busy
	// connecting. Results: getScan().
	bool requestScan();

	void getStatus(WifiStatus& out);
	// Copies the last scan (strongest first); returns the number of entries.
	size_t getScan(WifiScanEntry* out, size_t max);

	// Network list. All of these take the internal lock; none of them may be called with
	// xUIDrawMutex held except networkCount()/isKnown() (no NVS access).
	size_t networkCount();
	bool isKnown(const char* ssid);
	WifiCfg::Result addNetwork(const char* ssid, const char* password, bool hidden = false,
	                           bool keepPasswordIfEmpty = true);
	WifiCfg::Result removeNetwork(const char* ssid);
	WifiCfg::Result moveNetwork(const char* ssid, int delta);		// -1 = higher priority
	WifiCfg::Result setAccessPoint(const char* ssid, const char* password);
	// Display: the NVS write needs more stack than the UI task has (doc/PITFALLS.md), so this
	// stores the network in a task of its own and then starts the autoconnect.
	bool addNetworkAsync(const char* ssid, const char* password);

	AsyncWebServer& getServer() {return server;}
	void enableWebserver() {startupComplete = true;}	// call once, after all routes are registered

private:
	enum SwitchRequest : uint8_t {REQ_NONE = 0, REQ_ON, REQ_OFF, REQ_AP_ON, REQ_AP_OFF, REQ_SCAN, REQ_RESTART};

	void setupWebserver();
	void setupOta();					// own OTA, replaces the ElegantOTA dependency
	void setupWifiRoutes();				// WifiRoutes.cpp: /wifi page and its JSON interface
	void registerCli();
	uint32_t otaRebootAt = 0;			// millis() deadline, 0 = no restart pending
	size_t   otaTotal = 0;				// Content-Length of the running upload, for progress
	uint8_t  otaLastPerc = 255;			// throttles ui.otaProgress() to actual changes
	Ticker wifiCheckTicker;
	bool webserverStarted = false;
	// Routes are registered by several modules from main's setup() (stats, distance,
	// bclog), but the server is started from checkLoop() as soon as WiFi is up -- which
	// after a warm reboot is ~1s, long before setup() has got that far. Requests arriving
	// in that window 404 on routes that simply do not exist yet. enableWebserver() closes
	// that window; see the call at the end of main's setup().
	volatile bool startupComplete = false;	// routes are registered exactly once, see setupWebserver()
	AsyncWebServer server;

	String htmlresponse;		// Buffer for response

	// --- network list ---------------------------------------------------------------
	SemaphoreHandle_t cfgMutex = nullptr;	// guards cfg, scan cache and status; never call ui.* while holding it
	WifiCfg::Config cfg;
	void loadConfig();
	void saveConfigLocked();				// NVS write, cfgMutex held

	// --- state machine (checkLoop() only, except where noted) -------------------------
	std::atomic<uint8_t> switchRequest{REQ_NONE};
	std::atomic<WifiPhase> phase{WifiPhase::OFF};		// written by checkLoop() only
	uint32_t phaseSince = 0;				// millis() when `phase` was entered
	uint32_t offlineSince = 0;				// millis() since when there is no connection (or, in AP mode, no client)
	uint8_t candidates[WifiCfg::MAX_NETWORKS];
	uint8_t candidateCount = 0;
	uint8_t candidateIdx = 0;
	bool mdnsStarted = false;
	bool everConnected = false;				// since WiFi was last switched on: tells "not found" from "lost"
	char statusSsid[WifiCfg::SSID_MAX + 1] = "";	// guarded by cfgMutex, for getStatus()

	void enterPhase(WifiPhase p);
	void startAutoconnect(bool force);
	void startAccessPoint();
	void disableWifi(const char* reason);	// reason: status text for the settings screen
	void connectNext();
	void startMdns();
	void stopMdns();
	// Captive portal of the access point: every name resolves to the device and unknown URLs
	// redirect to /wifi (setupWebserver()'s not-found handler). Without it a phone that has mobile
	// data keeps using that for the browser and cannot reach 192.168.4.1 at all.
	DNSServer dns;
	std::atomic<bool> dnsRun{false};
	std::atomic<bool> dnsTaskAlive{false};
	void startCaptiveDns();
	void stopCaptiveDns();
	void publishUi(const char* offText = nullptr);
	static const char* phaseName(WifiPhase p);

	// --- scan ---------------------------------------------------------------------------
	WifiScanEntry scanCache[SCAN_MAX];
	size_t scanCount = 0;
	uint32_t scanHash = 0;
	uint32_t scanVersion = 0;
	std::atomic<bool> scanRunning{false};
	bool startScan();						// true if one was started
	void pollScan();

#ifdef DEBUG_APP
	void setupNvsDebug();
#endif

};
