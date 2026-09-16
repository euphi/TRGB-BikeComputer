/*
 * BLEDevices.cpp
 *
 *  Created on: 18.02.2023
 *      Author: ian
 */

#include <BLEDevices.h>
#include <BLEDevice.h>

#include <Arduino.h>

#include "Singletons.h"
#include "Stats/Distance.h"
#include "BikeNavProtocol.h"
#include "BikeGpsProtocol.h"

#include <task.h>

// HRM, CSC, FL, BikeNavRelay Navigation (see ../BikeNavRelay/PROTOCOL.md)
const BLEUUID BLEDevices::serviceUUID[DEV_COUNT] = { BLEUUID((uint16_t)0x180D), BLEUUID((uint16_t)0x1816), BLEUUID((uint16_t)0x1816), BLEUUID("e62efa94-afa8-11ed-afa1-0242ac120002"), BLEUUID("f7ac2b76-986b-45fd-8e44-f116a61f319d")};
const BLEUUID BLEDevices::serviceUUIDBat = BLEUUID((uint16_t) 0x180F);
const BLEUUID BLEDevices::serviceUUIDExposure = BLEUUID((uint16_t) 0xFD6F);
const BLEUUID BLEDevices::charUUID[DEV_COUNT] = { BLEUUID((uint16_t)0x2A37), BLEUUID((uint16_t)0x2A5B), BLEUUID((uint16_t)0x2A5B), BLEUUID("e62efe40-afa8-11ed-afa1-0242ac120002"), BLEUUID("7473da02-2de8-4f48-9e46-21b36380c176")};
const BLEUUID BLEDevices::charUUIDBat = BLEUUID((uint16_t) 0x2A19);

// GPS-Positions-Service (see ../TrailBridge/PROTOCOL.md) -- second, independent, unadvertised
// service on the same peer as DEV_NAV, discovered via GATT service discovery after connecting.
const BLEUUID BLEDevices::gpsServiceUUID = BLEUUID("66b5835c-9be6-43d1-b24a-f9337c0fcb7f");
const BLEUUID BLEDevices::gpsCharUUID = BLEUUID("10c49e7b-4808-4d63-9b68-9ba6c385db0d");

const char* BLEDevices::DEV_EMOJI[DEV_COUNT] = {"❤️","🚴","🚴","⚡", "🧭"};
const char* BLEDevices::DEV_STRING[DEV_COUNT] = {"HeartRate","CSC1","CSC2","Forumslader", "BikeNavRelay"};
const char* BLEDevices::CONN_STRING[CONN_COUNT] = {"Not Found","Advertised (not yet connected)","Connected","Lost"};

const uint8_t twoByteOn[] = {0x01,0x00};

//std::function<void(BLEScanResults)> scanCB;

BLEDevices::BLEDevices()
{
	xDevMutex = xSemaphoreCreateMutex();
}

void BLEDevices::setup() {
	  BLEDevice::init("TRGB_BTTacho BLE");
	  xTaskCreate(+[](void* thisInstance){((BLEDevices*)thisInstance)->scanAndConnectTask();}, "BLEScanUndConnectTask", 3072, this, 5, &scanTaskHandle);

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
			return client && client->getPeerAddress() == discAddr;
		});
		// Remove the client if found
		if (it != clients.end()) {
			EDevType dt = static_cast<EDevType>(std::distance(clients.begin(), it));  // Get index
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "%s diconnected. Remove Client for %s.", DEV_EMOJI[dt], DEV_STRING[dt]);
			it->reset();  // Delete the client and set pointer to nullptr
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
	for (size_t i = 0 ; i < dev.getServiceUUIDCount(); i++) {
		for (uint16_t d = DEV_HRM ; d < DEV_COUNT ; d++) {
			if (dev.getServiceUUID(i).equals(serviceUUID[d])) {
				bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "Filter: Found %s", DEV_EMOJI[d]);
				//Simple "connect everything" solution: return static_cast<EDevType>(d);
				// Now, we found a interesting device and we need to check if we want to connect it.

				// Is the device address stored for this type? (Note, if no adress is stored, pStoredAddress is nullptr
				if (pStoredAddress[d] && dev.getAddress().equals(*pStoredAddress[d])) {
					bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "Filter: Found stored address %s --> connect", pStoredAddress[d]->toString().c_str());
					connState[d] = CONN_ADVERTISED;
					return static_cast<EDevType>(d);
				} else if (pStoredAddress[d] == nullptr) {  // TODO: This relys on that it never happens that CSC1 address is free and CSC2 address not. Then if CSC2 is advertised it would also be stored as CSC1
					pStoredAddress[d] = new BLEAddress(dev.getAddress());
					bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "Filter: Slot free --> store address %s and connect", pStoredAddress[d]->toString().c_str());
					connState[d] = CONN_ADVERTISED;
					return static_cast<EDevType>(d);
				} else {
					bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "\t%s no new connection to %s allowed", DEV_EMOJI[d], DEV_STRING[d]);
				}
			}
		}
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
            return client && client->getPeerAddress().equals(newDevice.getAddress());
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
		if (cscIsSpeed[dt==DEV_CSC_1?1:2]) {
			stats.setConnected(false);
		} else {
			stats.addCadence(-1, 0);
		}
		break;
	case DEV_NAV:
		ui.updateNavi(String(), 0, NAV_MANEUVER_NONE);		// hide stale directions once the phone disconnects
		gpsFix = SGpsFix();		// GPS-Positions-Service shares this connection, so it's gone too
		break;
	}
}

