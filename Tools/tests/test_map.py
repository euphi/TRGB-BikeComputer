"""The map in the log service: tracks from rides, uploads and Komoot; overlay tiles."""

import datetime
import http.client
import io
import json
import random
import threading
import urllib.error
import zipfile
import zlib
from array import array

import pytest
from fastapi.testclient import TestClient

from bikelog import explorer
from bikelogservice import maptracks, timefilter
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


def test_fragment_does_not_hide_the_rest_of_a_tour(settings):
    store = maptracks.MapStore(settings)
    later = START + datetime.timedelta(minutes=60)
    store.add_upload(timed_gpx(km=2.0), "a.gpx")
    store.add_upload(timed_gpx(km=2.0, lat0=50.18, start=later), "b.gpx")
    store._add_file(timed_gpx(km=25.0), "komoot", "9", "Tour")
    state = store.state()
    assert len(set(state.duplicates.values())) == 1 and store.summary()["tracks"] == 1
    whole = explorer.tiles_of(explorer.runs_of(explorer.read_gpx(timed_gpx(km=25.0)).points), 14)
    assert whole <= state.stats[14].tiles


def test_recordings_of_one_ride_count_once_in_the_heatmap():
    points = [(50.00, 8.60), (50.01, 8.60)]
    a, b = explorer.make_track("a", points), explorer.make_track("b", points)
    z, (x, y) = 13, explorer.mercator(50.005, 8.60)
    tile = (z, int(x * 2 ** z), int(y * 2 ** z))
    assert explorer.heat_tile([a, b], *tile, {"b": "a"}) == explorer.heat_tile([a], *tile)


@pytest.mark.parametrize("paged", [False, True])
def test_komoot_lists_every_page(tmp_path, paged):
    def fetch(url, email, password):
        if "/account/" in url:
            return b'{"username": "1"}'
        page = int(url.split("page=")[1].split("&")[0])
        tours = [{"id": i, "sport": "hike"} for i in range(page * 100, min(page * 100 + 100, 250))]
        info = {"page": {"totalPages": 3, "totalElements": 250}} if paged else {}
        return json.dumps({"_embedded": {"tours": tours}, **info}).encode()
    settings = Settings(data_dir=tmp_path, komoot_email="a@b.de", komoot_password="pw")
    state = maptracks.MapStore(settings, fetch=fetch).komoot_import()
    assert (state["total"], state["skipped_sports"], state["errors"]) == (250, {"hike": 250}, [])


def test_komoot_broken_download_does_not_end_the_import(tmp_path, monkeypatch):
    monkeypatch.setattr(maptracks, "KOMOOT_PAUSE_S", 0)

    def fetch(url, email, password):
        if "/account/" in url:
            return b'{"username": "1"}'
        if url.endswith("/1.gpx"):
            raise http.client.IncompleteRead(b"")
        if url.endswith("/2.gpx"):
            return timed_gpx()
        tours = [{"id": i, "sport": "mtb"} for i in (1, 2)]
        return json.dumps({"_embedded": {"tours": tours}, "page": {"totalPages": 1}}).encode()
    settings = Settings(data_dir=tmp_path, komoot_email="a@b.de", komoot_password="pw")
    state = maptracks.MapStore(settings, fetch=fetch).komoot_import()
    assert (state["new"], len(state["errors"])) == (1, 1)


def test_komoot_error_does_not_show_the_address(monkeypatch):
    def fail(req, timeout):
        raise urllib.error.HTTPError(req.full_url, 500, "x", {}, None)
    monkeypatch.setattr(maptracks.urllib.request, "urlopen", fail)
    with pytest.raises(maptracks.KomootError) as err:
        maptracks._komoot_get("https://komoot.invalid/account/email/a%40b.de/", "a@b.de", "pw")
    assert "b.de" not in str(err.value)


def test_komoot_skips_foreign_and_deleted_tours(tmp_path, monkeypatch):
    monkeypatch.setattr(maptracks, "KOMOOT_PAUSE_S", 0)
    fetched = []

    def fetch(url, email, password):
        if "/account/" in url:
            return b'{"username": "4711"}'
        if url.endswith(".gpx"):
            fetched.append(url.rsplit("/", 1)[1])
            return timed_gpx()
        # Komoot also lists the tours of others one is tagged in
        tours = [{"id": 1, "sport": "mtb", "_embedded": {"creator": {"username": "4711"}}},
                 {"id": 2, "sport": "mtb", "_embedded": {"creator": {"username": "999"}}}]
        return json.dumps({"_embedded": {"tours": tours}, "page": {"totalPages": 1}}).encode()
    settings = Settings(data_dir=tmp_path, komoot_email="a@b.de", komoot_password="pw")
    store = maptracks.MapStore(settings, fetch=fetch)
    state = store.komoot_import()
    assert (state["new"], state["foreign"], fetched) == (1, 1, ["1.gpx"])
    assert store.delete("k1")
    again = maptracks.MapStore(settings, fetch=fetch)       # also after a restart
    state = again.komoot_import()
    assert (state["new"], state["deleted"], fetched) == (0, 1, ["1.gpx"])
    assert again.tracks() == []


