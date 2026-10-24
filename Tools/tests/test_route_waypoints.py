"""Waypoints of a route on the pages (/routes list, /routes/<id>) and in the stored check."""

import json

import pytest
from fastapi.testclient import TestClient

from bikelogservice import routes
from bikelogservice.app import create_app
from bikelogservice.config import Settings
from bikelogservice.storage import Storage

from test_training import course, gpx

API = "/api/v1"
BRASS_STROKE = 'stroke="#CBA36B" stroke-width="1" vector-effect="non-scaling-stroke"/>'


@pytest.fixture
def settings(tmp_path):
    return Settings(data_dir=tmp_path / "state")


@pytest.fixture
def client(settings):
    with TestClient(create_app(settings)) as client:
        yield client


def _wpt_gpx(*wpts: str) -> bytes:
    """The 6 km course (north along 8.6 E) with wpt in front of the track, as BRouter-Web writes it."""
    return gpx(course()).decode().replace("<trk>", "".join(wpts) + "<trk>", 1).encode()


def _add(client, data, filename="TEST Wegpunkte.gpx"):
    r = client.put(f"{API}/routes", params={"kind": "planned", "filename": filename}, content=data)
    assert r.status_code == 200, r.text
    return r.json()


def test_waypoints_show_on_page_and_list_escaped(client):
    evil = "Café | 10% ; a&amp;b=c &lt;b&gt;x&lt;/b&gt;"       # as the XML of the file has it
    route = _add(client, _wpt_gpx(
        '<wpt lat="50.0269" lon="8.6"><name>Kaffeepause</name><type>via</type></wpt>',
        f'<wpt lat="50.0449" lon="8.6"><name>{evil}</name></wpt>',
        '<wpt lat="50.0" lon="8.6"><name>from</name><type>from</type></wpt>'))
    page = client.get(f"/routes/{route['id']}").text
    assert "Kaffeepause" in page and "<h2>Karte</h2>" in page and "<h2>Wegpunkte</h2>" in page
    # the name is "Café | 10% ; a&b=c <b>x</b>": the browser must get it HTML-escaped, nowhere as a tag
    assert "Café | 10% ; a&amp;b=c &lt;b&gt;x&lt;/b&gt;" in page and "<b>x</b>" not in page
    assert "Start" in page                                      # the machine-named "from", greyed
    # two named waypoints = two strokes in the list's sparkline; from/to/via1 would not get one
    assert client.get("/routes").text.count(BRASS_STROKE) == 2


def test_route_without_waypoints_is_unchanged(client):
    route = _add(client, gpx(course()))
    page = client.get(f"/routes/{route['id']}").text
    assert "<h2>Wegpunkte</h2>" not in page and "<h2>Höhenprofil</h2>" in page
    assert BRASS_STROKE not in client.get("/routes").text


def test_stored_checks_of_an_older_version_are_redone(client, settings):
    route = _add(client, _wpt_gpx('<wpt lat="50.0269" lon="8.6"><name>Kaffeepause</name></wpt>'))
    path = settings.data_dir / "routes.json"
    stored = json.loads(path.read_text(encoding="utf-8"))
    stored[0]["check"] = {"version": 1, "verdict": "ok", "name": "x", "findings": [], "stats": {}}
    path.write_text(json.dumps(stored), encoding="utf-8")
    store = Storage(settings)
    assert routes.recheck(store) == 1
    assert routes.get(store, route["id"]).stats["wpts"][0]["name"] == "Kaffeepause"
