/*
 * WifiConfig.h
 *
 * The WiFi networks the device knows and the access point it can open itself -- the pure
 * data part: no Arduino, no NVS, no locking. WifiWebserver owns one instance, guards it with
 * a mutex and persists it as ONE blob (doc/PITFALLS.md: values that belong together go into
 * the NVS as a struct). Host test: test/native_wificonfig/wificonfig_test.cpp.
 *
 * Autoconnect tries the strongest visible network first (test ride 2026-10-04: at the flat's
 * door "IA216" was far more reliable than "IA216oT", which stood first in the list). The order of
 * the list only decides between networks of the same signal strength and among those that are
 * not seen (hidden). New networks are appended, the web page reorders them (the display does not).
 *
 * Passwords never leave this class towards the web or the log: there is no accessor that
 * hands them to anything but the WiFi driver (password()).
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

namespace WifiCfg {

constexpr size_t SSID_MAX = 32;			// IEEE 802.11: 0..32 bytes
constexpr size_t PW_MAX = 63;			// WPA2-PSK passphrase: 8..63 ASCII characters
constexpr size_t PW_MIN = 8;
constexpr size_t MAX_NETWORKS = 8;
// Name of this bike computer in the network: mDNS host (<name>.local) and the default hotspot
// SSID. One per build variant (platformio.ini: the FL build is "TRGB-FL"), so that two bike
// computers in the same WLAN do not collide and the log service can tell them apart.
#ifndef BC_HOSTNAME
#define BC_HOSTNAME "TRGB-BC"
#endif
constexpr const char* HOSTNAME = BC_HOSTNAME;
constexpr const char* DEFAULT_AP_SSID = BC_HOSTNAME;

struct Network {
	char ssid[SSID_MAX + 1];
	char pw[PW_MAX + 1];		// empty = open network
	uint8_t hidden;				// not found by a scan: connect to it blindly
};

struct AccessPoint {
	char ssid[SSID_MAX + 1];
	char pw[PW_MAX + 1];		// always a valid WPA2 passphrase once ensureAp() has run
};

// What goes into the NVS. Bump `version` if a field moves; new fields may use `reserved`.
struct Blob {
	uint8_t version;
	uint8_t count;
	uint8_t reserved[2];
	Network nets[MAX_NETWORKS];
	AccessPoint ap;
};
constexpr uint8_t BLOB_VERSION = 1;

// A scan result as far as selection needs it.
struct Visible {
	const char* ssid;
	int8_t rssi = -127;		// dBm; the default is the same for all, i.e. no preference
};

enum class Result : uint8_t {
	OK = 0,
	ADDED,				// a new entry at the end of the list
	UPDATED,			// the SSID was known, its password (and hidden flag) replaced
	BAD_SSID,
	BAD_PASSWORD,
	FULL,
	BAD_INDEX,
};

class Config {
public:
	Config();

	size_t count() const {return n;}
	const char* ssid(size_t i) const {return i < n ? nets[i].ssid : "";}
	bool hasPassword(size_t i) const {return i < n && nets[i].pw[0];}
	bool hidden(size_t i) const {return i < n && nets[i].hidden;}
	// For the WiFi driver only (WiFi.begin()); never to be logged or sent to a client.
	const char* password(size_t i) const {return i < n ? nets[i].pw : "";}
	int find(const char* ssid) const;			// index or -1

	static bool validSsid(const char* ssid);
	static bool validPassword(const char* pw);	// empty (open) or 8..63 characters

	// Appends, or replaces the password of an SSID that is already stored (its position, i.e.
	// its priority, stays). An empty `pw` for a known SSID keeps the old password, unless
	// `keepPasswordIfEmpty` is false (explicitly turning a network into an open one).
	Result add(const char* ssid, const char* pw, bool hidden = false, bool keepPasswordIfEmpty = true);
	Result remove(size_t i);
	// Moves entry `from` so that it ends up at index `to`; the ones in between shift.
	Result move(size_t from, size_t to);
	void clear() {n = 0; memset_nets();}

	// The access point. ensureAp() fills in the defaults: SSID "TRGB-BC" and a random
	// password (`rnd` = e.g. esp_random). Returns true if something was filled in.
	const AccessPoint& accessPoint() const {return ap;}
	bool ensureAp(uint32_t (*rnd)());
	Result setAccessPoint(const char* ssid, const char* pw);		// pw needs 8..63 characters

	// Autoconnect order: the stored networks that are in `visible` (or are marked hidden),
	// strongest signal first (a network seen several times counts with its best), then the
	// hidden ones that were not seen; equal signals keep the list order. Returns the number of
	// indices written to `out` (<= MAX_NETWORKS).
	size_t candidates(const Visible* visible, size_t nVisible, uint8_t* out) const;

	void toBlob(Blob& b) const;
	// Takes over what is plausible; false if the blob is unusable (the instance stays empty).
	bool fromBlob(const Blob& b);

	static constexpr size_t AP_PW_LEN = 10;

private:
	Network nets[MAX_NETWORKS];
	size_t n = 0;
	AccessPoint ap;
	void memset_nets();
};

}	// namespace WifiCfg
