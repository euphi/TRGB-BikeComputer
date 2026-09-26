/*
 * RoadQuality.h
 *
 * Road-surface quality and shock detection from the BMI160 accelerometer, plus a pitch
 * (gradient) estimate from the same data. Pure algorithm: no Arduino, FreeRTOS or I/O --
 * I2CSensors feeds it samples and turns the results into log records. That keeps it
 * testable on the host (test/native_roadquality/) and reproducible offline later.
 *
 * Concept and rationale: the "Wegequalität" plan, summarised here.
 *
 * RoadQuality, per sample (400 Hz, values in g, scale-corrected):
 *   - Gravity direction: slow low-pass of the raw vector (tau 2 s, 0.3 s at standstill),
 *     corrected for the longitudinal acceleration once PitchEstimator knows the forward axis.
 *   - Band-pass 2..80 Hz per axis (Butterworth biquads): removes pedalling/braking/rider
 *     motion below and housing resonances above -- the range ISO 2631 uses for vibration.
 *   - Split into vertical (along gravity), horizontal and total dynamic acceleration.
 *   - Per interval (1..10 s, counted in samples): RMS vertical/horizontal, peaks, VDV
 *     (fourth-power dose, weights shocks), threshold crossings, mean speed, distance, and
 *     a speed-normalised roughness index R = RMS_v / (baseline * (v/v_ref)^k) -> class 1..5.
 *   - Shocks: total dynamic acceleration above max(T_abs, k_rel * RMS_v of the last second),
 *     with hysteresis; a second peak 40..400 ms later is merged in (rear wheel) and checked
 *     against the wheelbase. Rate-limited.
 *   - Reference ride: median of the normalised RMS over ~60 s of smooth road -> baseline.
 *
 * PitchEstimator, per 50 ms block and per wheel-speed update:
 *   Along the forward axis e_f the sensor reads f.e_f = dv/dt + g*sin(theta). dv/dt comes
 *   from the wheel sensor, e_f is learned from accelerating/braking, and the remaining
 *   offset (mounting angle) is learned slowly against the barometric gradient. Result: a
 *   gradient that reacts within ~1 s without the barometer's noise, and without drift.
 */

#pragma once

#include <cstdint>
#include <cmath>

