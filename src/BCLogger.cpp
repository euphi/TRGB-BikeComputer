/*
 * BCLogger.cpp
 *
 *  Created on: 25.02.2023
 *      Author: ian
 */

#include <BCLogger.h>
#include <DateTime.h>
#include <SD_MMC.h>
#include "Singletons.h"
#include <esp_task_wdt.h>

const char *BCLogger::TAG_STRING[LogTagMax] = { "RAW", "FL", "BLE", "STAT", "WIFI", "SD", "OP", "CLI" };
const char *BCLogger::LEVEL_STRING[LogTypeMax] = { "DEBUG", "INFO", "WARN", "ERROR" };

const String BCLogger::TAG_SYMBOL[LogTagMax] = { String("📜"), String("📟"), String("🔵"), String("📊"), String("📶"), String("💾"), String("🎮"), String("⌨") };
const String BCLogger::LEVEL_SYMBOL[LogTypeMax] = { String("🐛"), String("ℹ️"), String("⚠️"), String("❌") };

const String BCLogger::LOGDIR = "/BIKECOMP";

void cmdCB(cmd *c) {
	const Command cmd(c); // Create wrapper object
	bclog.handleCommand(cmd);
}

BCLogger::BCLogger() {
}

void BCLogger::setup() {
	logPrefs[OUT_Serial].begin("LSer", true);
	logPrefs[OUT_File].begin("LFile", true);
	for (uint_fast8_t c = 0; c < LogOutputMax; c++) {
		for (uint_fast8_t d = 0; d < LogTagMax; d++) {
			LogType level = static_cast<LogType>(logPrefs[c].getLong(TAG_STRING[d], -1));
			if (level >= 0) {
				loglevel[c][d] = level;
			} else {
				logf(Log_Info, TAG_OP, "Can't load loglevel for %s from preferences [out: %d]", TAG_STRING[d], c);
			}
		}
	}
	logPrefs[OUT_File].end();
	logPrefs[OUT_Serial].end();

	logcmd = cli.addCmd("loglevel", cmdCB);
	logcmd.addPositionalArgument("logtag");
	logcmd.addPositionalArgument("loglevel");
	logcmd.addFlagArgument("serial");
	logcmd.addFlagArgument("file");

	logShow = cli.addCmd("showloglevel", cmdCB);

	replayLog = cli.addCmd("replay", cmdCB);
	replayLog.addPositionalArgument("path");

	uint8_t cardType = SD_MMC.cardType();
	if (cardType == CARD_NONE) {
		log(Log_Warn, TAG_SD, "No SD card attached!");
		return;
	}
	logf(Log_Info, TAG_SD, "SD Card Type: %s", (cardType == CARD_MMC) ? "MMC" : (cardType == CARD_SD) ? "SDSC" : (cardType == CARD_SDHC) ? "SDHC" : "UNKNOWN");
	logf(Log_Info, TAG_SD, "SD Card Size: %lluMBn", SD_MMC.cardSize() / (1024 * 1024));
	logf(Log_Info, TAG_SD, "Total space: %lluMB", SD_MMC.totalBytes() / (1024 * 1024));
	logf(Log_Info, TAG_SD, "Used space: %lluMB", SD_MMC.usedBytes() / (1024 * 1024));

	if (!SD_MMC.exists(LOGDIR) && !SD_MMC.mkdir(LOGDIR))
		log(Log_Error, TAG_SD, "Failed to create dir " + LOGDIR);

	if (DateTime.getParts().getYear() >= 2023) { // time seems to be somehow valid
		time_t now;
		time(&now);
		file_data = DateFormatter::format((LOGDIR + "/%Y%m%d/L_%H%M%S.bin").c_str(), now);
		file_debuglog = DateFormatter::format((LOGDIR + "/%Y%m%d/D_%H%M%S.log").c_str(), now);
		file_nmealog = DateFormatter::format((LOGDIR + "/%Y%m%d/N_%H%M%S.log").c_str(), now);
		fileNameIncludesDateTime = true;
	} else {
		log(Log_Warn, TAG_SD, "No time available to create file names");
		String dirName = LOGDIR + "/NO_TIME";
		SD_MMC.mkdir(dirName);
	    char fnumber[24];
	    //snprintf(fnumber, sizeof(fnumber), "%04u", listDir(dirName, 0)); --> use this line to calculate first available number
	    noTimeCounter.begin("NoTimeCounter");
	    uint16_t c = noTimeCounter.getShort("Counter", 40);
	    snprintf(fnumber, sizeof(fnumber), "%04u", c++);
	    noTimeCounter.putShort("Counter", c);
	    noTimeCounter.end();
		file_data = dirName + "/L" + fnumber + ".bin";
		file_debuglog = dirName + "/D" + fnumber + ".log";
		file_nmealog = dirName + "/N" + fnumber + ".log";
	}
	fdebug = SD_MMC.open(file_debuglog, FILE_APPEND, true);
	fnmea  = SD_MMC.open(file_nmealog, FILE_APPEND, true);
	fdata  = SD_MMC.open(file_data, FILE_APPEND, true);

	logf(Log_Info, TAG_SD, "New file name: %s\n", file_data.c_str());


	xTaskCreate(+[](void* thisInstance){((BCLogger*)thisInstance)->flushAllFiles();}, "FlusherTask", 3072, this, 5, &flushTaskHandle);
}

