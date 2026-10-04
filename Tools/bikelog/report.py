"""Technical report of one session: every key figure, computed from the binary log.

This is the layer a report generator -- a template or a local LLM -- reads from.
Everything that is arithmetic happens here, deterministically and testably; a
language model only gets the result (a few KB of JSON instead of thousands of
records) and is told to quote nothing that is not in it.

    compute(records, athlete, stats) -> dict     JSON-serialisable, REPORT_VERSION
    to_markdown(report) -> str                   German report without any LLM

Sections of the dict:

  meta     time span, format, record counts, simulator, clock
  ride     the figures of ridestats.RideStats plus stops and logging gaps
  climbs   climbs found in the barometric profile, with the firmware's criteria
           and categories (src/ClimbProfile.h, doc/CLIMB.md)
  heart    zones, TRIMP, aerobic decoupling -- zones/TRIMP need Athlete.hr_max
  power    *estimated* from speed, gradient and mass (no power meter): mean,
           normalised power, best 5/20 min; good for trends, not absolute values
  road     road classes, manual labels against the automatic class, shocks
  health   the device's side: sensor dropouts, GPS freshness, baro vs. GPS
           altitude, lost records, cut-off file, unset clock; plus "findings",
           a list of {level, code, text} for whatever stood out

Times inside a ride are seconds since its start ("t_s") and positions are
trip kilometres ("km"): easier to read for a person and for a model than epoch
timestamps.
"""

from __future__ import annotations

import bisect
import datetime
import json
import math
from dataclasses import asdict, dataclass, field
from pathlib import Path

from . import ridestats
from .record import (IF_CLIPPED, IF_DATA_GAP, IF_NO_SPEED, IF_TOO_SLOW, IF_UNCALIBRATED,
                     ROAD_CLASS_NAMES, SURFACE_NAMES, LabelRecord, ReadStats, Record,
                     RideStateRecord, RoadQualityRecord, ShockEvent, label_at)

#: Bump when a field changes its meaning or moves -- consumers (the service's
#: cache, a prompt) key on it.
REPORT_VERSION = 1

#: Records before this are from an unset ESP32 clock (see gpx.MIN_PLAUSIBLE_YEAR).
MIN_PLAUSIBLE_TIMESTAMP = int(datetime.datetime(2020, 1, 1, tzinfo=datetime.timezone.utc).timestamp())

#: A stop: standing (speed below ridestats.MOVING_KMH) for at least this long.
MIN_STOP_S = 20.0
#: A fix older than this is not "fresh" (same threshold as the GPX export).
FRESH_FIX_MS = 5000
#: Decoupling needs this much moving time with heart rate, else it is noise.
MIN_DECOUPLING_S = 40 * 60
#: Rolling window of the normalised power (Coggan: 30 s).
NP_WINDOW_S = 30.0

# Climb finding -- the defaults of Climb::Config (src/ClimbProfile.h), so a
# climb here is the same climb the device would have shown.
CLIMB_GRID_M = 25.0
CLIMB_START_GRADE = 3.0
CLIMB_START_WINDOW_M = 100.0
CLIMB_CONT_GRADE = 0.5
CLIMB_SUMMIT_FLAT_M = 2000.0
CLIMB_SUMMIT_DIP_M = 30.0
CLIMB_MIN_LENGTH_M = 300.0
CLIMB_MIN_AVG_GRADE = 3.0
#: score = length [m] x mean gradient [%] for category 6, 5, 4, 3, 2, 1, HC
CLIMB_CAT_SCORE = (2000, 4000, 8000, 16000, 32000, 64000, 80000)
CLIMB_CAT_NAMES = ("6", "5", "4", "3", "2", "1", "HC")

G = 9.81
RHO_AIR = 1.2


@dataclass
class Athlete:
    """What the report needs to know about the rider. Everything optional:
    without hr_max there are no zones and no TRIMP, without mass the power
    estimate uses the default."""

    hr_max: int | None = None
    hr_rest: int | None = None
    #: rider + bike + luggage (power model)
    mass_kg: float = 85.0
    #: rider alone, for W/kg; without it there is no W/kg
    rider_kg: float | None = None
    #: drag area (upright on the hoods ~0.40, drops ~0.32)
    cda: float = 0.40
    #: rolling resistance (road tyre on asphalt ~0.005, gravel ~0.008)
    crr: float = 0.006
    #: drivetrain efficiency
    drivetrain: float = 0.97
    #: upper bounds of zones 1..4 in % of hr_max; zone 5 is everything above
    zones_pct: tuple[float, ...] = (60.0, 70.0, 80.0, 90.0)
    #: Banister's TRIMP weighting (1.92; 1.67 is the value Banister gives for women)
    trimp_b: float = 1.92

    @classmethod
    def load(cls, path) -> "Athlete":
        data = json.loads(Path(path).read_text(encoding="utf-8"))
        known = {f for f in cls.__dataclass_fields__}
        unknown = set(data) - known
        if unknown:
            raise ValueError("unknown key(s) in %s: %s" % (path, ", ".join(sorted(unknown))))
        if "zones_pct" in data:
            data["zones_pct"] = tuple(float(x) for x in data["zones_pct"])
        return cls(**data)


