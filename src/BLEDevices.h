/*
 * BLEDevices.h
 *
 *  Created on: 18.02.2023
 *      Author: ian
 */

#pragma once

#include <vector>
#include <memory>

#include <BLEScan.h>
#include <BLEUtils.h>
#include <Arduino.h>
#include <Ticker.h>
#include <FLClassicParser.h>
#include <Preferences.h>
#include "BikeGpsProtocol.h"

class BLEDevices: public BLEAdvertisedDeviceCallbacks, BLEClientCallbacks {

typedef enum {
	DEV_UNKNOWN = -1,
	DEV_HRM = 0,
	DEV_CSC_1,
	DEV_CSC_2,
	DEV_FL,
	DEV_NAV,
	DEV_COUNT
} EDevType;

typedef enum {
	CONN_DEV_NOTFOUND,
	CONN_ADVERTISED,
	CONN_CONNECTED,
	CONN_LOST,
	CONN_COUNT
} EBLEConnState;

static const char* DEV_EMOJI[DEV_COUNT];
static const char* DEV_STRING[DEV_COUNT];
static const char* CONN_STRING[CONN_COUNT];

private:
	struct SDevToConnect {
	    std::unique_ptr<BLEAdvertisedDevice> advDev;  // BLE-Client für die Verbindung
	    EDevType devType;
	};
	// Refactoring Scan & Connect: New variables and methods:
	std::vector<SDevToConnect> connectDevices;		// Devices to be connected (filtered results from scan)
	//std::unique_ptr<BLEClient> clients[DEV_COUNT];						// BLEClients objects of connected devices
	std::array<std::unique_ptr<BLEClient>, DEV_COUNT> clients;
	BLEAddress *pStoredAddress[DEV_COUNT];

	SemaphoreHandle_t xDevMutex = nullptr;		// Mutex to control access to data structures for device handling

	void scanAndConnectTask();
	TaskHandle_t scanTaskHandle = nullptr;
	BLEScan* pBLEScan = nullptr;

	//BLEAddress *pServerAddress[DEV_COUNT];

	//BLEClient  *pClient[DEV_COUNT];  // FIXME: To be removed (or refactored)

	bool doConnect[DEV_COUNT] = {false, false, false, false, false};
	bool hasBatService[DEV_COUNT] = {true, true, true, false, false};
	int8_t batLevel[DEV_COUNT] = {-1, -1, -1, -1, -1};
	EBLEConnState connState[DEV_COUNT] = {CONN_DEV_NOTFOUND, CONN_DEV_NOTFOUND, CONN_DEV_NOTFOUND, CONN_DEV_NOTFOUND, CONN_DEV_NOTFOUND};
	Preferences StatPreferences;

	static const int scanTime = 8; //In seconds
	bool connectToServer(SDevToConnect& dev);

	virtual void onConnect(BLEClient *pClient);
	virtual void onDisconnect(BLEClient *pClient);
	void updateDisconnectedDev(const EDevType dt);

	String bufferFL;

    // CSC related
	uint32_t speed_rev = 0;
	uint16_t crank_rev_last = 0, crank_time_last=0;
	time_t crank_time_last_received = 0;
	uint16_t cadence = 0;
	bool cscIsSpeed[2] = {false, false};

	int32_t nav_distance = 0, nav_distance_int = 0;

	uint8_t reconnCount = 0;

	void handleNavData(const uint8_t* pData, size_t length);

	// GPS-Positions-Service: second, independent service on the same peer as DEV_NAV
	// (not advertised, see PROTOCOL.md "GPS-Positions-Service") -- so it has no EDevType/
	// BLEClient of its own, it's subscribed on the already-connected DEV_NAV client.
	SGpsFix gpsFix;
	uint32_t gpsFixReceivedMillis = 0;
	void subscribeGpsPosition(BLEClient* pClient);
	void handleGpsData(const uint8_t* pData, size_t length);

	void checkBatteries();
	int8_t readBatLevel(const EDevType dt);


	void startBLEScan();
	void restoreAdresses();
	void storeAdress(EDevType type, BLEAddress& addr);
	void resetAdress(EDevType type);

	EDevType nextCSCSlotAvailable();

	EDevType filterDevice(BLEAdvertisedDevice& dev);
	bool isAlreadyConnected(BLEAdvertisedDevice& newDevice);


public:
	BLEDevices();

	static const BLEUUID serviceUUID[DEV_COUNT];
	static const BLEUUID charUUID[DEV_COUNT];
	static const BLEUUID serviceUUIDBat;
	static const BLEUUID serviceUUIDExposure;

	static const BLEUUID charUUIDBat;

	static const BLEUUID gpsServiceUUID;
	static const BLEUUID gpsCharUUID;

	// Latest known GPS fix (or invalid, if none received / lost with the DEV_NAV connection).
	// fixAgeMs is updated to reflect the time elapsed since it was received over BLE.
	SGpsFix getGpsFix() const;

	// Interface BLEAdvertisedDeviceCallbacks
	void onResult(BLEAdvertisedDevice advertisedDevice);

	// helper functions called from lambda
	void notifyCallbackCSC( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify, EDevType ctype);

	uint16_t getHTMLPage(String& htmlresponse);
	uint16_t procHTMLCmd(String& htmlresponse, const String& cmd, const String& arg);


	void setup();

	static void taskInit(void * _thisInstance);
};
