/*
 * RoadQuality.cpp
 *
 * See RoadQuality.h. No Arduino/FreeRTOS includes on purpose -- this file is also compiled
 * on the host by test/native_roadquality/.
 */

#include "RoadQuality.h"

#include <algorithm>

namespace RQ {

static constexpr float PI_F = 3.14159265f;

// ******************** Biquad ********************

void Biquad::setLowpass(float fs, float fc, float q) {
	float w0 = 2.0f * PI_F * fc / fs;
	float cw = cosf(w0), alpha = sinf(w0) / (2.0f * q);
	float a0 = 1.0f + alpha;
	b0 = (1.0f - cw) / 2.0f / a0;
	b1 = (1.0f - cw) / a0;
	b2 = b0;
	a1 = -2.0f * cw / a0;
	a2 = (1.0f - alpha) / a0;
}

void Biquad::setHighpass(float fs, float fc, float q) {
	float w0 = 2.0f * PI_F * fc / fs;
	float cw = cosf(w0), alpha = sinf(w0) / (2.0f * q);
	float a0 = 1.0f + alpha;
	b0 = (1.0f + cw) / 2.0f / a0;
	b1 = -(1.0f + cw) / a0;
	b2 = b0;
	a1 = -2.0f * cw / a0;
	a2 = (1.0f - alpha) / a0;
}

void Biquad::primeDC(float x) {
	float y = x * (b0 + b1 + b2) / (1.0f + a1 + a2);
	z2 = b2 * x - a2 * y;
	z1 = b1 * x - a1 * y + z2;
}

// ******************** RoadQuality ********************

const char* RoadQuality::refStateString(RefState s) {
	switch (s) {
	case RefState::IDLE:	return "idle";
	case RefState::RUNNING:	return "running";
	case RefState::DONE:	return "done";
	case RefState::FAILED:	return "failed (timeout)";
	}
	return "?";
}

void RoadQuality::configure(const Config& c) {
	bool rateChanged = c.maxEventsPerMin != cfg.maxEventsPerMin;
	cfg = c;
	if (cfg.intervalS < 1) cfg.intervalS = 1;
	if (cfg.intervalS > 10) cfg.intervalS = 10;
	if (cfg.maxEventsPerMin > MAX_RATE_SLOTS) cfg.maxEventsPerMin = MAX_RATE_SLOTS;
	if (rateChanged) rateHead = rateCount = 0;

	const float fs = cfg.odrHz;
	for (uint8_t i = 0; i < 3; i++) {
		hp[i].setHighpass(fs, cfg.hpHz);
		lp[i].setLowpass(fs, cfg.lpHz);
		if (primed) {
			// New coefficients with old state would ring -- restart from the current gravity.
			hp[i].primeDC(gLp[i]);
			lp[i].primeDC(0);
		}
	}
	alphaG = 1.0f / (cfg.gravityTauS * fs);
	alphaGStill = 1.0f / (cfg.gravityTauStillS * fs);
	alpha1s = 1.0f / fs;
	alphaStill = 1.0f / (0.5f * fs);
	intervalSamples = (uint32_t)(cfg.intervalS * fs + 0.5f);
}

void RoadQuality::setSpeed(float kmh, SpeedSource src) {
	speedSrc = (src == SpeedSource::NONE || std::isnan(kmh)) ? SpeedSource::NONE : src;
	speedKmh = speedSrc == SpeedSource::NONE ? NAN : kmh;
}

void RoadQuality::setLongitudinal(const float f[3], float accelG, bool valid) {
	longValid = valid;
	if (!valid) return;
	for (uint8_t i = 0; i < 3; i++) fwd[i] = f[i];
	longAccelG = accelG;
}

uint8_t RoadQuality::process(const float a[3]) {
	uint8_t ready = READY_NONE;
	const float fs = cfg.odrHz;

	if (!primed) {
		for (uint8_t i = 0; i < 3; i++) {
			gLp[i] = a[i];
			hp[i].primeDC(a[i]);
			lp[i].primeDC(0);
		}
		primed = true;
	}
	idx++;

	// --- gravity direction and band-passed dynamic acceleration ---
	const float alpha = still ? alphaGStill : alphaG;
	float f[3], t2 = 0;
	bool clip = false;
	for (uint8_t i = 0; i < 3; i++) {
		gLp[i] += alpha * (a[i] - gLp[i]);
		f[i] = lp[i].process(hp[i].process(a[i]));
		t2 += f[i] * f[i];
		if (fabsf(a[i]) >= cfg.clipG) clip = true;
	}
	float up[3], un = 0;
	for (uint8_t i = 0; i < 3; i++) {
		// The low-passed vector also contains the longitudinal acceleration (braking tilts
		// it forward); remove that once the forward axis is known.
		up[i] = gLp[i] - (longValid ? longAccelG * fwd[i] : 0.0f);
		un += up[i] * up[i];
	}
	un = sqrtf(un);
	if (un < 0.3f) {				// nonsense (free fall, not primed): fall back to raw gravity
		un = sqrtf(gLp[0] * gLp[0] + gLp[1] * gLp[1] + gLp[2] * gLp[2]);
		for (uint8_t i = 0; i < 3; i++) up[i] = gLp[i];
	}
	if (un < 1e-3f) {up[0] = 0; up[1] = 0; up[2] = 1; un = 1;}
	float v = 0;
	for (uint8_t i = 0; i < 3; i++) v += f[i] * up[i] / un;
	const float h = sqrtf(std::max(0.0f, t2 - v * v));
	const float t = sqrtf(t2);
	const float vms = v * G_MS2;
	const float v4 = vms * vms * vms * vms;

	// --- standstill: faster gravity tracking ---
	emaDyn2 += alphaStill * (t2 - emaDyn2);
	const bool speedValid = speedSrc != SpeedSource::NONE;
	slowSamples = (speedValid && speedKmh < 1.0f) ? slowSamples + 1 : 0;
	const float stillLimit = 3.0f * cfg.noiseG;
	still = slowSamples >= (uint32_t)(5 * fs) && emaDyn2 < stillLimit * stillLimit;

	// --- interval accumulators ---
	if (n == 0) {
		pVMax = pVMin = v;
		pT = t;
	} else {
		if (v > pVMax) pVMax = v;
		if (v < pVMin) pVMin = v;
		if (t > pT) pT = t;
	}
	sumV2 += (double)v * v;
	sumH2 += (double)h * h;
	sumV4 += v4;
	if (arm1 && t > cfg.t1G) {cnt1++; arm1 = false;} else if (t < 0.5f * cfg.t1G) arm1 = true;
	if (arm2 && t > cfg.t2G) {cnt2++; arm2 = false;} else if (t < 0.5f * cfg.t2G) arm2 = true;
	if (clip) curFlags |= IF_CLIPPED;
	if (speedValid) {
		sumSpeed += speedKmh;
		nSpeed++;
		dist += speedKmh / 3.6f / fs;
		if (speedSrc == SpeedSource::GPS) curFlags |= IF_SPEED_FROM_GPS;
	}
	n++;

	// --- shock detection (uses the 1 s RMS from before this sample) ---
	switch (sState) {
	case ShockState::IDLE: {
		const float preRms = sqrtf(ms1);
		const float thrOn = std::max(cfg.shockAbsG, cfg.shockRelFactor * preRms);
		if (t > thrOn) {
			ev = {};
			ev.startIdx = ev.peakIdx = ev.lastAboveIdx = idx;
			ev.thrOn = thrOn;
			ev.thrOff = thrOn * cfg.shockOffFactor;
			ev.preRms = preRms;
			ev.peakT = t;
			ev.vMax = ev.vMin = v;
			ev.hMax = h;
			ev.sumV4 = v4;
			ev.over = 1;
			ev.clipped = clip;
			sState = ShockState::FIRST;
		}
		break;
	}
	case ShockState::FIRST:
		if (t > ev.peakT) {ev.peakT = t; ev.peakIdx = idx;}
		if (v > ev.vMax) ev.vMax = v;
		if (v < ev.vMin) ev.vMin = v;
		if (h > ev.hMax) ev.hMax = h;
		ev.sumV4 += v4;
		if (t > ev.thrOn) ev.over++;
		if (clip) ev.clipped = true;
		if (t > ev.thrOff) ev.lastAboveIdx = idx;
		if (idx - ev.lastAboveIdx >= msToSamples(cfg.shockEndMs) || idx - ev.startIdx >= msToSamples(cfg.shockMaxMs)) {
			ev.endIdx = ev.lastAboveIdx;
			ev.secondG = 0;
			ev.secondIdx = 0;
			sState = ShockState::WAIT_SECOND;
		}
		break;
	case ShockState::WAIT_SECOND: {
		const uint32_t since = idx - ev.peakIdx;
		if (since >= msToSamples(cfg.secondPeakMinMs) && t > ev.thrOff && t > ev.secondG) {
			ev.secondG = t;
			ev.secondIdx = idx;
			if (clip) ev.clipped = true;
		}
		if (since >= msToSamples(cfg.secondPeakMaxMs)) ready |= finishShock();
		break;
	}
	}

	ms1 += alpha1s * (v * v - ms1);

	if (n >= intervalSamples) {
		finishInterval();
		ready |= READY_INTERVAL;
	}
	return ready;
}

uint8_t RoadQuality::finishShock() {
	sState = ShockState::IDLE;
	const float fs = cfg.odrHz;
	const bool speedValid = speedSrc != SpeedSource::NONE;

	ShockResult r;
	r.samplesAgo = idx - ev.peakIdx;
	r.durationMs = (ev.endIdx - ev.startIdx + 1) * 1000.0f / fs;
	r.peakTotalG = ev.peakT;
	r.peakVertMaxG = ev.vMax;
	r.peakVertMinG = ev.vMin;
	r.peakHorizG = ev.hMax;
	r.preRmsG = ev.preRms;
	r.thresholdG = ev.thrOn;
	r.samplesOverThr = ev.over;
	r.vdv = powf((float)(ev.sumV4 / fs), 0.25f);
	r.speedKmh = speedValid ? speedKmh : NAN;
	if (ev.secondIdx) {
		r.secondPeakG = ev.secondG;
		r.secondPeakDelayMs = (ev.secondIdx - ev.peakIdx) * 1000.0f / fs;
	}
	r.severity = (ev.clipped || ev.peakT >= cfg.sev3G) ? 3 : (ev.peakT >= cfg.sev2G ? 2 : 1);
	if (ev.clipped) r.flags |= SF_CLIPPED;
	if (!speedValid) {
		r.flags |= SF_NO_SPEED;
	} else {
		if (speedKmh < 2.0f) r.flags |= SF_STANDSTILL;
		if (ev.secondIdx && speedKmh >= 3.0f) {
			float d = r.secondPeakDelayMs / 1000.0f * speedKmh / 3.6f;
			if (fabsf(d - cfg.wheelbaseM) <= cfg.wheelbaseTolM) r.flags |= SF_WHEELBASE_MATCH;
		}
	}

	// Rate limit: at most maxEventsPerMin logged events in any 60 s
	const uint8_t cap = cfg.maxEventsPerMin;
	if (cap) {
		if (rateCount == cap && (idx - rateRing[rateHead]) < (uint32_t)(60 * fs)) {
			evSuppressed++;
			suppressedTotal++;
			suppressedSinceLast = true;
			return READY_NONE;
		}
		rateRing[rateHead] = idx;
		rateHead = (rateHead + 1) % cap;
		if (rateCount < cap) rateCount++;
	}
	if (suppressedSinceLast) r.flags |= SF_AFTER_SUPPRESSED;
	suppressedSinceLast = false;
	r.seq = ++seq;
	evLogged++;
	lastShock = r;
	return READY_SHOCK;
}

uint8_t RoadQuality::classify(float r) const {
	for (uint8_t c = 0; c < 4; c++) {
		if (r < cfg.classThr[c]) return c + 1;
	}
	return 5;
}

void RoadQuality::finishInterval() {
	const float fs = cfg.odrHz;
	IntervalResult r;
	r.sampleCount = n;
	r.rmsVertG = sqrtf((float)(sumV2 / n));
	r.rmsHorizG = sqrtf((float)(sumH2 / n));
	r.peakVertMaxG = pVMax;
	r.peakVertMinG = pVMin;
	r.peakTotalG = pT;
	r.vdvVert = powf((float)(sumV4 / fs), 0.25f);
	r.distanceM = (float)dist;
	r.countOverT1 = cnt1;
	r.countOverT2 = cnt2;
	r.eventsLogged = evLogged;
	r.eventsSuppressed = evSuppressed;

	uint8_t flags = curFlags;
	if (!cfg.baselineCalibrated) flags |= IF_UNCALIBRATED;
	if (ref.state == RefState::RUNNING) flags |= IF_REF_RIDE;
	const float coverage = (float)nSpeed / n;
	bool speedOk = false;
	if (nSpeed == 0 || coverage < 0.8f) {
		flags |= IF_NO_SPEED;
		if (nSpeed) r.speedKmh = (float)(sumSpeed / nSpeed);
	} else {
		r.speedKmh = (float)(sumSpeed / nSpeed);
		speedOk = true;
		if (r.speedKmh < cfg.vMinKmh) flags |= IF_TOO_SLOW;
	}
	float norm = NAN;			// RMS_v scaled to v_ref
	if (speedOk && r.speedKmh > 0.5f) norm = r.rmsVertG / powf(r.speedKmh / cfg.vRefKmh, cfg.speedExp);
	if (speedOk && r.speedKmh >= cfg.vMinKmh && cfg.baselineG > 0) {
		r.roughness = norm / cfg.baselineG;
		r.roadClass = classify(r.roughness);
	}
	r.flags = flags;

	if (ref.state == RefState::RUNNING) {
		ref.total++;
		if (speedOk && r.speedKmh >= cfg.refMinKmh && !(curFlags & (IF_DATA_GAP | IF_CLIPPED)) && ref.count < MAX_REF_VALUES) {
			ref.values[ref.count++] = norm;
		}
		if (ref.count * cfg.intervalS >= cfg.refSeconds || ref.count >= MAX_REF_VALUES) {
			float sorted[MAX_REF_VALUES];
			std::copy(ref.values, ref.values + ref.count, sorted);
			std::sort(sorted, sorted + ref.count);
			float median = (ref.count % 2) ? sorted[ref.count / 2]
			                               : 0.5f * (sorted[ref.count / 2 - 1] + sorted[ref.count / 2]);
			cfg.baselineG = median;
			cfg.baselineCalibrated = true;
			ref.state = RefState::DONE;
		} else if (ref.total * cfg.intervalS >= cfg.refTimeoutS) {
			ref.state = RefState::FAILED;
		}
	}
	lastInterval = r;

	n = 0;
	sumV2 = sumH2 = sumV4 = 0;
	sumSpeed = 0;
	nSpeed = 0;
	dist = 0;
	cnt1 = cnt2 = 0;
	curFlags = 0;
	evLogged = evSuppressed = 0;
}

void RoadQuality::startReference() {
	ref.state = RefState::RUNNING;
	ref.count = 0;
	ref.total = 0;
}

void RoadQuality::cancelReference() {
	if (ref.state == RefState::RUNNING) ref.state = RefState::IDLE;
}

// ******************** PitchEstimator ********************

static float dot3(const float a[3], const float b[3]) {return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];}
static float norm3(const float a[3]) {return sqrtf(dot3(a, a));}

