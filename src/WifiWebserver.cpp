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
#include "CrashInfo.h"
#include "NvsUtil.h"

//TODO: Read this from preferences
const char* ntpServer = "pool.ntp.org";

static const BCLogger::LogTag TAG = BCLogger::TAG_WIFI;
// Refuse new HTTP requests below this much free internal heap (see the middleware in begin()).
static const size_t LOW_HEAP_REJECT_BYTES = 20 * 1024;

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

namespace {
// Scoped take/give for WifiWebserver::cfgMutex.
struct Lock {
	SemaphoreHandle_t m;
	explicit Lock(SemaphoreHandle_t mutex): m(mutex) {if (m) xSemaphoreTake(m, portMAX_DELAY);}
	~Lock() {if (m) xSemaphoreGive(m);}
};

const char* const NVS_NAMESPACE = "WifiSettings";
const char* const NVS_KEY_CFG = "cfg";
}

// ---------------------------------------------------------------------------
// Stored networks
// ---------------------------------------------------------------------------

void WifiWebserver::loadConfig() {
	Lock lock(cfgMutex);
	bool save = false;
	if (NvsUtil::namespaceExists(NVS_NAMESPACE)) {
		Preferences p;
		if (p.begin(NVS_NAMESPACE, false)) {
			WifiCfg::Blob blob;
			if (NvsUtil::loadBlob(p, NVS_KEY_CFG, blob) && !cfg.fromBlob(blob)) {
				bclog.log(BCLogger::Log_Warn, TAG, "Stored WiFi list unusable - starting empty");
			}
			// Keys of the single network that used to be stored here (and mostly overwritten from
			// the sources). Dropping them also removes that plaintext password from the NVS.
			static const char* const LEGACY[] = {"SSID_0", "PW_0", "SSID_1", "PW_1", "SSID_2", "PW_2", "disApMode"};
			for (const char* key : LEGACY) {
				if (p.isKey(key)) p.remove(key);
			}
			p.end();
		}
	}
	if (cfg.ensureAp(esp_random)) save = true;		// first boot: default SSID and a random password
	if (save) saveConfigLocked();
	bclog.logf(BCLogger::Log_Info, TAG, "%u WiFi network(s) stored, access point \"%s\"",
	           (unsigned) cfg.count(), cfg.accessPoint().ssid);
}

void WifiWebserver::saveConfigLocked() {
	WifiCfg::Blob blob;
	cfg.toBlob(blob);
	Preferences p;
	if (!p.begin(NVS_NAMESPACE, false) || !NvsUtil::saveBlob(p, NVS_KEY_CFG, blob)) {
		bclog.log(BCLogger::Log_Error, TAG, "Saving the WiFi list to NVS failed");
	}
	p.end();
}

size_t WifiWebserver::networkCount() {
	Lock lock(cfgMutex);
	return cfg.count();
}

bool WifiWebserver::isKnown(const char* ssid) {
	Lock lock(cfgMutex);
	return cfg.find(ssid) >= 0;
}

WifiCfg::Result WifiWebserver::addNetwork(const char* ssid, const char* password, bool hidden, bool keepPasswordIfEmpty) {
	Lock lock(cfgMutex);
	const WifiCfg::Result r = cfg.add(ssid, password, hidden, keepPasswordIfEmpty);
	if (r == WifiCfg::Result::ADDED || r == WifiCfg::Result::UPDATED) {
		saveConfigLocked();
		bclog.logf(BCLogger::Log_Info, TAG, "WiFi network \"%s\" %s (%u stored)", ssid,
		           r == WifiCfg::Result::ADDED ? "added" : "updated", (unsigned) cfg.count());
	}
	return r;
}

WifiCfg::Result WifiWebserver::removeNetwork(const char* ssid) {
	Lock lock(cfgMutex);
	const int i = cfg.find(ssid);
	if (i < 0) return WifiCfg::Result::BAD_INDEX;
	const WifiCfg::Result r = cfg.remove(i);
	if (r == WifiCfg::Result::OK) {
		saveConfigLocked();
		bclog.logf(BCLogger::Log_Info, TAG, "WiFi network \"%s\" removed", ssid);
	}
	return r;
}

