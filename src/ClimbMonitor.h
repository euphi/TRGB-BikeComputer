/*
 * ClimbMonitor.h
 *
 * The climb ahead, from TrailBridge's elevation-profile service: holds the Climb::Tracker
 * (ClimbProfile.h -- the algorithm and all settings are described there), feeds it from the
 * BLE task and hands its result to the climb screen (UIFacade, once a second).
 *
 * Position: the rider's place in the profile is the REMAINING_DISTANCE_M of the nav frames
 * (PROTOCOL.md "Höhenprofil-Service"). Between two nav frames it is carried on with the
 * wheel sensor's distance, as the distance to the next maneuver is.
 *
 * Settings: Climb::Config, each one by name -- stored in NVS (namespace "Climb", only those
 * that were changed), set with
 *   serial  climb [status]             status and all settings
 *           climb <name> [<value>]     show/set one, e.g. "climb cat4 9000"
 *           climb defaults             back to the built-in values
 *   web     /debug/climb, /debug/climb.json, /debug/climb/set?<name>=<value>
 *
 * Demo, to see the screen without a phone or a hill ("climb demo ..." or the web page): a
 * made-up profile goes through the normal frame parser; the rider moves with the wheel
 * sensor (or "climb pos <m>"). Frames from the phone are ignored meanwhile.
 *   climb demo [<length m> [<gradient %>]] | climb demo off | climb pos <m from its start>
 *   climb show | climb hide            open/close the climb screen
 */

#pragma once

#include <Arduino.h>
#include "freertos/semphr.h"
#include "ClimbProfile.h"

class ClimbMonitor {
public:
	ClimbMonitor();
	void setup();		// settings from NVS, console command, web routes

	// --- From the BLE task (TrailBridge) ---
	void onProfileFrame(const uint8_t* data, size_t length);
	void onNavRemaining(uint32_t remainingM);		// REMAINING_DISTANCE_M of a NAV_UPDATE
	void onRouteGone();								// NAV_NONE, or the phone disconnected

	// --- For the screen ---
	// Moves the rider on by the wheel distance and returns the state. version changes when
	// the profile points did (then fetch them with copyProfile()).
	void poll(Climb::Status& status, Climb::Config& config, uint32_t& version);
	// The joined profile: altitudes in dm, startRemainingM belongs to the first one.
	// Returns the number of points (0: none to show).
	uint16_t copyProfile(int16_t* altDm, uint16_t maxPoints, uint8_t& stepM);

	void printStatus();
	void getJson(String& out);
	void getPage(String& out);

private:
	SemaphoreHandle_t mutex;
	Climb::Tracker tracker;

	// Last nav frame: remaining route distance and the wheel distance at that moment
	bool navValid = false;
	float navRemainingM = 0;
	float navOdoM = 0;

	bool demo = false;
	uint16_t loggedClimbId = 0;
	uint8_t loggedRank = 0;

	void lock() {xSemaphoreTake(mutex, portMAX_DELAY);}
	void unlock() {xSemaphoreGive(mutex);}
	void advance();								// mutex held: rider position from navRemainingM and the wheel
	void logClimb(const Climb::Status& st);		// outside the mutex

	void loadConfig();
	bool setParam(const String& name, float value, String& message);
	void resetConfig();
	void startDemo(float lengthM, float gradePct);
	void stopDemo();
	bool setDemoPos(float posM);
	void handleCommand(const String& a, const String& b, const String& c);
};
