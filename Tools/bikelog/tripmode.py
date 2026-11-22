"""Which parts of a phone's all-day recording were ridden by bike.

A phone logging position all day (PhoneTrack/OsmAnd, OwnTracks, ...) mixes cycling with
walking, driving and the train. For the map only the cycling counts, so the recording is cut
into trips at stops and gaps, and every trip is judged by its speeds:

  split     a gap of GAP_S without a fix, or STOP_S spent within STOP_RADIUS_M, ends a trip
  walk      median moving speed under WALK_MAX_KMH
  motor     car, bus, train: median over BIKE_MEDIAN_KMH, the fastest tenth over
            BIKE_P90_KMH, a minute held over BIKE_SUSTAINED_KMH, or a mean over
            BIKE_MEAN_KMH door to door (a cyclist waits at lights, a train does not), or
            stop-and-go: the fastest tenth over SPREAD_P90_KMH and more than SPREAD_RATIO
            times the median (a cyclist rides evenly, a car in town does not)
  bike      none of that, and a median of at least BIKE_MIN_KMH
  unclear   in between (walking with the bike, jogging): left out
  short     under MIN_TRIP_M or MIN_TRIP_S

Speeds come from the positions (point to point, fixes worse than MAX_ACCURACY_M dropped),
not from the speed field some apps write: that is missing as often as not. Thresholds were
set by eye on 229 trips of one phone and are known to be wrong in both directions
(doc/ROADMAP.md, known bugs): the percentiles count intervals between fixes (time only if
those are evenly spaced), and a trip is judged as a whole, blind to gradient and to a
change of transport inside it.
"""

from __future__ import annotations

import datetime
import io
import math
import statistics
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field

GAP_S = 600.0
STOP_S = 300.0
STOP_RADIUS_M = 100.0
MAX_ACCURACY_M = 50.0
MIN_TRIP_M = 1000.0
MIN_TRIP_S = 180.0
#: below this a segment is standing, not moving (m/s)
MOVING_MS = 1.0
WALK_MAX_KMH = 8.0
BIKE_MIN_KMH = 10.0
BIKE_MEDIAN_KMH = 32.0
BIKE_P90_KMH = 45.0
BIKE_SUSTAINED_KMH = 55.0
BIKE_MEAN_KMH = 30.0
SPREAD_P90_KMH = 38.0
SPREAD_RATIO = 2.0
SUSTAINED_S = 60.0

KINDS = {"bike": "Rad", "walk": "zu Fuß", "motor": "Auto/Bahn", "unclear": "unklar", "short": "kurz"}


@dataclass(frozen=True)
class Fix:
    t: float            # epoch seconds
    lat: float
    lon: float
    accuracy: float | None = None


@dataclass
class Trip:
    fixes: list[Fix]
    kind: str = "short"
    reason: str = ""
    stats: dict = field(default_factory=dict)

    @property
    def start(self) -> float:
        return self.fixes[0].t

    @property
    def end(self) -> float:
        return self.fixes[-1].t

    def as_dict(self) -> dict:
        return {"start": datetime.datetime.fromtimestamp(self.start, datetime.timezone.utc)
                .isoformat(timespec="seconds"),
                "end": datetime.datetime.fromtimestamp(self.end, datetime.timezone.utc)
                .isoformat(timespec="seconds"),
                "kind": self.kind, "reason": self.reason, **self.stats}


def _dist(a: Fix, b: Fix) -> float:
    k = 111320.0
    dy = (b.lat - a.lat) * k
    dx = (b.lon - a.lon) * k * math.cos(math.radians(a.lat))
    return math.hypot(dx, dy)


def _epoch(text: str | None) -> float | None:
    if not text:
        return None
    try:
        dt = datetime.datetime.fromisoformat(text.strip().replace("Z", "+00:00"))
    except ValueError:
        return None
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=datetime.timezone.utc)
    return dt.timestamp()


def _local(tag: str) -> str:
    return tag.rpartition("}")[2]


def read_fixes(data: bytes, track_prefix: str | None = None) -> list[Fix]:
    """The timed track points of a GPX. With track_prefix, only <trk> whose <name> starts
    with it (PhoneTrack puts every device of a session into one file, one trk each)."""
    root = ET.parse(io.BytesIO(data)).getroot()
    out: list[Fix] = []
    for trk in root:
        if _local(trk.tag) != "trk":
            continue
        name = next((c.text or "" for c in trk if _local(c.tag) == "name"), "")
        if track_prefix and not name.strip().startswith(track_prefix):
            continue
        for p in trk.iter():
            if _local(p.tag) != "trkpt":
                continue
            t = acc = None
            for c in p.iter():
                tag = _local(c.tag)
                if tag == "time":
                    t = _epoch(c.text)
                elif tag in ("accuracy", "hdop") and c.text:
                    try:
                        acc = float(c.text)
                    except ValueError:
                        pass
            try:
                lat, lon = float(p.get("lat")), float(p.get("lon"))
            except (TypeError, ValueError):
                continue
            if t is not None:
                out.append(Fix(t, lat, lon, acc))
    return out