void PitchEstimator::setState(const State& s) {
	const float n = norm3(s.ef);
	if (n > 0.5f && s.efWeight > 0) {
		for (uint8_t i = 0; i < 3; i++) M[i] = s.ef[i] / n * s.efWeight;
		efWeight = s.efWeight;
		updateForward();
	}
	biasRad = s.biasRad;
	biasDistM = s.biasDistM;
}

PitchEstimator::State PitchEstimator::state() const {
	State s;
	for (uint8_t i = 0; i < 3; i++) s.ef[i] = efValid ? ef[i] : 0.0f;
	s.efWeight = efValid ? efWeight : 0.0f;
	s.biasRad = biasRad;
	s.biasDistM = biasDistM;
	return s;
}

void PitchEstimator::resetLearning() {
	for (uint8_t i = 0; i < 3; i++) {M[i] = 0; ef[i] = 0;}
	efWeight = efMagnitude = 0;
	efValid = false;
	biasRad = biasDistM = 0;
	sumThetaDs = sumDs = 0;
	thetaValid = false;
	gradRawPct = NAN;
}

void PitchEstimator::setGravityHint(const float g[3]) {
	const float n = norm3(g);
	if (n < 0.5f) return;
	for (uint8_t i = 0; i < 3; i++) gSlow[i] = g[i] / n;
	gSlowValid = true;
}

