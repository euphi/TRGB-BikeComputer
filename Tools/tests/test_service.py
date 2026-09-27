"""API behaviour of the bike log service.

The emphasis is on what a flaky delivery will actually do to it: send the same
file twice, send a changed file under the same name, send garbage, send files
of the session that is still running.
"""

import pytest
from fastapi.testclient import TestClient

from bikelog import fixtures
from bikelog.record import CURRENT_VERSION, write_records
from bikelogservice.app import create_app
from bikelogservice.config import Settings

API = "/api/v1"
SUMMARY = b"v=1\nstart=1758983412\nend=1758987012\ntime=corrected\nsrc=ntp\ndist_m=23456\n" \
          b"move_s=3000\nvavg_kmh=24.5\nshocks=3\n"


@pytest.fixture
def settings(tmp_path):
    return Settings(data_dir=tmp_path / "state")


@pytest.fixture
def client(settings):
    with TestClient(create_app(settings)) as client:
        yield client


@pytest.fixture
def ride_bytes(tmp_path):
    path = tmp_path / "ride.bin"
    write_records(path, fixtures.synthetic())
    return path.read_bytes()


def _put(client, payload, path="20260920/L_143012.bin", device="gravel", **kw):
    return client.put(f"{API}/devices/{device}/files/{path}", content=payload, **kw)


def _session_id(client, ride_bytes):
    return _put(client, ride_bytes).json()["session"]["id"]


def test_health(client):
    body = client.get(API + "/health").json()
    assert body["status"] == "ok"
    assert body["auth_required"] is False
    assert body["principal"] == "anonymous"
    assert body["pull"] is False


def test_put_indexes_the_log(client, ride_bytes):
    response = _put(client, ride_bytes)
    assert response.status_code == 200
    body = response.json()
    assert body["status"] == "new"
    session = body["session"]
    assert session["device"] == "gravel"
    assert (session["day"], session["stem"]) == ("20260920", "_143012")
    assert session["record_count"] == 240
    assert session["format_version"] == CURRENT_VERSION
    assert session["gps_points"] > 0
    assert session["clock_unset"] is False
    assert session["log_error"] is None
    assert [f["name"] for f in session["files"]] == ["L_143012.bin"]


def test_files_of_one_session_are_grouped(client, ride_bytes):
    _put(client, ride_bytes)
    _put(client, SUMMARY, "20260920/I_143012.txt")
    _put(client, b"debug\n", "20260920/D_143012.log")
    _put(client, b"\x01" * 64, "20260920/R_143012_01.bin")
    _put(client, b"other\n", "20260920/D_150000.log")
    listed = client.get(API + "/sessions").json()
    assert listed["total"] == 2
    ours = next(s for s in listed["sessions"] if s["stem"] == "_143012")
    assert sorted(f["name"] for f in ours["files"]) == \
        ["D_143012.log", "I_143012.txt", "L_143012.bin", "R_143012_01.bin"]
    assert ours["summary"]["dist_m"] == 23456
    assert ours["summary"]["time"] == "corrected"
    assert ours["start_time"] == 1758983412


def test_reupload_is_idempotent(client, ride_bytes):
    first = _put(client, ride_bytes).json()
    again = _put(client, ride_bytes).json()
    assert again["status"] == "unchanged"
    assert again["session"]["id"] == first["session"]["id"]
    assert client.get(API + "/sessions").json()["total"] == 1


def test_changed_file_replaces_the_old_one(client):
    _put(client, b"start=1\n", "20260920/I_143012.txt")
    body = _put(client, b"start=1758983412\n", "20260920/I_143012.txt").json()
    assert body["status"] == "replaced"
    assert body["session"]["summary"]["start"] == 1758983412
    sid = body["session"]["id"]
    assert client.get(f"{API}/sessions/{sid}/files/I_143012.txt").text == "start=1758983412\n"


def test_legacy_and_new_counters_stay_apart(client, ride_bytes):
    _put(client, ride_bytes, "NO_TIME/L0042.bin")
    _put(client, ride_bytes, "NO_TIME/L_0042.bin")
    assert client.get(API + "/sessions").json()["total"] == 2


def test_running_session_is_refused(client, ride_bytes):
    assert _put(client, ride_bytes, "CUR/L_0860.bin").status_code == 409


@pytest.mark.parametrize("path", ["../etc/passwd", "20260920/../../x.bin",
                                  "20260920/xL_1.bin", "a/b/L_1.bin", "20260920/.bin"])
def test_bad_paths_are_refused(client, path):
    assert _put(client, b"x", path).status_code in (400, 404)


