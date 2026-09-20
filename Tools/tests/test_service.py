"""API behaviour of the upload service.

The emphasis is on what an ESP32 on bad WiFi will actually do to this
service: send the same file twice, send half a file, send garbage.
"""

import pytest
from fastapi.testclient import TestClient

from bikelog import fixtures
from bikelog.record import write_records
from bikelogservice.app import create_app
from bikelogservice.config import Settings

API = "/api/v1"


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


def _upload(client, payload, **params):
    return client.post(API + "/rides", content=payload,
                       params={"filename": "L0001.bin", **params})


def test_health(client):
    body = client.get(API + "/health").json()
    assert body["status"] == "ok"
    assert body["auth_required"] is False
    assert body["principal"] == "anonymous"


def test_upload_indexes_the_ride(client, ride_bytes):
    response = _upload(client, ride_bytes, device="trgb-gravel")
    assert response.status_code == 201
    body = response.json()
    assert body["duplicate"] is False
    assert body["record_count"] == 240
    assert body["format_version"] == 1
    assert body["gps_points"] > 0
    assert body["device"] == "trgb-gravel"
    assert body["clock_unset"] is False
    assert body["duration_s"] == body["last_time"] - body["first_time"]


def test_reupload_is_idempotent(client, ride_bytes):
    first = _upload(client, ride_bytes).json()
    again = _upload(client, ride_bytes)
    # The firmware retries blindly after a dropped connection; a second
    # delivery of the same bytes must not create a second ride, and must not
    # look like an error either.
    assert again.status_code == 200
    assert again.json()["duplicate"] is True
    assert again.json()["id"] == first["id"]
    assert client.get(API + "/rides").json()["total"] == 1


def test_truncated_upload_is_accepted_and_flagged(client):
    records = fixtures.synthetic()
    import io
    buffer = io.BytesIO()
    from bikelog.record import LAYOUTS
    import struct
    layout = LAYOUTS[1]
    for rec in records:
        values = [1 if name == "format_version" else getattr(rec, name)
                  for name in layout.names]
        buffer.write(struct.pack(layout.fmt, *values))
    payload = buffer.getvalue() + b"\x00\x01\x02"
    body = _upload(client, payload).json()
    # An aborted ride is still a ride -- keep it, but record that the tail
    # was incomplete.
    assert body["record_count"] == len(records)
    assert body["trailing_bytes"] == 3


def test_garbage_upload_is_rejected(client):
    assert _upload(client, b"\xff" * 200).status_code == 422


def test_empty_upload_is_rejected(client):
    assert _upload(client, b"").status_code == 400


def test_upload_too_large(tmp_path, ride_bytes):
    small = Settings(data_dir=tmp_path / "s", max_upload_bytes=10)
    with TestClient(create_app(small)) as client:
        assert _upload(client, ride_bytes).status_code == 413


def test_clock_unset_is_flagged(client, tmp_path):
    path = tmp_path / "noclock.bin"
    write_records(path, fixtures.synthetic(start_time=60, unset_clock_records=2))
    body = _upload(client, path.read_bytes()).json()
    assert body["clock_unset"] is True


def test_get_and_list_and_404(client, ride_bytes):
    ride_id = _upload(client, ride_bytes).json()["id"]
    assert client.get(f"{API}/rides/{ride_id}").json()["id"] == ride_id
    listed = client.get(API + "/rides").json()
    assert listed["total"] == 1 and listed["rides"][0]["id"] == ride_id
    assert client.get(f"{API}/rides/doesnotexist").status_code == 404


def test_gpx_export(client, ride_bytes):
    ride_id = _upload(client, ride_bytes).json()["id"]
    response = client.get(f"{API}/rides/{ride_id}.gpx")
    assert response.status_code == 200
    assert response.headers["content-type"].startswith("application/gpx+xml")
    assert int(response.headers["X-Bikelog-Segments"]) == 2
    assert "<trkpt" in response.text


def test_gpx_export_honours_filter_parameters(client, ride_bytes):
    ride_id = _upload(client, ride_bytes).json()["id"]
    default = client.get(f"{API}/rides/{ride_id}.gpx")
    unfiltered = client.get(f"{API}/rides/{ride_id}.gpx",
                            params={"max_fix_age_ms": 0, "segment_gap_s": 0})
    assert int(unfiltered.headers["X-Bikelog-Points"]) > \
        int(default.headers["X-Bikelog-Points"])
    assert int(unfiltered.headers["X-Bikelog-Segments"]) == 1