namespace RQ {

static constexpr float G_MS2 = 9.80665f;

// Flags of an interval result (and LogRec::RoadQuality::flags -- same values, see LogRecords.h)
enum IntervalFlags : uint8_t {
	IF_TOO_SLOW       = 0x01,	// speed known but below vMin: not rated
	IF_DATA_GAP       = 0x02,	// FIFO overflow during the interval (samples lost)
	IF_CLIPPED        = 0x04,	// at least one sample at the +/-16 g limit
	IF_UNCALIBRATED   = 0x08,	// baseline is the default, no reference ride yet
	IF_REF_RIDE       = 0x10,	// reference ride was running
	IF_SPEED_FROM_GPS = 0x20,	// wheel speed unavailable, phone GPS speed used
	IF_NO_SPEED       = 0x40,	// no speed at all: not rated
	IF_GPS_VALID      = 0x80,	// set by the caller (position in the record is usable)
};

// Flags of a shock result (and LogRec::Shock::flags)
enum ShockFlags : uint8_t {
	SF_CLIPPED          = 0x01,
	SF_WHEELBASE_MATCH  = 0x02,	// second peak delay * speed ~ wheelbase: front + rear wheel
	SF_STANDSTILL       = 0x04,	// speed known and ~0 (bike dropped, kerb while pushing ...)
	SF_GPS_VALID        = 0x08,	// set by the caller
	SF_AFTER_SUPPRESSED = 0x10,	// events were suppressed by the rate limit before this one
	SF_NO_SPEED         = 0x20,
};

enum class SpeedSource : uint8_t {NONE = 0, WHEEL, GPS};

struct Config {
	float odrHz = 400.0f;
	uint8_t intervalS = 2;				// 1..10
	// Filters
	float hpHz = 2.0f;
	float lpHz = 80.0f;
	float gravityTauS = 2.0f;
	float gravityTauStillS = 0.3f;
	float noiseG = 0.005f;				// sensor noise floor (from the static calibration)
	// Roughness index
	float vRefKmh = 20.0f;
	float speedExp = 0.8f;				// SmartRoadSense: RI ~ v^0.76..0.81
	float vMinKmh = 6.0f;
	float baselineG = 0.15f;			// RMS_v at v_ref on smooth asphalt; default is a guess
	bool baselineCalibrated = false;
	float classThr[4] = {1.5f, 2.5f, 4.0f, 7.0f};	// R boundaries of class 1|2|3|4|5
	float t1G = 1.0f, t2G = 2.0f;		// threshold crossings counted per interval
	// Shocks
	float shockAbsG = 3.0f;
	float shockRelFactor = 6.0f;
	float shockOffFactor = 0.5f;		// hysteresis: below T_off = factor * T_on counts as "over"
	uint16_t shockEndMs = 30;			// below T_off this long ends the first peak
	uint16_t shockMaxMs = 500;			// longer than this is forced to end (continuous rough ground)
	uint16_t secondPeakMinMs = 40, secondPeakMaxMs = 400;
	float wheelbaseM = 1.05f, wheelbaseTolM = 0.3f;
	uint8_t maxEventsPerMin = 20;		// <= MAX_RATE_SLOTS
	float sev2G = 5.0f, sev3G = 8.0f;
	float clipG = 15.9f;
	// Reference ride
	float refMinKmh = 12.0f;
	uint16_t refSeconds = 60;			// of accepted intervals
	uint16_t refTimeoutS = 600;			// give up after this long (all intervals)
};

struct IntervalResult {
	uint32_t sampleCount = 0;
	float rmsVertG = 0, rmsHorizG = 0;
	float peakVertMaxG = 0, peakVertMinG = 0, peakTotalG = 0;
	float vdvVert = 0;					// m/s^1.75
	float speedKmh = NAN;				// mean over the samples with a speed, NAN if none
	float distanceM = 0;
	float roughness = NAN;				// NAN = not rated
	uint8_t roadClass = 0;				// 0 = not rated, 1 (smooth) .. 5 (very rough)
	uint8_t flags = 0;					// IntervalFlags
	uint16_t countOverT1 = 0, countOverT2 = 0;
	uint16_t eventsLogged = 0, eventsSuppressed = 0;
};

struct ShockResult {
	uint32_t samplesAgo = 0;			// first peak, relative to the sample that finalised the event
	float durationMs = 0;
	float peakTotalG = 0, peakVertMaxG = 0, peakVertMinG = 0, peakHorizG = 0;
	float preRmsG = 0;					// RMS_v of the second before the event
	float speedKmh = NAN;
	float secondPeakG = 0, secondPeakDelayMs = 0;	// 0 = no second peak
	float vdv = 0;						// VDV of the vertical component over the first peak
	float thresholdG = 0;				// T_on in effect
	uint16_t samplesOverThr = 0;
	uint8_t severity = 0;				// 1..3
	uint8_t flags = 0;					// ShockFlags
	uint32_t seq = 0;					// running number of logged events (gaps = none, suppressed ones aren't numbered)
};

// DF2T biquad (RBJ cookbook coefficients)
struct Biquad {
	float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
	float z1 = 0, z2 = 0;
	void setLowpass(float fs, float fc, float q = 0.70710678f);
	void setHighpass(float fs, float fc, float q = 0.70710678f);
	// Set the state to the steady state for a constant input x -- avoids the start-up
	// transient a 1 g gravity step would cause (which would look like a shock).
	void primeDC(float x);
	inline float process(float x) {
		float y = b0 * x + z1;
		z1 = b1 * x - a1 * y + z2;
		z2 = b2 * x - a2 * y;
		return y;
	}
};

class RoadQuality {
public:
	enum Ready : uint8_t {READY_NONE = 0, READY_INTERVAL = 1, READY_SHOCK = 2};
	enum class RefState : uint8_t {IDLE = 0, RUNNING, DONE, FAILED};

	RoadQuality() {configure(Config());}

	// Applies a new configuration. Filter coefficients are recomputed, the running interval
	// and event state are kept (a changed interval length takes effect with the next interval).
	void configure(const Config& cfg);
	const Config& config() const {return cfg;}

