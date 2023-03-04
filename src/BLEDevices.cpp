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

// HRM, CSC, FL, KomootApp
const BLEUUID BLEDevices::serviceUUID[DEV_COUNT] = { BLEUUID((uint16_t)0x180D), BLEUUID((uint16_t)0x1816), BLEUUID("e62efa94-afa8-11ed-afa1-0242ac120002"), BLEUUID("71C1E128-D92F-4FA8-A2B2-0F171DB3436C")};
const BLEUUID BLEDevices::charUUID[DEV_COUNT] = { BLEUUID((uint16_t)0x2A37), BLEUUID((uint16_t)0x2A5B), BLEUUID("e62efe40-afa8-11ed-afa1-0242ac120002"), BLEUUID("503DD605-9BCB-4F6E-B235-270A57483026")};

const char* BLEDevices::DEV_EMOJI[DEV_COUNT] = {"❤️","🚴","⚡", "🧭"};

const uint8_t twoByteOn[] = {0x01,0x00};

std::function<void(BLEScanResults)> scanCB;

BLEDevices::BLEDevices()
{

}

void BLEDevices::onResult(BLEAdvertisedDevice advertisedDevice) {
	if (bclog.checkLogLevel(BCLogger::Log_Debug, BCLogger::TAG_BLE)) {
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "🔵 Advertised Device: %s ", advertisedDevice.toString().c_str());
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "\tServiceDataCount: %d ServiceDataUUIDCount: %d ServiceUUIDCount: %d", advertisedDevice.getServiceDataCount(), advertisedDevice.getServiceDataUUIDCount(), advertisedDevice.getServiceUUIDCount());
		for (uint8_t c=0; c < advertisedDevice.getServiceDataCount(); c++) {
			std::string servStr = advertisedDevice.getServiceData(c);
			Serial.printf("\tServiceData: %s", servStr.c_str());
		}
		for (uint8_t c=0; c < advertisedDevice.getServiceDataUUIDCount(); c++) {
			BLEUUID uuid = advertisedDevice.getServiceDataUUID(c);
			bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "\tServiceData-UUID: %s", uuid.toString().c_str());
		}
	}
	for (uint8_t c=0; c < advertisedDevice.getServiceUUIDCount(); c++) {
		BLEUUID uuid = advertisedDevice.getServiceUUID(c);
		if (uuid.equals(serviceUUID[DEV_CSC])) {
			bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "\t🚴 Found CSC Device");
			pServerAddress[DEV_CSC] = new BLEAddress(advertisedDevice.getAddress());
			doConnect[DEV_CSC] = true;
			connState[DEV_CSC] = CONN_ADVERTISED;
		}
		if (uuid.equals(serviceUUID[DEV_HRM])) {
			bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "\t❤️ Found HRM Device");
			pServerAddress[DEV_HRM] = new BLEAddress(advertisedDevice.getAddress());
			doConnect[DEV_HRM] = true;
			connState[DEV_HRM] = CONN_ADVERTISED;
		}
		if (uuid.equals(serviceUUID[DEV_KOMOOT])) {
			bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "\t🧭 Found Komoot App");
			if (connState[DEV_KOMOOT] != CONN_CONNECTED) {
				pServerAddress[DEV_KOMOOT] = new BLEAddress(advertisedDevice.getAddress());
				doConnect[DEV_KOMOOT] = true;
				connState[DEV_KOMOOT] = CONN_ADVERTISED;
			}
		}
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "\tService-UUID: %s", uuid.toString().c_str());
	}
	if (advertisedDevice.getName().find("ForumsLader") != std::string::npos) {
		bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "\t⚡ Found FL Device");
		pServerAddress[DEV_FL] = new BLEAddress(advertisedDevice.getAddress());
		doConnect[DEV_FL]=true;
		connState[DEV_FL] = CONN_ADVERTISED;
	}
}


