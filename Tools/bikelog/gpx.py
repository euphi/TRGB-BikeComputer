"""Turn logged records into a GPX 1.1 track.

The XML is the easy half. The half that decides whether the result is usable
is which records are allowed into it:

* A record without LOG_GPS_VALID has no position at all -- the ride was
  logged, the phone just wasn't supplying fixes.
* A record whose fix is stale (gps_fix_age_ms above the threshold) carries a
  *repeated* position: TrailBridge resends the last known fix on every
  heartbeat while waiting for a new one, e.g. in a tunnel. Logging that
  verbatim piles a cloud of points onto one spot and drags averages down.
  This is exactly what the FIX_AGE_MS tag exists for, see
  ../TrailBridge/PROTOCOL.md.
* A record whose timestamp predates the firmware itself was written before
  NTP had set the clock (the ESP32 starts at the epoch and only learns the
  time once WiFi is up, see WifiWebserver.cpp). Strava and Komoot reject or
  misfile a track dated 1970.

Gaps in time start a new <trkseg>, so map viewers draw a break instead of a
straight line across a coffee stop. So does a change of ride state
(RideStateRecord, record type 4) -- the state then rides along as an
<extensions> block on the new <trkseg> itself, not as a waypoint or a
per-point tag: FreeRide/Ride/Coast/Cruise/Stop/Break are properties of a
stretch of the ride, not of a single point (doc/design/ride-state-machine.md).

Shocks (log format v2) become <wpt> waypoints -- potholes and kerbs on the
map -- subject to the same position/time checks as the track points; so do
changes of the manual road label.

What rides along in a track point, in two layers:

* Garmin's TrackPointExtension v2 (atemp, hr, cad, speed, course) -- the one
  extension Strava, Komoot, Garmin Connect and most viewers actually parse.
* With ``rich`` (the default), everything else the log knows, in this
  project's own namespace (BC_NS): trip distance, wheel speed, gradient
  (display/baro/IMU), both altitudes, GPS accuracy and fix age, road class and
  roughness of the road-quality interval the point falls into, and the manual
  label. Viewers skip unknown extensions, so this costs file size only; it
  makes the GPX a complete, self-describing export instead of a lossy one.

The metadata carries the ride's key figures (bikelog.ridestats) as <desc>
for humans and as an extension for tools.
"""

from __future__ import annotations

import bisect
import datetime
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field

from . import ridestats
from .geo import haversine_m
from .record import (LABEL_CHANGE, RIDE_STATE_NAMES, ROAD_CLASS_NAMES, ROAD_CLASS_OSM,
                     LabelRecord, Record, RideStateRecord, RoadQualityRecord, ShockEvent,
                     label_at, labels_of, ride_state_record_at, ride_states_of, split)
from .sanitize import SanitizeOptions, trim_jitter

GPX_NS = "http://www.topografix.com/GPX/1/1"
TPX_NS = "http://www.garmin.com/xmlschemas/TrackPointExtension/v2"
#: Own namespace for everything TrackPointExtension has no element for.
BC_NS = "https://github.com/euphi/TRGB-BikeComputer/gpx/v1"
XSI_NS = "http://www.w3.org/2001/XMLSchema-instance"

CREATOR = "TRGB-BikeComputer/bikelog"

# Plausibility bounds. Deliberately generous -- these are there to catch
# garbage (unset clock, missing sensor, absurd barometric reading), not to
# second-guess real measurements.
MIN_PLAUSIBLE_YEAR = 2020
BARO_ALTITUDE_RANGE = (-500.0, 9000.0)
TEMPERATURE_RANGE = (-50.0, 80.0)


