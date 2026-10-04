/*
 * I2CSensors.h
 *
 *  Created on: 03.01.2024
 *      Author: ian
 */

#pragma once

#ifdef TRGBBC_SENSORS_I2C
#include "SparkFunBME280.h"

#include <SPI.h>        // needed so that pio can find the SPI include in BMI160 library
#include <BMI160Gen.h>
#include <Ticker.h>
#include <Preferences.h>
#include <SimpleCLI.h>
#include <atomic>
#include "RoadQuality.h"

class I2CSensors {
public:
	I2CSensors();

	void setup() {
		initBME280();
		initBMI160();
	}

	uint16_t getHTMLPage(String& htmlresponse);

	//uint16_t procHTMLCmd(String& htmlresponse, const String& cmd, const String& arg);
	void readBME280();

	// BMI160 debug page (/debug/imu) and its live data (/debug/imu.json). Both only read a
	// snapshot the ImuTask publishes -- no I2C access from the web server's context.
	void getIMUDebugPage(String& html);
	void getIMUJson(String& json);
	// Only raise a flag; the ImuTask does the actual work (async_tcp's stack is tight).
	bool requestIMUCalibration();		// false if the sensor isn't running or a calibration is already underway
	void requestIMUMinMaxReset() {imuResetMinMaxRequest = true;}

	// ---- Road quality and accelerometer gradient (see RoadQuality.h) ----
	// For Statistics and the UI; all safe to call from any task.
	enum GradSource : uint8_t {GRAD_BARO = 0, GRAD_IMU = 1};
	uint8_t getRoadClass() const {return roadClass.load();}		// 0 = not rated, 1..5
	float getRoughness();					// R of the last interval, NAN if not rated
	uint32_t getShockCount();				// shock records written since boot
	float getImuGradient();					// %, bias-corrected; NAN if no current estimate
	// true (and pct set) if the display should show the accelerometer gradient right now:
	// source set to IMU, and the estimate is current and its bias has converged.
	bool imuGradientForDisplay(float& pct);
	// Statistics::calculateGradient() hands over each barometric gradient: it is the
	// reference the accelerometer's mounting offset is learned against.
	void feedBaroGradient(float pct, float deltaDistM);
	bool requestRefRide(bool start);		// false if the IMU isn't running
	// Raw capture on demand (R_*.bin, RawCapture.h): seconds 1..RAW_MAX_CAPTURE_S, 0 = stop.
	bool requestRawCapture(uint16_t seconds);

	// ---- Manual road label (RQ-Ride screen) ----
	// The rider's own rating of the surface, logged as ground truth next to the automatic
	// road class: a LogRec::Label record on every change (and every 60 s while set), and in
	// every shock record and raw-capture block. Starts unset at boot. Safe to call from any
	// task, including LVGL event callbacks -- they only set a request, the ImuTask logs it
	// within ~50 ms. All return false if the IMU isn't running (nothing gets logged then).
	//   surface  LogRec::Surface (0 = not set, 1 Asphalt, 2 Schotter, 3 Waldweg, 4 Feldweg,
	//            5 Pflaster, 6 Sonstiges); quality 0 = not set, 1 (best) .. 4 (worst)
	// Out-of-range values are rejected (false), not clamped.
	bool setRoadLabel(uint8_t surface, uint8_t quality);
	bool setRoadLabelSurface(uint8_t surface);		// keeps the quality
	bool setRoadLabelQuality(uint8_t quality);		// keeps the surface
	// Record button: open-ended raw capture. It is written in files of RAW_MAX_CAPTURE_S
	// each (R_*_01, _02, ...): a new one follows the moment a file is complete, until
	// stopRoadCapture() -- or the end of the log session (BCLogger::rotateSession()).
	// Test ride 2026-10-04: the capture ended on its own every 30 minutes.
	bool startRoadCapture();
	bool stopRoadCapture() {return requestRawCapture(0);}
	struct RoadLabelState {
		uint8_t surface = 0, quality = 0;		// as requested (shows the tap at once, before the ImuTask logged it)
		bool imuRunning = false;
		bool capturing = false;					// raw capture running
		float captureS = 0;						// length so far
		float captureDistM = 0;					// ridden since the capture started
		float labelDistM = 0;					// ridden under the current label (since its last change)
		uint32_t labelMs = 0;					// time under the current label
	};
	RoadLabelState getRoadLabelState();
	static const char* surfaceName(uint8_t surface);	// German display name, "" for 0
	void requestPitchReset() {pitchResetRequest = true;}

