/*
 * StatsStore.h
 *
 * The persisted ride statistics: times, cadence counters and distances for TOTAL, TOUR and
 * TRIP in one NVS blob (namespace NVS_STAT_PREFIX "Stats"). Statistics owns the values and
 * decides when to write (Statistics::persistNow()), Distance adds its part.
 */

#pragma once

#include <stdint.h>

namespace StatsStore {

static const uint8_t VERSION = 1;
static const uint8_t STATES = 6;		// Statistics::EDrivingStateMax
static const uint8_t SUMMARIES = 3;		// Statistics::SUM_ESP_TOTAL, _TOUR, _TRIP

struct __attribute__((packed)) Summary {
	uint64_t cadMilliRevs;				// crank revolutions in 1/1000 while pedaling ...
	uint64_t cadMs;						// ... and the time they took
	uint32_t timeMs[STATES];			// per driving state
	float distFree;						// m ridden in DS_FREE_RIDE
	float speedMax;						// km/h
	float distTotal;					// m at `revs`, including distLost
	float distLost;						// m ridden without the BC connected
	uint32_t revs;						// sensor's wheel revolution count at distTotal
};

struct __attribute__((packed)) Blob {
	uint8_t version;
	uint8_t revsKnownMask;				// bit per summary: `revs` is a real sensor value
	uint16_t reserved;
	Summary sum[SUMMARIES];
};

// 184 byte = 6 data entries + 2 = 8 NVS entries per write. Changing the layout needs a new
// VERSION (and a conversion in load(), or the statistics start at zero).
static_assert(sizeof(Summary) == 60 && sizeof(Blob) == 184, "persisted layout, see VERSION");

bool load(Blob& blob);					// false: nothing (valid) stored, blob untouched
bool save(const Blob& blob);
// One key per value in six namespaces, as stored before 2026-10: reads them into the blob,
// stores it and empties the old namespaces. False if there was nothing to migrate.
bool migrateLegacy(Blob& blob);

}
