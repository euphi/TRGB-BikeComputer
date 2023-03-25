/*
 * BLEDevices.h
 *
 *  Created on: 18.02.2023
 *      Author: ian
 */

#pragma once

#include <vector> // Arduino BLE somehow misses this include. So add it here.
#include <BLEScan.h>
#include <BLEUtils.h>
#include <Arduino.h>
#include <Ticker.h>
#include <FLClassicParser.h>

class BLEDevices: public BLEAdvertisedDeviceCallbacks {

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
	CONN_LOST
} EBLEConnState;

static const char* DEV_EMOJI[DEV_COUNT];

private:
	BLEScan* pBLEScan = nullptr;
	BLEAddress *pServerAddress[DEV_COUNT];
	BLEClient  *pClient[DEV_COUNT];

	bool scanning=false;
	bool doConnect[DEV_COUNT] = {false, false, false, false, false};
	bool hasBatService[DEV_COUNT] = {true, true, true, true, true};
	EBLEConnState connState[DEV_COUNT] = {CONN_DEV_NOTFOUND, CONN_DEV_NOTFOUND, CONN_DEV_NOTFOUND, CONN_DEV_NOTFOUND, CONN_DEV_NOTFOUND};

	static const int scanTime = 5; //In seconds
	bool connectToServer(EDevType ctype);

	String bufferFL;

    // CSC related
	uint32_t speed_rev_last = 0;
	uint16_t crank_rev_last, crank_time_last, speed_time_last = 0;
	time_t crank_time_last_received, speed_time_last_received = 0;
	uint16_t cadence = 0;
	float speed;
	bool cscIsSpeed[2] = {false, false};


	uint8_t reconnCount = 0;

	BLERemoteCharacteristic* pKomootRemoteCharacteristic = nullptr;
	Ticker komootTicker;
	Ticker connCheckTicker;
	Ticker batScanTicker;

	void komootLoop();
	void connCheckLoop();
	void batCheckLoop();
	void startBLEScan();

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

	void setup();

};