@dataclass
class GpxOptions:
    #: Drop fixes older than this. 0 disables the check.
    max_fix_age_ms: int = 5000
    #: Start a new track segment after a gap of this many seconds. 0 disables.
    segment_gap_s: float = 60.0
    #: "baro" (the pressure sensor), "gps" (the phone), or "auto" (baro when
    #: plausible, else the GPS altitude). Baro is the better sensor; GPS
    #: altitude is the fallback for devices without the I2C sensor.
    ele_source: str = "auto"
    #: Reject records timestamped before this year (unset clock).
    min_year: int = MIN_PLAUSIBLE_YEAR
    #: Drop fixes whose reported accuracy is worse than this, in meters.
    #: 0 disables -- records without an accuracy value are never dropped.
    max_accuracy_m: float = 0.0
    #: A step from the last counted point shorter than this is GPS noise
    #: (the phone's fix wandering a few metres while the bike stands still),
    #: not distance travelled -- see GpxStats.real_distance_m.
    jitter_radius_m: float = 8.0
    #: Remove the GPS-jitter cluster at the start/end of the ride and around
    #: every pause before building the track -- see bikelog.sanitize. On by
    #: default; only the tests that want to see the raw, un-trimmed points
    #: turn it off.
    sanitize: bool = True
    #: Write shocks (if given) as waypoints, from this severity (1..3) up.
    shocks: bool = True
    min_shock_severity: int = 1
    #: Write manual label changes as waypoints.
    labels: bool = True
    #: Start a new <trkseg> on every ride-state change (in addition to
    #: segment_gap_s) and tag it with the state in effect, as an <extensions>
    #: block on the <trkseg> itself -- a section property, not a waypoint.
    ride_states: bool = True
    #: Add the BC_NS extensions (per point, per waypoint, ride summary).
    rich: bool = True
    track_name: str | None = None
    track_type: str = "cycling"
    creator: str = CREATOR
    #: Optional <link> in the metadata (e.g. the session in the log service).
    link: str | None = None
    #: Device name, goes into <src> and the summary extension.
    device: str | None = None


@dataclass
class GpxStats:
    """Why records did not make it into the track -- the only feedback you
    get on a conversion, so it is returned rather than logged away."""

    total: int = 0
    written: int = 0
    segments: int = 0
    dropped_no_fix: int = 0
    dropped_stale: int = 0
    dropped_bad_time: int = 0
    dropped_null_island: int = 0
    dropped_inaccurate: int = 0
    dropped_duplicate_time: int = 0
    shocks_written: int = 0
    shocks_dropped: int = 0
    labels_written: int = 0
    first_time: datetime.datetime | None = None
    last_time: datetime.datetime | None = None
    #: Cumulative movement between the track points that made it in, with
    #: steps under jitter_radius_m ignored -- so a session spent parked
    #: somewhere, with the phone's GPS wandering around that one spot, comes
    #: out at (close to) zero rather than accumulating from the noise.
    real_distance_m: float = 0.0

    def summary(self) -> str:
        parts = [f"{self.written}/{self.total} Punkte in {self.segments} Segment(en)"]
        for label, value in (
            ("ohne Fix", self.dropped_no_fix),
            ("veraltet", self.dropped_stale),
            ("Zeit unplausibel", self.dropped_bad_time),
            ("Nullinsel", self.dropped_null_island),
            ("zu ungenau", self.dropped_inaccurate),
            ("Zeitduplikat", self.dropped_duplicate_time),
        ):
            if value:
                parts.append(f"{value} verworfen ({label})")
        if self.shocks_written or self.shocks_dropped:
            parts.append(f"{self.shocks_written} Stoß-Wegpunkt(e)"
                         + (f" ({self.shocks_dropped} ohne verwertbare Position)" if self.shocks_dropped else ""))
        if self.labels_written:
            parts.append(f"{self.labels_written} Label-Wegpunkt(e)")
        return ", ".join(parts)


class _Rejected(Exception):
    """Internal: carries the GpxStats counter name to bump."""

    def __init__(self, counter: str):
        self.counter = counter


def _check(rec: Record, opts: GpxOptions) -> None:
    if not rec.gps_valid:
        raise _Rejected("dropped_no_fix")
    if opts.max_fix_age_ms and rec.gps_fix_age_ms > opts.max_fix_age_ms:
        raise _Rejected("dropped_stale")
    # (0, 0) is in the Atlantic. The firmware zero-initialises the position,
    # so an exact null island is a missing value, not a boat trip.
    if rec.gps_lat_e7 == 0 and rec.gps_lon_e7 == 0:
        raise _Rejected("dropped_null_island")
    if rec.utc().year < opts.min_year:
        raise _Rejected("dropped_bad_time")
    if opts.max_accuracy_m and rec.has_gps_accuracy:
        if rec.gps_accuracy_m > opts.max_accuracy_m:
            raise _Rejected("dropped_inaccurate")


