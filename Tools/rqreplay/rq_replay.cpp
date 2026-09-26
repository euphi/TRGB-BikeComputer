/*
 * rq_replay -- run the firmware's road-quality algorithm (src/RoadQuality.cpp, unchanged)
 * over a raw accelerometer recording (src/RawCapture.h), with parameters to try out.
 *
 * Normally driven by "python3 -m bikelog raw replay", which builds this on demand. By hand:
 *
 *   g++ -std=c++17 -O2 -I../../src ../../src/RoadQuality.cpp rq_replay.cpp -o rq_replay
 *   ./rq_replay R_143012_01.bin shock=2.5 interval=1 baseline=0.12
 *
 * Output on stdout, one CSV line per result (t = ms since the file's start):
 *   I,t,class,R,rmsV,rmsH,peakVMax,peakVMin,peakT,vdv,speedKmh,distM,over1g,over2g,flags,gradRawPct,surface,quality
 *   S,t,ref,peak,vMax,vMin,horiz,durMs,secondG,secondDelayMs,severity,flags,thrG,preRmsG,surface,quality
 * (surface/quality: the manual road label of the block the result fell in, 0 = none.)
 * (NaN printed as "nan".) For a snippet file (S_*.bin) every block is replayed on its own,
 * with a fresh algorithm state -- the snippets are 0.75 s pieces, not a continuous signal --
 * and ref is the logged shock's event number.
 *
 * Parameters (key=value): interval shock rel off hp lp baseline exp vref vmin wheelbase
 * gtau maxevents t1 t2 sev2 sev3 latency
 */

#include "RawCapture.h"
#include "RoadQuality.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace RQ;

static bool readAll(const char* path, std::vector<uint8_t>& out) {
	FILE* f = fopen(path, "rb");
	if (!f) return false;
	uint8_t buf[65536];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.insert(out.end(), buf, buf + n);
	fclose(f);
	return true;
}

static void applyParams(Config& c, PitchEstimator::Config& p, const std::map<std::string, double>& kv) {
	for (auto& [k, v] : kv) {
		if (k == "interval") c.intervalS = (uint8_t)v;
		else if (k == "shock") c.shockAbsG = v;
		else if (k == "rel") c.shockRelFactor = v;
		else if (k == "off") c.shockOffFactor = v;
		else if (k == "hp") c.hpHz = v;
		else if (k == "lp") c.lpHz = v;
		else if (k == "baseline") {c.baselineG = v; c.baselineCalibrated = true;}
		else if (k == "exp") c.speedExp = v;
		else if (k == "vref") c.vRefKmh = v;
		else if (k == "vmin") c.vMinKmh = v;
		else if (k == "wheelbase") c.wheelbaseM = v;
		else if (k == "gtau") c.gravityTauS = v;
		else if (k == "maxevents") c.maxEventsPerMin = (uint8_t)v;
		else if (k == "t1") c.t1G = v;
		else if (k == "t2") c.t2G = v;
		else if (k == "sev2") c.sev2G = v;
		else if (k == "sev3") c.sev3G = v;
		else if (k == "latency") p.speedLatencyMs = v;
		else fprintf(stderr, "unknown parameter %s (ignored)\n", k.c_str());
	}
}

static void printInterval(double tMs, const IntervalResult& r, const PitchEstimator& pe, const RawCap::BlockHeader& bh) {
	printf("I,%.0f,%u,%.3f,%.4f,%.4f,%.3f,%.3f,%.3f,%.4f,%.2f,%.2f,%u,%u,%u,%.2f,%u,%u\n", tMs, r.roadClass, r.roughness,
	       r.rmsVertG, r.rmsHorizG, r.peakVertMaxG, r.peakVertMinG, r.peakTotalG, r.vdvVert, r.speedKmh, r.distanceM,
	       r.countOverT1, r.countOverT2, r.flags, pe.rawGradientPct(), bh.labelSurface, bh.labelQuality);
}

static void printShock(double tMs, uint32_t ref, const ShockResult& s, const RawCap::BlockHeader& bh) {
	printf("S,%.0f,%u,%.3f,%.3f,%.3f,%.3f,%.1f,%.3f,%.0f,%u,%u,%.3f,%.4f,%u,%u\n", tMs, ref, s.peakTotalG, s.peakVertMaxG,
	       s.peakVertMinG, s.peakHorizG, s.durationMs, s.secondPeakG, s.secondPeakDelayMs, s.severity, s.flags,
	       s.thresholdG, s.preRmsG, bh.labelSurface, bh.labelQuality);
}

