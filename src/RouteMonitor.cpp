/*
 * RouteMonitor.cpp
 *
 * See RouteMonitor.h.
 */

#include "RouteMonitor.h"
#include "Singletons.h"
#include "Stats/Distance.h"
#include "ClockSync.h"
#include <ArduinoJson.h>
#include <memory>
#include <time.h>

using namespace RouteOv;

// Wheel distance since power-on. Only differences are used.
static float odoM() {
	const float d = stats.getDistHandler().getDistance();
	return isnan(d) ? 0 : d;
}

static int64_t wallClock() {
	return ClockSync::isValidNow() ? static_cast<int64_t>(time(nullptr)) : 0;
}

RouteMonitor::RouteMonitor() {
	mutex = xSemaphoreCreateMutex();
}

void RouteMonitor::setup() {
	Command routeCmd = console.addCmd("route", +[](cmd* c) {
		Command command(c);
		routeMon.handleCommand(command.getArgument(0).getValue(), command.getArgument(1).getValue());
	}, +[](uint8_t pos, SerialConsole::Matches& m) {
		if (pos == 1) {
			for (const char* a : {"status", "demo", "pos", "show", "hide"}) m.add(a);
		} else if (pos == 2 && m.word(1).equalsIgnoreCase("demo")) {
			m.add("off");
		}
	});
	routeCmd.addPositionalArgument("a", "status");
	routeCmd.addPositionalArgument("b", "");
	routeCmd.setDescription("Route overview (destination, waypoints, climbs): route [status] | route demo | route demo off | "
			"route pos <m from the start of the demo> | route show | route hide");

	AsyncWebServer& server = webserver.getServer();
	// Before "/debug/route.json" is irrelevant (different path), but keep the action route first as /debug/climb does
	server.on("/debug/route/demo", HTTP_GET, [this](AsyncWebServerRequest *request) {
		if (request->hasParam("off")) {
			stopDemo();
		} else if (request->hasParam("pos")) {
			if (!setDemoPos(request->getParam("pos")->value().toFloat())) {
				request->send(409, "text/plain", "no demo running");
				return;
			}
		} else {
			startDemo();
		}
		request->send(200, "text/plain", "OK");
	});
	server.on("/debug/route.json", HTTP_GET, [this](AsyncWebServerRequest *request) {
		String json;
		getJson(json);
		request->send(200, "application/json", json);
	});
}

// ******************** BLE side ********************

bool RouteMonitor::onNavFrame(bool hasRevision, uint8_t revision, bool hasRemaining, uint32_t remainingM, uint32_t remainingTimeS) {
	lock();
	if (demo) {
		unlock();
		return false;
	}
	tracker.onNavFrame(hasRevision, revision, hasRemaining, remainingM, remainingTimeS, wallClock());
	if (hasRemaining) {
		navRemainingM = remainingM;
		navOdoM = odoM();
	}
	uint8_t wanted;
	const bool read = tracker.readWanted(millis(), wanted);
	unlock();
	return read;
}

void RouteMonitor::onRouteGone() {
	lock();
	if (!demo) {
		tracker.onGone();
		loggedRevision = 0;
	}
	unlock();
}

void RouteMonitor::onOverviewFrame(const uint8_t* data, size_t length) {
	lock();
	if (demo) {
		unlock();
		return;
	}
	const FrameResult result = tracker.feedFrame(data, length);
	const Overview& ov = tracker.overview();
	const uint8_t revision = ov.revision, count = ov.count, waypoints = ov.waypointsAhead, climbs = ov.climbsTotal;
	const uint16_t dropped = ov.dropped;
	const bool destination = ov.hasDestination;
	unlock();

	switch (result) {
	case FRAME_HELLO:
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "🗺️ TrailBridge overview HELLO");
		break;
	case FRAME_NONE:
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "🗺️ No route overview");
		break;
	case FRAME_OVERVIEW:
		if (revision != loggedRevision) {
			loggedRevision = revision;
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "🗺️ Overview revision %u: %u entries (%u waypoints ahead, %u climbs in all)%s%s, %u byte",
					revision, count, waypoints, climbs, destination ? "" : ", no destination", dropped ? ", some dropped" : "", static_cast<unsigned>(length));
		}
		break;
	default:
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "🗺️ Overview frame not understood (%u byte, version %d, type 0x%02X)", static_cast<unsigned>(length),
				length > 0 ? data[0] : -1, length > 1 ? data[1] : 0);
	}
}

bool RouteMonitor::readWanted(uint8_t& revision) {
	lock();
	const bool read = !demo && tracker.readWanted(millis(), revision);
	unlock();
	return read;
}

void RouteMonitor::readAttempted() {
	lock();
	tracker.readAttempted(millis());
	unlock();
}

// ******************** Screen side ********************