def _elevation(rec: Record, opts: GpxOptions) -> float | None:
    baro_ok = BARO_ALTITUDE_RANGE[0] <= rec.height <= BARO_ALTITUDE_RANGE[1]
    if opts.ele_source == "baro":
        return rec.height if baro_ok else None
    if opts.ele_source == "gps":
        return float(rec.gps_altitude_m) if rec.has_gps_altitude else None
    if baro_ok and rec.height != 0.0:
        return rec.height
    return float(rec.gps_altitude_m) if rec.has_gps_altitude else None


def default_track_name(first_time: datetime.datetime | None) -> str:
    """The name a track gets when GpxOptions.track_name is not set -- shared
    with bikelogservice/komoot.py so a merged tour is named the same way."""
    local_start = first_time.astimezone() if first_time else None
    return local_start.strftime("Fahrt %Y-%m-%d %H:%M") if local_start else "Fahrt"


def _iso(dt: datetime.datetime) -> str:
    return dt.strftime("%Y-%m-%dT%H:%M:%SZ")


def _sub(parent: ET.Element, ns: str, tag: str, text: str) -> ET.Element:
    el = ET.SubElement(parent, f"{{{ns}}}{tag}")
    el.text = text
    return el


def _fmt(value: float, digits: int) -> str:
    return f"{value:.{digits}f}"


class _RoadLookup:
    """Which road-quality interval (and label) a point in time falls into."""

    def __init__(self, road: list[RoadQualityRecord], labels: list[LabelRecord]):
        self.road = sorted(road, key=lambda rq: rq.time)
        self.ends = [rq.time for rq in self.road]
        self.labels = labels

    def interval(self, t: float) -> RoadQualityRecord | None:
        i = bisect.bisect_left(self.ends, t)
        if i < len(self.road):
            rq = self.road[i]
            if rq.time - rq.interval_ms / 1000.0 <= t:
                return rq
        return None

    def label(self, t: float) -> tuple[int, int]:
        return label_at(self.labels, t) if self.labels else (0, 0)


