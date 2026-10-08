"""The Rim & Ridge pages of the log service: ride report, training, goals, climbs, rider."""

import datetime

import pytest
from fastapi.testclient import TestClient

from bikelog import fixtures
from bikelog.record import write_records
from bikelogservice import analysis
from bikelogservice.app import create_app
from bikelogservice.config import Settings

from test_training import course, gpx

API = "/api/v1"


@pytest.fixture
def settings(tmp_path):
    return Settings(data_dir=tmp_path / "state")


@pytest.fixture
def client(settings, monkeypatch):
    # the synthetic ride is from 2026-05-17
    monkeypatch.setattr(analysis, "today", lambda: datetime.date(2026, 5, 20))
    with TestClient(create_app(settings)) as client:
        yield client


@pytest.fixture
def sid(client, tmp_path):
    path = tmp_path / "ride.bin"
    write_records(path, fixtures.with_road_quality(fixtures.synthetic(seconds=1800)))
    r = client.put(f"{API}/devices/gravel/files/20260517/L_090000.bin", content=path.read_bytes())
    return r.json()["session"]["id"]


def test_pages_share_the_rim_ridge_frame(client, sid):
    for path in ("/", f"/ride/{sid}", "/training", "/goals", "/climbs", "/athlete"):
        page = client.get(path)
        assert page.status_code == 200, path
        assert "#CBA36B" in page.text and "Big+Shoulders+Display" in page.text, path
        assert '<nav><a href="/"' in page.text, path


def test_index_links_the_ride_page(client, sid):
    assert f'href="/ride/{sid}"' in client.get("/").text


def test_ride_page(client, sid):
    html = client.get(f"/ride/{sid}").text
    assert "Höhenprofil" in html and "<svg" in html
    assert "Wegeklassen" in html and "Technik" in html
    assert "maximale Herzfrequenz" in html          # no athlete.json yet
    assert client.get(f"/ride/{sid + 99}").status_code == 404


def test_report_is_cached_and_recomputed_for_new_rider_data(client, settings, sid):
    store = client.app.state.storage
    client.get(f"{API}/sessions/{sid}/report.json")
    key, rep = store.cached_report(sid)
    assert rep["heart"]["trimp"] is None
    client.post("/athlete", data={"hr_max": "190", "hr_rest": "50"})
    body = client.get(f"{API}/sessions/{sid}/report.json").json()
    assert body["heart"]["trimp_method"] == "banister"
    assert store.cached_report(sid)[0] != key


def test_refresh_computes_what_is_missing(client, sid):
    store = client.app.state.storage
    analysis.refresh(store, background=False)       # waits for the one the upload started
    store.store_report(sid, "stale", {"empty": True})
    assert analysis.refresh(store, background=False) == 1
    assert analysis.refresh(store, background=False) == 0


def test_training_page_and_api(client, settings, sid):
    settings.athlete_path.write_text('{"hr_max": 185, "hr_rest": 50}')
    analysis.refresh(client.app.state.storage, background=False)
    html = client.get("/training").text
    assert "Fitness und Ermüdung" in html and "Stunden je Pulszone" in html
    data = client.get(f"{API}/training", params={"days": 10, "weeks": 2}).json()
    assert len(data["load"]) == 10
    assert data["load"][-1]["ctl"] > 0
    assert [w["rides"] for w in data["weeks"]] == [1, 0]        # Sunday 17 May, today Wed 20 May


def test_training_page_without_rides(client):
    assert "Noch keine Fahrten" in client.get("/training").text


def test_goals_create_upload_delete(client, sid):
    r = client.post("/goals", data={"name": "GF Strade Bianche", "date": "2027-03-07", "priority": "A",
                                    "distance_km": "", "ascent_m": "", "notes": "Schotter!"},
                    follow_redirects=False)
    assert r.status_code == 303
    events = client.get(f"{API}/events").json()
    assert events[0]["name"] == "GF Strade Bianche"
    assert events[0]["readiness"]["phase"] == "Grundlage"
    eid = events[0]["id"]
    r = client.put(f"{API}/events/{eid}/gpx", content=gpx(course()))
    assert r.status_code == 200 and r.json()["profile"]["climbs"]
    assert client.put(f"{API}/events/{eid}/gpx", content=b"<gpx/>").status_code == 400
    assert client.put(f"{API}/events/nope/gpx", content=gpx(course())).status_code == 404
    html = client.get("/goals").text
    assert "GF Strade Bianche" in html and "Grundlage" in html and "deine Zeit" in html
    # edit keeps the GPX
    client.post("/goals", data={"id": eid, "name": "GF Strade Bianche", "date": "2027-03-07",
                                "priority": "A", "distance_km": "137"})
    ev = client.get(f"{API}/events").json()[0]
    assert ev["distance_km"] == 137 and ev["gpx"]
    assert client.post(f"/goals/{eid}/delete", follow_redirects=False).status_code == 303
    assert client.get(f"{API}/events").json() == []
    assert client.post("/goals/nope/delete").status_code == 404


def test_goal_validation(client):
    assert client.post("/goals", data={"name": "x", "date": "morgen"}).status_code == 400
    assert client.post("/goals", data={"name": "", "date": "2027-01-01"}).status_code == 400


def test_athlete_form(client, settings):
    r = client.post("/athlete", data={"hr_max": "186", "hr_rest": "48", "mass_kg": "88,5",
                                      "rider_kg": "", "cda": "0.38", "crr": "0.006",
                                      "zones_pct": "60 72 82 90"}, follow_redirects=False)
    assert r.status_code == 303
    rider = analysis.athlete(client.app.state.storage)
    assert rider.hr_max == 186 and rider.mass_kg == 88.5 and rider.zones_pct == (60, 72, 82, 90)
    assert rider.rider_kg is None
    assert 'value="186"' in client.get("/athlete").text
    bad = client.post("/athlete", data={"zones_pct": "90 80"})
    assert bad.status_code == 400 and "Zonengrenzen" in bad.text


def test_climbs_page_lists_recurring_climbs(client, monkeypatch):
    from bikelog import training
    start = datetime.datetime(2026, 5, 1, tzinfo=datetime.timezone.utc)
    efforts = [{"category": "4", "length_m": 2000, "gain_m": 100, "avg_grade_pct": 5.0,
                "foot": (50.1, 8.6), "summit": (50.11, 8.6), "duration_s": d, "vam_m_h": 1000,
                "avg_hr": 150, "est_avg_power_w": 250} for d in (400, 380)]
    rides = [training.Ride(session_id=i + 1, start=start + datetime.timedelta(days=i), day=start.date(),
                           distance_km=30, moving_s=3600, ascent_m=300, trimp=None, zones_s=None,
                           avg_hr=None, normalized_w=None, best_w={}, decoupling_pct=None,
                           efficiency=None, climbs=[c]) for i, c in enumerate(efforts)]
    monkeypatch.setattr(analysis, "rides", lambda store: rides)
    html = client.get("/climbs").text
    assert "6:20" in html and "+0:20" in html