	// ---- Calibration state for the on-device settings screen ----
	// IMU calibration (3 s at standstill, requestIMUCalibration()) and reference ride
	// (requestRefRide()), both startable without the web page. Safe to call from any task.
	enum ImuCalState : uint8_t {CAL_NONE = 0, CAL_RUNNING, CAL_OK, CAL_ERR_MOTION, CAL_ERR_SCALE};
	struct CalibrationState {
		bool imuRunning = false;
		ImuCalState calState = CAL_NONE;	// last attempt; CAL_OK also for a calibration loaded at boot
		uint8_t calPercent = 0;				// of a running calibration
		bool calValid = false;				// a stored calibration is in use
		time_t calTime = 0;					// when it was taken, 0 = clock wasn't set
		RQ::RoadQuality::RefState refState = RQ::RoadQuality::RefState::IDLE;
		uint16_t refProgressS = 0;			// accepted seconds so far
		uint16_t refTargetS = 0;			// needed, at >= refMinKmh
		float refMinKmh = 0;
		bool baselineCal = false;			// baseline from a reference ride, else the default
		float baselineG = 0;
		time_t baselineTime = 0;
	};
	CalibrationState getCalibrationState();

	float getHeight() const;			// barometric; the simulator build can override it (SimSensors::getHeight())

	// ---- Height calibration (BME280 reference pressure) ----
	// The barometric height is derived from the measured pressure and a reference pressure
	// at sea level (QNH); calibrating means finding that reference. Three presets (known
	// places, e.g. home) can be stored and applied with one tap. All safe to call from any
	// task: they only set the reference in memory; the NVS write (one blob, see NvsUtil.h)
	// follows in the next BME280 cycle, so no caller needs NVS stack.
	static constexpr uint8_t HEIGHT_PRESET_COUNT = 3;
	static constexpr float HEIGHT_MIN_M = -430.0f, HEIGHT_MAX_M = 9000.0f;		// Dead Sea .. above any road
	static constexpr float SEA_LEVEL_MIN_HPA = 850.0f, SEA_LEVEL_MAX_HPA = 1090.0f;
	enum class HeightCalResult : uint8_t {OK = 0, NO_PRESSURE, OUT_OF_RANGE, NO_GPS_HEIGHT};
	struct HeightCalState {
		float pressHPa = NAN;				// measured, NAN until the first reading
		float heightM = NAN;				// as displayed
		float seaLevelHPa = 1013.0f;		// reference pressure at sea level in use
		float presetM[HEIGHT_PRESET_COUNT] = {};
	};
	HeightCalState getHeightCalState() const;
	HeightCalResult calibrateHeight(float knownHeightM);		// "I am at this height now"
	// Height above sea level from TrailBridge's GPS fix (MSL_ALTITUDE_DM, real fix not older than
	// GPS_HEIGHT_MAX_AGE_MS). usedHeightM gets the value that was applied. Phone GPS is only good
	// for about +-10 m vertically -- a rough start, not a replacement for a known place.
	static constexpr uint32_t GPS_HEIGHT_MAX_AGE_MS = 10000;
	bool getGpsHeight(float& heightM) const;					// false: no usable GPS height right now
	HeightCalResult calibrateHeightFromGps(float* usedHeightM = nullptr);
	HeightCalResult setSeaLevelPressure(float hPa);				// reference pressure directly
	HeightCalResult setHeightPreset(uint8_t index, float heightM);
	float getHeightPreset(uint8_t index) const;					// NAN for an invalid index
	static const char* heightCalResultText(HeightCalResult r);	// English, for the web page
	float getHumid() const {return humid;}
	float getPress() const {return press;}
	float getTemp() const {return temp;}

private:
	BME280 bme280;
	void initBME280();
	void initBMI160();

	float press = NAN, humid = NAN, temp = NAN, height = NAN;
	float refPres = 101300.0;		// Pa, reference pressure at sea level
	float heightPresets[HEIGHT_PRESET_COUNT] = {359.0f, 320.0f, 0.0f};
	std::atomic<bool> heightCalDirty{false};
	void loadHeightCal(Preferences& prefs);
	void persistHeightCal();
	static constexpr float BARO_SCALE_M = 44330.77f, BARO_EXPONENT = 0.190263f;		// height = f(p / pRef)

