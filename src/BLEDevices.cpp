/*
 * BLEDevices.cpp
 *
 *  Created on: 18.02.2023
 *      Author: ian
 */

#include <BLEDevices.h>
#include "WebPage.h"
#include <BLEDevice.h>

#include <Arduino.h>

#include "Singletons.h"
#include "Stats/Distance.h"
#include "BikeNavProtocol.h"
#include "BikeGpsProtocol.h"
#include "ClockSync.h"
#include "BleSlots.h"

#include <task.h>

// HRM, CSC, FL, TrailBridge Navigation (see ../TrailBridge/PROTOCOL.md)
const BLEUUID BLEDevices::serviceUUID[DEV_COUNT] = { BLEUUID((uint16_t)0x180D), BLEUUID((uint16_t)0x1816), BLEUUID((uint16_t)0x1816), BLEUUID("e62efa94-afa8-11ed-afa1-0242ac120002"), BLEUUID("f7ac2b76-986b-45fd-8e44-f116a61f319d")};
const BLEUUID BLEDevices::serviceUUIDBat = BLEUUID((uint16_t) 0x180F);
const BLEUUID BLEDevices::serviceUUIDExposure = BLEUUID((uint16_t) 0xFD6F);
const BLEUUID BLEDevices::charUUID[DEV_COUNT] = { BLEUUID((uint16_t)0x2A37), BLEUUID((uint16_t)0x2A5B), BLEUUID((uint16_t)0x2A5B), BLEUUID("e62efe40-afa8-11ed-afa1-0242ac120002"), BLEUUID("7473da02-2de8-4f48-9e46-21b36380c176")};
const BLEUUID BLEDevices::charUUIDBat = BLEUUID((uint16_t) 0x2A19);

// GPS-Positions-Service (see ../TrailBridge/PROTOCOL.md) -- second, independent, unadvertised
// service on the same peer as DEV_NAV, discovered via GATT service discovery after connecting.
const BLEUUID BLEDevices::gpsServiceUUID = BLEUUID("66b5835c-9be6-43d1-b24a-f9337c0fcb7f");
const BLEUUID BLEDevices::gpsCharUUID = BLEUUID("10c49e7b-4808-4d63-9b68-9ba6c385db0d");

// Elevation-profile service (see ../TrailBridge/PROTOCOL.md "Höhenprofil-Service") -- third
// service on the DEV_NAV peer, not advertised either. Event-driven, no heartbeat.
const BLEUUID BLEDevices::profileServiceUUID = BLEUUID("3c1f6a90-5b2e-4d7a-9c48-e0a1b7d25f63");
const BLEUUID BLEDevices::profileCharUUID = BLEUUID("a84e0d17-6f3b-4c52-8e9d-1b70c2f4a596");

const char* BLEDevices::DEV_EMOJI[DEV_COUNT] = {"❤️","🚴","🚴","⚡", "🧭"};
const char* BLEDevices::DEV_STRING[DEV_COUNT] = {"HeartRate","CSC1","CSC2","Forumslader", "TrailBridge"};
const char* BLEDevices::CONN_STRING[CONN_COUNT] = {"Not Found","Advertised (not yet connected)","Connected","Lost"};

const uint8_t twoByteOn[] = {0x01,0x00};

/**
 * @brief Compares two BLE addresses by their 6 address bytes only.
 *
 * Deliberately NOT BLEAddress::equals(): since Arduino-ESP32 3.x that also compares
 * m_addrType, which NimBLE reports inconsistently for the same sensor (PUBLIC 0 vs
 * PUBLIC_ID 2 once the identity address is resolved) -- the type is not a stable identity
 * marker. To actually connect, the type is taken live from the advertisement
 * (BLEClient::connect(BLEAdvertisedDevice*)), never from the stored address.
 *
 * Taken by value because getNative() is non-const while getAddress()/getPeerAddress()
 * return temporaries; a BLEAddress is 7 bytes, so the copy is free.
 */
static bool sameAddress(BLEAddress a, BLEAddress b) {
	return memcmp(a.getNative(), b.getNative(), ESP_BD_ADDR_LEN) == 0;
}

//std::function<void(BLEScanResults)> scanCB;

BLEDevices::BLEDevices()
{
	xDevMutex = xSemaphoreCreateMutex();
}

void BLEDevices::setup() {
	  BLEDevice::init("TRGB_BTTacho BLE");
	  // Task names must stay under configMAX_TASK_NAME_LEN (16), i.e. 15 chars max.
	  // xTaskCreate() silently truncates past that, but xTaskGetHandle() asserts on an
	  // over-long query string and aborts -- which is how the old 21-char
	  // "BLEScanUndConnectTask" crashed the stack-watermark report in WebInstrument.cpp.
	  // 4096 byte (ESP-IDF counts byte): with 3072 only 268 byte were left once TrailBridge
	  // was connected -- its connect runs the service discovery for both services (nav + GPS)
	  // in this task.
	  xTaskCreate(+[](void* thisInstance){((BLEDevices*)thisInstance)->scanAndConnectTask();}, "BLEScanConnect", 4096, this, 5, &scanTaskHandle);

//	  scanCB = [this](BLEScanResults result) {
//		  bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "🔵 ✔️ BLE scan completed: %d devices found.", result.getCount());
//		  this->scanning=false;
//	  };
	  //##SCANTASK-Removed: startBLEScan();
	  restoreAdresses();
}

/*
 * New (Feb 2025) Scan and connect task.
 * -----------------------------------------
 *
 * The BLE Stack in Arduino-ESP32 is quite tricky, as it mixes blocking and non-blocking methods. Internally it uses semaphores to access the BLE functions
 * of the ESP32-IDF. Unfortunately, it is quite easy to create a deadlock. For example, trying to connect while scan is still active, results in one.
 * Therefore I use one (and only one) task to do scanning and connecting.
 *
 * Basic scheme:
 *
 * 1. Start scanning
 *
 * 2. Store results (this could be done async, but there is no benefit, if we need to wait for the scan to end. (Scan could be stopped, but this may result in longer total time to connect all stored devices)
 *
 * 3. Connect to stored results and store connected clients.
 *
 * 4. Wait some time
 *
 * 5. Check all devices if still connected and delete clients that have been disconnected.
 *
 * 6. If not all devices are connected, goto 1, else 4.
 */
void BLEDevices::scanAndConnectTask() {
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "👨‍🏭 Start");

    TickType_t lastBatteryCheck = xTaskGetTickCount();  // Initialize battery check timer


	do {
		checkNavAlive();
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "👨‍🏭 --> Scanning");
		// Blocks, but does not directly access data structures -> don't use Mutex
		startBLEScan();
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "👨‍🏭 --> Scan finished & stored");
		// 3. ---------- Connect to stored results and store connected clients. ----------

		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "👨‍🏭 --> Connecting");

		// total time of the loop including should not exceed 30sec, because some CSC devices switch themself off again
		// after been activated by movement - and not get active again till standstill and movement again.
		TickType_t waitTime = 20 * 1000 / portTICK_PERIOD_MS;  // Default wait time: 20 sec
		TickType_t now = xTaskGetTickCount();

		// Mutex only for clients, so inside connectToServer()
		// Take Mutex - I really hope that there can't be a disconnect callback while connecting
		TickType_t start = xTaskGetTickCount();

		if (connectDevices.size()) {
			for (auto& dev : connectDevices) {connectToServer(dev);}
		} else {
			bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "Nothing to connect");
		}
		bclog.log(BCLogger::Log_Debug, BCLogger::TAG_BLE, "🗑️ - Delete advertised list");
		connectDevices.clear();
		TickType_t duration = xTaskGetTickCount() - start;

		// 4. --------- Wait some time ---------
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "👨‍🏭 --- Check batteries (every 5 Min) and Wait ---");


		if ((now - lastBatteryCheck) > (5 * 60 * 1000 / portTICK_PERIOD_MS)) {  // Every 5 minutes
			TickType_t start = xTaskGetTickCount();
			checkBatteries();  // This may block, so measure its execution time
			duration += xTaskGetTickCount() - start;

			// Ensure wait time stays within the 30s loop constraint
			waitTime = std::max(TickType_t(0), waitTime - duration);

			lastBatteryCheck = xTaskGetTickCount();  // Reset battery check timer
		}

		vTaskDelay(waitTime);  // Adjusted wait time

		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "👨‍🏭 --> Connection Check");

		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "👨‍🏭 --> Check for new re-scan");

	} while (true);
}

