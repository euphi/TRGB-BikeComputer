"""bikelog.training: load model, weeks, target events, recurring climbs."""

import datetime

import pytest

from bikelog import training

D0 = datetime.date(2026, 3, 2)          # a Monday


def ride(day, km=40.0, trimp=80.0, ascent=300.0, climbs=(), best20=None, zones=None):
    start = datetime.datetime.combine(day, datetime.time(10), datetime.timezone.utc)
    return training.Ride(session_id=1, start=start, day=day, distance_km=km, moving_s=km / 25 * 3600,
                         ascent_m=ascent, trimp=trimp, zones_s=zones, avg_hr=140, normalized_w=180,
                         best_w={"20min": best20} if best20 else {}, decoupling_pct=None,
                         efficiency=None, climbs=list(climbs))


def gpx(points, tag="trkpt"):
    pts = "".join(f'<{tag} lat="{lat}" lon="{lon}">' + (f"<ele>{ele}</ele>" if ele is not None else "")
                  + f"</{tag}>" for lat, lon, ele in points)
    wrap = ("<trk><trkseg>%s</trkseg></trk>" if tag == "trkpt" else "<rte>%s</rte>") % pts
    return f'<?xml version="1.0"?><gpx xmlns="http://www.topografix.com/GPX/1/1" version="1.1">{wrap}</gpx>'.encode()


def course(flat_km=2.0, climb_km=2.0, grade=5.0):
    """Points every 25 m going north: flat, then a climb, then flat again."""
    pts = []
    step = 25 / 111320.0
    ele = 100.0
    n_flat, n_climb = int(flat_km * 40), int(climb_km * 40)
    for i in range(n_flat + n_climb + n_flat):
        if n_flat <= i < n_flat + n_climb:
            ele += 25 * grade / 100
        pts.append((50.0 + i * step, 8.6, round(ele, 2)))
    return pts


def test_load_model_one_ride():
    pts = training.load_series([ride(D0, trimp=100)], D0, D0 + datetime.timedelta(days=1))
    assert pts[0].trimp == 100
    assert pts[0].ctl == pytest.approx(100 / 42, abs=0.1)
    assert pts[0].atl == pytest.approx(100 / 7, abs=0.1)
    assert pts[0].tsb == 0                                   # form is yesterday's
    assert pts[1].tsb == pytest.approx(100 / 42 - 100 / 7, abs=0.1)


def test_load_model_steady_training_converges():
    rides = [ride(D0 + datetime.timedelta(days=i), trimp=50) for i in range(300)]
    last = training.load_series(rides, D0 + datetime.timedelta(days=299), D0 + datetime.timedelta(days=299))[0]
    assert last.ctl == pytest.approx(50, abs=0.5)
    assert last.atl == pytest.approx(50, abs=0.1)


def test_window_starting_later_keeps_the_history():
    rides = [ride(D0 + datetime.timedelta(days=i), trimp=60) for i in range(60)]
    late = D0 + datetime.timedelta(days=59)
    assert training.load_series(rides, late, late)[0].ctl > 40


def test_weeks():
    rides = [ride(D0, km=50, best20=200, zones={"Z2": 3600}),
             ride(D0 + datetime.timedelta(days=2), km=30, trimp=None, best20=220, zones={"Z2": 600, "Z3": 300}),
             ride(D0 + datetime.timedelta(days=8), km=70)]
    ws = training.weeks(rides, D0 + datetime.timedelta(days=9), 3)
    assert [w.monday for w in ws] == [D0 - datetime.timedelta(days=7), D0, D0 + datetime.timedelta(days=7)]
    assert ws[0].rides == 0
    assert ws[1].rides == 2 and ws[1].distance_km == 80 and ws[1].longest_km == 50
    assert ws[1].trimp == 80 and ws[1].rides_without_hr == 1
    assert ws[1].zones_s == {"Z2": 4200, "Z3": 300}
    assert ws[1].best_20min_w == 220
    assert ws[2].distance_km == 70