float RouteMonitor::remainingNow() {
	float travelled = odoM() - navOdoM;
	if (travelled < 0) travelled = 0;		// counter rebased
	const float rem = navRemainingM - travelled;
	return rem > 0 ? rem : 0;
}

bool RouteMonitor::poll(View& view, uint32_t& version) {
	lock();
	if (demo) feedDemoNav();
	const bool ok = tracker.buildView(remainingNow(), view);
	version = tracker.version();
	unlock();
	return ok;
}

// ******************** Demo ********************

// A made-up 42 km route, as a frame TrailBridge could send it: five waypoints (one with a long name,
// one with umlauts) and three climbs. The speed behind the times is 20 km/h.
void RouteMonitor::startDemo() {
	struct Item {bool climb; uint32_t atM; const char* name; uint16_t gain; uint32_t length;};
	static const Item ITEMS[] = {
		{false, 40000, "Start Schotter", 0, 0},
		{true, 36500, nullptr, 85, 1900},
		{false, 32000, "B\xC3\xA4" "ckerei M\xC3\xBC" "ller", 0, 0},
		{true, 27000, nullptr, 210, 4800},
		{false, 21500, "Kreuzung Alte Landstra\xC3\x9F" "e Nord", 0, 0},
		{false, 17000, "Verpflegung km 25", 0, 0},
		{true, 11000, nullptr, 120, 2300},
		{false, 6000, "Biergarten", 0, 0},
	};
	demoStartM = 42000;

	uint8_t frame[320];
	size_t n = 0;
	auto put8 = [&](uint8_t v) {frame[n++] = v;};
	auto put32 = [&](uint32_t v) {for (uint8_t i = 0; i < 4; i++) put8((v >> (8 * i)) & 0xFF);};
	auto timeAt = [this](uint32_t atM) {return atM * 10 / demoSpeedMps10;};
	auto waypoint = [&](bool destination, uint32_t atM, const char* name) {
		const uint8_t len = strlen(name);
		put8(OVERVIEW_TAG_WAYPOINT);
		put8(OVERVIEW_WAYPOINT_FIXED_LEN + len);
		put8(destination ? OVERVIEW_WAYPOINT_FLAG_DESTINATION : 0);
		put32(atM);
		put32(timeAt(atM));
		memcpy(frame + n, name, len);
		n += len;
	};
	put8(OVERVIEW_PROTOCOL_VERSION);
	put8(OVERVIEW_MSG_OVERVIEW);
	put8(OVERVIEW_TAG_REVISION); put8(1); put8(1);
	put8(OVERVIEW_TAG_WAYPOINTS_AHEAD); put8(1); put8(5);
	put8(OVERVIEW_TAG_CLIMBS_TOTAL); put8(1); put8(3);
	waypoint(true, 0, "Ziel");
	uint8_t climbNumber = 0;
	for (const Item& it : ITEMS) {
		if (it.climb) {
			put8(OVERVIEW_TAG_CLIMB);
			put8(OVERVIEW_CLIMB_LEN);
			put8(++climbNumber);
			put8(it.gain & 0xFF); put8(it.gain >> 8);
			put32(it.length);
			put32(it.atM);
		} else {
			waypoint(false, it.atM, it.name);
		}
	}

	lock();
	demo = true;
	tracker.onGone();
	tracker.feedFrame(frame, n);
	navRemainingM = demoStartM;
	navOdoM = odoM();
	feedDemoNav();
	unlock();
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_CLI, "Route demo: %lu m, %u byte -- the rider moves with the wheel, or \"route pos <m>\"",
			static_cast<unsigned long>(demoStartM), static_cast<unsigned>(n));
}

void RouteMonitor::stopDemo() {
	lock();
	const bool was = demo;
	demo = false;
	if (was) tracker.onGone();
	unlock();
	if (was) bclog.log(BCLogger::Log_Info, BCLogger::TAG_CLI, "Route demo off");
}

bool RouteMonitor::setDemoPos(float travelledM) {
	lock();
	const bool ok = demo;
	if (ok) {
		navRemainingM = static_cast<float>(demoStartM) - travelledM;
		if (navRemainingM < 0) navRemainingM = 0;
		navOdoM = odoM();
		feedDemoNav();
	}
	unlock();
	return ok;
}

// The demo's own nav frame: remaining distance as it is now, time at the demo speed. Held revision = announced one.
void RouteMonitor::feedDemoNav() {
	const float rem = remainingNow();
	const uint32_t remM = static_cast<uint32_t>(rem + 0.5f);
	tracker.onNavFrame(true, 1, true, remM, remM * 10 / demoSpeedMps10, wallClock());
}

// ******************** Console, web ********************