def test_bad_device_name_is_refused(client, ride_bytes):
    assert _put(client, ride_bytes, device="a b").status_code == 422


def test_truncated_log_is_kept_and_flagged(client):
    import io
    import struct
    from bikelog.record import LAYOUTS
    buffer = io.BytesIO()
    layout = LAYOUTS[1]
    records = fixtures.synthetic()
    for rec in records:
        values = [1 if name == "format_version" else getattr(rec, name)
                  for name in layout.names]
        buffer.write(struct.pack(layout.fmt, *values))
    session = _put(client, buffer.getvalue() + b"\x00\x01\x02").json()["session"]
    # An aborted ride is still a ride -- keep it, but record the incomplete tail.
    assert session["record_count"] == len(records)
    assert session["trailing_bytes"] == 3


def test_garbage_log_is_kept_but_flagged(client):
    # It's the device's file: store it, say why nothing can be derived from it.
    session = _put(client, b"\xff" * 200).json()["session"]
    assert session["log_error"]
    sid = session["id"]
    assert client.get(f"{API}/sessions/{sid}/files/L_143012.bin").content == b"\xff" * 200


def test_empty_upload_is_rejected(client):
    assert _put(client, b"").status_code == 400


def test_upload_too_large(tmp_path, ride_bytes):
    small = Settings(data_dir=tmp_path / "s", max_upload_bytes=10)
    with TestClient(create_app(small)) as client:
        assert _put(client, ride_bytes).status_code == 413


def test_clock_unset_is_flagged(client, tmp_path):
    path = tmp_path / "noclock.bin"
    write_records(path, fixtures.synthetic(start_time=60, unset_clock_records=2))
    assert _put(client, path.read_bytes()).json()["session"]["clock_unset"] is True


def test_get_and_404(client, ride_bytes):
    sid = _session_id(client, ride_bytes)
    assert client.get(f"{API}/sessions/{sid}").json()["id"] == sid
    assert client.get(f"{API}/sessions/999").status_code == 404
    assert client.get(f"{API}/sessions/{sid}/files/nope.bin").status_code == 404


def test_gpx_export(client, ride_bytes):
    sid = _session_id(client, ride_bytes)
    response = client.get(f"{API}/sessions/{sid}.gpx")
    assert response.status_code == 200
    assert response.headers["content-type"].startswith("application/gpx+xml")
    assert int(response.headers["X-Bikelog-Segments"]) == 2
    assert "<trkpt" in response.text
    assert "gravel_20260920_143012.gpx" in response.headers["content-disposition"]


def test_gpx_export_honours_filter_parameters(client, ride_bytes):
    sid = _session_id(client, ride_bytes)
    default = client.get(f"{API}/sessions/{sid}.gpx")
    unfiltered = client.get(f"{API}/sessions/{sid}.gpx",
                            params={"max_fix_age_ms": 0, "segment_gap_s": 0})
    assert int(unfiltered.headers["X-Bikelog-Points"]) > \
        int(default.headers["X-Bikelog-Points"])
    assert int(unfiltered.headers["X-Bikelog-Segments"]) == 1


def test_gpx_without_any_fix_is_a_conflict_not_an_empty_file(client, tmp_path):
    path = tmp_path / "nofix.bin"
    write_records(path, fixtures.synthetic(seconds=30, no_fix_start_s=30))
    sid = _session_id(client, path.read_bytes())
    response = client.get(f"{API}/sessions/{sid}.gpx")
    assert response.status_code == 409
    assert "no usable GPS fix" in response.json()["detail"]


def test_export_of_a_session_without_log_is_404(client):
    sid = _put(client, b"debug\n", "20260920/D_143012.log").json()["session"]["id"]
    assert client.get(f"{API}/sessions/{sid}.gpx").status_code == 404
    assert client.get(f"{API}/sessions/{sid}.csv").status_code == 404


def test_csv_export(client, ride_bytes):
    sid = _session_id(client, ride_bytes)
    plain = client.get(f"{API}/sessions/{sid}.csv").text
    assert plain.splitlines()[0].startswith("Timestamp,Speed")
    with_gps = client.get(f"{API}/sessions/{sid}.csv", params={"with_gps": True}).text
    assert "GPS_Lat" in with_gps.splitlines()[0]


def test_raw_download_returns_the_exact_bytes(client, ride_bytes):
    sid = _session_id(client, ride_bytes)
    assert client.get(f"{API}/sessions/{sid}/files/L_143012.bin").content == ride_bytes


