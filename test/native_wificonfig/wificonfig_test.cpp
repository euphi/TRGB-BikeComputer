/*
 * Host test for src/WifiConfig.{h,cpp} -- no hardware, no PlatformIO. Build and run from
 * the repository root:
 *
 *   g++ -std=c++17 -O2 -Wall -Isrc src/WifiConfig.cpp test/native_wificonfig/wificonfig_test.cpp -o /tmp/wc_test && /tmp/wc_test
 *
 * Exit code 0 = all checks passed.
 */

#include "WifiConfig.h"

#include <cstdio>
#include <cstring>

using namespace WifiCfg;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t counter = 0;
static uint32_t fakeRnd() {return counter++ * 7919u;}

static void order() {
	Config c;
	CHECK(c.add("home", "password1") == Result::ADDED, "add");
	CHECK(c.add("phone", "hotspot123") == Result::ADDED, "add");
	CHECK(c.add("cafe", "") == Result::ADDED, "open network");
	CHECK(c.count() == 3 && !strcmp(c.ssid(2), "cafe") && !c.hasPassword(2), "appended in order");

	CHECK(c.move(2, 0) == Result::OK && !strcmp(c.ssid(0), "cafe") && !strcmp(c.ssid(1), "home") && !strcmp(c.ssid(2), "phone"), "move up");
	CHECK(c.move(0, 2) == Result::OK && !strcmp(c.ssid(0), "home") && !strcmp(c.ssid(1), "phone") && !strcmp(c.ssid(2), "cafe"), "move down");
	CHECK(c.move(1, 1) == Result::OK && !strcmp(c.ssid(1), "phone"), "move onto itself");
	CHECK(c.move(0, 3) == Result::BAD_INDEX && c.move(5, 0) == Result::BAD_INDEX, "move out of range");

	CHECK(c.remove(1) == Result::OK && c.count() == 2 && !strcmp(c.ssid(1), "cafe"), "remove");
	CHECK(c.remove(2) == Result::BAD_INDEX, "remove out of range");
	CHECK(c.find("phone") == -1 && c.find("cafe") == 1, "find");
}

static void addRules() {
	Config c;
	CHECK(c.add("", "password1") == Result::BAD_SSID, "empty ssid");
	CHECK(c.add("0123456789012345678901234567890123", "password1") == Result::BAD_SSID, "33 bytes");
	CHECK(c.add("01234567890123456789012345678901", "password1") == Result::ADDED, "32 bytes fit");
	CHECK(c.add("a", "short") == Result::BAD_PASSWORD, "WPA2 needs 8");
	CHECK(c.add("b", "12345678901234567890123456789012345678901234567890123456789012345") == Result::BAD_PASSWORD, "65 chars");

	// A known SSID keeps its position; an empty password keeps the old one.
	c.clear();
	c.add("one", "password1"); c.add("two", "password2");
	CHECK(c.add("one", "newpassword") == Result::UPDATED && c.count() == 2 && !strcmp(c.password(0), "newpassword"), "update");
	CHECK(c.add("one", "") == Result::UPDATED && !strcmp(c.password(0), "newpassword"), "empty keeps the password");
	CHECK(c.add("one", "", false, false) == Result::UPDATED && !c.hasPassword(0), "explicitly open");
	CHECK(c.add("two", "bad") == Result::BAD_PASSWORD && !strcmp(c.password(1), "password2"), "rejected update changes nothing");

	c.clear();
	for (size_t i = 0; i < MAX_NETWORKS; i++) {
		char s[8]; snprintf(s, sizeof(s), "n%zu", i);
		CHECK(c.add(s, "password1") == Result::ADDED, "fill %zu", i);
	}
	CHECK(c.add("one-too-many", "password1") == Result::FULL, "full");
	CHECK(c.add("n3", "password9") == Result::UPDATED, "update when full");
}

static void removeWipesSlot() {
	Config c;
	c.add("a", "secretsecret");
	c.remove(0);
	Blob b;
	c.toBlob(b);
	CHECK(b.count == 0 && b.nets[0].pw[0] == 0 && b.nets[0].ssid[0] == 0, "no password left in the unused slot");
}

static void candidates() {
	Config c;
	c.add("home", "password1");
	c.add("phone", "password2");
	c.add("hidden-one", "password3", true);
	c.add("cafe", "");
	uint8_t out[MAX_NETWORKS];

	// Same signal everywhere: list order among the seen ones, the hidden one that was not seen last
	const Visible all[] = {{"cafe"}, {"phone"}, {"neighbour"}, {"home"}};
	size_t k = c.candidates(all, 4, out);
	CHECK(k == 4 && out[0] == 0 && out[1] == 1 && out[2] == 3 && out[3] == 2, "equal signal: list order, unseen hidden last (k=%zu)", k);

	const Visible some[] = {{"cafe"}, {"phone"}};
	k = c.candidates(some, 2, out);
	CHECK(k == 3 && out[0] == 1 && out[1] == 3 && out[2] == 2, "visible + hidden only (k=%zu)", k);

	// The strongest network first, whatever the list says (flat door 2026-10-04: "home" weak, "cafe" strong)
	const Visible strength[] = {{"home", -80}, {"cafe", -45}, {"phone", -60}};
	k = c.candidates(strength, 3, out);
	CHECK(k == 4 && out[0] == 3 && out[1] == 1 && out[2] == 0 && out[3] == 2, "strongest first (k=%zu out=%d%d%d%d)", k, out[0], out[1], out[2], out[3]);

	// Seen twice (two access points): the better one counts
	const Visible twice[] = {{"home", -85}, {"phone", -70}, {"home", -50}};
	k = c.candidates(twice, 3, out);
	CHECK(k == 3 && out[0] == 0 && out[1] == 1 && out[2] == 2, "best of several access points (k=%zu)", k);

	// A seen hidden network competes with its signal like any other
	const Visible seenHidden[] = {{"hidden-one", -40}, {"home", -70}};
	k = c.candidates(seenHidden, 2, out);
	CHECK(k == 2 && out[0] == 2 && out[1] == 0, "seen hidden network sorted by signal (k=%zu)", k);

	k = c.candidates(nullptr, 0, out);
	CHECK(k == 1 && out[0] == 2, "nothing visible: only the hidden one is tried (k=%zu)", k);

	const Visible unnamed[] = {{nullptr}, {"home"}};
	k = c.candidates(unnamed, 2, out);
	CHECK(k == 2 && out[0] == 0 && out[1] == 2, "null ssid in the scan is skipped (k=%zu)", k);
}

