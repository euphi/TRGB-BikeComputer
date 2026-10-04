"""Training analysis across rides: load, weeks, target events, recurring climbs.

Works on session reports (bikelog.report, the JSON), never on the binary log --
everything here would work the same with rides from another source turned into
the same JSON. Pure functions, no I/O apart from the explicit load/save helpers.

    Ride.from_report(report)        one ride, or None for what is not training
                                    (simulator, shorter than MIN_RIDE_KM)
    load_series(rides, start, end)  daily TRIMP -> fitness (CTL, 42 d), fatigue
                                    (ATL, 7 d), form (TSB = CTL - ATL of the day before)
    weeks(rides, today, n)          per ISO week: rides, km, hours, ascent, TRIMP, zones
    Event / event_profile()         a target event and the climbs of its GPX course
    readiness(event, rides, today)  where the rider stands against that event
    recurring_climbs(rides)         the same climb ridden several times, best efforts

The load model is Banister's impulse-response model with TRIMP as the impulse, in
the exponentially weighted form most tools use (TrainingPeaks' PMC, Golden
Cheetah): CTL_t = CTL_t-1 + (TRIMP_t - CTL_t-1) / 42, ATL the same with 7 days.
Rides without heart rate have no TRIMP and count as 0 -- readiness() says how
many there were, so a gap in the chart is not mistaken for rest.
"""

from __future__ import annotations

import datetime
import json
import uuid
import xml.etree.ElementTree as ET
from dataclasses import asdict, dataclass, field
from pathlib import Path

from . import report, ridestats
from .geo import haversine_m

#: Shorter sessions are tests or parking, not training (same as the GPX export's Tours/).
MIN_RIDE_KM = 1.0
CTL_DAYS = 42
ATL_DAYS = 7
#: Two climbs are the same climb if foot and summit are this close.
SAME_CLIMB_M = 150.0
#: readiness() looks this far back for the longest ride / biggest week.
READINESS_DAYS = 42
#: ... and this far back for climbing speed (VAM).
VAM_DAYS = 90


@dataclass
class Ride:
    session_id: int | None
    start: datetime.datetime            # aware, UTC
    day: datetime.date                  # local date of the start
    distance_km: float
    moving_s: float
    ascent_m: float
    trimp: float | None
    zones_s: dict | None
    avg_hr: int | None
    normalized_w: float | None
    best_w: dict
    decoupling_pct: float | None
    efficiency: float | None
    climbs: list
    #: all sessions of the ride (several when the bike computer rebooted on the way)
    session_ids: list = field(default_factory=list)

    @classmethod
    def from_report(cls, rep: dict, session_id: int | None = None,
                    allow_test: bool = False) -> "Ride | None":
        """None for what is not training: empty, too short, or a test session
        (simulator, GPS playback) -- unless ``allow_test`` (the rider said it was real)."""
        if rep.get("empty"):
            return None
        if not allow_test and ((rep["meta"].get("test") or {}).get("kind") or rep["meta"]["simulated"]):
            return None
        ride = rep["ride"]
        if (ride["distance_km"] or 0) < MIN_RIDE_KM:
            return None
        start = datetime.datetime.fromisoformat(rep["meta"]["start_utc"])
        heart, power = rep["heart"], rep["power"]
        return cls(
            session_id=session_id, start=start, day=start.astimezone().date(),
            distance_km=ride["distance_km"], moving_s=ride["moving_s"] or 0,
            ascent_m=ride["ascent_m"] or 0, trimp=heart.get("trimp"), zones_s=heart.get("zones_s"),
            avg_hr=heart.get("avg"), normalized_w=power.get("normalized_w"),
            best_w=power.get("best_w") or {}, decoupling_pct=heart.get("decoupling_pct"),
            efficiency=heart.get("efficiency_w_per_bpm"), climbs=rep.get("climbs") or [])


# --- load ----------------------------------------------------------------------

