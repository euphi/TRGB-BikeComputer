"""Wind in the power estimate (bikelog.report + bikelog.weather), idle time before and after the
ride, and height changes while standing. Network is never touched: weather comes from
hand-made series or a fake fetch function."""

import datetime
import json
import math
import types

import pytest

from bikelog import report, ridestats, weather
from bikelog.record import (LOG_GPS_HAS_ACCURACY, LOG_GPS_HAS_BEARING, LOG_GPS_HAS_SPEED, LOG_GPS_VALID, Record,
                            RoadQualityRecord, ShockEvent)
from bikelog.weather import Sample, Weather
from bikelog.weather import fetch_days as real_fetch_days       # conftest replaces the module's

T0 = 1_780_000_000          # 2026-05-28 20:26:40 UTC
GPS = LOG_GPS_VALID | LOG_GPS_HAS_BEARING | LOG_GPS_HAS_SPEED
LAT0, LON0 = 49.45, 10.85


def gps_ride(legs, step_s=5.0, gps=True):
    """Records every step_s from legs of (seconds, speed km/h, heading deg | None, hr[, height change
    in m over the leg]). Position moves along the heading; a leg with speed 0 stands (a lift ride
    is a standing leg with a height change)."""
    records = []
    t, dist, height = 0.0, 0.0, 400.0
    lat, lon = LAT0, LON0
    for leg in legs:
        seconds, speed, heading, hr = leg[:4]
        dheight = leg[4] if len(leg) > 4 else 0.0
        n = int(seconds / step_s)
        for _ in range(n):
            t += step_s
            step = speed / 3.6 * step_s
            dist += step
            height += dheight / n
            if heading is not None:
                lat += step * math.cos(math.radians(heading)) / 111_320.0
                lon += step * math.sin(math.radians(heading)) / (111_320.0 * math.cos(math.radians(lat)))
            fields = dict(timestamp=T0 + int(t), speed=speed, temp=15.0, gradient=0.0, height=height,
                          distance=dist, hr=hr, cadence=80)
            if gps:
                fields.update(gps_flags=GPS | LOG_GPS_HAS_ACCURACY, gps_lat_e7=round(lat * 1e7),
                              gps_lon_e7=round(lon * 1e7), gps_bearing_deg_x100=round((heading or 0) * 100),
                              gps_speed_cms=round(speed / 3.6 * 100), gps_accuracy_m_x10=40)
            records.append(Record(**fields))
    return records


def steady_wind(speed_ms, from_deg, gust_ms=None):
    """Constant weather for the whole day of T0 (hourly, 10 degrees, 980 hPa)."""
    start = T0 - T0 % 3600 - 6 * 3600
    times = [start + 3600 * h for h in range(24)]
    n = len(times)
    return Weather(source="archive", lat=49.5, lon=10.9, times=times, speed_ms=[speed_ms] * n,
                   dir_deg=[from_deg] * n, gust_ms=[gust_ms or speed_ms * 1.5] * n,
                   temp_c=[10.0] * n, pressure_hpa=[980.0] * n)


ATHLETE = report.Athlete(mass_kg=90, cda=0.4, crr=0.005, hr_max=190, hr_rest=50)


# --- headwind math --------------------------------------------------------------------

def test_headwind_sign_follows_the_direction_the_wind_comes_from():
    wind = Sample(speed_ms=6.0, dir_deg=250.0)
    assert report.headwind_ms(wind, 270.0, 1.0) == pytest.approx(6 * math.cos(math.radians(20)))  # riding west
    assert report.headwind_ms(wind, 90.0, 1.0) < -5.0                                            # riding east
    assert report.headwind_ms(wind, 160.0, 1.0) == pytest.approx(0.0, abs=1e-9)                  # crosswind
    assert report.headwind_ms(wind, 270.0, 0.5) == pytest.approx(0.5 * 6 * math.cos(math.radians(20)))