void BLEDevices::startBLEScan() {
	if (!pBLEScan) {
		pBLEScan = BLEDevice::getScan(); //create new scan
		pBLEScan->setAdvertisedDeviceCallbacks(this);
		pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
		pBLEScan->setInterval(100);
		pBLEScan->setWindow(99);  // less or equal setInterval value
	} else {
		//pBLEScan->stop(); not necessary, scan is called blocking
		pBLEScan->clearResults();
	}
	bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "🔵 Start BLE scan");
	pBLEScan->start(scanTime); // No callback -> sync (blocking) scan
	bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "🏁 BLE scan finished");
}


void BLEDevices::onResult(BLEAdvertisedDevice advertisedDevice) {	// Call by value: advertisedDevice gehört Euch (liegt aber eh auf dem Stack)

	// Quick ignore of Exposure Notification
	if (advertisedDevice.getServiceUUID().equals(serviceUUIDExposure)) {
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "💤 Exposure UUID of device %s ", advertisedDevice.getAddress().toString().c_str());
		return;
	}

//	if ( (bclog.getLogLevel(BCLogger::TAG_BLE, false) == BCLogger::Log_Debug ) || (bclog.getLogLevel(BCLogger::TAG_BLE, true) == BCLogger::Log_Debug)) {
	if (bclog.checkLogLevel(BCLogger::Log_Debug, BCLogger::TAG_BLE)) {

		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "🔵 New Device: %s", advertisedDevice.getName().c_str());
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "         RSSI: %ddb", advertisedDevice.getRSSI());
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "      Address: %s", advertisedDevice.getAddress().toString().c_str());
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, " ServiceCount: %d", advertisedDevice.getServiceUUIDCount());
		for (uint8_t c = 0; c < advertisedDevice.getServiceUUIDCount(); c++) {
			BLEUUID uuid = advertisedDevice.getServiceUUID(c);
			bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "        ID: %s", uuid.toString().c_str());
		}
	}

	EDevType dt = filterDevice(advertisedDevice);
	if (dt > DEV_UNKNOWN) {
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "✅ found Service @ %s [%s] - store to connect", advertisedDevice.getName().c_str(),  advertisedDevice.getAddress().toString().c_str());
	    connectDevices.emplace_back(SDevToConnect{std::unique_ptr<BLEAdvertisedDevice>(new BLEAdvertisedDevice(advertisedDevice)), dt});
	}
}

void BLEDevices::onConnect(BLEClient *pClient) {
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "☑️ Connect %s", pClient->getPeerAddress().toString().c_str());

}


void BLEDevices::onDisconnect(BLEClient *pClient) {
    if (!pClient) return;  // Safety check

    BLEAddress discAddr = pClient->getPeerAddress();
    bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "🛑 Disconnect %s", discAddr.toString().c_str());
	if (xSemaphoreTake(xDevMutex, static_cast<TickType_t>(500 / portTICK_PERIOD_MS)) == pdTRUE) {
		// Find the client in the array
		auto it = std::find_if(clients.begin(), clients.end(), [&](const std::unique_ptr<BLEClient> &client) {
			return client && sameAddress(client->getPeerAddress(), discAddr);
		});
		// Remove the client if found
		if (it != clients.end()) {
			EDevType dt = static_cast<EDevType>(std::distance(clients.begin(), it));  // Get index
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "%s diconnected. Remove Client for %s.", DEV_EMOJI[dt], DEV_STRING[dt]);
			it->reset();  // Delete the client and set pointer to nullptr
			connState[dt] = CONN_LOST;
			updateDisconnectedDev(dt);

		} else {
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "Disconnected Client not found in array.");
		}
		xSemaphoreGive(xDevMutex);
	} else {
		bclog.logf(BCLogger::Log_Error, BCLogger::TAG_BLE, "❌⚠️❌⚠️❌ Mutex blocked while trying to find disconnected device ❌⚠️❌⚠️❌");
	}
}

BLEDevices::EDevType BLEDevices::filterDevice(BLEAdvertisedDevice& dev) {
	if (isAlreadyConnected(dev)) {
		bclog.log(BCLogger::Log_Debug, BCLogger::TAG_BLE,  "Filter: Already connected");
		return DEV_UNKNOWN;
	}
	BLEAddress addr = dev.getAddress();
	for (size_t i = 0 ; i < dev.getServiceUUIDCount(); i++) {
		// Several slots take the same service (CSC1/CSC2): a sensor with a stored address
		// keeps its slot, an unknown one is learned in the first free slot (BleSlots.h).
		bool candidate[DEV_COUNT];
		int16_t kind = DEV_UNKNOWN;
		for (uint16_t d = DEV_HRM ; d < DEV_COUNT ; d++) {
			candidate[d] = dev.getServiceUUID(i).equals(serviceUUID[d]);
			if (candidate[d] && kind == DEV_UNKNOWN) kind = d;
		}
		if (kind == DEV_UNKNOWN) continue;
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "Filter: Found %s", DEV_EMOJI[kind]);

		// resetAdress() frees a slot from the web server's task
		if (xSemaphoreTake(xDevMutex, static_cast<TickType_t>(500 / portTICK_PERIOD_MS)) != pdTRUE) {
			bclog.logf(BCLogger::Log_Error, BCLogger::TAG_BLE, "❌⚠️❌⚠️❌ Mutex blocked while choosing the slot for %s ❌⚠️❌⚠️❌", addr.toString().c_str());
			return DEV_UNKNOWN;
		}
		// TrailBridge: Android changes its address, so none is remembered, not even until the
		// next restart -- the phone would be refused when it comes back after a lost
		// connection. The slot takes the first phone found while it has no connection. With
		// one, any other TrailBridge address is refused: the phone keeps advertising while
		// connected, possibly under a new address already.
		if (candidate[DEV_NAV]) {
			const bool queued = std::any_of(connectDevices.begin(), connectDevices.end(), [](const SDevToConnect& c) {return c.devType == DEV_NAV;});
			if (clients[DEV_NAV] || queued) candidate[DEV_NAV] = false;
		}
		const uint8_t* stored[DEV_COUNT];
		for (uint16_t d = 0 ; d < DEV_COUNT ; d++) stored[d] = pStoredAddress[d] ? pStoredAddress[d]->getNative() : nullptr;
		const BleSlots::Choice choice = BleSlots::choose(stored, candidate, DEV_COUNT, addr.getNative());
		if (choice.slot != BleSlots::NO_SLOT) {
			if (choice.learn && choice.slot != DEV_NAV) pStoredAddress[choice.slot] = new BLEAddress(addr);
			connState[choice.slot] = CONN_ADVERTISED;
		}
		xSemaphoreGive(xDevMutex);

		if (choice.slot != BleSlots::NO_SLOT) {
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, choice.learn ? "Filter: Slot %s free --> take address %s and connect" : "Filter: Found stored address of %s: %s --> connect",
			           DEV_STRING[choice.slot], addr.toString().c_str());
			return static_cast<EDevType>(choice.slot);
		}
		// Debug for TrailBridge: every scan, as long as the connected phone advertises under another address
		bclog.logf(kind == DEV_NAV ? BCLogger::Log_Debug : BCLogger::Log_Warn, BCLogger::TAG_BLE, "\t%s no new connection to %s allowed", DEV_EMOJI[kind], DEV_STRING[kind]);
	}
	if (dev.getName().indexOf("ForumsLader") != -1) {
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "\t⚡ Found FL Device");
//		pServerAddress[DEV_FL] = new BLEAddress(advertisedDevice.getAddress());
//		if (connectUnknown || pStoredAddress[DEV_FL] == nullptr || pServerAddress[DEV_FL]->equals(*pStoredAddress[DEV_FL])) {
//			doConnect[DEV_FL] = true;
//			connState[DEV_FL] = CONN_ADVERTISED;
//		} else {
//			bclog.log(BCLogger::Log_Warn, BCLogger::TAG_BLE, "\t⚡ no new connection to FL allowed");
//			delete pServerAddress[DEV_FL];
//			pServerAddress[DEV_FL] = nullptr;
//		}
		return DEV_FL;
	}
	bclog.log(BCLogger::Log_Debug, BCLogger::TAG_BLE, "Filter: 💤 Device not interesting");
	return DEV_UNKNOWN;
}

