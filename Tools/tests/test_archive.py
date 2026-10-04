"""Idle sessions: hidden, no GPX, archived after a while, tombstones dropped once the
files left the SD card, and deleted on the device right away while it is reachable."""

import datetime
import json

import pytest
from fastapi.testclient import TestClient

from bikelog import fixtures, testride
from bikelog.record import write_records
from bikelogservice import archive, exporter
from bikelogservice.app import create_app
from bikelogservice.config import Settings
from bikelogservice.puller import DeviceUnavailable, HttpError, Puller, sync
from bikelogservice.sdlayout import parse_path
from bikelogservice.storage import Storage

API = "/api/v1"


def idle_records(seconds=900):
    """Switched on at home: wheel stands, the phone's GPS jitters around one spot."""
    records = fixtures.synthetic(seconds=seconds, pause=None, tunnel=None, no_fix_start_s=0)
    lat0, lon0 = records[0].gps_lat_e7, records[0].gps_lon_e7
    for i, r in enumerate(records):
        r.speed, r.distance = 0.0, 0.0
        r.gps_lat_e7 = lat0 + (i % 7 - 3) * 30          # +-30 cm steps around one spot
        r.gps_lon_e7 = lon0 + (i % 5 - 2) * 40
    return records


def ride_records():
    return fixtures.synthetic(seconds=600, pause=None, tunnel=None, no_fix_start_s=0)


def _bytes(tmp_path, records, name):
    path = tmp_path / name
    write_records(path, records)
    return path.read_bytes()


def test_idle_detection():
    assert testride.idle(idle_records())
    assert testride.idle([])
    assert not testride.idle(ride_records())
    # hours of jitter add up to a long path, but no extent: neither a test nor a ride
    info = testride.classify(idle_records(seconds=3600))
    assert info.kind is None and info.extent_m < 50


class FakeBC:
    """The device for the puller: a listing, the files, and /del/."""

    def __init__(self, files: dict[str, bytes], refuse=()):
        self.files = dict(files)
        self.refuse = set(refuse)
        self.deleted: list[str] = []

    def get(self, url: str) -> bytes:
        path = url.split("//", 1)[1].split("/", 1)[1]
        if path == "logfiles.json":
            return json.dumps({"files": [{"path": p, "size": len(b)} for p, b in self.files.items()]}).encode()
        if path.startswith("log/"):
            return self.files[path[4:]]
        if path.startswith("del/"):
            rel = path[4:]
            if rel in self.refuse or rel not in self.files:
                raise HttpError(url, 403, "Forbidden")
            del self.files[rel]
            self.deleted.append(rel)
            return b"<p>OK</p>"
        raise HttpError(url, 404, "Not Found")


@pytest.fixture
def setup(tmp_path):
    settings = Settings(data_dir=tmp_path / "state")
    store = Storage(settings)
    bc = FakeBC({
        "20260920/L_090000.bin": _bytes(tmp_path, ride_records(), "r.bin"),
        "20260920/L_100000.bin": _bytes(tmp_path, idle_records(), "i.bin"),
        "20260920/D_100000.log": b"debug\n",
        "20260920/D_110000.log": b"only debug\n",
    })
    yield settings, store, bc
    store.close()


def _sid(store, stem):
    return next(s.id for s in store.list(limit=100) if s.stem == stem)


def test_idle_sessions_are_hidden_and_get_no_gpx(setup):
    settings, store, bc = setup
    sync(store, "trgb", "http://bc", bc, pause_s=0)
    exporter.export_pending(store)
    assert [s.stem for s in store.list(idle=False)] == ["_090000"]
    assert {s.stem for s in store.idle_sessions()} == {"_100000", "_110000"}
    idle = store.get(_sid(store, "_100000"))
    assert idle.idle and idle.gpx_status == "idle" and idle.gpx_file is None


def test_archive_after_the_grace_period_and_drop_tombstone_when_gone_from_card(setup):
    settings, store, bc = setup
    sync(store, "trgb", "http://bc", bc, pause_s=0)
    sid = _sid(store, "_100000")
    assert archive.archive_idle(store) == 0                       # too fresh
    later = datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(days=8)
    assert archive.archive_idle(store, now=later) == 2
    assert (settings.archive_dir / "trgb" / "20260920" / "L_100000.bin").exists()
    assert not (settings.sessions_dir / "trgb" / "20260920" / "L_100000.bin").exists()
    assert store.get(sid) is None and {s.id for s in store.archived_sessions()} >= {sid}
    # still on the card: the next pull must not fetch it again
    result = sync(store, "trgb", "http://bc", bc, pause_s=0)
    assert result.fetched == 0 and result.purged == 0
    # gone from the card: the tombstone goes too
    for p in ("20260920/L_100000.bin", "20260920/D_100000.log"):
        del bc.files[p]
    result = sync(store, "trgb", "http://bc", bc, pause_s=0)
    assert result.purged == 1
    assert sid not in {s.id for s in store.archived_sessions()}
    assert (settings.archive_dir / "trgb" / "20260920" / "L_100000.bin").exists()   # archive stays