def test_power_is_higher_into_the_wind_and_lower_with_it():
    west = gps_ride([(1200, 25.0, 270.0, 140)])
    east = gps_ride([(1200, 25.0, 90.0, 140)])
    wx = steady_wind(6.0, 250.0)
    plain = report.compute(west, ATHLETE)["power"]["avg_moving_w"]
    into = report.compute(west, ATHLETE, weather=wx)["power"]
    with_it = report.compute(east, ATHLETE, weather=wx)["power"]
    assert into["avg_moving_w"] > plain * 1.3
    assert with_it["avg_moving_w"] < plain * 0.6
    assert into["wind_corrected"] and into["no_wind"]["avg_moving_w"] == plain
    # the no-wind figures stay what they were without any weather
    assert with_it["no_wind"]["avg_moving_w"] == plain


def test_wind_section_describes_the_weather_and_head_tail_shares():
    west = gps_ride([(900, 25.0, 270.0, 140)])
    east = gps_ride([(900, 25.0, 90.0, 140)])
    for records, head, tail in ((west, 1.0, 0.0), (east, 0.0, 1.0)):
        wind = report.compute(records, ATHLETE, weather=steady_wind(6.0, 250.0, gust_ms=11.0))["wind"]
        assert wind["available"] and wind["source"] == "archive"
        assert wind["mean_kmh"] == pytest.approx(21.6, abs=0.2)
        assert wind["dir_deg"] == pytest.approx(250, abs=1) and wind["dir_name"] == "WSW"
        assert wind["gust_max_kmh"] == pytest.approx(39.6, abs=0.2)
        assert wind["headwind_share"] == head and wind["tailwind_share"] == tail
        assert wind["temp_c"] == 10.0
        assert wind["air_density"] == pytest.approx(980 * 100 / (287.05 * 283.15), abs=0.002)
    assert wind["avg_headwind_kmh"] == pytest.approx(-0.55 * 21.6 * math.cos(math.radians(20)), abs=0.2)
    assert report.compute(west, ATHLETE, weather=steady_wind(6.0, 250.0))["wind"]["avg_headwind_kmh"] > 10


