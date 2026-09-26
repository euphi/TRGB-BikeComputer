/*
 * Host test for src/RoadQuality.{h,cpp} with synthetic signals -- no hardware, no
 * PlatformIO. Build and run from the repository root:
 *
 *   g++ -std=c++17 -O2 -Wall -Isrc src/RoadQuality.cpp test/native_roadquality/roadquality_test.cpp -o /tmp/rq_test && /tmp/rq_test
 *
 * Exit code 0 = all checks passed. The printed numbers are worth a look when tuning.
 */

#include "RoadQuality.h"

#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

using namespace RQ;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const float FS = 400.0f;

// Rotation matrix: sensor = R * bike. Arbitrary mounting: the sensor's x points to the
// bike's left, its y backwards-and-up by 5 degrees of pitch, z accordingly.
struct Mount {
	float R[3][3];
	explicit Mount(float pitchDeg) {
		const float p = pitchDeg * 3.14159265f / 180.0f;
		// bike frame (x fwd, y left, z up) pitched by p about y, then axes permuted
		const float Rp[3][3] = {{cosf(p), 0, sinf(p)}, {0, 1, 0}, {-sinf(p), 0, cosf(p)}};
		const float P[3][3] = {{0, 1, 0}, {-1, 0, 0}, {0, 0, 1}};		// sensor x = bike y, sensor y = -bike x
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 3; j++) {
				R[i][j] = 0;
				for (int k = 0; k < 3; k++) R[i][j] += P[i][k] * Rp[k][j];
			}
	}
	void apply(const float b[3], float s[3]) const {
		for (int i = 0; i < 3; i++) s[i] = R[i][0] * b[0] + R[i][1] * b[1] + R[i][2] * b[2];
	}
};

// Specific force in the bike frame (g): (dv/dt + sin th, 0, cos th) plus road vibration
static void bikeForce(float dvdtG, float thetaRad, float vibVert, float vibHoriz, float out[3]) {
	out[0] = dvdtG + sinf(thetaRad) + vibHoriz;
	out[1] = 0.3f * vibHoriz;
	out[2] = cosf(thetaRad) + vibVert;
}

struct Collected {
	std::vector<IntervalResult> intervals;
	std::vector<ShockResult> shocks;
};

static void feed(RoadQuality& rq, const float a[3], Collected& c) {
	uint8_t r = rq.process(a);
	if (r & RoadQuality::READY_INTERVAL) c.intervals.push_back(rq.interval());
	if (r & RoadQuality::READY_SHOCK) c.shocks.push_back(rq.shock());
}

// Band-limited road noise: white noise through a 2-pole resonance ~ 25 Hz
struct RoadNoise {
	std::mt19937 rng{42};
	std::normal_distribution<float> nd{0.0f, 1.0f};
	Biquad bp;
	float gain;
	explicit RoadNoise(float g) : gain(g) {bp.setLowpass(FS, 60.0f, 0.7f);}
	float next() {return gain * bp.process(nd(rng));}
};

static float mean(const std::vector<IntervalResult>& v, float IntervalResult::*m, size_t skip = 1) {
	double s = 0;
	size_t n = 0;
	for (size_t i = skip; i < v.size(); i++) {s += v[i].*m; n++;}
	return n ? s / n : NAN;
}

static void testStartupNoFalseShock() {
	printf("startup: gravity step must not look like a shock\n");
	RoadQuality rq;
	rq.setSpeed(20, SpeedSource::WHEEL);
	Collected c;
	const float a[3] = {0.1f, -0.2f, 0.97f};
	for (int i = 0; i < 4 * FS; i++) feed(rq, a, c);
	CHECK(c.shocks.empty(), "%zu shocks at start-up", c.shocks.size());
	CHECK(c.intervals.size() == 2, "%zu intervals in 4 s", c.intervals.size());
	CHECK(c.intervals.back().rmsVertG < 0.001f, "rms %.4f on a constant input", c.intervals.back().rmsVertG);
}

