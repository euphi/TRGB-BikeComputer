"""bikelog.report: the figures a session report (template or LLM) is built from."""

import json
import math

import pytest

from bikelog import cli, fixtures, report
from bikelog.record import ReadStats, Record, ShockEvent, read_file

T0 = 1_780_000_000          # 2026-05-28


def ride(profile, step_s=5.0, hr=140, cadence=85):
    """Records every step_s from a list of (seconds, speed km/h, gradient %).
    Height follows the gradient over the distance ridden."""
    records = []
    t, dist, height = 0.0, 0.0, 100.0
    for seconds, speed, grade in profile:
        for _ in range(int(seconds / step_s)):
            t += step_s
            step = speed / 3.6 * step_s
            dist += step
            height += step * grade / 100
            records.append(Record(timestamp=T0 + int(t), timestamp_ms=int(t % 1 * 1000), speed=speed,
                                  temp=15.0, gradient=grade, height=height, distance=dist,
                                  hr=hr, cadence=cadence))
    return records


def test_steady_flat_ride_power_matches_the_model():
    rep = report.compute(ride([(1800, 30.0, 0.0)]), report.Athlete(mass_kg=80, cda=0.35, crr=0.005))
    v = 30 / 3.6
    expected = (80 * 9.81 * 0.005 * v + 0.5 * 1.2 * 0.35 * v ** 3) / 0.97
    assert rep["power"]["avg_moving_w"] == pytest.approx(expected, abs=1)
    assert rep["power"]["normalized_w"] == pytest.approx(expected, abs=1)
    assert rep["power"]["best_w"]["20min"] == pytest.approx(expected, abs=1)
    assert rep["power"]["best_w"]["60min"] is None          # ride too short
    assert rep["power"]["best_w_per_kg"] is None            # no rider_kg given
    assert rep["ride"]["distance_km"] == pytest.approx(15.0, abs=0.05)


def test_climb_is_found_and_rated_like_the_firmware():
    # 2 km flat, 2 km at 5 % (100 m), 2 km down, 1 km flat
    records = ride([(480, 15.0, 0.0), (720, 10.0, 5.0), (240, 30.0, -5.0), (240, 15.0, 0.0)])
    rep = report.compute(records)
    assert len(rep["climbs"]) == 1
    climb = rep["climbs"][0]
    assert climb["category"] == "4"                        # 100 m x 100 = 10000 points
    assert climb["start_km"] == pytest.approx(2.0, abs=0.1)
    assert climb["length_m"] == pytest.approx(2000, abs=60)
    assert climb["gain_m"] == pytest.approx(100, abs=3)
    assert climb["avg_grade_pct"] == pytest.approx(5.0, abs=0.2)
    assert climb["vam_m_h"] == pytest.approx(500, abs=25)   # 100 m in 12 min
    assert climb["avg_hr"] == 140


def test_hills_below_category_6_only_on_request():
    records = ride([(240, 15.0, 0.0), (90, 10.0, 6.0), (240, 15.0, 0.0)])   # 250 m, 15 m
    assert report.compute(records)["climbs"] == []
    hills = report.compute(records, with_hills=True)["climbs"]
    assert len(hills) == 1 and hills[0]["category"] is None


def test_climb_categories():
    assert report.climb_category(400, 20) == "6"
    assert report.climb_category(1600, 80) == "4"
    assert report.climb_category(10000, 800) == "HC"
    assert report.climb_category(250, 20) is None          # too short
    assert report.climb_category(2000, 40) is None         # 2 % -- too flat


def test_zones_and_trimp_need_hr_max():
    records = ride([(1200, 25.0, 0.0)], hr=150)
    plain = report.compute(records)["heart"]
    assert plain["avg"] == 150 and plain["zones_s"] is None and plain["trimp"] is None

    edwards = report.compute(records, report.Athlete(hr_max=200))["heart"]
    assert edwards["trimp_method"] == "edwards"
    assert edwards["zones_s"]["Z3"] == pytest.approx(1195, abs=10)      # 75 % of 200
    assert edwards["trimp"] == pytest.approx(20 * 3, abs=1)            # 20 min in zone 3

    banister = report.compute(records, report.Athlete(hr_max=200, hr_rest=50))["heart"]
    hrr = (150 - 50) / 150
    assert banister["trimp_method"] == "banister"
    assert banister["trimp"] == pytest.approx(1195 / 60 * hrr * 0.64 * math.exp(1.92 * hrr), abs=1)


def test_decoupling_compares_the_halves():
    first = ride([(1500, 25.0, 0.0)], hr=130)
    second = ride([(1500, 25.0, 0.0)], hr=143)
    for rec in second:
        rec.timestamp += 1500
        rec.distance += first[-1].distance
    rep = report.compute(first + second)
    assert rep["heart"]["decoupling_pct"] == pytest.approx((1 - 130 / 143) * 100, abs=0.5)
    # too short: no value
    assert report.compute(first)["heart"]["decoupling_pct"] is None