def test_files_mirror_the_sd_card_on_disk(client, settings, ride_bytes):
    _put(client, ride_bytes)
    _put(client, b"x", "L0001.bin")          # legacy file directly in /BIKECOMP
    assert (settings.sessions_dir / "gravel" / "20260920" / "L_143012.bin").read_bytes() == ride_bytes
    assert (settings.sessions_dir / "gravel" / "_" / "L0001.bin").exists()


def test_delete_leaves_a_tombstone(client, settings, ride_bytes):
    sid = _session_id(client, ride_bytes)
    assert client.delete(f"{API}/sessions/{sid}").status_code == 204
    assert client.get(f"{API}/sessions/{sid}").status_code == 404
    assert client.delete(f"{API}/sessions/{sid}").status_code == 404
    assert not (settings.sessions_dir / "gravel" / "20260920" / "L_143012.bin").exists()
    store = client.app.state.storage
    # The puller must not fetch it again from the SD card ...
    assert store.known_files("gravel") == {"20260920/L_143012.bin": None}


def test_explicit_upload_revives_a_deleted_session(client, ride_bytes):
    sid = _session_id(client, ride_bytes)
    client.delete(f"{API}/sessions/{sid}")
    body = _put(client, ride_bytes).json()
    assert body["status"] == "new"
    assert client.get(f"{API}/sessions/{body['session']['id']}").status_code == 200


def test_index_page(client, ride_bytes):
    _put(client, ride_bytes)
    _put(client, SUMMARY, "20260920/I_143012.txt")
    page = client.get("/")
    assert page.status_code == 200
    assert "23.5 km" in page.text
    assert f"{API}/sessions/1.gpx" in page.text


def test_pull_endpoints_when_disabled(client):
    assert client.get(API + "/pull").json() == {"enabled": False}
    assert client.post(API + "/pull").status_code == 409


# --- the auth retrofit ------------------------------------------------
# Auth is off today. These tests exist so that switching it on stays a
# config change: if a route is ever added without the dependency, the
# "everything needs a token" test below stops passing.

def _secured(tmp_path):
    return Settings(data_dir=tmp_path / "sec", require_auth=True, tokens=["s3cret:gravel"])


def test_auth_enabled_rejects_requests_without_a_token(tmp_path, ride_bytes):
    with TestClient(create_app(_secured(tmp_path))) as client:
        assert client.get(API + "/health").status_code == 401
        assert _put(client, ride_bytes).status_code == 401


def test_auth_enabled_accepts_a_configured_token(tmp_path, ride_bytes):
    headers = {"Authorization": "Bearer s3cret"}
    with TestClient(create_app(_secured(tmp_path))) as client:
        assert client.get(API + "/health", headers=headers).json()["principal"] == "gravel"
        upload = _put(client, ride_bytes, headers=headers)
        assert upload.status_code == 200
        assert upload.json()["session"]["files"][0]["source"] == "push:gravel"


def test_auth_enabled_rejects_an_unknown_token(tmp_path):
    with TestClient(create_app(_secured(tmp_path))) as client:
        response = client.get(API + "/health", headers={"Authorization": "Bearer wrong"})
        assert response.status_code == 401


def test_every_route_requires_a_token_once_auth_is_on(tmp_path, ride_bytes):
    """Guards the retrofit promise: no route may bypass the dependency."""
    headers = {"Authorization": "Bearer s3cret"}
    with TestClient(create_app(_secured(tmp_path))) as client:
        sid = _put(client, ride_bytes, headers=headers).json()["session"]["id"]
        protected = [
            ("GET", "/"),
            ("GET", f"{API}/health"),
            ("GET", f"{API}/sessions"),
            ("GET", f"{API}/sessions/{sid}"),
            ("GET", f"{API}/sessions/{sid}.gpx"),
            ("GET", f"{API}/sessions/{sid}.csv"),
            ("GET", f"{API}/sessions/{sid}/files/L_143012.bin"),
            ("PUT", f"{API}/devices/gravel/files/20260920/L_143012.bin"),
            ("DELETE", f"{API}/sessions/{sid}"),
            ("GET", f"{API}/pull"),
            ("POST", f"{API}/pull"),
        ]
        routes = {r.path for r in client.app.routes if hasattr(r, "methods")} - {
            "/openapi.json", "/docs", "/docs/oauth2-redirect", "/redoc"}
        assert len(routes) == len({p for _, p in protected}), \
            "new route? add it to the list above"
        for method, path in protected:
            assert client.request(method, path).status_code == 401, path