/**
 * @brief Checks if the advertised BLE device is already connected.
 *
 * This method iterates over the list of connected BLE clients and compares their
 * addresses with the address of the newly advertised device.
 *
 * @param newDevice The advertised BLE device to check.
 * @return true if the device is already connected, false otherwise.
 */
bool BLEDevices::isAlreadyConnected(BLEAdvertisedDevice& newDevice) {
    auto it = std::find_if(clients.begin(), clients.end(),
        [&newDevice](const std::unique_ptr<BLEClient>& client) {
            return client && sameAddress(client->getPeerAddress(), newDevice.getAddress());
        });
    return it != clients.end();  // true, wenn Gerät bereits verbunden ist
}

/**
 * @brief Updates statistics, UI, and internal states after a device disconnects.
 *
 * This method handles device disconnection events by updating relevant components
 * such as statistics and connection states based on the device type.
 *
 * @param dt The type of device that has disconnected.
 */
void BLEDevices::updateDisconnectedDev(const EDevType dt) {
#ifdef BC_SIM
	// The simulator stands in for these (SimSensors.h) -- a real one leaving must not disconnect it
	if (sim.isActive() && (dt == DEV_HRM || dt == DEV_CSC_1 || dt == DEV_CSC_2)) return;
#endif
	switch (dt) {
#ifdef BC_FL_SUPPORT
	case DEV_FL:
		flparser.setConnState(FLClassicParser::FL_STATE_LOST);
		stats.setConnected(false);
		break;
#endif
	case DEV_HRM:
		stats.addHR(-1);
		break;
	case DEV_CSC_1:
	case DEV_CSC_2:
		if (cscIsSpeed[dt]) {
			stats.setConnected(false);
		} else {
			stats.addCadence(-1, 0);
		}
		break;
	case DEV_NAV:
		ui.updateNavi(String(), 0, NAV_MANEUVER_NONE);		// hide stale directions once the phone disconnects
		ui.updateLanes(nullptr, 0, 0, nullptr, 0, 0);
		gpsFix = SGpsFix();		// GPS-Positions-Service shares this connection, so it's gone too
		climb.onRouteGone();	// ... and so is the elevation profile
#ifdef BC_SIM
		sim.endTrailBridgeFeed();
#endif
		break;
	}
}

/**
 * @brief Restores previously stored BLE device addresses from NVS (persistent memory).
 *
 * This method retrieves stored BLE addresses from the non-volatile storage (NVS)
 * and assigns them to the corresponding device slots. If no address is found for a
 * specific device type, a log message is generated.
 */
void BLEDevices::restoreAdresses() {
	Preferences addrPrefs;
	addrPrefs.begin("BLEConn");
	for (uint16_t c = 0; c < DEV_NAV; c++) {
		const char* key = DEV_STRING[c];
		// getType() probes quietly; getBytesLength()/getBytes() emit a log_e on a missing key
		// or an undersized buffer, which CORE_DEBUG_LEVEL=1 would print on every boot.
		PreferenceType pt = addrPrefs.getType(key);
		if (pt == PT_INVALID) {
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "No BLE address stored in preferences for %s", key);
			continue;
		}

		uint8_t raw[ESP_BD_ADDR_LEN];
		if (pt == PT_BLOB && addrPrefs.getBytesLength(key) == ESP_BD_ADDR_LEN
				&& addrPrefs.getBytes(key, raw, sizeof(raw)) == ESP_BD_ADDR_LEN) {
			// Deliberately NOT BLEAddress(uint8_t[6]): under NimBLE that constructor
			// reverse-copies (it expects display order), while storeAdress() wrote the raw
			// native bytes -- the asymmetric pair is what flipped the address on every boot.
			// Writing straight back into getNative() is the exact inverse of the store, in
			// either BT stack. The default constructor zeroes m_address/m_addrType first.
			BLEAddress* pAddr = new BLEAddress();
			memcpy(pAddr->getNative(), raw, ESP_BD_ADDR_LEN);
			pStoredAddress[c] = pAddr;
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "Addr of %s: %s", key, pStoredAddress[c]->toString().c_str());
		} else {
			// Pre-NimBLE 16-byte blob (or one written by the broken NimBLE round-trip -- the
			// two are indistinguishable), so it can't be interpreted: drop it once. The slot
			// is free afterwards, the sensor is re-learned on the next scan and re-stored in
			// the new format. Same effect as the manual /dev/reset?dev=N, just automatic.
			addrPrefs.remove(key);
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "Dropped stale pre-NimBLE address for %s - will be re-learned on next scan", key);
		}
	}
	// Until 2026-10 a sensor stored as CSC2 was learned a second time as CSC1 once CSC1 had
	// been deleted, which left no slot for another sensor. Free the second one.
	if (pStoredAddress[DEV_CSC_1] && pStoredAddress[DEV_CSC_2] && sameAddress(*pStoredAddress[DEV_CSC_1], *pStoredAddress[DEV_CSC_2])) {
		addrPrefs.remove(DEV_STRING[DEV_CSC_2]);
		delete pStoredAddress[DEV_CSC_2];
		pStoredAddress[DEV_CSC_2] = nullptr;
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "%s and %s had the same address stored - %s is free again", DEV_STRING[DEV_CSC_1], DEV_STRING[DEV_CSC_2], DEV_STRING[DEV_CSC_2]);
	}
	addrPrefs.end();
}

/**
 * @brief Stores a BLE device address in NVS (persistent memory).
 *
 * This method saves the BLE address of a given device type to non-volatile storage (NVS)
 * for later retrieval. The function does not store addresses for the TrailBridge device, as
 * Android peripherals typically use a random/rotating address.
 *
 * @param type The device type whose address should be stored.
 * @param addr The BLE address to be stored.
 */
void BLEDevices::storeAdress(EDevType type, BLEAddress &addr) {
	if (type == DEV_NAV) return;	// TrailBridge (Android peripheral) likely uses a random/rotating address
	Preferences addrPrefs;
	addrPrefs.begin("BLEConn");
	// ESP_BD_ADDR_LEN, not 16: m_address is a 6-byte array, so the old length read 10 bytes
	// past the end of the BLEAddress object. restoreAdresses() reverses this exactly.
	size_t rc = addrPrefs.putBytes(DEV_STRING[type], addr.getNative(), ESP_BD_ADDR_LEN);
	bclog.logf(rc > 0 ? BCLogger::Log_Debug : BCLogger::Log_Error, BCLogger::TAG_BLE, "Stored %d bytes to pref %s: %s", rc, DEV_STRING[type],	addr.toString().c_str());
	addrPrefs.end();
}