static float roughRide(float noiseG, float kmh, uint8_t* cls) {
	RoadQuality rq;
	rq.setSpeed(kmh, SpeedSource::WHEEL);
	RoadNoise vn(noiseG), hn(noiseG * 0.5f);
	Collected c;
	for (int i = 0; i < 20 * FS; i++) {
		float b[3], a[3];
		bikeForce(0, 0, vn.next(), hn.next(), b);
		Mount(0).apply(b, a);
		feed(rq, a, c);
	}
	*cls = c.intervals.back().roadClass;
	return mean(c.intervals, &IntervalResult::rmsVertG);
}

static void testRoughness() {
	printf("roughness: classes must rise with the vibration level\n");
	uint8_t cSmooth, cMedium, cRough;
	float rSmooth = roughRide(0.15f, 20, &cSmooth);
	float rMedium = roughRide(0.45f, 20, &cMedium);
	float rRough = roughRide(1.5f, 20, &cRough);
	printf("  rms_v smooth %.3f g (class %u), medium %.3f g (class %u), rough %.3f g (class %u)\n",
	       rSmooth, cSmooth, rMedium, cMedium, rRough, cRough);
	CHECK(rSmooth < rMedium && rMedium < rRough, "rms not monotonic");
	CHECK(cSmooth < cMedium && cMedium < cRough, "classes not monotonic: %u %u %u", cSmooth, cMedium, cRough);

	RoadQuality rq;
	Collected c;
	RoadNoise vn(0.3f);
	for (int i = 0; i < 4 * FS; i++) {
		float a[3] = {0, 0, 1.0f + vn.next()};
		feed(rq, a, c);
	}
	CHECK(c.intervals.back().roadClass == 0 && (c.intervals.back().flags & IF_NO_SPEED), "no speed must not be rated");
	rq.setSpeed(3, SpeedSource::WHEEL);
	for (int i = 0; i < 2 * FS; i++) {
		float a[3] = {0, 0, 1.0f + vn.next()};
		feed(rq, a, c);
	}
	CHECK(c.intervals.back().roadClass == 0 && (c.intervals.back().flags & IF_TOO_SLOW), "3 km/h must not be rated");
}

// Half-sine bump of the given peak and duration, vertical, starting at sample s0
static float bump(int i, int s0, float peakG, float ms) {
	int len = (int)(ms * FS / 1000);
	if (i < s0 || i >= s0 + len) return 0;
	return peakG * sinf(3.14159265f * (i - s0) / len);
}

static void testShockPair() {
	printf("shocks: front + rear wheel over one pothole at 20 km/h\n");
	RoadQuality rq;
	const float kmh = 20;
	rq.setSpeed(kmh, SpeedSource::WHEEL);
	RoadNoise vn(0.1f);
	Collected c;
	const int front = 3 * FS;
	const int rear = front + (int)(1.05f / (kmh / 3.6f) * FS);	// wheelbase / v
	for (int i = 0; i < 6 * FS; i++) {
		float v = vn.next() + bump(i, front, 7.0f, 12) + bump(i, rear, 3.5f, 12);
		float a[3] = {0, 0, 1.0f + v};
		feed(rq, a, c);
	}
	CHECK(c.shocks.size() == 1, "%zu shocks, expected 1 (pair merged)", c.shocks.size());
	if (!c.shocks.empty()) {
		const ShockResult& s = c.shocks[0];
		printf("  peak %.2f g, second %.2f g after %.0f ms, duration %.1f ms, severity %u, flags 0x%02x, samplesAgo %u\n",
		       s.peakTotalG, s.secondPeakG, s.secondPeakDelayMs, s.durationMs, s.severity, s.flags, s.samplesAgo);
		CHECK(s.flags & SF_WHEELBASE_MATCH, "wheelbase match expected");
		CHECK(s.severity == 2, "severity %u, expected 2 (5..8 g)", s.severity);
		CHECK(s.peakTotalG > 5.0f, "peak %.2f too low (band-pass should keep most of a 12 ms bump)", s.peakTotalG);
		CHECK(s.samplesAgo > (uint32_t)(0.39f * FS) && s.samplesAgo < (uint32_t)(0.42f * FS), "samplesAgo %u", s.samplesAgo);
	}
	uint16_t logged = 0;
	for (auto& iv : c.intervals) logged += iv.eventsLogged;
	CHECK(logged == 1, "intervals report %u events", logged);
}