//BLEDevices::EDevType BLEDevices::nextCSCSlotAvailable() {
//	bool csc1Free = (pStoredAddress[DEV_CSC_1] == nullptr);
//	bool csc2Free = (pStoredAddress[DEV_CSC_2] == nullptr);
//	bool csc1Disconnected =  connState[DEV_CSC_1] == CONN_LOST || connState[DEV_CSC_1] == CONN_DEV_NOTFOUND;
//	bool csc2Disconnected =  connState[DEV_CSC_2] == CONN_LOST || connState[DEV_CSC_2] == CONN_DEV_NOTFOUND;
//	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "🚴\tCSC1: [%c] free \t [%c] conn\n\tCSC2: [%c] free \t [%c] conn\n", csc1Free ? 'X':' ', csc2Free ? 'X':' ',csc1Disconnected ? 'X':' ',csc2Disconnected ? 'X':' ');
//	if (! ( csc1Free || csc2Free)) return DEV_UNKNOWN;  // both connection already reserved
//	if (csc1Free && csc1Disconnected) return DEV_CSC_1;  // CSC1 unreserved and disconnected
//	if (csc2Free && csc2Disconnected) return DEV_CSC_2;  // CSC2 unreserved and disconnected
//
//	bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "\t🚴 No new connection allowed");
//	return DEV_UNKNOWN;			 // no unreserved connection that is still disconnected
//	//--> now, overwriting disconnected connection is allowed
//	if (csc1Disconnected) return DEV_CSC_1;
//	if (csc2Disconnected) return DEV_CSC_2;
//	//--> both connection are already connected
//	bclog.log(BCLogger::Log_Warn, BCLogger::TAG_BLE, "\t🚴 More than two Cycling Speed and Cadence (CSC) devices advertised - ignoring");
//	return DEV_UNKNOWN;
//}


/**
 * @brief Restores previously stored BLE device addresses from NVS (persistent memory).
 *
 * This method retrieves stored BLE addresses from the non-volatile storage (NVS)
 * and assigns them to the corresponding device slots. If no address is found for a
 * specific device type, a log message is generated.
 */
void BLEDevices::restoreAdresses() {
	StatPreferences.begin("BLEConn");
	for (uint16_t c = 0; c < DEV_NAV; c++) {
		uint8_t bit128[16];
		if (StatPreferences.getBytes(DEV_STRING[c], &bit128, 16) > 0) {
			pStoredAddress[c] = new BLEAddress(bit128);
			bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "Addr of %s: %s", DEV_STRING[c], pStoredAddress[c]->toString().c_str());
		} else {
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "No BLE address stored in preferences for %s", DEV_STRING[c]);
		}
	}
	StatPreferences.remove(DEV_STRING[DEV_NAV]);  // no need to store, because address is random. This line deletes existing stored adresses and can be deleted soon
	StatPreferences.end();
}