def test_hr_dropout_is_reported_between_valid_readings_only():
    records = ride([(600, 25.0, 0.0)])
    for rec in records[40:50]:          # 50 s without reading
        rec.hr = 0
    for rec in records[-5:]:            # strap taken off at the end: no dropout
        rec.hr = 0
    health = report.compute(records)["health"]
    assert len(health["hr"]["dropouts"]) == 1
    assert health["hr"]["dropouts"][0]["duration_s"] == 50
    assert [f["code"] for f in health["findings"]] == ["hr_dropout", "no_gps"]


def test_stops_include_logging_gaps():
    a = ride([(300, 20.0, 0.0), (60, 0.0, 0.0), (300, 20.0, 0.0)])
    b = ride([(300, 20.0, 0.0)])
    for rec in b:                       # device slept 10 min
        rec.timestamp += a[-1].timestamp - T0 + 600
        rec.distance += a[-1].distance
    rep = report.compute(a + b)
    assert rep["ride"]["stops"]["count"] == 2
    assert rep["ride"]["stops"]["longest_s"] == pytest.approx(605, abs=5)
    assert "log_gap" in [f["code"] for f in rep["health"]["findings"]]


def test_standing_on_after_a_logging_gap_is_one_stop():
    a = ride([(300, 20.0, 0.0)])
    b = ride([(30, 0.0, 0.0), (300, 20.0, 0.0)])
    for rec in b:
        rec.timestamp += a[-1].timestamp - T0 + 120
        rec.distance += a[-1].distance
    stops = report.compute(a + b)["ride"]["stops"]
    assert stops["count"] == 1
    assert stops["longest_s"] == pytest.approx(155, abs=5)


def test_unset_clock_records_are_left_out():
    records = fixtures.synthetic(unset_clock_records=4, seconds=300)
    rep = report.compute(records)
    assert rep["meta"]["unset_clock_records"] == 4
    assert rep["meta"]["start_utc"].startswith("2026-05-17")
    assert rep["ride"]["duration_s"] < 400


def test_lost_records_and_cut_off_file(tmp_path):
    records = ride([(300, 20.0, 0.0)])
    shocks = [ShockEvent(timestamp=T0 + 100, peak_total_mg=4000, severity=1, event_seq=n)
              for n in (1, 2, 5)]
    path = tmp_path / "L.bin"
    fixtures.write_bin(path, records + shocks)
    with open(path, "ab") as fh:
        fh.write(b"\x00" * 10)
    stats = ReadStats()
    rep = report.compute(list(read_file(path, stats, types=None)), stats=stats)
    assert rep["health"]["lost_records"]["shocks"] == 2
    assert rep["health"]["trailing_bytes"] == 10
    assert {"records_lost", "truncated"} <= {f["code"] for f in rep["health"]["findings"]}
    assert rep["road"]["shocks"]["count"] == 3


def test_wheel_circumference_off_against_gps():
    records = fixtures.synthetic(seconds=600, pause=None, tunnel=None)
    for rec in records:
        rec.speed *= 1.06                # wheel says 6 % more than GPS
    codes = [f["code"] for f in report.compute(records)["health"]["findings"]]
    assert "wheel_circumference" in codes


def test_synthetic_ride_with_road_quality_round_trip(tmp_path):
    records = fixtures.with_road_quality(fixtures.synthetic(seconds=900))
    rep = report.compute(records, report.Athlete(hr_max=190, hr_rest=55, rider_kg=72))
    json.dumps(rep)                     # serialisable as it is
    assert rep["report_version"] == report.REPORT_VERSION
    assert rep["road"]["intervals"] > 0
    assert rep["road"]["shocks"]["top"][0]["g"] == 6.2
    assert rep["road"]["shocks"]["top"][0]["both_wheels"] is True
    assert rep["power"]["best_w_per_kg"]["5min"] is not None
    md = report.to_markdown(rep)
    assert "## Fahrt" in md and "## Wege" in md and "## Technik" in md


def test_empty_log():
    rep = report.compute([])
    assert rep["empty"] is True
    assert "Leeres Log" in report.to_markdown(rep)


def test_athlete_file(tmp_path):
    path = tmp_path / "athlete.json"
    path.write_text('{"hr_max": 182, "zones_pct": [55, 75, 85, 92]}')
    athlete = report.Athlete.load(path)
    assert athlete.hr_max == 182 and athlete.zones_pct == (55.0, 75.0, 85.0, 92.0)
    path.write_text('{"hrmax": 182}')
    with pytest.raises(ValueError):
        report.Athlete.load(path)


def test_cli_report(tmp_path, capsys):
    log = tmp_path / "L.bin"
    fixtures.write_bin(log, fixtures.with_road_quality(fixtures.synthetic(seconds=600)))
    assert cli.main(["report", "-i", str(log)]) == 0
    assert "# Sitzungsbericht" in capsys.readouterr().out
    out = tmp_path / "r.json"
    assert cli.main(["report", "-i", str(log), "--json", "-o", str(out)]) == 0
    assert json.loads(out.read_text())["meta"]["format_version"] == 2
