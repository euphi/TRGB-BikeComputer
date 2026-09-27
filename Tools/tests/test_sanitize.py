"""trim_jitter(): what it collapses, and just as importantly, what it leaves
alone -- a short/slow but genuinely moving track must survive untouched."""

import datetime

from bikelog.record import LOG_GPS_VALID, Record
from bikelog.sanitize import SanitizeOptions, trim_jitter

BASE_LAT, BASE_LON = 52.4000, 8.7000
MOVE_LAT_STEP = 0.0001                  # ~11 m per record -- comfortably "moving"
START = int(datetime.datetime(2026, 5, 17, 9, 0, tzinfo=datetime.timezone.utc).timestamp())


def _rec(t: int, speed: float, lat: float = BASE_LAT, lon: float = BASE_LON,
         fix: bool = True) -> Record:
    return Record(timestamp=START + t, speed=speed, gps_flags=LOG_GPS_VALID if fix else 0,
                  gps_lat_e7=round(lat * 1e7), gps_lon_e7=round(lon * 1e7))


def _moving(start_t: int, n: int, lat0: float = BASE_LAT) -> list[Record]:
    return [_rec(start_t + i, 24.0, lat0 + i * MOVE_LAT_STEP) for i in range(n)]


def _stopped(start_t: int, n: int, lat: float = BASE_LAT) -> list[Record]:
    return [_rec(start_t + i, 0.0, lat) for i in range(n)]


def test_leading_and_trailing_stop_collapse_to_one_point_each():
    records = _stopped(0, 5) + _moving(5, 10) + _stopped(15, 5)
    result = trim_jitter(records)
    assert len(result) == 1 + 10 + 1
    assert result[0].timestamp == START + 4         # last fix before movement started
    assert result[-1].timestamp == START + 15       # first fix after movement stopped


def test_a_short_slow_ride_that_never_stops_is_untouched():
    records = _moving(0, 4)                 # too short to prove anything, but never stopped
    assert trim_jitter(records) == records


def test_records_without_a_fix_are_kept_and_do_not_anchor_a_cluster():
    records = [_rec(0, 0.0, fix=False)] + _stopped(1, 3) + _moving(4, 5)
    result = trim_jitter(records)
    # the fixless record survives (nothing to trim it against); the 3
    # stopped fixes before it collapse to the last one
    assert result[0].gps_valid is False
    assert sum(1 for r in result if not r.gps_valid) == 1
    assert len([r for r in result if r.speed == 0.0 and r.gps_valid]) == 1


def test_internal_pause_trims_both_sides():
    before = _moving(0, 10)
    stopped_before_pause = _stopped(10, 3, lat=before[-1].latitude)
    # a real gap: nothing logged for 120 s while the device slept
    stopped_after_pause = _stopped(133, 3, lat=before[-1].latitude)
    after = _moving(136, 10, lat0=before[-1].latitude)
    records = before + stopped_before_pause + stopped_after_pause + after
    result = trim_jitter(records)
    # each stopped run collapses to the single fix pressed against the gap
    kept_stopped = [r for r in result if r.speed == 0.0]
    assert len(kept_stopped) == 2
    assert kept_stopped[0].timestamp == stopped_before_pause[-1].timestamp
    assert kept_stopped[1].timestamp == stopped_after_pause[0].timestamp


def test_pause_shorter_than_min_pause_s_is_not_touched():
    records = _moving(0, 5) + _stopped(5, 2) + _moving(7, 5)
    result = trim_jitter(records, SanitizeOptions(min_pause_s=100.0))
    assert result == records


def test_disabled_via_gpx_options():
    from bikelog import gpx
    records = _stopped(0, 5) + _moving(5, 10)
    _, stats = gpx.to_string(records, gpx.GpxOptions(sanitize=False, max_fix_age_ms=0))
    assert stats.written == len(records)
    _, sanitized = gpx.to_string(records, gpx.GpxOptions(sanitize=True, max_fix_age_ms=0))
    assert sanitized.written == 1 + 10