@dataclass
class LoadPoint:
    day: datetime.date
    trimp: float
    ctl: float              # fitness
    atl: float              # fatigue
    tsb: float              # form: yesterday's CTL - ATL


def load_series(rides: list[Ride], start: datetime.date, end: datetime.date) -> list[LoadPoint]:
    """One point per day from start to end. The model runs from the first ride
    on (CTL and ATL start at 0 there), so the window can start later."""
    daily: dict[datetime.date, float] = {}
    for r in rides:
        daily[r.day] = daily.get(r.day, 0.0) + (r.trimp or 0.0)
    first = min(daily, default=start)
    day = min(first, start)
    ctl = atl = 0.0
    out = []
    while day <= end:
        tsb = ctl - atl
        load = daily.get(day, 0.0)
        ctl += (load - ctl) / CTL_DAYS
        atl += (load - atl) / ATL_DAYS
        if day >= start:
            out.append(LoadPoint(day, round(load, 1), round(ctl, 1), round(atl, 1), round(tsb, 1)))
        day += datetime.timedelta(days=1)
    return out


# --- weeks ---------------------------------------------------------------------

@dataclass
class Week:
    monday: datetime.date
    rides: int = 0
    distance_km: float = 0.0
    moving_s: float = 0.0
    ascent_m: float = 0.0
    trimp: float = 0.0
    rides_without_hr: int = 0
    zones_s: dict = field(default_factory=dict)
    best_20min_w: float | None = None
    longest_km: float = 0.0


def monday_of(day: datetime.date) -> datetime.date:
    return day - datetime.timedelta(days=day.weekday())


def weeks(rides: list[Ride], today: datetime.date, n: int = 16) -> list[Week]:
    """The last n ISO weeks up to and including today's, oldest first, empty ones included."""
    last = monday_of(today)
    out = {last - datetime.timedelta(weeks=i): None for i in range(n)}
    table = {m: Week(m) for m in out}
    for r in rides:
        w = table.get(monday_of(r.day))
        if w is None:
            continue
        w.rides += 1
        w.distance_km += r.distance_km
        w.moving_s += r.moving_s
        w.ascent_m += r.ascent_m
        w.longest_km = max(w.longest_km, r.distance_km)
        if r.trimp is None:
            w.rides_without_hr += 1
        else:
            w.trimp += r.trimp
        for z, s in (r.zones_s or {}).items():
            w.zones_s[z] = w.zones_s.get(z, 0) + s
        b20 = r.best_w.get("20min")
        if b20 and (w.best_20min_w is None or b20 > w.best_20min_w):
            w.best_20min_w = b20
    return [table[m] for m in sorted(table)]


# --- target events -------------------------------------------------------------

@dataclass
class Event:
    name: str
    date: datetime.date
    id: str = field(default_factory=lambda: uuid.uuid4().hex[:8])
    #: A = the goal of the season, B = important, C = training race
    priority: str = "A"
    distance_km: float | None = None
    ascent_m: float | None = None
    notes: str = ""
    #: file name of the course GPX next to events.json, if uploaded
    gpx: str | None = None
    #: event_profile() of that GPX, kept so pages need not parse it again
    profile: dict | None = None

    def as_dict(self) -> dict:
        data = asdict(self)
        data["date"] = self.date.isoformat()
        return data

    @classmethod
    def from_dict(cls, data: dict) -> "Event":
        data = dict(data)
        data["date"] = datetime.date.fromisoformat(data["date"])
        return cls(**data)

    @property
    def course_km(self) -> float | None:
        return self.distance_km or (self.profile or {}).get("distance_km")

    @property
    def course_ascent_m(self) -> float | None:
        return self.ascent_m or (self.profile or {}).get("ascent_m")


def load_events(path: Path) -> list[Event]:
    if not path.exists():
        return []
    return sorted((Event.from_dict(e) for e in json.loads(path.read_text(encoding="utf-8"))),
                  key=lambda e: e.date)


