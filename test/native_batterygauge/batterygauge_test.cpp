/*
 * Host test for src/BatteryGauge.{h,cpp} -- no hardware, no PlatformIO. It replays the two
 * measured discharge runs minute by minute and compares the gauge against the truth those runs
 * carry: the share of the total runtime that was still left at that minute. Build and run from
 * the repository root (it reads the CSVs from Tools/TestData/):
 *
 *   g++ -std=c++17 -O2 -Wall -Isrc src/BatteryGauge.cpp test/native_batterygauge/batterygauge_test.cpp -o /tmp/bg_test && /tmp/bg_test
 *
 * With -v it prints the error profile of every run instead of only the summary.
 * Exit code 0 = all checks passed.
 */

#include "BatteryGauge.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace BatGauge;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct Sample { int minute; uint16_t mv; };

static std::vector<Sample> load(const char* path) {
	std::vector<Sample> out;
	FILE* f = fopen(path, "r");
	if (!f) { printf("  FAIL cannot open %s (run from the repository root)\n", path); failures++; return out; }
	char line[256];
	bool header = true;
	while (fgets(line, sizeof(line), f)) {
		if (line[0] == '#') continue;
		if (header) { header = false; continue; }		// time,minutes,mv,pct_*,wifi,ble
		int minute = 0, mv = 0;
		if (sscanf(strchr(line, ',') + 1, "%d,%d", &minute, &mv) == 2) out.push_back({minute, uint16_t(mv)});
	}
	fclose(f);
	return out;
}

struct Result { float maxErr, medianErr, bias; int lastShown; bool monotone; };

// Replays a run. `startAt` skips the first minutes, so the gauge has to come up cold in the
// middle of a discharge (what happens when the computer is switched on during a ride).
// `speed` stretches the clock against the voltage: 1.5 means the battery is really empty after
// 499/1.5 minutes while the voltage still walks down the measured trajectory, i.e. the voltage
// is right and the lab runtime the gauge believes in is 50% too optimistic. That is the one
// thing leaning on a clock can get badly wrong, so it is measured here rather than hoped away.
static Result replay(const std::vector<Sample>& run, int startAt, bool verbose, const char* label,
                     float speed = 1.0f) {
	const int total = run.back().minute;
	Gauge g;
	std::vector<float> errs;
	Result r = {0, 0, 0, 0, true};
	int prevShown = 101;
	if (verbose) printf("  %s\n    minute    mV  shown  truth   err\n", label);
	for (const Sample& s : run) {
		if (s.minute < startAt) continue;
		g.update(uint32_t(60000.0f / speed), s.mv, false);
		const float truth = float(total - s.minute) / float(total) * 100.0f;
		const int shown = g.percent();
		// The leading rows of a run still carry the charger in the 60 s average; the gauge waits
		// those out (onCharger) and has nothing to show yet -- the same rows curveOnly() skips.
		if (g.onCharger()) continue;
		if (shown > prevShown) r.monotone = false;
		prevShown = shown;
		errs.push_back(float(shown) - truth);
		if (verbose && s.minute % 25 == 0)
			printf("    %6d  %4u  %5d  %5.1f  %+5.1f\n", s.minute, s.mv, shown, truth, float(shown) - truth);
		r.lastShown = shown;
	}
	std::vector<float> abs_;
	for (float e : errs) { abs_.push_back(std::fabs(e)); r.bias += e; }
	r.bias /= float(errs.size());
	std::sort(abs_.begin(), abs_.end());
	r.maxErr = abs_.back();
	r.medianErr = abs_[abs_.size() / 2];
	return r;
}

// What the plain voltage curve alone would have done -- the yardstick the clock has to beat.
static Result curveOnly(const std::vector<Sample>& run) {
	const int total = run.back().minute;
	std::vector<float> errs, abs_;
	Result r = {0, 0, 0, 0, true};
	for (const Sample& s : run) {
		if (s.mv >= 4100) continue;
		const float truth = float(total - s.minute) / float(total) * 100.0f;
		errs.push_back(float(curvePercent(s.mv)) - truth);
	}
	for (float e : errs) { abs_.push_back(std::fabs(e)); r.bias += e; }
	r.bias /= float(errs.size());
	std::sort(abs_.begin(), abs_.end());
	r.maxErr = abs_.back();
	r.medianErr = abs_[abs_.size() / 2];
	return r;
}

