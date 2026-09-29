"""Key figures of one ride, computed from the binary log alone.

Used for the GPX metadata (description + extension) and anywhere else a ride
needs a one-line summary. Deliberately independent of the device's I_*.txt:
older logs have none, and the log is the ground truth anyway.
"""

from __future__ import annotations

import bisect
from dataclasses import dataclass, field

from .record import (ROAD_CLASS_NAMES, SURFACE_NAMES, LabelRecord, Record,
                     RoadQualityRecord, ShockEvent)

#: Below this the bike counts as standing (km/h) -- the wheel sensor reports
#: small non-zero values while rolling to a stop.
MOVING_KMH = 2.0
#: A longer gap between two records is a logging pause, not riding.
MAX_STEP_S = 30.0
#: Barometric altitude noise is ~0.5 m; only climbs beyond this count.
ELEVATION_HYSTERESIS_M = 2.0

HR_VALID = range(25, 251)       # 0 = no sensor, 255 = sensor without reading
CADENCE_VALID = range(1, 250)   # 0 = not pedalling/no sensor, 255 = sensor without reading


def hr_valid(hr: int) -> bool:
    return hr in HR_VALID


def cadence_valid(cadence: int) -> bool:
    return cadence in CADENCE_VALID


@dataclass
class RideStats:
    start: int | None = None                # epoch s
    end: int | None = None
    distance_m: float = 0.0
    duration_s: float = 0.0
    moving_s: float = 0.0
    max_speed_kmh: float = 0.0
    avg_moving_kmh: float | None = None
    ascent_m: float = 0.0
    descent_m: float = 0.0
    min_ele_m: float | None = None
    max_ele_m: float | None = None
    avg_hr: int | None = None
    max_hr: int | None = None
    avg_cadence: int | None = None          # while pedalling
    min_temp: float | None = None
    max_temp: float | None = None
    max_grade: float | None = None
    min_grade: float | None = None
    shocks: dict[int, int] = field(default_factory=dict)            # severity -> count
    road_class_m: dict[int, float] = field(default_factory=dict)    # class -> metres
    label_m: dict[str, float] = field(default_factory=dict)         # "Schotter Q2" -> metres

    def describe(self) -> str:
        """German one-paragraph summary for GPX <desc>."""
        def num(value: float, digits: int = 1) -> str:
            return f"{value:.{digits}f}".replace(".", ",")

        parts = [f"{num(self.distance_m / 1000)} km"]
        if self.moving_s:
            h, rest = divmod(int(self.moving_s), 3600)
            parts.append(f"Fahrzeit {h}:{rest // 60:02d} h")
        if self.avg_moving_kmh:
            parts.append(f"Ø {num(self.avg_moving_kmh)} km/h")
        if self.max_speed_kmh:
            parts.append(f"max. {num(self.max_speed_kmh)} km/h")
        if self.ascent_m >= 1:
            parts.append(f"↑ {self.ascent_m:.0f} m ↓ {self.descent_m:.0f} m")
        if self.avg_hr:
            parts.append(f"Puls Ø {self.avg_hr} / max. {self.max_hr}")
        if self.avg_cadence:
            parts.append(f"Trittfrequenz Ø {self.avg_cadence}")
        if self.min_temp is not None:
            parts.append(f"{self.min_temp:.0f}–{self.max_temp:.0f} °C")
        if self.shocks:
            total = sum(self.shocks.values())
            parts.append(f"{total} {'Stoß' if total == 1 else 'Stöße'} (" + "/".join(
                str(self.shocks.get(s, 0)) for s in (1, 2, 3)) + ")")
        text = ", ".join(parts)
        rated = {c: m for c, m in self.road_class_m.items() if c}
        if rated:
            total = sum(rated.values())
            text += ". Wegequalität: " + ", ".join(
                f"{ROAD_CLASS_NAMES[c]} {m / total:.0%}" for c, m in sorted(rated.items()))
        if self.label_m:
            text += ". Labels: " + ", ".join(
                f"{name} {num(m / 1000)} km"
                for name, m in sorted(self.label_m.items(), key=lambda kv: -kv[1]))
        return text


