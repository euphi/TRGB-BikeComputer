/*
 * NvsUtil.h
 *
 * Values that belong together go into the NVS as one struct (a blob): it is replaced
 * atomically, and it costs 2 + size/32 entries instead of one per integer and three per
 * float. That matters because every NVS page erase stalls the bus the panel reads its
 * framebuffer from -- see doc/PITFALLS.md, "NVS-Schreiben und Display-Flackern".
 */

#pragma once

#include <Preferences.h>
#include <nvs.h>

namespace NvsUtil {

// Quiet probe: Preferences::begin(name, true) logs an [E] line for a missing namespace.
inline bool namespaceExists(const char* name) {
	nvs_handle_t h;
	if (nvs_open(name, NVS_READONLY, &h) != ESP_OK) return false;
	nvs_close(h);
	return true;
}

// Loads a blob of exactly sizeof(T); `out` stays untouched otherwise. Probes with
// getType() first, getBytes()/getBytesLength() log an [E] line for a missing key.
template <typename T>
bool loadBlob(Preferences& p, const char* key, T& out) {
	if (p.getType(key) != PT_BLOB || p.getBytesLength(key) != sizeof(T)) return false;
	T tmp;
	if (p.getBytes(key, &tmp, sizeof(T)) != sizeof(T)) return false;
	out = tmp;
	return true;
}

// Preferences::getFloat() logs an [E] line for a missing key (a float is a blob to the NVS).
inline float getFloat(Preferences& p, const char* key, float defaultValue) {
	return p.isKey(key) ? p.getFloat(key, defaultValue) : defaultValue;
}

template <typename T>
bool saveBlob(Preferences& p, const char* key, const T& in) {
	return p.putBytes(key, &in, sizeof(T)) == sizeof(T);
}

// Keys left behind by code that is gone (found on /debug/nvs, 2026-10). Checked at every
// boot; once they are removed that is a lookup in RAM, nothing is written.
inline void removeStaleKeys() {
	static const struct {const char* ns; const char* key;} STALE[] = {
		{"PrettyOTA", nullptr},			// library removed -- the whole namespace
		{"LogBackfill", "done"},
	};
	for (const auto& s : STALE) {
		Preferences p;
		if (!namespaceExists(s.ns) || !p.begin(s.ns, false)) continue;
		if (!s.key) {
			p.clear();
		} else if (p.isKey(s.key)) {
			p.remove(s.key);
		}
		p.end();
	}
}

}