int main(int argc, char** argv) {
	const bool verbose = argc > 1 && strcmp(argv[1], "-v") == 0;

	// --- the curve itself -------------------------------------------------------------------
	CHECK(curvePercent(EMPTY_MV) == 0, "curve at the empty bound is %u, not 0", curvePercent(EMPTY_MV));
	CHECK(curvePercent(FULL_MV) == 100, "curve at the full bound is %u, not 100", curvePercent(FULL_MV));
	CHECK(curvePercent(2000) == 0 && curvePercent(4500) == 100, "the curve must clamp outside its bounds");
	int prev = -1;
	for (int mv = 2000; mv <= 4500; mv++) {
		const int p = curvePercent(uint16_t(mv));
		CHECK(p >= prev, "curve is not monotone: %d%% at %d mV after %d%%", p, mv, prev);
		prev = p;
	}
	// The plateau has to be the least trustworthy part, that is the whole premise of the gauge.
	CHECK(curveSlopePpPerMv(3530) > curveSlopePpPerMv(3800), "the plateau should be the steepest part of the curve");
	CHECK(curveSlopePpPerMv(3530) > curveSlopePpPerMv(3000), "the plateau should be steeper than the low end");

	// --- charging ----------------------------------------------------------------------------
	// On the charger nothing is measurable, so after it the gauge must take the voltage -- not
	// 100 %, not what it held before. 2026-10-16/17: a charge that stopped at 66 % was shown as
	// 100 % and the night ran out after five and a half hours.
	{
		Gauge g;
		g.anchor(3415);								// 22 %, as on 2026-10-16 19:46
		for (int i = 0; i < 274; i++) g.update(60000, 4660, true);
		CHECK(g.onCharger(), "on the charger the gauge must say so");
		CHECK(g.percent() == 22, "on the charger the estimate is held, got %u%%", g.percent());
		g.update(60000, 4300, false);				// average still carries the charger
		CHECK(g.onCharger(), "above the charger threshold the gauge is still on the charger");
		g.update(30000, 3630, false);				// clean, but not settled yet
		CHECK(g.percent() == 22, "before the settle time nothing may change, got %u%%", g.percent());
		g.update(60000, 3625, false);				// 1.5 min off the charger: anchors now
		CHECK(!g.onCharger(), "after the settle time the gauge is off the charger");
		CHECK(g.percent() == curvePercent(3625), "unplugging must anchor to the voltage: %u%% vs %u%%",
		      g.percent(), curvePercent(3625));
	}
	{
		// Booting with the charger attached: no starting point, and none is invented.
		Gauge g;
		for (int i = 0; i < 600; i++) g.update(60000, 4650, true);
		CHECK(!g.known(), "a boot on the charger must not claim to know anything");
		g.update(60000, 3600, false);
		g.update(30000, 3600, false);				// 1.5 min off the charger: anchors now
		CHECK(g.percent() == curvePercent(3600), "after unplugging: the curve, got %u%%", g.percent());
	}
	{
		Gauge g;
		CHECK(!g.known(), "a fresh gauge must not claim to know anything");
		g.anchor(3174);
		CHECK(g.percent() == 11, "anchor(3174) gives %u%%, expected the curve value 11%%", g.percent());
		const State saved = g.state();
		Gauge h;
		h.restore(saved);
		CHECK(h.percent() == g.percent(), "a restored gauge must show what was saved");
		Gauge bad;
		State nonsense = {}; nonsense.percent = NAN; nonsense.shown = NAN; nonsense.valid = 1;
		bad.restore(nonsense);
		CHECK(!bad.known(), "a NaN state must be rejected");
	}

	// --- the two measured runs -------------------------------------------------------------
	struct Run { const char* name; const char* path; };
	const Run runs[] = {
		{"2026-10-13", "Tools/TestData/battery-discharge-20261013.csv"},
		{"2026-10-16", "Tools/TestData/battery-discharge-20261016.csv"},
	};
	printf("\n  run         curve only       gauge            cold at 250 min  runtime 1.5x   runtime 0.75x\n");
	printf("              max  med  bias   max  med  bias   max  med  bias   max  med  bias  max  med  bias\n");
	for (const Run& run : runs) {
		std::vector<Sample> samples = load(run.path);
		if (samples.empty()) continue;
		const Result c = curveOnly(samples);
		const Result g = replay(samples, 0, verbose, run.name);
		const Result m = replay(samples, 250, false, run.name);
		const Result f = replay(samples, 0, false, run.name, 1.5f);
		const Result s = replay(samples, 0, false, run.name, 0.75f);
		printf("  %s %5.1f %4.1f %+5.1f  %5.1f %4.1f %+5.1f  %5.1f %4.1f %+5.1f  %5.1f %4.1f %+5.1f %5.1f %4.1f %+5.1f\n",
		       run.name, c.maxErr, c.medianErr, c.bias, g.maxErr, g.medianErr, g.bias,
		       m.maxErr, m.medianErr, m.bias, f.maxErr, f.medianErr, f.bias, s.maxErr, s.medianErr, s.bias);

		CHECK(g.maxErr < c.maxErr, "%s: the gauge (%.1f pp) must beat the bare curve (%.1f pp)", run.name, g.maxErr, c.maxErr);
		CHECK(g.maxErr <= 7.0f, "%s: worst error %.1f pp, budget is 7 pp", run.name, g.maxErr);
		CHECK(g.medianErr <= 5.0f, "%s: median error %.1f pp, budget is 5 pp", run.name, g.medianErr);
		// A clock that is 50% off is the worst this design can do. It must stay in the same
		// league as the bare curve -- if this budget ever has to grow, the gauge leans too
		// hard on fullRuntimeMin.
		CHECK(f.maxErr <= 20.0f, "%s: with the runtime 1.5x off the error is %.1f pp, budget is 20 pp", run.name, f.maxErr);
		CHECK(s.maxErr <= 20.0f, "%s: with the runtime 0.75x off the error is %.1f pp, budget is 20 pp", run.name, s.maxErr);
		CHECK(f.lastShown == 0 && s.lastShown == 0, "%s: a wrong runtime must still end at 0%%, got %d%% / %d%%", run.name, f.lastShown, s.lastShown);
		CHECK(g.monotone, "%s: the shown percentage rose while discharging", run.name);
		CHECK(g.lastShown == 0, "%s: ends at %d%%, expected 0%%", run.name, g.lastShown);
		// Switched on in the middle of a discharge the gauge only has the plateau voltage to go
		// by, so it starts off worse -- but it must not be worse than the curve it anchored to.
		CHECK(m.maxErr <= c.maxErr + 1.0f, "%s: cold start %.1f pp is worse than the bare curve %.1f pp", run.name, m.maxErr, c.maxErr);
	}

	printf("\n%s\n", failures ? "FAILURES" : "all checks passed");
	return failures != 0;
}