	// Current speed; called whenever it is known to have changed (once per 50 ms poll is fine).
	void setSpeed(float kmh, SpeedSource src);
	// Longitudinal acceleration along the forward axis (from PitchEstimator), used to keep
	// "up" pointing up while braking/accelerating. valid=false disables the correction.
	void setLongitudinal(const float fwd[3], float accelG, bool valid);
	// Samples were lost (FIFO overflow) -- flags the running interval.
	void notifyDataGap() {curFlags |= IF_DATA_GAP;}

	// One sample in g. Returns a combination of Ready bits; the results are then available
	// via interval()/shock() until the next call that returns the same bit.
	uint8_t process(const float a[3]);

	const IntervalResult& interval() const {return lastInterval;}
	const ShockResult& shock() const {return lastShock;}

	// Live values for display
	float rmsVert1sG() const {return sqrtf(ms1);}
	uint8_t lastRoadClass() const {return lastInterval.roadClass;}
	const float* gravity() const {return gLp;}
	bool isStill() const {return still;}
	uint32_t shocksLogged() const {return seq;}
	uint32_t shocksSuppressed() const {return suppressedTotal;}

	// Reference ride (calibration stage 3)
	void startReference();
	void cancelReference();
	RefState refState() const {return ref.state;}
	uint16_t refProgressS() const {return ref.count * cfg.intervalS;}
	uint16_t refElapsedS() const {return ref.total * cfg.intervalS;}
	// Acknowledge DONE/FAILED (back to IDLE). The new baseline is already in config().
	void clearReference() {if (ref.state != RefState::RUNNING) ref.state = RefState::IDLE;}

	static const char* refStateString(RefState s);

private:
	static constexpr uint8_t MAX_RATE_SLOTS = 32;
	static constexpr uint16_t MAX_REF_VALUES = 64;

	Config cfg;
	Biquad hp[3], lp[3];
	bool primed = false;
	uint32_t idx = 0;					// running sample counter (wraps after ~124 days at 400 Hz)

	// gravity / up
	float gLp[3] = {0, 0, 1};
	float fwd[3] = {0, 0, 0};
	float longAccelG = 0;
	bool longValid = false;
	float alphaG = 0, alphaGStill = 0, alpha1s = 0, alphaStill = 0;
	float emaDyn2 = 0;					// for still detection
	uint32_t slowSamples = 0;
	bool still = false;

	// speed
	float speedKmh = NAN;
	SpeedSource speedSrc = SpeedSource::NONE;

	// interval accumulators
	uint32_t intervalSamples = 800;
	uint32_t n = 0;
	double sumV2 = 0, sumH2 = 0, sumV4 = 0;
	float pVMax = 0, pVMin = 0, pT = 0;
	double sumSpeed = 0;
	uint32_t nSpeed = 0;
	double dist = 0;
	uint16_t cnt1 = 0, cnt2 = 0;
	bool arm1 = true, arm2 = true;
	uint8_t curFlags = 0;
	uint16_t evLogged = 0, evSuppressed = 0;
	IntervalResult lastInterval;

	// 1 s EMA of v^2 (for the relative shock threshold)
	float ms1 = 0;

	// shock state machine
	enum class ShockState : uint8_t {IDLE, FIRST, WAIT_SECOND};
	ShockState sState = ShockState::IDLE;
	struct {
		uint32_t startIdx, peakIdx, lastAboveIdx, endIdx;
		float thrOn, thrOff, preRms;
		float peakT, vMax, vMin, hMax;
		double sumV4;
		uint16_t over;
		bool clipped;
		float secondG;
		uint32_t secondIdx;
	} ev = {};
	ShockResult lastShock;
	uint32_t seq = 0;
	uint32_t suppressedTotal = 0;
	bool suppressedSinceLast = false;
	uint32_t rateRing[MAX_RATE_SLOTS] = {};
	uint8_t rateHead = 0, rateCount = 0;

	// reference ride
	struct {
		RefState state = RefState::IDLE;
		uint16_t count = 0, total = 0;
		float values[MAX_REF_VALUES];
	} ref;

