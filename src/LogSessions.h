/*
 * LogSessions.h
 *
 * Logging sessions on the SD card. A session is one boot: BCLogger writes all its files
 * into the working directory, named by a running session number (NVS), because at boot
 * the clock is usually not set yet:
 *
 *   /BIKECOMP/CUR/L_0042.bin     data log (LogRecords.h)
 *                 D_0042.log     debug log, N_0042.log Forumslader NMEA
 *                 S_0042.bin     shock snippets, R_0042_NN.bin raw captures (RawCapture.h)
 *                 T_0042.txt     time hints: session start and clock steps (SessionStats.h)
 *
 * After the next boot, a background task finishes every session in CUR except the new one:
 *
 *   1. start time = the start line of T_*, corrected by the clock step that made the clock
 *      valid (NTP or GPS, see ClockSync.h) if the session started without a clock
 *   2. L_* moves to /BIKECOMP/YYYYMMDD/L_HHMMSS.bin -- with its 1970 timestamps rewritten
 *      if a correction is known -- or to /BIKECOMP/NO_TIME/L_0042.bin if no time is known
 *   3. I_HHMMSS.txt next to it: the session summary (distance, duration, road-quality
 *      counts; SessStats::Summary), shown by the log file listing
 *   4. all other files of the session follow under the same new name; empty ones are
 *      deleted. Raw files and the debug log keep their original timestamps -- I_*.txt
 *      carries the correction (corr_ms) for tools that need it.
 *
 * The same task also adds summaries to data logs from before this scheme, and renews those
 * of an older SessStats::Summary::VERSION (format v2 logs only, BACKFILL_PER_BOOT per boot,
 * until NVS says all are done for this version).
 *
 * Every step tolerates SD errors: a file that can't be moved stays in CUR and is tried again
 * after the next boot. A session whose finishing crashed the device twice is moved to
 * NO_TIME unparsed on the third attempt, so a broken file can't cause a boot loop.
 */

#pragma once

#include <Arduino.h>
#include "SessionStats.h"

namespace LogSessions {

extern const String WORKDIR;			// LOGDIR + "/CUR"
static constexpr const char* WORKDIR_NAME = "CUR";

// Starts the background task for all sessions in WORKDIR other than activeStem ("_0042").
void startFinalizer(const String& activeStem);
bool busy();

// Session stem of a log file name: "L_0042.bin" -> "_0042", "R_143012_01.bin" -> "_143012",
// "L0042.bin" (old NO_TIME naming) -> "0042". Empty if the name doesn't look like a log file.
String stemOf(const char* name);

// Reads an I_*.txt summary. false if missing or unreadable.
bool readSummary(const String& path, SessStats::Summary& out);

}	// namespace LogSessions
