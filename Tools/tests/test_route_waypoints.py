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


def _linked_gpx(href: str) -> bytes:
    return gpx(course()).decode().replace("<trk>", f'<trk><link href="{href}"><text>x</text></link>', 1).encode()


def test_planner_link_on_the_route_page(client):
    route = _add(client, _linked_gpx("http://brouter.ia216.internal/#map=14/50/8/x&amp;lonlats=8.6,50;8.6,50.1"
                                     "&amp;profile=gravel-schnell&amp;profileparams=hills:1"))
    url = "http://brouter.ia216.internal/#map=14/50/8/x&lonlats=8.6,50;8.6,50.1&profile=gravel-schnell&profileparams=hills:1"
    assert route["check"]["stats"]["planner_url"] == url
    page = client.get(f"/routes/{route['id']}").text
    edit = ("http://brouter.ia216.internal/?bikelog=" + route["id"] + "&bikelogname=TEST+Wegpunkte"
            "#map=14/50/8/x&lonlats=8.6,50;8.6,50.1&profile=gravel-schnell&profileparams=hills:1")
    assert "In BRouter bearbeiten" in page and edit.replace("&", "&amp;") in page


def test_planner_link_replaces_the_route_of_an_earlier_edit(client):
    route = _add(client, _linked_gpx("http://b.internal/?bikelog=abc&amp;bikelogname=Alt&amp;x=1#lonlats=8.6,50"))
    page = client.get(f"/routes/{route['id']}").text
    assert f"http://b.internal/?x=1&amp;bikelog={route['id']}&amp;bikelogname=TEST+Wegpunkte#lonlats=8.6,50" in page


def test_planner_link_of_bikerouter_only_opens(client):
    route = _add(client, _linked_gpx("https://bikerouter.de/#lonlats=8.6,50"))
    page = client.get(f"/routes/{route['id']}").text
    assert "In bikerouter.de öffnen" in page and "bikelog=" not in page


@pytest.mark.parametrize("href", [
    "javascript:alert(1)//#lonlats=1,2",            # never a link on the page
    "https://example.org/some/tour",                # a link, but not to a planned route
    'http://x.org/#lonlats=1,2&quot; onclick=&quot;alert(1)',
])
def test_planner_link_only_a_harmless_route_url(client, href):
    route = _add(client, _linked_gpx(href))
    page = client.get(f"/routes/{route['id']}").text
    if "quot" in href:                              # a route URL: the page must escape it
        assert 'onclick="' not in page
    else:
        assert "planner_url" not in route["check"]["stats"] and "In BRouter bearbeiten" not in page


def test_recheck_finds_the_link_in_stored_routes_and_keeps_the_revision(client, settings):
    route = _add(client, _linked_gpx("https://bikerouter.de/#lonlats=8.6,50;8.6,50.1"))
    store = Storage(settings)
    stored = routes.load(store)
    stored[0].check = {"verdict": "ok", "stats": {}, "findings": [], "version": routes.CHECK_VERSION - 1}
    routes._save(store, stored)
    assert routes.recheck(store) == 1
    after = routes.get(store, route["id"])
    assert after.stats["planner_url"] == "https://bikerouter.de/#lonlats=8.6,50;8.6,50.1"
    assert after.revision == route["revision"]      # TrailBridge does not load it again