WifiCfg::Result WifiWebserver::moveNetwork(const char* ssid, int delta) {
	Lock lock(cfgMutex);
	const int i = cfg.find(ssid);
	if (i < 0) return WifiCfg::Result::BAD_INDEX;
	const int to = i + delta;
	if (to < 0 || to >= (int) cfg.count()) return WifiCfg::Result::OK;		// already first/last
	const WifiCfg::Result r = cfg.move(i, to);
	if (r == WifiCfg::Result::OK) saveConfigLocked();
	return r;
}

WifiCfg::Result WifiWebserver::setAccessPoint(const char* ssid, const char* password) {
	Lock lock(cfgMutex);
	const WifiCfg::Result r = cfg.setAccessPoint(ssid, password);
	if (r == WifiCfg::Result::OK) {
		saveConfigLocked();
		bclog.logf(BCLogger::Log_Info, TAG, "Access point settings changed (SSID \"%s\")", ssid);
	}
	return r;
}

bool WifiWebserver::addNetworkAsync(const char* ssid, const char* password) {
	struct Job {char ssid[WifiCfg::SSID_MAX + 1]; char pw[WifiCfg::PW_MAX + 1];};
	Job* job = new (std::nothrow) Job;
	if (!job) return false;
	strlcpy(job->ssid, ssid, sizeof(job->ssid));
	strlcpy(job->pw, password, sizeof(job->pw));
	const BaseType_t ok = xTaskCreate(+[](void* arg) {
		Job* j = static_cast<Job*>(arg);
		const WifiCfg::Result r = webserver.addNetwork(j->ssid, j->pw);
		// Connect right away when the radio is idle. While it is up (online or access point)
		// the new network is just stored: it has the lowest priority, and switching would
		// drop the connection the user may be using right now.
		const WifiPhase p = webserver.phase.load();
		if ((r == WifiCfg::Result::ADDED || r == WifiCfg::Result::UPDATED)
		    && (p == WifiPhase::OFF || p == WifiPhase::WAITING)) {
			webserver.requestReconnect();
		}
		delete j;
		vTaskDelete(NULL);
	}, "WifiAdd", 4096, job, 4, nullptr);
	if (ok != pdPASS) {
		delete job;
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Setup, CLI
// ---------------------------------------------------------------------------

void WifiWebserver::setup() {
	LittleFS.begin();		// WifiWebserver is also responsible for enabling LittleFS, because it is only used for Website storage (Logging is on SDCARD, which is maintained in BClogger).
	cfgMutex = xSemaphoreCreateMutex();
	loadConfig();

	configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", ntpServer);
	registerCli();
	wifiCheckTicker.attach_ms(500, +[](WifiWebserver *thisInstance) {thisInstance->checkLoop();}, this);
	startAutoconnect(false);
}

static const char* resultText(WifiCfg::Result r) {
	switch (r) {
	case WifiCfg::Result::OK: return "ok";
	case WifiCfg::Result::ADDED: return "added";
	case WifiCfg::Result::UPDATED: return "updated";
	case WifiCfg::Result::BAD_SSID: return "SSID must be 1..32 bytes";
	case WifiCfg::Result::BAD_PASSWORD: return "password must be 8..63 characters (empty = open network)";
	case WifiCfg::Result::FULL: return "list is full";
	case WifiCfg::Result::BAD_INDEX: return "unknown network";
	}
	return "?";
}

void WifiWebserver::registerCli() {
	static Command wifiCmd = console.addCmd("wifi", +[](cmd* c) {
		Command command(c);
		const String action = command.getArgument("action").getValue();
		const String a = command.getArgument("a").getValue();
		const String b = command.getArgument("b").getValue();
		if (action.equalsIgnoreCase("on")) {
			webserver.requestReconnect();
		} else if (action.equalsIgnoreCase("off")) {
			webserver.requestDisable();
		} else if (action.equalsIgnoreCase("ap")) {
			webserver.requestAccessPoint(!a.equalsIgnoreCase("off"));
		} else if (action.equalsIgnoreCase("scan")) {
			if (!webserver.requestScan()) bclog.log(BCLogger::Log_Info, BCLogger::TAG_CLI, "Scan not possible right now");
		} else if (action.equalsIgnoreCase("add")) {
			// the password is not echoed anywhere: SerialConsole::run() redacts "wifi add ..."
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_CLI, "wifi add: %s",
			           resultText(webserver.addNetwork(a.c_str(), b.c_str())));
		} else if (action.equalsIgnoreCase("apset")) {
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_CLI, "wifi apset: %s",
			           resultText(webserver.setAccessPoint(a.c_str(), b.c_str())));
		} else if (action.equalsIgnoreCase("del")) {
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_CLI, "wifi del: %s", resultText(webserver.removeNetwork(a.c_str())));
		} else if (action.equalsIgnoreCase("list")) {
			Lock lock(webserver.cfgMutex);
			for (size_t i = 0; i < webserver.cfg.count(); i++) {
				bclog.logf(BCLogger::Log_Info, BCLogger::TAG_CLI, "%u. %s%s%s", (unsigned) (i + 1), webserver.cfg.ssid(i),
				           webserver.cfg.hasPassword(i) ? "" : " (open)", webserver.cfg.hidden(i) ? " (hidden)" : "");
			}
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_CLI, "%u network(s), access point \"%s\"",
			           (unsigned) webserver.cfg.count(), webserver.cfg.accessPoint().ssid);
		}
		WifiStatus st;
		webserver.getStatus(st);
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_CLI, "WiFi %s, IPv4 %s, SSID %s%s", phaseName(st.phase),
		           st.ip[0] ? st.ip : "-", st.ssid[0] ? st.ssid : "-", st.scanning ? ", scanning" : "");
	}, +[](uint8_t pos, SerialConsole::Matches& m) {
		if (pos == 1) {
			m.add("status"); m.add("on"); m.add("off"); m.add("ap"); m.add("scan");
			m.add("list"); m.add("add"); m.add("del"); m.add("apset");
		}
	});
	wifiCmd.addPositionalArgument("action", "status");
	wifiCmd.addPositionalArgument("a", "");
	wifiCmd.addPositionalArgument("b", "");
	wifiCmd.setDescription("WiFi: status | on (autoconnect, like the settings screen) | off | ap [on|off] | scan | "
	                       "list | add <ssid> <password> | del <ssid> | apset <ssid> <password> (hotspot). Quote names with spaces.");
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

