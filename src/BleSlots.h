/*
 * BleSlots.h
 *
 * Pure (no Arduino, no BLE stack) part of BLEDevices: which slot an advertised device
 * belongs to. Several slots can take the same kind of device -- CSC1 and CSC2 both take a
 * speed/cadence sensor -- and each slot remembers the address of its sensor.
 *
 * A sensor whose address is stored keeps that slot, wherever the slot is. Only an unknown
 * sensor is learned, in the first free slot of its kind. The slots don't depend on each
 * other: CSC1 may be free while CSC2 is taken.
 *
 * Host test: test/native_bleslots/bleslots_test.cpp (build command in its header).
 */

#pragma once

#include <cstdint>
#include <cstring>

namespace BleSlots {

static constexpr int ADDR_LEN = 6;
static constexpr int NO_SLOT = -1;

struct Choice {
	int slot = NO_SLOT;
	bool learn = false;		// the slot was free: store the address in it
};

// stored[i]: address stored for slot i, nullptr = free. candidate[i]: slot i takes this kind
// of device.
inline Choice choose(const uint8_t* const stored[], const bool candidate[], int count, const uint8_t* addr) {
	Choice c;
	for (int i = 0; i < count; i++) {
		if (candidate[i] && stored[i] && memcmp(stored[i], addr, ADDR_LEN) == 0) {
			c.slot = i;
			return c;
		}
	}
	for (int i = 0; i < count; i++) {
		if (candidate[i] && !stored[i]) {
			c.slot = i;
			c.learn = true;
			return c;
		}
	}
	return c;
}

}
