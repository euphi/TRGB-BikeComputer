/*
 * Host test for src/RouteOverview.{h,cpp} -- no hardware, no PlatformIO. Build and run from
 * the repository root:
 *
 *   g++ -std=c++17 -O2 -Wall -Isrc src/RouteOverview.cpp test/native_routeoverview/routeoverview_test.cpp -o /tmp/routeoverview_test && /tmp/routeoverview_test
 *
 * Exit code 0 = all checks passed. The test vector is the 78-byte example of PROTOCOL.md
 * ("Streckenübersicht-Service", "Beispiel"); TrailBridge's OverviewFrameEncoderTest checks the
 * same bytes on the other side.
 */

#include "RouteOverview.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace RouteOv;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const uint8_t DOC_FRAME[] = {
	0x01, 0x01,
	0x01, 0x01, 0x03,
	0x02, 0x01, 0x02,
	0x03, 0x01, 0x02,
	0x04, 0x0D, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5A, 0x69, 0x65, 0x6C,
	0x04, 0x10, 0x00, 0x24, 0x77, 0x00, 0x00, 0xD4, 0x17, 0x00, 0x00, 0x42, 0xC3, 0xA4, 0x63, 0x6B, 0x65, 0x72,
	0x05, 0x0B, 0x02, 0xC2, 0x01, 0x08, 0x20, 0x00, 0x00, 0xF0, 0x55, 0x00, 0x00,
	0x04, 0x13, 0x00, 0xE0, 0x2E, 0x00, 0x00, 0x60, 0x09, 0x00, 0x00, 0x57, 0x65, 0x67, 0x70, 0x75, 0x6E, 0x6B, 0x74, 0x20, 0x32,
};

// ---- a frame builder, for the cases the document does not show ----
struct Frame {
	std::vector<uint8_t> b{OVERVIEW_PROTOCOL_VERSION, OVERVIEW_MSG_OVERVIEW};
	void tlv(uint8_t tag, const std::vector<uint8_t>& v) {
		b.push_back(tag);
		b.push_back(static_cast<uint8_t>(v.size()));
		b.insert(b.end(), v.begin(), v.end());
	}
	void head(uint8_t rev, uint8_t wp, uint8_t climbs) {
		tlv(OVERVIEW_TAG_REVISION, {rev});
		tlv(OVERVIEW_TAG_WAYPOINTS_AHEAD, {wp});
		tlv(OVERVIEW_TAG_CLIMBS_TOTAL, {climbs});
	}
	static void put32(std::vector<uint8_t>& v, uint32_t x) {for (int i = 0; i < 4; i++) v.push_back((x >> (8 * i)) & 0xFF);}
	void waypoint(bool dest, uint32_t atM, uint32_t timeAtS, const char* name) {
		std::vector<uint8_t> v{static_cast<uint8_t>(dest ? OVERVIEW_WAYPOINT_FLAG_DESTINATION : 0)};
		put32(v, atM);
		put32(v, timeAtS);
		for (const char* p = name; *p; p++) v.push_back(static_cast<uint8_t>(*p));
		tlv(OVERVIEW_TAG_WAYPOINT, v);
	}
	void climb(uint8_t number, uint16_t gain, uint32_t length, uint32_t footAt) {
		std::vector<uint8_t> v{number, static_cast<uint8_t>(gain & 0xFF), static_cast<uint8_t>(gain >> 8)};
		put32(v, length);
		put32(v, footAt);
		tlv(OVERVIEW_TAG_CLIMB, v);
	}
};

static Overview ov;
static View view;

static void testDocumentFrame() {
	printf("document frame\n");
	CHECK(sizeof(DOC_FRAME) == 78, "vector is %u byte", static_cast<unsigned>(sizeof(DOC_FRAME)));
	CHECK(parseFrame(DOC_FRAME, sizeof(DOC_FRAME), ov) == FRAME_OVERVIEW, "parse");
	CHECK(ov.revision == 3 && ov.waypointsAhead == 2 && ov.climbsTotal == 2, "head %u/%u/%u", ov.revision, ov.waypointsAhead, ov.climbsTotal);
	CHECK(ov.hasDestination && !strcmp(ov.destination.name, "Ziel") && ov.destination.atM == 0 && ov.destination.timeAtS == 0, "destination");
	CHECK(ov.count == 3, "%u entries", ov.count);
	CHECK(ov.entries[0].kind == KIND_WAYPOINT && !strcmp(ov.entries[0].name, "B\xC3\xA4" "cker") && ov.entries[0].atM == 30500 && ov.entries[0].timeAtS == 6100,
			"Bäcker %s %lu %lu", ov.entries[0].name, static_cast<unsigned long>(ov.entries[0].atM), static_cast<unsigned long>(ov.entries[0].timeAtS));
	CHECK(ov.entries[1].kind == KIND_CLIMB && ov.entries[1].number == 2 && ov.entries[1].gainM == 450 && ov.entries[1].lengthM == 8200 && ov.entries[1].atM == 22000,
			"climb %u %u %lu %lu", ov.entries[1].number, ov.entries[1].gainM, static_cast<unsigned long>(ov.entries[1].lengthM), static_cast<unsigned long>(ov.entries[1].atM));
	CHECK(ov.entries[2].kind == KIND_WAYPOINT && !strcmp(ov.entries[2].name, "Wegpunkt 2") && ov.entries[2].atM == 12000 && ov.entries[2].timeAtS == 2400, "Wegpunkt 2");
}