void BCLogger::flushAllFiles() {  // Ticker all 5 seconds
	do {  // FlusherTask, Prio 5
		bclog.log(Log_Debug, TAG_SD, "Flush");
		if (fdebug) fdebug.flush();
		yield();
		if (fnmea) fnmea.flush();
		yield();
		if (fdata) fdata.flush();
		vTaskDelay(5000 / portTICK_PERIOD_MS);


	} while (true);
}

void BCLogger::printLoglevels() {
	for (int16_t line = -1; line < LogOutputMax; line++) {
		Serial.print(line == -1 ? "TAG" : line == 0 ? "Serial" : "File");
		Serial.print("\t|");
		for (uint16_t t = TAG_RAW_NMEA; t < LogTagMax; t++) {
			if (line == -1) {
				Serial.print(TAG_STRING[t]);
			} else {
				Serial.print(LEVEL_STRING[loglevel[line][t]]);
			}
			Serial.print("\t|");
			yield();
		}
		Serial.println();
	}
}
void BCLogger::handleCommand(const Command &cmd) {
	if (cmd.equals(logShow)) {
		printLoglevels();
		return;
	}

	if (cmd.equals(replayLog)) {
		Argument pathArg = cmd.getArgument("path");
		logf(Log_Info, TAG_OP, "Replay file %s", pathArg.getValue());
		replayFile(pathArg.getValue());
		return;
	}
	// Get arguments
	Argument argTag = cmd.getArgument("logtag");
	Argument argLevel = cmd.getArgument("loglevel");
	bool argSerial = cmd.getArgument("serial").isSet();
	bool argFile = cmd.getArgument("file").isSet();

	uint16_t t = TAG_RAW_NMEA;
	for (; t < LogTagMax; t++) {
		if (argTag.getValue().equalsIgnoreCase(TAG_STRING[t]))
			break;
	}
	if (t == LogTagMax) {
		logf(Log_Warn, TAG_OP, "Invalid log tag %s", argTag.getValue().c_str());
		return;
	}
	uint16_t l = Log_Debug;
	for (; l < LogTypeMax; l++) {
		if (argLevel.getValue().equalsIgnoreCase(LEVEL_STRING[l]))
			break;
	}
	Serial.print("Level: ");
	Serial.println(l);
	Serial.flush();
	if (l == LogTypeMax) {
		logf(Log_Warn, TAG_OP, "Invalid log level %s", argLevel.getValue().c_str());
		return;
	}
	setLogLevel(static_cast<LogType>(l), static_cast<LogTag>(t), argFile, argSerial);
}

