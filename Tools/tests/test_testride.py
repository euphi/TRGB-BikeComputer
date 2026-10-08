"""Test sessions (simulator, GPS playback): detection, report, and what the log service does."""

import pytest
from fastapi.testclient import TestClient

from bikelog import fixtures, report, testride, training
from bikelog.record import LOG_SIMULATED, RideStateRecord, RSF_SIMULATED, write_records
from bikelogservice.app import create_app
from bikelogservice.config import Settings

API = "/api/v1"


def ride():
    return fixtures.synthetic(seconds=600, pause=None, tunnel=None, no_fix_start_s=0)


def simulated():
    records = ride()
    for r in records:
        r.gps_flags |= LOG_SIMULATED
    return records


def playback():
    """GPS travels, the wheel stands: the phone played a GPX on the desk."""
    records = ride()
    for r in records:
        r.speed, r.distance = 0.0, 0.0
    return records


def test_a_real_ride_is_no_test():
    info = testride.classify(ride())
    assert info.kind is None and info.gps_m > 3000


def test_simulator_flag():
    info = testride.classify(simulated())
    assert info.kind == testride.KIND_SIM and info.simulated_share == 1.0


def test_simulator_flag_in_a_ride_state_record():
    records = ride() + [RideStateRecord(timestamp=ride()[-1].timestamp, flags=RSF_SIMULATED)]
    assert testride.classify(records).kind == testride.KIND_SIM


def test_gps_playback_is_suspected():
    info = testride.classify(playback())
    assert info.kind == testride.KIND_GPS_PLAYBACK
    assert "TrailBridge-Testfahrt" in info.reason


def test_a_short_wobble_without_wheel_is_no_playback():
    records = ride()[:60]                   # ~400 m of GPS
    for r in records:
        r.speed, r.distance = 0.0, 0.0
    assert testride.classify(records).kind is None


def test_report_and_training_leave_tests_out():
    rep = report.compute(playback())
    assert rep["meta"]["test"]["kind"] == testride.KIND_GPS_PLAYBACK
    assert any(f["code"] == "test_gps_playback" for f in rep["health"]["findings"])
    assert "**Testfahrt**" in report.to_markdown(rep)
    sim = report.compute(simulated())
    assert training.Ride.from_report(sim) is None
    assert training.Ride.from_report(sim, allow_test=True) is not None
    assert training.Ride.from_report(report.compute(ride())) is not None


@pytest.fixture
def client(tmp_path):
    with TestClient(create_app(Settings(data_dir=tmp_path / "state"))) as client:
        yield client


def _put(client, tmp_path, records, stem):
    path = tmp_path / f"{stem}.bin"
    write_records(path, records)
    return client.put(f"{API}/devices/gravel/files/20260517/L_{stem}.bin",
                      content=path.read_bytes()).json()["session"]


def test_service_hides_marks_and_archives_tests(client, tmp_path):
    real = _put(client, tmp_path, ride(), "090000")
    sim = _put(client, tmp_path, simulated(), "100000")
    play = _put(client, tmp_path, playback(), "110000")
    assert (real["is_test"], sim["test_kind"], play["test_kind"]) == (False, "sim", "gps_playback")

    # hidden by default, list and API
    page = client.get("/").text
    assert f'id="s{real["id"]}"' in page and f'id="s{sim["id"]}"' not in page
    assert "2 Testfahrten ausgeblendet" in page
    shown = client.get("/", params={"tests": True}).text
    assert f'id="s{sim["id"]}"' in shown and "Simuliert" in shown and "GPS-Wiedergabe?" in shown
    assert client.get(f"{API}/sessions").json()["total"] == 1
    assert client.get(f"{API}/sessions", params={"tests": True}).json()["total"] == 3

    # never in Tours (so never in Nextcloud or the Komoot question)
    store = client.app.state.storage
    assert store.get(real["id"]).gpx_status == "ok"
    assert store.get(sim["id"]).gpx_status == "test"
    assert store.get(sim["id"]).gpx_file.startswith("Debug_Archive/")

    # the ride page says why and lets the rider overrule it
    html = client.get(f"/ride/{play['id']}").text
    assert "Testfahrt" in html and "Doch eine echte Fahrt" in html
    r = client.post(f"/ui/sessions/{play['id']}/test", data={"mark": "real"}, follow_redirects=False)
    assert r.status_code == 303
    assert store.get(play["id"]).is_test is False
    assert store.get(play["id"]).gpx_status == "ok"
    assert "Von dir als echte Fahrt bestätigt" in client.get(f"/ride/{play['id']}").text

    # and the other way round, by API
    body = client.post(f"{API}/sessions/{real['id']}/test", params={"mark": "test"}).json()
    assert body["is_test"] is True and body["gpx_status"] == "test"
    assert client.post(f"{API}/sessions/{real['id']}/test", params={"mark": "maybe"}).status_code == 422
    client.post(f"{API}/sessions/{real['id']}/test", params={"mark": "auto"})
    assert store.get(real["id"]).is_test is False


def test_training_counts_only_rides(client, tmp_path, monkeypatch):
    import datetime
    from bikelogservice import analysis
    monkeypatch.setattr(analysis, "today", lambda: datetime.date(2026, 5, 20))
    _put(client, tmp_path, ride(), "090000")
    sim = _put(client, tmp_path, simulated(), "100000")
    store = client.app.state.storage
    assert [r.session_id for r in analysis.rides(store)] != [] and \
        sim["id"] not in [r.session_id for r in analysis.rides(store)]
    store.set_test_override(sim["id"], "real")
    assert sim["id"] in [r.session_id for r in analysis.rides(store)]


def test_old_index_is_migrated(tmp_path):
    """A v7 index (before test detection) gets the columns and the sessions classified."""
    import sqlite3
    settings = Settings(data_dir=tmp_path / "state")
    with TestClient(create_app(settings)) as client:
        sid = _put(client, tmp_path, simulated(), "100000")["id"]
    db = sqlite3.connect(settings.db_path)
    db.execute("ALTER TABLE sessions DROP COLUMN test_kind")
    db.execute("ALTER TABLE sessions DROP COLUMN test_override")
    db.execute("PRAGMA user_version = 7")
    db.commit()
    db.close()
    with TestClient(create_app(settings)) as client:
        assert client.app.state.storage.get(sid).test_kind == "sim"