static void testView() {
	printf("distance and time\n");
	Tracker t;
	t.onNavFrame(true, 3, true, 36000, 7200, 1000000);
	CHECK(t.feedFrame(DOC_FRAME, sizeof(DOC_FRAME)) == FRAME_OVERVIEW, "feed");
	CHECK(t.buildView(36000, view), "view");
	CHECK(view.valid && view.revision == 3 && view.navEpoch == 1000000, "head");
	CHECK(view.hasDestination && view.destination.destination && view.destination.distM == 36000 && view.destination.timeS == 7200,
			"destination %ld m %lu s", static_cast<long>(view.destination.distM), static_cast<unsigned long>(view.destination.timeS));
	CHECK(view.count == 3, "%u rows", view.count);
	CHECK(view.rows[0].distM == 5500 && view.rows[0].timeS == 1100, "Bäcker %ld m %lu s", static_cast<long>(view.rows[0].distM), static_cast<unsigned long>(view.rows[0].timeS));
	CHECK(view.rows[1].kind == KIND_CLIMB && view.rows[1].distM == 14000 && view.rows[1].summitM == 22200 && view.rows[1].timeS == TIME_UNKNOWN, "climb foot %ld summit %ld", static_cast<long>(view.rows[1].distM), static_cast<long>(view.rows[1].summitM));
	CHECK(view.rows[2].distM == 24000 && view.rows[2].timeS == 4800, "Wegpunkt 2 %ld m %lu s", static_cast<long>(view.rows[2].distM), static_cast<unsigned long>(view.rows[2].timeS));
	CHECK(!view.shortened, "complete list");
	CHECK(view.waypointsAhead == 2 && view.waypointsLeft == 2, "2 waypoints ahead: %u/%u", view.waypointsAhead, view.waypointsLeft);

	// Carried on by the wheel between two nav frames: the distance moves, the time stays that of the frame
	CHECK(t.buildView(35900.4f, view), "view");
	CHECK(view.rows[0].distM == 5400 && view.rows[0].timeS == 1100, "after 100 m: %ld m %lu s", static_cast<long>(view.rows[0].distM), static_cast<unsigned long>(view.rows[0].timeS));

	// The Bäcker is reached: gone from the list, the climb is next
	CHECK(t.buildView(30000, view), "view");
	CHECK(view.count == 2 && view.rows[0].kind == KIND_CLIMB && view.rows[0].distM == 8000, "after the Bäcker: %u rows, first %ld", view.count, static_cast<long>(view.rows[0].distM));
	CHECK(view.waypointsLeft == 1, "one waypoint reached: %u left", view.waypointsLeft);

	// In the climb: the foot is behind (<= 0), the summit still ahead; its end drops it
	CHECK(t.buildView(20000, view), "view");
	CHECK(view.count == 2 && view.rows[0].kind == KIND_CLIMB && view.rows[0].distM == -2000 && view.rows[0].summitM == 6200, "in the climb: %ld / %ld", static_cast<long>(view.rows[0].distM), static_cast<long>(view.rows[0].summitM));
	CHECK(t.buildView(13800, view), "view");
	CHECK(view.count == 1 && view.rows[0].kind == KIND_WAYPOINT, "summit reached: %u rows", view.count);
	CHECK(t.buildView(11000, view) && view.waypointsLeft == 0, "both waypoints reached: %u left", view.waypointsLeft);

	// Destination arrived: never negative
	CHECK(t.buildView(0, view) && view.destination.distM == 0 && view.destination.timeS == 7200, "destination at 0 m: %ld", static_cast<long>(view.destination.distM));
	CHECK(view.count == 0, "nothing ahead at the destination");

	// Time anchors unknown (GPX without timestamps): spread over the distance
	Frame f;
	f.head(5, 2, 0);
	f.waypoint(true, 0, OVERVIEW_TIME_UNKNOWN, "Ziel");
	f.waypoint(false, 20000, OVERVIEW_TIME_UNKNOWN, "Halbzeit");
	Tracker u;
	u.onNavFrame(true, 5, true, 40000, 8000, 0);
	CHECK(u.feedFrame(f.b.data(), f.b.size()) == FRAME_OVERVIEW, "feed");
	CHECK(u.buildView(40000, view) && view.count == 1 && view.rows[0].distM == 20000 && view.rows[0].timeS == 4000, "spread: %lu s", static_cast<unsigned long>(view.rows[0].timeS));
	CHECK(view.destination.timeS == 8000, "destination time %lu", static_cast<unsigned long>(view.destination.timeS));
	// 64 bit: 4,000,000,000 s x 2,000,000,000 m does not fit into 32 bit
	Tracker big;
	big.onNavFrame(true, 5, true, 2000000000u, 4000000000u, 0);
	big.feedFrame(f.b.data(), f.b.size());
	CHECK(big.buildView(2000000000.0f, view) && view.count == 1 && view.rows[0].timeS == 3999960000u, "64 bit: %lu s", static_cast<unsigned long>(view.rows[0].timeS));
	// Garbage does not wrap around into the list: a distance beyond 2^31 is clamped
	Tracker huge;
	huge.onNavFrame(true, 5, true, 4000000000u, 100, 0);
	huge.feedFrame(f.b.data(), f.b.size());
	CHECK(huge.buildView(4000000000.0f, view) && view.count == 1 && view.rows[0].distM == INT32_MAX, "clamped: %ld", static_cast<long>(view.rows[0].distM));
	Tracker zero;
	zero.onNavFrame(true, 5, true, 0, 0, 0);
	zero.feedFrame(f.b.data(), f.b.size());
	CHECK(zero.buildView(0, view) && view.count == 0, "zero remaining distance");

	// No nav frame yet: nothing to show
	Tracker nonav;
	nonav.feedFrame(DOC_FRAME, sizeof(DOC_FRAME));
	CHECK(!nonav.buildView(1000, view) && !view.valid, "no nav frame");
}

