/*
 * SimSensors.h
 *
 *  Created on: 29.09.2026
 *
 * Fake speed, cadence and heart rate for testing the statistics without riding -- only in
 * the simulator build (-DBC_SIM, env trgb-esp32-s3-sim), never in the normal firmware.
 *
 * It does not set any statistics directly: once a second it builds the notifications a real
 * CSC speed sensor, CSC cadence sensor and heart-rate strap would send (cumulative wheel/crank
 * revolutions with the 1/1024 s time of the last revolution) and feeds them through
 * BLEDevices::notifyCallbackCSC(). So Distance (revolutions, lost distance, reconnects) and
 * Statistics (states, times, averages) run exactly the code they run on the bike. While the
 * simulator is active, notifications and disconnects of real speed/cadence/HR sensors are
 * ignored.
 *
 * Fed by TrailBridge: a test ride in the app (GPX playback) sends the values in its position
 * frames, flagged GPS_SIM_SENSORS (PROTOCOL.md "Sensorwerte und Simulationsmodus"); BLEDevices
 * passes them to feedFromTrailBridge(). It ends with the first frame without the flag, with
 * POSITION_NONE, a stale frame or the disconnect of the phone -- and only a simulation that
 * TrailBridge started: a manual "sim ..." is not switched off by the phone's real frames.
 *
 * Control:
 *   serial  sim <km/h> [<rpm>|-] [<bpm>|-]   set (starts the simulation), "-" = no such sensor
 *           sim stop                           sensors off (disconnect, like switching them off)
 *           sim [status]
 *   web     /debug/sim (page with GPX player), /debug/sim.json,
 *           /debug/sim/set?speed=&cad=&hr= (cad/hr empty or missing = no such sensor),
 *           /debug/sim/stop
 *   GPX     python3 -m bikelog sim ride.gpx --serial /dev/ttyACM0 | --http TRGB-BC.local
 *
 * The simulator build keeps its ride statistics in NVS namespaces of its own
 * (NVS_STAT_PREFIX, include/global_settings.h), so fake kilometres never reach the real
 * odometer. See doc/SIMULATOR.md.
 */

#pragma once

#ifdef BC_SIM

#include <Arduino.h>
#include <Ticker.h>
#include <freertos/FreeRTOS.h>

class SimSensors {
public:
	void setup();		// CLI command, web routes, 1 s ticker -- after stats and bleDevs

	// Starts or updates the simulation. cadence/hr < 0: that sensor is not simulated (if it
	// was, it disconnects).
	void set(float speedKmh, int16_t cadenceRpm, int16_t hr);
	// All simulated sensors disconnect. The revolution counters keep their value, as a
	// real sensor's do, so the next set() is a reconnect.
	void stop();

	// The phone's test ride (see above): simulated = the frame carries GPS_SIM_SENSORS.
	// Speed in km/h, cadence/hr/power < 0 = no such sensor, heightM NAN = no barometer.
	void feedFromTrailBridge(bool simulated, float speedKmh, int16_t cadenceRpm, int16_t hr, float heightM, int32_t powerW);
	// Switches the simulation off, if (and only if) TrailBridge started it.
	void endTrailBridgeFeed();

	bool isActive() const {return active;}

	// The simulated barometer height (m above sea level), if the simulation has one: replaces
	// I2CSensors::getHeight(), so the gradient comes out of the normal height/distance
	// calculation (Statistics::calculateGradient()). Only a TrailBridge test ride sets it.
	bool getHeight(float& heightM) const {
		heightM = simHeightM;
		return active && !isnan(simHeightM);
	}
	int32_t getPower() const {return simPowerW;}	// W, -1 = no power meter; nothing consumes it yet
	void getJson(String& out);

private:
	void tick();		// 1 s ticker: advance the revolutions, send the notifications
	void getPage(String& out);
	void handleCommand(const String& a, const String& b, const String& c);
	void printStatus();

	portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
	Ticker ticker;

	volatile bool active = false;	// requested by set()/stop()
	volatile bool fedByTrailBridge = false;	// the current simulation comes from the phone
	volatile float simHeightM = NAN;		// from the phone only, see getHeight()
	volatile int32_t simPowerW = -1;
	bool running = false;			// applied by tick() -- only tick() touches stats
	float speedKmh = 0;
	int16_t cadenceRpm = -1;
	int16_t hr = -1;
	bool cadenceSent = false, hrSent = false;	// a notification went out -> needs a disconnect when dropped

	uint32_t lastTickMs = 0;
	// Wheel and crank: position in revolutions (the whole part is the counter a sensor
	// reports, the fraction the revolution in progress) and the time of the last whole one in ms.
	// Start at 0 on every boot, like a sensor that restarts its counter.
	double wheelPos = 0, crankPos = 0;
	double wheelEventMs = 0, crankEventMs = 0;

	uint32_t updates = 0;		// set() calls, for the status
};

#endif	// BC_SIM
