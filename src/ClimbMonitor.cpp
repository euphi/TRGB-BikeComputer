/*
 * ClimbMonitor.cpp
 *
 * See ClimbMonitor.h.
 */

#include "ClimbMonitor.h"
#include "Singletons.h"
#include "Stats/Distance.h"
#include "BikeProfileProtocol.h"
#include "WebPage.h"
#include <ArduinoJson.h>
#include <Preferences.h>

static const char* const NVS_NAMESPACE = "Climb";

// Wheel distance since power-on. Only differences are used.
static float odoM() {
	const float d = stats.getDistHandler().getDistance();
	return isnan(d) ? 0 : d;
}

ClimbMonitor::ClimbMonitor() {
	mutex = xSemaphoreCreateMutex();
}

void ClimbMonitor::setup() {
	loadConfig();

	Command climbCmd = console.addCmd("climb", +[](cmd* c) {
		Command command(c);
		climb.handleCommand(command.getArgument(0).getValue(), command.getArgument(1).getValue(), command.getArgument(2).getValue());
	}, +[](uint8_t pos, SerialConsole::Matches& m) {
		if (pos == 1) {
			for (const char* a : {"status", "defaults", "demo", "pos", "show", "hide"}) m.add(a);
			for (uint8_t i = 0; i < Climb::paramCount(); i++) m.add(Climb::paramInfo(i).name);
		} else if (pos == 2 && m.word(1).equalsIgnoreCase("demo")) {
			m.add("off");
		}
	});
	climbCmd.addPositionalArgument("a", "status");
	climbCmd.addPositionalArgument("b", "");
	climbCmd.addPositionalArgument("c", "");
	climbCmd.setDescription("Climbs (elevation profile): climb [status] | climb <setting> [<value>] | climb defaults | "
			"climb demo [<length m> [<gradient %>]] | climb demo off | climb pos <m> | climb show | climb hide");

	// Action routes before "/debug/climb": a plain route also matches "<uri>/..." (see /debug/imu)
	AsyncWebServer& server = webserver.getServer();
	server.on("/debug/climb/set", HTTP_GET, [this](AsyncWebServerRequest *request) {
		String message;
		bool ok = request->params() > 0;
		for (size_t i = 0; ok && i < request->params(); i++) {
			const AsyncWebParameter* p = request->getParam(i);
			ok = setParam(p->name(), p->value().toFloat(), message);
		}
		request->send(ok ? 200 : 400, "text/plain", ok ? "OK" : message);
	});
	server.on("/debug/climb/defaults", HTTP_GET, [this](AsyncWebServerRequest *request) {
		resetConfig();
		request->send(200, "text/plain", "OK");
	});
	server.on("/debug/climb/demo", HTTP_GET, [this](AsyncWebServerRequest *request) {
		auto value = [request](const char* name, float dflt) -> float {
			return request->hasParam(name) ? request->getParam(name)->value().toFloat() : dflt;
		};
		if (request->hasParam("off")) {
			stopDemo();
		} else if (request->hasParam("pos")) {
			if (!setDemoPos(value("pos", 0))) {
				request->send(409, "text/plain", "no demo running");
				return;
			}
		} else {
			startDemo(value("len", 2000), value("grade", 6));
		}
		request->send(200, "text/plain", "OK");
	});
	server.on("/debug/climb.json", HTTP_GET, [this](AsyncWebServerRequest *request) {
		String json;
		getJson(json);
		request->send(200, "application/json", json);
	});
	server.on("/debug/climb", HTTP_GET, [this](AsyncWebServerRequest *request) {
		String html;
		getPage(html);
		request->send(200, "text/html", html);
	});
}

// ******************** BLE side ********************

void ClimbMonitor::onProfileFrame(const uint8_t* data, size_t length) {
	lock();
	if (demo) {
		unlock();
		return;
	}
	const Climb::Tracker::FrameResult result = tracker.feedFrame(data, length);
	if (result == Climb::Tracker::FRAME_PROFILE) advance();
	const Climb::Status st = tracker.status();
	const uint16_t points = tracker.pointCount();
	const uint32_t startRem = tracker.startRemainingM();
	unlock();

	switch (result) {
	case Climb::Tracker::FRAME_HELLO:
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "⛰️ TrailBridge profile HELLO");
		break;
	case Climb::Tracker::FRAME_NONE:
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "⛰️ No profile (climb over, route ended or left)");
		break;
	case Climb::Tracker::FRAME_PROFILE:
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "⛰️ Profile: %u points from %lu m remaining (%u byte)", points, static_cast<unsigned long>(startRem), length);
		logClimb(st);
		break;
	default:
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "⛰️ Profile frame not understood (%u byte, version %d, type 0x%02X)", length,
				length > 0 ? data[0] : -1, length > 1 ? data[1] : 0);
	}
}