def test_gpx_without_any_fix_is_a_conflict_not_an_empty_file(client, tmp_path):
    path = tmp_path / "nofix.bin"
    write_records(path, fixtures.synthetic(seconds=30, no_fix_start_s=30))
    ride_id = _upload(client, path.read_bytes()).json()["id"]
    response = client.get(f"{API}/rides/{ride_id}.gpx")
    assert response.status_code == 409
    assert "no usable GPS fix" in response.json()["detail"]


def test_csv_export(client, ride_bytes):
    ride_id = _upload(client, ride_bytes).json()["id"]
    plain = client.get(f"{API}/rides/{ride_id}.csv").text
    assert plain.splitlines()[0].startswith("Timestamp,Speed")
    with_gps = client.get(f"{API}/rides/{ride_id}.csv", params={"with_gps": True}).text
    assert "GPS_Lat" in with_gps.splitlines()[0]


def test_raw_download_returns_the_exact_bytes(client, ride_bytes):
    ride_id = _upload(client, ride_bytes).json()["id"]
    assert client.get(f"{API}/rides/{ride_id}.bin").content == ride_bytes


def test_delete(client, ride_bytes):
    ride_id = _upload(client, ride_bytes).json()["id"]
    assert client.delete(f"{API}/rides/{ride_id}").status_code == 204
    assert client.get(f"{API}/rides/{ride_id}").status_code == 404
    assert client.delete(f"{API}/rides/{ride_id}").status_code == 404


def test_deleted_ride_can_be_uploaded_again(client, ride_bytes):
    ride_id = _upload(client, ride_bytes).json()["id"]
    client.delete(f"{API}/rides/{ride_id}")
    assert _upload(client, ride_bytes).status_code == 201


# --- the auth retrofit ------------------------------------------------
# Auth is off today. These tests exist so that switching it on stays a
# config change: if a route is ever added without the dependency, the
# "everything needs a token" test below stops passing.


def test_auth_enabled_rejects_requests_without_a_token(tmp_path, ride_bytes):
    secured = Settings(data_dir=tmp_path / "sec", require_auth=True,
                       tokens=["s3cret:gravel"])
    with TestClient(create_app(secured)) as client:
        assert client.get(API + "/health").status_code == 401
        assert _upload(client, ride_bytes).status_code == 401


def test_auth_enabled_accepts_a_configured_token(tmp_path, ride_bytes):
    secured = Settings(data_dir=tmp_path / "sec", require_auth=True,
                       tokens=["s3cret:gravel"])
    headers = {"Authorization": "Bearer s3cret"}
    with TestClient(create_app(secured)) as client:
        assert client.get(API + "/health", headers=headers).json()["principal"] == "gravel"
        upload = client.post(API + "/rides", content=ride_bytes, headers=headers)
        assert upload.status_code == 201
        assert upload.json()["uploaded_by"] == "gravel"


def test_auth_enabled_rejects_an_unknown_token(tmp_path):
    secured = Settings(data_dir=tmp_path / "sec", require_auth=True,
                       tokens=["s3cret:gravel"])
    with TestClient(create_app(secured)) as client:
        response = client.get(API + "/health", headers={"Authorization": "Bearer wrong"})
        assert response.status_code == 401


def test_every_route_requires_a_token_once_auth_is_on(tmp_path, ride_bytes):
    """Guards the retrofit promise: no route may bypass the dependency."""
    secured = Settings(data_dir=tmp_path / "sec", require_auth=True,
                       tokens=["s3cret:gravel"])
    headers = {"Authorization": "Bearer s3cret"}
    with TestClient(create_app(secured)) as client:
        ride_id = client.post(API + "/rides", content=ride_bytes,
                              headers=headers).json()["id"]
        protected = [
            ("GET", f"{API}/health"),
            ("GET", f"{API}/rides"),
            ("GET", f"{API}/rides/{ride_id}"),
            ("GET", f"{API}/rides/{ride_id}.gpx"),
            ("GET", f"{API}/rides/{ride_id}.csv"),
            ("GET", f"{API}/rides/{ride_id}.bin"),
            ("POST", f"{API}/rides"),
            ("DELETE", f"{API}/rides/{ride_id}"),
        ]
        for method, path in protected:
            assert client.request(method, path).status_code == 401, path
