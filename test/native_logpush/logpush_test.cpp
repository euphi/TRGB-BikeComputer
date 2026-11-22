/*
 * Host test for src/LogPushCore.{h,cpp} -- no hardware, no PlatformIO. Build and run from
 * the repository root:
 *
 *   g++ -std=c++17 -O2 -Wall -Isrc src/LogPushCore.cpp test/native_logpush/logpush_test.cpp -o /tmp/logpush_test && /tmp/logpush_test
 *
 * Exit code 0 = all checks passed.
 */

#include "LogPushCore.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace PushCore;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void testUrls() {
	CHECK(validUrl("https://cloud.example.com/remote.php/dav/files/bike/Push-Inbox/TRGB-FL"), "nextcloud");
	CHECK(validUrl("http://192.168.0.5:8080/api/v1/devices/trgb/files"), "service");
	CHECK(!validUrl("ftp://x/y"), "scheme");
	CHECK(!validUrl("https://"), "no host");
	CHECK(!validUrl("https:///path"), "empty host");
	CHECK(!validUrl("https://h/a b"), "space");
	CHECK(!validUrl("https://h/a?x=1"), "query");
	CHECK(!validUrl(""), "empty");
	CHECK(isHttps("https://h") && !isHttps("http://h"), "https");

	char out[256];
	CHECK(joinUrl("https://h/a/", "20261018/L_143012.bin", out, sizeof(out)) && !strcmp(out, "https://h/a/20261018/L_143012.bin"), "join slash: %s", out);
	CHECK(joinUrl("https://h/a", "L0042.bin", out, sizeof(out)) && !strcmp(out, "https://h/a/L0042.bin"), "join: %s", out);
	CHECK(!joinUrl("https://h/a", "L0042.bin", out, 10), "join too long");

	CHECK(parentUrl("https://h/a/b/c", out, sizeof(out)) && !strcmp(out, "https://h/a/b"), "parent: %s", out);
	CHECK(parentUrl("https://h/a/b/", out, sizeof(out)) && !strcmp(out, "https://h/a"), "parent of folder: %s", out);
	CHECK(parentUrl("https://h/a", out, sizeof(out)) && !strcmp(out, "https://h"), "parent to host: %s", out);
	CHECK(!parentUrl("https://h", out, sizeof(out)), "host has no parent");
	CHECK(!parentUrl("https://h/", out, sizeof(out)), "host/ has no parent");
}

static Blob blobWith(const char* url, const char* user, const char* secret, bool enabled = true) {
	Blob b;
	memset(&b, 0, sizeof(b));
	b.version = BLOB_VERSION;
	b.enabled = enabled;
	snprintf(b.url, sizeof(b.url), "%s", url);
	snprintf(b.user, sizeof(b.user), "%s", user);
	snprintf(b.secret, sizeof(b.secret), "%s", secret);
	return b;
}

// The form as the page sends it: the URL field empty, the user as /push/state showed it.
static ConfigForm form(const char* url, const char* user, const char* secret = "", bool enabled = true,
                       bool clearSecret = false, bool clearUrl = false) {
	return ConfigForm{enabled, url, user, secret, clearSecret, clearUrl};
}

static void testApplyConfig() {
	// loading the page and saving with only "enabled" changed loses nothing
	Blob b = blobWith("https://h/s/AbC12", "", "sharepw");
	applyConfig(b, form("", "", "", false));
	CHECK(!b.enabled && !strcmp(b.url, "https://h/s/AbC12") && !b.user[0] && !strcmp(b.secret, "sharepw"), "toggle enabled (link)");
	applyConfig(b, form("", "", "", true));
	CHECK(b.enabled && !strcmp(b.secret, "sharepw"), "toggle back");
	b = blobWith("https://h/remote.php/dav/files/bike/In", "bike", "apppw");
	applyConfig(b, form("", "bike"));
	CHECK(b.enabled && !strcmp(b.user, "bike") && !strcmp(b.secret, "apppw") && !strcmp(b.url, "https://h/remote.php/dav/files/bike/In"), "toggle (account)");
	// a stored target of the old form: the page shows the token user as "***" and sends it back
	b = blobWith("https://h/public.php/webdav", "TOKEN123", "");
	applyConfig(b, form("", MASKED));
	CHECK(!strcmp(b.user, "TOKEN123") && b.enabled, "legacy: kept while the target stays");
	// A) the token does not survive a change or removal of its target
	b = blobWith("https://h/public.php/webdav", "TOKEN123", "");
	applyConfig(b, form("", MASKED, "", true, false, true));
	CHECK(!b.url[0] && !b.user[0] && !b.enabled, "A: clearurl with ***: user '%s'", b.user);
	b = blobWith("https://h/public.php/webdav", "TOKEN123", "");
	applyConfig(b, form("https://h/s/NewLink1", MASKED));
	CHECK(!strcmp(b.url, "https://h/s/NewLink1") && !b.user[0], "A: new link with ***: user '%s'", b.user);
	b = blobWith("https://h/public.php/webdav", "TOKEN123", "");
	applyConfig(b, form("http://other.example/x", MASKED));
	CHECK(!b.user[0], "A: other server with ***: user '%s'", b.user);
	// clearurl clears the user in any case
	b = blobWith("https://h/remote.php/dav/files/bike/In", "bike", "apppw");
	applyConfig(b, form("", "bike", "", true, false, true));
	CHECK(!b.url[0] && !b.user[0] && !b.secret[0] && !b.enabled, "clearurl: everything of the target goes");
	// a different target or user takes the password with it, unless a new one comes along
	b = blobWith("https://h/remote.php/dav/files/bike/In", "bike", "apppw");
	applyConfig(b, form("https://h/s/AbC12", ""));
	CHECK(!b.secret[0] && !b.user[0], "link instead of account: no password carried over");
	b = blobWith("https://h/remote.php/dav/files/bike/In", "bike", "apppw");
	applyConfig(b, form("https://h/remote.php/dav/files/bike/Other", "bike", "newpw"));
	CHECK(!strcmp(b.secret, "newpw"), "new target with a new password");
	b = blobWith("https://h/remote.php/dav/files/bike/In", "bike", "apppw");
	applyConfig(b, form("", "other"));
	CHECK(!strcmp(b.user, "other") && !b.secret[0], "other user: password gone");
	b = blobWith("https://h/remote.php/dav/files/bike/In", "bike", "apppw");
	applyConfig(b, form("", ""));
	CHECK(!b.user[0] && !b.secret[0], "a POST without user clears the user, and with it the password");
	b = blobWith("https://h/remote.php/dav/files/bike/In", "bike", "apppw");
	applyConfig(b, form("", "bike", "", true, true));
	CHECK(!b.secret[0] && !strcmp(b.user, "bike"), "clear = 1 removes only the password");
	// no target: enabling stays off
	b = blobWith("", "", "", false);
	applyConfig(b, form("", "", "", true));
	CHECK(!b.enabled, "enabled without a target");
}