/**
 * @brief Forgets the device of a slot: removes its address from NVS and RAM and ends its connection.
 *
 * The slot is free afterwards and takes the next unknown device of its kind (filterDevice()).
 * Other slots are not touched: a sensor stored as CSC2 stays CSC2 when CSC1 is deleted.
 * (It used to be moved to CSC1 in RAM only, while it was still connected as CSC2 and still
 * stored as CSC2 in NVS.)
 *
 * The connection is ended because the slot's client would otherwise be taken over by the
 * next device learned there, while the forgotten one keeps delivering data. A device that
 * is still switched on and in range is simply learned again on the next scan.
 *
 * Called from the web server's task.
 *
 * @param type The device type whose address should be removed.
 */
void BLEDevices::resetAdress(EDevType type) {
	cscKind[type] = 0;
	Preferences addrPrefs;
	addrPrefs.begin("BLEConn");
	// isKey() first: a device seen but never connected is in RAM only (storeAdress() is called
	// after the connect), and remove() logs an error for a missing key.
	bool succ = !addrPrefs.isKey(DEV_STRING[type]) || addrPrefs.remove(DEV_STRING[type]);
	addrPrefs.end();
	bclog.logf(succ ? BCLogger::Log_Debug : BCLogger::Log_Warn, BCLogger::TAG_BLE, "Removed stored address for pref %s: %s", DEV_STRING[type], succ ? "OK":"FAILED");

	if (xSemaphoreTake(xDevMutex, static_cast<TickType_t>(500 / portTICK_PERIOD_MS)) == pdTRUE) {
		delete pStoredAddress[type];
		pStoredAddress[type] = nullptr;
		// Asynchronous: onDisconnect() removes the client and updates the statistics.
		if (clients[type]) clients[type]->disconnect();
		xSemaphoreGive(xDevMutex);
	} else {
		bclog.logf(BCLogger::Log_Error, BCLogger::TAG_BLE, "❌⚠️❌⚠️❌ Mutex blocked while deleting the address of %s ❌⚠️❌⚠️❌", DEV_STRING[type]);
	}
}

/**
 * @brief Ends a TrailBridge connection that delivers nothing any more.
 *
 * TrailBridge repeats its nav and GPS frames every 5 s so that a dead connection can be told
 * from "nothing has changed" (PROTOCOL.md, "Heartbeat"). That is needed: when the app is
 * stopped or restarted, Android keeps the link itself up, so no disconnect arrives here --
 * while the restarted app advertises under a new address and is refused as long as the slot
 * has a connection (filterDevice()).
 *
 * Called from the scan task before each scan.
 */
void BLEDevices::checkNavAlive() {
	bool removed = false, timedOut = false;
	if (xSemaphoreTake(xDevMutex, static_cast<TickType_t>(500 / portTICK_PERIOD_MS)) != pdTRUE) return;
	if (clients[DEV_NAV] && !clients[DEV_NAV]->isConnected()) {
		// No disconnect to wait for, e.g. the connect went through but onDisconnect() never did
		clients[DEV_NAV].reset();
		connState[DEV_NAV] = CONN_LOST;
		removed = true;
	} else if (clients[DEV_NAV] && millis() - navLastFrameMs > NAV_TIMEOUT_MS) {
		clients[DEV_NAV]->disconnect();		// asynchronous: onDisconnect() removes the client
		timedOut = true;
	}
	xSemaphoreGive(xDevMutex);

	if (removed) {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_BLE, "🧭 TrailBridge client without connection removed");
		updateDisconnectedDev(DEV_NAV);
	}
	if (timedOut) {
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "🧭 No frame from TrailBridge for %u s - disconnecting", (unsigned)(NAV_TIMEOUT_MS / 1000));
		// Give onDisconnect() the time to free the slot before the scan reports the phone
		for (uint8_t i = 0; i < 10 && clients[DEV_NAV]; i++) vTaskDelay(pdMS_TO_TICKS(100));
	}
}

// ---------------------------------------------

/**
 * @brief Attempts to connect to a BLE device and set up communication.
 *
 * This method establishes a connection to the specified BLE device,
 * retrieves its services and characteristics, and registers for notifications.
 * If the connection is successful, it updates the stored address and checks
 * for battery service availability.
 *
 * @param dev The device structure containing the device type and advertised device.
 * @return true if the connection was successful, false otherwise.
 */
bool BLEDevices::connectToServer(SDevToConnect& dev) {
	const EDevType dt = dev.devType;
	BLEAddress addr = dev.advDev->getAddress();
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "%s - connecting to devicetype %s [%s]", DEV_EMOJI[dt], DEV_STRING[dt], addr.toString().c_str() );

	if (clients[dt]) {
		bclog.logf(BCLogger::Log_Error, BCLogger::TAG_BLE, "❌ %s - Client %s already existing - now new connection", DEV_EMOJI[dt], clients[dt].get()->getPeerAddress().toString().c_str());
		clients[dt]->disconnect();
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "disconnected client, so it can reconnect");
	} else {
		auto client = std::unique_ptr<BLEClient>(new BLEClient());
		client->setClientCallbacks(this);
		// Only DEV_NAV needs a large MTU (nav/GPS TLV frames up to ~253 byte payload, see
		// PROTOCOL.md); HRM/CSC/FL notify a handful of bytes and are fine with the default.
		// Requesting 256 for all five connections wasted ATT buffers (internal, DMA-competing
		// RAM) on four connections that never used it -- see nav_debug.log SD write failures.
		if (dt == DEV_NAV) client->setMTU(256);
		if (xSemaphoreTake(xDevMutex, static_cast<TickType_t>(500 / portTICK_PERIOD_MS)) == pdTRUE) {
			clients[dt] = std::move(client);
			xSemaphoreGive(xDevMutex);
		} else {
			bclog.logf(BCLogger::Log_Error, BCLogger::TAG_BLE, "❌⚠️❌⚠️❌ Mutex blocked when creating new client! ❌⚠️❌⚠️❌");
			return false;
		}
	}


	// ----------> Note that this is a blocking call! <----------------------
	bool connected = clients[dt]->connect(dev.advDev.get());
	connState[dt] = connected ? CONN_CONNECTED : CONN_LOST;
	//--------
	if (!connected) {
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "❌ Can't connect to device %s.", addr.toString().c_str());
			clients[dt].reset();	// Delete client (release() alone would leak it -- releases ownership without deleting)
			return false;
	}
	BLEUUID uuid = serviceUUID[dt];
	BLERemoteService* pRemoteService = clients[dt]->getService(uuid);
	if (pRemoteService == nullptr) {
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "🔵⚠️ Cannot find %s remote service %s", DEV_EMOJI[dt], uuid.toString().c_str());
		return false;
	}
	BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID[dt]);
	if (pRemoteCharacteristic == nullptr) {
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "🔵⚠️ Cannot find %s remote characteristic", DEV_EMOJI[dt]);
		return false;
	} else if (dt == DEV_FL) {
		stats.setConnected(true);  // "Connected" for Stats means that a speed sensor is connected (used for avg calculation). For CSC sensors this is done in the NotifyCallback, because here it is not yet known if sensor is speed or cadence
	}

	// TrailBridge's characteristic is INDICATE (confirmed ack per frame, see PROTOCOL.md "Warum Indicate statt Notify"), all others use plain Notify.
	bool useNotify = (dt != DEV_NAV);
	pRemoteCharacteristic->registerForNotify([&, dt](BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {notifyCallbackCSC(pBLERemoteCharacteristic, pData, length, isNotify, dt);}, useNotify);
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "🔵%s %s registered\n", DEV_EMOJI[dt], useNotify ? "Notify" : "Indicate");

	if (dt == DEV_NAV) {
		navLastFrameMs = millis();
		// Fallback per PROTOCOL.md: Read the last frame directly, don't wait for the first Indicate/heartbeat.
		String initial = pRemoteCharacteristic->readValue();
		if (initial.length() > 0) handleNavData(reinterpret_cast<const uint8_t*>(initial.c_str()), initial.length());

		// Second, independent service on the same peer, not advertised -- only shows up via
		// GATT service discovery, which connect() already did. See PROTOCOL.md "GPS-Positions-Service".
		subscribeGpsPosition(clients[dt].get());
		// Third one: the elevation profile of a GPX route. See PROTOCOL.md "Höhenprofil-Service".
		subscribeProfile(clients[dt].get());
	}

	storeAdress(dt, addr);	// update stored adress in NVS - regardless if it really has changed or not
	if (hasBatService[dt]) readBatLevel(dt);
	return true;
}