void ClimbMonitor::onNavRemaining(uint32_t remainingM) {
	lock();
	if (!demo) {
		navRemainingM = remainingM;
		navOdoM = odoM();
		navValid = true;
		tracker.setRemaining(navRemainingM);
	}
	const Climb::Status st = tracker.status();
	unlock();
	logClimb(st);		// the next climb comes up as the rider moves on
}

void ClimbMonitor::onRouteGone() {
	lock();
	if (!demo) {
		navValid = false;
		tracker.reset();
	}
	unlock();
}

// ******************** Screen side ********************

void ClimbMonitor::advance() {
	if (!navValid) return;
	float travelled = odoM() - navOdoM;
	if (travelled < 0) travelled = 0;		// counter rebased
	const float rem = navRemainingM - travelled;
	tracker.setRemaining(rem > 0 ? rem : 0);
}

void ClimbMonitor::poll(Climb::Status& status, Climb::Config& config, uint32_t& version) {
	lock();
	advance();
	status = tracker.status();
	config = tracker.cfg;
	version = tracker.version();
	unlock();
	// No logClimb() here: this is the UI task, with too little stack for a float log line
}

uint16_t ClimbMonitor::copyProfile(int16_t* altDm, uint16_t maxPoints, uint8_t& stepM) {
	lock();
	uint16_t n = tracker.pointCount();
	if (n > maxPoints) n = maxPoints;
	memcpy(altDm, tracker.altitudesDm(), n * sizeof(altDm[0]));
	stepM = tracker.stepM();
	unlock();
	return n;
}

void ClimbMonitor::logClimb(const Climb::Status& st) {
	if (!st.active || (st.climbId == loggedClimbId && st.rank == loggedRank)) return;
	loggedClimbId = st.climbId;
	loggedRank = st.rank;
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "⛰️ Climb #%u: %s%s, %.0f m long, %.0f m up (%.1f %%)%s, foot in %.0f m",
			st.climbId, st.rank ? "category " : "not rated", Climb::rankLabel(st.rank), st.lengthM, st.totalAscentM, st.avgGradePct,
			st.summitOpen ? ", summit not in the profile yet" : "", st.toFootM);
}

// ******************** Settings ********************

void ClimbMonitor::loadConfig() {
	Climb::Config cfg;
	Preferences p;
	if (p.begin(NVS_NAMESPACE, true)) {
		for (uint8_t i = 0; i < Climb::paramCount(); i++) {
			const char* name = Climb::paramInfo(i).name;
			// isKey() probes quietly; getFloat() logs an error for a missing key
			if (p.isKey(name)) Climb::paramSet(cfg, i, p.getFloat(name, NAN));
		}
		p.end();
	}
	lock();
	tracker.cfg = cfg;
	unlock();
}

bool ClimbMonitor::setParam(const String& name, float value, String& message) {
	const int8_t index = Climb::paramFind(name.c_str());
	if (index < 0) {
		message = "Unknown setting: " + name;
		return false;
	}
	const Climb::ParamInfo& info = Climb::paramInfo(index);
	lock();
	const bool ok = Climb::paramSet(tracker.cfg, index, value);
	if (ok) value = Climb::paramGet(tracker.cfg, index);		// as stored (rounded)
	unlock();
	if (!ok) {
		message = String(info.name) + ": " + String(info.min, 1) + " .. " + String(info.max, 1) + " " + info.unit;
		return false;
	}
	Preferences p;
	p.begin(NVS_NAMESPACE, false);
	p.putFloat(info.name, value);
	p.end();
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_CLI, "Climb setting %s = %g %s", info.name, value, info.unit);
	message = String(info.name) + " = " + String(value, 2) + " " + info.unit;
	return true;
}