void PitchEstimator::addBlock(const float sum[3], uint32_t count, uint32_t endMs) {
	if (count == 0) return;
	Block& b = ring[head];
	b.endMs = endMs;
	b.n = count;
	for (uint8_t i = 0; i < 3; i++) b.sum[i] = sum[i];
	head = (head + 1) % RING;
	if (filled < RING) filled++;

	float mean[3];
	for (uint8_t i = 0; i < 3; i++) mean[i] = sum[i] / count;
	if (!gSlowValid) {
		for (uint8_t i = 0; i < 3; i++) gSlow[i] = mean[i];
		gSlowValid = true;
	} else {
		float dt = (endMs - lastBlockMs) / 1000.0f;
		float alpha = std::min(1.0f, dt / cfg.gravityTauS);
		for (uint8_t i = 0; i < 3; i++) gSlow[i] += alpha * (mean[i] - gSlow[i]);
	}
	lastBlockMs = endMs;
}

bool PitchEstimator::windowMean(uint32_t fromMs, uint32_t toMs, float out[3]) const {
	float s[3] = {0, 0, 0};
	uint32_t cnt = 0;
	for (uint8_t k = 0; k < filled; k++) {
		const Block& b = ring[k];
		// signed differences: correct across the millis() wrap
		if ((int32_t)(b.endMs - fromMs) > 0 && (int32_t)(b.endMs - toMs) <= 0) {
			for (uint8_t i = 0; i < 3; i++) s[i] += b.sum[i];
			cnt += b.n;
		}
	}
	if (cnt == 0) return false;
	for (uint8_t i = 0; i < 3; i++) out[i] = s[i] / cnt;
	return true;
}