/**
 * @brief Stores a BLE device address in NVS (persistent memory).
 *
 * This method saves the BLE address of a given device type to non-volatile storage (NVS)
 * for later retrieval. The function does not store addresses for the BikeNavRelay device, as
 * Android peripherals typically use a random/rotating address.
 *
 * @param type The device type whose address should be stored.
 * @param addr The BLE address to be stored.
 */
void BLEDevices::storeAdress(EDevType type, BLEAddress &addr) {
	if (type == DEV_NAV) return;	// BikeNavRelay (Android peripheral) likely uses a random/rotating address, like Komoot before it
	StatPreferences.begin("BLEConn");
	size_t rc = StatPreferences.putBytes(DEV_STRING[type], addr.getNative(), 16);
	bclog.logf(rc > 0 ? BCLogger::Log_Debug : BCLogger::Log_Error, BCLogger::TAG_BLE, "Stored %d bytes to pref %s: %s", rc, DEV_STRING[type],	addr.toString().c_str());
	StatPreferences.end();
}

/**
 * @brief Removes a stored BLE device address from NVS and clears internal references.
 *
 * This method deletes a previously stored BLE address for a given device type from
 * non-volatile storage (NVS) and clears the corresponding internal pointer.
 *
 * Special handling is applied to cycling sensors (CSC): If the address of `DEV_CSC_1`
 * is removed while `DEV_CSC_2` still has a stored address, the `DEV_CSC_2` address is
 * moved to `DEV_CSC_1` to maintain consistency.
 *
 * FIXME: This logic is flawed. It would work fine, if the BC is restarted immediately afterwards. However, it confuses connections if CSC2 is already connected.
 *
 * @param type The device type whose address should be removed.
 */
void BLEDevices::resetAdress(EDevType type) {
	StatPreferences.begin("BLEConn");
	bool succ = StatPreferences.remove(DEV_STRING[type]);
	bclog.logf(succ ? BCLogger::Log_Debug : BCLogger::Log_Warn, BCLogger::TAG_BLE, "Removed stored address for pref %s: %s", DEV_STRING[type], succ ? "OK":"FAILED");
	if (succ) {
		if (pStoredAddress[type]) {
			delete pStoredAddress[type];
	        pStoredAddress[type] = nullptr; // Prevent use-after-free
		}

		// ensure that DEV_CSC1 cannot be empty when CSC2 has stored address. So, if this happens, move CSC2 to CSC1
		if (type == DEV_CSC_1 && pStoredAddress[DEV_CSC_2]) {
			pStoredAddress[DEV_CSC_1] = pStoredAddress[DEV_CSC_2];
			pStoredAddress[DEV_CSC_2] = nullptr;
		} else {
			pStoredAddress[type] = nullptr;
		}
	}
	StatPreferences.end();
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
		client->setMTU(256);		// TODO: Is it necessary to set it that large? (Nav frames can be up to ~253 byte usable payload, see PROTOCOL.md)
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
			Serial.printf("❌ Can't connect to device %s.\n", addr.toString().c_str());
			clients[dt].release();	// Delete client
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

	// BikeNavRelay's characteristic is INDICATE (confirmed ack per frame, see PROTOCOL.md "Warum Indicate statt Notify"), all others use plain Notify.
	bool useNotify = (dt != DEV_NAV);
	pRemoteCharacteristic->registerForNotify([&, dt](BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {notifyCallbackCSC(pBLERemoteCharacteristic, pData, length, isNotify, dt);}, useNotify);
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "🔵%s %s registered\n", DEV_EMOJI[dt], useNotify ? "Notify" : "Indicate");

	if (dt == DEV_NAV) {
		// Fallback per PROTOCOL.md: Read the last frame directly, don't wait for the first Indicate/heartbeat.
		String initial = pRemoteCharacteristic->readValue();
		if (initial.length() > 0) handleNavData(reinterpret_cast<const uint8_t*>(initial.c_str()), initial.length());

		// Second, independent service on the same peer, not advertised -- only shows up via
		// GATT service discovery, which connect() already did. See PROTOCOL.md "GPS-Positions-Service".
		subscribeGpsPosition(clients[dt].get());
	}

	storeAdress(dt, addr);	// update stored adress in NVS - regardless if it really has changed or not
	if (hasBatService[dt]) readBatLevel(dt);
	return true;
}