void ClimbMonitor::resetConfig() {
	Preferences p;
	p.begin(NVS_NAMESPACE, false);
	p.clear();
	p.end();
	lock();
	tracker.cfg = Climb::Config();
	unlock();
	bclog.log(BCLogger::Log_Info, BCLogger::TAG_CLI, "Climb settings back to defaults");
}

// ******************** Demo ********************

void ClimbMonitor::startDemo(float lengthM, float gradePct) {
	static const uint8_t STEP = 25;
	static const uint16_t LEAD_STEPS = 8, TAIL_STEPS = 20;		// 200 m flat before, 300 m down + 200 m flat behind
	lengthM = constrain(lengthM, 200.0f, 4250.0f);				// LEAD + climb + TAIL <= PROFILE_MAX_DELTAS
	gradePct = constrain(gradePct, 1.0f, 20.0f);
	const uint16_t climbSteps = static_cast<uint16_t>(lengthM / STEP);
	const uint16_t n = LEAD_STEPS + climbSteps + TAIL_STEPS;
	const uint32_t startRem = static_cast<uint32_t>(n) * STEP + 1000;	// the "route" goes on for another km

	// The wire frame, as TrailBridge would send it
	uint8_t frame[17 + PROFILE_MAX_DELTAS];
	size_t len = 0;
	frame[len++] = PROFILE_PROTOCOL_VERSION;
	frame[len++] = PROFILE_MSG_PROFILE_UPDATE;
	frame[len++] = PROFILE_TAG_START_REMAINING_DISTANCE_M;
	frame[len++] = 4;
	for (uint8_t i = 0; i < 4; i++) frame[len++] = (startRem >> (8 * i)) & 0xFF;
	frame[len++] = PROFILE_TAG_STEP_M;
	frame[len++] = 1;
	frame[len++] = STEP;
	const int16_t baseDm = 2400;
	frame[len++] = PROFILE_TAG_BASE_ALT_DM;
	frame[len++] = 2;
	frame[len++] = baseDm & 0xFF;
	frame[len++] = baseDm >> 8;
	frame[len++] = PROFILE_TAG_DELTAS_DM;
	frame[len++] = static_cast<uint8_t>(n);
	float altDm = baseDm;
	int32_t sentDm = baseDm;
	for (uint16_t k = 0; k < n; k++) {
		float g = 0;
		if (k >= LEAD_STEPS && k < LEAD_STEPS + climbSteps) {
			// Uneven like a real road: two waves around the mean, never downhill
			const float x = static_cast<float>(k - LEAD_STEPS) / climbSteps;
			g = gradePct * (1.0f + 0.45f * sinf(x * 9.4f) + 0.25f * sinf(x * 23.0f + 1.0f));
			if (g < 0.5f) g = 0.5f;
		} else if (k >= LEAD_STEPS + climbSteps && k < LEAD_STEPS + climbSteps + 12) {
			g = -5.0f;
		}
		altDm += g * STEP / 10.0f;			// % of 25 m, in dm
		const int32_t d = constrain(static_cast<int32_t>(lroundf(altDm)) - sentDm, -127, 127);
		frame[len++] = static_cast<uint8_t>(static_cast<int8_t>(d));
		sentDm += d;
	}

	lock();
	demo = true;
	tracker.reset();
	tracker.feedFrame(frame, len);
	navValid = true;
	navRemainingM = startRem;
	navOdoM = odoM();
	tracker.setRemaining(navRemainingM);
	unlock();
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_CLI, "Climb demo: %.0f m at %.1f %% -- the rider moves with the wheel, or \"climb pos <m>\"", lengthM, gradePct);
}

void ClimbMonitor::stopDemo() {
	lock();
	const bool was = demo;
	demo = false;
	if (was) {
		navValid = false;
		tracker.reset();
	}
	unlock();
	if (was) bclog.log(BCLogger::Log_Info, BCLogger::TAG_CLI, "Climb demo off");
}

bool ClimbMonitor::setDemoPos(float posM) {
	lock();
	const bool ok = demo;
	if (ok) {
		navRemainingM = static_cast<float>(tracker.startRemainingM()) - posM;
		navOdoM = odoM();
		tracker.setRemaining(navRemainingM);
	}
	unlock();
	return ok;
}

// ******************** Console, web ********************