def _trackpoint(parent: ET.Element, rec: Record, opts: GpxOptions,
                lookup: _RoadLookup | None = None) -> None:
    pt = ET.SubElement(parent, f"{{{GPX_NS}}}trkpt",
                       {"lat": f"{rec.latitude:.7f}", "lon": f"{rec.longitude:.7f}"})
    ele = _elevation(rec, opts)
    if ele is not None:
        _sub(pt, GPX_NS, "ele", f"{ele:.1f}")
    _sub(pt, GPX_NS, "time", _iso(rec.utc()))

    # TrackPointExtension v2, in schema order: atemp, wtemp, depth, hr, cad,
    # speed, course, bearing.
    values: list[tuple[str, str]] = []
    if TEMPERATURE_RANGE[0] <= rec.temp <= TEMPERATURE_RANGE[1] and rec.temp != 0.0:
        values.append(("atemp", f"{rec.temp:.1f}"))
    if ridestats.hr_valid(rec.hr):
        values.append(("hr", str(rec.hr)))
    if rec.cadence and rec.cadence < 250:
        values.append(("cad", str(rec.cadence)))
    speed_ms = rec.gps_speed_ms if rec.has_gps_speed else rec.speed_ms
    if speed_ms:
        values.append(("speed", f"{speed_ms:.2f}"))
    if rec.has_gps_bearing:
        values.append(("course", f"{rec.gps_bearing_deg:.1f}"))

    bc: list[tuple[str, str]] = []
    if opts.rich:
        if rec.simulated:
            bc.append(("simulated", "1"))
        bc.append(("dist", _fmt(rec.distance, 1)))
        bc.append(("wheelSpeed", _fmt(rec.speed_ms, 2)))
        if rec.gradient != 0.0 or rec.grad_baro is not None or rec.grad_imu is not None:
            bc.append(("grade", _fmt(rec.gradient, 1)))
        if rec.grad_baro is not None:
            bc.append(("gradeBaro", _fmt(rec.grad_baro, 2)))
        if rec.grad_imu is not None:
            bc.append(("gradeImu", _fmt(rec.grad_imu, 2)))
        if BARO_ALTITUDE_RANGE[0] <= rec.height <= BARO_ALTITUDE_RANGE[1] and rec.height != 0.0:
            bc.append(("eleBaro", _fmt(rec.height, 1)))
        if rec.has_gps_altitude:
            bc.append(("eleGps", str(rec.gps_altitude_m)))
        if rec.has_gps_accuracy:
            bc.append(("accuracy", _fmt(rec.gps_accuracy_m, 1)))
        bc.append(("fixAge", str(rec.gps_fix_age_ms)))
        rq = lookup.interval(rec.time) if lookup else None
        road_class = rq.road_class if rq else rec.road_class
        if road_class:
            bc.append(("roadClass", str(road_class)))
            bc.append(("smoothness", ROAD_CLASS_OSM.get(road_class, "")))
        if rq is not None:
            if rq.roughness is not None:
                bc.append(("roughness", _fmt(rq.roughness, 2)))
            bc.append(("rmsVert", str(rq.rms_vert_mg)))
        if lookup:
            surface, quality = lookup.label(rec.time)
            if surface or quality:
                bc.append(("label", ridestats.label_name(surface, quality)))

    if not values and not bc:
        return
    ext = ET.SubElement(pt, f"{{{GPX_NS}}}extensions")
    if values:
        tpx = ET.SubElement(ext, f"{{{TPX_NS}}}TrackPointExtension")
        for tag, text in values:
            _sub(tpx, TPX_NS, tag, text)
    if bc:
        own = ET.SubElement(ext, f"{{{BC_NS}}}TrackPoint")
        for tag, text in bc:
            _sub(own, BC_NS, tag, text)


def _shock_usable(shock: ShockEvent, opts: GpxOptions) -> bool:
    if not shock.gps_valid or (shock.gps_lat_e7 == 0 and shock.gps_lon_e7 == 0):
        return False
    if opts.max_fix_age_ms and shock.gps_fix_age_ms > opts.max_fix_age_ms:
        return False
    return shock.utc().year >= opts.min_year


def _waypoint(parent: ET.Element, shock: ShockEvent, rich: bool = True) -> None:
    wpt = ET.SubElement(parent, f"{{{GPX_NS}}}wpt",
                        {"lat": f"{shock.latitude:.7f}", "lon": f"{shock.longitude:.7f}"})
    ET.SubElement(wpt, f"{{{GPX_NS}}}time").text = _iso(shock.utc())
    ET.SubElement(wpt, f"{{{GPX_NS}}}name").text = f"Stoß {shock.peak_g:.1f} g".replace(".", ",")
    desc = [f"Schwere {shock.severity}", f"Peak {shock.peak_g:.2f} g"]
    if shock.second_peak_mg:
        desc.append(f"2. Peak {shock.second_peak_mg / 1000:.2f} g nach {shock.second_peak_delay_ms} ms"
                    + (" (Vorder- und Hinterrad)" if shock.wheelbase_match else ""))
    if shock.speed_kmh is not None:
        desc.append(f"{shock.speed_kmh:.1f} km/h")
    ET.SubElement(wpt, f"{{{GPX_NS}}}desc").text = ", ".join(desc)
    _sub(wpt, GPX_NS, "sym", "Danger Area")
    ET.SubElement(wpt, f"{{{GPX_NS}}}type").text = f"shock-{shock.severity}"
    if rich:
        own = ET.SubElement(ET.SubElement(wpt, f"{{{GPX_NS}}}extensions"), f"{{{BC_NS}}}Shock")
        for tag, text in (
            ("severity", str(shock.severity)),
            ("peak", _fmt(shock.peak_g, 2)),
            ("peakVertMax", _fmt(shock.peak_vert_max_mg / 1000, 2)),
            ("peakVertMin", _fmt(shock.peak_vert_min_mg / 1000, 2)),
            ("peakHoriz", _fmt(shock.peak_horiz_mg / 1000, 2)),
            ("durationMs", str(shock.duration_ms)),
            ("vdv", _fmt(shock.vdv, 3)),
            ("preRms", _fmt(shock.pre_rms_mg / 1000, 3)),
            ("secondPeak", _fmt(shock.second_peak_mg / 1000, 2)),
            ("secondPeakDelayMs", str(shock.second_peak_delay_ms)),
            ("wheelbaseMatch", "1" if shock.wheelbase_match else "0"),
        ):
            _sub(own, BC_NS, tag, text)
        if shock.speed_kmh is not None:
            _sub(own, BC_NS, "speed", _fmt(shock.speed_kmh / 3.6, 2))
        if shock.label_surface or shock.label_quality:
            _sub(own, BC_NS, "label", ridestats.label_name(shock.label_surface, shock.label_quality))


