/*
 * WifiConfig.cpp -- see WifiConfig.h.
 */

#include "WifiConfig.h"

#include <string.h>

namespace WifiCfg {

// Only characters that cannot be mixed up when typed from the display (no 0/o, 1/l/i).
static const char AP_PW_ALPHABET[] = "abcdefghjkmnpqrstuvwxyz23456789";

static void copyStr(char* dst, size_t cap, const char* src) {
	const size_t len = src ? strnlen(src, cap) : 0;
	if (len) memcpy(dst, src, len);
	dst[len] = '\0';
}

Config::Config() {
	memset_nets();
	memset(&ap, 0, sizeof(ap));
}

void Config::memset_nets() {
	memset(nets, 0, sizeof(nets));
}

int Config::find(const char* s) const {
	for (size_t i = 0; i < n; i++) {
		if (strcmp(nets[i].ssid, s) == 0) return (int) i;
	}
	return -1;
}

bool Config::validSsid(const char* s) {
	if (!s) return false;
	const size_t len = strlen(s);
	return len >= 1 && len <= SSID_MAX;
}

bool Config::validPassword(const char* pw) {
	if (!pw) return false;
	const size_t len = strlen(pw);
	return len == 0 || (len >= PW_MIN && len <= PW_MAX);
}

Result Config::add(const char* s, const char* pw, bool hid, bool keepPasswordIfEmpty) {
	if (!validSsid(s)) return Result::BAD_SSID;
	if (!pw) pw = "";
	const int known = find(s);
	if (known >= 0) {
		Network& net = nets[known];
		if (pw[0] || !keepPasswordIfEmpty) {
			if (!validPassword(pw)) return Result::BAD_PASSWORD;
			copyStr(net.pw, PW_MAX, pw);
		}
		net.hidden = hid;
		return Result::UPDATED;
	}
	if (!validPassword(pw)) return Result::BAD_PASSWORD;
	if (n >= MAX_NETWORKS) return Result::FULL;
	Network& net = nets[n++];
	memset(&net, 0, sizeof(net));
	copyStr(net.ssid, SSID_MAX, s);
	copyStr(net.pw, PW_MAX, pw);
	net.hidden = hid;
	return Result::ADDED;
}

Result Config::remove(size_t i) {
	if (i >= n) return Result::BAD_INDEX;
	for (size_t k = i; k + 1 < n; k++) nets[k] = nets[k + 1];
	n--;
	memset(&nets[n], 0, sizeof(nets[n]));		// don't leave the password behind in the unused slot
	return Result::OK;
}

Result Config::move(size_t from, size_t to) {
	if (from >= n || to >= n) return Result::BAD_INDEX;
	if (from == to) return Result::OK;
	const Network moved = nets[from];
	if (from < to) {
		for (size_t k = from; k < to; k++) nets[k] = nets[k + 1];
	} else {
		for (size_t k = from; k > to; k--) nets[k] = nets[k - 1];
	}
	nets[to] = moved;
	return Result::OK;
}

bool Config::ensureAp(uint32_t (*rnd)()) {
	bool changed = false;
	if (!validSsid(ap.ssid)) {
		copyStr(ap.ssid, SSID_MAX, DEFAULT_AP_SSID);
		changed = true;
	}
	if (strlen(ap.pw) < PW_MIN) {
		const size_t alphabet = sizeof(AP_PW_ALPHABET) - 1;
		for (size_t i = 0; i < AP_PW_LEN; i++) ap.pw[i] = AP_PW_ALPHABET[rnd() % alphabet];
		ap.pw[AP_PW_LEN] = '\0';
		changed = true;
	}
	return changed;
}

Result Config::setAccessPoint(const char* s, const char* pw) {
	if (!validSsid(s)) return Result::BAD_SSID;
	if (!pw || strlen(pw) < PW_MIN || strlen(pw) > PW_MAX) return Result::BAD_PASSWORD;
	copyStr(ap.ssid, SSID_MAX, s);
	copyStr(ap.pw, PW_MAX, pw);
	return Result::OK;
}

size_t Config::candidates(const Visible* visible, size_t nVisible, uint8_t* out) const {
	size_t k = 0;
	for (size_t i = 0; i < n; i++) {
		bool take = nets[i].hidden != 0;
		for (size_t v = 0; !take && v < nVisible; v++) {
			take = visible[v].ssid && strcmp(visible[v].ssid, nets[i].ssid) == 0;
		}
		if (take) out[k++] = (uint8_t) i;
	}
	return k;
}

void Config::toBlob(Blob& b) const {
	memset(&b, 0, sizeof(b));
	b.version = BLOB_VERSION;
	b.count = (uint8_t) n;
	memcpy(b.nets, nets, sizeof(nets));
	b.ap = ap;
}

bool Config::fromBlob(const Blob& b) {
	if (b.version != BLOB_VERSION || b.count > MAX_NETWORKS) return false;
	clear();
	for (size_t i = 0; i < b.count; i++) {
		// Terminate defensively and drop entries that could never connect.
		char s[SSID_MAX + 1], pw[PW_MAX + 1];
		copyStr(s, SSID_MAX, b.nets[i].ssid);
		copyStr(pw, PW_MAX, b.nets[i].pw);
		if (add(s, pw, b.nets[i].hidden != 0) != Result::ADDED) continue;
	}
	copyStr(ap.ssid, SSID_MAX, b.ap.ssid);
	copyStr(ap.pw, PW_MAX, b.ap.pw);
	return true;
}

}	// namespace WifiCfg
