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

class BLEDevices: public BLEAdvertisedDeviceCallbacks, BLEClientCallbacks {

typedef enum {
	DEV_UNKNOWN = -1,
	DEV_HRM = 0,
	DEV_CSC_1,
	DEV_CSC_2,
	DEV_FL,
	DEV_KOMOOT,
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
	    std::unique_ptr<BLEAdvertisedDevice> client;  // BLE-Client für die Verbindung
	    EDevType devType;
	};
	// Refactoring Scan & Connect: New variables and methods:
	std::vector<SDevToConnect> connectDevices;		// Devices to be connected (filtered results from scan)
	//std::unique_ptr<BLEClient> clients[DEV_COUNT];						// BLEClients objects of connected devices
	std::array<std::unique_ptr<BLEClient>, DEV_COUNT> clients;


	BLEAddress *pStoredAddress[DEV_COUNT];

	void scanAndConnectTask();
	TaskHandle_t scanTaskHandle;
	BLEScan* pBLEScan = nullptr;

	//BLEAddress *pServerAddress[DEV_COUNT];

	//BLEClient  *pClient[DEV_COUNT];  // FIXME: To be removed (or refactored)

	bool doConnect[DEV_COUNT] = {false, false, false, false, false};
	bool hasBatService[DEV_COUNT] = {true, true, true, false, false};
	int16_t batLevel[DEV_COUNT] = {-1, -1, -1, -1, -1};
	EBLEConnState connState[DEV_COUNT] = {CONN_DEV_NOTFOUND, CONN_DEV_NOTFOUND, CONN_DEV_NOTFOUND, CONN_DEV_NOTFOUND, CONN_DEV_NOTFOUND};
	Preferences StatPreferences;

	static const int scanTime = 8; //In seconds
	bool connectToServer(SDevToConnect& dev);

	virtual void onConnect(BLEClient *pClient);
	virtual void onDisconnect(BLEClient *pClient);

	String bufferFL;

    // CSC related
	uint32_t speed_rev = 0;
	uint16_t crank_rev_last = 0, crank_time_last=0;
	time_t crank_time_last_received = 0;
	uint16_t cadence = 0;
	bool cscIsSpeed[2] = {false, false};

	int32_t nav_distance, nav_distance_int = 0;
	uint32_t nav_timestamp = 0;

	uint8_t reconnCount = 0;

	BLERemoteCharacteristic* pKomootRemoteCharacteristic = nullptr;

	//TickType_t tickCounter[4];
	Ticker komootTicker;
	Ticker connCheckTicker;
	Ticker batScanTicker;


	void init();
	void taskLoop();


	void komootLoop();
	void connCheckLoop();
	void batCheckLoop();
	void startBLEScan();
	void restoreAdresses();
	void storeAdress(EDevType type, BLEAddress& addr);
	void resetAdress(EDevType type);

	EDevType nextCSCSlotAvailable();

	TaskHandle_t bleTaskHandle;
	EDevType filterDevice(BLEAdvertisedDevice& dev);
	bool isAlreadyConnected(BLEAdvertisedDevice& newDevice);


public:
	BLEDevices();

	static const BLEUUID serviceUUID[DEV_COUNT];
	static const BLEUUID charUUID[DEV_COUNT];
	static const BLEUUID serviceUUIDBat;
	static const BLEUUID serviceUUIDExposure;

	static const BLEUUID charUUIDBat;

	// Interface BLEAdvertisedDeviceCallbacks
	void onResult(BLEAdvertisedDevice advertisedDevice);

	// helper functions called from lambda
	void notifyCallbackCSC( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify, EDevType ctype);

	uint16_t getHTMLPage(String& htmlresponse);
	uint16_t procHTMLCmd(String& htmlresponse, const String& cmd, const String& arg);


	void setup();

	static void taskInit(void * _thisInstance);
};
