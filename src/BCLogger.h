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
		LogTagMax
	};
private:
	// Bits of LogData::gpsFlags. GPS_LOG_VALID mirrors SGpsFix::valid (a POSITION_UPDATE was
	// received at all); it does NOT mean the fix is fresh -- check gpsFixAgeMs for that
	// (see BikeGpsProtocol.h). The other bits mirror SGpsFix's hasXxx flags: the corresponding
	// value field is 0 when its bit is clear.
	enum LogDataGpsFlags : uint8_t {
		LOG_GPS_VALID        = 0x01,
		LOG_GPS_HAS_ALTITUDE = 0x02,
		LOG_GPS_HAS_SPEED    = 0x04,
		LOG_GPS_HAS_BEARING  = 0x08,
		LOG_GPS_HAS_ACCURACY = 0x10,
	};

	struct LogData {
		time_t timestamp;					//        4
		float speed;						// + 4 =  8
		float temp;  						// + 4 = 12
		float grad;							// + 4 = 16
		float height;						// + 4 = 20
		float dist_m;						// + 4 = 24
		uint8_t hr : 8;						// + 1 = 25
		uint8_t cadence: 8;  				// + 1 = 26
		// GPS position from TrailBridge's GPS-Positions-Service (see ../TrailBridge/PROTOCOL.md
		// and BikeGpsProtocol.h) -- added here, binary format bumped incompatibly.
		uint8_t gpsFlags;					// + 1 = 27  (see LogDataGpsFlags)
		int32_t gpsLatitudeE7;				// + 4 = 31
		int32_t gpsLongitudeE7;				// + 4 = 35
		int32_t gpsAltitudeM;				// + 4 = 39
		uint32_t gpsSpeedCms;				// + 4 = 43
		uint16_t gpsBearingDegX100;			// + 2 = 45
		uint16_t gpsAccuracyMX10;			// + 2 = 47
		uint32_t gpsFixAgeMs;				// + 4 = 51
	};


	enum LogOutput {
		OUT_Serial,
		OUT_File,
		LogOutputMax
	};

	//                                         TAG_RAW_NMEA TAG_FL    TAG_BLE   TAG_STAT  TAG_WIFI 	TAG_SD    TAG_OP,    TAG_CLI,  TAG_UI
	LogType loglevel[LogOutputMax][LogTagMax] = {{Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Debug, Log_Info, Log_Info},   // Terminal
			                                     {Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Error, Log_Info}};   // File


	Preferences logPrefs[LogOutputMax];
	Preferences noTimeCounter;

	String file_data, file_nmealog, file_debuglog;		// Filename for logfiles
	File fdata, fnmea, fdebug;

	bool fileNameIncludesDateTime = false;

	void storeAllPrefs();
	void storeLoglevel(LogType level, LogTag tag, bool file, bool serial);
	void printLoglevels();

	Ticker replayTicker;
	File fileReplay;
	uint32_t timeLasteLine = 0;

	Command logcmd;
	Command logShow;
	Command replayLog;

	TaskHandle_t flushTaskHandle = nullptr;
	SemaphoreHandle_t xPrintMutex = nullptr;


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
	void appendDataLog(const float speed, const float temp, const float gradient, const float distance, const float height, const uint8_t hr, const uint8_t cadence, const SGpsFix& gps);

	int16_t listDir(const String& dirname, uint8_t levels);

	void handleCommand(const Command& cmd);
	void flushAllFiles();

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
	void getFileHTML(String &rc, File &root, uint8_t strip_front);
	bool cleanUp(File& root, uint32_t minsize);
	void replayNextLine();

	AsyncEventSource logevents;
	void sendLogEvent(const String& logMessage, const String& tag);
};