void BLEDevices::notifyCallbackCSC(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify, EDevType ctype) {
	uint16_t crank_rev, crank_time, speed_time, hr,  delta;
	uint8_t flags;
	bool isSpeed;
	switch (ctype) {
#ifdef BC_FL_SUPPORT
	case DEV_FL:
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_FL, "Received %d bytes:\n\t%s", length, pData);
		bufferFL.concat(pData, length);
		if (bufferFL.indexOf('\n') >0) {
			bclog.log(BCLogger::Log_Info, BCLogger::TAG_FL, bufferFL.substring(0, bufferFL.length()-1));
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
			cscIsSpeed[ctype==DEV_CSC_1?1:2] = true;
			speed_rev = (pData[4] << 24) + (pData[3] << 16) + (pData[2] << 8) + pData[1];		// LSB first
			speed_time = (pData[6] << 8) + pData[5];	// LSB first
			stats.getDistHandler().updateRevs(speed_rev, speed_time);
			uint32_t d = nav_distance - (stats.getDistance(Statistics::SUM_ESP_START, true) - nav_distance_int);
			ui.updateNaviDist(d);
//#endif
		} else {
			cscIsSpeed[ctype==DEV_CSC_1?1:2] = false;
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
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "Cadence %d revs per minute", cadence);
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
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "Heart rate: %d ❤ per minute", hr);
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
 * @brief Parses one BikeNavRelay frame (Indicate payload or Read fallback) and updates the nav UI.
 *
 * Frame layout per ../BikeNavRelay/PROTOCOL.md: byte 0 = protocol version, byte 1 = message type,
 * followed by TLV entries (tag 1 byte | length 1 byte | value) when the message is NAV_UPDATE.
 * Unknown tags are skipped by length, not interpreted -- that's the whole point of TLV over the
 * old fixed Komoot byte layout: future protocol additions won't break this parser.
 */
void BLEDevices::handleNavData(const uint8_t* pData, size_t length) {
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
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "🧭 BikeNavRelay HELLO");
		break;

	case NAV_MSG_NAV_NONE:
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "🧭 No active route");
		ui.updateNavi(String(), 0, NAV_MANEUVER_NONE, 0, NAV_MANEUVER_NONE, 0, String(), 0, 0);
		break;

	case NAV_MSG_NAV_UPDATE: {
		uint8_t maneuver = NAV_MANEUVER_UNKNOWN;
		uint8_t nextManeuver = NAV_MANEUVER_UNKNOWN;
		uint8_t roundaboutExit = 0;
		uint32_t maneuverDist = 0, nextManeuverDist = 0, remainingDist = 0, remainingTime = 0;
		String street, nextStreet;

		size_t pos = 2;
		while (pos + 2 <= length) {
			uint8_t tag = pData[pos];
			uint8_t len = pData[pos + 1];
			pos += 2;
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
				if (len >= 4) maneuverDist = val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24);
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
				if (len >= 4) nextManeuverDist = val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24);
				break;
			case NAV_TAG_NEXT_STREET_NAME:
				nextStreet = String(val, len);
				break;
			case NAV_TAG_REMAINING_DISTANCE_M:
				if (len >= 4) remainingDist = val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24);
				break;
			case NAV_TAG_REMAINING_TIME_S:
				if (len >= 4) remainingTime = val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24);
				break;
			default:
				break;	// unknown tag: length already respected below, value ignored
			}
			pos += len;
		}

		nav_distance = maneuverDist;
		nav_distance_int = stats.getDistance(Statistics::SUM_ESP_START, true);

		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "🧭 %s in %d m auf %s (Rest: %d m / %d s). Next: %s in %d m auf %s",
				navManeuverToString(maneuver), maneuverDist, street.c_str(), remainingDist, remainingTime,
				navManeuverToString(nextManeuver), nextManeuverDist, nextStreet.c_str());

		ui.updateNavi(street, maneuverDist, maneuver, roundaboutExit, nextManeuver, nextManeuverDist,
				nextStreet, remainingDist, remainingTime);
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
 * @brief Parses one GPS-Positions-Service frame (Indicate payload or Read fallback).
 *
 * Frame layout per ../TrailBridge/PROTOCOL.md, section "GPS-Positions-Service": byte 0 = protocol
 * version, byte 1 = message type, followed by TLV entries (tag 1 byte | length 1 byte | value)
 * when the message is POSITION_UPDATE. Unknown tags are skipped by length, not interpreted --
 * same TLV-forward-compat reasoning as the nav parser above.
 */