//void BCLogger::storeAllPrefs() {
//	logPrefs[OUT_Serial].begin("LSer");
//	logPrefs[OUT_File].begin("LFile");
//	for (uint_fast8_t c = 0; c < LogOutputMax; c++) {
//		for (uint_fast8_t d = 0; d < LogTagMax; d++) {
//			logPrefs[c].putLong(TAG_STRING[d], loglevel[c][d]);
//		}
//	}
//	logPrefs[OUT_File].end();
//	logPrefs[OUT_Serial].end();
//}

void BCLogger::storeLoglevel(LogType level, LogTag tag, bool file, bool serial) {
	if (file) {
		logPrefs[OUT_File].begin("LFile");
		logPrefs[OUT_File].putLong(TAG_STRING[tag], loglevel[OUT_File][tag]);
		logPrefs[OUT_File].end();
	}
	if (serial) {
		logPrefs[OUT_Serial].begin("LSer");
		logPrefs[OUT_Serial].putLong(TAG_STRING[tag], loglevel[OUT_Serial][tag]);
		logPrefs[OUT_Serial].end();
	}
}

void BCLogger::setLogLevel(LogType level, LogTag tag, bool file, bool serial) {
	if (level >= LogTypeMax || level < 0) {
		logf(Log_Error, TAG_OP, "Wrong new log-level %d", level);
		return;
	}
	if (tag >= LogTagMax || tag < 0) {
		logf(Log_Error, TAG_OP, "Wrong new log-tag %d", tag);
		return;
	}
	logf(Log_Info, TAG_OP, "New log-level %s [%d] for tag %s [%d] for %s %s.", LEVEL_STRING[level], level, TAG_STRING[tag], tag, file ? "File" : "",
			serial ? "Serial" : "");
	if (file)
		loglevel[OUT_File][tag] = level;
	if (serial)
		loglevel[OUT_Serial][tag] = level;
	storeLoglevel(level, tag, file, serial);
}


BCLogger::LogType BCLogger::getLogLevel(LogTag tag, bool serial) {
	if (tag >= LogTagMax || tag < 0) {
		logf(Log_Error, TAG_OP, "getLogLevel for wrong log-tag %d", tag);
		return BCLogger::Log_Error;
	}
	return loglevel[serial?OUT_Serial:OUT_File][tag];
}


void BCLogger::log(LogType type, LogTag tag, const String& str) const {
	bool write_file = checkLogLevel(type, tag, true) && !file_debuglog.isEmpty();  // empty during init
	bool write_serial = checkLogLevel(type, tag, false);
	if (!(write_file || write_serial))
		return;

	File f;
	if (write_file) {
		f = (tag == TAG_RAW_NMEA) ? fnmea : fdebug;
		if (!f) {
			Serial.printf("❌ 💾 Failed to use file %s for appending.\n", f.name());
			write_file = false;
			write_serial = true; // writing to file failed, so send it to serial
			//if (!write_serial) return;
		}
	}
	time_t now;
	time(&now);
	String format = String(DateFormatter::COMPAT) + ": ";
	String timeStr = DateFormatter::format(format.c_str(), now);

	String symbolStr = LEVEL_SYMBOL[type] + TAG_SYMBOL[tag] + String(" ");

	if (write_file) {
		f.print(timeStr);
		f.println(str);
	}
	yield();
	if (write_serial) {
		Serial.print(symbolStr);
		Serial.print(timeStr);
		Serial.println(str);
	}
}

void BCLogger::logf(LogType type, LogTag tag, const char *format, ...) const {
	if (!(checkLogLevel(type, tag, true) || checkLogLevel(type, tag, false))) return;
	va_list arg;
	va_start(arg, format);
	char temp[256];
	size_t len = vsnprintf(temp, sizeof(temp), format, arg);
	if (len >= sizeof(temp)) {
		log(Log_Error, TAG_OP, "Logger: Log-String shortened (too long)");
		log(type, tag, format);
	}
	va_end(arg);
	log(type, tag, String(temp));
}