const char* WifiWebserver::phaseName(WifiPhase p) {
	switch (p) {
	case WifiPhase::OFF: return "off";
	case WifiPhase::SCANNING: return "scanning";
	case WifiPhase::CONNECTING: return "connecting";
	case WifiPhase::WAITING: return "waiting";
	case WifiPhase::ONLINE: return "online";
	case WifiPhase::ACCESS_POINT: return "access point";
	}
	return "?";
}

void WifiWebserver::enterPhase(WifiPhase p) {
	phase = p;
	phaseSince = millis();
}

void WifiWebserver::getStatus(WifiStatus& out) {
	out = WifiStatus();
	out.phase = phase.load();
	out.scanning = scanRunning;
	{
		Lock lock(cfgMutex);
		strlcpy(out.ssid, statusSsid, sizeof(out.ssid));
		out.scanVersion = scanVersion;
		if (out.phase == WifiPhase::ACCESS_POINT) {
			strlcpy(out.ssid, cfg.accessPoint().ssid, sizeof(out.ssid));
			strlcpy(out.apPassword, cfg.accessPoint().pw, sizeof(out.apPassword));
		}
	}
	if (out.phase == WifiPhase::ONLINE) {
		strlcpy(out.ip, WiFi.localIP().toString().c_str(), sizeof(out.ip));
		out.rssi = WiFi.RSSI();
	} else if (out.phase == WifiPhase::ACCESS_POINT) {
		strlcpy(out.ip, WiFi.softAPIP().toString().c_str(), sizeof(out.ip));
		out.stations = WiFi.softAPgetStationNum();
	}
}

// Tells the settings screen what is going on -- only called on a change, never per tick.
void WifiWebserver::publishUi(const char* offText) {
	WifiStatus st;
	getStatus(st);
	switch (st.phase) {
	case WifiPhase::OFF:
		ui.updateIP(String(offText ? offText : "WLAN aus"), UIFacade::WIFI_UI_OFF);
		break;
	case WifiPhase::ONLINE:
		ui.updateIP(String(st.ip), UIFacade::WIFI_UI_ONLINE);
		break;
	case WifiPhase::ACCESS_POINT:
		// The password is what somebody standing at the display needs to join.
		ui.updateIP(String(st.apPassword), UIFacade::WIFI_UI_AP, String("HOTSPOT ") + st.ssid);
		break;
	default:
		ui.updateIP(String("verbinde ..."), UIFacade::WIFI_UI_CONNECTING);
		break;
	}
}

