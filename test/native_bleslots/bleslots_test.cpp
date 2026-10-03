/*
 * Host test for src/BleSlots.h -- no hardware, no PlatformIO. Build and run from the
 * repository root:
 *
 *   g++ -std=c++17 -O2 -Wall -Isrc test/native_bleslots/bleslots_test.cpp -o /tmp/bleslots_test && /tmp/bleslots_test
 *
 * Exit code 0 = all checks passed.
 */

#include "BleSlots.h"

#include <cstdio>

using namespace BleSlots;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// Same order as BLEDevices::EDevType
enum {HRM, CSC1, CSC2, FL, NAV, COUNT};

static const uint8_t A[ADDR_LEN] = {0xA0, 1, 2, 3, 4, 5};
static const uint8_t B[ADDR_LEN] = {0xB0, 1, 2, 3, 4, 5};
static const uint8_t C[ADDR_LEN] = {0xC0, 1, 2, 3, 4, 5};

// The slots as BLEDevices keeps them, with what filterDevice() does with a choice
struct Slots {
	const uint8_t* stored[COUNT] = {};

	Choice advertise(const uint8_t* addr, bool csc = true) {
		bool candidate[COUNT] = {};
		if (csc) {candidate[CSC1] = candidate[CSC2] = true;} else {candidate[HRM] = true;}
		Choice c = choose(stored, candidate, COUNT, addr);
		if (c.learn) stored[c.slot] = addr;
		return c;
	}
};

static void testLearn() {
	printf("Learning\n");
	Slots s;
	Choice c = s.advertise(A);
	CHECK(c.slot == CSC1 && c.learn, "first sensor is learned as CSC1 (slot %d)", c.slot);
	c = s.advertise(A);
	CHECK(c.slot == CSC1 && !c.learn, "known sensor keeps CSC1 (slot %d)", c.slot);
	c = s.advertise(B);
	CHECK(c.slot == CSC2 && c.learn, "second sensor is learned as CSC2 (slot %d)", c.slot);
	c = s.advertise(C);
	CHECK(c.slot == NO_SLOT && !c.learn, "third sensor is refused (slot %d)", c.slot);
	CHECK(s.stored[CSC1] == A && s.stored[CSC2] == B, "a refused sensor changes nothing");
	c = s.advertise(C, false);
	CHECK(c.slot == HRM && c.learn && s.stored[CSC1] == A && s.stored[CSC2] == B, "another kind of device takes its own slot");
}

// The known bug: the first sensor is removed while the second one is stored (and connected)
static void testFirstRemoved() {
	printf("CSC1 removed, CSC2 stays\n");
	Slots s;
	s.advertise(A);
	s.advertise(B);
	s.stored[CSC1] = nullptr;

	Choice c = s.advertise(B);
	CHECK(c.slot == CSC2 && !c.learn, "the remaining sensor stays CSC2 (slot %d, learn %d)", c.slot, c.learn);
	CHECK(s.stored[CSC1] == nullptr, "... and is not learned a second time as CSC1");
	c = s.advertise(C);
	CHECK(c.slot == CSC1 && c.learn, "a new sensor takes the freed CSC1 (slot %d)", c.slot);
	c = s.advertise(B);
	CHECK(c.slot == CSC2 && !c.learn, "the remaining sensor still is CSC2 (slot %d)", c.slot);
	c = s.advertise(A);
	CHECK(c.slot == NO_SLOT, "the removed sensor is refused once its slot is taken (slot %d)", c.slot);
}

static void testSecondRemoved() {
	printf("CSC2 removed\n");
	Slots s;
	s.advertise(A);
	s.advertise(B);
	s.stored[CSC2] = nullptr;
	Choice c = s.advertise(A);
	CHECK(c.slot == CSC1 && !c.learn, "the remaining sensor stays CSC1 (slot %d)", c.slot);
	c = s.advertise(B);
	CHECK(c.slot == CSC2 && c.learn, "the removed sensor is learned again while its slot is free (slot %d)", c.slot);
}

// Addresses are compared by value: BLEDevices hands over a copy from the advertisement
static void testByValue() {
	printf("Compared by value\n");
	Slots s;
	s.advertise(A);
	uint8_t copy[ADDR_LEN];
	memcpy(copy, A, ADDR_LEN);
	Choice c = s.advertise(copy);
	CHECK(c.slot == CSC1 && !c.learn, "same bytes, other pointer (slot %d, learn %d)", c.slot, c.learn);
	copy[5]++;
	c = s.advertise(copy);
	CHECK(c.slot == CSC2 && c.learn, "the last byte counts (slot %d)", c.slot);
}

int main() {
	testLearn();
	testFirstRemoved();
	testSecondRemoved();
	testByValue();
	printf(failures ? "%d check(s) FAILED\n" : "all checks passed\n", failures);
	return failures ? 1 : 0;
}