void BLEDevices::notifyCallbackCSC(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify, EDevType ctype) {
	uint16_t crank_rev, crank_time, speed_time, hr,  delta;
	uint8_t flags;
	bool isSpeed;
#ifdef BC_SIM
	// Real sensor (the simulator passes no characteristic) while the simulator stands in for it
	if (pBLERemoteCharacteristic && sim.isActive() && (ctype == DEV_HRM || ctype == DEV_CSC_1 || ctype == DEV_CSC_2)) return;
#endif
	switch (ctype) {
#ifdef BC_FL_SUPPORT
	case DEV_FL:
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_FL, "Received %d bytes:\n\t%s", length, pData);
		bufferFL.concat(pData, length);
		if (bufferFL.indexOf('\n') >0) {
			// Debug, not Info: fires on every BLE notify, and FLClassicParser::updateFromString()
			// below logs the same sentence again as TAG_RAW_NMEA -- no need for two copies active
			// by default.
			bclog.log(BCLogger::Log_Debug, BCLogger::TAG_FL, bufferFL.substring(0, bufferFL.length()-1));
			flparser.updateFromString(bufferFL);
			bufferFL.clear();
		}
		break;
#endif
	case DEV_CSC_1:
	case DEV_CSC_2:
		//Extra Debug only: bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "Received from CSC %d bytes", length);
		flags = pData[0];
		//Extra Debug only: bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, " - Flag %x: Wheel: [%c] Crank: [%c]", flags, (flags & 1) ? 'x':' ', (flags & 2) ? 'x':' ');
		isSpeed = (flags & 1);
		if ((!isSpeed && !(flags & 2)) || (length < (5 + (isSpeed?2:0)))) {		// 1 + 4 bytes for cadence, 1 + 6 bytes for speed
			bclog.log(BCLogger::Log_Error, BCLogger::TAG_BLE, "Unknown flags or message to small from CSC device");
			return;
		}
		if (isSpeed) {
//#ifndef  BC_FL_SUPPORT
			cscIsSpeed[ctype] = true;
			cscKind[ctype] = 1;
			speed_rev = ((uint32_t)pData[4] << 24) + (pData[3] << 16) + (pData[2] << 8) + pData[1];		// LSB first (cast: uint8_t would promote to int and shift into its sign bit)
			speed_time = (pData[6] << 8) + pData[5];	// LSB first
			stats.getDistHandler().updateRevs(speed_rev, speed_time);
			// Signed arithmetic throughout, then clamp - stats.getDistance() returns
			// uint32_t, so "nav_distance - (getDistance() - nav_distance_int)" done as
			// written (nav_distance/nav_distance_int are int32_t, but usual arithmetic
			// conversions promote the WHOLE expression to unsigned the moment the
			// uint32_t getDistance() is involved) silently underflows to a huge bogus
			// value the moment traveled-since-reference exceeds nav_distance - i.e.
			// almost guaranteed once you're close to (or a few meters past) the
			// maneuver, exactly where an accurate distance estimate matters most for
			// UIFacade::evaluateNaviAutoSwitch()'s 300m/350m thresholds. Found
			// 2026-09-20 after the auto-switch state machine was reported unreliable.
			int32_t traveledSinceRef = (int32_t) stats.getDistance(Statistics::SUM_ESP_START, true) - nav_distance_int;
			int32_t d = nav_distance - traveledSinceRef;
			if (d < 0) d = 0;
			ui.updateNaviDist((uint32_t) d);
//#endif
		} else {
			cscIsSpeed[ctype] = false;
			cscKind[ctype] = 2;
			crank_rev = (pData[2] << 8) + pData[1];		// LSB first
			crank_time = (pData[4] << 8) + pData[3];	// LSB first
			delta = crank_time - crank_time_last;
			if (crank_rev_last == 0) crank_rev_last = crank_rev;		// if this is the first received message, set the "last known" value to current value -> cadence = 0.
			bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "%d revs at %d. Delta: %d at %ul", crank_rev, crank_time, (crank_rev - crank_rev_last), delta);
			if (delta > 0) {
				uint32_t rev_delta = (crank_rev - crank_rev_last) * 1024 * 60; // 32bit-calculation necessary for rev_delta >= 2.
				cadence = rev_delta / delta;
				crank_time_last_received = millis();
				crank_time_last = crank_time;
			} else {
				if (millis() - crank_time_last_received > 1200) {
					cadence = 0;
				} else {
					bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "Iwwwwwwwgnore 0 revolutions since last update is less than 1200ms");
				}
			}
			crank_rev_last = crank_rev;
			// Debug, not Info: fires on every BLE notify, immediately upstream of
			// Statistics::addCadence()'s accumulator write (see Statistics.h's PSRAM-placement
			// comment on why that specific write path is sensitive to being on a hot/frequent
			// trigger).
			bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "Cadence %d revs per minute", cadence);
			stats.addCadence(cadence, crank_rev);
		}
		break;
	case DEV_HRM:
		if (length < 2)	return;
		flags = pData[0];
		hr = pData[1];
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "HR Data with length %ul. HR 8bit: %x FLAG: %X", length,	hr, flags);
		if ((pData[0] & 1)) {		// LSB flag = 16 bit or 8bit
			hr |= (pData[2] << 8);
		}
		// Debug, not Info: fires on every BLE notify, immediately upstream of
		// Statistics::addHR()'s accumulator write (see the Cadence log above for why that matters
		// on this hardware).
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "Heart rate: %d ❤ per minute", hr);
		stats.addHR(hr);
		break;
	case DEV_NAV:
		handleNavData(pData, length);
		break;
	default:
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "Notify Callback for %d\n", ctype);
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_BLE, pBLERemoteCharacteristic->toString().c_str());
	}
}

/**
 * @brief Parses a LANES/NEXT_LANES TLV value (tag 0x0A/0x0B) into up to
 * NAV_LANES_MAX NavLane entries. Extra lanes beyond that cap are silently
 * dropped (real roads rarely exceed 6-8 lanes, see BikeNavProtocol.h).
 */
static uint8_t parseLanes(const uint8_t* val, uint8_t len, NavLane* out) {
	uint8_t count = len / 4;
	if (count > NAV_LANES_MAX) count = NAV_LANES_MAX;
	for (uint8_t i = 0; i < count; i++) {
		out[i].primary   = val[i * 4 + 0];
		out[i].secondary = val[i * 4 + 1];
		out[i].tertiary  = val[i * 4 + 2];
		out[i].flags     = val[i * 4 + 3];
	}
	return count;
}

/**
 * @brief Parses one TrailBridge frame (Indicate payload or Read fallback) and updates the nav UI.
 *
 * Frame layout per ../TrailBridge/PROTOCOL.md: byte 0 = protocol version, byte 1 = message type,
 * followed by TLV entries (tag 1 byte | length 1 byte | value) when the message is NAV_UPDATE.
 * Unknown tags are skipped by length, not interpreted, so future protocol additions won't break
 * this parser.
 */