void WifiWebserver::startMdns() {
	if (mdnsStarted) return;
	mdnsStarted = MDNS.begin("TRGB-BC");
	if (mdnsStarted) MDNS.addService("http", "tcp", 80);
}

void WifiWebserver::stopMdns() {
	if (!mdnsStarted) return;		// bound to the interface that goes away with a mode change
	MDNS.end();
	mdnsStarted = false;
}

void WifiWebserver::startCaptiveDns() {
	if (dnsRun) return;
	dns.setErrorReplyCode(DNSReplyCode::NoError);
	if (!dns.start(53, "*", WiFi.softAPIP())) {
		bclog.log(BCLogger::Log_Warn, TAG, "Captive portal DNS could not start");
		return;
	}
	dnsRun = true;
	dnsTaskAlive = true;
	const BaseType_t ok = xTaskCreate(+[](void* arg) {
		WifiWebserver* self = static_cast<WifiWebserver*>(arg);
		while (self->dnsRun) {
			self->dns.processNextRequest();
			vTaskDelay(pdMS_TO_TICKS(10));
		}
		self->dnsTaskAlive = false;
		vTaskDelete(NULL);
	}, "CaptiveDNS", 3072, this, 2, nullptr);
	if (ok != pdPASS) {
		dnsRun = false;
		dnsTaskAlive = false;
		dns.stop();
	}
}

void WifiWebserver::stopCaptiveDns() {
	if (!dnsRun) return;
	dnsRun = false;
	for (int i = 0; i < 50 && dnsTaskAlive; i++) vTaskDelay(pdMS_TO_TICKS(10));
	dns.stop();
}

void WifiWebserver::disableWifi(const char* reason) {
	stopCaptiveDns();
	stopMdns();
	if (scanRunning) {
		WiFi.scanDelete();
		scanRunning = false;
	}
	WiFi.softAPdisconnect(true);
	WiFi.disconnect(true);
	WiFi.setSleep(true);
	WiFi.mode(WIFI_MODE_NULL);
	{
		Lock lock(cfgMutex);
		statusSsid[0] = '\0';
	}
	enterPhase(WifiPhase::OFF);
	publishUi(reason);
}

// Switches the radio to station mode and starts looking for a stored network.
void WifiWebserver::startAutoconnect(bool force) {
	if (networkCount() == 0 && !force) {
		bclog.log(BCLogger::Log_Info, TAG, "No WiFi network stored - WiFi stays off");
		disableWifi("kein Netz gespeichert");
		return;
	}
	if (phase == WifiPhase::ACCESS_POINT) {
		stopCaptiveDns();
		stopMdns();
		WiFi.softAPdisconnect(true);
	}
	WiFi.mode(WIFI_STA);
	WiFi.setSleep(true);
	WiFi.enableIPv6();
	WiFi.disconnect();
	offlineSince = millis();
	everConnected = false;
	candidateCount = 0;
	enterPhase(WifiPhase::SCANNING);
	startScan();
	publishUi();
}

void WifiWebserver::startAccessPoint() {
	char ssid[WifiCfg::SSID_MAX + 1], pw[WifiCfg::PW_MAX + 1];
	{
		Lock lock(cfgMutex);
		if (cfg.ensureAp(esp_random)) saveConfigLocked();
		strlcpy(ssid, cfg.accessPoint().ssid, sizeof(ssid));
		strlcpy(pw, cfg.accessPoint().pw, sizeof(pw));
		statusSsid[0] = '\0';
	}
	stopMdns();
	if (scanRunning) {
		WiFi.scanDelete();
		scanRunning = false;
	}
	WiFi.disconnect();
	// AP + STA: the station half stays idle but lets the web page scan for networks.
	WiFi.mode(WIFI_AP_STA);
	if (!WiFi.softAP(ssid, pw)) {
		bclog.log(BCLogger::Log_Error, TAG, "Could not start the access point");
		disableWifi("Hotspot-Fehler");
		return;
	}
	offlineSince = millis();
	enterPhase(WifiPhase::ACCESS_POINT);
	bclog.logf(BCLogger::Log_Info, TAG, "Access point \"%s\" at %s", ssid, WiFi.softAPIP().toString().c_str());
	startMdns();
	startCaptiveDns();
	if (startupComplete) setupWebserver();
	publishUi();
}

// ---------------------------------------------------------------------------
// Scan
// ---------------------------------------------------------------------------

