/*
 * LogPush.h
 *
 * Uploads finished log sessions to a WebDAV folder (a Nextcloud) or straight to the log
 * service, for a bike computer that is not parked in reach of the home WiFi, where the
 * service's pull cannot get at it (doc/LOGSERVICE.md "Push"). Pure part: LogPushCore.h.
 *
 * Push is an addition to the pull, not a replacement: the SD card stays as it is (nothing is
 * moved or deleted after an upload), and the service files a pushed file exactly like a pulled
 * one -- the same bytes twice are "unchanged". What has been uploaded is recorded in
 * MANIFEST_PATH on the SD card, so each file goes up once (again if its size changes).
 *
 * When: WifiWebserver::checkLoop() calls poll() every 500 ms. SETTLE_MS after every WiFi
 * connect (LogSessions has moved the last session out of CUR/ by then), then every
 * PERIOD_MS while online (a ride session ended with a long press appears without a reconnect),
 * RETRY_MS after a failure. A run with nothing new costs a walk over the card, no network.
 * The work happens in a task of its own that exists only while it runs.
 *
 * Memory: TLS needs ~40 KB, more than the internal heap has to spare (doc/PITFALLS.md). While
 * a run is going, mbedTLS allocates from PSRAM (setTlsPsram()), the task stack (9 KB) is in PSRAM
 * as well, and a run only starts with MIN_FREE_INTERNAL free.
 *
 * Secrets: the password/token only comes in through the POST body of /push/config, never goes
 * back out (the page shows "set") and is never logged.
 */

#pragma once

#include <Arduino.h>
#include <atomic>
#include "LogPushCore.h"

struct LogPushStatus {
	bool enabled = false;
	bool configured = false;		// URL set
	bool hasSecret = false;
	bool running = false;
	char url[PushCore::URL_MAX + 1] = "";
	char user[PushCore::USER_MAX + 1] = "";
	char state[24] = "idle";		// idle, waiting, listing, uploading, ok, failed, ...
	char current[PushCore::REL_MAX + 1] = "";
	char lastError[96] = "";
	uint32_t filesDone = 0, filesTodo = 0;
	uint32_t bytesDone = 0, bytesTodo = 0;
	uint32_t manifestEntries = 0;
	uint32_t lastRunAgoS = 0;		// 0 = never since boot
	time_t lastOk = 0;				// wall clock of the last complete run (0 = none since boot)
	uint32_t lastOkFiles = 0;		// files uploaded by it
};

class LogPush {
public:
	static constexpr const char* MANIFEST_PATH = "/bcpush.txt";
	static constexpr uint32_t SETTLE_MS = 20 * 1000;
	static constexpr uint32_t PERIOD_MS = 10 * 60 * 1000;
	static constexpr uint32_t RETRY_MS = 5 * 60 * 1000;
	static constexpr uint32_t MEMORY_RETRY_MS = 60 * 1000;
	static constexpr size_t MIN_FREE_INTERNAL = 28 * 1024;	// was 36 with the stack internal
	static constexpr size_t MANIFEST_CAP = 4096;	// files on the card that can be remembered
	static constexpr size_t TODO_CAP = 1024;		// files per run, the rest goes in the next one

	void setup();					// after webserver.setup(): NVS, CLI, web routes
	// From WifiWebserver::checkLoop() (esp_timer task): only decides and starts the task.
	void poll(bool online);

	enum class Mode : uint8_t {NONE = 0, PUSH, TEST, MARK};
	// false if a run is going or the push is not configured.
	bool request(Mode m);

	void getStatus(LogPushStatus& out);
	// For BCLogger::deleteFile(): the file is being uploaded right now (open, must not be deleted).
	bool isBusyWith(const String& sdPath);

	// /push/config. `secret` empty = keep the stored one, unless clearSecret.
	enum class SaveResult : uint8_t {OK, BAD_URL, TOO_LONG};
	SaveResult configure(bool enabled, const char* url, const char* user, const char* secret, bool clearSecret);
	void setEnabled(bool on);

private:
	SemaphoreHandle_t mtx = nullptr;		// guards cfg and st
	PushCore::Blob cfg{};
	LogPushStatus st;
	char busyPath[64] = "";				// guarded by mtx: SD path of the open file
	std::atomic<bool> running{false};
	std::atomic<uint8_t> pending{0};		// Mode requested by the page / CLI
	uint32_t onlineSince = 0;				// poll() only
	std::atomic<uint32_t> nextDue{0};		// millis(); 0 = SETTLE_MS after the connect
	std::atomic<uint32_t> lastRunMs{0};
	struct TaskArg {LogPush* self; Mode mode; bool manual;};

	void load();
	void save();							// mtx held
	bool start(Mode m, bool manual);		// manual: from the page / CLI, runs even when switched off
	static void taskMain(void* arg);
	void run(Mode m, bool manual);
	void setState(const char* state, const char* error = nullptr);
	void registerCli();
	void registerRoutes();
};