def save_events(path: Path, events: list[Event]) -> None:
    tmp = path.with_name(path.name + ".part")
    tmp.write_text(json.dumps([e.as_dict() for e in sorted(events, key=lambda e: e.date)],
                              ensure_ascii=False, indent=2), encoding="utf-8")
    tmp.replace(path)


def _gpx_points(data: bytes) -> list[tuple[float, float, float | None]]:
    """(lat, lon, ele) of a GPX's track points -- route points if it has no track."""
    root = ET.fromstring(data)
    pts = []
    for tag in ("trkpt", "rtept"):
        for el in root.iter():
            if el.tag.rsplit("}", 1)[-1] != tag:
                continue
            ele = next((c.text for c in el if c.tag.rsplit("}", 1)[-1] == "ele"), None)
            pts.append((float(el.get("lat")), float(el.get("lon")),
                        float(ele) if ele not in (None, "") else None))
        if pts:
            break
    return pts


def event_profile(gpx: bytes) -> dict:
    """Distance, ascent and the rated climbs of a course GPX, with the same climb
    criteria as the ride report. Raises ValueError without elevations."""
    pts = _gpx_points(gpx)
    if len(pts) < 2:
        raise ValueError("no track or route points in this GPX")
    track: list[tuple[float, float]] = []       # (distance m, ele m)
    dist = 0.0
    prev = None
    for lat, lon, ele in pts:
        if prev is not None:
            dist += haversine_m(prev[0], prev[1], lat, lon)
        prev = (lat, lon)
        if ele is not None and (not track or dist > track[-1][0]):
            track.append((dist, ele))
    if len(track) < 2:
        raise ValueError("GPX has no elevations (<ele>) -- export it with heights")
    grid = []
    k = 0
    x = 0.0
    while x <= track[-1][0]:
        while track[k + 1][0] < x:
            k += 1
        (d1, h1), (d2, h2) = track[k], track[k + 1]
        grid.append((x, h1 + (x - d1) / (d2 - d1) * (h2 - h1), 0.0))
        x += report.CLIMB_GRID_M
    ascent = 0.0
    ref = grid[0][1]
    for _, h, _ in grid:
        if h - ref >= ridestats.ELEVATION_HYSTERESIS_M:
            ascent += h - ref
            ref = h
        elif ref - h >= ridestats.ELEVATION_HYSTERESIS_M:
            ref = h
    climbs = []
    for foot, summit in report.find_climbs(grid):
        length = grid[summit][0] - grid[foot][0]
        gain = grid[summit][1] - grid[foot][1]
        cat = report.climb_category(length, gain)
        if cat:
            climbs.append({"category": cat, "start_km": round(grid[foot][0] / 1000, 2),
                           "length_m": round(length), "gain_m": round(gain),
                           "avg_grade_pct": round(gain / length * 100, 1)})
    step = max(1, len(grid) // 300)
    return {
        "distance_km": round(dist / 1000, 1),
        "ascent_m": round(ascent),
        "min_ele_m": round(min(h for _, h, _ in grid)),
        "max_ele_m": round(max(h for _, h, _ in grid)),
        "climbs": climbs,
        "profile": [[round(d / 1000, 3), round(h, 1)] for d, h, _ in grid[::step]],
    }


#: (days before the event, phase, what it means) -- the usual periodisation,
#: coarse on purpose. The plan itself is the rider's (or the LLM's) business.
PHASES = (
    (0, "Vorbei", "Das Rennen liegt hinter dir."),
    (8, "Tapering", "Umfang deutlich runter, kurze Intensität erhalten, ausgeruht an den Start."),
    (22, "Spitze", "Rennspezifisch: Belastungen wie im Rennen (Länge, Anstiege, Untergrund), "
                   "danach genug Erholung."),
    (85, "Aufbau", "Umfang halten, Intensität steigern: Schwellen- und Bergintervalle, lange "
                   "Fahrten mit Renntempo-Abschnitten."),
    (10 ** 6, "Grundlage", "Umfang aufbauen, überwiegend ruhig (Zone 1–2), Kraft und Technik."),
)


def phase(days_left: int) -> tuple[str, str]:
    if days_left < 0:
        return PHASES[0][1], PHASES[0][2]
    for limit, name, text in PHASES[1:]:
        if days_left < limit:
            return name, text
    return PHASES[-1][1], PHASES[-1][2]


def readiness(event: Event, rides: list[Ride], today: datetime.date) -> dict:
    """The rider's recent riding held against the event: plain comparisons,
    no prediction. Climb times are estimated from the best VAM on comparable
    climbs (at least half the height) of the last VAM_DAYS."""
    days_left = (event.date - today).days
    name, text = phase(days_left)
    since = today - datetime.timedelta(days=READINESS_DAYS)
    recent = [r for r in rides if since <= r.day <= today]
    longest = max((r.distance_km for r in recent), default=0.0)
    week_asc = max((w.ascent_m for w in weeks(rides, today, READINESS_DAYS // 7)), default=0.0)
    load = load_series(rides, today, today)
    vam_since = today - datetime.timedelta(days=VAM_DAYS)
    efforts = [c for r in rides if vam_since <= r.day <= today for c in r.climbs
               if c.get("vam_m_h") and c.get("category")]
    estimates = []
    for c in (event.profile or {}).get("climbs", []):
        comparable = [e for e in efforts if e["gain_m"] >= c["gain_m"] * 0.5]
        best = max((e["vam_m_h"] for e in comparable), default=None)
        estimates.append({**c, "vam_m_h": best,
                          "est_duration_s": round(c["gain_m"] / best * 3600) if best else None,
                          "based_on": len(comparable)})
    km, asc = event.course_km, event.course_ascent_m
    return {
        "days_left": days_left,
        "weeks_left": round(days_left / 7, 1),
        "phase": name,
        "phase_text": text,
        "rides_recent": len(recent),
        "rides_recent_without_hr": sum(1 for r in recent if r.trimp is None),
        "longest_km": round(longest, 1),
        "longest_share": round(longest / km, 2) if km else None,
        "max_week_ascent_m": round(week_asc),
        "week_ascent_share": round(week_asc / asc, 2) if asc else None,
        "ctl": load[-1].ctl if load else 0.0,
        "tsb": load[-1].tsb if load else 0.0,
        "climbs": estimates,
    }


# --- recurring climbs ------------------------------------------------------------

def _near(a, b) -> bool:
    return bool(a and b) and haversine_m(a[0], a[1], b[0], b[1]) <= SAME_CLIMB_M


def recurring_climbs(rides: list[Ride], min_efforts: int = 2) -> list[dict]:
    """Climbs ridden at least min_efforts times (foot and summit within
    SAME_CLIMB_M), each with its efforts, fastest first; climbs sorted by height."""
    groups: list[dict] = []
    for r in sorted(rides, key=lambda r: r.start):
        for c in r.climbs:
            if not (c.get("foot") and c.get("summit") and c.get("duration_s")):
                continue
            effort = {"day": r.day.isoformat(), "session_id": r.session_id,
                      "duration_s": c["duration_s"], "vam_m_h": c.get("vam_m_h"),
                      "avg_hr": c.get("avg_hr"), "est_avg_power_w": c.get("est_avg_power_w")}
            group = next((g for g in groups if _near(g["foot"], c["foot"])
                          and _near(g["summit"], c["summit"])), None)
            if group is None:
                group = {"foot": c["foot"], "summit": c["summit"], "category": c.get("category"),
                         "length_m": c["length_m"], "gain_m": c["gain_m"],
                         "avg_grade_pct": c["avg_grade_pct"], "efforts": []}
                groups.append(group)
            group["efforts"].append(effort)
    out = [g for g in groups if len(g["efforts"]) >= min_efforts]
    for g in out:
        g["efforts"].sort(key=lambda e: e["duration_s"])
    return sorted(out, key=lambda g: -g["gain_m"])