void BLEDevices::handleNavData(const uint8_t* pData, size_t length) {
	navLastFrameMs = millis();
	if (length < 2) {
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "🧭 Nav frame too short (%d byte)", length);
		return;
	}
	uint8_t version = pData[0];
	uint8_t msgType = pData[1];
	if (version != BIKENAV_PROTOCOL_VERSION) {
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "🧭 Unsupported nav protocol version %d (expected %d)", version, BIKENAV_PROTOCOL_VERSION);
		return;
	}

	switch (msgType) {
	case NAV_MSG_HELLO:
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "🧭 TrailBridge HELLO");
		break;

	case NAV_MSG_NAV_NONE:
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "🧭 No active route");
		ui.updateNavi(String(), 0, NAV_MANEUVER_NONE, 0, NAV_MANEUVER_NONE, 0, String(), 0, 0);
		ui.updateLanes(nullptr, 0, 0, nullptr, 0, 0);
		climb.onRouteGone();
		break;

	case NAV_MSG_NAV_UPDATE: {
		uint8_t maneuver = NAV_MANEUVER_UNKNOWN;
		uint8_t nextManeuver = NAV_MANEUVER_UNKNOWN;
		uint8_t roundaboutExit = 0;
		uint32_t maneuverDist = 0, nextManeuverDist = 0, remainingDist = 0, remainingTime = 0;
		bool hasRemainingDist = false;
		String street, nextStreet;
		NavLane lanes[NAV_LANES_MAX];
		NavLane nextLanes[NAV_LANES_MAX];
		uint8_t laneCount = 0, nextLaneCount = 0;
		uint32_t laneDist = 0, nextLaneDist = 0;

		size_t pos = 2;
		while (pos + 2 <= length) {
			uint8_t tag = pData[pos];
			uint8_t len = pData[pos + 1];
			pos += 2;
			bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "🧭 TLV tag=0x%02X len=%d", tag, len);
			if (pos + len > length) {
				bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "🧭 TLV tag 0x%02X length %d exceeds frame, aborting parse", tag, len);
				break;
			}
			const uint8_t* val = pData + pos;
			switch (tag) {
			case NAV_TAG_MANEUVER:
				if (len >= 1) maneuver = val[0];
				break;
			case NAV_TAG_MANEUVER_DISTANCE_M:
				if (len >= 4) maneuverDist = val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24);
				break;
			case NAV_TAG_ROUNDABOUT_EXIT:
				if (len >= 1) roundaboutExit = val[0];
				break;
			case NAV_TAG_STREET_NAME:
				street = String(val, len);
				break;
			case NAV_TAG_NEXT_MANEUVER:
				if (len >= 1) nextManeuver = val[0];
				break;
			case NAV_TAG_NEXT_MANEUVER_DISTANCE_M:
				if (len >= 4) nextManeuverDist = val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24);
				break;
			case NAV_TAG_NEXT_STREET_NAME:
				nextStreet = String(val, len);
				break;
			case NAV_TAG_REMAINING_DISTANCE_M:
				if (len >= 4) { hasRemainingDist = true; remainingDist = val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24); }
				break;
			case NAV_TAG_REMAINING_TIME_S:
				if (len >= 4) remainingTime = val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24);
				break;
			case NAV_TAG_LANES:
				laneCount = parseLanes(val, len, lanes);
				break;
			case NAV_TAG_NEXT_LANES:
				nextLaneCount = parseLanes(val, len, nextLanes);
				break;
			case NAV_TAG_LANE_DISTANCE_M:
				if (len >= 4) laneDist = val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24);
				break;
			case NAV_TAG_NEXT_LANE_DISTANCE_M:
				if (len >= 4) nextLaneDist = val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24);
				break;
			default:
				break;	// unknown tag: length already respected below, value ignored
			}
			pos += len;
		}

		nav_distance = maneuverDist;
		nav_distance_int = stats.getDistance(Statistics::SUM_ESP_START, true);
		// The rider's place in the elevation profile (PROTOCOL.md "Position im Profil ohne Wegstreckenzähler")
		if (hasRemainingDist) climb.onNavRemaining(remainingDist);

		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "🧭 %s in %d m auf %s (Rest: %d m / %d s). Next: %s in %d m auf %s",
				navManeuverToString(maneuver), maneuverDist, street.c_str(), remainingDist, remainingTime,
				navManeuverToString(nextManeuver), nextManeuverDist, nextStreet.c_str());

		if (laneCount > 0 || nextLaneCount > 0) {
			auto laneStr = [](const NavLane* l, uint8_t n) {
				String s;
				for (uint8_t i = 0; i < n; i++) {
					if (i) s += " | ";
					s += navManeuverToString(l[i].primary);
					if (l[i].secondary != NAV_MANEUVER_NONE) { s += "/"; s += navManeuverToString(l[i].secondary); }
					if (l[i].tertiary != NAV_MANEUVER_NONE) { s += "/"; s += navManeuverToString(l[i].tertiary); }
					if (l[i].flags & NAV_LANE_FLAG_ACTIVE) s += "*";
				}
				return s;
			};
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "🧭 Lanes in %d m: [%s]  Next lanes in %d m: [%s]",
					laneDist, laneStr(lanes, laneCount).c_str(), nextLaneDist, laneStr(nextLanes, nextLaneCount).c_str());
		}

		ui.updateNavi(street, maneuverDist, maneuver, roundaboutExit, nextManeuver, nextManeuverDist,
				nextStreet, remainingDist, remainingTime);
		ui.updateLanes(lanes, laneCount, laneDist, nextLanes, nextLaneCount, nextLaneDist);
		break;
	}

	default:
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "🧭 Unknown nav message type 0x%02X", msgType);
	}
}

/**
 * @brief Subscribes to the GPS-Positions-Service on an already-connected DEV_NAV peer.
 *
 * This service is not advertised (31-byte legacy advertising limit on the app side, see
 * PROTOCOL.md), so it can't be found via the usual scan/filterDevice path. It only shows up
 * via GATT service discovery, which BLEClient::connect() already performed for pClient.
 * If it's missing, this is an older TrailBridge version without the service -- log and move on.
 */
void BLEDevices::subscribeGpsPosition(BLEClient* pClient) {
	BLERemoteService* pService = pClient->getService(gpsServiceUUID);
	if (pService == nullptr) {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_BLE, "📍⚠️ GPS position service not found on peer (older TrailBridge version?)");
		return;
	}
	BLERemoteCharacteristic* pCharacteristic = pService->getCharacteristic(gpsCharUUID);
	if (pCharacteristic == nullptr) {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_BLE, "📍⚠️ GPS position characteristic not found");
		return;
	}
	// Indicate, like the nav characteristic (see PROTOCOL.md "Warum Indicate statt Notify") --
	// its own subscription/CCCD, independent of the nav service's heartbeat/state.
	pCharacteristic->registerForNotify([this](BLERemoteCharacteristic* c, uint8_t* pData, size_t length, bool isNotify) {handleGpsData(pData, length);}, false);
	bclog.log(BCLogger::Log_Debug, BCLogger::TAG_BLE, "📍 GPS position Indicate registered");

	// Fallback per PROTOCOL.md: Read the last frame directly, don't wait for the first Indicate/heartbeat.
	String initial = pCharacteristic->readValue();
	if (initial.length() > 0) handleGpsData(reinterpret_cast<const uint8_t*>(initial.c_str()), initial.length());
}

/**
 * @brief Subscribes to the elevation-profile service on an already-connected DEV_NAV peer.
 *
 * Same pattern as subscribeGpsPosition(): not advertised, found by the service discovery
 * connect() already did; missing on a TrailBridge version without it. The frames are parsed
 * by ClimbMonitor (Climb::Tracker::feedFrame()).
 */