def _tile_at(z, lat=50.01, lon=8.60):
    x, y = explorer.mercator(lat, lon)
    return z, int(x * 2 ** z), int(y * 2 ** z)


def test_zoom_layer_tiles_are_a_quarter_map_tile_wide(settings, monkeypatch):
    store = maptracks.MapStore(settings)
    store.add_upload(timed_gpx(), "a.gpx")
    state = store.state()
    runs = [run for track in state.tracks for run in track.runs]
    for z in range(3, 18):
        assert state.stats_at(z).tiles == explorer.tiles_of(runs, z), z
        tile = _tile_at(z)
        grid = min(z + 2, 17)                               # 64 px: two zooms finer than the map
        expected = explorer.visited_tile(state.stats_at(grid), *tile, faint=True)
        assert store.tile("zoom", *tile)[0] == expected, z
    tile = _tile_at(19)                                     # no finer than the squadrats
    assert store.tile("zoom", *tile)[0] == store.tile("squadrats", *tile)[0]
    monkeypatch.setattr(maptracks, "ZOOM_TILES_FINER", 0)   # other grid: browsers must not get a 304
    assert maptracks.MapStore(settings).state().revision != state.revision


def test_zoom_layer_route_and_page(client):
    client.put(f"{API}/map/tracks", content=timed_gpx())
    z, x, y = _tile_at(9)
    r = client.get(f"{API}/map/tiles/zoom/{z}/{x}/{y}.png")
    assert r.status_code == 200 and r.content != explorer.EMPTY_PNG
    assert "BikeLog Zoom-Kacheln" in client.get("/map").text


def test_stored_heat_tiles_survive_restart_and_only_follow_their_rides(settings, monkeypatch):
    store = maptracks.MapStore(settings)
    store.add_upload(timed_gpx(), "a.gpx")
    assert store.fill() > 0 and store.fill() == 0
    tile = _tile_at(13)
    png = store.state().heat.draw(store.state().heat.pieces(*tile), *tile)
    store.stop()

    draws = []
    draw = explorer.HeatIndex.draw
    monkeypatch.setattr(explorer.HeatIndex, "draw", lambda self, *a: draws.append(a[1:]) or draw(self, *a))
    again = maptracks.MapStore(settings)
    assert again.tile("heat", *tile)[0] == png and draws == []      # from the file
    assert again.fill() == 0

    again.add_upload(timed_gpx(lat0=52.0, lon0=10.0, start=START + datetime.timedelta(days=1)), "b.gpx")
    again.state()
    assert again.fill() > 0
    assert tile not in draws                                        # the ride is elsewhere
    assert _tile_at(13, 52.01, 10.0) in draws
    again.stop()


def test_heat_cache_is_only_a_cache(settings):
    path = settings.data_dir / "map" / "heat-cache.sqlite"
    path.parent.mkdir(parents=True)
    path.write_bytes(b"no database" * 1000)
    store = maptracks.MapStore(settings)
    store.add_upload(timed_gpx(), "a.gpx")
    assert store.fill() > 0                                         # rebuilt
    assert store.tile("heat", *_tile_at(12))[0] != explorer.EMPTY_PNG
    store.stop()


def test_background_thread_fills_and_stops(settings, monkeypatch):
    monkeypatch.setattr(maptracks, "QUIET_S", 0)
    store = maptracks.MapStore(settings)
    store.add_upload(timed_gpx(), "a.gpx")
    done = threading.Event()
    fill = store.fill
    monkeypatch.setattr(store, "fill", lambda state=None: (fill(state), done.set()))
    store.start()
    assert done.wait(30)
    store.stop()
    assert not store._filler.is_alive()


def test_revalidation_draws_nothing(client, monkeypatch):
    client.put(f"{API}/map/tracks", content=timed_gpx())
    z, x, y = _tile_at(16)
    url = f"{API}/map/tiles/heat/{z}/{x}/{y}.png"
    etag = client.get(url).headers["etag"]
    monkeypatch.setattr(client.app.state.map, "tile", lambda *a: pytest.fail("drawn for a 304"))
    assert client.get(url, headers={"If-None-Match": etag}).status_code == 304


