"""Grouping sessions into merged tours, and the upload endpoint -- against a
fake kompy so the tests never touch the network."""

from __future__ import annotations

import sys
import types

import pytest
from fastapi.testclient import TestClient

from bikelog import fixtures
from bikelog.record import Record, write_records
from bikelogservice import komoot as komoot_module
from bikelogservice.app import create_app
from bikelogservice.config import Settings
from bikelogservice.storage import Session

API = "/api/v1"


def _session(id_, first_time, last_time, device="gravel") -> Session:
    return Session(id=id_, device=device, day="20260920", stem=f"_{id_}",
                   created_at="", updated_at="", deleted_at=None, summary=None,
                   format_version=2, record_count=10, trailing_bytes=0,
                   first_time=first_time, last_time=last_time, distance_m=100.0,
                   gps_points=5, clock_unset=0, log_error=None)


# --- grouping (pure, no storage/network) --------------------------------

def test_short_gap_merges_long_gap_does_not():
    a = _session(1, 1000, 1100)
    b = _session(2, 1150, 1300)              # 50 s gap: merges with a
    c = _session(3, 3000, 3100)              # 1700 s gap: separate tour
    groups = komoot_module.group_into_tours([a, b, c], merge_gap_s=300)
    assert [[s.id for s in g] for g in groups] == [[1, 2], [3]]


def test_different_devices_never_merge():
    a = _session(1, 1000, 1100, device="gravel")
    b = _session(2, 1150, 1300, device="tourer")
    groups = komoot_module.group_into_tours([a, b], merge_gap_s=300)
    assert [[s.id for s in g] for g in groups] == [[1], [2]]


def test_sessions_with_unknown_times_never_merge():
    a = _session(1, None, None)
    b = _session(2, 1000, 1100)
    groups = komoot_module.group_into_tours([a, b], merge_gap_s=300)
    assert [[s.id for s in g] for g in groups] == [[1], [2]]


# --- upload, via the service, against a fake kompy -----------------------

class _FakeConnector:
    calls: list[dict] = []

    def __init__(self, email, password):
        self.email, self.password = email, password

    def upload_tour(self, **kwargs):
        _FakeConnector.calls.append(kwargs)
        return True


@pytest.fixture
def fake_kompy(monkeypatch):
    _FakeConnector.calls = []
    monkeypatch.setitem(sys.modules, "kompy",
                        types.SimpleNamespace(KomootConnector=_FakeConnector))
    monkeypatch.setitem(sys.modules, "gpxpy",
                        types.SimpleNamespace(parse=lambda xml: xml))
    return _FakeConnector


@pytest.fixture
def settings(tmp_path):
    return Settings(data_dir=tmp_path / "state",
                    komoot_email="me@example.com", komoot_password="secret")


@pytest.fixture
def client(settings):
    with TestClient(create_app(settings)) as client:
        yield client


def _put(client, payload, path, device="gravel"):
    return client.put(f"{API}/devices/{device}/files/{path}", content=payload)


def _ride_bytes(tmp_path, name, **kwargs):
    path = tmp_path / name
    write_records(path, fixtures.synthetic(no_fix_start_s=0, pause=None, tunnel=None, **kwargs))
    return path.read_bytes()


def test_upload_a_real_ride(client, tmp_path, fake_kompy):
    payload = _ride_bytes(tmp_path, "ride.bin", seconds=200)     # ~1.3 km, one session
    sid = _put(client, payload, "20260920/L_090000.bin").json()["session"]["id"]

    resp = client.post(f"{API}/sessions/{sid}/komoot")
    assert resp.status_code == 200
    assert resp.json() == {"status": "uploaded", "session_ids": [sid]}
    assert fake_kompy.calls[0]["tour_name"].startswith("Fahrt ")
    assert fake_kompy.calls[0]["activity_type"] == "touringbicycle"
    assert client.get(f"{API}/sessions/{sid}").json()["komoot_status"] == "uploaded"


def test_uploaded_gpx_has_no_waypoints(client, tmp_path, fake_kompy):
    """Komoot's import endpoint rejects a GPX that has any <wpt> at all (see
    komoot.render_tour's docstring) -- shocks and labels must not appear as
    waypoints in what gets uploaded, even though they do in a normal export."""
    from bikelog import fixtures as bikelog_fixtures
    records = bikelog_fixtures.with_road_quality(
        bikelog_fixtures.synthetic(no_fix_start_s=0, pause=None, tunnel=None, seconds=300))
    path = tmp_path / "rich.bin"
    write_records(path, records)
    sid = _put(client, path.read_bytes(), "20260920/L_090000.bin").json()["session"]["id"]

    resp = client.post(f"{API}/sessions/{sid}/komoot")
    assert resp.status_code == 200
    tour_object = fake_kompy.calls[0]["tour_object"]     # our fake gpxpy.parse is the identity
    assert "<wpt" not in tour_object
    assert "<trkpt" in tour_object                       # sanity: the track itself is still there