def merge(*recordings: list[Fix]) -> list[Fix]:
    """One time-ordered recording from several (day files, overlapping exports): a time
    seen twice is the same fix."""
    by_time: dict[float, Fix] = {}
    for rec in recordings:
        for f in rec:
            by_time.setdefault(f.t, f)
    return [by_time[t] for t in sorted(by_time)]


def split(fixes: list[Fix]) -> list[list[Fix]]:
    """Trips between stops (STOP_S within STOP_RADIUS_M) and gaps (GAP_S). The stop itself
    belongs to neither neighbour, except its first fix, which ends the trip before it."""
    fixes = [f for f in fixes if f.accuracy is None or f.accuracy <= MAX_ACCURACY_M]
    trips: list[list[Fix]] = []
    cur: list[Fix] = []
    for f in fixes:
        if cur and f.t - cur[-1].t > GAP_S:
            trips.append(cur)
            cur = []
        cur.append(f)
        j = len(cur) - 1
        while j > 0 and _dist(cur[j - 1], f) < STOP_RADIUS_M:
            j -= 1
        if j < len(cur) - 1 and f.t - cur[j].t >= STOP_S:
            if j > 0:
                trips.append(cur[:j + 1])
            cur = [f]
    if cur:
        trips.append(cur)
    return trips


def classify(fixes: list[Fix]) -> Trip:
    trip = Trip(fixes)
    dist = sum(_dist(a, b) for a, b in zip(fixes, fixes[1:]))
    dur = fixes[-1].t - fixes[0].t if fixes else 0.0
    trip.stats = {"distance_km": round(dist / 1000, 2), "duration_min": round(dur / 60, 1)}
    if dist < MIN_TRIP_M or dur < MIN_TRIP_S or len(fixes) < 5:
        trip.kind, trip.reason = "short", "unter 1 km oder 3 min"
        return trip
    moving = [_dist(a, b) / (b.t - a.t) for a, b in zip(fixes, fixes[1:]) if 0 < b.t - a.t <= SUSTAINED_S]
    moving = [v for v in moving if v > MOVING_MS]
    if len(moving) < 3:
        trip.kind, trip.reason = "short", "kaum Bewegung"
        return trip
    median = statistics.median(moving) * 3.6
    p90 = statistics.quantiles(moving, n=10)[-1] * 3.6
    sustained = 0.0
    j = 0
    acc = [0.0]
    for a, b in zip(fixes, fixes[1:]):
        acc.append(acc[-1] + _dist(a, b))
    for i, f in enumerate(fixes):
        while f.t - fixes[j].t > SUSTAINED_S:
            j += 1
        span = f.t - fixes[j].t
        if span >= SUSTAINED_S * 0.75:
            sustained = max(sustained, (acc[i] - acc[j]) / span * 3.6)
    mean = dist / dur * 3.6
    trip.stats.update({"median_kmh": round(median, 1), "p90_kmh": round(p90, 1),
                       "sustained_kmh": round(sustained, 1), "mean_kmh": round(mean, 1)})
    if median < WALK_MAX_KMH and p90 < BIKE_MIN_KMH + 2:
        trip.kind, trip.reason = "walk", f"Median {median:.0f} km/h"
    elif median > BIKE_MEDIAN_KMH:
        trip.kind, trip.reason = "motor", f"Median {median:.0f} km/h"
    elif p90 > BIKE_P90_KMH:
        trip.kind, trip.reason = "motor", f"schnellstes Zehntel {p90:.0f} km/h"
    elif sustained > BIKE_SUSTAINED_KMH:
        trip.kind, trip.reason = "motor", f"eine Minute mit {sustained:.0f} km/h"
    elif p90 > SPREAD_P90_KMH and p90 > SPREAD_RATIO * median:
        trip.kind, trip.reason = "motor", f"Stop-and-go: Median {median:.0f}, schnellstes Zehntel {p90:.0f} km/h"
    elif mean > BIKE_MEAN_KMH:
        trip.kind, trip.reason = "motor", f"Schnitt {mean:.0f} km/h ohne Halt"
    elif median >= BIKE_MIN_KMH:
        trip.kind, trip.reason = "bike", f"Median {median:.0f} km/h, Spitze {sustained:.0f} km/h"
    else:
        trip.kind, trip.reason = "unclear", f"Median {median:.0f} km/h (Schieben, Joggen?)"
    return trip


def trips(fixes: list[Fix]) -> list[Trip]:
    """Every trip of a recording, judged."""
    return [classify(t) for t in split(fixes) if len(t) >= 2]


def to_gpx(trip: Trip, name: str) -> bytes:
    """A plain GPX of one trip (position and time), the original a map track is kept as."""
    from xml.sax.saxutils import escape
    pts = "".join(
        f'<trkpt lat="{f.lat:.7f}" lon="{f.lon:.7f}"><time>'
        f'{datetime.datetime.fromtimestamp(f.t, datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")}'
        f"</time></trkpt>" for f in trip.fixes)
    return ('<?xml version="1.0" encoding="UTF-8"?>\n<gpx xmlns="http://www.topografix.com/GPX/1/1" '
            f'version="1.1" creator="BikeLog tripmode"><trk><name>{escape(name)}</name>'
            f"<trkseg>{pts}</trkseg></trk></gpx>\n").encode()