def _days(*offsets):
    return [(START + datetime.timedelta(days=d)).astimezone().date() for d in offsets]


def test_period_shows_what_a_map_of_only_those_rides_would(settings, tmp_path):
    """Every layer and the figures, pixel for pixel, against a store that has only those rides."""
    old = timed_gpx(km=6.0, start=START)
    mid = timed_gpx(km=4.0, lat0=50.01, lon0=8.605, start=START + datetime.timedelta(days=40))
    new = timed_gpx(km=9.0, lat0=49.98, start=START + datetime.timedelta(days=80))
    store = maptracks.MapStore(settings)
    for i, gpx in enumerate((old, mid, new)):
        store.add_upload(gpx, f"{i}.gpx")
    d0, d40, d80 = _days(0, 40, 80)
    cases = [((None, d0), [old]), ((d40, d40), [mid]), ((d40, None), [mid, new]), ((d0, d40), [old, mid]),
             ((d0, d80), [old, mid, new])]
    for n, (period, rides) in enumerate(cases):
        only = maptracks.MapStore(Settings(data_dir=tmp_path / f"only{n}"))
        for i, gpx in enumerate(rides):
            only.add_upload(gpx, f"{i}.gpx")
        for layer in maptracks.LAYERS:
            for z in (6, 10, 12, 14, 17):
                tile = _tile_at(z)
                assert store.tile(layer, *tile, *period)[0] == only.tile(layer, *tile)[0], (period, layer, z)
        got, want = store.summary(*period), only.summary()
        for key in ("tracks", "explorer", "squadrats", "bounds", "sources"):
            assert got[key] == want[key], (period, key)
    assert store.state(d0, d80) is store.state()                    # all of it: the whole's tiles and ETag
    assert store.state(d40, d40) is not store.state() and store.state(d40, d40).revision != store.state().revision


def test_period_without_rides_is_empty(settings):
    store = maptracks.MapStore(settings)
    store.add_upload(timed_gpx(), "a.gpx")
    first, last = _days(5, 9)
    assert store.tile("heat", *_tile_at(12), first, last)[0] == explorer.EMPTY_PNG
    assert store.tile("visited", *_tile_at(9), first, last)[0] == explorer.EMPTY_PNG
    got = store.summary(first, last)
    assert (got["tracks"], got["explorer"]["tiles"], got["squadrats"]["max_square"], got["bounds"]) == (0, 0, 0, None)


def test_period_takes_a_ride_with_all_its_recordings_on_the_day_it_began(settings):
    night = datetime.datetime.combine(_days(0)[0], datetime.time(23, 50)).astimezone()
    store = maptracks.MapStore(settings)
    utc = night.astimezone(datetime.timezone.utc)
    store.add_upload(timed_gpx(km=8.0, start=utc), "tour.gpx")          # 24 min, past midnight
    # another device's part of it, begun after midnight
    store.add_upload(timed_gpx(km=2.0, lat0=50.05, start=utc + datetime.timedelta(minutes=14)), "part.gpx")
    today, tomorrow = _days(0, 1)
    assert store.state(today, today) is store.state()
    assert len(store.state(tomorrow, None).tracks) == 0


def test_track_without_time_only_belongs_to_the_whole(settings):
    store = maptracks.MapStore(settings)
    store.add_upload(timed_gpx(), "a.gpx")
    untimed = b'<gpx><trk><trkseg><trkpt lat="51" lon="9"/><trkpt lat="51.01" lon="9"/></trkseg></trk></gpx>'
    store.add_upload(untimed, "b.gpx")
    assert store.summary()["tracks"] == 2
    got = store.summary(_days(-1000)[0], None)
    assert (got["tracks"], got["undated"]) == (1, 1)


def test_period_does_not_push_out_the_stored_heat_tiles(settings, monkeypatch):
    store = maptracks.MapStore(settings)
    store.add_upload(timed_gpx(), "a.gpx")
    store.add_upload(timed_gpx(km=3.0, start=START + datetime.timedelta(days=9)), "b.gpx")
    store.fill()
    tile = _tile_at(12)
    stored = store._heat_cache.keys(12)
    assert store.tile("heat", *tile, *_days(0, 1))[0] != store.tile("heat", *tile)[0]
    assert store._heat_cache.keys(12) == stored
    assert store.fill() == 0


