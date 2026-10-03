/*
 * BLEDevices.h
 *
 *  Created on: 18.02.2023
 *      Author: ian
 */

#pragma once

#include <vector>
#include <memory>
#include <atomic>

#include <BLEScan.h>
#include <BLEUtils.h>
#include <Arduino.h>
#include <Ticker.h>
#include <FLClassicParser.h>
#include <Preferences.h>
#include "BikeGpsProtocol.h"

class BLEDevices: public BLEAdvertisedDeviceCallbacks, BLEClientCallbacks {

public:
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

private:
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
	BLEAddress *pStoredAddress[DEV_COUNT] = {nullptr};	// filterDevice() treats nullptr as "slot free" - don't leave this indeterminate

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
	// Indexed by EDevType (DEV_CSC_1/DEV_CSC_2 = 1/2). Was [2] indexed with 1/2 until 2026-09-27:
	// cscIsSpeed[2] then overwrote the low byte of nav_distance right below on every CSC2
	// notification, so the wheel-interpolated nav distance on Main/RQ dropped to 0 m (under
	// 256 m) or by up to 255 m until the next nav frame repaired it.
	bool cscIsSpeed[DEV_COUNT] = {};
	uint8_t cscKind[DEV_COUNT] = {};		// 0 unknown, 1 speed, 2 cadence: set by the first notification

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

	// Elevation-profile service: third service on the DEV_NAV peer, same pattern as the GPS
	// one. Frames go to ClimbMonitor (climb), which also gets the nav frames' remaining
	// route distance -- the rider's position in the profile.
	void subscribeProfile(BLEClient* pClient);

	// millis() of the last nav or GPS frame, for checkNavAlive(). Six heartbeats.
	static constexpr uint32_t NAV_TIMEOUT_MS = 30000;
	std::atomic<uint32_t> navLastFrameMs{0};
	void checkNavAlive();

	void checkBatteries();
	int8_t readBatLevel(const EDevType dt);


	void startBLEScan();
	void restoreAdresses();
	void storeAdress(EDevType type, BLEAddress& addr);
	void resetAdress(EDevType type);
	static void forgetTask(void* arg);

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
	static const BLEUUID profileServiceUUID;
	static const BLEUUID profileCharUUID;

	// Latest known GPS fix (or invalid, if none received / lost with the DEV_NAV connection).
	// fixAgeMs is updated to reflect the time elapsed since it was received over BLE.
	SGpsFix getGpsFix() const;

	// Interface BLEAdvertisedDeviceCallbacks
	void onResult(BLEAdvertisedDevice advertisedDevice);

	// helper functions called from lambda
	void notifyCallbackCSC( BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify, EDevType ctype);
#ifdef BC_SIM
	// SimSensors: a notification as a real sensor sends it, through notifyCallbackCSC() --
	// speed in slot CSC_1, cadence in CSC_2. Real sensors of these kinds are ignored meanwhile.
	void simNotifyCSC(bool speed, uint8_t* pData, size_t length) {notifyCallbackCSC(nullptr, pData, length, true, speed ? DEV_CSC_1 : DEV_CSC_2);}
	void simNotifyHR(uint8_t* pData, size_t length) {notifyCallbackCSC(nullptr, pData, length, true, DEV_HRM);}
#endif

	// ---- Device status for the settings page ----
	struct DevStatus {
		EBLEConnState state = CONN_DEV_NOTFOUND;
		int8_t battery = -1;			// percent, -1 unknown
		bool hasAddress = false;		// a sensor is remembered in this slot
		uint8_t cscKind = 0;			// CSC slots: 0 not known yet, 1 speed, 2 cadence (from the first data)
	};
	DevStatus getDevStatus(EDevType dt) const;
	// Forget the sensor of a slot (stored address) and drop its connection, so a new one can be
	// taken. Own short task: NVS access and the device mutex don't belong on the UI task.
	// TrailBridge is refused (its address is never stored anyway).
	bool requestForget(EDevType dt);

	uint16_t getHTMLPage(String& htmlresponse);
	uint16_t procHTMLCmd(String& htmlresponse, const String& cmd, const String& arg);


	void setup();

	static void taskInit(void * _thisInstance);
};
