"""Build binary log files without a bike computer.

There is no real recorded data yet, and waiting for a ride to test a
converter is the wrong way round. Two generators:

``synthetic()``  an artificial ride that deliberately contains the awkward
                 cases -- a stretch before the first GPS fix, stale
                 heartbeat fixes (tunnel), a pause long enough to split the
                 track, records whose clock was not yet set by NTP.

``from_gpx()``   a real track exported from Komoot/Strava turned back into
                 a .bin. Gives realistic geometry and elevation for testing
                 GPX round-trips and, later, the heatmap -- without needing
                 the hardware.
"""

from __future__ import annotations

import datetime
import math
import xml.etree.ElementTree as ET

from .record import (CURRENT_VERSION, IF_GPS_VALID, IF_NO_SPEED, IF_TOO_SLOW,
                     IF_UNCALIBRATED, LOG_GPS_HAS_ACCURACY, LOG_GPS_HAS_ALTITUDE,
                     LOG_GPS_HAS_BEARING, LOG_GPS_HAS_SPEED, LOG_GPS_VALID,
                     SF_GPS_VALID, SF_WHEELBASE_MATCH, U16_INVALID, Record,
                     RoadQualityRecord, ShockEvent, write_records)

GPX_NS = "http://www.topografix.com/GPX/1/1"
TPX_NS = "http://www.garmin.com/xmlschemas/TrackPointExtension/v1"

EARTH_RADIUS_M = 6371000.0


def haversine_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp = p2 - p1
    dl = math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * EARTH_RADIUS_M * math.asin(math.sqrt(a))