def test_only_a_few_periods_are_kept(settings):
    store = maptracks.MapStore(settings)
    for d in range(maptracks.PERIOD_CACHE + 3):
        store.add_upload(timed_gpx(km=1.0, start=START + datetime.timedelta(days=d)), f"{d}.gpx")
    for day in _days(*range(maptracks.PERIOD_CACHE + 3)):
        store.state(day, day)
    assert len(store.state().periods) == maptracks.PERIOD_CACHE


def test_period_routes(client):
    client.put(f"{API}/map/tracks", params={"filename": "a.gpx"}, content=timed_gpx())
    client.put(f"{API}/map/tracks", params={"filename": "b.gpx"},
               content=timed_gpx(km=3.0, start=START + datetime.timedelta(days=9)))
    z, x, y = _tile_at(12)
    url = f"{API}/map/tiles/heat/{z}/{x}/{y}.png"
    day = _days(0)[0].isoformat()
    whole, part = client.get(url), client.get(url, params={"to": day})
    assert part.status_code == 200 and part.headers["etag"] != whole.headers["etag"]
    assert part.content != whole.content
    assert client.get(url, params={"to": day}, headers={"If-None-Match": part.headers["etag"]}).status_code == 304
    assert client.get(url, params={"to": day}, headers={"If-None-Match": whole.headers["etag"]}).status_code == 200
    summary = client.get(f"{API}/map", params={"from": day, "to": day}).json()
    assert (summary["tracks"], summary["from"], summary["to"]) == (1, day, day)
    assert client.get(f"{API}/map").json()["tracks"] == 2
    assert client.get(url, params={"from": "2026-13-01"}).status_code == 422
    assert client.get(url, params={"from": "2026-07-02", "to": "2026-07-01"}).status_code == 400


def test_period_script(client):
    r = client.get(f"{API}/map/timefilter.js")
    assert r.headers["content-type"].startswith("text/javascript") and "var FIRST = null;" in r.text
    client.put(f"{API}/map/tracks", params={"filename": "a.gpx"}, content=timed_gpx(name="Geheime Runde"))
    text = client.get(f"{API}/map/timefilter.js").text
    assert 'var FIRST = "2026-06";' in text
    assert "Geheime" not in text and "8.60" not in text     # any site can load a script
    assert f'src="{API}/map/timefilter.js"' in client.get("/map").text


def _colours(png):
    """The palette entries (RGBA) a tile really uses."""
    pos, idat, plte, trns = 8, b"", b"", b""
    while pos < len(png):
        n = int.from_bytes(png[pos:pos + 4], "big")
        kind, data = png[pos + 4:pos + 8], png[pos + 8:pos + 8 + n]
        if kind == b"IDAT":
            idat += data
        elif kind == b"PLTE":
            plte = data
        elif kind == b"tRNS":
            trns = data
        pos += 12 + n
    raw = zlib.decompress(idat)
    used = set(b"".join(raw[r * 257 + 1:(r + 1) * 257] for r in range(256)))
    return {tuple(plte[i * 3:i * 3 + 3]) + (trns[i],) for i in used}


def test_distance_counts_each_ride_once(settings):
    store = maptracks.MapStore(settings)
    store.add_upload(timed_gpx(km=6.0), "a.gpx")
    store.add_upload(timed_gpx(km=5.9, name="noch einmal"), "a-again.gpx")     # the same ride
    store.add_upload(timed_gpx(km=2.0, start=START + datetime.timedelta(days=3)), "b.gpx")
    for zoom in (7, 14, 17):
        whole, part = store.state().metres_at(zoom), store.state(*_days(3, 3)).metres_at(zoom)
        assert sum(whole.values()) == pytest.approx(8000, rel=0.02), zoom   # not 14 km
        assert sum(part.values()) == pytest.approx(2000, rel=0.02), zoom    # follows the period
        assert set(whole) == store.state().stats_at(zoom).tiles


def test_distance_there_and_back_counts_twice(settings):
    out = [(50.0 + i * 25 / 111320, 8.6) for i in range(121)]     # 3 km
    track = explorer.make_track("t", out + out[::-1])
    assert sum(explorer.tile_lengths(track.runs, 14).values()) == pytest.approx(6000, rel=0.01)


def test_tile_lengths_have_the_tiles_and_the_length_of_the_track():
    rnd = random.Random(3)
    for lat0 in (0.0, 41.3, 50.0, 64.0):
        points = [(lat0, 8.6)]
        for _ in range(500):
            lat, lon = points[-1]
            step = rnd.choice((0.00005, 0.0003, 0.002, 0.008))      # within a tile and across several
            points.append((lat + rnd.uniform(-step, step), lon + rnd.uniform(-step, step)))
        runs = explorer.runs_of(points)
        for zoom in (9, 14, 17):
            lengths = explorer.tile_lengths(runs, zoom)
            assert set(lengths) == explorer.tiles_of(runs, zoom), (lat0, zoom)
            assert min(lengths.values()) >= 0
            assert sum(lengths.values()) == pytest.approx(explorer.distance_m(points), rel=0.003), (lat0, zoom)