def test_heading_from_positions_when_the_gps_has_no_bearing():
    records = gps_ride([(600, 25.0, 270.0, 140)])
    for r in records:
        r.gps_flags = LOG_GPS_VALID | LOG_GPS_HAS_ACCURACY   # fix without bearing
        r.gps_bearing_deg_x100 = 0
    headings = report._headings(records)
    assert headings[len(records) // 2] == pytest.approx(270.0, abs=1.5)
    power = report.compute(records, ATHLETE, weather=steady_wind(6.0, 250.0))["power"]
    assert power["avg_moving_w"] > power["no_wind"]["avg_moving_w"] * 1.3


def test_weather_interpolation_turns_the_short_way():
    w = Weather("archive", 49.5, 10.9, times=[0, 3600], speed_ms=[5.0, 5.0], dir_deg=[350.0, 10.0],
                gust_ms=[None, None], temp_c=[10.0, 12.0], pressure_hpa=[None, None])
    mid = w.at(1800)
    assert mid.dir_deg == pytest.approx(0.0, abs=0.5) or mid.dir_deg > 359.5
    assert mid.speed_ms == pytest.approx(5 * math.cos(math.radians(10)), abs=0.01)
    assert mid.temp_c == pytest.approx(11.0)
    assert w.at(-3601) is None and w.at(3600 + 3601) is None


# --- fallback --------------------------------------------------------------------------

def test_without_weather_the_estimate_is_the_old_one():
    records = gps_ride([(1200, 25.0, 270.0, 140)])
    rep = report.compute(records, ATHLETE)
    assert rep["wind"] == {"available": False, "reason": "no_weather"}
    assert "no_wind" not in rep["power"] and rep["power"]["wind_corrected"] is False
    v = 25 / 3.6
    expected = (90 * 9.81 * 0.005 * v + 0.5 * report.RHO_AIR * 0.4 * v ** 3) / 0.97
    assert rep["power"]["avg_moving_w"] == pytest.approx(expected, abs=1)
    assert report.compute(gps_ride([(600, 25.0, 270.0, 140)], gps=False), ATHLETE)["wind"]["reason"] == "no_gps"
    assert report.weather_request(gps_ride([(600, 25.0, 270.0, 140)], gps=False)) is None
    md = report.to_markdown(rep)
    assert "Keine Winddaten" in md and "ohne Wind" in md


def test_weather_request_is_the_ride_proper():
    records = gps_ride([(600, 0.0, None, 100), (1200, 25.0, 90.0, 140), (900, 0.0, None, 100)])
    lat, lon, start, end = report.weather_request(records)
    assert lat == pytest.approx(LAT0, abs=0.01) and LON0 < lon < LON0 + 0.12
    assert start == pytest.approx(T0 + 600, abs=10) and end == pytest.approx(T0 + 1800, abs=10)


# --- decoupling -----------------------------------------------------------------------

LOOP = [(2700, 28.0, 270.0, 150), (2700, 28.0, 90.0, 150)]       # out west, back east, same effort


def test_decoupling_without_weather_is_the_no_wind_figure():
    heart = report.compute(gps_ride(LOOP), ATHLETE)["heart"]
    assert heart["decoupling_pct"] == pytest.approx(0.0, abs=0.1)
    assert heart["decoupling_basis"] == "no_wind"


def test_decoupling_is_suppressed_when_the_wind_decides():
    rep = report.compute(gps_ride(LOOP), ATHLETE, weather=steady_wind(7.0, 250.0))
    heart = rep["heart"]
    assert heart["decoupling_pct"] is None and heart["decoupling_basis"] == "suppressed"
    assert heart["decoupling_nowind_pct"] == pytest.approx(0.0, abs=0.1)
    assert heart["decoupling_wind_pct"] > 30
    assert heart["decoupling_spread_pts"] > report.DECOUPLING_MAX_SPREAD_PTS
    head1, head2 = heart["halves_headwind_kmh"]
    assert head1 > 10 and head2 < -10
    md = report.to_markdown(rep)
    assert "nicht angegeben" in md and "Wind entscheidet" in md


def test_decoupling_is_kept_when_there_is_hardly_any_wind():
    heart = report.compute(gps_ride(LOOP), ATHLETE, weather=steady_wind(0.4, 250.0))["heart"]
    assert heart["decoupling_basis"] == "wind"
    assert heart["decoupling_pct"] == heart["decoupling_wind_pct"] and heart["decoupling_spread_pts"] < 8


def test_decoupling_is_kept_when_both_halves_share_the_wind():
    # same direction twice, so the wind is the same in both halves (and there is a lot of it)
    heart = report.compute(gps_ride([(2700, 28.0, 270.0, 150), (2700, 28.0, 270.0, 150)]), ATHLETE,
                           weather=steady_wind(3.0, 250.0))["heart"]
    assert heart["decoupling_pct"] is not None
    assert heart["decoupling_spread_pts"] < 1


# --- idle time ------------------------------------------------------------------------

def test_idle_before_and_after_is_not_part_of_the_ride():
    # 10 min standing in the garage, 20 min riding, 3 min stop, 10 min riding, 25 min standing at home
    legs = [(600, 0.0, None, 70), (1200, 25.0, 90.0, 140), (180, 0.0, None, 120),
            (600, 25.0, 90.0, 140), (1500, 0.0, None, 60)]
    records = gps_ride(legs)
    for r in records:
        if r.speed == 0.0 and r.timestamp - T0 > 3000:
            r.temp = 21.0                       # warm flat at the end
    road = [RoadQualityRecord(timestamp=T0 + 100, distance_m=0.0), RoadQualityRecord(timestamp=T0 + 900, distance_m=5.0),
            RoadQualityRecord(timestamp=T0 + 3000, distance_m=0.0)]
    shocks = [ShockEvent(timestamp=T0 + 50), ShockEvent(timestamp=T0 + 900)]
    rep = report.compute(records + road + shocks, ATHLETE)
    assert rep["meta"]["idle"]["leading_s"] == pytest.approx(600, abs=10)
    assert rep["meta"]["idle"]["trailing_s"] == pytest.approx(1500, abs=10)
    assert rep["ride"]["duration_s"] == pytest.approx(1200 + 180 + 600, abs=10)
    assert rep["ride"]["moving_s"] == pytest.approx(1800, abs=10)
    assert rep["ride"]["stops"]["count"] == 1 and rep["ride"]["stops"]["total_s"] == pytest.approx(180, abs=10)
    assert rep["ride"]["temp_c"]["max"] == 15.0
    assert rep["road"]["intervals"] == 1 and rep["road"]["shocks"]["count"] == 1
    assert rep["health"]["gps"]["fresh_share"] == 1.0
    assert "Leerlauf, nicht mitgezählt | 0:10:00 vor, 0:25:00 nach" in report.to_markdown(rep)


def test_a_log_without_any_riding_is_left_whole():
    records = gps_ride([(900, 0.0, None, 70)])
    rep = report.compute(records, ATHLETE)
    assert rep["meta"]["idle"] == {"leading_s": 0, "trailing_s": 0}
    assert rep["ride"]["duration_s"] == pytest.approx(900, abs=10)


def test_rolling_the_wheel_by_hand_does_not_start_the_ride():
    # 10 m of wheel turning at the door (speed >= 2 km/h for 10 s), then standing, then the ride
    legs = [(10, 3.0, None, 70), (300, 0.0, None, 70), (900, 25.0, 90.0, 140)]
    rep = report.compute(gps_ride(legs), ATHLETE)
    assert rep["meta"]["idle"]["leading_s"] == pytest.approx(310, abs=10)


# --- height changes while standing -------------------------------------------------------

def test_a_lift_ride_while_standing_is_not_climbing():
    # 10 min riding, 2 min standing while the height jumps by 14 m (a lift), 10 min riding on flat
    # ground, 2 min standing at the end while the height jumps by 10 m
    legs = [(600, 25.0, 90.0, 140), (120, 0.0, None, 100, 14.0), (600, 25.0, 90.0, 140),
            (120, 0.0, None, 100, 10.0)]
    records = gps_ride(legs)
    stats = ridestats.compute(records)
    assert stats.ascent_m == 0 and stats.descent_m == 0
    # heights while riding only: 400 before the lift, 414 after it, never the 424 of the end
    assert stats.min_ele_m == pytest.approx(400.0, abs=0.01) and stats.max_ele_m == pytest.approx(414.0, abs=0.01)
    rep = report.compute(records, ATHLETE, with_hills=True)
    assert rep["ride"]["ascent_m"] == 0
    assert rep["climbs"] == []
    assert max(h for _, h in rep["profile"]) - min(h for _, h in rep["profile"]) < 0.5


def test_real_climbing_still_counts_next_to_a_lift_ride():
    legs = [(300, 25.0, 90.0, 140), (120, 0.0, None, 100, -10.0), (300, 15.0, 90.0, 140, 30.0)]
    stats = ridestats.compute(gps_ride(legs))
    assert stats.ascent_m == pytest.approx(30.0, abs=3)
    assert stats.descent_m == 0


def test_corrected_heights_hold_the_level_while_standing():
    records = gps_ride([(100, 25.0, 90.0, 140), (50, 0.0, None, 100, 10.0), (100, 25.0, 90.0, 140)])
    heights = ridestats.corrected_heights(records)
    assert max(heights) - min(heights) < 0.01
    assert heights[len(records) // 2] == pytest.approx(400.0, abs=0.01)


# --- weather module ----------------------------------------------------------------------

def day_data(day, speed=5.0, direction=250.0, nulls=False):
    start = int(datetime.datetime(day.year, day.month, day.day, tzinfo=datetime.timezone.utc).timestamp())
    none = [None] * 24
    return {"time": [start + 3600 * h for h in range(24)],
            "wind_speed_10m": none if nulls else [speed] * 24,
            "wind_direction_10m": none if nulls else [direction] * 24,
            "wind_gusts_10m": none if nulls else [speed * 2] * 24,
            "temperature_2m": none if nulls else [10.0] * 24,
            "surface_pressure": none if nulls else [980.0] * 24}


class FakeFetch:
    def __init__(self, archive_nulls=(), fail=False):
        self.calls = []
        self.archive_nulls = set(archive_nulls)
        self.fail = fail

    def __call__(self, url, lat, lon, first, last):
        self.calls.append((url, lat, lon, first, last))
        if self.fail:
            raise weather.WeatherError("down")
        days = [first + datetime.timedelta(days=n) for n in range((last - first).days + 1)]
        nulls = url == weather.ARCHIVE_URL
        return {d: day_data(d, nulls=nulls and d in self.archive_nulls) for d in days}


RIDE_TS = int(datetime.datetime(2026, 3, 14, 10, tzinfo=datetime.timezone.utc).timestamp())
LONG_AFTER = RIDE_TS + 30 * 86400


def test_cell_rounds_to_a_tenth_of_a_degree():
    assert weather.cell(49.449, 10.851) == (49.4, 10.9)
    assert weather.cell(49.45, 10.85) == (49.5, 10.9)
    assert weather.cell(-0.04, -0.06) == (-0.0, -0.1) or weather.cell(-0.04, -0.06) == (0.0, -0.1)


def test_load_fetches_once_and_then_reads_the_cache(tmp_path):
    fetch = FakeFetch()
    got = weather.load(tmp_path, 49.4491, 10.8512, RIDE_TS, RIDE_TS + 3600, now=LONG_AFTER, fetch=fetch)
    assert got.weather is not None and got.error is None and got.retry_after is None
    assert got.weather.source == "archive" and (got.weather.lat, got.weather.lon) == (49.4, 10.9)
    assert got.weather.at(RIDE_TS + 600).speed_ms == pytest.approx(5.0)
    assert len(fetch.calls) == 1
    # privacy: the request carries the cell, not the position
    assert fetch.calls[0][1:3] == (49.4, 10.9)
    assert sorted(p.name for p in tmp_path.iterdir()) == ["2026-03-14_49.4_10.9.json"]
    again = weather.load(tmp_path, 49.4491, 10.8512, RIDE_TS, RIDE_TS + 3600, now=LONG_AFTER, fetch=FakeFetch(fail=True))
    assert again.weather is not None and again.weather.at(RIDE_TS).speed_ms == pytest.approx(5.0)


def test_url_has_only_the_rounded_cell():
    seen = []

    class Resp:
        def __enter__(self):
            return self

        def __exit__(self, *exc):
            return False

        def read(self):
            return json.dumps({"hourly": {"time": ["2026-03-14T00:00"], **{k: [1.0] for k in weather.HOURLY}}}).encode()

    def opener(req, timeout):
        seen.append((req.full_url, timeout))
        return Resp()

    day = datetime.date(2026, 3, 14)
    out = real_fetch_days(weather.ARCHIVE_URL, 49.4, 10.9, day, day, opener=opener)
    assert out[day]["wind_speed_10m"] == [1.0]
    url, timeout = seen[0]
    assert "latitude=49.4" in url and "longitude=10.9" in url and "wind_speed_unit=ms" in url
    assert "timezone=GMT" in url and timeout == weather.TIMEOUT_S


def test_archive_gaps_are_filled_from_the_historical_forecast(tmp_path):
    day = datetime.date(2026, 3, 14)
    fetch = FakeFetch(archive_nulls=[day])
    got = weather.load(tmp_path, 49.4, 10.9, RIDE_TS, RIDE_TS + 3600, now=LONG_AFTER, fetch=fetch)
    assert got.weather is not None and got.weather.source == "historical-forecast"
    assert [c[0] for c in fetch.calls] == [weather.ARCHIVE_URL, weather.FORECAST_URL]


def test_a_failed_fetch_is_no_wind_and_a_retry_time_not_an_exception(tmp_path):
    got = weather.load(tmp_path, 49.4, 10.9, RIDE_TS, RIDE_TS + 3600, now=LONG_AFTER, fetch=FakeFetch(fail=True))
    assert got.weather is None and got.error == "network"
    assert got.retry_after == LONG_AFTER + weather.RETRY_S
    # backoff: the next ride does not wait for another timeout
    fetch = FakeFetch()
    second = weather.load(tmp_path, 49.4, 10.9, RIDE_TS, RIDE_TS + 3600, now=LONG_AFTER, fetch=fetch)
    assert second.weather is None and second.error == "network" and fetch.calls == []


def test_a_day_with_no_data_at_all_is_reported_as_such(tmp_path):
    day = datetime.date(2026, 3, 14)

    def empty(url, lat, lon, first, last):
        return {day: day_data(day, nulls=True)}

    got = weather.load(tmp_path, 49.4, 10.9, RIDE_TS, RIDE_TS + 3600, now=LONG_AFTER, fetch=empty)
    assert got.weather is None and got.error == "no_data"


def test_recent_days_are_provisional_and_fetched_again_once_final(tmp_path):
    fetch = FakeFetch()
    now = RIDE_TS + 3600
    first = weather.load(tmp_path, 49.4, 10.9, RIDE_TS, RIDE_TS + 1800, now=now, fetch=fetch)
    assert first.weather is not None and first.retry_after is not None
    final_at = first.retry_after
    assert final_at > now + 6 * 86400
    weather.load(tmp_path, 49.4, 10.9, RIDE_TS, RIDE_TS + 1800, now=now + 86400, fetch=fetch)
    assert len(fetch.calls) == 1                                    # cache
    done = weather.load(tmp_path, 49.4, 10.9, RIDE_TS, RIDE_TS + 1800, now=final_at + 1, fetch=fetch)
    assert len(fetch.calls) == 2 and done.retry_after is None       # now final


def test_a_ride_across_midnight_gets_both_days(tmp_path):
    late = int(datetime.datetime(2026, 3, 14, 23, 30, tzinfo=datetime.timezone.utc).timestamp())
    fetch = FakeFetch()
    got = weather.load(tmp_path, 49.4, 10.9, late, late + 3600, now=LONG_AFTER, fetch=fetch)
    assert got.weather is not None and len(got.weather.times) == 48
    assert got.weather.at(late + 1800) is not None


# --- the service side ----------------------------------------------------------------------

def test_service_compute_report_uses_the_cache_and_marks_retries(tmp_path, monkeypatch):
    from bikelogservice import analysis
    from bikelogservice.config import Settings

    store = types.SimpleNamespace(settings=Settings(data_dir=tmp_path))
    records = gps_ride([(1200, 25.0, 270.0, 140)])
    # offline (conftest): no wind, why, and when to try again
    rep = analysis.compute_report(store, records, ATHLETE)
    assert rep["wind"]["available"] is False and rep["wind"]["reason"] == "network"
    assert rep["wind"]["retry_after"] > T0
    assert analysis.retry_due(rep, now=rep["wind"]["retry_after"] + 1)
    assert not analysis.retry_due(rep, now=T0)
    # online
    monkeypatch.setattr(weather, "fetch_days", FakeFetch())
    monkeypatch.setattr(weather, "_failed_at", None)
    rep = analysis.compute_report(store, records, ATHLETE)
    assert rep["wind"]["available"] and rep["wind"]["source"] == "archive"
    assert rep["power"]["wind_corrected"]
    assert (tmp_path / "weather").is_dir() and any((tmp_path / "weather").iterdir())
    # switched off
    off = types.SimpleNamespace(settings=Settings(data_dir=tmp_path / "x", wind=False))
    rep = analysis.compute_report(off, records, ATHLETE)
    assert rep["wind"]["reason"] == "disabled" and "retry_after" not in rep["wind"]


def test_reports_asking_for_a_retry_are_found_in_the_index(tmp_path):
    from bikelogservice.config import Settings
    from bikelogservice.storage import Storage

    from bikelogservice.sdlayout import parse_path

    store = Storage(Settings(data_dir=tmp_path))
    ids = [store.put_file("trgb", parse_path("20260920/L_%02d0000.bin" % n), b"\0" * 64).session.id
           for n in range(1, 5)]
    store.store_report(ids[0], "k", {"wind": {"available": False, "retry_after": 1000}})
    store.store_report(ids[1], "k", {"wind": {"available": True, "retry_after": 5000}})
    store.store_report(ids[2], "k", {"wind": {"available": True}})
    store.store_report(ids[3], "k", {"empty": True})
    assert store.reports_due(999) == set()
    assert store.reports_due(1000) == {ids[0]}
    assert store.reports_due(6000) == {ids[0], ids[1]}