void PitchEstimator::updateForward() {
	efMagnitude = norm3(M);
	// Consistent data gives |M| ~ efWeight (F_h.e_f = dv/dt); far below means the
	// direction is noise, not signal.
	if (efWeight >= cfg.efMinWeight && efMagnitude > 0.3f * efWeight) {
		for (uint8_t i = 0; i < 3; i++) ef[i] = M[i] / efMagnitude;
		efValid = true;
	}
}

void PitchEstimator::onSpeed(uint32_t tMs, float kmh) {
	if (std::isnan(kmh)) {
		hasSpeed = false;
		return;
	}
	const float vms = kmh / 3.6f;
	if (!hasSpeed) {
		hasSpeed = true;
		lastSpeedMs = tMs;
		lastSpeedMs2 = vms;
		return;
	}
	const uint32_t dtMs = tMs - lastSpeedMs;
	const float vPrev = lastSpeedMs2;
	lastSpeedMs = tMs;
	lastSpeedMs2 = vms;
	if (dtMs < 200 || dtMs > 3000) {
		isFrozen = true;
		return;
	}
	const float dt = dtMs / 1000.0f;
	const float dvdt = (vms - vPrev) / dt;
	dvdtG = dvdt / G_MS2;
	const float vMeanKmh = (vms + vPrev) / 2.0f * 3.6f;

	const uint32_t lat = (uint32_t)cfg.speedLatencyMs;
	float F[3];
	if (!gSlowValid || !windowMean(tMs - dtMs - lat, tMs - lat, F)) {
		isFrozen = true;
		return;
	}

	// Horizontal part of the mean specific force, relative to the long-term gravity
	float u[3], Fh[3];
	const float gn = norm3(gSlow);
	for (uint8_t i = 0; i < 3; i++) u[i] = gSlow[i] / gn;
	const float Fu = dot3(F, u);
	for (uint8_t i = 0; i < 3; i++) Fh[i] = F[i] - Fu * u[i];

	// Learn the forward axis: F_h ~ dv/dt * e_f while accelerating or braking
	const float adv = fabsf(dvdt);
	if (vMeanKmh >= cfg.learnMinKmh && adv >= cfg.learnMinAccelMs2 && adv <= cfg.maxAccelMs2) {
		for (uint8_t i = 0; i < 3; i++) M[i] = M[i] * cfg.efForget + dvdtG * Fh[i];
		efWeight = efWeight * cfg.efForget + dvdtG * dvdtG;
		updateForward();
	}

	if (!efValid || vMeanKmh < cfg.minKmh) {
		isFrozen = true;
		return;
	}
	const float Ff = dot3(Fh, ef);
	float lat3[3];
	for (uint8_t i = 0; i < 3; i++) lat3[i] = Fh[i] - Ff * ef[i];
	if (adv > cfg.maxAccelMs2 || norm3(lat3) > cfg.maxLateralG) {
		isFrozen = true;
		return;
	}

	// f.e_f = dv/dt + sin(theta)   (all in g)
	float s = dot3(F, ef) - dvdtG;
	s = std::max(-0.6f, std::min(0.6f, s));
	const float theta = asinf(s);
	thetaSmooth = thetaValid ? thetaSmooth + cfg.smoothing * (theta - thetaSmooth) : theta;
	thetaValid = true;
	thetaMs = tMs;
	isFrozen = false;
	nUpdates++;
	gradRawPct = 100.0f * tanf(thetaSmooth);

	if (vMeanKmh >= cfg.learnMinKmh) {
		const double ds = (vms + vPrev) / 2.0f * dt;
		sumThetaDs += theta * ds;
		sumDs += ds;
	}
}

void PitchEstimator::onBaroGradient(float gradPct, float deltaDistM) {
	// The barometer's gradient covers the stretch since its previous calculation; so do
	// the sums, which are therefore reset every time, used or not.
	const double ds = sumDs, sumT = sumThetaDs;
	sumDs = sumThetaDs = 0;
	if (!efValid || std::isnan(gradPct) || deltaDistM < 5.0f || fabsf(gradPct) > 25.0f) return;
	if (ds < 0.7 * deltaDistM) return;			// IMU estimate held for too much of the stretch
	const float thetaImu = (float)(sumT / ds);
	const float thetaBaro = atanf(gradPct / 100.0f);
	const float w = std::min(1.0f, deltaDistM / cfg.biasDistanceM);
	biasRad += w * ((thetaImu - thetaBaro) - biasRad);
	biasDistM += deltaDistM;
}

bool PitchEstimator::gradient(float& pct, uint32_t nowMs, uint32_t maxAgeMs) const {
	if (!thetaValid || (nowMs - thetaMs) > maxAgeMs) return false;
	pct = 100.0f * tanf(thetaSmooth - biasRad);
	return true;
}

}	// namespace RQ