def label_name(surface: int, quality: int) -> str:
    name = SURFACE_NAMES.get(surface, "") or "ohne Untergrund"
    return f"{name} Q{quality}" if quality else name


def compute(records: list[Record], road: list[RoadQualityRecord] | None = None,
            shocks: list[ShockEvent] | None = None,
            labels: list[LabelRecord] | None = None) -> RideStats:
    st = RideStats()
    if not records:
        return st
    st.start, st.end = records[0].timestamp, records[-1].timestamp
    st.duration_s = records[-1].time - records[0].time
    st.distance_m = max(0.0, records[-1].distance - records[0].distance)

    hr_sum = hr_n = cad_sum = cad_n = 0
    moving_dist = 0.0
    ele_ref: float | None = None
    prev: Record | None = None
    for rec in records:
        if rec.speed > st.max_speed_kmh and rec.speed < 150:
            st.max_speed_kmh = rec.speed
        if hr_valid(rec.hr):
            hr_sum += rec.hr
            hr_n += 1
            st.max_hr = max(st.max_hr or 0, rec.hr)
        if cadence_valid(rec.cadence):
            cad_sum += rec.cadence
            cad_n += 1
        if -50 <= rec.temp <= 80 and rec.temp != 0.0:
            st.min_temp = rec.temp if st.min_temp is None else min(st.min_temp, rec.temp)
            st.max_temp = rec.temp if st.max_temp is None else max(st.max_temp, rec.temp)
        grade = rec.gradient
        if -40 <= grade <= 40 and grade != 0.0:
            st.max_grade = grade if st.max_grade is None else max(st.max_grade, grade)
            st.min_grade = grade if st.min_grade is None else min(st.min_grade, grade)
        ele = rec.height if -500 <= rec.height <= 9000 and rec.height != 0.0 else None
        if ele is not None:
            st.min_ele_m = ele if st.min_ele_m is None else min(st.min_ele_m, ele)
            st.max_ele_m = ele if st.max_ele_m is None else max(st.max_ele_m, ele)
            if ele_ref is None:
                ele_ref = ele
            elif ele - ele_ref >= ELEVATION_HYSTERESIS_M:
                st.ascent_m += ele - ele_ref
                ele_ref = ele
            elif ele_ref - ele >= ELEVATION_HYSTERESIS_M:
                st.descent_m += ele_ref - ele
                ele_ref = ele
        if prev is not None:
            dt = rec.time - prev.time
            if 0 < dt <= MAX_STEP_S and rec.speed >= MOVING_KMH:
                st.moving_s += dt
                moving_dist += max(0.0, rec.distance - prev.distance)
        prev = rec

    if hr_n:
        st.avg_hr = round(hr_sum / hr_n)
    if cad_n:
        st.avg_cadence = round(cad_sum / cad_n)
    if st.moving_s > 0 and moving_dist > 0:
        st.avg_moving_kmh = moving_dist / st.moving_s * 3.6

    for shock in shocks or ():
        st.shocks[shock.severity] = st.shocks.get(shock.severity, 0) + 1

    road = road or []
    for rq in road:
        st.road_class_m[rq.road_class] = st.road_class_m.get(rq.road_class, 0.0) + rq.distance_m

    # Distance under each manual label, from the road-quality intervals (they
    # carry their own distance and come every 1..10 s).
    if labels and road:
        times = [lab.time for lab in labels]
        for rq in road:
            i = bisect.bisect_right(times, rq.time)
            if not i:
                continue
            lab = labels[i - 1]
            if lab.is_set:
                name = label_name(lab.surface, lab.quality)
                st.label_m[name] = st.label_m.get(name, 0.0) + rq.distance_m
    return st