static void testShareLinks() {
	char base[256], tok[TOKEN_MAX + 1];
	CHECK(shareLink("https://cloud.example.org/s/AbCdEf123456789", base, sizeof(base), tok, sizeof(tok))
	      && !strcmp(base, "https://cloud.example.org/public.php/webdav") && !strcmp(tok, "AbCdEf123456789"), "link: %s %s", base, tok);
	CHECK(shareLink("https://h/cloud/index.php/s/AbC12/", base, sizeof(base), tok, sizeof(tok))
	      && !strcmp(base, "https://h/cloud/public.php/webdav") && !strcmp(tok, "AbC12"), "index.php + subdir: %s", base);
	CHECK(!shareLink("https://h/s/AbC12/download", base, sizeof(base), tok, sizeof(tok)), "more after token");
	CHECK(!shareLink("https://h/remote.php/dav/files/u/In", base, sizeof(base), tok, sizeof(tok)), "dav url");
	char shown[256];
	maskUrl("https://cloud.example.org/s/AbC12", shown, sizeof(shown));
	CHECK(!strcmp(shown, "https://cloud.example.org/s/***"), "mask link: %s", shown);
	maskUrl("https://h/cloud/index.php/s/AbC12/", shown, sizeof(shown));
	CHECK(!strcmp(shown, "https://h/cloud/index.php/s/***"), "mask link, index.php, slash: %s", shown);
	maskUrl("https://bike:Pw9@h/remote.php/dav/files/bike/In", shown, sizeof(shown));
	CHECK(!strcmp(shown, "https://***@h/remote.php/dav/files/bike/In"), "mask userinfo: %s", shown);
	maskUrl("https://h/public.php/dav/files/AbC12/sub", shown, sizeof(shown));
	CHECK(!strcmp(shown, "https://h/public.php/dav/files/***"), "mask dav path: %s", shown);
	CHECK(userIsSecret("https://h/public.php/webdav") && !userIsSecret("https://h/remote.php/dav/files/bike/In") && !userIsSecret("https://h/s/AbC12"), "user is secret");
	CHECK(!validUrl("https://bike:Pw9@h/x") && !validUrl("https://h/public.php/webdav") && !validUrl("https://h/public.php/dav/files/AbC12")
	      && validUrl("https://h/s/AbC12") && validUrl("https://h/remote.php/dav/files/me@x.org/In"), "validUrl refuses userinfo and public.php");
	maskUrl("https://h/remote.php/dav/files/bike/In", shown, sizeof(shown));
	CHECK(!strcmp(shown, "https://h/remote.php/dav/files/bike/In"), "plain folder stays: %s", shown);
	maskUrl("http://srv:8080/api/v1/devices/TRGB-BC/files", shown, sizeof(shown));
	CHECK(!strcmp(shown, "http://srv:8080/api/v1/devices/TRGB-BC/files"), "service url stays: %s", shown);
	maskUrl("https://h/s/AbC12", shown, 12);
	CHECK(strlen(shown) == 11 && !strstr(shown, "AbC12"), "truncated: %s", shown);
	maskUrl("https://h/s/my-bike-link", shown, sizeof(shown));
	CHECK(!strcmp(shown, "https://h/s/***"), "custom token: %s", shown);
	maskUrl("https://h/s/x/index.php/s/TOKEN123", shown, sizeof(shown));
	CHECK(!strcmp(shown, "https://h/s/***"), "second marker: %s", shown);
	maskUrl("https://bike:Pw@Rest9@h/In", shown, sizeof(shown));
	CHECK(!strcmp(shown, "https://***@h/In"), "last @: %s", shown);
	maskUrl("", shown, sizeof(shown));
	CHECK(!shown[0], "no target stays empty: '%s'", shown);
	CHECK(!validUrl("https://h/s/***") && !validUrl("https://h/***"), "a masked URL is not a URL");
	maskUrl("gopher://x/s/AbC12", shown, sizeof(shown));
	CHECK(!strcmp(shown, "***"), "no scheme: %s", shown);
	CHECK(!shareLink("https://h/s/", base, sizeof(base), tok, sizeof(tok)), "no token");
	char name[128];
	CHECK(dropName("TRGB-BC", "20261008", "L_204725.bin", name, sizeof(name)) && !strcmp(name, "TRGB-BC~20261008~L_204725.bin"), "drop: %s", name);
	CHECK(dropName("TRGB-BC", "", "L0042.bin", name, sizeof(name)) && !strcmp(name, "TRGB-BC~~L0042.bin"), "drop loose: %s", name);
}

