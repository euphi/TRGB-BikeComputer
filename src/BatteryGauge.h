/*
 * BatteryGauge.h -- state of charge as the share of runtime that is left.
 *
 * The board measures only the battery voltage, and voltage is a poor clock in the middle of a
 * Li-Ion discharge: two full runs to switch-off (2026-10-13 and 2026-10-16, both 8 h 19 min to
 * the minute) differ by up to 50 mV around 3.5 V, where the curve is so flat that 50 mV is
 * worth 100 minutes of runtime. Measuring the voltage better would not help -- the battery
 * itself does not put the information there.
 *
 * So this gauge runs a clock and lets the voltage correct it, weighted by how much the voltage
 * can say at that point of the curve:
 *
 *   percent -= elapsed / FULL_RUNTIME_MIN       the clock: percent is defined as remaining
 *                                               runtime, so it drains linearly by definition
 *   percent += (curve(mv) - percent) * dt/tau   the correction, with tau from the curve slope
 *
 * tau follows from the measurement error the 50 mV spread causes in percentage points,
 * sd = |dpercent/dmv| * 50 mV: a few points at the steep ends (tau minutes), 14 points in the
 * plateau (tau hours). The clock therefore carries the plateau and the voltage re-anchors the
 * estimate at both ends, where it is worth something.
 *
 * What that buys, measured against the two runs (test/native_batterygauge/):
 *
 *   worst error over a full run   bare curve   this gauge
 *   both reference runs             12.5 pp       6.9 pp
 *   runtime 50% off the lab value   12.5 pp      17.0 pp
 *
 * The second row is the price: Config::fullRuntimeMin is the lab runtime (display on, no
 * sensors, room temperature), and leaning on the clock means leaning on that number. Both
 * reference runs had WiFi connected for part or all of the time without a measurable
 * difference, so the display dominates the draw and BLE is expected to be noise as well -- but
 * a ride draws more, and the cold is the one that will really hurt, because it shortens the
 * runtime and depresses the voltage at the same time. Nothing here compensates for temperature
 * yet; the board has a BME280, so it could. Tuning the runtime from real rides is what the
 * remaining-minutes field in the minute log line is for.
 *
 * Pure: no Arduino, no hardware, no logging. Host test: test/native_batterygauge/.
 */

#pragma once

#include <stdint.h>

namespace BatGauge {

// Measured discharge curve, fitted over both reference runs (data in Tools/TestData/
// battery-discharge-2026101{3,6}.csv). Percent is the share of the runtime that is left at that
// voltage, averaged over the two runs.
// Below a LiPo's 4.2 V / 3.0 V because the pin does not see the cell but the system rail behind
// the P-FET that connects it (see update()), so the FET's drop under load is in every reading --
// switching WiFi off lifted it by 25 mV. The curve was measured at that same point, so it holds.
constexpr uint16_t EMPTY_MV = 2620;		// the board browned out at 2623 mV
constexpr uint16_t FULL_MV  = 3910;		// highest reading seen after unplugging the charger

uint8_t curvePercent(uint16_t mv);		// clamped to 0..100 outside [EMPTY_MV, FULL_MV]
float   curveSlopePpPerMv(uint16_t mv);	// |dpercent/dmv| of the segment mv falls into

// Tuning. Collected here because every one of them is a measurement that real rides will
// improve on -- none of them is a fact about the hardware.
struct Config {
	uint16_t fullRuntimeMin = 499;	// lab runtime of a full battery, display on, no sensors
	uint16_t vSpreadMv      = 50;	// run-to-run voltage spread, the measurement error
	float    tauMinMinutes  = 6.0f;	// time constant where the voltage is at its best
	float    sdMinPp        = 2.0f;	// ... which is this measurement error, in percentage points
	float    tauMaxMinutes  = 600.0f;	// cap, so the plateau never ignores the voltage completely
	uint16_t chargerMv      = 4100;	// above this the charger holds the pin, not the battery
	float    settleMinutes  = 1.5f;	// after unplugging: until the caller's 60 s average is clean
};

// Survives a restart in RTC memory, so a brownout reboot or a restart from the settings screen
// does not re-anchor the estimate to the plateau voltage. POD on purpose.
struct State {
	float   percent;		// the filter's estimate
	float   shown;			// what percent() returns -- never rises while discharging
	uint8_t valid;			// 0 = nothing known yet
	uint8_t onCharger;		// on the charger, or just off it and waiting for a clean reading
	float   settleMin;		// minutes since the charger went away
};

class Gauge {
public:
	explicit Gauge(const Config& cfg = Config()) : _cfg(cfg) {}

	// No history: take the curve at face value. In the plateau that is worth +-12 points, which
	// is the best anyone can do from a single voltage reading.
	void anchor(uint16_t mv);

	// dtMs since the last call. `charging` is the caller's charger detection. On the charger
	// nothing about the battery can be measured: the pin sits on the system rail behind the
	// power path (LilyGO schematic: divider R2/R3 on net BAT, fed from USB through a diode or
	// from the cell through a P-FET), so it reads the USB supply, and the charge current shows
	// up only as the voltage drop in cable and plug. The same charge read 4355 mV on one cable
	// and 4757 mV on another (2026-10-17), so that reading says nothing portable. The estimate
	// is therefore held while charging and re-anchored to the voltage once the cable is off
	// and the reading is clean -- how much went in is not knowable here. A charge in deep sleep,
	// the normal case, ends the same way: the caller anchors after the wake-up.
	void update(uint32_t dtMs, uint16_t mv, bool charging);
	bool onCharger() const { return _st.onCharger != 0; }

	uint8_t percent() const;			// 0..100, monotone while discharging
	uint16_t remainingMinutes() const;	// percent() of the lab runtime
	bool known() const { return _st.valid != 0; }

	const State& state() const { return _st; }
	void restore(const State& s);		// ignores a state that is not valid

	const Config& config() const { return _cfg; }

private:
	Config _cfg;
	State  _st = {};
};

}
