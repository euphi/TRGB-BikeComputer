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
straight line across a coffee stop.

Shocks (log format v2) become <wpt> waypoints -- potholes and kerbs on the
map -- subject to the same position/time checks as the track points. The
road-quality intervals stay CSV-only, for the same reason as gradient: no
extension that viewers read carries them.
"""

from __future__ import annotations

import datetime
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field

from .record import Record, ShockEvent

GPX_NS = "http://www.topografix.com/GPX/1/1"
TPX_NS = "http://www.garmin.com/xmlschemas/TrackPointExtension/v1"
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
    #: Write shocks (if given) as waypoints, from this severity (1..3) up.
    shocks: bool = True
    min_shock_severity: int = 1
    track_name: str | None = None
    track_type: str = "cycling"
    creator: str = CREATOR


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
    first_time: datetime.datetime | None = None
    last_time: datetime.datetime | None = None

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


def _iso(dt: datetime.datetime) -> str:
    return dt.strftime("%Y-%m-%dT%H:%M:%SZ")


def _trackpoint(parent: ET.Element, rec: Record, opts: GpxOptions) -> None:
    pt = ET.SubElement(parent, f"{{{GPX_NS}}}trkpt",
                       {"lat": f"{rec.latitude:.7f}", "lon": f"{rec.longitude:.7f}"})
    ele = _elevation(rec, opts)
    if ele is not None:
        ET.SubElement(pt, f"{{{GPX_NS}}}ele").text = f"{ele:.1f}"
    ET.SubElement(pt, f"{{{GPX_NS}}}time").text = _iso(rec.utc())

    # Sensor data rides along in Garmin's TrackPointExtension -- the one
    # extension Strava, Komoot and most viewers actually parse. Gradient and
    # trip distance have no element there and stay CSV-only on purpose;
    # inventing an extension nobody reads would only bloat the file.
    values: list[tuple[str, str]] = []
    if TEMPERATURE_RANGE[0] <= rec.temp <= TEMPERATURE_RANGE[1] and rec.temp != 0.0:
        values.append(("atemp", f"{rec.temp:.1f}"))
    if rec.hr:
        values.append(("hr", str(rec.hr)))
    if rec.cadence:
        values.append(("cad", str(rec.cadence)))
    speed_ms = rec.gps_speed_ms if rec.has_gps_speed else rec.speed_ms
    if speed_ms:
        values.append(("speed", f"{speed_ms:.2f}"))
    if not values:
        return
    ext = ET.SubElement(pt, f"{{{GPX_NS}}}extensions")
    tpx = ET.SubElement(ext, f"{{{TPX_NS}}}TrackPointExtension")
    for tag, text in values:
        ET.SubElement(tpx, f"{{{TPX_NS}}}{tag}").text = text


def _shock_usable(shock: ShockEvent, opts: GpxOptions) -> bool:
    if not shock.gps_valid or (shock.gps_lat_e7 == 0 and shock.gps_lon_e7 == 0):
        return False
    if opts.max_fix_age_ms and shock.gps_fix_age_ms > opts.max_fix_age_ms:
        return False
    return shock.utc().year >= opts.min_year


def _waypoint(parent: ET.Element, shock: ShockEvent) -> None:
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
    ET.SubElement(wpt, f"{{{GPX_NS}}}type").text = f"shock-{shock.severity}"


def build_tree(records, opts: GpxOptions | None = None,
               shocks: list[ShockEvent] | None = None) -> tuple[ET.ElementTree, GpxStats]:
    opts = opts or GpxOptions()
    stats = GpxStats()

    ET.register_namespace("", GPX_NS)
    ET.register_namespace("gpxtpx", TPX_NS)
    ET.register_namespace("xsi", XSI_NS)

    gpx = ET.Element(f"{{{GPX_NS}}}gpx", {
        "version": "1.1",
        "creator": opts.creator,
        f"{{{XSI_NS}}}schemaLocation":
            f"{GPX_NS} {GPX_NS}/gpx.xsd {TPX_NS} {TPX_NS}/TrackPointExtensionv1.xsd",
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
            _waypoint(gpx, shock)
            stats.shocks_written += 1
    trk = ET.SubElement(gpx, f"{{{GPX_NS}}}trk")
    ET.SubElement(trk, f"{{{GPX_NS}}}type").text = opts.track_type

    seg: ET.Element | None = None
    previous: datetime.datetime | None = None

    for rec in records:
        stats.total += 1
        try:
            _check(rec, opts)
        except _Rejected as rejected:
            setattr(stats, rejected.counter, getattr(stats, rejected.counter) + 1)
            continue
        now = rec.utc()
        # The logger samples about once a second; a repeated second would put
        # two points at the same instant, which some importers treat as a
        # corrupt track. Keep the first one.
        if previous is not None and now == previous:
            stats.dropped_duplicate_time += 1
            continue
        if seg is None or (opts.segment_gap_s and previous is not None
                           and (now - previous).total_seconds() > opts.segment_gap_s):
            seg = ET.SubElement(trk, f"{{{GPX_NS}}}trkseg")
            stats.segments += 1
        _trackpoint(seg, rec, opts)
        stats.written += 1
        if stats.first_time is None:
            stats.first_time = now
        stats.last_time = now
        previous = now

    name = opts.track_name or (
        stats.first_time.strftime("Fahrt %Y-%m-%d %H:%M") if stats.first_time else "Fahrt")
    ET.SubElement(metadata, f"{{{GPX_NS}}}name").text = name
    if stats.first_time:
        ET.SubElement(metadata, f"{{{GPX_NS}}}time").text = _iso(stats.first_time)
    # <name> must precede <type> in a GPX <trk>; insert rather than append.
    trk_name = ET.Element(f"{{{GPX_NS}}}name")
    trk_name.text = name
    trk.insert(0, trk_name)

    ET.indent(gpx, space="  ")
    return ET.ElementTree(gpx), stats


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