def test_ride_from_report_skips_simulator_and_short_sessions():
    rep = {"meta": {"simulated": False, "start_utc": "2026-05-17T09:00:00+00:00"},
           "ride": {"distance_km": 12.0, "moving_s": 1800, "ascent_m": 100},
           "heart": {"trimp": 30, "zones_s": None, "avg": 130, "decoupling_pct": None,
                     "efficiency_w_per_bpm": None},
           "power": {"normalized_w": 150, "best_w": {}}, "climbs": []}
    assert training.Ride.from_report(rep, 7).session_id == 7
    rep["meta"]["simulated"] = True
    assert training.Ride.from_report(rep) is None
    rep["meta"]["simulated"] = False
    rep["ride"]["distance_km"] = 0.4
    assert training.Ride.from_report(rep) is None
    assert training.Ride.from_report({"empty": True}) is None


def test_event_profile_finds_the_climb():
    prof = training.event_profile(gpx(course()))
    assert prof["distance_km"] == pytest.approx(6.0, abs=0.1)
    assert prof["ascent_m"] == pytest.approx(100, abs=3)
    assert len(prof["climbs"]) == 1
    c = prof["climbs"][0]
    assert c["category"] == "4" and c["start_km"] == pytest.approx(2.0, abs=0.1)
    assert prof["profile"][0] == [0.0, 100.0]


def test_event_profile_takes_route_points_and_needs_elevation():
    assert training.event_profile(gpx(course(), tag="rtept"))["climbs"]
    with pytest.raises(ValueError):
        training.event_profile(gpx([(50.0, 8.6, None), (50.01, 8.6, None)]))


def test_phases():
    assert training.phase(-1)[0] == "Vorbei"
    assert training.phase(3)[0] == "Tapering"
    assert training.phase(14)[0] == "Spitze"
    assert training.phase(60)[0] == "Aufbau"
    assert training.phase(154)[0] == "Grundlage"


def test_readiness():
    today = datetime.date(2026, 10, 4)
    event = training.Event(name="GF Strade Bianche", date=datetime.date(2027, 3, 7),
                           profile=training.event_profile(gpx(course(climb_km=4))))
    climbs = [{"category": "4", "gain_m": 150, "vam_m_h": 900, "length_m": 3000}]
    rides = [ride(today - datetime.timedelta(days=3), km=60, ascent=700, climbs=climbs),
             ride(today - datetime.timedelta(days=60), km=150)]          # too long ago
    r = training.readiness(event, rides, today)
    assert r["days_left"] == 154 and r["phase"] == "Grundlage"
    assert r["longest_km"] == 60
    assert r["longest_share"] == pytest.approx(60 / event.course_km, abs=0.01)
    assert r["max_week_ascent_m"] == 700
    est = r["climbs"][0]
    assert est["gain_m"] == pytest.approx(200, abs=3)
    assert est["vam_m_h"] == 900
    assert est["est_duration_s"] == pytest.approx(est["gain_m"] / 900 * 3600, abs=1)


def test_recurring_climbs():
    def climb(foot, summit, duration):
        return {"category": "5", "length_m": 900, "gain_m": 50, "avg_grade_pct": 5.5,
                "foot": foot, "summit": summit, "duration_s": duration, "vam_m_h": 50 / duration * 3600}
    a, b = (50.1, 8.6), (50.108, 8.6)
    near_a = (50.1005, 8.6005)                          # ~65 m away
    rides = [ride(D0, climbs=[climb(a, b, 300)]),
             ride(D0 + datetime.timedelta(days=3), climbs=[climb(near_a, b, 280)]),
             ride(D0 + datetime.timedelta(days=5), climbs=[climb((50.3, 8.6), (50.31, 8.6), 200)])]
    groups = training.recurring_climbs(rides)
    assert len(groups) == 1
    assert [e["duration_s"] for e in groups[0]["efforts"]] == [280, 300]


def test_events_round_trip(tmp_path):
    path = tmp_path / "events.json"
    e = training.Event(name="Eschborn-Frankfurt", date=datetime.date(2027, 5, 1), priority="B")
    training.save_events(path, [e])
    back = training.load_events(path)
    assert back[0].name == "Eschborn-Frankfurt" and back[0].date == e.date and back[0].id == e.id
    assert training.load_events(tmp_path / "none.json") == []