static void testRateLimit() {
	printf("shocks: rate limit (one hit per second for 40 s, 20/min allowed)\n");
	RoadQuality rq;
	rq.setSpeed(15, SpeedSource::WHEEL);
	Collected c;
	for (int i = 0; i < 41 * FS; i++) {
		float v = bump(i % (int)FS, (int)(0.5f * FS), 4.0f, 10);
		float a[3] = {0, 0, 1.0f + v};
		feed(rq, a, c);
	}
	uint32_t suppressed = rq.shocksSuppressed();
	printf("  logged %zu, suppressed %u\n", c.shocks.size(), suppressed);
	CHECK(c.shocks.size() == 20, "%zu logged, expected 20", c.shocks.size());
	CHECK(suppressed >= 19, "%u suppressed", suppressed);
}

static void testRelativeThreshold() {
	printf("shocks: on a trail the threshold rises with the background\n");
	RoadQuality rq;
	rq.setSpeed(15, SpeedSource::WHEEL);
	RoadNoise vn(0.8f);
	Collected c;
	for (int i = 0; i < 30 * FS; i++) {
		float a[3] = {0, 0, 1.0f + vn.next()};
		feed(rq, a, c);
	}
	printf("  %zu events on 0.8 g noise (rms_v %.2f g)\n", c.shocks.size(), mean(c.intervals, &IntervalResult::rmsVertG));
	CHECK(c.shocks.size() <= 3, "trail floods the log: %zu events", c.shocks.size());
}

static void testReferenceRide() {
	printf("reference ride: baseline from 60 s of smooth road\n");
	RoadQuality rq;
	rq.setSpeed(20, SpeedSource::WHEEL);
	RoadNoise vn(0.15f);
	Collected c;
	rq.startReference();
	for (int i = 0; i < 70 * FS && rq.refState() == RoadQuality::RefState::RUNNING; i++) {
		float a[3] = {0, 0, 1.0f + vn.next()};
		feed(rq, a, c);
	}
	float rms = mean(c.intervals, &IntervalResult::rmsVertG, 0);
	printf("  state %s after %u s, baseline %.4f g, measured rms %.4f g\n",
	       RoadQuality::refStateString(rq.refState()), rq.refElapsedS(), rq.config().baselineG, rms);
	CHECK(rq.refState() == RoadQuality::RefState::DONE, "not done");
	CHECK(fabsf(rq.config().baselineG - rms) < 0.1f * rms, "baseline off");
	CHECK(rq.config().baselineCalibrated, "not flagged calibrated");
	// Afterwards the same road is class 1
	for (int i = 0; i < 4 * FS; i++) {
		float a[3] = {0, 0, 1.0f + vn.next()};
		feed(rq, a, c);
	}
	CHECK(c.intervals.back().roadClass == 1, "class %u on the reference road", c.intervals.back().roadClass);
	CHECK(!(c.intervals.back().flags & IF_UNCALIBRATED), "still flagged uncalibrated");
}