int main(int argc, char** argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s <R_*.bin|S_*.bin> [key=value ...]\n", argv[0]);
		return 2;
	}
	std::vector<uint8_t> data;
	if (!readAll(argv[1], data)) {
		fprintf(stderr, "cannot read %s\n", argv[1]);
		return 1;
	}
	if (data.size() < sizeof(RawCap::FileHeader) || memcmp(data.data(), "BCRW", 4) != 0) {
		fprintf(stderr, "%s is not a raw capture file\n", argv[1]);
		return 1;
	}
	RawCap::FileHeader fh;
	memcpy(&fh, data.data(), sizeof(fh));
	if (fh.version != RawCap::VERSION) {
		fprintf(stderr, "raw format version %u, this tool reads %u\n", fh.version, RawCap::VERSION);
		return 1;
	}

	std::map<std::string, double> kv;
	for (int i = 2; i < argc; i++) {
		const char* eq = strchr(argv[i], '=');
		if (!eq) {fprintf(stderr, "ignoring %s (expected key=value)\n", argv[i]); continue;}
		kv[std::string(argv[i], eq - argv[i])] = atof(eq + 1);
	}
	Config cfg;
	cfg.odrHz = fh.odrHz;
	cfg.noiseG = fh.noiseG > 0 ? fh.noiseG : cfg.noiseG;
	cfg.wheelbaseM = fh.wheelbaseM > 0 ? fh.wheelbaseM : cfg.wheelbaseM;
	cfg.intervalS = fh.intervalS ? fh.intervalS : cfg.intervalS;
	PitchEstimator::Config pcfg;
	applyParams(cfg, pcfg, kv);

	const bool snippets = fh.kind == RawCap::KIND_SNIPPETS;
	const float k = fh.scale / fh.lsbPerG;
	const double framMs = 1000.0 / fh.odrHz;

	RoadQuality rq;
	PitchEstimator pe;
	rq.configure(cfg);
	pe.configure(pcfg);
	float g0[3] = {fh.g0[0] * fh.scale, fh.g0[1] * fh.scale, fh.g0[2] * fh.scale};
	pe.setGravityHint(g0);
	uint32_t lastSpeedUpd = 0;
	bool haveSpeedUpd = false;

	size_t pos = sizeof(fh);
	uint32_t blocks = 0, resyncs = 0;
	while (pos + sizeof(RawCap::BlockHeader) <= data.size()) {
		RawCap::BlockHeader bh;
		memcpy(&bh, data.data() + pos, sizeof(bh));
		if (bh.sync != RawCap::SYNC) {
			pos++;
			resyncs++;
			continue;
		}
		const size_t end = pos + sizeof(bh) + (size_t)bh.count * 6;
		if (end > data.size()) break;
		const int16_t* fr = reinterpret_cast<const int16_t*>(data.data() + pos + sizeof(bh));
		const double t0 = (double)(bh.epochMs - fh.startEpochMs);
		blocks++;

		if (snippets) {							// every snippet on its own
			rq = RoadQuality();
			rq.configure(cfg);
		} else if (bh.flags & RawCap::BF_GAP_BEFORE) {
			rq.notifyDataGap();
		}
		const float kmh = bh.speedCms == RawCap::SPEED_UNKNOWN ? NAN : bh.speedCms * 0.036f;
		rq.setSpeed(kmh, std::isnan(kmh) ? SpeedSource::NONE : ((bh.flags & RawCap::BF_SPEED_FROM_GPS) ? SpeedSource::GPS : SpeedSource::WHEEL));
		// Wheel-speed updates as the firmware's PitchEstimator saw them
		if (!snippets && bh.speedAgeMs != RawCap::SPEED_AGE_UNKNOWN && !std::isnan(kmh)) {
			const uint32_t upd = (uint32_t)(t0 - bh.speedAgeMs);
			if (!haveSpeedUpd || upd != lastSpeedUpd) {
				pe.onSpeed(upd, kmh);
				lastSpeedUpd = upd;
				haveSpeedUpd = true;
			}
		}

		float sum[3] = {0, 0, 0};
		for (uint16_t i = 0; i < bh.count; i++) {
			float a[3] = {fr[3 * i] * k, fr[3 * i + 1] * k, fr[3 * i + 2] * k};
			for (int j = 0; j < 3; j++) sum[j] += a[j];
			uint8_t ready = rq.process(a);
			const double t = t0 + i * framMs;
			if (ready & RoadQuality::READY_INTERVAL && !snippets) printInterval(t, rq.interval(), pe, bh);
			if (ready & RoadQuality::READY_SHOCK) {
				const ShockResult& s = rq.shock();
				printShock(t - s.samplesAgo * framMs, snippets ? bh.ref : s.seq, s, bh);
			}
		}
		if (!snippets && bh.count) {
			pe.addBlock(sum, bh.count, (uint32_t)(t0 + (bh.count - 1) * framMs));
			rq.setLongitudinal(pe.forward(), pe.longitudinalAccelG(), pe.longitudinalValid((uint32_t)t0));
		}
		pos = end;
	}
	fprintf(stderr, "%u blocks, %u resync byte(s)\n", blocks, resyncs);
	return 0;
}