def _label_usable(lab: LabelRecord, opts: GpxOptions) -> bool:
    if lab.reason != LABEL_CHANGE or not lab.gps_valid:
        return False
    if lab.gps_lat_e7 == 0 and lab.gps_lon_e7 == 0:
        return False
    if opts.max_fix_age_ms and lab.gps_fix_age_ms > opts.max_fix_age_ms:
        return False
    return lab.utc().year >= opts.min_year


def _label_waypoint(parent: ET.Element, lab: LabelRecord) -> None:
    wpt = ET.SubElement(parent, f"{{{GPX_NS}}}wpt",
                        {"lat": f"{lab.latitude:.7f}", "lon": f"{lab.longitude:.7f}"})
    _sub(wpt, GPX_NS, "time", _iso(lab.utc()))
    name = ridestats.label_name(lab.surface, lab.quality) if lab.is_set else "Label aus"
    _sub(wpt, GPX_NS, "name", name)
    if lab.prev_surface or lab.prev_quality:
        _sub(wpt, GPX_NS, "desc", "vorher %s (%s km)" % (
            ridestats.label_name(lab.prev_surface, lab.prev_quality),
            f"{lab.prev_distance_m / 1000:.2f}".replace(".", ",")))
    _sub(wpt, GPX_NS, "sym", "Flag, Blue")
    _sub(wpt, GPX_NS, "type", "label")


def _summary_extension(parent: ET.Element, stats: ridestats.RideStats, opts: GpxOptions,
                       device_summary: dict | None) -> None:
    ride = ET.SubElement(ET.SubElement(parent, f"{{{GPX_NS}}}extensions"), f"{{{BC_NS}}}Ride")
    if opts.device:
        _sub(ride, BC_NS, "device", opts.device)
    for tag, value, digits in (
        ("distance", stats.distance_m, 0),
        ("duration", stats.duration_s, 0),
        ("movingTime", stats.moving_s, 0),
        ("maxSpeed", stats.max_speed_kmh / 3.6, 2),
        ("avgMovingSpeed", (stats.avg_moving_kmh or 0) / 3.6 if stats.avg_moving_kmh else None, 2),
        ("ascent", stats.ascent_m, 0),
        ("descent", stats.descent_m, 0),
        ("minEle", stats.min_ele_m, 1),
        ("maxEle", stats.max_ele_m, 1),
        ("avgHr", stats.avg_hr, 0),
        ("maxHr", stats.max_hr, 0),
        ("avgCadence", stats.avg_cadence, 0),
        ("minTemp", stats.min_temp, 1),
        ("maxTemp", stats.max_temp, 1),
        ("maxGrade", stats.max_grade, 1),
        ("minGrade", stats.min_grade, 1),
    ):
        if value is not None:
            _sub(ride, BC_NS, tag, _fmt(value, digits))
    for severity, count in sorted(stats.shocks.items()):
        el = _sub(ride, BC_NS, "shocks", str(count))
        el.set("severity", str(severity))
    for cls, metres in sorted(stats.road_class_m.items()):
        el = _sub(ride, BC_NS, "roadClassDistance", _fmt(metres, 0))
        el.set("class", str(cls))
        el.set("name", ROAD_CLASS_NAMES.get(cls, "?"))
    for name, metres in sorted(stats.label_m.items()):
        el = _sub(ride, BC_NS, "labelDistance", _fmt(metres, 0))
        el.set("label", name)
    if device_summary:
        # The device's own I_*.txt, verbatim -- includes things the log does not
        # (time correction source, suppressed shocks, raw captures).
        dev = ET.SubElement(ride, f"{{{BC_NS}}}deviceSummary")
        for key, value in device_summary.items():
            if isinstance(key, str) and key.replace("_", "").isalnum():
                _sub(dev, BC_NS, key, str(value))


