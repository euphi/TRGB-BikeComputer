"""Bikes, which bike computer rides on which bike, and rides imported as GPX."""

import datetime

import pytest
from fastapi.testclient import TestClient

from bikelog import bikes, fixtures, gpximport, report
from bikelog.record import write_records
from bikelogservice import analysis
from bikelogservice.app import create_app
from bikelogservice.config import Settings

API = "/api/v1"
D = datetime.date


def test_assignment_by_date():
    gravel, pendler = bikes.Bike("Gravel", id="g"), bikes.Bike("Pendler", id="p")
    reg = bikes.Registry([gravel, pendler], [bikes.Assignment("bc", "g", D(2026, 1, 1)),
                                            bikes.Assignment("bc", "p", D(2026, 6, 1))])
    assert reg.bike_for("bc", D(2025, 12, 31)) is None
    assert reg.bike_for("bc", D(2026, 5, 31)) is gravel
    assert reg.bike_for("bc", D(2026, 6, 1)) is pendler
    assert reg.bike_for("bc", None) is gravel
    assert reg.bike_for("other", D(2026, 6, 1)) is None
    assert bikes.Registry.from_dict(reg.as_dict()).bike_for("bc", D(2026, 7, 1)).name == "Pendler"


def test_effective_athlete():
    rider = report.Athlete(rider_kg=75, mass_kg=85, cda=0.4, crr=0.006)
    assert bikes.effective_athlete(rider, None) is rider
    on = bikes.effective_athlete(rider, bikes.Bike("Pendler", mass_kg=16, cda=0.55, crr=0.008))
    assert (on.mass_kg, on.cda, on.crr) == (91, 0.55, 0.008)
    partial = bikes.effective_athlete(report.Athlete(mass_kg=85), bikes.Bike("x", mass_kg=9))
    assert partial.mass_kg == 85                      # no rider weight: the old system mass stays


def ride_gpx(start="2025-05-01T07:00:00Z", points=1800, hr=True):
    t0 = datetime.datetime.fromisoformat(start.replace("Z", "+00:00"))
    pts = []
    for i in range(points):
        t = (t0 + datetime.timedelta(seconds=i)).strftime("%Y-%m-%dT%H:%M:%SZ")
        ext = (f"<extensions><gpxtpx:TrackPointExtension><gpxtpx:hr>{140 + i % 10}</gpxtpx:hr>"
               "</gpxtpx:TrackPointExtension></extensions>") if hr else ""
        pts.append(f'<trkpt lat="{50.1 + i * 0.00006:.6f}" lon="8.6"><ele>{120 + i * 0.02:.2f}</ele>'
                   f"<time>{t}</time>{ext}</trkpt>")
    return ('<?xml version="1.0"?><gpx version="1.1" xmlns="http://www.topografix.com/GPX/1/1" '
            'xmlns:gpxtpx="http://www.garmin.com/xmlschemas/TrackPointExtension/v1"><trk><trkseg>'
            + "".join(pts) + "</trkseg></trk></gpx>").encode()


