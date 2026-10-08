"""Nextcloud sync: uploading Tours, removing stale copies -- against a fake
`requests` module so the tests never touch the network."""

from __future__ import annotations

import sys
import types

import pytest
from fastapi.testclient import TestClient

from bikelog import fixtures
from bikelog.record import write_records
from bikelogservice import nextcloud as nextcloud_module
from bikelogservice.app import create_app
from bikelogservice.config import Settings
from bikelogservice.storage import Storage

API = "/api/v1"


class _FakeResponse:
    def __init__(self, status_code):
        self.status_code = status_code

    def raise_for_status(self):
        if self.status_code >= 400:
            raise RuntimeError(f"HTTP {self.status_code}")


class _FakeHttp:
    calls: list[tuple] = []
    mkcol_status = 201
    put_status = 201
    delete_status = 204

    def __init__(self):
        self.auth = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False

    def request(self, method, url):
        _FakeHttp.calls.append((method, url))
        return _FakeResponse(_FakeHttp.mkcol_status)

    def put(self, url, data=None):
        if hasattr(data, "read"):
            data.read()
        _FakeHttp.calls.append(("PUT", url))
        return _FakeResponse(_FakeHttp.put_status)

    def delete(self, url):
        _FakeHttp.calls.append(("DELETE", url))
        return _FakeResponse(_FakeHttp.delete_status)


@pytest.fixture
def fake_requests(monkeypatch):
    _FakeHttp.calls = []
    _FakeHttp.mkcol_status = 201
    _FakeHttp.put_status = 201
    _FakeHttp.delete_status = 204
    monkeypatch.setitem(sys.modules, "requests", types.SimpleNamespace(Session=_FakeHttp))
    return _FakeHttp


@pytest.fixture
def settings(tmp_path):
    return Settings(data_dir=tmp_path / "state", nextcloud_url="https://cloud.example.com",
                    nextcloud_user="bikelog", nextcloud_password="app-pw",
                    nextcloud_dir="Rides/BikeLog")


class _NoThread:
    """The app syncs in a background thread after every upload; these tests call
    sync_pending() themselves and count what it did -- with the thread racing them the
    count would depend on who came first."""

    def __init__(self, *args, **kwargs):
        pass

    def start(self):
        pass


@pytest.fixture
def client(settings, monkeypatch):
    import bikelogservice.app as app_module
    # only the app module's name "threading" -- the real module stays as it is
    monkeypatch.setattr(app_module, "threading", types.SimpleNamespace(Thread=_NoThread))
    with TestClient(create_app(settings)) as client:
        yield client


def _put(client, payload, path, device="gravel"):
    return client.put(f"{API}/devices/{device}/files/{path}", content=payload)


def _ride_bytes(tmp_path, name, **kwargs):
    path = tmp_path / name
    write_records(path, fixtures.synthetic(no_fix_start_s=0, pause=None, tunnel=None, **kwargs))
    return path.read_bytes()


def test_real_ride_gets_synced(client, tmp_path, fake_requests):
    payload = _ride_bytes(tmp_path, "ride.bin", seconds=200)      # ~1.3 km -> Tours
    sid = _put(client, payload, "20260920/L_090000.bin").json()["session"]["id"]
    store = client.app.state.storage

    counts = nextcloud_module.sync_pending(store)
    assert counts == {"ok": 1}

    session = store.get(sid)
    assert session.nextcloud_status == "ok"
    name = session.gpx_file.split("/")[-1]
    assert session.nextcloud_file == name
    assert ("PUT", f"https://cloud.example.com/remote.php/dav/files/bikelog/Rides/BikeLog/{name}") \
        in fake_requests.calls
    assert ("MKCOL", "https://cloud.example.com/remote.php/dav/files/bikelog/Rides") in fake_requests.calls
    assert ("MKCOL", "https://cloud.example.com/remote.php/dav/files/bikelog/Rides/BikeLog") \
        in fake_requests.calls


def test_short_ride_is_never_synced(client, tmp_path, fake_requests):
    payload = _ride_bytes(tmp_path, "ride.bin", seconds=60)       # ~400 m -> Debug_Archive
    _put(client, payload, "20260920/L_090000.bin")
    assert nextcloud_module.sync_pending(client.app.state.storage) == {}
    assert not fake_requests.calls


def test_disabled_without_credentials(tmp_path, fake_requests):
    store = Storage(Settings(data_dir=tmp_path / "off"))
    assert nextcloud_module.sync_pending(store) == {}
    assert not fake_requests.calls


def test_unchanged_export_is_not_reuploaded(client, tmp_path, fake_requests):
    payload = _ride_bytes(tmp_path, "ride.bin", seconds=200)
    _put(client, payload, "20260920/L_090000.bin")
    store = client.app.state.storage
    assert nextcloud_module.sync_pending(store) == {"ok": 1}
    assert nextcloud_module.sync_pending(store) == {}           # nothing changed, nothing to do


def test_upload_failure_is_retried_next_time(client, tmp_path, fake_requests):
    fake_requests.put_status = 500
    payload = _ride_bytes(tmp_path, "ride.bin", seconds=200)
    sid = _put(client, payload, "20260920/L_090000.bin").json()["session"]["id"]
    store = client.app.state.storage
    assert nextcloud_module.sync_pending(store) == {"error": 1}
    assert store.get(sid).nextcloud_status == "error"

    fake_requests.put_status = 201
    assert nextcloud_module.sync_pending(store) == {"ok": 1}


def test_reclassified_session_is_removed_from_nextcloud(client, tmp_path, fake_requests):
    payload = _ride_bytes(tmp_path, "ride.bin", seconds=200)
    sid = _put(client, payload, "20260920/L_090000.bin").json()["session"]["id"]
    store = client.app.state.storage
    nextcloud_module.sync_pending(store)
    assert store.get(sid).nextcloud_status == "ok"

    # Simulate an export-logic change reclassifying this session as not a
    # real ride any more (exporter.py would also remove/replace the local file).
    session = store.get(sid)
    store.set_export(sid, None, "debug", session.gpx_version)
    fake_requests.calls.clear()

    counts = nextcloud_module.sync_pending(store)
    assert counts == {"removed": 1}
    assert [m for m, _ in fake_requests.calls] == ["DELETE"]
    session = store.get(sid)
    assert session.nextcloud_status is None
    assert session.nextcloud_file is None