	Ticker bme280Cycle;

	// ---------------- BMI160: FIFO acquisition and static calibration ----------------
	// Accelerometer only, 400 Hz, +/-16 g, read in bursts from the sensor's 1024 byte FIFO
	// (headerless: 6 byte per frame = 170 frames = 425 ms of buffer). The gyro is not in the
	// FIFO; it is only offset-calibrated so its values are usable later.
	static constexpr uint8_t IMU_I2C_ADDR = 0x68;
	static constexpr uint16_t IMU_ODR_HZ = 400;
	static constexpr float IMU_LSB_PER_G = 2048.0f;			// +/-16 g range
	static constexpr uint32_t IMU_POLL_MS = 50;				// ~20 frames = 120 byte per poll
	static constexpr uint16_t IMU_FRAME_BYTES = 6;
	// Arduino-ESP32's Wire buffer is 128 byte (I2C_BUFFER_LENGTH): a larger requestFrom() is
	// silently truncated. Read in chunks of whole frames below that, so no frame is ever split.
	static constexpr uint16_t IMU_CHUNK_BYTES = 20 * IMU_FRAME_BYTES;	// 120
	// The FIFO holds 1024 byte; at or above this fill level it may already have dropped
	// frames, so it is counted as an overflow and flushed.
	static constexpr uint16_t IMU_FIFO_OVERFLOW_BYTES = 1024 - 2 * IMU_FRAME_BYTES;
	static constexpr uint16_t IMU_CAL_SAMPLES = 3 * IMU_ODR_HZ;		// 3 s
	static constexpr float IMU_CAL_MAX_SIGMA_G = 0.015f;			// stillness check per axis
	static constexpr float IMU_CAL_SCALE_MIN = 0.9f, IMU_CAL_SCALE_MAX = 1.1f;

	static const char* const CAL_STATE_STRING[];

	// Everything the web page shows. Written by the ImuTask, copied out under imuMux.
	struct ImuSnapshot {
		bool running = false;			// sensor found and ImuTask started
		uint8_t deviceId = 0;
		float samplesPerSec = 0;
		uint16_t fifoFill = 0, fifoFillMax = 0;
		uint32_t overflows = 0;
		uint32_t totalSamples = 0;
		uint32_t i2cErrors = 0;			// short/failed FIFO reads
		uint32_t invalidFrames = 0;		// 0x8000 "FIFO empty" frames dropped
		float last[3] = {0, 0, 0};		// [g], scale-corrected
		float mean[3] = {0, 0, 0};		// last 1 s window
		float sigma[3] = {0, 0, 0};
		float magMean = 0;				// mean |a| over the last 1 s window
		float min[3] = {0, 0, 0}, max[3] = {0, 0, 0};	// since last reset
		float magMax = 0;
		ImuCalState calState = CAL_NONE;
		uint16_t calProgress = 0;		// samples collected in the running calibration
		float calMeasuredSigma[3] = {0, 0, 0};	// of the last attempt, also a failed one
	};
	// Persisted as one blob in Preferences namespace "RoadQ" (shared with the road-quality code).
	struct ImuCalibration {
		bool valid = false;
		float g0[3] = {0, 0, 0};		// gravity vector at rest [g], before scale correction
		float scale = 1.0f;				// 1 g / |g0|
		float sigma[3] = {0, 0, 0};		// noise floor per axis [g]
		int16_t gyroOffset[3] = {0, 0, 0};	// BMI160 offset registers, 0.061 deg/s per LSB
		time_t calTime = 0;				// 0 = no valid wall-clock time at calibration
	};

	void imuTask();
	void imuProcessSample(const int16_t raw[3], int64_t sampleEpochMs);
	void imuFinishCalibration();
	void loadIMUCalibration();
	void storeIMUCalibration();

	TaskHandle_t imuTaskHandle = nullptr;
	portMUX_TYPE imuMux = portMUX_INITIALIZER_UNLOCKED;
	ImuSnapshot imuSnap;				// guarded by imuMux
	ImuCalibration imuCal;				// guarded by imuMux (written by ImuTask, read by the web page)
	std::atomic<bool> imuCalRequest{false};
	std::atomic<bool> imuResetMinMaxRequest{false};