	uint32_t msToSamples(float ms) const {return (uint32_t)(ms * cfg.odrHz / 1000.0f + 0.5f);}
	uint8_t finishShock();
	void finishInterval();
	uint8_t classify(float r) const;
};


// Gradient from the accelerometer, see the file comment.
class PitchEstimator {
public:
	struct Config {
		float speedLatencyMs = 500.0f;	// CSC speed is the mean over the last ~1 s window: shift the accel window back
		float minKmh = 4.0f;			// below: no estimate
		float learnMinKmh = 8.0f;		// below: no forward-axis / bias learning
		float maxAccelMs2 = 2.0f;		// |dv/dt| above: hold (hard braking, timing errors dominate)
		float maxLateralG = 0.15f;		// sideways component above: hold (curve; the stem turns with the bars)
		float learnMinAccelMs2 = 0.3f;	// forward axis is learned from accelerations at least this strong
		float efMinWeight = 0.02f;		// sum of (dv/dt in g)^2 before the forward axis is trusted
		float efForget = 0.998f;		// per learning update, lets a remounted sensor re-learn
		float biasDistanceM = 300.0f;	// distance constant of the bias learning against the barometer
		float biasMinDistM = 200.0f;	// learned over at least this far before "converged"
		float gravityTauS = 20.0f;		// long-term gravity (for the horizontal split)
		float smoothing = 0.5f;			// EMA factor per speed update on the output
	};
	struct State {						// persisted
		float ef[3] = {0, 0, 0};
		float efWeight = 0;
		float biasRad = 0;
		float biasDistM = 0;
	};

	PitchEstimator() = default;
	void configure(const Config& c) {cfg = c;}
	const Config& config() const {return cfg;}
	void setState(const State& s);
	State state() const;
	void resetLearning();				// forget forward axis and bias
	void setGravityHint(const float g[3]);	// e.g. the calibration's g0 (normalised internally)

	// Sum of the samples of one poll block (g) and the block's end time (ms, monotonic)
	void addBlock(const float sum[3], uint32_t count, uint32_t endMs);
	// A new wheel-speed value arrived at tMs
	void onSpeed(uint32_t tMs, float kmh);
	// A barometric gradient was computed over deltaDistM (Statistics::calculateGradient())
	void onBaroGradient(float gradPct, float deltaDistM);

	bool forwardValid() const {return efValid;}
	const float* forward() const {return ef;}
	bool biasConverged() const {return biasDistM >= cfg.biasMinDistM;}
	// Latest estimate. false if none yet or older than maxAgeMs.
	bool gradient(float& pct, uint32_t nowMs, uint32_t maxAgeMs = 10000) const;
	float rawGradientPct() const {return gradRawPct;}	// without the bias correction
	float biasDeg() const {return biasRad * 57.29578f;}
	float biasDistance() const {return biasDistM;}
	float regressionSlope() const {return efWeight > 0 ? efMagnitude / efWeight : 0;}	// ~1 if e_f is consistent
	float longitudinalAccelG() const {return dvdtG;}
	bool longitudinalValid(uint32_t nowMs) const {return efValid && hasSpeed && (nowMs - lastSpeedMs) < 3000;}
	bool frozen() const {return isFrozen;}
	uint32_t updates() const {return nUpdates;}

private:
	static constexpr uint8_t RING = 64;	// 64 blocks of 50 ms = 3.2 s
	Config cfg;
	struct Block {uint32_t endMs; float sum[3]; uint32_t n;};
	Block ring[RING] = {};
	uint8_t head = 0, filled = 0;

	float gSlow[3] = {0, 0, 0};
	bool gSlowValid = false;
	uint32_t lastBlockMs = 0;

	float M[3] = {0, 0, 0};				// regression accumulator for e_f
	float efWeight = 0, efMagnitude = 0;
	float ef[3] = {0, 0, 0};
	bool efValid = false;

	bool hasSpeed = false;
	uint32_t lastSpeedMs = 0;
	float lastSpeedMs2 = 0;
	float dvdtG = 0;

	float thetaSmooth = 0;
	bool thetaValid = false;
	uint32_t thetaMs = 0;
	float gradRawPct = NAN;
	bool isFrozen = false;
	uint32_t nUpdates = 0;

	float biasRad = 0, biasDistM = 0;
	double sumThetaDs = 0, sumDs = 0;

	bool windowMean(uint32_t fromMs, uint32_t toMs, float out[3]) const;
	void updateForward();
};

}	// namespace RQ