static void testRobustness() {
	printf("robustness\n");
	// A last TLV that is cut off keeps what came before
	CHECK(parseFrame(DOC_FRAME, 70, ov) == FRAME_OVERVIEW && ov.count == 2 && ov.hasDestination, "cut at 70: %u entries", ov.count);
	CHECK(parseFrame(DOC_FRAME, 11, ov) == FRAME_OVERVIEW && ov.count == 0 && !ov.hasDestination && ov.revision == 3, "head only");
	CHECK(parseFrame(DOC_FRAME, 4, ov) == FRAME_INVALID, "revision cut off: no revision");
	// Other frames
	const uint8_t hello[] = {1, 0};
	const uint8_t none[] = {1, 2};
	const uint8_t wrongVersion[] = {2, 1, 1, 1, 1};
	const uint8_t wrongType[] = {1, 9};
	const uint8_t shortFrame[] = {1};
	CHECK(parseFrame(hello, 2, ov) == FRAME_HELLO, "hello");
	CHECK(parseFrame(none, 2, ov) == FRAME_NONE, "none");
	CHECK(parseFrame(wrongVersion, sizeof(wrongVersion), ov) == FRAME_INVALID, "version");
	CHECK(parseFrame(wrongType, 2, ov) == FRAME_INVALID, "type");
	CHECK(parseFrame(shortFrame, 1, ov) == FRAME_INVALID, "short");
	CHECK(parseFrame(nullptr, 0, ov) == FRAME_INVALID, "empty");

	// Unknown tags are skipped; a climb longer than 11 byte is read up to 11
	Frame f;
	f.head(7, 1, 1);
	f.tlv(0x63, {1, 2, 3});
	f.waypoint(true, 0, 0, "Ziel");
	{
		std::vector<uint8_t> v{1, 0x64, 0x00};
		Frame::put32(v, 500);
		Frame::put32(v, 1234);
		v.push_back(0xAA);
		v.push_back(0xBB);
		f.tlv(OVERVIEW_TAG_CLIMB, v);
	}
	f.tlv(OVERVIEW_TAG_WAYPOINT, {0, 1, 2});		// too short: ignored
	CHECK(parseFrame(f.b.data(), f.b.size(), ov) == FRAME_OVERVIEW && ov.count == 1 && ov.entries[0].gainM == 100 && ov.entries[0].atM == 1234, "unknown tag, long climb");

	// No revision: invalid, and what was there is gone
	Frame g;
	g.tlv(OVERVIEW_TAG_WAYPOINTS_AHEAD, {1});
	g.waypoint(true, 0, 0, "Ziel");
	CHECK(parseFrame(g.b.data(), g.b.size(), ov) == FRAME_INVALID && ov.revision == 0 && ov.count == 0 && !ov.hasDestination, "no revision");

	// Names: control characters, a cut-off UTF-8 character, trailing spaces, all 32 byte
	Frame n;
	n.head(1, 4, 0);
	n.waypoint(true, 0, 0, "Ziel");
	n.waypoint(false, 900, 0, "A\tB\nC");
	n.waypoint(false, 800, 0, "Kiosk   ");
	n.waypoint(false, 700, 0, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\xC3\xA4");		// 31 x a + ä = 33 byte: the cut would split the ä
	n.waypoint(false, 600, 0, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\xC3\xA4");		// 30 x a + ä = 32 byte: fits
	CHECK(parseFrame(n.b.data(), n.b.size(), ov) == FRAME_OVERVIEW && ov.count == 4, "names parse");
	CHECK(!strcmp(ov.entries[0].name, "A B C"), "control characters: '%s'", ov.entries[0].name);
	CHECK(!strcmp(ov.entries[1].name, "Kiosk"), "trailing spaces: '%s'", ov.entries[1].name);
	CHECK(strlen(ov.entries[2].name) == 31 && ov.entries[2].name[30] == 'a', "cut character dropped: %u byte", static_cast<unsigned>(strlen(ov.entries[2].name)));
	CHECK(strlen(ov.entries[3].name) == 32 && static_cast<uint8_t>(ov.entries[3].name[30]) == 0xC3, "whole character kept: %u byte", static_cast<unsigned>(strlen(ov.entries[3].name)));

	// More entries than fit: the near ones are kept, the rest counted
	Frame m;
	m.head(2, 255, 0);
	m.waypoint(true, 0, 0, "Ziel");
	for (int i = 0; i < MAX_ENTRIES + 5; i++) m.waypoint(false, 100000 - i * 100, OVERVIEW_TIME_UNKNOWN, "x");
	CHECK(parseFrame(m.b.data(), m.b.size(), ov) == FRAME_OVERVIEW && ov.count == MAX_ENTRIES && ov.dropped == 5, "overflow: %u kept, %u dropped", ov.count, ov.dropped);
	CHECK(ov.entries[0].atM == 100000, "nearest kept");

	// The screen gets MAX_VIEW_ROWS at most, nearest first, and is told that the list is cut
	Tracker t;
	t.onNavFrame(true, 2, true, 200000, 40000, 0);
	t.feedFrame(m.b.data(), m.b.size());
	CHECK(t.buildView(200000, view) && view.count == MAX_VIEW_ROWS && view.shortened, "row cap: %u rows, shortened %d", view.count, view.shortened);
	CHECK(view.rows[0].distM == 100000 && view.rows[MAX_VIEW_ROWS - 1].distM == 100000 + (MAX_VIEW_ROWS - 1) * 100, "nearest first");
	// A full overview that fits is not "shortened"
	Frame small;
	small.head(3, 2, 0);
	small.waypoint(true, 0, 0, "Ziel");
	small.waypoint(false, 900, 0, "a");
	small.waypoint(false, 800, 0, "b");
	Tracker s;
	s.onNavFrame(true, 3, true, 1000, 200, 0);
	s.feedFrame(small.b.data(), small.b.size());
	CHECK(s.buildView(1000, view) && view.count == 2 && !view.shortened, "small list complete");
}

static void testShortened() {
	printf("shortened list\n");
	Tracker t;
	t.onNavFrame(true, 4, true, 50000, 10000, 0);
	Frame f;
	f.head(4, 5, 4);									// 5 waypoints and 4 climbs on the route ...
	f.waypoint(true, 0, 0, "Ziel");
	f.waypoint(false, 30000, 3000, "A");
	f.climb(2, 100, 1000, 25000);						// ... only climb 2 and 3 are listed after climb 1 was ridden
	f.climb(3, 100, 1000, 15000);
	t.feedFrame(f.b.data(), f.b.size());
	t.buildView(50000, view);
	CHECK(view.shortened, "4 waypoints and climb 4 are missing");

	Frame c;
	c.head(5, 1, 4);
	c.waypoint(true, 0, 0, "Ziel");
	c.waypoint(false, 30000, 3000, "A");
	c.climb(3, 100, 1000, 25000);
	c.climb(4, 100, 1000, 15000);						// the last climb of the route is listed: complete
	t.onNavFrame(true, 5, true, 50000, 10000, 0);
	t.feedFrame(c.b.data(), c.b.size());
	t.buildView(50000, view);
	CHECK(!view.shortened, "complete");

	Frame d;
	d.head(6, 0, 4);									// every climb ridden, nothing listed: not a cut
	d.waypoint(true, 0, 0, "Ziel");
	t.onNavFrame(true, 6, true, 5000, 1000, 0);
	t.feedFrame(d.b.data(), d.b.size());
	t.buildView(5000, view);
	CHECK(!view.shortened && view.count == 0, "all climbs ridden");
}

static void testRevisions() {
	printf("revisions\n");
	Tracker t;
	uint8_t rev = 0;
	CHECK(!t.readWanted(0, rev), "nothing announced");
	t.onNavFrame(true, 3, true, 36000, 7200, 0);
	CHECK(t.readWanted(100, rev) && rev == 3, "revision 3 announced, none held");
	t.readAttempted(100);
	CHECK(!t.readWanted(1000, rev), "backing off after an attempt");
	CHECK(t.readWanted(100 + Tracker::RETRY_MS, rev) && rev == 3, "retry after the pause");
	t.onNavFrame(true, 4, true, 36000, 7200, 0);
	CHECK(t.readWanted(1200, rev) && rev == 4, "a newer revision is read at once");

	Frame f;
	f.head(4, 0, 0);
	f.waypoint(true, 0, 0, "Ziel");
	const uint32_t v0 = t.version();
	CHECK(t.feedFrame(f.b.data(), f.b.size()) == FRAME_OVERVIEW && t.revision() == 4 && t.version() != v0, "stored");
	CHECK(!t.readWanted(99999, rev), "held revision matches");
	t.onNavFrame(true, 4, true, 35000, 7000, 0);
	CHECK(!t.readWanted(99999, rev), "still matches");

	// The read gave a newer revision than the nav frame knew: one more read is asked for, backed off,
	// and settled by the next nav frame
	Frame g;
	g.head(5, 0, 0);
	g.waypoint(true, 0, 0, "Ziel");
	t.onNavFrame(true, 4, true, 35000, 7000, 0);
	t.feedFrame(g.b.data(), g.b.size());
	CHECK(t.revision() == 5 && t.readWanted(200000, rev) && rev == 4, "nav frame is behind");
	t.readAttempted(200000);
	CHECK(!t.readWanted(200100, rev), "backed off");
	t.onNavFrame(true, 5, true, 34000, 6800, 0);
	CHECK(!t.readWanted(300000, rev), "nav frame caught up");

	// Navigation from OsmAnd (no tag): the overview goes
	const uint32_t v1 = t.version();
	t.onNavFrame(false, 0, true, 34000, 6800, 0);
	CHECK(!t.hasOverview() && t.version() != v1 && !t.readWanted(400000, rev), "tag missing drops the overview");
	CHECK(!t.buildView(34000, view), "no view");

	// OVERVIEW_NONE, and the connection gone
	t.onNavFrame(true, 6, true, 34000, 6800, 0);
	Frame h;
	h.head(6, 0, 0);
	h.waypoint(true, 0, 0, "Ziel");
	t.feedFrame(h.b.data(), h.b.size());
	CHECK(t.hasOverview(), "held");
	const uint8_t none[] = {1, 2};
	CHECK(t.feedFrame(none, 2) == FRAME_NONE && !t.hasOverview(), "OVERVIEW_NONE");
	t.feedFrame(h.b.data(), h.b.size());
	t.onGone();
	CHECK(!t.hasOverview() && !t.navValid() && !t.readWanted(500000, rev), "gone");

	// A broken frame does not touch a good overview
	Tracker k;
	k.onNavFrame(true, 3, true, 36000, 7200, 0);
	k.feedFrame(DOC_FRAME, sizeof(DOC_FRAME));
	const uint8_t junk[] = {9, 9, 9};
	const uint32_t vk = k.version();
	CHECK(k.feedFrame(junk, 3) == FRAME_INVALID && k.hasOverview() && k.version() == vk, "junk ignored");
	const uint8_t hello[] = {1, 0};
	CHECK(k.feedFrame(hello, 2) == FRAME_HELLO && k.hasOverview(), "hello ignored");
}

int main() {
	testDocumentFrame();
	testView();
	testRobustness();
	testShortened();
	testRevisions();
	if (failures) {
		printf("%d check(s) failed\n", failures);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
