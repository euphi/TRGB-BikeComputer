"""Planned and regular routes in the log service, and the route feed TrailBridge polls."""

import pytest
from fastapi.testclient import TestClient

from bikelogservice import routes
from bikelogservice.app import create_app
from bikelogservice.config import Settings

from test_training import course, gpx

API = "/api/v1"
FEED = API + "/trailbridge/route"


@pytest.fixture
def settings(tmp_path):
    return Settings(data_dir=tmp_path / "state")


@pytest.fixture
def client(settings):
    with TestClient(create_app(settings)) as client:
        yield client


def _add(client, data=None, kind="planned", filename="Hausrunde.gpx"):
    r = client.put(f"{API}/routes", params={"kind": kind, "filename": filename},
                   content=data if data is not None else gpx(course()))
    assert r.status_code == 200, r.text
    return r.json()


def test_upload_checks_and_stores_the_file(client, settings):
    data = gpx(course())
    route = _add(client, data, kind="regular")
    assert (route["name"], route["kind"], route["check"]["verdict"]) == ("Hausrunde", "regular", "ok")
    assert route["check"]["stats"]["climbs"]
    assert (settings.data_dir / "routes" / f"{route['id']}.gpx").read_bytes() == data
    assert [r["id"] for r in client.get(f"{API}/routes").json()] == [route["id"]]
    download = client.get(f"{API}/routes/{route['id']}.gpx")
    assert download.content == data
    assert "Hausrunde.gpx" in download.headers["content-disposition"]


def test_unreadable_gpx_is_refused_and_not_kept(client, settings):
    r = client.put(f"{API}/routes", content=b"<gpx/>")
    assert r.status_code == 400 and "unbrauchbar" in r.json()["detail"]
    assert client.get(f"{API}/routes").json() == []


def test_page_shows_route_findings_and_trailbridge_box(client):
    route = _add(client, gpx([(lat, lon, None) for lat, lon, _ in course()]))
    page = client.get("/routes").text
    assert '<a href="/routes" class=on>Strecken</a>' in page
    assert f'id="r{route["id"]}"' in page
    assert "Mit Einschränkungen" in page and "Keine (vollständigen) Höhen" in page
    assert "Für TrailBridge bereitstellen" in page
    assert "hat TrailBridge nicht gefragt" in page


def test_edit_and_delete(client, settings):
    route = _add(client)
    r = client.post(f"/routes/{route['id']}", data={"name": "Feierabend", "kind": "regular", "notes": "Di+Do"},
                    follow_redirects=False)
    assert r.status_code == 303
    got = client.get(f"{API}/routes/{route['id']}").json()
    assert (got["name"], got["kind"], got["notes"]) == ("Feierabend", "regular", "Di+Do")
    assert client.delete(f"{API}/routes/{route['id']}").status_code == 204
    assert not list((settings.data_dir / "routes").glob("*.gpx"))
    assert client.get(f"{API}/routes/{route['id']}").status_code == 404


def test_feed_is_empty_until_a_route_is_marked(client):
    _add(client)
    assert client.get(FEED).status_code == 204


def test_feed_round_trip(client):
    data = gpx(course())
    route = _add(client, data)
    marked = client.post(f"{API}/routes/{route['id']}/trailbridge").json()
    assert marked["marked_at"] and not marked["on_phone"]

    r = client.get(FEED)
    assert r.status_code == 200
    body = r.json()
    assert (body["id"], body["revision"]) == (route["id"], marked["revision"])
    assert r.headers["etag"] == f'"{body["revision"]}"'
    assert client.get(FEED, headers={"If-None-Match": r.headers["etag"]}).status_code == 304

    gpx_r = client.get(body["gpx_url"])
    assert gpx_r.content == data and gpx_r.headers["x-bikelog-revision"] == body["revision"]

    ack = client.post(FEED + "/fetched", params={"revision": body["revision"]})
    assert ack.status_code == 200
    assert client.get(f"{API}/routes/{route['id']}").json()["on_phone"]
    assert "Von TrailBridge geladen" in client.get("/routes").text


def test_marking_again_or_a_new_file_is_a_new_revision(client):
    route = _add(client)
    first = client.post(f"{API}/routes/{route['id']}/trailbridge").json()["revision"]
    client.post(FEED + "/fetched", params={"revision": first})

    pts = course(flat_km=3)
    replaced = client.put(f"{API}/routes/{route['id']}/gpx", content=gpx(pts)).json()
    assert replaced["marked_at"] and replaced["revision"] != first and not replaced["on_phone"]
    # a confirmation of the old revision is too late
    assert client.post(FEED + "/fetched", params={"revision": first}).status_code == 409

    client.post(f"{API}/routes/{route['id']}/trailbridge", params={"on": False})
    assert client.get(FEED).status_code == 204
    again = client.post(f"{API}/routes/{route['id']}/trailbridge").json()["revision"]
    assert again not in (first, replaced["revision"])


def test_only_one_route_is_marked(client):
    a, b = _add(client, filename="a.gpx"), _add(client, filename="b.gpx")
    client.post(f"{API}/routes/{a['id']}/trailbridge")
    client.post(f"/routes/{b['id']}/trailbridge", data={"on": "1"}, follow_redirects=False)
    state = {r["id"]: r["marked_at"] for r in client.get(f"{API}/routes").json()}
    assert state[a["id"]] is None and state[b["id"]]
    assert client.get(FEED).json()["id"] == b["id"]


def test_stale_checks_are_redone_on_start(settings, monkeypatch):
    with TestClient(create_app(settings)) as client:
        rid = _add(client)["id"]
    monkeypatch.setattr(routes, "CHECK_VERSION", routes.CHECK_VERSION + 1)
    with TestClient(create_app(settings)) as client:
        assert client.get(f"{API}/routes/{rid}").json()["check"]["version"] == routes.CHECK_VERSION


def test_feed_follows_the_service_auth(tmp_path):
    """No password of its own: the network (LAN/VPN) limits access, like for the pages.
    Only if the service's auth is switched on, the feed needs one of its tokens too."""
    settings = Settings(data_dir=tmp_path / "state", require_auth=True, tokens=["dev:phone"])
    with TestClient(create_app(settings)) as client:
        assert client.get(FEED).status_code == 401
        assert client.get(FEED, headers={"Authorization": "Bearer dev"}).status_code == 204
