/*
 * StatsStore.cpp
 */

#include "StatsStore.h"
#include "Statistics.h"
#include "NvsUtil.h"
#include "Singletons.h"
#include <global_settings.h>

namespace StatsStore {

static const char* const NAMESPACE = NVS_STAT_PREFIX "Stats";
static const char* const KEY = "bike0";

bool load(Blob& blob) {
	Preferences p;
	if (!p.begin(NAMESPACE, false)) return false;		// read/write: creates the namespace on the first boot
	Blob stored;
	const bool ok = NvsUtil::loadBlob(p, KEY, stored) && stored.version == VERSION;
	p.end();
	if (ok) blob = stored;
	return ok;
}

bool save(const Blob& blob) {
	Preferences p;
	if (!p.begin(NAMESPACE, false)) return false;
	const bool ok = NvsUtil::saveBlob(p, KEY, blob);
	p.end();
	return ok;
}

// ******************** Migration from the layout before 2026-10 ********************
// Namespaces ST_<type> (times as i32 per driving state, DIST_FREE, SPEED_MAX, CAD_MREVS,
// CAD_MS) and DIST_0_<type> (total, lost_total, revs). Can go once no device is left that
// still carries them -- /debug/nvs shows it.

static String legacyStatNamespace(uint8_t c) {
	return String(NVS_STAT_PREFIX) + Statistics::SUM_TYPE_STRING[c];
}

static String legacyDistNamespace(uint8_t c) {
	return String(NVS_STAT_PREFIX "DIST_0_") + (Statistics::SUM_TYPE_STRING[c] + 3);		// +3 skips "ST_"
}

bool migrateLegacy(Blob& blob) {
	Blob b = {};
	b.version = VERSION;
	bool found = false;
	for (uint8_t c = 0; c < SUMMARIES; c++) {
		Summary s = {};
		Preferences p;
		if (NvsUtil::namespaceExists(legacyStatNamespace(c).c_str()) && p.begin(legacyStatNamespace(c).c_str(), true)) {
			found = true;
			for (uint8_t d = 0; d < STATES; d++) {
				s.timeMs[d] = p.getLong(Statistics::PREF_TIME_STRING[d], 0);
			}
			s.distFree = NvsUtil::getFloat(p, "DIST_FREE", 0);
			s.speedMax = NvsUtil::getFloat(p, "SPEED_MAX", 0);
			s.cadMilliRevs = p.getULong64("CAD_MREVS", 0);
			s.cadMs = p.getULong64("CAD_MS", 0);
			p.end();
		}
		if (NvsUtil::namespaceExists(legacyDistNamespace(c).c_str()) && p.begin(legacyDistNamespace(c).c_str(), true)) {
			found = true;
			s.distTotal = NvsUtil::getFloat(p, "total", 0);
			s.distLost = NvsUtil::getFloat(p, "lost_total", 0);
			if (p.isKey("revs")) {
				s.revs = p.getULong("revs", 0);
				b.revsKnownMask |= 1 << c;
			}
			p.end();
		}
		b.sum[c] = s;
	}
	if (!found) return false;

	// The old values only go once the new ones are safely in the flash.
	Blob check;
	if (!save(b) || !load(check) || memcmp(&b, &check, sizeof(b)) != 0) {
		bclog.log(BCLogger::Log_Error, BCLogger::TAG_STAT, "❌ Can't store the migrated statistics - old NVS keys stay, next boot tries again");
		blob = b;
		return true;
	}
	for (uint8_t c = 0; c < SUMMARIES; c++) {
		for (const String& ns : {legacyStatNamespace(c), legacyDistNamespace(c)}) {
			Preferences p;
			if (NvsUtil::namespaceExists(ns.c_str()) && p.begin(ns.c_str(), false)) {
				p.clear();
				p.end();
			}
		}
	}
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_STAT, "Statistics moved into one NVS blob (%s/%s), old keys removed", NAMESPACE, KEY);
	blob = b;
	return true;
}

}