def build_tree(records, opts: GpxOptions | None = None,
               shocks: list[ShockEvent] | None = None,
               road: list[RoadQualityRecord] | None = None,
               labels: list[LabelRecord] | None = None,
               ride_states: list[RideStateRecord] | None = None,
               device_summary: dict | None = None) -> tuple[ET.ElementTree, GpxStats]:
    opts = opts or GpxOptions()
    stats = GpxStats()
    records = list(records)
    road = road or []
    labels = labels or []
    ride_states = ride_states or []

    ET.register_namespace("", GPX_NS)
    ET.register_namespace("gpxtpx", TPX_NS)
    ET.register_namespace("bc", BC_NS)
    ET.register_namespace("xsi", XSI_NS)

    gpx = ET.Element(f"{{{GPX_NS}}}gpx", {
        "version": "1.1",
        "creator": opts.creator,
        f"{{{XSI_NS}}}schemaLocation":
            f"{GPX_NS} {GPX_NS}/gpx.xsd {TPX_NS} https://www8.garmin.com/xmlschemas/TrackPointExtensionv2.xsd",
    })
    metadata = ET.SubElement(gpx, f"{{{GPX_NS}}}metadata")
    # GPX 1.1 order: metadata, wpt*, rte*, trk*
    if opts.shocks and shocks:
        for shock in shocks:
            if shock.severity < opts.min_shock_severity:
                continue
            if not _shock_usable(shock, opts):
                stats.shocks_dropped += 1
                continue
            _waypoint(gpx, shock, opts.rich)
            stats.shocks_written += 1
    if opts.labels:
        for lab in labels:
            if _label_usable(lab, opts):
                _label_waypoint(gpx, lab)
                stats.labels_written += 1
    trk = ET.SubElement(gpx, f"{{{GPX_NS}}}trk")
    ET.SubElement(trk, f"{{{GPX_NS}}}type").text = opts.track_type

    lookup = _RoadLookup(road, labels) if (opts.rich and (road or labels)) else None
    use_ride_states = bool(opts.ride_states and ride_states)
    seg: ET.Element | None = None
    current_state: int | None = None
    previous: datetime.datetime | None = None
    lat = [90.0, -90.0]
    lon = [180.0, -180.0]
    anchor: tuple[float, float] | None = None      # for real_distance_m, see below

    # Which records are even GPX-worthy -- tallied here so dropped_* stays a
    # count of what _check() rejected, not of what sanitize() later trims off
    # an already-accepted stream.
    kept: list[Record] = []
    for rec in records:
        stats.total += 1
        try:
            _check(rec, opts)
        except _Rejected as rejected:
            setattr(stats, rejected.counter, getattr(stats, rejected.counter) + 1)
            continue
        kept.append(rec)

    if opts.sanitize:
        kept = trim_jitter(kept, SanitizeOptions())

    for rec in kept:
        now = rec.utc()
        # The logger samples about once a second; a repeated second would put
        # two points at the same instant, which some importers treat as a
        # corrupt track. Keep the first one.
        if previous is not None and now == previous:
            stats.dropped_duplicate_time += 1
            continue
        state_rec = ride_state_record_at(ride_states, rec.time) if use_ride_states else None
        state = state_rec.state if state_rec else None
        if (seg is None
                or (opts.segment_gap_s and previous is not None
                    and (now - previous).total_seconds() > opts.segment_gap_s)
                or (use_ride_states and state != current_state)):
            seg = ET.SubElement(trk, f"{{{GPX_NS}}}trkseg")
            stats.segments += 1
            if use_ride_states and state is not None:
                own = ET.SubElement(ET.SubElement(seg, f"{{{GPX_NS}}}extensions"),
                                    f"{{{BC_NS}}}RideState")
                _sub(own, BC_NS, "state", str(state))
                _sub(own, BC_NS, "name", RIDE_STATE_NAMES.get(state, "?"))
                if state_rec.simulated:
                    _sub(own, BC_NS, "simulated", "1")
            current_state = state
        _trackpoint(seg, rec, opts, lookup)
        lat = [min(lat[0], rec.latitude), max(lat[1], rec.latitude)]
        lon = [min(lon[0], rec.longitude), max(lon[1], rec.longitude)]
        if anchor is None:
            anchor = (rec.latitude, rec.longitude)
        else:
            step_m = haversine_m(anchor[0], anchor[1], rec.latitude, rec.longitude)
            if step_m >= opts.jitter_radius_m:
                stats.real_distance_m += step_m
                anchor = (rec.latitude, rec.longitude)
        stats.written += 1
        if stats.first_time is None:
            stats.first_time = now
        stats.last_time = now
        previous = now

    ride = ridestats.compute(records, road, shocks, labels)
    name = opts.track_name or default_track_name(stats.first_time)
    description = ride.describe()
    # Sensor simulator (simulator build): speed/cadence/HR are not from a ride
    simulated = any(r.simulated for r in records)
    if simulated:
        name = "[SIM] " + name
        description = "SIMULIERT (Sensor-Simulator) -- " + description

    # metadata, in schema order: name, desc, author, copyright, link, time,
    # keywords, bounds, extensions
    _sub(metadata, GPX_NS, "name", name)
    _sub(metadata, GPX_NS, "desc", description)
    if opts.link:
        link = ET.SubElement(metadata, f"{{{GPX_NS}}}link", {"href": opts.link})
        _sub(link, GPX_NS, "text", "BikeLog")
    if stats.first_time:
        _sub(metadata, GPX_NS, "time", _iso(stats.first_time))
    _sub(metadata, GPX_NS, "keywords", ", ".join(
        k for k in ("Fahrrad", opts.track_type, opts.device, "simuliert" if simulated else None) if k))
    if stats.written:
        ET.SubElement(metadata, f"{{{GPX_NS}}}bounds", {
            "minlat": f"{lat[0]:.7f}", "minlon": f"{lon[0]:.7f}",
            "maxlat": f"{lat[1]:.7f}", "maxlon": f"{lon[1]:.7f}"})
    if opts.rich:
        _summary_extension(metadata, ride, opts, device_summary)

    # trk, in schema order: name, cmt, desc, src, link, number, type, ...
    head = [(GPX_NS, "name", name), (GPX_NS, "desc", description),
            (GPX_NS, "src", "TRGB-BikeComputer" + (f" ({opts.device})" if opts.device else ""))]
    for i, (ns, tag, text) in enumerate(head):
        el = ET.Element(f"{{{ns}}}{tag}")
        el.text = text
        trk.insert(i, el)

    ET.indent(gpx, space="  ")
    return ET.ElementTree(gpx), stats


def from_records(everything, opts: GpxOptions | None = None,
                 device_summary: dict | None = None) -> tuple[str, GpxStats]:
    """GPX from a log's full record stream (read_stream(..., types=None)):
    ride data, road quality, shocks and labels are sorted out here."""
    everything = list(everything)
    records, road, shocks = split(everything)
    tree, stats = build_tree(records, opts, shocks, road, labels_of(everything),
                             ride_states_of(everything), device_summary)
    xml = ET.tostring(tree.getroot(), encoding="unicode", xml_declaration=True)
    return xml + "\n", stats


def to_string(records, opts: GpxOptions | None = None,
              shocks: list[ShockEvent] | None = None) -> tuple[str, GpxStats]:
    tree, stats = build_tree(records, opts, shocks)
    xml = ET.tostring(tree.getroot(), encoding="unicode", xml_declaration=True)
    return xml + "\n", stats


def write(path, records, opts: GpxOptions | None = None,
          shocks: list[ShockEvent] | None = None) -> GpxStats:
    xml, stats = to_string(records, opts, shocks)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(xml)
    return stats