void RouteMonitor::handleCommand(const String& a, const String& b) {
	if (a.equalsIgnoreCase("status")) {
		printStatus();
	} else if (a.equalsIgnoreCase("demo")) {
		if (b.equalsIgnoreCase("off")) {
			stopDemo();
			Serial.println("Route demo off");
		} else {
			startDemo();
			Serial.println("Route demo started (\"route pos <m>\" moves the rider, \"route show\" opens the screen, \"route demo off\" ends it)");
		}
	} else if (a.equalsIgnoreCase("pos")) {
		Serial.println(setDemoPos(b.toFloat()) ? "OK" : "No demo running (\"route demo\")");
	} else if (a.equalsIgnoreCase("show")) {
		ui.showRouteScreen();
	} else if (a.equalsIgnoreCase("hide")) {
		ui.hideRouteScreen();
	} else {
		Serial.println("Unknown: \"help route\" lists the commands");
	}
}

void RouteMonitor::printStatus() {
	std::unique_ptr<View> viewPtr(new View);		// 1.6 kB: neither on the console's stack nor permanent
	View& view = *viewPtr;
	uint32_t version;
	const bool ok = poll(view, version);
	lock();
	const bool isDemo = demo;
	const uint8_t held = tracker.revision(), announced = tracker.announcedRevision();
	const float rem = remainingNow();
	unlock();

	Serial.printf("Route overview%s: revision held %u, announced %u, rider %.0f m from the destination\r\n", isDemo ? " (DEMO)" : "", held, announced, rem);
	if (!ok) {
		Serial.println("  nothing to show (no overview or no nav frame)");
		return;
	}
	Serial.printf("  %u waypoints ahead, %u climbs in all%s\r\n", view.waypointsAhead, view.climbsTotal, view.shortened ? " (list shortened)" : "");
	const int64_t epoch = view.navEpoch;
	auto clock = [epoch](uint32_t timeS, char* out, size_t size) {
		if (epoch == 0) {
			snprintf(out, size, "--:--");
			return;
		}
		const time_t t = static_cast<time_t>(epoch + timeS);
		struct tm lt;
		localtime_r(&t, &lt);
		snprintf(out, size, "%02d:%02d", lt.tm_hour, lt.tm_min);
	};
	char eta[8];
	if (view.hasDestination) {
		clock(view.destination.timeS, eta, sizeof(eta));
		Serial.printf("  destination %-24s %7ld m  %6lu s  arrival %s\r\n", view.destination.name, static_cast<long>(view.destination.distM),
				static_cast<unsigned long>(view.destination.timeS), eta);
	}
	for (uint8_t i = 0; i < view.count; i++) {
		const Row& r = view.rows[i];
		if (r.kind == KIND_CLIMB) {
			Serial.printf("  climb %u/%u %-17s %7ld m to the foot, %u m up over %lu m\r\n", r.number, view.climbsTotal, r.distM <= 0 ? "(in it)" : "", static_cast<long>(r.distM),
					r.gainM, static_cast<unsigned long>(r.lengthM));
		} else {
			clock(r.timeS, eta, sizeof(eta));
			Serial.printf("  waypoint %-24s %7ld m  %6lu s  arrival %s\r\n", r.name, static_cast<long>(r.distM), static_cast<unsigned long>(r.timeS), eta);
		}
	}
}

void RouteMonitor::getJson(String& out) {
	std::unique_ptr<View> viewPtr(new View);		// 1.6 kB: not on the async web task's stack, not permanent
	View& view = *viewPtr;
	uint32_t version;
	const bool ok = poll(view, version);
	lock();
	const bool isDemo = demo;
	const uint8_t announced = tracker.announcedRevision();
	unlock();

	JsonDocument doc;
	doc["demo"] = isDemo;
	doc["valid"] = ok;
	doc["announced"] = announced;
	if (ok) {
		doc["revision"] = view.revision;
		doc["waypointsAhead"] = view.waypointsAhead;
		doc["climbsTotal"] = view.climbsTotal;
		doc["shortened"] = view.shortened;
		doc["navEpoch"] = view.navEpoch;
		if (view.hasDestination) {
			JsonObject d = doc["destination"].to<JsonObject>();
			d["name"] = view.destination.name;
			d["distM"] = view.destination.distM;
			d["timeS"] = view.destination.timeS;
		}
		JsonArray rows = doc["rows"].to<JsonArray>();
		for (uint8_t i = 0; i < view.count; i++) {
			const Row& r = view.rows[i];
			JsonObject o = rows.add<JsonObject>();
			o["kind"] = r.kind == KIND_CLIMB ? "climb" : "waypoint";
			o["distM"] = r.distM;
			if (r.kind == KIND_CLIMB) {
				o["number"] = r.number;
				o["gainM"] = r.gainM;
				o["lengthM"] = r.lengthM;
				o["summitM"] = r.summitM;
			} else {
				o["name"] = r.name;
				o["timeS"] = r.timeS;
			}
		}
	}
	serializeJson(doc, out);
}
