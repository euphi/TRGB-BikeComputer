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
	CHECK(validUrl("https://cloud.example.com/remote.php/dav/files/bike/BikeLog-Inbox/TRGB-FL"), "nextcloud");
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

static void testShareLinks() {
	char base[256], tok[TOKEN_MAX + 1];
	CHECK(shareLink("https://nc.1337.cx/s/cDNFwtdPRx7jsRA", base, sizeof(base), tok, sizeof(tok))
	      && !strcmp(base, "https://nc.1337.cx/public.php/webdav") && !strcmp(tok, "cDNFwtdPRx7jsRA"), "link: %s %s", base, tok);
	CHECK(shareLink("https://h/cloud/index.php/s/AbC12/", base, sizeof(base), tok, sizeof(tok))
	      && !strcmp(base, "https://h/cloud/public.php/webdav") && !strcmp(tok, "AbC12"), "index.php + subdir: %s", base);
	CHECK(!shareLink("https://h/s/AbC12/download", base, sizeof(base), tok, sizeof(tok)), "more after token");
	CHECK(!shareLink("https://h/remote.php/dav/files/u/In", base, sizeof(base), tok, sizeof(tok)), "dav url");
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
	testNames();
	testManifest();
	if (failures) {
		printf("%d check(s) failed\n", failures);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