def test_second_upload_without_force_is_a_conflict(client, tmp_path, fake_kompy):
    payload = _ride_bytes(tmp_path, "ride.bin", seconds=200)
    sid = _put(client, payload, "20260920/L_090000.bin").json()["session"]["id"]
    client.post(f"{API}/sessions/{sid}/komoot")

    again = client.post(f"{API}/sessions/{sid}/komoot")
    assert again.status_code == 409
    assert len(fake_kompy.calls) == 1

    forced = client.post(f"{API}/sessions/{sid}/komoot", params={"force": True})
    assert forced.status_code == 200
    assert len(fake_kompy.calls) == 2


def test_upload_without_credentials_is_a_conflict(tmp_path, fake_kompy):
    off = Settings(data_dir=tmp_path / "off")
    with TestClient(create_app(off)) as client:
        payload = _ride_bytes(tmp_path, "ride.bin", seconds=200)
        sid = _put(client, payload, "20260920/L_090000.bin").json()["session"]["id"]
        resp = client.post(f"{API}/sessions/{sid}/komoot")
    assert resp.status_code == 409
    assert not fake_kompy.calls


def test_short_ride_is_a_conflict_not_uploaded(client, tmp_path, fake_kompy):
    payload = _ride_bytes(tmp_path, "ride.bin", seconds=60)      # ~400 m
    sid = _put(client, payload, "20260920/L_090000.bin").json()["session"]["id"]
    resp = client.post(f"{API}/sessions/{sid}/komoot")
    assert resp.status_code == 409
    assert not fake_kompy.calls


def test_sessions_a_short_pause_apart_upload_as_one_tour(client, tmp_path, fake_kompy):
    t0 = 1758983000
    first = _ride_bytes(tmp_path, "a.bin", seconds=100, start_time=t0)              # ~660 m
    second = _ride_bytes(tmp_path, "b.bin", seconds=100, start_time=t0 + 99 + 120)   # 120 s later
    sid_a = _put(client, first, "20260920/L_090000.bin").json()["session"]["id"]
    sid_b = _put(client, second, "20260920/L_093000.bin").json()["session"]["id"]

    resp = client.post(f"{API}/sessions/{sid_a}/komoot")
    assert resp.status_code == 200
    assert sorted(resp.json()["session_ids"]) == sorted([sid_a, sid_b])
    assert len(fake_kompy.calls) == 1
    assert client.get(f"{API}/sessions/{sid_b}").json()["komoot_status"] == "uploaded"


def test_rebased_records_continue_distance_instead_of_resetting(client, tmp_path):
    t0 = 1758983000
    first = _ride_bytes(tmp_path, "a.bin", seconds=50, start_time=t0)
    second = _ride_bytes(tmp_path, "b.bin", seconds=50, start_time=t0 + 49 + 10)
    sid_a = _put(client, first, "20260920/L_090000.bin").json()["session"]["id"]
    sid_b = _put(client, second, "20260920/L_090200.bin").json()["session"]["id"]
    store = client.app.state.storage
    session_a, session_b = store.get(sid_a), store.get(sid_b)

    a_last_distance = [r for r in store.records(session_a, types=None)
                       if isinstance(r, Record)][-1].distance
    combined = komoot_module._rebased_records(store, [session_a, session_b])
    ride_records = [r for r in combined if isinstance(r, Record)]
    # without the rebase, session b's records would restart near distance 0
    assert ride_records[-1].distance > a_last_distance * 1.5


# --- the standing question "upload to Komoot?" ------------------------------

def _page(client):
    return client.get("/").text


def test_a_new_ride_is_asked_about_on_every_page_view_until_answered(client, tmp_path, fake_kompy):
    payload = _ride_bytes(tmp_path, "ride.bin", seconds=200)
    sid = _put(client, payload, "20260920/L_090000.bin").json()["session"]["id"]

    first = _page(client)
    assert "Neue Fahrt bereit" in first and "Zu Komoot hochladen" in first
    assert f"/ui/sessions/{sid}/komoot-ignore" in first
    # closing and reopening the page answers nothing
    assert "Neue Fahrt bereit" in _page(client)
    assert not fake_kompy.calls