def test_empty_listing_drops_nothing(setup):
    settings, store, bc = setup
    sync(store, "trgb", "http://bc", bc, pause_s=0)
    later = datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(days=8)
    archive.archive_idle(store, now=later)
    bc.files.clear()
    assert sync(store, "trgb", "http://bc", bc, pause_s=0).purged == 0


def test_delete_on_device(setup):
    settings, store, bc = setup
    sync(store, "trgb", "http://bc", bc, pause_s=0)
    p = Puller(store, Settings(data_dir=settings.data_dir, pull_targets=["trgb=TRGB-BC"]), http=bc,
               resolver=lambda host: "10.0.0.9")
    done, errors = archive.delete_on_device(store, p, store.idle_sessions())
    assert done == 2 and not errors
    assert sorted(bc.deleted) == ["20260920/D_100000.log", "20260920/D_110000.log", "20260920/L_100000.bin"]
    assert store.idle_sessions() == [] and store.archived_sessions() == []
    assert (settings.archive_dir / "trgb" / "20260920" / "D_110000.log").exists()
    assert [s.stem for s in store.list()] == ["_090000"]          # the ride is untouched


def test_delete_on_device_refused_or_offline(setup):
    settings, store, bc = setup
    sync(store, "trgb", "http://bc", bc, pause_s=0)
    bc.refuse.add("20260920/D_110000.log")
    p = Puller(store, Settings(data_dir=settings.data_dir), http=bc, resolver=lambda host: "10.0.0.9")
    done, errors = archive.delete_on_device(store, p, store.idle_sessions())
    assert done == 1 and "D_110000.log" in errors[0]
    assert [s.stem for s in store.idle_sessions()] == ["_110000"]  # stays, nothing queued
    offline = Puller(store, Settings(data_dir=settings.data_dir), http=bc, resolver=lambda host: None)
    with pytest.raises(DeviceUnavailable):
        archive.delete_on_device(store, offline, store.idle_sessions())


def test_pages(tmp_path):
    settings = Settings(data_dir=tmp_path / "state")
    with TestClient(create_app(settings)) as client:
        client.put(f"{API}/devices/trgb/files/20260920/L_100000.bin",
                   content=_bytes(tmp_path, idle_records(), "i.bin"))
        client.put(f"{API}/devices/trgb/files/20260920/L_090000.bin",
                   content=_bytes(tmp_path, ride_records(), "r.bin"))
        page = client.get("/").text
        assert "1 Leerlauf-Sitzung" in page and 'href="/archive"' in page
        assert "L_100000" not in page
        # empty filter fields are "no bound", not a 422
        assert client.get("/?min_km=&max_km=&tests=true").status_code == 200
        assert client.get("/?min_km=abc").status_code == 400
        arch = client.get("/archive").text
        assert "Leerlauf, noch nicht archiviert (1)" in arch and "L_100000.bin" in arch
        assert "Abholen ist aus" in arch
        # without a puller nothing can be deleted on the device
        r = client.post("/ui/archive/device-delete", data={}, follow_redirects=False)
        assert r.status_code == 303
        assert "Nichts gelöscht" in client.get(r.headers["location"]).text
        assert client.post(f"{API}/archive/device-delete").status_code == 409
        assert client.post(f"{API}/archive/device-delete", params={"session_id": 999}).status_code == 404
        assert client.get(f"{API}/sessions", params={"idle": True}).json()["total"] == 2


def test_migration_marks_old_sessions(tmp_path):
    import sqlite3
    settings = Settings(data_dir=tmp_path / "state")
    store = Storage(settings)
    store.put_file("trgb", parse_path("20260920/L_100000.bin"), _bytes(tmp_path, idle_records(), "i.bin"))
    store.put_file("trgb", parse_path("20260920/L_090000.bin"), _bytes(tmp_path, ride_records(), "r.bin"))
    store.close()
    db = sqlite3.connect(settings.db_path)
    for col in ("idle", "archived_at"):
        db.execute(f"ALTER TABLE sessions DROP COLUMN {col}")
    db.execute("PRAGMA user_version = 8")
    db.commit()
    db.close()
    store = Storage(settings)
    assert {s.stem: s.idle for s in store.list()} == {"_100000": 1, "_090000": 0}
    store.close()
