"""The map in the log service: tracks from rides, uploads and Komoot; overlay tiles."""

import datetime
import io
import json
import zipfile

import pytest
from fastapi.testclient import TestClient

from bikelog import explorer
from bikelogservice import maptracks
from bikelogservice.app import create_app
from bikelogservice.config import Settings

API = "/api/v1"
START = datetime.datetime(2026, 6, 1, 8, 0, tzinfo=datetime.timezone.utc)


def timed_gpx(lat0=50.00, lon0=8.60, km=6.0, start=START, name="Runde", step_m=25.0, kmh=20.0):
    pts = []
    n = int(km * 1000 / step_m)
    for i in range(n):
        t = start + datetime.timedelta(seconds=i * step_m / (kmh / 3.6))
        pts.append(f'<trkpt lat="{lat0 + i * step_m / 111320:.7f}" lon="{lon0:.7f}"><ele>300</ele>'
                   f'<time>{t.strftime("%Y-%m-%dT%H:%M:%SZ")}</time></trkpt>')
    return (f'<?xml version="1.0"?><gpx xmlns="http://www.topografix.com/GPX/1/1" version="1.1">'
            f'<trk><name>{name}</name><trkseg>{"".join(pts)}</trkseg></trk></gpx>').encode()


@pytest.fixture
def settings(tmp_path):
    return Settings(data_dir=tmp_path / "state", planner_url="http://brouter.test")


@pytest.fixture
def client(settings):
    with TestClient(create_app(settings)) as client:
        yield client


def _sync(client):
    app = client.app
    app.state.map.sync_rides(app.state.storage)


def test_ride_import_shows_up_on_the_map(client):
    r = client.put(f"{API}/import/gpx", content=timed_gpx())
    assert r.status_code == 200, r.text
    _sync(client)
    summary = client.get(f"{API}/map").json()
    assert summary["sources"]["ride"]["tracks"] == 1
    assert summary["explorer"]["tiles"] >= 4                 # 6 km north at 1.6 km a tile
    assert summary["squadrats"]["tiles"] >= 25
    assert summary["bounds"][0] == pytest.approx(50.00, abs=1e-3)


def test_upload_zip_and_duplicates(client):
    client.put(f"{API}/import/gpx", content=timed_gpx())
    _sync(client)
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w") as z:
        z.writestr("export/same_ride.gpx", timed_gpx())                        # the ride again
        z.writestr("export/older.gpx", timed_gpx(lon0=11.2, start=START - datetime.timedelta(days=30)))
        z.writestr("readme.txt", "x")
    r = client.put(f"{API}/map/tracks", params={"filename": "strava.zip"}, content=buf.getvalue())
    assert r.status_code == 200, r.text
    assert [x["status"] for x in r.json()] == ["new", "new"]
    tracks = client.get(f"{API}/map/tracks").json()
    dup = [t for t in tracks if t["duplicate_of"]]
    assert len(dup) == 1 and dup[0]["source"] == "upload" and dup[0]["duplicate_of"].startswith("r")
    summary = client.get(f"{API}/map").json()
    assert summary["tracks"] == 2 and summary["sources"]["upload"]["duplicates"] == 1
    # the same file again is known, not new
    again = client.put(f"{API}/map/tracks", params={"filename": "older.gpx"},
                       content=timed_gpx(lon0=11.2, start=START - datetime.timedelta(days=30)))
    assert again.json()[0]["status"] == "known"
    # rides cannot be deleted here, uploads can
    ride = next(t for t in tracks if t["source"] == "ride")
    assert client.delete(f"{API}/map/tracks/{ride['id']}").status_code == 409
    assert client.delete(f"{API}/map/tracks/{dup[0]['id']}").status_code == 204
    assert client.put(f"{API}/map/tracks", content=b"<gpx/>").status_code == 400


def test_mixed_recording_keeps_only_cycling(client):
    walk = timed_gpx(km=1.5, kmh=5, name="IanMobil")
    bike = timed_gpx(lat0=49.5, km=12, kmh=22, start=START + datetime.timedelta(hours=2), name="IanMobil")
    car = timed_gpx(lat0=49.7, km=40, kmh=95, start=START + datetime.timedelta(hours=4), name="IanMobil",
                    step_m=150)
    body = [{"name": "d1.gpx", "data": walk.decode()}, {"name": "d2.gpx", "data": bike.decode()},
            {"name": "d3.gpx", "data": car.decode()}]
    r = client.put(f"{API}/map/tracks", params={"mixed": "true", "track": "IanMobil"},
                   content=json.dumps(body), headers={"Content-Type": "application/json"})
    assert r.status_code == 200, r.text
    res = r.json()
    assert (res["counts"]["bike"], res["counts"]["walk"], res["counts"]["motor"]) == (1, 1, 1)
    assert [t["status"] for t in res["trips"] if t["kind"] == "bike"] == ["new"]
    assert client.get(f"{API}/map").json()["sources"]["upload"]["tracks"] == 1
    # the other device in the same files is ignored
    r = client.put(f"{API}/map/tracks", params={"mixed": "true", "track": "Camper"},
                   content=json.dumps(body), headers={"Content-Type": "application/json"})
    assert r.json()["counts"]["bike"] == 0


