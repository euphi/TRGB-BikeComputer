"""bikelog.tripmode: a phone's all-day recording, cut into trips, only cycling kept."""

import math

from bikelog import tripmode as tm

T0 = 1_780_000_000.0


def leg(t0, lat0, kmh, minutes, step_s=6.0, stop_and_go=False):
    """Fixes going north from lat0 at kmh for minutes, one every step_s."""
    out, lat, t = [], lat0, t0
    for i in range(int(minutes * 60 / step_s)):
        v = kmh
        if stop_and_go:
            v = kmh * (2.6 if i % 10 < 3 else 0.6)      # bursts and crawling, mean about kmh
        out.append(tm.Fix(t, lat, 11.0, 5.0))
        lat += v / 3.6 * step_s / 111320.0
        t += step_s
    return out


def stop(t0, lat, minutes, step_s=30.0):
    return [tm.Fix(t0 + i * step_s, lat + (i % 2) * 2e-5, 11.0, 5.0) for i in range(int(minutes * 60 / step_s))]


def test_day_with_walk_bike_and_train():
    walk = leg(T0, 49.0, 5, 20)
    s1 = stop(walk[-1].t + 6, walk[-1].lat, 10)
    bike = leg(s1[-1].t + 6, s1[-1].lat, 22, 40)
    s2 = stop(bike[-1].t + 6, bike[-1].lat, 15)
    train = leg(s2[-1].t + 6, s2[-1].lat, 110, 30)
    trips = tm.trips(walk + s1 + bike + s2 + train)
    kinds = [t.kind for t in trips if t.kind != "short"]
    assert kinds == ["walk", "bike", "motor"]
    ride = next(t for t in trips if t.kind == "bike")
    assert math.isclose(ride.stats["distance_km"], 22 * 40 / 60, rel_tol=0.05)


def test_town_traffic_is_no_bike_ride():
    car = leg(T0, 49.0, 22, 15, stop_and_go=True)
    assert tm.classify(car).kind == "motor"
    assert tm.classify(leg(T0, 49.0, 20, 30)).kind == "bike"


def test_a_long_descent_is_still_a_bike_ride():
    ride = leg(T0, 49.0, 22, 20) + leg(T0 + 1200, 49.0 + 22 / 3 / 111.32, 50, 2) + \
        leg(T0 + 1320, 49.0 + 22 / 3 / 111.32 + 1.67 / 111.32, 22, 20)
    assert tm.classify(ride).kind == "bike"


def test_gap_splits_and_bad_fixes_are_dropped():
    a = leg(T0, 49.0, 20, 10)
    b = leg(a[-1].t + 3600, 49.5, 20, 10)
    bad = [tm.Fix(a[3].t + 1, 50.0, 12.0, 500.0)]
    assert len(tm.split(sorted(a + b + bad, key=lambda f: f.t))) == 2


def test_read_fixes_filters_device_and_merges_overlaps():
    def gpx(name, fixes):
        pts = "".join(f'<trkpt lat="{f.lat}" lon="{f.lon}"><time>2026-05-01T10:{i // 60:02d}:{i % 60:02d}Z</time>'
                      f"<extensions><accuracy>4.0</accuracy></extensions></trkpt>" for i, f in fixes)
        return f"<trk><name>{name}</name><trkseg>{pts}</trkseg></trk>"
    phone = [(i, tm.Fix(0, 49.0 + i * 1e-4, 11.0)) for i in range(10)]
    other = [(i, tm.Fix(0, 52.0, 13.0)) for i in range(10)]
    data = f'<gpx xmlns="http://www.topografix.com/GPX/1/1">{gpx("IanMobil", phone)}{gpx("Ian", other)}</gpx>'.encode()
    fixes = tm.read_fixes(data, "IanMobil")
    assert len(fixes) == 10 and all(f.lat < 50 for f in fixes) and fixes[0].accuracy == 4.0
    assert len(tm.merge(fixes, fixes[5:])) == 10


def test_to_gpx_round_trip():
    trip = tm.classify(leg(T0, 49.0, 20, 10))
    again = tm.read_fixes(tm.to_gpx(trip, "Handy & Rad"))
    assert len(again) == len(trip.fixes) and again[0].t == trip.fixes[0].t