def test_ignoring_ends_the_question_for_good(client, tmp_path, fake_kompy):
    payload = _ride_bytes(tmp_path, "ride.bin", seconds=200)
    sid = _put(client, payload, "20260920/L_090000.bin").json()["session"]["id"]

    resp = client.post(f"/ui/sessions/{sid}/komoot-ignore", follow_redirects=False)
    assert resp.status_code == 303 and resp.headers["location"].startswith("/?msg=")
    assert "Neue Fahrt bereit" not in _page(client)
    assert client.get(f"{API}/sessions/{sid}").json()["komoot_prompt"] == "ignored"
    assert not fake_kompy.calls
    # the table's own Komoot button is still there for a change of mind
    assert f"/ui/sessions/{sid}/komoot" in _page(client)
    # and the API can put the question back
    client.post(f"{API}/sessions/{sid}/komoot/ignore", params={"ask_again": True})
    assert "Neue Fahrt bereit" in _page(client)


def test_accepting_uploads_and_ends_the_question(client, tmp_path, fake_kompy):
    payload = _ride_bytes(tmp_path, "ride.bin", seconds=200)
    sid = _put(client, payload, "20260920/L_090000.bin").json()["session"]["id"]

    resp = client.post(f"/ui/sessions/{sid}/komoot", follow_redirects=False)
    assert resp.status_code == 303
    assert len(fake_kompy.calls) == 1
    assert "Neue Fahrt bereit" not in _page(client)
    assert client.get(f"{API}/sessions/{sid}").json()["komoot_status"] == "uploaded"


def test_a_failed_upload_keeps_asking(client, tmp_path, fake_kompy, monkeypatch):
    monkeypatch.setattr(fake_kompy, "upload_tour", lambda self, **kw: False)
    payload = _ride_bytes(tmp_path, "ride.bin", seconds=200)
    sid = _put(client, payload, "20260920/L_090000.bin").json()["session"]["id"]
    client.post(f"/ui/sessions/{sid}/komoot")
    assert "Neue Fahrt bereit" in _page(client)


def test_merged_sessions_are_one_question(client, tmp_path, fake_kompy):
    t0 = 1758983000
    first = _ride_bytes(tmp_path, "a.bin", seconds=200, start_time=t0)
    second = _ride_bytes(tmp_path, "b.bin", seconds=200, start_time=t0 + 199 + 120)
    sid_a = _put(client, first, "20260920/L_090000.bin").json()["session"]["id"]
    sid_b = _put(client, second, "20260920/L_093000.bin").json()["session"]["id"]
    assert _page(client).count("Zu Komoot hochladen</button>") == 1
    client.post(f"/ui/sessions/{sid_a}/komoot-ignore")
    store = client.app.state.storage
    assert store.get(sid_b).komoot_prompt == "ignored"


def test_short_rides_and_missing_credentials_are_not_asked_about(tmp_path, fake_kompy):
    short = _ride_bytes(tmp_path, "short.bin", seconds=60)
    with TestClient(create_app(Settings(data_dir=tmp_path / "a", komoot_email="x@y.z", komoot_password="p"))) as c:
        _put(c, short, "20260920/L_090000.bin")
        assert "Neue Fahrt bereit" not in c.get("/").text
    real = _ride_bytes(tmp_path, "real.bin", seconds=200)
    with TestClient(create_app(Settings(data_dir=tmp_path / "b"))) as c:
        _put(c, real, "20260920/L_090000.bin")
        assert "Neue Fahrt bereit" not in c.get("/").text


def test_upgrade_does_not_ask_about_existing_sessions(tmp_path):
    from bikelogservice.storage import Storage
    settings = Settings(data_dir=tmp_path / "up", komoot_email="x@y.z", komoot_password="p")
    store = Storage(settings)
    store.put_file("gravel", __import__("bikelogservice.sdlayout", fromlist=["x"]).parse_path("20260920/L_090000.bin"),
                   _ride_bytes(tmp_path, "r.bin", seconds=200))
    store._db.execute("ALTER TABLE sessions DROP COLUMN komoot_prompt")
    store._db.execute("PRAGMA user_version = 6")
    store._db.commit()
    store.close()
    store = Storage(settings)
    assert [s.komoot_prompt for s in store.list()] == ["ignored"]
    store.close()