void BCLogger::appendDataLog(float speed, float temp, float gradient, uint32_t distance, float height, uint8_t hr) {
	LogData b;
	time_t now;
	time(&now);

	b.timestamp = now;
	b.speed = speed;
	b.temp = temp;
	b.grad = gradient;
	b.dist_m = distance;
	b.height = height;
	b.hr = hr;

	if (!fdata) {
		log(Log_Warn, TAG_SD, "Data file not open");
		return;
	}
	fdata.write((byte*) &b, sizeof(b));
}

int16_t BCLogger::listDir(const String &dirname, uint8_t levels) const {
	int16_t max_number = 0;
	logf(Log_Debug, TAG_SD, "📁 Listing directory: %s", dirname);

	File root = SD_MMC.open(dirname);
	if (!root) {
		log(Log_Error, TAG_SD, "Failed to open directory");
		return -1;
	}
	if (!root.isDirectory()) {
		log(Log_Error, TAG_SD, "Not a directory");
		root.close();
		return -1;
	}
	File file = root.openNextFile();
	while (file) {
		if (file.isDirectory()) {
			Serial.print("  DIR : ");
			Serial.println(file.name());
			if (levels) {
				listDir(String(file.name()), levels - 1);
			}
		} else {
			Serial.print("  FILE: ");
			Serial.print(file.name());
			Serial.print("  SIZE: ");
			Serial.println(file.size());
			unsigned int filenumber;
			if (sscanf(file.name(), "N%4u.BIN", &filenumber) == 1) {
				if (filenumber > max_number)
					max_number = filenumber;
			}
		}
		file = root.openNextFile();
	}
	root.close();
	return ++max_number;
}


uint16_t BCLogger::getAllFileLinks(String &rc) const {
	rc += "<html><body><h1>Logfiles</h1>\n<p>\n";
	File root = SD_MMC.open(LOGDIR);

	  if(!root){
	    rc += "Failed to open directory";
	    Serial.println("500 - Can't open file/dir");
	    return 500;
	  }
	  if(!root.isDirectory()){
	    rc += "Not a directory";
	    Serial.println("500 - Not a directory");
	    root.close();
	    return 500;
	  }
	  getFileHTML(rc, root, 9);  // 9 chars for "/BIKECOMP"

	rc += "</p></body></html>";
	root.close();
	return 200;
}

void BCLogger::getFileHTML(String &rc, File &root, uint8_t strip_front) const {
    File file = root.openNextFile();  // First file in root-DIR
	while (file) {
		esp_task_wdt_reset();
		log(Log_Info, TAG_SD, file.name());
		if (file.name()[0] == 'x' || file.name()[0] == 'x') {  // D / N ->x to temporarily access nmea/debug files
			file = root.openNextFile();   // next file in root-DIR
			continue;
		}
		String filehtml;
		if (file.isDirectory()) {
			rc += "<h2>";
			rc += file.name();
			rc += "</h2>\n";
			String fh;
			getFileHTML(fh, file, strip_front);
			rc += fh;
			rc += "\n";
		} else {
			String uri= (file.path()+ strip_front+1);
			rc = rc + "<a href=\"/log/"+uri+"\">" + uri + "</a>";
			rc = rc + " (" + file.size() + ")";
			if (!fileReplay && (uri.charAt(uri.length()-9) == 'N' || uri.charAt(uri.length()-12) == 'N')) { 	//RAW_NMEA-File
					rc = rc + " <a href=\"/replay/"+uri+"\">Replay</a> ";
			}
			rc = rc + " <a href=\"/del/"+uri+"\">DEL</a><br />\n";
		}
		file = root.openNextFile();   // next file in root-DIR
		yield();
	}
	file.close();
}