bool BLEDevices::connectToServer(EDevType ctype) {
	doConnect[ctype] = false;
	if (!pClient[ctype]) {
		pClient[ctype] = new BLEClient();
		pClient[ctype]->setMTU(256);
	} else {
		pClient[ctype]->disconnect();
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "Client already exists for %s %d - reusing it", DEV_EMOJI[ctype], ctype);
	}
	bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "%s Connecting to address %s", DEV_EMOJI[ctype], pServerAddress[ctype]->toString().c_str());
	if (pClient[ctype]->connect(*pServerAddress[ctype], (ctype==DEV_CSC || ctype==DEV_KOMOOT) ? BLE_ADDR_TYPE_RANDOM : BLE_ADDR_TYPE_PUBLIC)) {
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "Client %s connected", DEV_EMOJI[ctype]);
		connState[ctype] = CONN_CONNECTED;
	} else {
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "🔵⚠️ Cannot connect %s to %s", DEV_EMOJI[ctype], pServerAddress[ctype]->toString().c_str());
		return false;
	}

	BLEUUID uuid = serviceUUID[ctype];
	BLERemoteService* pRemoteService = pClient[ctype]->getService(uuid);
	if (pRemoteService == nullptr) {
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "🔵⚠️ Cannot find %s remote service %s", DEV_EMOJI[ctype], uuid.toString().c_str());
		return false;
	}
	BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID[ctype]);
	if (pRemoteCharacteristic == nullptr) {
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "🔵⚠️ Cannot find %s remote characteristic", DEV_EMOJI[ctype]);
		return false;
	}
	if (ctype == DEV_KOMOOT) {
		//pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)twoByteOn, 2, true);
		pKomootRemoteCharacteristic = pRemoteCharacteristic;
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "🔵%s Setup handler for komoot\n", DEV_EMOJI[ctype]);
	} else if (ctype == DEV_FL) {
		stats.setConnected(true);
	}

	pRemoteCharacteristic->registerForNotify([&, ctype](BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {notifyCallbackCSC(pBLERemoteCharacteristic, pData, length, isNotify, ctype);});
	bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "🔵%s Notify registered\n", DEV_EMOJI[ctype]);
	return true;
}

void BLEDevices::startBLEScan() {
	if (!pBLEScan) {
		pBLEScan = BLEDevice::getScan(); //create new scan
		pBLEScan->setAdvertisedDeviceCallbacks(this);
		pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
		pBLEScan->setInterval(100);
		pBLEScan->setWindow(99);  // less or equal setInterval value
	} else {
		pBLEScan->stop();
		pBLEScan->clearResults();
	}
	bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "🔵 BLE scan started");
//	pBLEScan->start(scanTime, [](BLEScanResults result) {	 // Non-Blocking
//		//scanning = false;
//		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "🔵 ✔️ BLE scan completed: %d devices found.", result.getCount());
//		scanCB(result);
//	});
	pBLEScan->start(scanTime, [](BLEScanResults result) {scanCB(result);});  // non-blocking

	scanning = true;
}

void BLEDevices::setup() {
	  BLEDevice::init("TRGB_BTTacho BLE");
	  scanCB = [this](BLEScanResults result) {
		  bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "🔵 ✔️ BLE scan completed: %d devices found.", result.getCount());
		  this->scanning=false;
	  };
	  startBLEScan();
	  komootTicker.attach(4, +[](BLEDevices* thisInstance) {thisInstance->komootLoop();}, this);
	  connCheckTicker.attach_ms(250, +[](BLEDevices* thisInstance) {thisInstance->connCheckLoop();}, this);
	  // 	typedef std::function<void(EFLConnState cstate)> FLStateUpdateHandler;
}

void BLEDevices::notifyCallbackCSC(BLERemoteCharacteristic *pBLERemoteCharacteristic, uint8_t *pData, size_t length, bool isNotify, EDevType ctype) {
	uint8_t flags;
	uint16_t crank_rev, crank_time, hr,  delta;
	switch (ctype) {
	case DEV_FL:
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_FL, "Received %d bytes:\n\t%s", length, pData);
		bufferFL.concat(pData, length);
		if (bufferFL.indexOf('\n') >0) {
			bclog.log(BCLogger::Log_Info, BCLogger::TAG_FL, bufferFL.substring(0, bufferFL.length()-1));
			flparser.updateFromString(bufferFL);
			bufferFL.clear();
		}
		break;
	case DEV_CSC:

		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "Received from CSC %d bytes", length);
