/*
 * SessionStats.h
 *
 * Pure (no Arduino, no file system) part of finishing a logging session after the next
 * boot, see LogSessions.h:
 *
 *   - TimeHints: parses the session's time-hint file T_<stem>.txt and turns timestamps
 *     written before the clock was set (1970) into real ones.
 *   - Stats: short summary of a data log L_*.bin (distance, duration, road-quality
 *     counts), fed record by record.
 *   - Summary: the stats file I_<stem>.txt, written by the finaliser and read back by
 *     the log file listing.
 *
 * Host test: test/native_sessionstats/sessionstats_test.cpp (build command in its header).
 *
 * Time-hint file, one line per event, written by BCLogger:
 *   start <epochMs> <valid 0|1> <uptimeMs>        at session start
 *   step <src> <offsetMs> <newEpochMs> <uptimeMs>  the clock was stepped by offsetMs
 * src is ntp, gps or ? (unknown).
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace SessStats {

// Anything before 2023-01-01 is the unset clock counting up from 1970.
static constexpr int64_t VALID_EPOCH_S = 1672531200;
inline bool isValidMs(int64_t epochMs) {return epochMs >= VALID_EPOCH_S * 1000;}

static constexpr size_t SRC_LEN = 8;

struct TimeHints {
	bool hasStart = false;
	int64_t startMs = 0;				// system clock at session start, as it was then
	// Sum of all steps up to and including the one that made the clock valid -- what an
	// invalid (1970) timestamp of this session needs added. Later steps are drift
	// corrections of an already valid clock and don't concern the 1970 timestamps.
	bool hasCorrection = false;
	int64_t correctionMs = 0;
	char source[SRC_LEN] = "";			// who set the clock valid (ntp/gps/?)
	uint16_t steps = 0;					// all steps, including drift corrections

	void parseLine(const char* line);
	// Corrects epochMs in place if it is invalid and a correction is known.
	// Returns true if the result is a valid time.
	bool correct(int64_t& epochMs) const;
};

// Speed above which the bike counts as moving.
static constexpr float MOVING_KMH = 2.0f;
// Records further apart than this are a gap (records lost, card busy): not counted as moving.
static constexpr uint32_t MAX_STEP_MS = 30000;

struct Stats {
	uint32_t nData = 0, nRq = 0, nRqRated = 0, nShock = 0, nShockSuppressed = 0, nLabel = 0, nOther = 0;
	uint32_t nGps = 0;					// data records with a fresh GPS fix
	uint32_t nTimeInvalid = 0;			// records whose time could not be corrected
	uint32_t nTimeCorrected = 0;
	bool hasTime = false;
	int64_t firstMs = 0, lastMs = 0;	// over all record types, corrected
	double distM = 0;
	uint32_t movingMs = 0;
	float vmaxKmh = 0;

	// rec: one 64-byte record as written (LogRecords.h). Returns the timestamp after
	// correction in *correctedMs if non-null.
	void add(const uint8_t* rec, const TimeHints& hints, int64_t* correctedMs = nullptr);

private:
	bool hasPrev = false;
	int64_t prevMs = 0;
	float prevDist = 0;
};

// Contents of I_<stem>.txt. Also what the listing shows. Bump VERSION when a value is
// computed differently: the finaliser then recomputes older summaries (LogSessions.h).
struct Summary {
	static constexpr uint16_t VERSION = 2;		// 2: moving time/vmax from the wheel sensor only
	uint16_t version = VERSION;
	int64_t startS = 0, endS = 0;		// corrected where possible
	char time[12] = "none";				// ok | corrected | none
	char source[SRC_LEN] = "";
	int64_t correctionMs = 0;
	uint32_t distM = 0, durS = 0, moveS = 0;
	float vmaxKmh = 0, vavgKmh = 0;
	uint32_t nData = 0, nRq = 0, nRqRated = 0, nShock = 0, nShockSuppressed = 0, nLabel = 0, nGps = 0;
	uint16_t nRaw = 0;					// raw capture files R_*
	bool lCorrected = false;			// timestamps in L_*.bin were rewritten

	void fromStats(const Stats& s, const TimeHints& h);
	// Plain "key=value" lines. Returns the length written (0 if buf too small).
	size_t format(char* buf, size_t len) const;
	// Unknown keys are ignored. Returns false if nothing usable was found.
	bool parse(const char* text);
	// One line for the listing, e.g. "42.3 km · 2:15 h (1:58 moving) · Ø 21.5 km/h ...",
	// as HTML (entities for the dots; nothing from the file is copied in verbatim) or as
	// plain UTF-8 for the log.
	size_t describe(char* buf, size_t len, bool html = true) const;
};

// Reads a little-endian value at offset off of a record.
template <class T> inline T get(const uint8_t* rec, size_t off) {
	T v; const uint8_t* p = rec + off; uint8_t* d = reinterpret_cast<uint8_t*>(&v);
	for (size_t i = 0; i < sizeof(T); i++) d[i] = p[i];
	return v;
}
template <class T> inline void put(uint8_t* rec, size_t off, T v) {
	const uint8_t* s = reinterpret_cast<const uint8_t*>(&v); uint8_t* p = rec + off;
	for (size_t i = 0; i < sizeof(T); i++) p[i] = s[i];
}

// Timestamp of any record type (they all start with time_t seconds; the ms sit at 58 in
// Data and at 8 in the others).
int64_t recordTimeMs(const uint8_t* rec);
// Rewrites the record's timestamp (seconds + ms) to epochMs.
void setRecordTimeMs(uint8_t* rec, int64_t epochMs);

}	// namespace SessStats