void BLEDevices::subscribeProfile(BLEClient* pClient) {
	BLERemoteService* pService = pClient->getService(profileServiceUUID);
	if (pService == nullptr) {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_BLE, "⛰️⚠️ Elevation profile service not found on peer (older TrailBridge version?)");
		return;
	}
	BLERemoteCharacteristic* pCharacteristic = pService->getCharacteristic(profileCharUUID);
	if (pCharacteristic == nullptr) {
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_BLE, "⛰️⚠️ Elevation profile characteristic not found");
		return;
	}
	pCharacteristic->registerForNotify([](BLERemoteCharacteristic* c, uint8_t* pData, size_t length, bool isNotify) {climb.onProfileFrame(pData, length);}, false);
	bclog.log(BCLogger::Log_Debug, BCLogger::TAG_BLE, "⛰️ Elevation profile Indicate registered");

	// No heartbeat on this service: the last frame is only to be had by reading it.
	String initial = pCharacteristic->readValue();
	if (initial.length() > 0) climb.onProfileFrame(reinterpret_cast<const uint8_t*>(initial.c_str()), initial.length());
}

/**
 * @brief Parses one GPS-Positions-Service frame (Indicate payload or Read fallback).
 *
 * Frame layout per ../TrailBridge/PROTOCOL.md, section "GPS-Positions-Service": byte 0 = protocol
 * version, byte 1 = message type, followed by TLV entries (tag 1 byte | length 1 byte | value)
 * when the message is POSITION_UPDATE. Unknown tags are skipped by length, not interpreted --
 * same TLV-forward-compat reasoning as the nav parser above.
 */
void BLEDevices::handleGpsData(const uint8_t* pData, size_t length) {
	navLastFrameMs = millis();
	if (length < 2) {
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "📍 GPS frame too short (%d byte)", length);
		return;
	}
	uint8_t version = pData[0];
	uint8_t msgType = pData[1];
	if (version != GPS_PROTOCOL_VERSION) {
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "📍 Unsupported GPS protocol version %d (expected %d)", version, GPS_PROTOCOL_VERSION);
		return;
	}

	switch (msgType) {
	case GPS_MSG_HELLO:
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "📍 TrailBridge GPS HELLO");
		break;

	case GPS_MSG_POSITION_NONE:
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "📍 No GPS fix (GPS off, permission missing, or no reception yet)");
		gpsFix = SGpsFix();
#ifdef BC_SIM
		sim.endTrailBridgeFeed();
#endif
		break;

	case GPS_MSG_POSITION_UPDATE: {
		SGpsFix fix;
		fix.valid = true;

		size_t pos = 2;
		while (pos + 2 <= length) {
			uint8_t tag = pData[pos];
			uint8_t len = pData[pos + 1];
			pos += 2;
			if (pos + len > length) {
				bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "📍 TLV tag 0x%02X length %d exceeds frame, aborting parse", tag, len);
				break;
			}
			const uint8_t* val = pData + pos;
			switch (tag) {
			case GPS_TAG_LATITUDE_E7:
				if (len >= 4) fix.latitudeE7 = (int32_t)(val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24));
				break;
			case GPS_TAG_LONGITUDE_E7:
				if (len >= 4) fix.longitudeE7 = (int32_t)(val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24));
				break;
			case GPS_TAG_ALTITUDE_M:
				if (len >= 4) { fix.hasAltitude = true; fix.altitudeM = (int32_t)(val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24)); }
				break;
			case GPS_TAG_SPEED_CMS:
				if (len >= 4) { fix.hasSpeed = true; fix.speedCms = val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24); }
				break;
			case GPS_TAG_BEARING_DEG_X100:
				if (len >= 2) { fix.hasBearing = true; fix.bearingDegX100 = val[0] | (val[1] << 8); }
				break;
			case GPS_TAG_ACCURACY_M_X10:
				if (len >= 2) { fix.hasAccuracy = true; fix.accuracyMX10 = val[0] | (val[1] << 8); }
				break;
			case GPS_TAG_FIX_AGE_MS:
				if (len >= 4) fix.fixAgeMs = val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24);
				break;
			case GPS_TAG_UTC_TIME_MS:
				if (len >= 8) {
					uint64_t t = 0;
					for (int i = 7; i >= 0; i--) t = (t << 8) | val[i];
					fix.hasUtcTime = true;
					fix.utcTimeMs = static_cast<int64_t>(t);
				}
				break;
			case GPS_TAG_HEART_RATE_BPM:
				if (len >= 1) { fix.hasHeartRate = true; fix.heartRateBpm = val[0]; }
				break;
			case GPS_TAG_CADENCE_RPM:
				if (len >= 1) { fix.hasCadence = true; fix.cadenceRpm = val[0]; }
				break;
			case GPS_TAG_SIM_FLAGS:
				if (len >= 1) fix.simFlags = val[0];
				break;
			case GPS_TAG_BARO_HEIGHT_DM:
				if (len >= 4) { fix.hasBaroHeight = true; fix.baroHeightDm = (int32_t)(val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24)); }
				break;
			case GPS_TAG_MSL_ALTITUDE_DM:
				if (len >= 4) { fix.hasMslAltitude = true; fix.mslAltitudeDm = (int32_t)(val[0] | (val[1] << 8) | (val[2] << 16) | ((uint32_t)val[3] << 24)); }
				break;
			case GPS_TAG_POWER_W:
				if (len >= 2) { fix.hasPower = true; fix.powerW = val[0] | (val[1] << 8); }
				break;
			default:
				break;	// unknown tag: length already respected below, value ignored
			}
			pos += len;
		}

		gpsFix = fix;
		gpsFixReceivedMillis = millis();
		// Both values come from this frame, so their sum is the phone's time at sending.
		if (fix.hasUtcTime) ClockSync::offerGpsTime(fix.utcTimeMs + fix.fixAgeMs);
#ifdef BC_SIM
		// A TrailBridge test ride (GPX playback) stands in for the speed/cadence/HR sensors.
		// A stale frame (heartbeat resend while the app is gone) counts as the end of it.
		sim.feedFromTrailBridge((fix.simFlags & GPS_SIM_SENSORS) && fix.fixAgeMs <= 5000,
				fix.hasSpeed ? fix.speedCms * 0.036f : 0.0f,
				fix.hasCadence ? fix.cadenceRpm : -1, fix.hasHeartRate ? fix.heartRateBpm : -1,
				fix.hasBaroHeight ? fix.baroHeightDm / 10.0f : NAN, fix.hasPower ? fix.powerW : -1);
#endif

		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "📍 %.7f, %.7f (fix age %u ms)", fix.latitudeE7 / 1e7, fix.longitudeE7 / 1e7, fix.fixAgeMs);
		break;
	}

	default:
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "📍 Unknown GPS message type 0x%02X", msgType);
	}
}

/**
 * @brief Returns the latest known GPS fix, with fixAgeMs advanced by the time elapsed since
 * it was received over BLE (so a caller doesn't need to also track the reception timestamp).
 */
SGpsFix BLEDevices::getGpsFix() const {
	SGpsFix fix = gpsFix;
	if (fix.valid) {
		fix.fixAgeMs += (millis() - gpsFixReceivedMillis);
	}
	return fix;
}

/**
 * @brief Checks and updates the battery levels for connected BLE devices.
 *
 * This function iterates over all possible device types and checks whether they are
 * connected and support battery level reading. If so, it retrieves the battery level
 * from the corresponding BLE characteristic.
 *
 * - If the read operation is successful, the battery level is updated.
 * - If the read operation fails, a warning is logged, the battery service is disabled
 *   for this device (`hasBatService[dt] = false`), and the battery level is set to -1.
 *
 * Logging is provided for debugging and informational purposes.
 *
 * @note This function should be called periodically to keep battery levels up to date.
 */
void BLEDevices::checkBatteries() {
	for (EDevType dt = DEV_HRM; dt < DEV_COUNT; dt = static_cast<EDevType>(dt + 1)) {
		if (connState[dt] == CONN_CONNECTED && hasBatService[dt]) {
			readBatLevel(dt);
		}
	}
}