def test_tile_lengths_split_a_segment_where_it_crosses():
    n = 2 ** 17
    y = 45000.5
    runs = [(array("d", [70000.25 / n, 70003.5 / n]), array("d", [y / n, y / n]))]
    lengths = explorer.tile_lengths(runs, 17)
    width = explorer.metres_per_unit(y / n) / n
    want = {70000: 0.75, 70001: 1, 70002: 1, 70003: 0.5}
    assert lengths == pytest.approx({(x, 45000): share * width for x, share in want.items()}, rel=1e-6)


def test_km_steps_fit_the_grid():
    assert explorer.km_steps(17) == (0.5, 1, 2, 5, 10)      # not 0.2: that is a tile's width
    assert explorer.km_steps(14) == (2, 5, 10, 20, 50)
    assert explorer.km_steps(7) == (200, 500, 1000, 2000, 5000)
    for zoom in range(3, 18):
        steps = explorer.km_steps(zoom)
        assert len(steps) == 5 and list(steps) == sorted(set(steps)), zoom
        assert all(f"{v:g}".strip("0.") in ("1", "2", "5") or f"{v:g}" in ("1e+06",) for v in steps), zoom


def test_distance_colour_steps():
    stats = explorer.tile_stats({(8000, 5000)}, 14)
    for km, step in ((0, 1), (1.9, 1), (2, 2), (9.9, 3), (10, 4), (49, 5), (50, 6), (9000, 6)):
        metres = {(8000, 5000): km * 1000}
        png = explorer.visited_tile(stats, 14, 8000, 5000, metres=metres)
        assert explorer.DISTANCE_PALETTE[step] in _colours(png), km
        png = explorer.visited_tile(stats, 14, 8000, 5000, metres=metres, dark=True)
        assert explorer.DISTANCE_PALETTE_DARK[step] in _colours(png), km


def test_cluster_and_max_cluster_look_different():
    for palette in (explorer.VISITED_PALETTE, explorer.VISITED_PALETTE_CLOSE):
        for base in ((242, 239, 233), (173, 209, 158), (170, 211, 223), (14, 14, 16)):
            seen = [[b + (c - b) * colour[3] / 255 for b, c in zip(base, colour)] for colour in palette[2:4]]
            light = [0.2126 * r + 0.7152 * g + 0.0722 * b for r, g, b in seen]
            assert abs(light[0] - light[1]) > 20, base          # of 255; was under 10 in the faint one


def test_distance_colour_has_no_cluster_and_no_square(settings):
    store = maptracks.MapStore(settings)
    for i in range(4):
        store.add_upload(timed_gpx(km=9.0, lon0=8.60 + i * 0.022, start=START + datetime.timedelta(days=i)), f"{i}.gpx")
    marks = {c[:3] for c in (explorer.VISITED_PALETTE[i] for i in (1, 2, 3, 5))}
    for layer, z in (("visited", 12), ("squadrats", 15), ("zoom", 11)):
        tile = _tile_at(z, 50.03, 8.62)
        assert marks & {c[:3] for c in _colours(store.tile(layer, *tile)[0])}, layer
        assert not marks & {c[:3] for c in _colours(store.tile(layer, *tile, colour="distance")[0])}, layer
    tile = _tile_at(11)
    assert store.tile("heat", *tile, colour="distance")[0] == store.tile("heat", *tile)[0]


def test_colour_route_and_script(client):
    client.put(f"{API}/map/tracks", params={"filename": "a.gpx"}, content=timed_gpx())
    z, x, y = _tile_at(12)
    url = f"{API}/map/tiles/visited/{z}/{x}/{y}.png"
    cluster = client.get(url)
    assert client.get(url, params={"colour": "cluster"}).content == cluster.content
    km, dark = (client.get(url, params={"colour": c}) for c in ("distance", "distance-dark"))
    assert len({cluster.content, km.content, dark.content}) == 3
    assert client.get(url, params={"colour": "pink"}).status_code == 422
    script = client.get(f"{API}/map/timefilter.js").text
    assert "__LEGEND__" not in script and "__SCALE__" not in script
    assert timefilter.legends()["distance"][0] in script
    assert 'data-legend="distance"' in client.get("/map").text