bool WifiWebserver::startScan() {
	if (scanRunning) return false;
	if (WiFi.scanNetworks(true) == WIFI_SCAN_FAILED) {
		bclog.log(BCLogger::Log_Warn, TAG, "WiFi scan could not be started");
		return false;
	}
	scanRunning = true;
	return true;
}

bool WifiWebserver::requestScan() {
	const WifiPhase p = phase.load();
	if (scanRunning || p == WifiPhase::CONNECTING) return false;
	switchRequest = REQ_SCAN;
	return true;
}

static uint32_t hashSsid(const char* s) {		// FNV-1a
	uint32_t h = 2166136261u;
	while (*s) h = (h ^ (uint8_t) *s++) * 16777619u;
	return h;
}

// Takes the finished scan into the cache: no empty (hidden) names, one entry per SSID with
// its strongest signal, strongest first.
void WifiWebserver::pollScan() {
	if (!scanRunning) return;
	const int16_t n = WiFi.scanComplete();
	if (n == WIFI_SCAN_RUNNING) return;
	size_t count = 0;
	uint32_t hash = 0;
	{
		Lock lock(cfgMutex);
		for (int16_t i = 0; i < n; i++) {
			const String ssid = WiFi.SSID(i);
			if (ssid.isEmpty() || ssid.length() > WifiCfg::SSID_MAX) continue;
			const int8_t rssi = WiFi.RSSI(i);
			size_t k = 0;
			while (k < count && strcmp(scanCache[k].ssid, ssid.c_str()) != 0) k++;
			if (k < count) {		// same name on another channel/BSSID: keep the stronger one
				if (rssi <= scanCache[k].rssi) continue;
				for (; k + 1 < count; k++) scanCache[k] = scanCache[k + 1];
				count--;
			}
			size_t at = 0;
			while (at < count && scanCache[at].rssi >= rssi) at++;
			if (count == SCAN_MAX) {
				if (at >= SCAN_MAX) continue;
				count--;			// drop the weakest
			}
			for (size_t m = count; m > at; m--) scanCache[m] = scanCache[m - 1];
			strlcpy(scanCache[at].ssid, ssid.c_str(), sizeof(scanCache[at].ssid));
			scanCache[at].rssi = rssi;
			scanCache[at].open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
			scanCache[at].known = false;
			count++;
		}
		for (size_t i = 0; i < count; i++) hash += hashSsid(scanCache[i].ssid);		// order independent
		if (hash != scanHash || count != scanCount) scanVersion++;
		scanHash = hash;
		scanCount = count;
	}
	WiFi.scanDelete();
	scanRunning = false;
	bclog.logf(BCLogger::Log_Debug, TAG, "WiFi scan: %d found, %u usable", (int) n, (unsigned) count);
}

size_t WifiWebserver::getScan(WifiScanEntry* out, size_t max) {
	Lock lock(cfgMutex);
	const size_t n = scanCount < max ? scanCount : max;
	for (size_t i = 0; i < n; i++) {
		out[i] = scanCache[i];
		out[i].known = cfg.find(out[i].ssid) >= 0;
	}
	return n;
}

// ---------------------------------------------------------------------------
// The 500 ms tick
// ---------------------------------------------------------------------------

// Tries the next candidate, or ends the round and waits for the next scan.
void WifiWebserver::connectNext() {
	char ssid[WifiCfg::SSID_MAX + 1] = "", pw[WifiCfg::PW_MAX + 1] = "";
	while (candidateIdx < candidateCount) {
		{
			Lock lock(cfgMutex);
			const uint8_t i = candidates[candidateIdx];
			strlcpy(ssid, cfg.ssid(i), sizeof(ssid));
			strlcpy(pw, cfg.password(i), sizeof(pw));
			strlcpy(statusSsid, ssid, sizeof(statusSsid));
		}
		if (ssid[0]) break;		// the list changed under us
		candidateIdx++;
	}
	if (!ssid[0]) {
		bclog.log(BCLogger::Log_Debug, TAG, "WiFi: no (more) network to try");
		enterPhase(WifiPhase::WAITING);
		return;
	}
	bclog.logf(BCLogger::Log_Info, TAG, "WiFi: connecting to \"%s\" (%u of %u)", ssid,
	           (unsigned) candidateIdx + 1, (unsigned) candidateCount);
	WiFi.disconnect();
	WiFi.begin(ssid, pw[0] ? pw : nullptr);
	memset(pw, 0, sizeof(pw));
	enterPhase(WifiPhase::CONNECTING);
}