int8_t BLEDevices::readBatLevel(const EDevType dt) {
	if (!clients[dt]) return batLevel[dt];
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "Read %s battery level", DEV_EMOJI[dt]);
	String valStr = clients[dt]->getValue(serviceUUIDBat, charUUIDBat);
	if (!valStr.isEmpty()) {
		batLevel[dt] = static_cast<int8_t>(valStr[0]);  // Convert first byte to int8
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "%s battery level %d %%", DEV_EMOJI[dt], batLevel[dt]);
	} else {
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "⚠️ Battery level read failed for %s", DEV_EMOJI[dt]);
		hasBatService[dt] = false;
		batLevel[dt] = -1;  // Indicate failure
	}
	return batLevel[dt];
}


namespace {

// Connection state -> badge class from the shared stylesheet, indexed by EBLEConnState.
// Indexed rather than switched because EBLEConnState is private to BLEDevices and cannot
// be named out here; keep this in the enum's order:
//   CONN_DEV_NOTFOUND, CONN_ADVERTISED, CONN_CONNECTED, CONN_LOST
// The stylesheet's class names go by severity, not meaning: badge-info is green,
// badge-debug blue, badge-warn amber, badge-error red.
const char* const kConnBadge[] = { "", "badge-warn", "badge-info", "badge-error" };

const char* connBadge(int state) {
	const int n = sizeof(kConnBadge) / sizeof(kConnBadge[0]);
	return (state >= 0 && state < n) ? kConnBadge[state] : "";
}

// Battery as a drawn gauge rather than a bare number: colour and fill both carry the
// level, so a row can be read at a glance. -1 means the device has no battery service
// or the read failed.
void appendBattery(String& out, int8_t level) {
	if (level < 0) {
		out += F("<span class=\"eyebrow\">n/a</span>");
		return;
	}
	const char* colour = (level > 50) ? "var(--rr-ok,#6FA98C)"
	                   : (level > 20) ? "var(--rr-warn,#D7B463)"
	                                  : "var(--rr-err,#C1604A)";
	out += F("<span class=\"bat\" style=\"color:");
	out += colour;
	out += F("\"><i><b style=\"width:");
	out += level;
	out += F("%\"></b></i>");
	out += level;
	out += F("%</span>");
}

}	// anonymous namespace

uint16_t BLEDevices::getHTMLPage(String &htmlresponse) {
	// The battery gauge is the only thing the shared stylesheet has no class for.
	// currentColor drives border and fill, so appendBattery() only sets one colour.
	static const char* kCss =
		".bat{display:inline-flex;align-items:center;gap:6px;white-space:nowrap}"
		".bat i{position:relative;display:inline-block;width:26px;height:12px;"
		"border:1px solid currentColor;border-radius:3px}"
		".bat i:after{content:'';position:absolute;right:-4px;top:3px;width:2px;height:6px;"
		"background:currentColor;border-radius:0 1px 1px 0}"
		".bat b{position:absolute;left:1px;top:1px;bottom:1px;background:currentColor;border-radius:1px}"
		".addr{font-family:'IBM Plex Mono',monospace;font-size:0.76rem}"
		".del-addr{color:var(--rr-err,#C1604A);text-decoration:none;margin-left:8px;font-size:0.95rem}";

	WebPage::begin(htmlresponse, "BLE Devices", kCss);
	htmlresponse += F("<p class=\"eyebrow\">Deleting a stored address disconnects the device and frees the slot, "
	                  "so a different device of that type can pair on the next scan.</p>\n"
	                  "<table><thead><tr><th>Device</th><th>Stored Address</th><th>State</th>"
	                  "<th>Battery</th><th>Data</th></tr></thead><tbody>\n");

	for (uint16_t c = 0; c < DEV_COUNT; c++) {
		htmlresponse += F("<tr><td>");
		htmlresponse += DEV_EMOJI[c];
		htmlresponse += ' ';
		htmlresponse += DEV_STRING[c];
		htmlresponse += F("</td><td class=\"addr\">");
		if (pStoredAddress[c]) {
			htmlresponse += pStoredAddress[c]->toString().c_str();
			// Directly behind the address it belongs to -- this used to sit in a far-right
			// column labelled "Reconnect", which is not what it does: it forgets the paired
			// peer so a NEW device can take the slot.
			htmlresponse += F("<a class=\"del-addr\" href=\"#\" title=\"Delete stored address\" onclick=\"delAddr(");
			htmlresponse += c;
			htmlresponse += F(",'");
			htmlresponse += DEV_STRING[c];
			htmlresponse += F("');return false;\">&#10005;</a>");
		} else {
			htmlresponse += F("<span class=\"eyebrow\">not stored &mdash; open for pairing</span>");
		}
		htmlresponse += F("</td><td><span class=\"badge ");
		htmlresponse += connBadge(connState[c]);
		htmlresponse += F("\" title=\"");
		// The live peer address is only interesting when something is actually connected,
		// so it rides along as a tooltip instead of costing a whole column.
		htmlresponse += clients[c] ? clients[c].get()->getPeerAddress().toString().c_str() : "-";
		htmlresponse += F("\">");
		htmlresponse += CONN_STRING[connState[c]];
		htmlresponse += F("</span></td><td>");
		appendBattery(htmlresponse, hasBatService[c] ? batLevel[c] : -1);
		htmlresponse += F("</td><td class=\"eyebrow\">");
		if (c == DEV_CSC_1 || c == DEV_CSC_2) {
			htmlresponse += cscIsSpeed[c] ? F("Speed, ") : F("Cadence, ");
				htmlresponse += static_cast<unsigned long>(cscIsSpeed[c] ? speed_rev : crank_rev_last);
			htmlresponse += F(" rev");
		} else {
			htmlresponse += F("&mdash;");
		}
		htmlresponse += F("</td></tr>\n");
	}
	htmlresponse += F("</tbody></table>\n");

	WebPage::end(htmlresponse,
		"function delAddr(d,n){if(!confirm('Delete the stored address for '+n+'?\\n\\n"
		"The device is disconnected and the slot freed, so a different device can pair on the next scan.'))return;"
		"req('/dev/delete?dev='+d,'Address deleted',()=>location.reload());}\n");
	return 200;
}

void BLEDevices::forgetTask(void* arg) {
	bleDevs.resetAdress(static_cast<EDevType>(reinterpret_cast<intptr_t>(arg)));
	vTaskDelete(NULL);
}

BLEDevices::DevStatus BLEDevices::getDevStatus(EDevType dt) const {
	DevStatus s;
	if (dt < 0 || dt >= DEV_COUNT) return s;
	s.state = connState[dt];
	s.battery = batLevel[dt];
	s.hasAddress = pStoredAddress[dt] != nullptr;
	s.cscKind = cscKind[dt];
	return s;
}

bool BLEDevices::requestForget(EDevType dt) {
	if (dt < 0 || dt >= DEV_COUNT || dt == DEV_NAV) return false;
	return xTaskCreate(forgetTask, "BleForget", 4096, reinterpret_cast<void*>(static_cast<intptr_t>(dt)), 5, nullptr) == pdPASS;
}

uint16_t BLEDevices::procHTMLCmd(String& htmlresponse, const String& cmd, const String& arg) {
	// "delete" is what this actually does: forget the stored peer so the slot is free
	// again. "reset" is the original spelling, kept so old links/bookmarks still work.
	if (cmd.equals("delete") || cmd.equals("reset")) {
		int8_t devNum = arg.toInt();
		if (devNum < 0 || devNum >= DEV_COUNT) {
			htmlresponse += "Invalid devices";
			return 400;
		}
		resetAdress(static_cast<EDevType>(devNum));
		htmlresponse += "OK - deleted address";
		return 200;
	} else {
		htmlresponse += "Not implemented (yet)\n";
		return 501;
	}
	return 500;
}
