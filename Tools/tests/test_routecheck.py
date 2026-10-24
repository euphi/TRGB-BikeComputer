"""bikelog.routecheck: can TrailBridge navigate a GPX, and its key figures."""

from bikelog import routecheck

from test_training import course, gpx

NS = 'xmlns="http://www.topografix.com/GPX/1/1" version="1.1"'


def _doc(body: str) -> bytes:
    return f'<?xml version="1.0"?><gpx {NS}>{body}</gpx>'.encode()


def _trkpts(points, times=False) -> str:
    return "".join(f'<trkpt lat="{lat}" lon="{lon}">' + (f"<ele>{ele}</ele>" if ele is not None else "")
                   + (f"<time>2026-10-24T08:{i // 60:02d}:{i % 60:02d}Z</time>" if times else "")
                   + "</trkpt>" for i, (lat, lon, ele) in enumerate(points))


def _levels(result) -> set[str]:
    return {f["level"] for f in result["findings"]}


def test_track_with_heights_is_navigable_with_derived_turns():
    r = routecheck.check(gpx(course()), "Hausrunde")
    assert r["verdict"] == "ok"
    assert r["name"] == "Hausrunde"                     # no <name> in the file
    st = r["stats"]
    assert st["geometry"] == "trk" and st["turn_source"] == "geometry"
    assert 5.9 < st["distance_km"] < 6.1
    assert st["ascent_m"] > 90 and len(st["climbs"]) == 1
    assert st["max_spacing_m"] == 25 and not st["loop"]
    assert any("aus den Winkeln" in f["text"] for f in r["findings"])


def test_name_comes_from_the_file():
    data = _doc("<trk><name>Taunus-Schleife</name><trkseg>" + _trkpts(course()) + "</trkseg></trk>")
    assert routecheck.check(data, "fallback")["name"] == "Taunus-Schleife"


def test_without_heights_is_usable_with_a_warning():
    r = routecheck.check(gpx([(lat, lon, None) for lat, lon, _ in course()]))
    assert r["verdict"] == "warn"
    assert "ascent_m" not in r["stats"]
    assert any("Höhen" in f["text"] for f in r["findings"] if f["level"] == "warn")


def test_route_points_only_warn_and_read_turns_from_their_text():
    pts = '<rtept lat="50.0" lon="8.6"><name>Start</name></rtept>' \
          '<rtept lat="50.0005" lon="8.6"><desc>Turn left onto Bahnstraße</desc></rtept>' \
          '<rtept lat="50.0005" lon="8.6007"><name>Ziel</name></rtept>'
    r = routecheck.check(_doc(f"<rte>{pts}</rte>"))
    assert r["verdict"] == "warn"
    assert r["stats"]["geometry"] == "rte"
    assert (r["stats"]["turn_source"], r["stats"]["turns"]) == ("rtept", 1)


def test_osmand_segments_are_the_turns():
    ext = ('<extensions><osmand:route xmlns:osmand="https://osmand.net">'
           '<osmand:segment startTrkptIdx="0" turnType="C"/>'
           '<osmand:segment startTrkptIdx="40" turnType="TL"/>'
           '<osmand:segment startTrkptIdx="80" turnType="TR"/></osmand:route></extensions>')
    r = routecheck.check(_doc("<trk><trkseg>" + _trkpts(course()) + "</trkseg>" + ext + "</trk>"))
    assert (r["stats"]["turn_source"], r["stats"]["turns"]) == ("osmand", 2)


def test_bikerouter_way_points_are_not_routing_info():
    """bikerouter.de: track plus rtept that only name start, via and destination."""
    pts = course()
    rte = "".join(f'<rtept lat="{lat}" lon="{lon}"><name>{n}</name></rtept>'
                  for (lat, lon, _), n in ((pts[0], "Start"), (pts[80], "Via 1"), (pts[-1], "Ziel")))
    r = routecheck.check(_doc("<trk><trkseg>" + _trkpts(pts) + f"</trkseg></trk><rte>{rte}</rte>"))
    assert r["stats"]["turn_source"] == "geometry"
    assert any("nur Start" in f["text"] for f in r["findings"])


def test_times_give_the_planned_duration():
    r = routecheck.check(_doc("<trk><trkseg>" + _trkpts(course(), times=True) + "</trkseg></trk>"))
    assert r["stats"]["duration_s"] == len(course()) - 1


def test_waypoints_beside_the_route_do_not_count():
    pts = course()
    wpt = (f'<wpt lat="{pts[100][0]}" lon="{pts[100][1] + 0.0003}"><name>Brunnen</name></wpt>'   # ~20 m off
           f'<wpt lat="{pts[100][0]}" lon="{pts[100][1] + 0.01}"><name>Burg</name></wpt>')       # ~700 m off
    r = routecheck.check(_doc(wpt + "<trk><trkseg>" + _trkpts(pts) + "</trkseg></trk>"))
    assert (r["stats"]["waypoints"], r["stats"]["waypoints_off_route"]) == (1, 1)


def test_sparse_points_and_holes_between_segments_warn():
    pts = course()
    assert routecheck.check(gpx(pts[::20]))["verdict"] == "ok"      # 500 m apart: a router on a straight road
    r = routecheck.check(gpx(pts[::100]))                              # 2.5 km: simplified
    assert r["verdict"] == "warn" and any("vereinfacht" in f["text"] for f in r["findings"])
    holes = _doc("<trk><trkseg>" + _trkpts(pts[:80]) + "</trkseg><trkseg>" + _trkpts(pts[120:])
                 + "</trkseg></trk>")
    r = routecheck.check(holes)
    assert any("Lücke" in f["text"] for f in r["findings"] if f["level"] == "warn")