def bearing_deg(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dl = math.radians(lon2 - lon1)
    y = math.sin(dl) * math.cos(p2)
    x = math.cos(p1) * math.sin(p2) - math.sin(p1) * math.cos(p2) * math.cos(dl)
    return (math.degrees(math.atan2(y, x)) + 360.0) % 360.0


def _gps_record(rec: Record, lat: float, lon: float, altitude_m: int | None,
                speed_ms: float | None, bearing: float | None,
                accuracy_m: float | None, fix_age_ms: int) -> Record:
    flags = LOG_GPS_VALID
    rec.gps_lat_e7 = round(lat * 1e7)
    rec.gps_lon_e7 = round(lon * 1e7)
    rec.gps_fix_age_ms = fix_age_ms
    if altitude_m is not None:
        flags |= LOG_GPS_HAS_ALTITUDE
        rec.gps_altitude_m = int(altitude_m)
    if speed_ms is not None:
        flags |= LOG_GPS_HAS_SPEED
        rec.gps_speed_cms = max(0, round(speed_ms * 100))
    if bearing is not None:
        flags |= LOG_GPS_HAS_BEARING
        rec.gps_bearing_deg_x100 = round(bearing * 100) % 36000
    if accuracy_m is not None:
        flags |= LOG_GPS_HAS_ACCURACY
        rec.gps_accuracy_m_x10 = min(65535, round(accuracy_m * 10))
    rec.gps_flags = flags
    return rec


def synthetic(start_time: int | None = None, seconds: int = 420,
              start_lat: float = 52.4000, start_lon: float = 8.7000,
              pause: tuple[int, int] | None = (120, 300),
              tunnel: tuple[int, int] | None = (60, 90),
              no_fix_start_s: int = 20,
              unset_clock_records: int = 0) -> list[Record]:
    """An artificial 1 Hz ride containing the cases the exporter must handle.

    ``pause`` and ``tunnel`` are (from_s, to_s) windows relative to the ride
    start, or None. During the pause nothing is logged at all -- that is what
    a real stop looks like once the device sleeps, and it leaves the time gap
    the exporter is supposed to turn into a second track segment.
    """
    if start_time is None:
        start_time = int(datetime.datetime(2026, 5, 17, 9, 0,
                                           tzinfo=datetime.timezone.utc).timestamp())
    records: list[Record] = []
    lat, lon = start_lat, start_lon
    distance = 0.0
    height = 95.0
    tunnel_fix: tuple[float, float, int] | None = None

    for i in range(unset_clock_records):
        # Before NTP the ESP32 clock sits near the epoch. A boot without WiFi
        # puts a handful of such records in front of the real ride -- with a
        # perfectly good position, because the phone's GPS does not depend on
        # the ESP32 knowing what year it is. Exactly the case that produces a
        # track dated 1970 if the exporter does not check.
        stale_clock = Record(timestamp=i, temp=18.0, height=height)
        _gps_record(stale_clock, start_lat, start_lon, int(height), 0.0, None, 8.0, 400)
        records.append(stale_clock)

    for i in range(seconds):
        if pause and pause[0] <= i < pause[1]:
            continue                        # stopped: nothing is logged

        speed_kmh = 24.0 + 6.0 * math.sin(i / 25.0)
        step_m = speed_kmh / 3.6
        distance += step_m
        # Heading roughly north-east; close enough for a fixture.
        lat += step_m * 0.7 / 111320.0
        lon += step_m * 0.7 / (111320.0 * math.cos(math.radians(lat)))
        height += math.sin(i / 60.0) * 0.4

        rec = Record(
            timestamp=start_time + i,
            speed=speed_kmh,
            temp=17.5 + math.sin(i / 90.0),
            gradient=math.sin(i / 60.0) * 3.0,
            height=height,
            distance=distance,
            hr=132 + int(10 * math.sin(i / 40.0)),
            cadence=78 + int(6 * math.sin(i / 30.0)),
        )

        if i < no_fix_start_s:
            pass                            # GPS still searching: no position
        elif tunnel and tunnel[0] <= i < tunnel[1]:
            # The phone resends the last fix it had on every heartbeat; only
            # the age grows. Without the age check these all pile onto one
            # spot, which is the whole point of FIX_AGE_MS.
            if tunnel_fix is None:
                tunnel_fix = (lat, lon, int(height))
            _gps_record(rec, tunnel_fix[0], tunnel_fix[1], tunnel_fix[2],
                        0.0, None, 35.0, (i - tunnel[0] + 1) * 1000)
        else:
            tunnel_fix = None
            _gps_record(rec, lat, lon, int(height), step_m, 45.0, 4.5, 350)

        records.append(rec)

    return records


def _text(node, path: str, ns: dict) -> str | None:
    found = node.find(path, ns)
    return found.text if found is not None and found.text else None


def from_gpx(path) -> list[Record]:
    """Rebuild log records from a GPX track (Komoot/Strava export).

    Fields the GPX does not carry are derived where that is honest
    (distance and speed from the geometry, gradient from elevation) and left
    at zero where it is not (heart rate and cadence without a
    TrackPointExtension).
    """
    tree = ET.parse(path)
    ns = {"gpx": GPX_NS, "tpx": TPX_NS}
    records: list[Record] = []
    previous = None
    distance = 0.0

    for pt in tree.getroot().iterfind(".//gpx:trkpt", ns):
        lat = float(pt.get("lat"))
        lon = float(pt.get("lon"))
        ele_text = _text(pt, "gpx:ele", ns)
        ele = float(ele_text) if ele_text else 0.0
        time_text = _text(pt, "gpx:time", ns)
        if not time_text:
            raise ValueError("trkpt without <time> -- cannot build a timed log from this GPX")
        when = datetime.datetime.fromisoformat(time_text.replace("Z", "+00:00"))
        timestamp = int(when.timestamp())

        step_m = 0.0
        step_s = 0.0
        bearing = None
        gradient = 0.0
        if previous is not None:
            step_m = haversine_m(previous[0], previous[1], lat, lon)
            step_s = max(0.0, timestamp - previous[3])
            bearing = bearing_deg(previous[0], previous[1], lat, lon)
            if step_m > 0.5:
                gradient = (ele - previous[2]) / step_m * 100.0
        distance += step_m

        hr_text = _text(pt, "gpx:extensions/tpx:TrackPointExtension/tpx:hr", ns)
        cad_text = _text(pt, "gpx:extensions/tpx:TrackPointExtension/tpx:cad", ns)
        temp_text = _text(pt, "gpx:extensions/tpx:TrackPointExtension/tpx:atemp", ns)
        speed_text = _text(pt, "gpx:extensions/tpx:TrackPointExtension/tpx:speed", ns)

        speed_ms = float(speed_text) if speed_text else (step_m / step_s if step_s else 0.0)
        rec = Record(
            timestamp=timestamp,
            speed=speed_ms * 3.6,
            temp=float(temp_text) if temp_text else 18.0,
            gradient=max(-30.0, min(30.0, gradient)),
            height=ele,
            distance=distance,
            hr=int(hr_text) if hr_text else 0,
            cadence=int(cad_text) if cad_text else 0,
        )
        _gps_record(rec, lat, lon, int(round(ele)), speed_ms, bearing, 4.0, 400)
        records.append(rec)
        previous = (lat, lon, ele, timestamp)

    return records


def _rough_section(i: int) -> bool:
    """Every 100 s of ride, 30 s of cobbles."""
    return i % 100 >= 70


def with_road_quality(records: list[Record], interval_s: int = 2,
                      shocks: tuple[tuple[int, float, bool], ...] = ((150, 6.2, True), (330, 4.1, False)),
                      baseline_mg: float = 150.0) -> list:
    """Interleave road-quality intervals and shocks into a ride (format v2).

    ``records`` is a 1 Hz ride as from synthetic(); intervals follow its
    timestamps, rough 30 s sections alternate with smooth road. ``shocks``
    are (ride second, peak g, wheelbase match) -- a pothole crossed by both
    wheels, or a single knock. The result is in time order, as the firmware
    writes it.
    """
    out: list = []
    current_class = 0                   # what the firmware puts into the data record: the latest class
    for i, rec in enumerate(records):
        rec.road_class = current_class
        out.append(rec)
        if i % interval_s == interval_s - 1:
            rough = _rough_section(i)
            rms = 620.0 if rough else 150.0
            speed = rec.speed
            flags = IF_UNCALIBRATED
            roughness = None
            road_class = 0
            if speed < 0.5:
                flags |= IF_NO_SPEED
            elif speed < 6:
                flags |= IF_TOO_SLOW
            else:
                roughness = rms / (baseline_mg * (speed / 20.0) ** 0.8)
                road_class = next((c + 1 for c, thr in enumerate((1.5, 2.5, 4.0, 7.0)) if roughness < thr), 5)
            rq = RoadQualityRecord(
                timestamp=rec.timestamp, timestamp_ms=500, interval_ms=interval_s * 1000,
                sample_count=interval_s * 400,
                speed_cms=round(speed / 3.6 * 100) if speed else U16_INVALID,
                rms_vert_mg=round(rms), rms_horiz_mg=round(rms * 0.45),
                peak_vert_max_mg=round(rms * 3.1), peak_vert_min_mg=-round(rms * 2.9),
                peak_total_mg=round(rms * 3.4),
                roughness_x100=U16_INVALID if roughness is None else round(roughness * 100),
                road_class=road_class, flags=flags | (IF_GPS_VALID if rec.gps_valid else 0),
                vdv_vert=rms / 1000 * 9.81 * interval_s ** 0.25,
                distance_m=speed / 3.6 * interval_s,
                count_over_t1=6 if rough else 0, count_over_t2=1 if rough else 0,
                gps_lat_e7=rec.gps_lat_e7, gps_lon_e7=rec.gps_lon_e7,
                gps_fix_age_ms=rec.gps_fix_age_ms, gps_accuracy_m_x10=rec.gps_accuracy_m_x10,
            )
            out.append(rq)
            current_class = road_class
    seq = 0
    for second, peak_g, pair in shocks:
        if second >= len(records):
            continue
        rec = records[second]
        seq += 1
        speed = rec.speed
        shock = ShockEvent(
            timestamp=rec.timestamp, timestamp_ms=250, duration_ms=9,
            peak_total_mg=round(peak_g * 1000), peak_vert_max_mg=round(peak_g * 950),
            peak_vert_min_mg=-round(peak_g * 400), peak_horiz_mg=round(peak_g * 150),
            pre_rms_mg=150, speed_cms=round(speed / 3.6 * 100),
            second_peak_mg=round(peak_g * 500) if pair else 0,
            second_peak_delay_ms=round(1.05 / (speed / 3.6) * 1000) if pair else 0,
            severity=3 if peak_g >= 8 else (2 if peak_g >= 5 else 1),
            flags=(SF_WHEELBASE_MATCH if pair else 0) | (SF_GPS_VALID if rec.gps_valid else 0),
            vdv=0.4, samples_over_threshold=3, threshold_mg=3000,
            gps_lat_e7=rec.gps_lat_e7, gps_lon_e7=rec.gps_lon_e7,
            gps_fix_age_ms=rec.gps_fix_age_ms, gps_accuracy_m_x10=rec.gps_accuracy_m_x10,
            event_seq=seq,
        )
        # right after the data record of that second
        out.insert(next(k for k, x in enumerate(out) if x is rec) + 1, shock)
    return out


def write_bin(path, records, version: int = CURRENT_VERSION) -> int:
    return write_records(path, records, version)