	// ImuTask-private working state (never touched by other tasks)
	struct {
		double sum[3], sumSq[3], sumMag;
		uint32_t n;
		uint32_t startMs;
	} imuWin = {};
	struct {
		double sum[3], sumSq[3];
		uint16_t n;
	} imuCalAcc = {};
	bool imuCalActive = false;
	// Published to imuSnap once per poll rather than per sample (fewer critical sections)
	float imuLast[3] = {0, 0, 0}, imuMin[3] = {0, 0, 0}, imuMax[3] = {0, 0, 0};
	float imuMagMax = 0;
	uint32_t imuTotalSamples = 0;
	uint32_t imuI2cErrors = 0, imuInvalidFrames = 0;
	// I2C error breakdown (ImuTask only), logged once a minute when non-zero: errors seen on
	// the ride (2026-09-27, ~10/min) but not on the desk -- which phase fails says whether
	// it is the bus/contact (address NACK) or a read cut short.
	uint32_t imuErrWrite = 0, imuErrShort = 0, imuLostFrames = 0;
	uint8_t imuErrLastCode = 0;			// Wire.endTransmission() result of the last write-phase error
	uint32_t imuErrLogMs = 0, imuErrLogged = 0;
	bool imuRead(uint8_t reg, uint8_t* buf, uint16_t n);
	bool imuMinMaxValid = false;

	// ---------------- Road quality, shocks, accelerometer gradient ----------------
	// Settings the user can change (CLI "rq ..."), persisted in "RoadQ". Guarded by imuMux;
	// the ImuTask picks changes up via rqSettingsChanged.
	struct RqSettings {
		uint8_t intervalS = 2;
		float shockAbsG = 3.0f;
		float wheelbaseM = 1.05f;
		uint8_t gradSrc = GRAD_BARO;		// accelerometer gradient is opt-in until verified on real rides
	};
	// Everything the web page and the UI show. Written by the ImuTask, guarded by imuMux.
	struct RqSnapshot {
		bool haveInterval = false;
		RQ::IntervalResult last;
		bool haveShock = false;
		RQ::ShockResult lastShock;
		time_t lastShockTime = 0;
		uint32_t shocks = 0, suppressed = 0, rqRecords = 0;
		float rms1sG = 0;
		bool still = false;
		float speedKmh = NAN;
		uint8_t speedSrc = 0;				// RQ::SpeedSource
		RQ::RoadQuality::RefState refState = RQ::RoadQuality::RefState::IDLE;
		uint16_t refProgressS = 0, refElapsedS = 0;
		float baselineG = 0;
		bool baselineCal = false;
		time_t baselineTime = 0;
		// pitch
		bool efValid = false;
		float ef[3] = {0, 0, 0};
		float slope = 0, biasDeg = 0, biasDistM = 0;
		bool biasConverged = false;
		bool gradValid = false, frozen = false;
		float gradImu = NAN, gradRaw = NAN, gradBaro = NAN;
		uint32_t pitchUpdates = 0;
	};
	static constexpr uint32_t PITCH_SAVE_INTERVAL_MS = 10 * 60 * 1000;	// NVS wear: learned state at most every 10 min

	RQ::RoadQuality roadq;					// ImuTask only
	RQ::PitchEstimator pitch;				// ImuTask only
	RqSettings rqSettings;					// guarded by imuMux
	RqSnapshot rqSnap;						// guarded by imuMux
	std::atomic<bool> rqSettingsChanged{false};
	std::atomic<uint8_t> refRequest{0};		// 1 = start, 2 = cancel
	std::atomic<bool> pitchResetRequest{false};
	std::atomic<uint8_t> roadClass{0};
	struct {bool pending; float pct; float distM;} baroMail = {};	// guarded by imuMux
	time_t baselineTime = 0;				// ImuTask only (mirrored into rqSnap)

	// ImuTask-private
	float blockSum[3] = {0, 0, 0};
	uint32_t blockN = 0;
	uint32_t lastSpeedUpdateSeen = 0;
	int64_t intervalStartEpochMs = 0;
	uint32_t pitchSavedMs = 0;
	RQ::PitchEstimator::State pitchSaved;
	RQ::RoadQuality::RefState refPrev = RQ::RoadQuality::RefState::IDLE;
	float curSpeedKmh = NAN;				// as set in rqBeforeBurst(), for the raw block headers
	uint8_t curSpeedSrc = 0;				// RQ::SpeedSource
	uint32_t curSpeedUpdMs = 0;