void ClimbMonitor::handleCommand(const String& a, const String& b, const String& c) {
	if (a.equalsIgnoreCase("status")) {
		printStatus();
	} else if (a.equalsIgnoreCase("defaults")) {
		resetConfig();
		Serial.println("Climb settings back to defaults");
	} else if (a.equalsIgnoreCase("demo")) {
		if (b.equalsIgnoreCase("off")) {
			stopDemo();
			Serial.println("Climb demo off");
		} else {
			startDemo(b.isEmpty() ? 2000 : b.toFloat(), c.isEmpty() ? 6 : c.toFloat());
			Serial.println("Climb demo started (\"climb pos <m>\" moves the rider, \"climb demo off\" ends it)");
		}
	} else if (a.equalsIgnoreCase("pos")) {
		Serial.println(setDemoPos(b.toFloat()) ? "OK" : "No demo running (\"climb demo\")");
	} else if (a.equalsIgnoreCase("show")) {
		ui.showClimbScreen();
	} else if (a.equalsIgnoreCase("hide")) {
		ui.hideClimbScreen();
	} else if (b.isEmpty()) {
		const int8_t index = Climb::paramFind(a.c_str());
		if (index < 0) {
			Serial.println("Unknown: \"climb status\" lists the settings, \"help climb\" the commands");
			return;
		}
		const Climb::ParamInfo& info = Climb::paramInfo(index);
		lock();
		const float v = Climb::paramGet(tracker.cfg, index);
		unlock();
		Serial.printf("%s = %g %s (%g .. %g) -- %s\r\n", info.name, v, info.unit, info.min, info.max, info.help);
	} else {
		String message;
		setParam(a, b.toFloat(), message);
		Serial.println(message);
	}
}

void ClimbMonitor::printStatus() {
	Climb::Status st;
	Climb::Config cfg;
	uint32_t version;
	poll(st, cfg, version);
	lock();
	const uint16_t points = tracker.pointCount();
	const uint32_t startRem = tracker.startRemainingM();
	const bool nav = navValid, isDemo = demo;
	const float rem = navRemainingM;
	unlock();

	Serial.printf("Climb%s: profile %u points from %lu m remaining, nav frame %s (%.0f m remaining)\r\n", isDemo ? " (DEMO)" : "",
			points, static_cast<unsigned long>(startRem), nav ? "yes" : "none", rem);
	if (!st.positionValid) {
		Serial.println("  rider not in a profile");
	} else if (!st.active) {
		Serial.printf("  rider at %.0f m, %.1f m altitude, no climb ahead\r\n", st.posM, st.altM);
	} else {
		Serial.printf("  climb #%u %s%s: %.0f m long, %.0f m up, mean %.1f %%%s\r\n", st.climbId, st.rank ? "category " : "not rated",
				Climb::rankLabel(st.rank), st.lengthM, st.totalAscentM, st.avgGradePct, st.summitOpen ? " (summit not in the profile yet)" : "");
		Serial.printf("  rider at %.0f m, %.1f m altitude: foot in %.0f m, summit in %.0f m, %.0f m up to go, next %u m at %.1f %%\r\n",
				st.posM, st.altM, st.toFootM, st.toSummitM, st.remainingAscentM, cfg.lookAheadM, st.aheadGradePct);
	}
	Serial.println("Settings (climb <name> <value>):");
	for (uint8_t i = 0; i < Climb::paramCount(); i++) {
		const Climb::ParamInfo& info = Climb::paramInfo(i);
		Serial.printf("  %-12s %8g %-4s %s\r\n", info.name, Climb::paramGet(cfg, i), info.unit, info.help);
	}
}