static void accessPoint() {
	Config c;
	CHECK(c.ensureAp(fakeRnd), "first call fills in the defaults");
	CHECK(!strcmp(c.accessPoint().ssid, WifiCfg::DEFAULT_AP_SSID), "default ssid");
	CHECK(!strcmp(WifiCfg::DEFAULT_AP_SSID, BC_HOSTNAME), "default ssid is the host name of the build");
	CHECK(strlen(c.accessPoint().pw) == Config::AP_PW_LEN && Config::validPassword(c.accessPoint().pw), "generated password is a valid passphrase");
	for (const char* p = c.accessPoint().pw; *p; p++) CHECK(!strchr("0oOl1iI", *p), "ambiguous character '%c'", *p);
	char before[PW_MAX + 1];
	strcpy(before, c.accessPoint().pw);
	CHECK(!c.ensureAp(fakeRnd) && !strcmp(before, c.accessPoint().pw), "second call changes nothing");

	CHECK(c.setAccessPoint("MyBike", "12345678") == Result::OK && !strcmp(c.accessPoint().ssid, "MyBike"), "set");
	CHECK(c.setAccessPoint("MyBike", "1234567") == Result::BAD_PASSWORD, "AP needs a password (no open AP)");
	CHECK(c.setAccessPoint("MyBike", "") == Result::BAD_PASSWORD, "empty AP password");
	CHECK(c.setAccessPoint("", "12345678") == Result::BAD_SSID, "empty AP ssid");
	CHECK(!strcmp(c.accessPoint().pw, "12345678"), "failed set changes nothing");

	// a stored password that is too short (older/garbled blob) is replaced
	Config d;
	d.setAccessPoint("X", "12345678");
	Blob b; d.toBlob(b);
	strcpy(b.ap.pw, "123");
	Config e;
	CHECK(e.fromBlob(b) && e.ensureAp(fakeRnd) && strlen(e.accessPoint().pw) == Config::AP_PW_LEN && !strcmp(e.accessPoint().ssid, "X"), "short AP password regenerated, ssid kept");
}

static void blob() {
	Config c;
	c.add("home", "password1"); c.add("cafe", ""); c.add("hid", "password3", true);
	c.ensureAp(fakeRnd);
	Blob b;
	c.toBlob(b);
	CHECK(b.version == BLOB_VERSION && b.count == 3, "header");

	Config d;
	CHECK(d.fromBlob(b), "load");
	CHECK(d.count() == 3 && !strcmp(d.ssid(0), "home") && !strcmp(d.password(0), "password1") && !d.hasPassword(1) && d.hidden(2) && !d.hidden(0), "round trip");
	CHECK(!strcmp(d.accessPoint().ssid, c.accessPoint().ssid) && !strcmp(d.accessPoint().pw, c.accessPoint().pw), "ap round trip");

	Blob bad = b;
	bad.version = 99;
	Config e;
	CHECK(!e.fromBlob(bad) && e.count() == 0, "unknown version rejected");
	bad = b; bad.count = MAX_NETWORKS + 1;
	CHECK(!e.fromBlob(bad), "count out of range rejected");

	// garbage in an entry: unterminated ssid, empty ssid, bad password
	bad = b;
	memset(bad.nets[0].ssid, 'x', sizeof(bad.nets[0].ssid));		// no terminator
	bad.nets[1].ssid[0] = 0;
	strcpy(bad.nets[2].pw, "x");
	Config f;
	CHECK(f.fromBlob(bad) && f.count() == 1 && strlen(f.ssid(0)) == SSID_MAX, "implausible entries dropped, ssid terminated (count=%zu)", f.count());
}

static void hostname() {
	CHECK(validHostname("TRGB-BC") && validHostname("TRGB-FL") && validHostname("pendler2"), "plain names");
	CHECK(validHostname(DEFAULT_HOSTNAME), "the build's default is valid");
	CHECK(!validHostname("") && !validHostname(nullptr), "empty");
	CHECK(!validHostname("-bc") && !validHostname("bc-"), "hyphen at the ends");
	CHECK(!validHostname("my bc") && !validHostname("bc.local") && !validHostname("bc_1") && !validHostname("Räder"),
	      "only letters, digits and hyphens");
	CHECK(validHostname("a234567890123456789012345678901") && !validHostname("a2345678901234567890123456789012"),
	      "at most HOSTNAME_MAX characters");
}

int main() {
	hostname();
	order();
	addRules();
	removeWipesSlot();
	candidates();
	accessPoint();
	blob();
	printf(failures ? "%d FAILED\n" : "all wificonfig checks passed\n", failures);
	return failures ? 1 : 0;
}
