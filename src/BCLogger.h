/*
 * BCLogger.h
 *
 *  Created on: 25.02.2023
 *      Author: ian
 */

#pragma once

#include <Preferences.h>
#include <SimpleCLI.h>
#include <FS.h>
#include <Ticker.h>
#include <AsyncEventSource.h>
#include "BikeGpsProtocol.h"
#include "LogRecords.h"
#include <atomic>
#include <freertos/message_buffer.h>

class BCLogger {
public:
	enum LogType {
		Log_Debug = 0,
		Log_Info,
		Log_Warn,
		Log_Error,
		LogTypeMax
	};

	enum LogTag {
		TAG_RAW_NMEA = 0,
		TAG_FL,
		TAG_BLE,
		TAG_STAT,
		TAG_WIFI,
		TAG_SD,
		TAG_OP,
		TAG_CLI,
		TAG_UI,
		TAG_WEB,		// HTTP server: requests, and the memory diagnostics that go with them
		LogTagMax
	};
private:
	// The binary record layouts (format v2) live in LogRecords.h, shared with I2CSensors,
	// which builds the road-quality and shock records itself.

	enum LogOutput {
		OUT_Serial,
		OUT_File,
		LogOutputMax
	};

	//                                         TAG_RAW_NMEA TAG_FL    TAG_BLE   TAG_STAT  TAG_WIFI 	TAG_SD    TAG_OP,    TAG_CLI,  TAG_UI    TAG_WEB
	LogType loglevel[LogOutputMax][LogTagMax] = {{Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Debug, Log_Info, Log_Info, Log_Info},   // Terminal
			                                     {Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Error, Log_Info, Log_Info}};   // File


	Preferences logPrefs[LogOutputMax];
	Preferences noTimeCounter;

	String file_data, file_nmealog, file_debuglog;		// Filename for logfiles
	File fdata, fnmea, fdebug;

	// This boot's session in LogSessions::WORKDIR: "_0042" -> L_0042.bin, D_0042.log, ...
	// Finished (dated, summarised) after the next boot, see LogSessions.h.
	String sessionStem;
	String file_hints;									// T_0042.txt: start and clock steps
	void appendHint(const char* line);
	void checkClockStep();
	bool isActiveSessionFile(const String& path) const;

	void storeLoglevel(LogType level, LogTag tag, bool file, bool serial);
	void printLoglevels();
	void checkTagTablesComplete() const;

	Ticker replayTicker;
	File fileReplay;
	uint32_t timeLasteLine = 0;

	Command logcmd;
	Command logShow;
	Command replayLog;

	TaskHandle_t flushTaskHandle = nullptr;
	SemaphoreHandle_t xPrintMutex = nullptr;

	// Every binary record (data, road quality, shock) goes through this queue and is written
	// by FlusherTask -- so the producers (esp_timer's 5 s data tick, the 400 Hz ImuTask) never
	// wait for the SD card, and the records land in the file in the order they were made.
	static constexpr UBaseType_t RECORD_QUEUE_LEN = 16;		// 1 KB internal RAM; the writer is woken per record, so a few suffice
	QueueHandle_t recordQueue = nullptr;
	std::atomic<uint32_t> recordsDropped{0};				// queue full or no data file
	std::atomic<uint32_t> recordsWritten{0};
	void writeRecord(const uint8_t* rec);

	// Raw byte streams (RawCapture.h) -- same idea as the records: the producer (ImuTask) only
	// puts messages into this buffer, FlusherTask writes them. A message is
	// [RawCmd][RawStream][payload]; a message buffer keeps the boundaries.
	static constexpr size_t RAW_BUFFER_SIZE = 4096;			// internal RAM (scarce!): ~1.6 s of capture at 2.4 KB/s
	enum RawCmd : uint8_t {RAW_CMD_OPEN = 0, RAW_CMD_WRITE, RAW_CMD_CLOSE};
	MessageBufferHandle_t rawBuffer = nullptr;
	File fraw[2];
	char rawName[2][40] = {};								// guarded by rawNameMux (read by the web page)
	String rawFilePath[2];									// FlusherTask only: reopened for append after a close
	portMUX_TYPE rawNameMux = portMUX_INITIALIZER_UNLOCKED;
	std::atomic<uint32_t> rawBytes[2] = {};
	std::atomic<uint32_t> rawDropped{0};
	uint8_t rawCaptureCount = 0;
	uint8_t* rawRx = nullptr;								// receive buffer, RAW_MAX_MSG, allocated in setup()
	bool rawSend(uint8_t* msg, size_t len, uint8_t cmd, uint8_t stream);
	void rawDrain();
	void rawHandle(const uint8_t* msg, size_t len);
	String rawPath(uint8_t stream);
	void wakeFlusher();


public:
	BCLogger();
	void setup();
	void log(LogType type, LogTag tag, const String& str);
	void logf(LogType type, LogTag tag, const char* format, ...);
	inline bool checkLogLevel(LogType type, LogTag tag, bool write_file) const {return loglevel[write_file?OUT_File:OUT_Serial][tag] <= type;}
	inline bool checkLogLevel(LogType type, LogTag tag) const {return (loglevel[OUT_File][tag] <= type)  ||  (loglevel[OUT_Serial][tag] <= type);  }