void BLEDevices::handleGpsData(const uint8_t* pData, size_t length) {
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
				if (len >= 4) fix.latitudeE7 = (int32_t)(val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24));
				break;
			case GPS_TAG_LONGITUDE_E7:
				if (len >= 4) fix.longitudeE7 = (int32_t)(val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24));
				break;
			case GPS_TAG_ALTITUDE_M:
				if (len >= 4) { fix.hasAltitude = true; fix.altitudeM = (int32_t)(val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24)); }
				break;
			case GPS_TAG_SPEED_CMS:
				if (len >= 4) { fix.hasSpeed = true; fix.speedCms = val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24); }
				break;
			case GPS_TAG_BEARING_DEG_X100:
				if (len >= 2) { fix.hasBearing = true; fix.bearingDegX100 = val[0] | (val[1] << 8); }
				break;
			case GPS_TAG_ACCURACY_M_X10:
				if (len >= 2) { fix.hasAccuracy = true; fix.accuracyMX10 = val[0] | (val[1] << 8); }
				break;
			case GPS_TAG_FIX_AGE_MS:
				if (len >= 4) fix.fixAgeMs = val[0] | (val[1] << 8) | (val[2] << 16) | (val[3] << 24);
				break;
			default:
				break;	// unknown tag: length already respected below, value ignored
			}
			pos += len;
		}

		gpsFix = fix;
		gpsFixReceivedMillis = millis();

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


uint16_t BLEDevices::getHTMLPage(String &htmlresponse) {
	uint16_t rc = 200;
    htmlresponse.reserve(2048);  // Pre-allocate memory to improve performance

    htmlresponse += "<!DOCTYPE html><html lang=\"en\">\n<head>\n"
                    "  <meta charset=\"UTF-8\">\n"
                    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
                    "  <title>Connected Devices</title>\n"
                    "  <link rel=\"stylesheet\" href=\"/stylesheet.css\">\n"
                    "</head>\n<body>\n"
                    "  <h2>Connected Devices</h2>\n"
                    "  <table>\n"
                    "    <thead>\n"
                    "      <tr><th>Device Type</th><th>Stored Address</th><th>Connection State</th>"
                    "<th>Current Address</th><th>Additional Data 1</th><th>Additional Data 2</th>"
                    "<th>Battery Level</th><th>Reconnect</th></tr>\n"
                    "    </thead>\n<tbody>\n";
    for (uint16_t c = 0; c < DEV_COUNT; c++) {
        String reconnectLink = (pStoredAddress[c])
            ? "<a class=\"reconnect-link\" href=\"reset?dev=" + String(c) + "\">↻</a>"
            : "-";

        htmlresponse += "<tr><td>" + String(DEV_STRING[c]) + "</td><td>"
                     + (pStoredAddress[c] ? pStoredAddress[c]->toString().c_str() : "-empty-") + "</td><td>"
                     + String(CONN_STRING[connState[c]]) + "</td><td>"
                     + (clients[c] ? clients[c].get()->getPeerAddress().toString().c_str() : "-n/a-") + "</td><td>"
                     + ((c == DEV_CSC_1 || c == DEV_CSC_2) ? (cscIsSpeed[c] ? "Speed" : "Cadence") : "-n/a-") + "</td><td>"
                     + String((c == DEV_CSC_1 || c == DEV_CSC_2) ? (cscIsSpeed[c] ? speed_rev : crank_rev_last) : -1) + "</td><td>"
                     + String(batLevel[c]) + "%</td><td>"
                     + reconnectLink + "</td></tr>\n";
    }
    htmlresponse += "</tbody>\n</table>\n</body></html>";
    return rc;
}

uint16_t BLEDevices::procHTMLCmd(String& htmlresponse, const String& cmd, const String& arg) {
	if (cmd.equals("reset")) {
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