def test_gpx_to_records_smooths_the_gradient():
    records = gpximport.records_from_gpx(ride_gpx())
    assert records[0].hr == 140
    mid = records[len(records) // 2]
    assert mid.gradient == pytest.approx(0.02 / 6.67 * 100, abs=0.1)   # 2 cm per ~6.7 m
    with pytest.raises(gpximport.NotARide):
        gpximport.records_from_gpx(b'<gpx xmlns="http://www.topografix.com/GPX/1/1"><rte>'
                                   b'<rtept lat="50" lon="8"/></rte></gpx>')


@pytest.fixture
def client(tmp_path, monkeypatch):
    monkeypatch.setattr(analysis, "today", lambda: D(2026, 5, 20))
    with TestClient(create_app(Settings(data_dir=tmp_path / "state"))) as client:
        yield client


def _put(client, tmp_path, stem="090000", day="20260517", device="gravel"):
    path = tmp_path / f"{stem}.bin"
    write_records(path, fixtures.synthetic(seconds=900, pause=None, tunnel=None, no_fix_start_s=0))
    return client.put(f"{API}/devices/{device}/files/{day}/L_{stem}.bin", content=path.read_bytes()).json()["session"]["id"]


def test_bike_and_assignment_change_the_power_estimate(client, tmp_path):
    client.post("/athlete", data={"rider_kg": "75", "mass_kg": "85"})
    sid = _put(client, tmp_path)
    before = client.get(f"{API}/sessions/{sid}/report.json").json()
    assert before["meta"]["bike"] is None and before["athlete"]["mass_kg"] == 85

    client.post("/bikes", data={"name": "Pendler", "type": "Trekking/Pendler", "mass_kg": "18",
                                "cda": "0.55", "crr": "0.008"})
    reg = analysis.registry(client.app.state.storage)
    bike_id = reg.bikes[0].id
    client.post("/bikes/assign", data={"device": "gravel", "bike_id": bike_id, "since": "2026-01-01"})
    after = client.get(f"{API}/sessions/{sid}/report.json").json()
    assert after["meta"]["bike"]["name"] == "Pendler"
    assert after["athlete"]["mass_kg"] == 93 and after["athlete"]["cda"] == 0.55
    assert after["power"]["avg_moving_w"] > before["power"]["avg_moving_w"]

    page = client.get("/bikes").text
    assert "Pendler" in page and "gravel" in page and "Strecke 2026" in page
    assert "gravel · Pendler" in client.get("/").text
    assert "<b>Pendler</b>" in client.get(f"/ride/{sid}").text

    # a single ride on another bike
    client.post("/bikes", data={"name": "Gravel", "mass_kg": "9.5", "cda": "0.4"})
    gravel = [b for b in analysis.registry(client.app.state.storage).bikes if b.name == "Gravel"][0]
    client.post(f"/ui/sessions/{sid}/bike", data={"bike_id": gravel.id})
    assert client.get(f"{API}/sessions/{sid}/report.json").json()["meta"]["bike"]["name"] == "Gravel"
    assert "(nur diese Fahrt)" in client.get(f"/ride/{sid}").text

    client.post("/bikes/assign/delete", data={"device": "gravel", "since": "2026-01-01"})
    assert analysis.registry(client.app.state.storage).assignments == []
    client.post(f"/bikes/{bike_id}/delete")
    assert [b.name for b in analysis.registry(client.app.state.storage).bikes] == ["Gravel"]


def test_import_gpx_as_past_participation(client):
    client.post("/goals", data={"name": "Eschborn-Frankfurt", "date": "2027-05-01"})
    eid = client.get(f"{API}/events").json()[0]["id"]
    r = client.put(f"{API}/import/gpx", params={"event_id": eid}, content=ride_gpx())
    assert r.status_code == 200, r.text
    session = r.json()
    assert session["device"] == "import" and session["day"] == "20250501"
    assert sorted(f["name"] for f in session["files"])[0].startswith("G_")
    store = client.app.state.storage
    assert store.get(session["id"]).gpx_status == "import"          # never exported/uploaded
    # same file again: the same session
    again = client.put(f"{API}/import/gpx", params={"event_id": eid}, content=ride_gpx()).json()
    assert again["id"] == session["id"]
    assert client.get(f"{API}/events").json()[0]["participations"] == [session["id"]]
    html = client.get("/goals").text
    assert "Deine bisherigen Teilnahmen" in html and f'href="/ride/{session["id"]}"' in html
    rep = client.get(f"{API}/sessions/{session['id']}/report.json").json()
    assert rep["heart"]["avg"] and rep["ride"]["distance_km"] > 10
    # unlink keeps the ride
    client.post(f"/goals/{eid}/participations/{session['id']}/delete")
    assert client.get(f"{API}/events").json()[0]["participations"] == []
    assert store.get(session["id"]) is not None


def test_import_rejects_routes_and_garbage(client):
    assert client.put(f"{API}/import/gpx", content=b"<gpx/>").status_code == 400
    assert client.put(f"{API}/import/gpx", content=b"not xml").status_code == 400
    assert client.put(f"{API}/import/gpx", params={"bike_id": "nope"}, content=ride_gpx()).status_code == 400
    assert client.put(f"{API}/import/gpx", params={"event_id": "nope"}, content=ride_gpx()).status_code == 404
    assert "GPX-Fahrten importieren" in client.get("/").text