	void setLogLevel(LogType type, LogTag tag, bool file, bool serial );
	LogType getLogLevel(LogTag tag, bool serial = false);

	// DataLogger
	// gradient is the value shown on the display; gradBaro/gradImu are the two sources behind
	// it (NAN if unavailable), roadClass the latest road-quality class (0 = not rated).
	void appendDataLog(const float speed, const float temp, const float gradient, const float distance, const float height, const uint8_t hr, const uint8_t cadence, const SGpsFix& gps,
	                   const float gradBaro = NAN, const float gradImu = NAN, const uint8_t roadClass = 0);
	// Queue one complete 64-byte record (LogRecords.h) for the data file. Non-blocking: safe
	// from any task; returns false (and counts it) if the queue is full.
	template <class T> bool appendRecord(const T& rec) {
		static_assert(sizeof(T) == LogRec::RECORD_SIZE, "log records are 64 byte");
		return enqueueRecord(&rec);
	}
	bool enqueueRecord(const void* rec64);
	uint32_t getRecordsDropped() const {return recordsDropped;}
	uint32_t getRecordsWritten() const {return recordsWritten;}
	// Fills timestamp (s) and timestampMs from the system clock
	static void nowEpoch(time_t& sec, uint16_t& ms);

	// Raw byte streams into their own files next to the data log (see RawCapture.h):
	// RAW_CAPTURE opens a new R_<session>_NN.bin each time, RAW_SNIPPETS the session's
	// S_<session>.bin. A WRITE after a CLOSE reopens the stream's last file for appending --
	// an open file holds ~4 KB of internal RAM, so the rarely written snippet file is closed
	// after every snippet. Only one producer task may call these (the message buffer has a
	// single-writer contract) -- that is the ImuTask. All non-blocking; false = dropped.
	// msg must have RAW_PREFIX spare bytes in front of the payload (filled in here), so
	// the payload needn't be copied once more on the producer's stack.
	enum RawStream : uint8_t {RAW_CAPTURE = 0, RAW_SNIPPETS = 1};
	static constexpr size_t RAW_PREFIX = 2;
	static constexpr size_t RAW_MAX_MSG = 512;					// larger payloads are sent in pieces
	size_t rawSpace() const {return rawBuffer ? xMessageBufferSpacesAvailable(rawBuffer) : 0;}
	bool rawOpen(RawStream s, uint8_t* msg, size_t len) {return rawSend(msg, len, RAW_CMD_OPEN, s);}
	bool rawWrite(RawStream s, uint8_t* msg, size_t len) {return rawSend(msg, len, RAW_CMD_WRITE, s);}
	bool rawClose(RawStream s) {uint8_t m[RAW_PREFIX]; return rawSend(m, RAW_PREFIX, RAW_CMD_CLOSE, s);}
	uint32_t getRawBytes(RawStream s) const {return rawBytes[s];}
	uint32_t getRawDropped() const {return rawDropped;}
	void getRawFileName(RawStream s, char* out, size_t len);

	int16_t listDir(const String& dirname, uint8_t levels);

	void handleCommand(const Command& cmd);
	void flushAllFiles();
	// Flushes the open log files right now, from any task -- before a deliberate restart or
	// deep sleep, which would otherwise lose up to 5 s of log (FlusherTask's period).
	void flushFiles();

	static const char* TAG_STRING[LogTagMax];
	static const char* LEVEL_STRING[LogTypeMax];

	static const String TAG_SYMBOL[LogTagMax];
	static const String LEVEL_SYMBOL[LogTypeMax];
	static const String LOGDIR;

	uint16_t getAllFileLinks(String &rc);
	bool deleteFile(const String& path);
	void autoCleanUp(const char* root_name);

	bool replayFile(const String& path);

	void save_coredump_to_littlefs(const String& filename);


private:
	void appendDayTable(String &rc, const char* dayDir, uint32_t& totalFiles, uint32_t& totalBytes);
	bool cleanUp(File& root, uint32_t minsize);
	void replayNextLine();

	AsyncEventSource logevents;
	void sendLogEvent(LogType type, LogTag tag, const String& timeStr, const String& logMessage);
};
