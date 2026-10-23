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


def test_list_is_short_and_details_are_on_the_route_page(client):
    route = _add(client, gpx([(lat, lon, None) for lat, lon, _ in course()]))
    page = client.get("/routes").text
    assert '<a href="/routes" class=on>Strecken</a>' in page
    assert f'<a href="/routes/{route["id"]}"><b>Hausrunde</b></a>' in page
    assert "Mit Einschränkungen" in page                     # in the list only when not fine
    assert "Keine (vollständigen) Höhen" not in page and "Für TrailBridge bereitstellen" not in page
    assert "hat TrailBridge nicht gefragt" in page
    detail = client.get(f"/routes/{route['id']}").text
    assert "Keine (vollständigen) Höhen" in detail and "Für TrailBridge bereitstellen" in detail
    assert client.get("/routes/nope").status_code == 404


def test_list_sections_and_small_profile(client):
    current, regular, old = (_add(client, filename=f"{n}.gpx", kind=k)
                             for n, k in (("a", "planned"), ("b", "regular"), ("c", "planned")))
    client.post(f"/routes/{old['id']}/archive", data={"on": "1"}, follow_redirects=False)
    page = client.get("/routes").text
    i_cur, i_reg, i_arc = (page.index(f'id="r{r["id"]}"') for r in (current, regular, old))
    assert page.index("<h2>Aktuell</h2>") < i_cur < page.index("Regelmäßig <span") < i_reg \
        < page.index("Archiv <span") < i_arc
    assert '<details class="sec" open><summary>Regelmäßig' in page
    assert '<details class="sec"><summary>Archiv' in page        # closed
    assert page.count('class="spark"') == 3


def test_archive_takes_the_route_back_from_trailbridge(client):
    route = _add(client)
    client.post(f"{API}/routes/{route['id']}/trailbridge")
    r = client.post(f"/routes/{route['id']}/archive", data={"on": "1"}, follow_redirects=False)
    assert r.headers["location"].startswith(f"/routes/{route['id']}?")
    got = client.get(f"{API}/routes/{route['id']}").json()
    assert got["archived"] and not got["marked_at"]
    assert client.get(FEED).status_code == 204
    # marking it again brings it back from the archive
    assert not client.post(f"{API}/routes/{route['id']}/trailbridge").json()["archived"]


def test_tags(client):
    route = _add(client)
    assert route["tags"] == []
    tagged = client.put(f"{API}/routes", params={"tags": "training,bogus", "filename": "t.gpx"},
                        content=gpx(course())).json()
    assert tagged["tags"] == ["training"]
    client.post(f"/routes/{route['id']}", data={"name": "Arbeit", "tags": "1", "tag_commute": "on"})
    assert client.get(f"{API}/routes/{route['id']}").json()["tags"] == ["commute"]
    assert "Pendel/Alltag" in client.get("/routes").text
    # a form without the tag boxes leaves them alone
    client.post(f"/routes/{route['id']}", data={"name": "Arbeit"})
    assert client.get(f"{API}/routes/{route['id']}").json()["tags"] == ["commute"]


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
    assert "Von TrailBridge geladen" in client.get(f"/routes/{route['id']}").text


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
