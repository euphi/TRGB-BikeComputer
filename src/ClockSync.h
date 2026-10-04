/*
 * ClockSync.h
 *
 * The system clock and who sets it. Sources: NTP (configTzTime() in WifiWebserver, needs
 * WLAN) and the phone's GPS time (TrailBridge, tag UTC_TIME_MS, see BikeGpsProtocol.h).
 * Without either the clock counts up from 1970 after a hard reset.
 *
 * Step detection: ESP-IDF derives the system time from esp_timer plus a boot offset, so
 * "system time - esp_timer_get_time()" is constant until someone sets the clock. A change
 * of that difference is a step -- whoever made it. BCLogger polls for steps from its
 * FlusherTask and writes them into the session's time-hint file (SessionStats.h), from which
 * the timestamps written before the clock was set get corrected after the next boot.
 */

#pragma once

#include <cstdint>

namespace ClockSync {

enum Source : uint8_t {SRC_UNKNOWN = 0, SRC_NTP, SRC_GPS};
const char* sourceName(Source s);

// Registers the SNTP notification (only to label NTP steps).
void setup();

// Remembers the current boot offset as the reference for pollStep(). BCLogger calls this
// at session start, in the same moment it writes the session's start line.
void baseline();

// GPS time from a POSITION_UPDATE: utcMs = UTC_TIME_MS + FIX_AGE_MS, i.e. "now" at the
// phone. Called from the BLE task; cheap if nothing is due.
//
// The phone's GPS time is not reliable right after its GNSS chip was switched on: on the
// test ride of 2026-10-04 it was 14.8 h behind (the chip's own clock, stuck at the last
// time it ran) for an hour, earlier rides showed 18..80 min that NTP then took back. So:
//   - no clock yet (1970): take the GPS time, but as provisional -- rechecked every
//     GPS_RECHECK_PROVISIONAL_MS for GPS_PROVISIONAL_MS, and then any size of correction
//     is accepted (the clock being replaced is the guess of a guess);
//   - a clock that is valid (kept by the RTC over deep sleep, set by NTP, or set by GPS more
//     than GPS_PROVISIONAL_MS ago): corrected by
//     at most GPS_MAX_CORRECTION_MS, a larger difference is ignored (and logged). The RTC
//     of the ESP32 drifts by seconds to minutes over a night, never by hours;
//   - both: only if off by more than GPS_MIN_CORRECTION_MS (BLE latency makes the GPS
//     time worse than NTP), otherwise at most once an hour.
// NTP always wins and ends the provisional state.
void offerGpsTime(int64_t utcMs);
static constexpr int64_t GPS_MIN_CORRECTION_MS = 2000;
static constexpr int64_t GPS_MAX_CORRECTION_MS = 10LL * 60 * 1000;
static constexpr uint32_t GPS_RECHECK_MS = 3600UL * 1000UL;
static constexpr uint32_t GPS_RECHECK_PROVISIONAL_MS = 5UL * 60UL * 1000UL;
static constexpr uint32_t GPS_PROVISIONAL_MS = 30UL * 60UL * 1000UL;

struct Step {
	int64_t offsetMs;		// clock moved by this
	int64_t newEpochMs;		// system time right after the step
	uint32_t uptimeMs;
	Source source;
};
// True if the clock was stepped by at least STEP_MIN_MS since baseline() / the last step.
// Only one caller (FlusherTask).
bool pollStep(Step& out);
static constexpr int64_t STEP_MIN_MS = 1000;

bool isValidNow();

// Serial CLI "clock": shows the time; "clock ntp" asks NTP right away (it otherwise only
// resyncs every 3 h); "clock unset" puts the clock back to 1970 + uptime,
// as after a power loss -- for testing the GPS/NTP setting and the log correction without
// pulling the battery.
void registerCli();

}	// namespace ClockSync