void WifiWebserver::checkLoop() {
	pollScan();

	switch (switchRequest.exchange(REQ_NONE)) {
	case REQ_ON:
		switch (phase.load()) {
		case WifiPhase::OFF:
		case WifiPhase::ACCESS_POINT:
			bclog.log(BCLogger::Log_Info, TAG, "WiFi switched on");
			startAutoconnect(false);
			break;
		case WifiPhase::WAITING:
			enterPhase(WifiPhase::SCANNING);		// don't wait out the pause
			startScan();
			break;
		default:
			bclog.log(BCLogger::Log_Info, TAG, "WiFi on requested, but WiFi is already on");
		}
		break;
	case REQ_OFF:
		if (phase != WifiPhase::OFF) {
			bclog.log(BCLogger::Log_Info, TAG, "WiFi switched off on request");
			disableWifi("WLAN aus");
		}
		break;
	case REQ_AP_ON:
		if (phase != WifiPhase::ACCESS_POINT) startAccessPoint();
		break;
	case REQ_AP_OFF:
		if (phase == WifiPhase::ACCESS_POINT) {
			bclog.log(BCLogger::Log_Info, TAG, "Access point switched off on request");
			disableWifi("Hotspot aus");
		}
		break;
	case REQ_RESTART:
		bclog.log(BCLogger::Log_Info, TAG, "WiFi: autoconnect restarted on request");
		startAutoconnect(false);
		break;
	case REQ_SCAN:
		switch (phase.load()) {
		case WifiPhase::OFF:
			startAutoconnect(true);		// the radio has to be on, and a stored network may be in range
			break;
		case WifiPhase::WAITING:
			enterPhase(WifiPhase::SCANNING);
			startScan();
			break;
		case WifiPhase::ONLINE:
		case WifiPhase::ACCESS_POINT:
			startScan();
			break;
		default:
			break;		// SCANNING: running anyway; CONNECTING: not worth disturbing
		}
		break;
	default:
		break;
	}

	const uint32_t now = millis();
	switch (phase.load()) {
	case WifiPhase::SCANNING:
		if (!scanRunning) {		// the cache has the result (empty if the scan failed)
			WifiCfg::Visible visible[SCAN_MAX];
			{
				Lock lock(cfgMutex);
				for (size_t i = 0; i < scanCount; i++) {
					visible[i].ssid = scanCache[i].ssid;
					visible[i].rssi = scanCache[i].rssi;
				}
				candidateCount = cfg.candidates(visible, scanCount, candidates);
			}
			candidateIdx = 0;
			connectNext();
		}
		break;
	case WifiPhase::CONNECTING: {
		const wl_status_t st = WiFi.status();
		if (st == WL_CONNECTED) {
			// Wait for every module to have registered its routes -- see startupComplete.
			// NTP is unaffected: configTzTime() already ran in setup().
			if (!startupComplete) break;
			everConnected = true;
			enterPhase(WifiPhase::ONLINE);
			bclog.logf(BCLogger::Log_Info, TAG, "Wifi connected to \"%s\". IPv4: %s", statusSsid, WiFi.localIP().toString().c_str());
			setupWebserver();
			startMdns();
			publishUi();
		} else if (st == WL_NO_SSID_AVAIL || st == WL_CONNECT_FAILED || now - phaseSince > CONNECT_TIMEOUT_MS) {
			bclog.logf(BCLogger::Log_Info, TAG, "WiFi: \"%s\" failed (status %d)", statusSsid, (int) st);
			candidateIdx++;
			connectNext();
		}
		break;
	}
	case WifiPhase::WAITING:
		if (now - phaseSince > RESCAN_PAUSE_MS) {
			enterPhase(WifiPhase::SCANNING);
			startScan();
		}
		break;
	case WifiPhase::ONLINE:
		if (WiFi.status() != WL_CONNECTED) {
			bclog.log(BCLogger::Log_Warn, TAG, "Wifi connection lost - looking for a network again");
			stopMdns();
			startAutoconnect(true);		// resets the 5 minutes: they count from the loss
			everConnected = true;
		}
		break;
	case WifiPhase::ACCESS_POINT:
		if (WiFi.softAPgetStationNum() > 0) offlineSince = now;
		break;
	default:
		break;
	}

	// The 5 minute rule, for everything that is not (yet) a working connection: no network found
	// or none that works, connection lost, or an access point nobody joined.
	const WifiPhase p = phase.load();
	if (p != WifiPhase::OFF && p != WifiPhase::ONLINE && now - offlineSince > OFFLINE_TIMEOUT_MS) {
		bclog.logf(BCLogger::Log_Warn, TAG, "No WiFi connection for %u min - disabling it", (unsigned) (OFFLINE_TIMEOUT_MS / 60000));
		disableWifi(p == WifiPhase::ACCESS_POINT ? "Hotspot aus (kein Client)"
		            : everConnected ? "Verbindung verloren" : "kein WLAN gefunden");
	}

	// A successful OTA cannot restart from the upload handler -- that runs on async_tcp
	// and the client still has to receive the response. The flag is set there and acted
	// on here, one tick later, by which time the reply has gone out.
	if (otaRebootAt && millis() >= otaRebootAt) {
		bclog.log(BCLogger::Log_Info, TAG, "OTA complete - restarting");
		stats.persistNow();		// otherwise up to 5 min of statistics are lost (Statistics::PERSIST_INTERVAL_MS)
		ESP.restart();
	}
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
	// checkLoop() calls this on every WiFi (re)connect and when the access point starts.
	// AsyncServer::begin() is idempotent
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
			// Load shedding: every request in flight costs several KB of internal heap (TCP
			// buffers, request object, handler strings below CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL).
			// Measured 2026-09-26: 8 parallel requests took the free internal heap from ~49 KB
			// down to 1.2 KB -- the point where WiFi/BLE/SD allocations fail and the device
			// hangs or crashes. Below the threshold, refuse cheaply instead; the pages' pollers
			// simply try again on their next tick.
			if (internalFree < LOW_HEAP_REJECT_BYTES) {
				request->send(503, "text/plain", "Busy (low memory) - retry");
				return;
			}
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
	// Height calibration. The action routes come before the "/sensor/" page route (a plain
	// route also matches "<uri>/...", see the BMI160 note below). Plain GET like the other
	// actions, so the page's req() helper can call them.
	//   /sensor/cal?preset=N | gps=1 | height=<m> | qnh=<hPa>     N = 0..2
	//   /sensor/preset?i=N&height=<m>                              change a preset
	server.on("/sensor/cal", HTTP_GET, [](AsyncWebServerRequest *request) {
		I2CSensors::HeightCalResult r;
		if (request->hasArg("preset")) {
			const long i = request->arg("preset").toInt();
			r = (i >= 0 && i < I2CSensors::HEIGHT_PRESET_COUNT) ? sensors.calibrateHeight(sensors.getHeightPreset(i))
			                                                    : I2CSensors::HeightCalResult::OUT_OF_RANGE;
		} else if (request->hasArg("gps")) {
			r = sensors.calibrateHeightFromGps();
		} else if (request->hasArg("height")) {
			r = sensors.calibrateHeight(request->arg("height").toFloat());
		} else if (request->hasArg("qnh")) {
			r = sensors.setSeaLevelPressure(request->arg("qnh").toFloat());
		} else {
			request->send(400, "text/plain", "Missing parameter");
			return;
		}
		request->send(r == I2CSensors::HeightCalResult::OK ? 200 : 409, "text/plain", I2CSensors::heightCalResultText(r));
	});

	server.on("/sensor/preset", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (!request->hasArg("i") || !request->hasArg("height")) {
			request->send(400, "text/plain", "Missing parameter");
			return;
		}
		const I2CSensors::HeightCalResult r = sensors.setHeightPreset(request->arg("i").toInt(), request->arg("height").toFloat());
		request->send(r == I2CSensors::HeightCalResult::OK ? 200 : 409, "text/plain", I2CSensors::heightCalResultText(r));
	});

	// Old form target (bookmarks): calibrate to a known height
	server.on("/sensor/submit", HTTP_POST, [this](AsyncWebServerRequest *request) {
		const I2CSensors::HeightCalResult r = sensors.calibrateHeight(request->arg("height").toFloat());
		request->send(r == I2CSensors::HeightCalResult::OK ? 200 : 409, "text/plain", I2CSensors::heightCalResultText(r));
	});

	server.on("/sensor/", HTTP_GET,  [this](AsyncWebServerRequest *request) {
		htmlresponse.clear();
		sensors.getHTMLPage(htmlresponse);
		request->send(200, "text/html", htmlresponse.c_str());
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
	server.on("/debug/imu/ref", HTTP_GET, [](AsyncWebServerRequest *request) {
		bool start = request->hasParam("start") && request->getParam("start")->value() == "1";
		if (sensors.requestRefRide(start)) {
			request->send(200, "text/plain", start ? "Reference ride started" : "Reference ride cancelled");
		} else {
			request->send(409, "text/plain", "IMU not running");
		}
	});
	server.on("/debug/imu/raw", HTTP_GET, [](AsyncWebServerRequest *request) {
		long sec = request->hasParam("s") ? request->getParam("s")->value().toInt() : 60;
		if (sec < 0) sec = 0;
		if (sensors.requestRawCapture(sec)) {
			request->send(200, "text/plain", sec ? "Raw capture started" : "Raw capture stopped");
		} else {
			request->send(409, "text/plain", "IMU not running");
		}
	});
	server.on("/debug/imu/pitchreset", HTTP_GET, [](AsyncWebServerRequest *request) {
		sensors.requestPitchReset();
		request->send(200, "text/plain", "Gradient learning reset");
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


	CrashInfo::registerRoutes(server);

	// Debug menu: these pages exist but were reachable only by typing the URL.
	// Registered as "/debug/menu" because "/debug/" is the static live-log page.
	server.on("/debug/menu", HTTP_GET, [](AsyncWebServerRequest *request) {
		struct Entry { const char* href; const char* name; const char* desc; };
		static const Entry kEntries[] = {
			{ "/debug/",              "Live Log",        "Log stream over SSE, as it happens" },
			{ "/debug/nvs",           "NVS Contents",    "Every key stored in non-volatile storage" },
			{ "/debug/coredump",      "Core Dump",       "Last reset reason, where the last crash happened" },
#ifdef TRGBBC_SENSORS_I2C
			{ "/debug/imu",           "IMU (BMI160)",    "Accelerometer, calibration, road quality, shocks, gradient" },
#endif
			{ "/debug/climb",         "Climbs",          "Elevation profile, climb categories and their settings, demo" },
#ifdef BC_SIM
			{ "/debug/sim",           "Sensor Simulator","Fake speed, cadence and heart rate, GPX playback" },
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

	setupWifiRoutes();

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
	// The library sends text files as bare "text/plain", and without a charset Chrome falls
	// back to a legacy codepage -- the emojis in the debug log turn into mojibake. The
	// response exists after next() but isn't sent yet, so its type can still be changed.
	// Decided by extension: the response keeps its type private until it assembles the head.
	server.serveStatic("/log/", SD_MMC, "/BIKECOMP/").addMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
		next();
		AsyncWebServerResponse* response = request->getResponse();
		const String& url = request->url();
		if (response && response->code() < 300 && (url.endsWith(".log") || url.endsWith(".txt")))
			response->setContentType("text/plain; charset=utf-8");
	});
	server.serveStatic("/", LittleFS, "/site/").setCacheControl("max-age=31536000").setDefaultFile("index.html");
	//server.serveStatic("/core/", LittleFS, "/core/").setCacheControl("max-age=31536000");

	// URI not found
	server.onNotFound([this](AsyncWebServerRequest *request) {
		// Access point: the connectivity probes of Android/iOS/Windows (any host, any path) end
		// here, and the redirect is what makes the phone open the page and use this network.
		if (phase.load() == WifiPhase::ACCESS_POINT) {
			request->redirect(String("http://") + WiFi.softAPIP().toString() + "/wifi");
			return;
		}
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
		// Fill level: what a page erase (= one display flicker) frees depends on it, see
		// doc/PITFALLS.md. A key takes 1 entry, a string or blob 2 + size/32 (a float is a blob).
		nvs_stats_t nvsStats;
		// One page (126 entries) is the reserve for the garbage collection and never holds data.
		if (nvs_get_stats(NULL, &nvsStats) == ESP_OK && nvsStats.total_entries > 126) {
			const unsigned usable = nvsStats.total_entries - 126;
			char line[160];
			snprintf(line, sizeof(line), "<p>%u of %u usable entries hold data (%u %%), %u namespaces.</p>\n",
			         (unsigned) nvsStats.used_entries, usable, (unsigned) (nvsStats.used_entries * 100 / usable),
			         (unsigned) nvsStats.namespace_count);
			resp += line;
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
		resp += F(" keys</p>\n");
		WebPage::end(resp);
		request->send(200, "text/html", resp);
	});

}

#endif