	// ---------------- Raw data: capture on demand, shock snippets ----------------
	static constexpr uint16_t RAW_MAX_CAPTURE_S = 1800;	// 30 min, ~4.5 MB

	// ---------------- Manual road label ----------------
	static constexpr uint32_t LABEL_REFRESH_MS = 60000;
	std::atomic<uint16_t> labelWanted{0};	// surface | quality << 8, set by any task
	struct LabelSnapshot {
		float captureDistM = 0, labelDistM = 0;
		uint32_t labelMs = 0;
	} labelSnap;							// guarded by imuMux
	// ImuTask-private
	uint16_t labelActive = 0;				// as last logged
	uint32_t labelSinceMs = 0, labelWrittenMs = 0, labelSeq = 0;
	float labelDistM = 0, captureDistM = 0;
	void labelBeforeBurst(uint32_t nowMs);	// logs a change, or the 60 s refresh
	void labelWrite(uint8_t reason, uint16_t prev, uint32_t nowMs);
	uint8_t labelSurface() const {return labelActive & 0xFF;}
	uint8_t labelQuality() const {return labelActive >> 8;}
	static constexpr uint16_t SNIP_PRE_MS = 250, SNIP_POST_MS = 500;
	static constexpr uint16_t SNIP_PRE = SNIP_PRE_MS * IMU_ODR_HZ / 1000;		// 100 frames
	static constexpr uint16_t SNIP_POST = SNIP_POST_MS * IMU_ODR_HZ / 1000;		// 200 frames
	static constexpr uint16_t SNIP_FRAMES = SNIP_PRE + SNIP_POST;
	static constexpr uint16_t RAW_RING = 320;	// frames of history (0.8 s): a snippet is taken right at peak + SNIP_POST, so SNIP_FRAMES suffice
	std::atomic<bool> rawChain{false};		// startRoadCapture(): follow a complete file with the next one
	std::atomic<int32_t> rawRequest{-1};	// -1 = none, 0 = stop, n = start n seconds
	struct RawSnapshot {
		bool capturing = false;
		uint32_t captureFrames = 0, captureTarget = 0, captureBlocks = 0;
		uint32_t snippets = 0, snippetsDropped = 0;
	} rawSnap;								// guarded by imuMux
	// ImuTask-private
	bool rawCapturing = false;
	uint32_t rawCaptureLeft = 0, rawCaptureFrames = 0, rawCaptureTarget = 0, rawBlockNo = 0;
	bool rawGap = false;					// samples lost since the last capture block
	bool snipFileOpen = false;
	uint16_t snipFileSession = 0;		// BCLogger::getRawSessionNo() when it was opened
	bool snipPending = false;
	uint32_t snipPeakIdx = 0, snipSeq = 0;
	float snipSpeedKmh = NAN;
	uint32_t snippets = 0, snippetsDropped = 0;
	int16_t rawRing[RAW_RING][3] = {};
	void rawFileHeader(uint8_t kind, uint8_t* out);
	void rawBeforeBurst();					// start/stop requests
	void rawCaptureChunk(const uint8_t* frames, uint16_t n, int64_t firstEpochMs);
	void rawAfterSample(const int16_t raw[3], int64_t sampleEpochMs);
	void rawWriteSnippet(int64_t lastEpochMs);
	void rawPublish();

	void rqLoad();							// settings, baseline and learned pitch state from "RoadQ"
	RQ::Config rqConfig(const RqSettings& s) const;
	void rqSaveBaseline();
	void rqSavePitch(uint32_t nowMs);
	void rqBeforeBurst(uint32_t nowMs);		// once per poll: requests, speed, barometer
	void rqAfterBurst(uint32_t nowMs);		// once per poll: pitch block, snapshot
	void rqHandleResults(uint8_t ready, int64_t sampleEpochMs);
	void rqPublish(uint32_t nowMs);

	Command rqCmd;
	void handleRqCommand(const Command& cmd);
	void printRqStatus();
};

#endif
