/*
 * RouteMonitor.h
 *
 * The route overview from TrailBridge's fourth service (destination, waypoints and climbs of the
 * GPX route still ahead, RouteOverview.h): holds the RouteOv::Tracker, feeds it from the BLE
 * side and hands a ready-made list to the route screen (UIFacade, once a second).
 *
 * BLE side (BLEDevices.cpp): every nav frame passes its OVERVIEW_REVISION and remaining
 * distance/time on (onNavFrame()); when that says the overview must be read, a task of its own
 * reads the characteristic (readWanted()/onOverviewFrame()) -- never the indicate callback, a
 * long read blocks.
 *
 * Position: the nav frame's REMAINING_DISTANCE_M, carried on between two frames with the wheel
 * sensor's distance, as ClimbMonitor does it.
 *
 * Debugging, to see the screen without a phone: a made-up overview goes through the normal frame
 * parser; the rider moves with the wheel sensor (or "route pos <m>"). Frames from the phone are
 * ignored meanwhile.
 *   serial  route [status]                 state and the list as the screen gets it
 *           route demo | route demo off    made-up route / back to the phone's
 *           route pos <m>                  rider <m> metres from the start of the demo route
 *           route show | route hide        open/close the route screen
 *   web     /debug/route.json, /debug/route/demo[?off=1|?pos=<m>]
 */

#pragma once

#include <Arduino.h>
#include "freertos/semphr.h"
#include "RouteOverview.h"

class RouteMonitor {
public:
	RouteMonitor();
	void setup();		// console command, web routes

	// --- From the BLE task (TrailBridge) ---
	// A nav frame. Returns true if the overview has to be read now (revision announced that is not
	// the one held).
	bool onNavFrame(bool hasRevision, uint8_t revision, bool hasRemaining, uint32_t remainingM, uint32_t remainingTimeS);
	void onRouteGone();		// NAV_NONE, or the phone disconnected
	void onOverviewFrame(const uint8_t* data, size_t length);

	// --- From the reader task ---
	bool readWanted(uint8_t& revision);		// also true again RouteOv::Tracker::RETRY_MS after a try that did not help
	void readAttempted();

	// --- For the screen ---
	// Distances carried on by the wheel, times of the last nav frame. version changes when the
	// overview does (then the list is built anew). Returns false if there is nothing to show.
	bool poll(RouteOv::View& view, uint32_t& version);

	void printStatus();
	void getJson(String& out);

private:
	SemaphoreHandle_t mutex;
	RouteOv::Tracker tracker;
	uint8_t loggedRevision = 0;

	// Last nav frame: remaining route distance and the wheel distance at that moment
	float navRemainingM = 0;
	float navOdoM = 0;

	bool demo = false;
	uint32_t demoStartM = 0;			// REMAINING_AT_M of the demo route's start
	uint32_t demoSpeedMps10 = 55;		// 5.5 m/s = 20 km/h, in 0.1 m/s

	void lock() {xSemaphoreTake(mutex, portMAX_DELAY);}
	void unlock() {xSemaphoreGive(mutex);}
	float remainingNow();				// mutex held: nav frame's distance less what the wheel did since
	void startDemo();
	void stopDemo();
	bool setDemoPos(float travelledM);
	void feedDemoNav();					// mutex held
	void handleCommand(const String& a, const String& b);
};
