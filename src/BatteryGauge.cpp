#include "BatteryGauge.h"

#include <math.h>
#include <stddef.h>

namespace BatGauge {

namespace {

// Mean of the two reference runs: percent of the runtime left at that voltage. Seven points
// keep the fit within 1.4 points of the measured mean.
struct Point { uint16_t mv; uint8_t pct; };
constexpr Point CURVE[] = {
	{EMPTY_MV,   0},
	{3174,      11},
	{3397,      20},
	{3475,      29},
	{3584,      59},	// the plateau: 109 mV cover 30 points, hence tau in hours here
	{3707,      79},
	{FULL_MV,  100},
};
constexpr size_t CURVE_N = sizeof(CURVE) / sizeof(CURVE[0]);

// Index of the segment mv falls into, i.e. the i with CURVE[i-1].mv <= mv < CURVE[i].mv.
size_t segment(uint16_t mv) {
	for (size_t i = 1; i < CURVE_N - 1; i++)
		if (mv < CURVE[i].mv) return i;
	return CURVE_N - 1;
}

}

uint8_t curvePercent(uint16_t mv) {
	if (mv <= EMPTY_MV) return 0;
	if (mv >= FULL_MV) return 100;
	const size_t i = segment(mv);
	const uint16_t span = CURVE[i].mv - CURVE[i - 1].mv;
	return uint8_t(CURVE[i - 1].pct + (CURVE[i].pct - CURVE[i - 1].pct) * (mv - CURVE[i - 1].mv) / span);
}

float curveSlopePpPerMv(uint16_t mv) {
	const size_t i = segment(mv < EMPTY_MV ? EMPTY_MV : (mv > FULL_MV ? FULL_MV : mv));
	return float(CURVE[i].pct - CURVE[i - 1].pct) / float(CURVE[i].mv - CURVE[i - 1].mv);
}

void Gauge::anchor(uint16_t mv) {
	_st.percent = float(curvePercent(mv));
	_st.shown   = _st.percent;
	_st.valid   = 1;
}

void Gauge::restore(const State& s) {
	if (!s.valid || !(s.percent >= 0.0f) || s.percent > 100.0f) return;	// !(>=0) also catches NaN
	_st = s;
}

void Gauge::update(uint32_t dtMs, uint16_t mv, bool charging) {
	// The charger holds the pin well above any battery voltage, and the caller's 60 s average
	// stays up there for a few minutes after unplugging. Nothing can be measured until it drops.
	if (charging || mv >= _cfg.chargerMv) {
		_st.percent = 100.0f;
		_st.shown   = 100.0f;
		_st.valid   = 1;
		return;
	}
	if (!_st.valid) { anchor(mv); return; }

	const float dtMin = float(dtMs) / 60000.0f;

	// The clock. percent is defined as remaining runtime, so it drains linearly.
	_st.percent -= 100.0f * dtMin / float(_cfg.fullRuntimeMin);

	// The correction, weighted by what the voltage can say here: sd is the error the run-to-run
	// voltage spread causes in percentage points, tau grows with its square (the steady-state
	// weight of a measurement with that error).
	const float sd  = fmaxf(curveSlopePpPerMv(mv) * float(_cfg.vSpreadMv), _cfg.sdMinPp);
	const float rel = sd / _cfg.sdMinPp;
	const float tau = fminf(_cfg.tauMinMinutes * rel * rel, _cfg.tauMaxMinutes);
	const float w   = dtMin / tau >= 1.0f ? 1.0f : dtMin / tau;		// a long gap must not overshoot
	_st.percent += (float(curvePercent(mv)) - _st.percent) * w;

	if (_st.percent < 0.0f) _st.percent = 0.0f;
	if (_st.percent > 100.0f) _st.percent = 100.0f;

	// What is shown never rises while discharging: an upward correction is absorbed by the
	// clock over the next minutes instead of letting the icon fill up again.
	if (_st.percent < _st.shown) _st.shown = _st.percent;
	if (mv <= EMPTY_MV) _st.shown = 0.0f;		// the board is about to give up, say so
}

// Truncated, not rounded: a battery indicator should never claim more than it has, and it lets
// the last percent drain away instead of showing 1% as the board gives up.
uint8_t Gauge::percent() const {
	if (!_st.valid) return 0;
	return uint8_t(_st.shown);
}

uint16_t Gauge::remainingMinutes() const {
	return uint16_t(_st.shown * float(_cfg.fullRuntimeMin) / 100.0f);
}

}