//		for (uint8_t c = 0; c<length; c++) {Serial.print(*(pData+c), 16);}
		if (length < 1) {
			return;
		}
		flags = pData[0];
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, " - Flag %x: Wheel: [%c] Crank: [%c]", flags, (flags & 1) ? 'x':' ', (flags & 2) ? 'x':' ');
		if (length < 5) return;
		crank_rev = (pData[2] << 8 )+ pData[1];		// LSB first
		crank_time = (pData[4] << 8 )+ pData[3];	// LSB first
		delta = crank_time - crank_time_last;
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE,"%d revs at %d. Delta: %d at %ul", crank_rev, crank_time, (crank_rev - crank_rev_last), delta);
		if (delta > 0) {
			uint32_t rev_delta = (crank_rev - crank_rev_last) * 1024 * 60; // 32bit-calculation necessary for rev_delta >= 2.
			cadence = rev_delta / delta;
			crank_time_last_received = millis();
			crank_time_last = crank_time;
		} else {
			if (millis()-crank_time_last_received > 600) {
				cadence = 0;
			} else {
				bclog.log(BCLogger::Log_Info, BCLogger::TAG_BLE, "Ignore 0 revolutions since last update is less than 600ms");
			}
		}
		crank_rev_last = crank_rev;
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "Cadence %d revs per minute", cadence);
		break;
	case DEV_HRM:
		if (length < 2)
			return;
		flags = pData[0];
		hr = pData[1];
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "HR Data with length %ul. HR 8bit: %x FLAG: %X", length,	hr, flags);
		if ((pData[0] & 1)) {		// LSB flag = 16 bit or 8bit
			hr |= (pData[2] << 8);
		}
		bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "Heart rate: %d ❤ per minute", hr);
		stats.addHR(hr);
		break;
	default:
		bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "Notify Callback for %d\n", ctype);
		bclog.log(BCLogger::Log_Warn, BCLogger::TAG_BLE, pBLERemoteCharacteristic->toString().c_str());
	}
}

void BLEDevices::komootLoop() {
	if (millis() < 6000) return;
	if (pKomootRemoteCharacteristic) {
		std::string value = pKomootRemoteCharacteristic->readValue();
		bclog.logf(BCLogger::Log_Debug, BCLogger::TAG_BLE, "Read komoot string with %d bytes", value.length());
		if (value.length() > 4) {
			//in case we have update flag but characteristic changed due to navigation stop between
			std::string street;
			street = value.substr(9); //this causes abort when there are not at least 9 bytes available
			std::string direction;
			direction = value.substr(4, 4);
			uint8_t d = direction[0];
			std::string distance;
			distance = value.substr(5, 8);
			uint32_t dist = distance[0] | distance[1] << 8 | distance[2] << 16 | distance[3] << 24;
			bclog.logf(BCLogger::Log_Info, BCLogger::TAG_BLE, "Komoot-Navigation: %d m in Richtung %0x auf %s", dist, d, street.c_str());
		} else {
			bclog.log(BCLogger::Log_Error, BCLogger::TAG_BLE, "Less than 10 byte received from komoot");
		}
	}
}

void BLEDevices::connCheckLoop() {
	if (millis() < 6000) return;

	if (!scanning) reconnCount++;
	for (uint16_t c = 0; c < DEV_COUNT; c++) {
		if (doConnect[c]) connectToServer(static_cast<EDevType>(c));		// check if new connection shall be established
		if (connState[c] == CONN_CONNECTED && !pClient[c]->isConnected()) { // check if existing connection is lost
			connState[c] = CONN_LOST;
			bclog.logf(BCLogger::Log_Warn, BCLogger::TAG_BLE, "%s Lost connection.", DEV_EMOJI[c]);
			if (c == DEV_FL) {
				flparser.setConnState(FLClassicParser::FL_STATE_LOST);
				stats.setConnected(false);
			}
			reconnCount += 16;
		}
	}
	if (reconnCount >= 40) {
		reconnCount = 0;
		startBLEScan();
	}
}