bool BCLogger::deleteFile(const String& path){
  if (SD_MMC.remove(path.c_str())) {
	  logf(Log_Info, TAG_SD, "File %s deleted.", path.c_str());
	  return true;
  } else {
	  logf(Log_Warn, TAG_SD, "Deletion of file %s failed.", path.c_str());
	  return false;
  }
}

void BCLogger::autoCleanUp(const char* root_name) {
	  File root = SD_MMC.open(root_name);
	  if(!root){
	    log(Log_Warn, TAG_SD, F("🧹 autoCleanUp - Failed to open directory"));
	    return;
	  }
	  if(!root.isDirectory()){
		log(Log_Warn, TAG_SD, F("🧹 autoCleanUp - Not a directory"));
	    return;
	  }
	  cleanUp(root, 512);
}

void BCLogger::replayNextLine() {
	uint32_t next;
	do {
		String wasteStr = fileReplay.readStringUntil('$');	// waste string necessary for first line. It always helps to get in sync again if replay gets out of sync for whatever reason.
		String dataStr = "$" + fileReplay.readStringUntil('\n');
		String timeStr = fileReplay.readStringUntil(':').substring(11);
		uint32_t timeNext = timeStr.toInt();
		if (!timeNext) {
			bclog.log(Log_Warn, TAG_SD, "End of Replay file. Closing.");
			fileReplay.close();
			replayTicker.detach();		// there should be no timer running, but better safe than sorry
			stats.setConnected(false);
			return;
		}
		if (timeLasteLine == 0)	timeLasteLine = timeNext;
		next = timeNext-timeLasteLine;
		timeLasteLine = timeNext;
		if (next > 40) next -=40; //FIXME: Ugly workaround for 60seconds wrap
		bclog.logf(Log_Debug, TAG_SD, "Replay Strings:\n\tWaste:\t%s\n\tData:\t%s\n\tTime:\t%s (%d -> next in %d sec))",wasteStr.c_str(), dataStr.c_str(), timeStr.c_str(), timeNext, next );
		flparser.updateFromString(dataStr);
	} while (!next);
	replayTicker.once(next, +[](BCLogger* thisInstance){thisInstance->replayNextLine();}, this);
}

bool BCLogger::replayFile(const String &path) {
	if (fileReplay) {
		logf(Log_Warn, TAG_SD, "ReplayFile %s already open - closing it.", path);
		fileReplay.close();
		timeLasteLine = 0;
	}
	fileReplay = SD_MMC.open(path);
	if (!fileReplay) {
		logf(Log_Error, TAG_SD, "Failed to open replay file %s", path);
		return false;
	}
	stats.setConnected(true);	// Fake connect FL
	replayNextLine();
	return true;
}

bool BCLogger::cleanUp(File& root, uint32_t minsize) {
    File file = root.openNextFile();  // First file in root-DIR
    bool allFileDeleted = true;
	while (file) {
		esp_task_wdt_reset();
		if (file.isDirectory()) {
			bool dirClean = cleanUp(file, minsize);
			allFileDeleted &= dirClean;
			bool delOk = SD_MMC.rmdir(file.path());
			logf(delOk ? Log_Info : Log_Warn, TAG_SD, "🧹%s Directory %s empty. Deleting - %s", delOk ? "✅":"❌", file.name(), delOk ? "OK":"failed!");
		} else if (file.size() < minsize) {
			bool delOk = SD_MMC.remove(file.path());
			logf(delOk ? Log_Info : Log_Warn, TAG_SD, "🧹%s File %s to small (%d). Deleting - %s", delOk ? "✅":"❌", file.name(), file.size(), delOk ? "OK":"failed!");
		} else {
			allFileDeleted = false;
		}
		yield();
		file = root.openNextFile();   // next file in root-DIR
	}
	return allFileDeleted;
}