def test_loop():
    pts = course(flat_km=1, climb_km=0.5)
    r = routecheck.check(gpx(pts + pts[::-1]))
    assert r["stats"]["loop"]


def test_what_trailbridge_cannot_read_is_an_error():
    for data in (b"not xml", b"<kml/>", _doc("<trk><trkseg>" + _trkpts(course()[:1]) + "</trkseg></trk>"),
                 _doc('<trk><trkseg><trkpt lat="50" lon="8"/><trkpt lon="8.1"/></trkseg></trk>'),
                 b'<?xml version="1.0"?><!DOCTYPE gpx [<!ENTITY a "b">]><gpx/>',
                 gpx(course()[:3])):                       # 50 m long
        assert routecheck.check(data)["verdict"] == "error", data[:80]


# --- waypoints for the page ---------------------------------------------------------------

def _with_wpts(wpts: str) -> bytes:
    """The 6 km course (north along 8.6 E) with wpt in front of the track, as BRouter-Web writes it."""
    body = "<trk><trkseg>" + _trkpts(course()) + "</trkseg></trk>"
    return _doc(wpts + body)


def _north(m: float, east_m: float = 0.0) -> str:
    return f'lat="{50.0 + m / 111320.0:.6f}" lon="{8.6 + east_m / 71700.0:.6f}"'


def test_named_via_poi_and_machine_names_are_told_apart():
    data = _with_wpts(
        f'<wpt {_north(0)}><name>from</name><type>from</type></wpt>'
        f'<wpt {_north(2000)}><name>via1</name><type>via</type></wpt>'
        f'<wpt {_north(3000)}><name>Kaffeepause</name><type>via</type></wpt>'
        f'<wpt {_north(4000, 300)}><name>Brunnen</name></wpt>'
        f'<wpt {_north(6000)}><name>to</name><type>to</type></wpt>')
    w = routecheck.check(data)["stats"]["wpts"]
    assert [(x["name"], x["kind"]) for x in w] == [
        ("from", "auto"), ("via1", "auto"), ("Kaffeepause", "named"), ("Brunnen", "poi"), ("to", "auto")]
    assert abs(w[2]["km"] - 3.0) < 0.02
    assert abs(w[2]["km"] + w[2]["to_end_km"] - 5.975) < 0.01      # the course is 239 x 25 m
    assert abs(w[3]["km"] - 4.0) < 0.02                      # projected onto the track
    assert 290 <= w[3]["off_m"] <= 310                       # a place beside the route
    assert w[0]["km"] == 0 and w[4]["to_end_km"] == 0


def test_a_real_name_on_a_from_point_is_a_name():
    w = routecheck.check(_with_wpts(f'<wpt {_north(0)}><name>Zuhause</name><type>from</type></wpt>'))
    assert w["stats"]["wpts"][0]["kind"] == "named"


def test_destination_of_a_loop_is_at_the_end_not_the_start():
    pts = [(50.0, 8.6, 100.0), (50.01, 8.6, 100.0), (50.01, 8.61, 100.0), (50.0, 8.61, 100.0), (50.0, 8.6, 100.0)]
    data = _doc(f'<wpt lat="50.0" lon="8.6"><name>to</name><type>to</type></wpt>'
                f'<wpt lat="50.0" lon="8.6"><name>from</name><type>from</type></wpt>'
                "<trk><trkseg>" + _trkpts(pts) + "</trkseg></trk>")
    first, last = sorted(routecheck.check(data)["stats"]["wpts"], key=lambda x: x["type"])
    assert first["type"] == "from" and first["km"] == 0
    assert last["type"] == "to" and last["to_end_km"] == 0 and last["km"] > 3


def test_a_wpt_far_from_the_route_is_counted_not_listed():
    data = _with_wpts(f'<wpt {_north(3000, 5000)}><name>Anderswo</name></wpt>'
                      f'<wpt {_north(3000)}><name>Hier</name></wpt>')
    st = routecheck.check(data)["stats"]
    assert [w["name"] for w in st["wpts"]] == ["Hier"] and st["waypoints_far"] == 1


def test_special_characters_stay_as_they_are():
    name = "Café | 10% ; a&b=c <b>x</b>"
    data = _with_wpts(f'<wpt {_north(1000)}><name>Caf&#233; | 10% ; a&amp;b=c &lt;b&gt;x&lt;/b&gt;</name></wpt>')
    assert routecheck.check(data)["stats"]["wpts"][0]["name"] == name


def test_osmand_turn_hints_are_no_waypoints():
    rte = '<rte><rtept lat="50.0" lon="8.6"><desc>left</desc></rtept></rte>'
    data = _doc(rte + "<trk><trkseg>" + _trkpts(course()) + "</trkseg></trk>")
    assert "wpts" not in routecheck.check(data)["stats"]


def test_no_wpt_no_list_and_the_map_track_is_short():
    st = routecheck.check(gpx(course()))["stats"]
    assert "wpts" not in st
    assert 2 <= len(st["track"]) <= routecheck.MAP_POINTS + 1
    assert st["track"][0] == [50.0, 8.6] and st["track"][-1][0] > 50.05
