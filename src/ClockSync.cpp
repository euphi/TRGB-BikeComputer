/*
 * ClockSync.cpp
 *
 * See ClockSync.h.
 */

#include "ClockSync.h"
#include "SessionStats.h"
#include "Singletons.h"

#include <atomic>
#include <cstdlib>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <sys/time.h>

namespace ClockSync {

namespace {

// Who set the clock last, for labelling the step pollStep() sees afterwards.
std::atomic<uint8_t> pendingSource{SRC_UNKNOWN};

// FlusherTask only (baseline() runs in setup() before any poll)
bool haveBaseline = false;
int64_t bootOffsetUs = 0;

// BLE task only
bool gpsChecked = false;
uint32_t gpsCheckedAt = 0;

int64_t nowUs() {
	struct timeval tv;
	gettimeofday(&tv, nullptr);
	return static_cast<int64_t>(tv.tv_sec) * 1000000 + tv.tv_usec;
}

int64_t currentBootOffsetUs() {
	return nowUs() - esp_timer_get_time();
}

}	// namespace

const char* sourceName(Source s) {
	switch (s) {
	case SRC_NTP: return "ntp";
	case SRC_GPS: return "gps";
	default:      return "?";
	}
}

void setup() {
	// Runs in the lwIP task after SNTP has set the clock -- only a flag here.
	sntp_set_time_sync_notification_cb([](struct timeval*) {pendingSource = SRC_NTP;});
}

void baseline() {
	bootOffsetUs = currentBootOffsetUs();
	haveBaseline = true;
}

bool isValidNow() {
	return SessStats::isValidMs(nowUs() / 1000);
}

void offerGpsTime(int64_t utcMs) {
	const uint32_t now = millis();
	const bool valid = isValidNow();
	if (gpsChecked && valid && now - gpsCheckedAt < GPS_RECHECK_MS) return;
	if (!SessStats::isValidMs(utcMs)) return;		// phone without a real time yet
	gpsChecked = true;
	gpsCheckedAt = now;

	const int64_t sysMs = nowUs() / 1000;
	const int64_t diff = utcMs - sysMs;
	if (valid && llabs(diff) < GPS_MIN_CORRECTION_MS) {
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_OP, "🕒 GPS time check: clock off by %lld ms - kept", (long long)diff);
		return;
	}
	struct timeval tv;
	tv.tv_sec = static_cast<time_t>(utcMs / 1000);
	tv.tv_usec = static_cast<suseconds_t>((utcMs % 1000) * 1000);
	pendingSource = SRC_GPS;
	if (settimeofday(&tv, nullptr) != 0) {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_OP, "🕒 Setting the clock from GPS failed");
		return;
	}
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_OP, "🕒 Clock %s from GPS (was off by %lld ms)", valid ? "corrected" : "set", (long long)diff);
}

void registerCli() {
	static Command clockCmd = cli.addCmd("clock", [](cmd* c) {
		Command command(c);
		const String action = command.getArgument("action").getValue();
		if (action == "unset") {
			struct timeval tv;
			tv.tv_sec = static_cast<time_t>(esp_timer_get_time() / 1000000);
			tv.tv_usec = 0;
			pendingSource = SRC_UNKNOWN;
			settimeofday(&tv, nullptr);
			bclog.log(BCLogger::Log_Warn, BCLogger::TAG_CLI, "🕒 Clock reset to 1970 + uptime (test)");
		} else if (action == "ntp") {
			bclog.log(BCLogger::Log_Info, BCLogger::TAG_CLI, esp_sntp_restart() ? "🕒 NTP request sent" : "🕒 NTP not running (WLAN off?)");
		}
		const time_t now = time(nullptr);
		char buf[40];
		strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", localtime(&now));
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_CLI, "🕒 %s (%s)", buf, isValidNow() ? "valid" : "not set");
	});
	clockCmd.addPositionalArgument("action", "show");
	clockCmd.setDescription("Show the clock; \"clock ntp\" asks NTP now, \"clock unset\" resets it to 1970 (test)");
}

bool pollStep(Step& out) {
	const int64_t cur = currentBootOffsetUs();
	if (!haveBaseline) {
		bootOffsetUs = cur;
		haveBaseline = true;
		return false;
	}
	const int64_t d = cur - bootOffsetUs;
	if (llabs(d) < STEP_MIN_MS * 1000) return false;	// jitter between the two reads
	bootOffsetUs = cur;
	out.offsetMs = d / 1000;
	out.newEpochMs = (cur + esp_timer_get_time()) / 1000;
	out.uptimeMs = millis();
	out.source = static_cast<Source>(pendingSource.exchange(SRC_UNKNOWN));
	return true;
}

}	// namespace ClockSync