// Ride with speed changes and a hill profile; sensor mounted rotated and pitched by 5 deg.
static void testPitch() {
	printf("pitch: forward axis and mounting offset learned, gradient tracked\n");
	Mount mount(5.0f);
	PitchEstimator pe;
	RoadQuality rq;
	RoadNoise vn(0.15f), hn(0.07f);
	std::mt19937 rng(7);
	std::normal_distribution<float> baroNoise(0.0f, 2.5f);

	const uint32_t blockMs = 50;
	const int perBlock = (int)(FS * blockMs / 1000);
	double v = 20 / 3.6, x = 0;
	double lastBaroX = 0, lastBaroH = 0, h = 0;
	float sum[3] = {0, 0, 0};
	uint32_t cnt = 0;
	double speedWinSum = 0;
	int speedWinN = 0;
	float maxErrSteady = 0;
	int steadyChecks = 0, validChecks = 0;

	auto gradAt = [](double xm) -> double {	// %: flat, 5 % climb, -3 % descent, flat, repeating every 3 km
		double p = fmod(xm, 3000.0);
		if (p < 600) return 0;
		if (p < 1500) return 5;
		if (p < 2400) return -3;
		return 0;
	};

	const int totalMs = 30 * 60 * 1000;			// 30 minutes
	for (int ms10 = 0; ms10 * 2.5 < totalMs; ms10++) {
		const double t = ms10 / FS;
		const uint32_t tMs = (uint32_t)(t * 1000);
		// speed: 20 km/h +/- 6 with a 25 s period -> +/-0.4 m/s^2
		const double vTarget = (20 + 6 * sin(2 * 3.14159265 * t / 25)) / 3.6;
		const double dvdt = (vTarget - v) * FS;
		v = vTarget;
		x += v / FS;
		const double grad = gradAt(x);
		const double theta = atan(grad / 100);
		h += v / FS * sin(theta);

		float b[3], a[3];
		bikeForce((float)(dvdt / G_MS2), (float)theta, vn.next(), hn.next(), b);
		mount.apply(b, a);
		rq.process(a);
		for (int i = 0; i < 3; i++) sum[i] += a[i];
		cnt++;
		speedWinSum += v;
		speedWinN++;

		if (cnt == (uint32_t)perBlock) {
			pe.addBlock(sum, cnt, tMs);
			sum[0] = sum[1] = sum[2] = 0;
			cnt = 0;
			rq.setLongitudinal(pe.forward(), pe.longitudinalAccelG(), pe.longitudinalValid(tMs));
		}
		// CSC: mean speed of the last second, arriving once per second
		if (tMs % 1000 == 0 && ms10 % 4 == 0 && speedWinN) {
			pe.onSpeed(tMs, (float)(speedWinSum / speedWinN * 3.6));
			speedWinSum = 0;
			speedWinN = 0;
		}
		// Barometer: every >= 8 m, height with noise
		if (x - lastBaroX >= 8.0) {
			double measured = (h - lastBaroH) / (x - lastBaroX) * 100 + baroNoise(rng);
			pe.onBaroGradient((float)measured, (float)(x - lastBaroX));
			lastBaroX = x;
			lastBaroH = h;
		}
		// Check once per second after 15 minutes, away from gradient changes
		if (t > 900 && tMs % 1000 == 0 && ms10 % 4 == 0) {
			float est;
			double p = fmod(x, 3000.0);
			bool steady = (p > 700 && p < 1400) || (p > 1600 && p < 2300) || (p > 100 && p < 500);
			if (pe.gradient(est, tMs)) {
				validChecks++;
				if (steady) {
					maxErrSteady = std::max(maxErrSteady, (float)fabs(est - grad));
					steadyChecks++;
				}
			}
		}
	}
	printf("  forward valid %d (slope %.2f), bias %.2f deg over %.0f m, %u updates, max error on steady sections %.2f %% (%d checks, %d valid)\n",
	       pe.forwardValid(), pe.regressionSlope(), pe.biasDeg(), pe.biasDistance(), pe.updates(), maxErrSteady, steadyChecks, validChecks);
	// The mounting pitch ends up in e_f itself (it is learned perpendicular to the average
	// gravity, i.e. level); the bias only absorbs what is left, e.g. from learning on slopes.
	float bikeFwd[3] = {1, 0, 0}, sensorFwd[3];
	mount.apply(bikeFwd, sensorFwd);
	float d = pe.forward()[0] * sensorFwd[0] + pe.forward()[1] * sensorFwd[1] + pe.forward()[2] * sensorFwd[2];
	printf("  learned forward axis (%.3f, %.3f, %.3f), cos to true %.4f\n", pe.forward()[0], pe.forward()[1], pe.forward()[2], d);
	CHECK(pe.forwardValid(), "forward axis not learned");
	CHECK(d > 0.99f, "forward axis off: cos %.4f", d);
	CHECK(fabsf(pe.biasDeg()) < 2.0f, "bias %.2f deg", pe.biasDeg());
	CHECK(maxErrSteady < 1.5f, "gradient error %.2f %% on steady sections", maxErrSteady);
	CHECK(steadyChecks > 300, "too few valid estimates: %d", steadyChecks);
}

int main() {
	testStartupNoFalseShock();
	testRoughness();
	testShockPair();
	testRateLimit();
	testRelativeThreshold();
	testReferenceRide();
	testPitch();
	printf(failures ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", failures);
	return failures ? 1 : 0;
}