static void testNames() {
	CHECK(pushable("L_143012.bin", 10), "L");
	CHECK(pushable("R_143012_01.bin", 10), "R");
	CHECK(pushable("L0042.bin", 10), "legacy");
	CHECK(!pushable("L_143012.bin", 0), "empty file");
	CHECK(!pushable("xL_143012.bin", 10), "hidden");
	CHECK(!pushable("L_143012", 10), "no extension");
	CHECK(!pushable("L-1.bin", 10), "dash");
	CHECK(!pushable("L_1.toolong", 10), "long extension");
	CHECK(!pushable("_L.bin", 10), "not a letter first");
	CHECK(validDay("20261018") && validDay("NO_TIME"), "days");
	CHECK(!validDay("CUR") && !validDay("") && !validDay("a/b") && !validDay("12345678901234567"), "bad days");

	char rel[REL_MAX + 1];
	CHECK(relPath("20261018", "R_143012_01.bin", rel, sizeof(rel)) && !strcmp(rel, "20261018/R_143012_01.bin"), "rel: %s", rel);
	CHECK(relPath("", "L0042.bin", rel, sizeof(rel)) && !strcmp(rel, "L0042.bin"), "rel loose: %s", rel);
	CHECK(!relPath("1234567890123456", "L_12345678901234567890123456789.bin", rel, sizeof(rel)), "rel too long");
}

static void testManifest() {
	std::vector<Entry> mem(4);
	Manifest m;
	m.attach(mem.data(), mem.size());
	CHECK(m.parseLine("20261018/L_143012.bin 1234\n"), "line");
	CHECK(m.parseLine("20261017/I_090000.txt 99\r\n"), "line crlf");
	CHECK(m.parseLine("20261018/L_143012.bin 2000"), "later line wins");
	CHECK(!m.parseLine("garbage"), "no size");
	CHECK(!m.parseLine("x 12a"), "bad size");
	CHECK(!m.parseLine(" 12"), "no path");
	CHECK(m.count() == 2, "count %u", (unsigned) m.count());
	CHECK(!strcmp(m.at(0).path, "20261017/I_090000.txt"), "sorted");

	CHECK(!m.needsPush("20261018/L_143012.bin", 2000), "same size");
	CHECK(m.needsPush("20261017/I_090000.txt", 100), "size changed");
	CHECK(m.needsPush("20261019/L_080000.bin", 5), "new");
	CHECK(m.seenCount() == 2, "seen %u", (unsigned) m.seenCount());

	CHECK(m.set("20261019/L_080000.bin", 5), "add");
	CHECK(m.set("A.bin", 1), "add 4");
	CHECK(!m.set("B.bin", 1), "full");
	CHECK(m.set("A.bin", 2) && m.at(m.find("A.bin")).size == 2, "update when full");
	for (size_t i = 1; i < m.count(); i++) CHECK(strcmp(m.at(i - 1).path, m.at(i).path) < 0, "order at %u", (unsigned) i);

	char line[64];
	CHECK(Manifest::formatLine("20261018/L_143012.bin", 4294967295u, line, sizeof(line)) && !strcmp(line, "20261018/L_143012.bin 4294967295\n"), "format: %s", line);
	CHECK(!Manifest::formatLine("20261018/L_143012.bin", 1, line, 10), "format too long");

	CHECK(m.set("20261019/L_080000.bin", 6, true) && m.at(m.find("20261019/L_080000.bin")).seen, "set seen");
	CHECK(!m.at(m.find("A.bin")).seen, "set without seen");
	CHECK(!m.needsCompaction(10), "few lines");
	CHECK(m.needsCompaction(m.seenCount() + COMPACT_SLACK), "many dead lines");
}

int main() {
	testUrls();
	testShareLinks();
	testApplyConfig();
	testNames();
	testManifest();
	if (failures) {
		printf("%d check(s) failed\n", failures);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
