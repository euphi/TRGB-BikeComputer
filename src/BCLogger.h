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
		LogTagMax
	};
private:
	struct LogData {
		time_t timestamp;					//        4
		float speed;						// + 4 =  8
		float temp;  						// + 4 = 12
		float grad;							// + 4 = 16
		float height;						// + 4 = 20
		uint32_t dist_m;					// + 4 = 24
		uint8_t hr : 8;						// + 1 = 25
		uint8_t cadence: 8;  				// + 1 = 26
	};


	enum LogOutput {
		OUT_Serial,
		OUT_File,
		LogOutputMax
	};

	//                                         TAG_RAW_NMEA TAG_FL    TAG_BLE   TAG_STAT  TAG_WIFI 	TAG_SD    TAG_OP,  TAG_CLI
	LogType loglevel[LogOutputMax][LogTagMax] = {{Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Debug, Log_Info},   // Terminal
			                                     {Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Info, Log_Error}};   // File


	Preferences logPrefs[LogOutputMax];

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


public:
	BCLogger();
	void setup();
	void log(LogType type, LogTag tag, const String& str) const;
	void logf(LogType type, LogTag tag, const char* format, ...) const;
	inline bool checkLogLevel(LogType type, LogTag tag, bool write_file = false) const {return loglevel[write_file?OUT_File:OUT_Serial][tag] <= type;}

	void setLogLevel(LogType type, LogTag tag, bool file, bool serial);

	// DataLogger
	void appendDataLog(float speed, float temp, float gradient, uint32_t distance, float height, uint8_t hr);

	int16_t listDir(const String& dirname, uint8_t levels) const;

	void handleCommand(const Command& cmd);
	void flushAllFiles();

	static const char* TAG_STRING[LogTagMax];
	static const char* LEVEL_STRING[LogTypeMax];

	static const String TAG_SYMBOL[LogTagMax];
	static const String LEVEL_SYMBOL[LogTypeMax];
	static const String LOGDIR;

	uint16_t getAllFileLinks(String &rc) const;
	bool deleteFile(const String& path);
	void autoCleanUp(const char* root_name);

	bool replayFile(const String& path);




private:
	void getFileHTML(String &rc, File &root, uint8_t strip_front) const;
	bool cleanUp(File& root, uint32_t minsize);
	void replayNextLine();

};