void ClimbMonitor::getJson(String& out) {
	Climb::Status st;
	Climb::Config cfg;
	uint32_t version;
	poll(st, cfg, version);
	lock();
	const uint16_t points = tracker.pointCount();
	const uint32_t startRem = tracker.startRemainingM();
	const bool isDemo = demo;
	unlock();

	JsonDocument doc;
	doc["demo"] = isDemo;
	doc["points"] = points;
	doc["startRemaining"] = startRem;
	doc["positionValid"] = st.positionValid;
	doc["active"] = st.active;
	doc["onClimb"] = st.onClimb;
	doc["summitOpen"] = st.summitOpen;
	doc["id"] = st.climbId;
	doc["rank"] = st.rank;
	doc["category"] = Climb::rankLabel(st.rank);
	doc["pos"] = st.posM;
	doc["alt"] = st.altM;				// NAN: serialised as null
	doc["length"] = st.lengthM;
	doc["ascent"] = st.totalAscentM;
	doc["remainingAscent"] = st.remainingAscentM;
	doc["toFoot"] = st.toFootM;
	doc["toSummit"] = st.toSummitM;
	doc["avgGrade"] = st.avgGradePct;
	doc["aheadGrade"] = st.aheadGradePct;
	JsonObject settings = doc["settings"].to<JsonObject>();
	for (uint8_t i = 0; i < Climb::paramCount(); i++) settings[Climb::paramInfo(i).name] = Climb::paramGet(cfg, i);
	serializeJson(doc, out);
}

void ClimbMonitor::getPage(String& out) {
	lock();
	const Climb::Config cfg = tracker.cfg;
	unlock();

	WebPage::begin(out, "Climbs", "input[type=number]{width:7em}td.n{text-align:right}");
	out += F("<p class=\"eyebrow\">Elevation profile of TrailBridge's GPX route: which rises count as a climb, how they are "
	         "rated (score = length in m x mean gradient in % = 100 x height gain), and when the climb screen shows up. "
	         "Changes apply at once and are stored.</p>\n"
	         "<h3>Status</h3>\n<pre id=\"st\" class=\"eyebrow\"></pre>\n"
	         "<div class=\"row\">"
	         "<label>Demo: m <input type=\"number\" id=\"dl\" value=\"2000\" min=\"200\" max=\"4250\" step=\"100\"></label>"
	         "<label>% <input type=\"number\" id=\"dg\" value=\"6\" min=\"1\" max=\"20\" step=\"0.5\"></label>"
	         "<a class=\"btn\" href=\"#\" onclick=\"req('/debug/climb/demo?len='+$('dl').value+'&grade='+$('dg').value,'Demo started');return false;\">Start</a>"
	         "<label>rider at m <input type=\"number\" id=\"dp\" value=\"300\" step=\"50\"></label>"
	         "<a class=\"btn btn-ghost\" href=\"#\" onclick=\"req('/debug/climb/demo?pos='+$('dp').value,'Moved');return false;\">Move</a>"
	         "<a class=\"btn btn-ghost\" href=\"#\" onclick=\"req('/debug/climb/demo?off=1','Demo off');return false;\">Off</a>"
	         "</div>\n"
	         "<h3>Settings</h3>\n<table><thead><tr><th>Name</th><th>Value</th><th></th><th>Meaning</th></tr></thead><tbody>\n");
	for (uint8_t i = 0; i < Climb::paramCount(); i++) {
		const Climb::ParamInfo& info = Climb::paramInfo(i);
		out += F("<tr><td>");
		out += info.name;
		out += F("</td><td><input type=\"number\" step=\"any\" min=\"");
		out += String(info.min, 1);
		out += F("\" max=\"");
		out += String(info.max, 1);
		out += F("\" value=\"");
		out += String(Climb::paramGet(cfg, i), 2);
		out += F("\" onchange=\"req('/debug/climb/set?");
		out += info.name;
		out += F("='+this.value,'Stored')\"></td><td class=\"eyebrow\">");
		out += info.unit;
		out += F("</td><td class=\"eyebrow\">");
		out += info.help;
		out += F("</td></tr>\n");
	}
	out += F("</tbody></table>\n<div class=\"row\" style=\"margin-top:8px;\">"
	         "<a class=\"btn btn-ghost\" href=\"#\" onclick=\"if(confirm('All climb settings back to the defaults?'))"
	         "req('/debug/climb/defaults','Defaults restored',()=>location.reload());return false;\">Defaults</a></div>\n");
	WebPage::end(out,
		"const $=i=>document.getElementById(i);\n"
		"async function poll(){try{const j=await (await fetch('/debug/climb.json')).json();delete j.settings;"
		"$('st').textContent=Object.entries(j).map(([k,v])=>k+': '+(typeof v=='number'?Math.round(v*10)/10:v)).join('\\n');}catch(e){}}\n"
		"poll();setInterval(poll,2000);\n");
}