def test_tiles(client):
    client.put(f"{API}/import/gpx", content=timed_gpx())
    _sync(client)
    x, y = explorer.mercator(50.01, 8.60)
    for layer, z in (("visited", 12), ("squadrats", 15), ("heat", 13)):
        tx, ty = int(x * 2 ** z), int(y * 2 ** z)
        r = client.get(f"{API}/map/tiles/{layer}/{z}/{tx}/{ty}.png")
        assert r.status_code == 200 and r.headers["content-type"] == "image/png"
        assert r.content != explorer.EMPTY_PNG, layer
        assert client.get(f"{API}/map/tiles/{layer}/{z}/{tx}/{ty}.png",
                          headers={"If-None-Match": r.headers["etag"]}).status_code == 304
    assert client.get(f"{API}/map/tiles/heat/13/0/0.png").content == explorer.EMPTY_PNG
    assert client.get(f"{API}/map/tiles/nope/13/0/0.png").status_code == 422


class FakeKomoot:
    def __init__(self):
        self.calls = []

    def __call__(self, url, email, password, timeout=60):
        self.calls.append(url)
        if "/account/email/" in url:
            return json.dumps({"username": "4711"}).encode()
        if "/users/4711/tours/" in url:
            assert "type=tour_recorded" in url
            tours = [{"id": 1, "name": "Gravel", "sport": "mtb_easy", "date": "2026-05-01T10:00:00.000+02:00"},
                     {"id": 2, "name": "Wanderung", "sport": "hike", "date": "2026-05-02T10:00:00.000+02:00"},
                     {"id": 3, "name": "Kaputt", "sport": "racebike", "date": "2026-05-03T10:00:00.000+02:00"}]
            return json.dumps({"_embedded": {"tours": tours}, "page": {"totalPages": 1, "number": 0}}).encode()
        if url.endswith("/tours/1.gpx"):
            return timed_gpx(start=datetime.datetime(2026, 5, 1, 8, tzinfo=datetime.timezone.utc))
        if url.endswith("/tours/2.gpx"):
            return timed_gpx(lon0=11.5, start=datetime.datetime(2026, 5, 2, 8, tzinfo=datetime.timezone.utc))
        if url.endswith("/tours/3.gpx"):
            raise maptracks.KomootError("HTTP 500")
        raise AssertionError(url)


def test_komoot_import(tmp_path, monkeypatch):
    monkeypatch.setattr(maptracks, "KOMOOT_PAUSE_S", 0)
    settings = Settings(data_dir=tmp_path / "state", komoot_email="a@b.de", komoot_password="pw")
    fake = FakeKomoot()
    store = maptracks.MapStore(settings, fetch=fake)
    state = store.komoot_import()
    assert (state["new"], state["skipped"], len(state["errors"]), state["running"]) == (1, 1, 1, False)
    assert [t.name for t in store.tracks()] == ["Runde"]
    assert store.tracks()[0].sport == "mtb_easy"
    # again: nothing downloaded twice; with all sports the hike comes too
    fake.calls.clear()
    state = store.komoot_import(all_sports=True)
    assert (state["new"], state["known"]) == (1, 1)
    assert not any(c.endswith("/tours/1.gpx") for c in fake.calls)
    # survives a restart, and a new MAP_VERSION reads the originals again
    monkeypatch.setattr(maptracks, "MAP_VERSION", maptracks.MAP_VERSION + 1)
    again = maptracks.MapStore(settings, fetch=fake)
    assert len(again.tracks()) == 2 and again.summary()["tracks"] == 2


def test_map_page(client):
    client.put(f"{API}/import/gpx", content=timed_gpx())
    _sync(client)
    page = client.get("/map").text
    assert "Max Square" in page and "BikeLog Heatmap" in page and 'id="plan"' in page
    assert "Komoot ist nicht eingerichtet" in page


def test_route_page_counts_new_tiles(client):
    client.put(f"{API}/import/gpx", content=timed_gpx())
    _sync(client)
    # the same way north, and 6 km further: only the second half is new
    route = client.put(f"{API}/routes", params={"filename": "weiter.gpx"},
                       content=timed_gpx(km=12.0).replace(b"<time>", b"<!--").replace(b"</time>", b"-->"))
    assert route.status_code == 200, route.text
    page = client.get(f"/routes/{route.json()['id']}").text
    assert "Neue Kacheln" in page
    fresh = client.app.state.map.new_tiles(explorer.read_gpx(timed_gpx(km=12.0)).points)
    assert 2 <= fresh["explorer"] <= 6 and fresh["squadrats"] >= 20