# --- small helpers -----------------------------------------------------------

def _r(value, digits: int = 1):
    """Round for the JSON; None stays None."""
    if value is None:
        return None
    return round(value, digits) if digits else int(round(value))


def _median(values: list[float]) -> float | None:
    if not values:
        return None
    s = sorted(values)
    n = len(s)
    return s[n // 2] if n % 2 else (s[n // 2 - 1] + s[n // 2]) / 2


def _quantile(values: list[float], q: float) -> float | None:
    if not values:
        return None
    s = sorted(values)
    return s[min(len(s) - 1, int(q * len(s)))]


def _height(rec: Record) -> float | None:
    return rec.height if -500 <= rec.height <= 9000 and rec.height != 0.0 else None


def _km(km: float) -> str:
    return f"{km:.1f}".replace(".", ",")


def _seq_gaps(seqs: list[int]) -> int:
    """Missing numbers in a running sequence (records lost on the way to the card)."""
    if not seqs:
        return 0
    return max(0, max(seqs) - min(seqs) + 1 - len(set(seqs)))


@dataclass
class _Step:
    """One interval between two consecutive data records."""

    t: float            # seconds since start, at the end of the interval
    dt: float
    km: float
    speed_kmh: float
    moving: bool
    hr: int | None
    cadence: int | None
    power_w: float | None


def _power(athlete: Athlete, v: float, v_prev: float, dt: float, grade_pct: float) -> float:
    """Pedal power needed for speed v [m/s] on a gradient, from the usual
    model: rolling + climbing + air + acceleration, divided by drivetrain
    losses. No wind -- that is the main error. Never negative: coasting
    downhill is 0 W, not energy back."""
    angle = math.atan(grade_pct / 100.0)
    m = athlete.mass_kg
    rolling = m * G * athlete.crr * math.cos(angle) * v
    climbing = m * G * math.sin(angle) * v
    air = 0.5 * RHO_AIR * athlete.cda * v ** 3
    accel = m * (v - v_prev) / dt * v if dt > 0 else 0.0
    return max(0.0, (rolling + climbing + air + accel) / athlete.drivetrain)


def _steps(records: list[Record], athlete: Athlete) -> list[_Step]:
    steps = []
    t0 = records[0].time
    d0 = records[0].distance
    for prev, rec in zip(records, records[1:]):
        dt = rec.time - prev.time
        if not 0 < dt <= ridestats.MAX_STEP_S:
            continue
        moving = rec.speed >= ridestats.MOVING_KMH
        grade = rec.gradient if -40 <= rec.gradient <= 40 else 0.0
        steps.append(_Step(
            t=rec.time - t0, dt=dt, km=(rec.distance - d0) / 1000.0, speed_kmh=rec.speed,
            moving=moving,
            hr=rec.hr if ridestats.hr_valid(rec.hr) else None,
            cadence=rec.cadence if ridestats.cadence_valid(rec.cadence) else None,
            power_w=_power(athlete, rec.speed_ms, prev.speed_ms, dt, grade) if moving else 0.0,
        ))
    return steps


def _best_mean(steps: list[_Step], window_s: float, attr: str) -> float | None:
    """Highest time-weighted mean of ``attr`` over any stretch of ``window_s``
    seconds without a logging gap (two pointers over the steps)."""
    best = None
    lo = 0
    acc = 0.0
    span = 0.0
    for hi, st in enumerate(steps):
        if hi and steps[hi].t - steps[hi - 1].t > steps[hi].dt + 1e-6:
            lo, acc, span = hi, 0.0, 0.0       # gap: start over
        value = getattr(st, attr)
        if value is None:
            lo, acc, span = hi + 1, 0.0, 0.0
            continue
        acc += value * st.dt
        span += st.dt
        while span - steps[lo].dt >= window_s:
            acc -= getattr(steps[lo], attr) * steps[lo].dt
            span -= steps[lo].dt
            lo += 1
        if span >= window_s:
            mean = acc / span
            best = mean if best is None else max(best, mean)
    return best


# --- sections ------------------------------------------------------------------

def _meta(records: list[Record], stats: ReadStats | None, dropped_clock: int) -> dict:
    first, last = records[0], records[-1]
    return {
        "start_utc": first.utc().isoformat(timespec="seconds"),
        "end_utc": last.utc().isoformat(timespec="seconds"),
        "duration_s": _r(last.time - first.time, 0),
        "format_version": stats.version if stats else None,
        "records": {"data": len(records), **({str(k): v for k, v in sorted(stats.by_type.items())}
                                             if stats else {})},
        "simulated": any(r.simulated for r in records),
        "unset_clock_records": dropped_clock,
    }


def _ride(rs: ridestats.RideStats, records: list[Record], steps: list[_Step]) -> dict:
    # A stop is standing for MIN_STOP_S, or a logging gap: the device logs
    # nothing while it sleeps at a longer stop.
    stops = []
    standing = None                     # [start time, end time]
    for prev, rec in zip(records, records[1:]):
        gap = rec.time - prev.time > ridestats.MAX_STEP_S
        if gap or rec.speed < ridestats.MOVING_KMH:
            standing = [standing[0] if standing else prev.time, rec.time]
            if rec.speed < ridestats.MOVING_KMH:
                continue                # still standing after the gap: same stop
        if standing and standing[1] - standing[0] >= MIN_STOP_S:
            stops.append(standing[1] - standing[0])
        standing = None
    if standing and standing[1] - standing[0] >= MIN_STOP_S:
        stops.append(standing[1] - standing[0])
    data = {
        "distance_km": _r(rs.distance_m / 1000, 2),
        "duration_s": _r(rs.duration_s, 0),
        "moving_s": _r(rs.moving_s, 0),
        "avg_moving_kmh": _r(rs.avg_moving_kmh),
        "max_kmh": _r(rs.max_speed_kmh),
        "ascent_m": _r(rs.ascent_m, 0),
        "descent_m": _r(rs.descent_m, 0),
        "min_ele_m": _r(rs.min_ele_m, 0),
        "max_ele_m": _r(rs.max_ele_m, 0),
        "max_grade_pct": _r(rs.max_grade),
        "min_grade_pct": _r(rs.min_grade),
        "temp_c": None if rs.min_temp is None else {"min": _r(rs.min_temp), "max": _r(rs.max_temp)},
        "avg_cadence": rs.avg_cadence,
        "pedalling_share": None,
        "stops": {"count": len(stops), "total_s": _r(sum(stops), 0),
                  "longest_s": _r(max(stops, default=0), 0)},
    }
    moving = [s for s in steps if s.moving]
    if moving:
        pedal = sum(s.dt for s in moving if s.cadence)
        data["pedalling_share"] = _r(pedal / sum(s.dt for s in moving), 2)
    return data


def _profile_grid(records: list[Record]) -> list[tuple[float, float, float]]:
    """(distance from start [m], height [m], seconds since start) every
    CLIMB_GRID_M, linearly interpolated from the barometric heights."""
    pts = []
    t0, d0 = records[0].time, records[0].distance
    for rec in records:
        h = _height(rec)
        if h is None:
            continue
        d = rec.distance - d0
        if pts and d <= pts[-1][0]:
            continue                    # standing: keep the first height of the stop
        pts.append((d, h, rec.time - t0))
    if len(pts) < 2:
        return []
    grid = []
    k = 0
    x = 0.0
    while x <= pts[-1][0]:
        while pts[k + 1][0] < x:
            k += 1
        (d1, h1, t1), (d2, h2, t2) = pts[k], pts[k + 1]
        f = (x - d1) / (d2 - d1)
        grid.append((x, h1 + f * (h2 - h1), t1 + f * (t2 - t1)))
        x += CLIMB_GRID_M
    return grid


def find_climbs(grid: list[tuple[float, float, float]]) -> list[tuple[int, int]]:
    """(foot, summit) grid indices, with the criteria of ClimbProfile.cpp: foot =
    first point from which the next 100 m rise by 3 % on average; the climb goes
    on to a later point that is higher than the summit so far by 0.5 % of the
    way to it and at most 2 km further; a descent of more than 30 m ends it."""
    climbs = []
    n = len(grid)
    win = int(round(CLIMB_START_WINDOW_M / CLIMB_GRID_M))
    flat = int(round(CLIMB_SUMMIT_FLAT_M / CLIMB_GRID_M))
    i = 0
    while i + win < n:
        if (grid[i + win][1] - grid[i][1]) / CLIMB_START_WINDOW_M * 100 < CLIMB_START_GRADE:
            i += 1
            continue
        foot = summit = i
        j = i + 1
        while j < n and j - summit <= flat:
            rise = grid[j][1] - grid[summit][1]
            if rise > 0 and rise >= (j - summit) * CLIMB_GRID_M * CLIMB_CONT_GRADE / 100:
                summit = j
            if grid[summit][1] - grid[j][1] > CLIMB_SUMMIT_DIP_M:
                break
            j += 1
        if summit > foot:
            climbs.append((foot, summit))
        i = summit + 1
    return climbs


def climb_category(length_m: float, gain_m: float) -> str | None:
    """None for a hill that is not rated (too short, too flat, below category 6)."""
    if length_m < CLIMB_MIN_LENGTH_M or gain_m / length_m * 100 < CLIMB_MIN_AVG_GRADE:
        return None
    score = gain_m * 100
    cat = None
    for name, threshold in zip(CLIMB_CAT_NAMES, CLIMB_CAT_SCORE):
        if score >= threshold:
            cat = name
    return cat


def _climbs(records: list[Record], steps: list[_Step], with_hills: bool) -> list[dict]:
    grid = _profile_grid(records)
    out = []
    for foot, summit in find_climbs(grid):
        d1, h1, t1 = grid[foot]
        d2, h2, t2 = grid[summit]
        length, gain = d2 - d1, h2 - h1
        cat = climb_category(length, gain)
        if cat is None and not with_hills:
            continue
        inside = [s for s in steps if t1 < s.t <= t2]
        dur = sum(s.dt for s in inside)
        hrs = [(s.hr, s.dt) for s in inside if s.hr]
        pw = [(s.power_w, s.dt) for s in inside if s.power_w is not None]
        out.append({
            "category": cat,
            "start_km": _r(d1 / 1000, 2),
            "length_m": _r(length, 0),
            "gain_m": _r(gain, 0),
            "avg_grade_pct": _r(gain / length * 100),
            # height gain over 100 m (4 grid steps) in metres = gradient in %
            "max_grade_100m_pct": _r(max((grid[k + 4][1] - grid[k][1] for k in range(foot, summit - 3)),
                                         default=gain / length * 100)),
            "start_t_s": _r(t1, 0),
            "duration_s": _r(dur, 0),
            "avg_kmh": _r(length / dur * 3.6) if dur else None,
            "vam_m_h": _r(gain / dur * 3600, 0) if dur else None,
            "avg_hr": _r(sum(h * d for h, d in hrs) / sum(d for _, d in hrs), 0) if hrs else None,
            "max_hr": max((h for h, _ in hrs), default=None),
            "est_avg_power_w": _r(sum(p * d for p, d in pw) / sum(d for _, d in pw), 0) if pw else None,
        })
    return out


def _heart(steps: list[_Step], athlete: Athlete) -> dict:
    moving = [s for s in steps if s.moving]
    with_hr = [s for s in steps if s.hr]
    data: dict = {
        "coverage_moving": _r(sum(s.dt for s in moving if s.hr) / sum(s.dt for s in moving), 2)
        if moving else None,
        "avg": _r(sum(s.hr * s.dt for s in with_hr) / sum(s.dt for s in with_hr), 0) if with_hr else None,
        "max": max((s.hr for s in with_hr), default=None),
        "zones_s": None,
        "trimp": None,
        "trimp_method": None,
        "decoupling_pct": None,
        "efficiency_w_per_bpm": None,
    }
    if not with_hr:
        return data
    if athlete.hr_max:
        bounds = [athlete.hr_max * p / 100 for p in athlete.zones_pct]
        zones = [0.0] * (len(bounds) + 1)
        for s in with_hr:
            zones[bisect.bisect_right(bounds, s.hr)] += s.dt
        data["zones_s"] = {"Z%d" % (i + 1): _r(z, 0) for i, z in enumerate(zones)}
        data["zones_bpm"] = [_r(b, 0) for b in bounds]
        if athlete.hr_rest and athlete.hr_max > athlete.hr_rest:
            # Banister: minutes x HRr x 0.64 e^(b HRr)
            trimp = 0.0
            for s in with_hr:
                hrr = min(1.0, max(0.0, (s.hr - athlete.hr_rest) / (athlete.hr_max - athlete.hr_rest)))
                trimp += s.dt / 60 * hrr * 0.64 * math.exp(athlete.trimp_b * hrr)
            data["trimp"], data["trimp_method"] = _r(trimp, 0), "banister"
        else:
            # Edwards: minutes in zone x zone number, zones from 50 % hr_max up
            edges = [athlete.hr_max * p for p in (0.5, 0.6, 0.7, 0.8, 0.9)]
            trimp = sum(s.dt / 60 * bisect.bisect_right(edges, s.hr) for s in with_hr)
            data["trimp"], data["trimp_method"] = _r(trimp, 0), "edwards"
    both = [s for s in moving if s.hr and s.power_w is not None]
    total = sum(s.dt for s in both)
    if both:
        data["efficiency_w_per_bpm"] = _r(sum(s.power_w * s.dt for s in both) / sum(s.hr * s.dt for s in both), 2)
    if total >= MIN_DECOUPLING_S:
        # Pa:HR -- (power/HR first half) vs. (power/HR second half) of the moving time
        half, acc = total / 2, 0.0
        first, second = [], []
        for s in both:
            (first if acc < half else second).append(s)
            acc += s.dt
        def ratio(part):
            hr = sum(s.hr * s.dt for s in part)
            return sum(s.power_w * s.dt for s in part) / hr if hr else None
        r1, r2 = ratio(first), ratio(second)
        if r1 and r2:
            data["decoupling_pct"] = _r((r1 - r2) / r1 * 100)
    return data


def _power_section(steps: list[_Step], athlete: Athlete) -> dict:
    moving = [s for s in steps if s.moving and s.power_w is not None]
    if not moving:
        return {"estimated": True, "avg_moving_w": None}
    t_mov = sum(s.dt for s in moving)
    work = sum(s.power_w * s.dt for s in steps if s.power_w is not None)
    # Normalised power: 30 s rolling mean, 4th power, mean, 4th root -- over all
    # steps, standing counted as 0 W like a power meter would.
    rolled = []
    window: list[_Step] = []
    span = 0.0
    for s in steps:
        window.append(s)
        span += s.dt
        while span - window[0].dt >= NP_WINDOW_S:
            span -= window.pop(0).dt
        if span >= NP_WINDOW_S * 0.8:
            rolled.append((sum(x.power_w * x.dt for x in window) / span, s.dt))
    np_w = (sum(p ** 4 * d for p, d in rolled) / sum(d for _, d in rolled)) ** 0.25 if rolled else None
    best = {"%dmin" % m: _r(_best_mean(steps, m * 60, "power_w"), 0) for m in (1, 5, 20, 60)}
    return {
        "estimated": True,
        "model": {"mass_kg": athlete.mass_kg, "cda": athlete.cda, "crr": athlete.crr},
        "avg_moving_w": _r(sum(s.power_w * s.dt for s in moving) / t_mov, 0),
        "normalized_w": _r(np_w, 0),
        "work_kj": _r(work / 1000, 0),
        "best_w": best,
        "best_w_per_kg": None,
    }


def _road(road: list[RoadQualityRecord], shocks: list[ShockEvent], labels: list[LabelRecord],
          t0: float, d_at) -> dict:
    by_class: dict[str, float] = {}
    for rq in road:
        name = ROAD_CLASS_NAMES.get(rq.road_class, "?")
        by_class[name] = by_class.get(name, 0.0) + rq.distance_m
    rated_m = sum(m for name, m in by_class.items() if name != ROAD_CLASS_NAMES[0])
    groups: dict[tuple[int, int], list[RoadQualityRecord]] = {}
    for rq in road:
        key = label_at(labels, rq.time)
        if key != (0, 0):
            groups.setdefault(key, []).append(rq)
    label_cmp = []
    for (surf, q), rs in sorted(groups.items()):
        rated = [r for r in rs if r.rated]
        rough = [r.roughness for r in rated if r.roughness is not None]
        label_cmp.append({
            "surface": SURFACE_NAMES.get(surf, "?") or None,
            "quality": q or None,
            "km": _r(sum(r.distance_m for r in rs) / 1000, 2),
            "roughness_median": _r(_median(rough), 2),
            "classes_1_5": [sum(1 for r in rated if r.road_class == c) for c in range(1, 6)],
        })
    top = sorted(shocks, key=lambda s: s.peak_total_mg, reverse=True)[:5]
    return {
        "intervals": len(road),
        "class_km": {k: _r(v / 1000, 2) for k, v in by_class.items()},
        "class_share_rated": {k: _r(v / rated_m, 2) for k, v in by_class.items()
                              if rated_m and k != ROAD_CLASS_NAMES[0]},
        "uncalibrated": any(r.flags & IF_UNCALIBRATED for r in road),
        "labels": label_cmp,
        "shocks": {
            "count": len(shocks),
            "by_severity": {str(s): sum(1 for x in shocks if x.severity == s) for s in (1, 2, 3)},
            "suppressed": sum(r.events_suppressed for r in road),
            "top": [{"t_s": _r(s.time - t0, 0), "km": _r(d_at(s.time) / 1000, 2), "g": _r(s.peak_g),
                     "severity": s.severity, "kmh": _r(s.speed_kmh),
                     "both_wheels": s.wheelbase_match,
                     "lat": round(s.latitude, 5) if s.gps_valid else None,
                     "lon": round(s.longitude, 5) if s.gps_valid else None} for s in top],
        },
    }


def _dropouts(records: list[Record], valid, t0: float) -> list[dict]:
    """Stretches while moving in which a sensor that had delivered stopped
    delivering: [{t_s, duration_s, km}]. Only between two valid readings --
    a sensor switched on late or off early is not a dropout."""
    out = []
    start = None
    seen = False
    for rec in records[1:]:
        ok = valid(rec)
        if ok:
            if start is not None and rec.time - start.time >= 10:
                out.append({"t_s": _r(start.time - t0, 0), "duration_s": _r(rec.time - start.time, 0),
                            "km": _r((start.distance - records[0].distance) / 1000, 2)})
            start = None
            seen = True
        elif seen and start is None and rec.speed >= ridestats.MOVING_KMH:
            start = rec
    return out


def _health(records: list[Record], road: list[RoadQualityRecord], shocks: list[ShockEvent],
            labels: list[LabelRecord], states: list[RideStateRecord], stats: ReadStats | None,
            dropped_clock: int) -> dict:
    t0 = records[0].time
    findings: list[dict] = []

    def find(level: str, code: str, text: str) -> None:
        findings.append({"level": level, "code": code, "text": text})

    moving = [r for r in records if r.speed >= ridestats.MOVING_KMH]
    hr_drop = _dropouts(records, lambda r: ridestats.hr_valid(r.hr), t0)
    hr_no_reading = sum(1 for r in moving if r.hr == 255)
    cad_no_reading = sum(1 for r in moving if r.cadence == 255)
    if hr_drop:
        find("warn", "hr_dropout", "Puls %d-mal ausgefallen, zusammen %d s (längster %d s ab km %s)" % (
            len(hr_drop), sum(d["duration_s"] for d in hr_drop),
            max(d["duration_s"] for d in hr_drop),
            _km(max(hr_drop, key=lambda d: d["duration_s"])["km"])))
    if hr_no_reading:
        find("info", "hr_no_reading", "%d Datensätze mit verbundenem Pulsgurt ohne Wert (255) -- "
             "Kontakt/Elektroden?" % hr_no_reading)

    # Wheel sensor: GPS says riding, wheel says standing.
    gps_moving = [r for r in records if r.gps_valid and r.gps_fix_age_ms <= FRESH_FIX_MS
                  and r.gps_speed_ms is not None and r.gps_speed_ms * 3.6 >= 10]
    wheel_dead = sum(1 for r in gps_moving if r.speed < ridestats.MOVING_KMH)
    if gps_moving and wheel_dead / len(gps_moving) > 0.02:
        find("warn", "wheel_dropout", "Geschwindigkeitssensor: in %d von %d Datensätzen fährt laut "
             "GPS das Rad (>= 10 km/h), der Sensor meldet Stillstand" % (wheel_dead, len(gps_moving)))
    # Wheel circumference: wheel vs. GPS speed while riding steadily
    ratios = [r.speed / (r.gps_speed_ms * 3.6) for r in gps_moving
              if r.speed >= 10 and (r.gps_accuracy_m or 99) <= 10]
    wheel_ratio = _median(ratios) if len(ratios) >= 30 else None
    if wheel_ratio and abs(wheel_ratio - 1) > 0.03:
        find("warn", "wheel_circumference", "Radsensor misst %s %% gegenüber GPS -- Radumfang prüfen"
             % f"{(wheel_ratio - 1) * 100:+.1f}".replace(".", ","))

    with_fix = [r for r in records if r.gps_valid]
    fresh = [r for r in with_fix if r.gps_fix_age_ms <= FRESH_FIX_MS]
    gps_gaps = _dropouts(records, lambda r: r.gps_valid and r.gps_fix_age_ms <= FRESH_FIX_MS, t0)
    acc = [r.gps_accuracy_m for r in fresh if r.gps_accuracy_m is not None]
    if records and not with_fix:
        find("info", "no_gps", "Kein GPS (TrailBridge nicht verbunden) -- keine Spur, kein Map-Matching")
    elif gps_gaps:
        longest = max(gps_gaps, key=lambda d: d["duration_s"])
        if longest["duration_s"] >= 60:
            find("warn", "gps_gap", "GPS %d-mal ohne frischen Fix, längste Lücke %d s ab km %s" % (
                len(gps_gaps), longest["duration_s"], _km(longest["km"])))

    # Barometric vs. GPS altitude: the offset is calibration, a change of the
    # offset over the ride is drift (weather) -- see doc/HEIGHT.md.
    pairs = [(r.time, _height(r) - r.gps_altitude_m) for r in fresh
             if r.has_gps_altitude and _height(r) is not None and (r.gps_accuracy_m or 99) <= 15]
    baro = None
    if len(pairs) >= 40:
        q = len(pairs) // 4
        first = _median([d for _, d in pairs[:q]])
        last = _median([d for _, d in pairs[-q:]])
        baro = {"offset_m": _r(_median([d for _, d in pairs]), 0), "drift_m": _r(last - first, 0)}
        if abs(baro["offset_m"]) > 25:
            find("info", "baro_offset", "Baro-Höhe liegt im Mittel %+d m neben der GPS-Höhe -- "
                 "Höhe kalibrieren (Einstellungen > Höhe)" % baro["offset_m"])
        if abs(baro["drift_m"]) > 20:
            find("info", "baro_drift", "Baro-Höhe driftet gegenüber GPS um %+d m (Wetter/Luftdruck)"
                 % baro["drift_m"])

    lost = {"shocks": _seq_gaps([s.event_seq for s in shocks]),
            "labels": _seq_gaps([lab.label_seq for lab in labels]),
            "ride_states": _seq_gaps([s.state_seq for s in states])}
    if any(lost.values()):
        find("warn", "records_lost", "Lücken in der Nummerierung -- verlorene Datensätze: " +
             ", ".join("%s %d" % kv for kv in lost.items() if kv[1]))

    log_gaps = [(prev, rec) for prev, rec in zip(records, records[1:])
                if rec.time - prev.time > ridestats.MAX_STEP_S]
    if log_gaps:
        longest = max(log_gaps, key=lambda pr: pr[1].time - pr[0].time)
        find("info", "log_gap", "%d Aufzeichnungslücke(n) > %d s (Gerät schlief oder Log "
             "unterbrochen), längste %d s ab km %s" % (
            len(log_gaps), ridestats.MAX_STEP_S, longest[1].time - longest[0].time,
            _km((longest[0].distance - records[0].distance) / 1000)))

    trailing = stats.trailing_bytes if stats else 0
    if trailing:
        find("warn", "truncated", "Datei endet mit %d angebrochenen Bytes -- Aufzeichnung abgebrochen "
             "(Akku, Reset, Karte gezogen?)" % trailing)
    if stats and stats.unknown_type:
        find("info", "unknown_types", "%d Datensätze unbekannten Typs (neuere Firmware als bikelog?)"
             % stats.unknown_type)
    if dropped_clock:
        find("info", "clock_unset", "%d Datensätze mit ungesetzter Uhr (vor NTP/GPS-Zeit) ausgelassen"
             % dropped_clock)
    if any(r.simulated for r in records):
        find("info", "simulated", "Sensorwerte vom Simulator, keine echte Fahrt")

    rq = {
        "uncalibrated_share": _r(sum(1 for r in road if r.flags & IF_UNCALIBRATED) / len(road), 2) if road else None,
        "data_gap": sum(1 for r in road if r.flags & IF_DATA_GAP),
        "clipped": sum(1 for r in road if r.flags & IF_CLIPPED),
        "too_slow": sum(1 for r in road if r.flags & (IF_TOO_SLOW | IF_NO_SPEED)),
    }
    if road and rq["uncalibrated_share"] > 0.5:
        find("info", "rq_uncalibrated", "Wegequalität ohne Referenzfahrt -- Klassen beruhen auf der "
             "Standard-Baseline")
    if rq["data_gap"]:
        find("warn", "imu_data_gap", "%d Wegequalitäts-Intervalle mit Lücke in den IMU-Daten" % rq["data_gap"])
    if rq["clipped"]:
        find("info", "imu_clipped", "%d Intervalle mit übersteuertem Beschleunigungssensor" % rq["clipped"])

    temps = [r.temp for r in records if r.temp != 0.0]
    if temps and (min(temps) < -30 or max(temps) > 60):
        find("warn", "temp_implausible", "Temperatur außerhalb -30..60 °C (%d..%d) -- Sensor?"
             % (round(min(temps)), round(max(temps))))

    return {
        "hr": {"dropouts": hr_drop, "no_reading_records": hr_no_reading},
        "cadence": {"no_reading_records": cad_no_reading},
        "wheel": {"gps_moving_records": len(gps_moving), "stopped_while_gps_moving": wheel_dead,
                  "speed_ratio_to_gps": _r(wheel_ratio, 3)},
        "gps": {"records_with_fix": len(with_fix), "fresh": len(fresh),
                "fresh_share": _r(len(fresh) / len(records), 2),
                "gaps": gps_gaps,
                "accuracy_m_median": _r(_median(acc)), "accuracy_m_p90": _r(_quantile(acc, 0.9))},
        "baro_vs_gps": baro,
        "lost_records": lost,
        "log_gaps": len(log_gaps),
        "trailing_bytes": trailing,
        "road_quality": rq,
        "findings": findings,
    }


# --- entry points --------------------------------------------------------------

def compute(everything: list, athlete: Athlete | None = None, stats: ReadStats | None = None,
            with_hills: bool = False) -> dict:
    """The report of a session from all of its records (read_file(..., types=None)).
    ``with_hills`` also lists climbs below category 6."""
    athlete = athlete or Athlete()
    data = [r for r in everything if isinstance(r, Record)]
    plausible = [r for r in data if r.timestamp >= MIN_PLAUSIBLE_TIMESTAMP]
    # Unset-clock records in front of the ride: leave them out, but only if
    # anything is left -- an entire ride without clock is still a ride.
    records = plausible or data
    dropped = len(data) - len(records)
    if not records:
        return {"report_version": REPORT_VERSION, "empty": True}
    road = [r for r in everything if isinstance(r, RoadQualityRecord)]
    shocks = [r for r in everything if isinstance(r, ShockEvent)]
    labels = [r for r in everything if isinstance(r, LabelRecord)]
    states = [r for r in everything if isinstance(r, RideStateRecord)]

    t0 = records[0].time
    times = [r.time for r in records]

    def d_at(t: float) -> float:
        i = max(0, bisect.bisect_right(times, t) - 1)
        return records[i].distance - records[0].distance

    rs = ridestats.compute(records, road, shocks, labels)
    steps = _steps(records, athlete)
    power = _power_section(steps, athlete)
    if power.get("best_w") and athlete.rider_kg:
        power["best_w_per_kg"] = {k: _r(v / athlete.rider_kg, 2) if v else None
                                  for k, v in power["best_w"].items()}
    return {
        "report_version": REPORT_VERSION,
        "meta": _meta(records, stats, dropped),
        "athlete": asdict(athlete),
        "ride": _ride(rs, records, steps),
        "climbs": _climbs(records, steps, with_hills),
        "heart": _heart(steps, athlete),
        "power": power,
        "road": _road(road, shocks, labels, t0, d_at),
        "health": _health(records, road, shocks, labels, states, stats, dropped),
    }


# --- Markdown ------------------------------------------------------------------

def _num(value, digits: int = 1) -> str:
    if value is None:
        return "–"
    return f"{value:.{digits}f}".replace(".", ",")


def _hms(seconds) -> str:
    if seconds is None:
        return "–"
    seconds = int(seconds)
    h, rest = divmod(seconds, 3600)
    return f"{h}:{rest // 60:02d}:{rest % 60:02d}"


def to_markdown(rep: dict, title: str | None = None) -> str:
    """The report as German Markdown, straight from the figures -- the
    fallback when no LLM is around, and the reference an LLM text must agree with."""
    if rep.get("empty"):
        return "# Sitzungsbericht\n\nLeeres Log.\n"
    meta, ride, heart, power, road, health = (rep[k] for k in ("meta", "ride", "heart", "power", "road", "health"))
    start = datetime.datetime.fromisoformat(meta["start_utc"]).astimezone()
    out = [f"# {title or 'Sitzungsbericht ' + start.strftime('%d.%m.%Y %H:%M')}", ""]
    if meta["simulated"]:
        out += ["> **Simulierte Sensorwerte** -- keine echte Fahrt.", ""]

    out += ["## Fahrt", "",
            "| | |", "|---|---|",
            f"| Strecke | {_num(ride['distance_km'], 2)} km |",
            f"| Fahrzeit / Gesamt | {_hms(ride['moving_s'])} / {_hms(ride['duration_s'])} |",
            f"| Ø / max. | {_num(ride['avg_moving_kmh'])} / {_num(ride['max_kmh'])} km/h |",
            f"| Höhenmeter | ↑ {ride['ascent_m']} m ↓ {ride['descent_m']} m "
            f"({ride['min_ele_m']}–{ride['max_ele_m']} m) |",
            f"| Stopps | {ride['stops']['count']} ({_hms(ride['stops']['total_s'])}, längster "
            f"{_hms(ride['stops']['longest_s'])}) |"]
    if ride["temp_c"]:
        out.append(f"| Temperatur | {_num(ride['temp_c']['min'], 0)}–{_num(ride['temp_c']['max'], 0)} °C |")
    if ride["avg_cadence"]:
        out.append(f"| Trittfrequenz | Ø {ride['avg_cadence']} rpm, getreten "
                   f"{_num((ride['pedalling_share'] or 0) * 100, 0)} % der Fahrzeit |")
    out.append("")

    if rep["climbs"]:
        out += ["## Anstiege", "",
                "| Kat. | ab km | Länge | Höhe | Ø / max. % | Zeit | VAM | Puls Ø/max | Leistung* |",
                "|---|---|---|---|---|---|---|---|---|"]
        for c in rep["climbs"]:
            out.append(
                f"| {c['category'] or '–'} | {_num(c['start_km'])} | {_num(c['length_m'] / 1000, 2)} km "
                f"| {c['gain_m']} m | {_num(c['avg_grade_pct'])} / {_num(c['max_grade_100m_pct'])} "
                f"| {_hms(c['duration_s'])} | {c['vam_m_h'] or '–'} "
                f"| {c['avg_hr'] or '–'}/{c['max_hr'] or '–'} | {c['est_avg_power_w'] or '–'} W |")
        out.append("")

    out += ["## Herz und Belastung", ""]
    if heart["avg"]:
        out.append(f"- Puls Ø {heart['avg']} / max. {heart['max']} bpm, Abdeckung "
                   f"{_num((heart['coverage_moving'] or 0) * 100, 0)} % der Fahrzeit")
        if heart["zones_s"]:
            total = sum(heart["zones_s"].values()) or 1
            out.append("- Zonen: " + ", ".join(f"{z} {_num(s / total * 100, 0)} %"
                                               for z, s in heart["zones_s"].items()))
        if heart["trimp"] is not None:
            out.append(f"- TRIMP ({heart['trimp_method']}): {heart['trimp']}")
        if heart["decoupling_pct"] is not None:
            out.append(f"- Aerobe Entkopplung (Leistung/Puls, 1. vs. 2. Hälfte): "
                       f"{_num(heart['decoupling_pct'])} %")
    else:
        out.append("- Kein Puls aufgezeichnet.")
    if power.get("avg_moving_w") is not None:
        best = power["best_w"]
        out.append(f"- Leistung* Ø {power['avg_moving_w']} W, normalisiert {power['normalized_w']} W, "
                   f"Arbeit {power['work_kj']} kJ; beste 5 min {best['5min'] or '–'} W, "
                   f"20 min {best['20min'] or '–'} W")
        out += ["", f"\\* geschätzt aus Tempo, Steigung und {_num(power['model']['mass_kg'], 0)} kg "
                "Systemgewicht, ohne Wind -- für Vergleiche zwischen Fahrten, nicht als Messwert."]
    out.append("")

    if road["intervals"]:
        out += ["## Wege", ""]
        if road["class_share_rated"]:
            out.append("- Wegeklassen: " + ", ".join(f"{k} {_num(v * 100, 0)} %"
                                                     for k, v in road["class_share_rated"].items()))
        for lab in road["labels"]:
            out.append(f"- Label {lab['surface'] or '–'} Q{lab['quality'] or '–'}: {_num(lab['km'], 2)} km, "
                       f"Rauheit Median {_num(lab['roughness_median'], 2)}, Klassen 1–5: "
                       + "/".join(str(n) for n in lab["classes_1_5"]))
        sh = road["shocks"]
        out.append(f"- Stöße: {sh['count']} (Schwere 1/2/3: " + "/".join(
            str(sh["by_severity"][s]) for s in ("1", "2", "3")) + ")")
        for s in sh["top"]:
            out.append(f"  - km {_num(s['km'])}: {_num(s['g'])} g bei {_num(s['kmh'])} km/h"
                       + (" (beide Räder)" if s["both_wheels"] else ""))
        out.append("")

    out += ["## Technik", ""]
    if health["findings"]:
        icon = {"warn": "⚠", "info": "ℹ"}
        out += [f"- {icon.get(f['level'], '-')} {f['text']}" for f in health["findings"]]
    else:
        out.append("- Keine Auffälligkeiten.")
    gps = health["gps"]
    out.append(f"- GPS: {_num(gps['fresh_share'] * 100, 0)} % der Datensätze mit frischem Fix, "
               f"Genauigkeit Median {_num(gps['accuracy_m_median'])} m")
    out.append("")
    return "\n".join(out)
