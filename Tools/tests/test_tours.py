"""A reboot on the way: two sessions, one ride -- in training, on the pages."""

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
START = int(datetime.datetime(2026, 5, 17, 9, 0, tzinfo=datetime.timezone.utc).timestamp())


def part(start, seconds=900):
    return fixtures.synthetic(start_time=start, seconds=seconds, pause=None, tunnel=None, no_fix_start_s=0)


@pytest.fixture
def client(tmp_path, monkeypatch):
    monkeypatch.setattr(analysis, "today", lambda: datetime.date(2026, 5, 20))
    with TestClient(create_app(Settings(data_dir=tmp_path / "state"))) as client:
        yield client


def _put(client, tmp_path, records, stem):
    path = tmp_path / f"{stem}.bin"
    write_records(path, records)
    return client.put(f"{API}/devices/gravel/files/20260517/L_{stem}.bin",
                      content=path.read_bytes()).json()["session"]["id"]


def test_reboot_on_the_way_is_one_ride(client, tmp_path):
    a = _put(client, tmp_path, part(START), "090000")
    b = _put(client, tmp_path, part(START + 900 + 120), "091700")      # 2 min reboot
    c = _put(client, tmp_path, part(START + 6 * 3600), "150000")       # afternoon: own ride
    store = client.app.state.storage
    rides = analysis.rides(store)
    assert [r.session_ids for r in rides] == [[a, b], [c]]
    single = analysis.report_for(store, store.get(a))["ride"]["distance_km"]
    assert rides[0].distance_km == pytest.approx(2 * single, rel=0.02)   # distance runs on over the join
    tour = analysis.tour_report_for(store, [store.get(a), store.get(b)])
    assert tour["meta"]["sessions"] == [a, b]
    assert tour["ride"]["stops"]["count"] == 1                           # the reboot is a stop

    page = client.get("/").text
    assert f'href="/tour/{a}"' in page and "Teil 1/2" in page and "Teil 2/2" in page
    # one head row for the whole ride, its parts below it (oldest first), the lone ride apart
    assert page.count('class="tour-head"') == 1 and f'id="t{a}"' in page
    assert page.index(f'id="s{c}"') < page.index(f'id="t{a}"') < page.index(f'id="s{a}"') < page.index(f'id="s{b}"')
    assert page.count("tour-part") >= 2 and 'class="tour-part last"' in page
    assert f'id="s{c}" class' not in page
    ride = client.get(f"/ride/{b}").text
    assert "Teil 2 von 2 einer Fahrt" in ride and f'href="/tour/{a}"' in ride
    whole = client.get(f"/tour/{b}").text
    assert "2 Sitzungen, zusammengefasst" in whole and "Höhenprofil" in whole
    assert "Teil" not in client.get(f"/ride/{c}").text.split("<h2>")[0]


def test_tour_report_is_cached_and_refreshed(client, tmp_path):
    a = _put(client, tmp_path, part(START), "090000")
    b = _put(client, tmp_path, part(START + 1000), "091640")
    store = client.app.state.storage
    analysis.refresh(store, background=False)
    group = [store.get(a), store.get(b)]
    key = analysis.tour_key(store, group)
    assert store.cached_tour_report(key) is not None


def test_new_goal_with_gpx_in_one_go(client):
    r = client.post("/goals", data={"name": "Eschborn-Frankfurt", "date": "2027-05-01"},
                    headers={"Accept": "application/json"})
    eid = r.json()["id"]
    assert client.put(f"{API}/events/{eid}/gpx", content=gpx(course())).status_code == 200
    html = client.get("/goals").text
    assert "Andere Strecke (GPX) hochladen" in html


def test_goal_without_gpx_offers_the_upload_prominently(client):
    client.post("/goals", data={"name": "GF Strade Bianche", "date": "2027-03-07"})
    html = client.get("/goals").text
    assert "Noch keine Strecke" in html and "Strecke (GPX) hochladen" in html
    assert 'onsubmit="return newGoal(this)"' in html
