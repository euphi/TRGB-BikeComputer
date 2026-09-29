"""GPX -> simulator values (bikelog sim), and the HTTP link against a fake device."""

import json
import math
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlparse

import pytest

from bikelog import sim

# 1 degree of latitude in m (haversine with the earth radius bikelog.geo uses)
M_PER_DEG_LAT = 6371000 * math.pi / 180


def _gpx(tmp_path, points, version="1.1", ext=False):
    """points: (seconds, metres north, elevation[, hr, cad])"""
    ns = "http://www.topografix.com/GPX/%s" % version.replace(".", "/")
    pts = []
    for p in points:
        s, north, ele = p[:3]
        extension = ""
        if ext:
            extension = ("<extensions><gpxtpx:TrackPointExtension><gpxtpx:hr>%d</gpxtpx:hr>"
                         "<gpxtpx:cad>%d</gpxtpx:cad></gpxtpx:TrackPointExtension></extensions>" % (p[3], p[4]))
        pts.append('<trkpt lat="%.7f" lon="8.0"><ele>%.1f</ele><time>2026-09-29T10:%02d:%02dZ</time>%s</trkpt>'
                   % (52.0 + north / M_PER_DEG_LAT, ele, s // 60, s % 60, extension))
    path = tmp_path / "t.gpx"
    path.write_text('<?xml version="1.0"?><gpx version="%s" xmlns="%s" '
                    'xmlns:gpxtpx="http://www.garmin.com/xmlschemas/TrackPointExtension/v1">'
                    "<trk><trkseg>%s</trkseg></trk></gpx>" % (version, ns, "".join(pts)))
    return path


def _steady(seconds, mps, ele_per_s=0.0, **kw):
    return [(s, s * mps, 100 + s * ele_per_s) + kw.get("extra", ()) for s in range(seconds + 1)]


def test_speed_from_geometry(tmp_path):
    track = sim.load(_gpx(tmp_path, _steady(60, 5.0)))       # 5 m/s = 18 km/h
    sample = sim.sampler(track)
    assert track.distance == pytest.approx(300, rel=1e-3)
    for s in (0, 10, 30.5, 60):
        assert sample(s).speed == pytest.approx(18.0, rel=1e-3)
    assert sample(30).cad == 80           # no cadence in the file: default while moving
    assert sample(30).hr is None


def test_gpx_1_0_and_extensions(tmp_path):
    points = [(s, s * 5.0, 100, 120 + s, 70 + s) for s in range(31)]
    track = sim.load(_gpx(tmp_path, points, version="1.0", ext=True))
    assert track.has_cad and track.has_hr
    x = sim.sampler(track)(10)
    assert (x.hr, x.cad) == (130, 80)


def test_coasting_downhill_and_standing(tmp_path):
    # 60 s level, 60 s at -6 %, 60 s standing
    pts = [(s, s * 5.0, 100) for s in range(61)]
    pts += [(60 + s, 300 + s * 5.0, 100 - s * 0.3) for s in range(1, 61)]
    pts += [(120 + s, 600, 82) for s in range(1, 61)]
    sample = sim.sampler(sim.load(_gpx(tmp_path, pts)), default_cad=85)
    assert sample(30).cad == 85
    assert sample(90).cad == 0            # coasting
    assert sample(150).speed == 0 and sample(150).cad == 0


def test_long_pause_shortened(tmp_path):
    pts = [(s, s * 5.0, 100) for s in range(11)] + [(610 + s, 50 + s * 5.0, 100) for s in range(11)]
    assert sim.load(_gpx(tmp_path, pts)).duration == 620
    track = sim.load(_gpx(tmp_path, pts), max_gap=30)
    assert track.duration == 50
    assert track.distance == pytest.approx(100, rel=1e-3)


def test_sent_follows_the_firmware_hysteresis():
    sent = sim.Sent()
    for v in (3, 5, 6, 20, 1, 0.2, 0.2):          # moving from > 5.5 until < 0.3
        sent.add(sim.Sample(v, 80, None, 0), 1.0)
    assert sent.moving_s == 3                     # 6, 20 and 1; the first 0.2 ends it
    assert sent.max_speed == 20
    assert sent.dist == pytest.approx(sum((3, 5, 6, 20, 1, 0.2, 0.2)) / 3.6)


class _FakeDevice(BaseHTTPRequestHandler):
    calls = []

    def do_GET(self):
        url = urlparse(self.path)
        _FakeDevice.calls.append((url.path, parse_qs(url.query)))
        body = b"OK"
        if url.path == "/stat/summary":
            n = len(_FakeDevice.calls)
            body = json.dumps({"TRIP": {"dist": {"net": n}, "time": {"moving": n, "total": n},
                                        "maxSpeed": 18.0, "cadence": 80.0}}).encode()
        self.send_response(200)
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *args):
        pass


def test_http_playback_against_fake_device(tmp_path):
    server = HTTPServer(("127.0.0.1", 0), _FakeDevice)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    host = "127.0.0.1:%d" % server.server_port
    try:
        track = sim.load(_gpx(tmp_path, _steady(2, 5.0)))
        before = sim.fetch_summary(host)
        sent = sim.play(track, sim.HttpLink(host))
        table = sim.compare(sent, before, sim.fetch_summary(host))
    finally:
        server.shutdown()
    sets = [q for p, q in _FakeDevice.calls if p == "/debug/sim/set"]
    assert len(sets) == 3
    assert float(sets[1]["speed"][0]) == pytest.approx(18.0, rel=1e-3)
    assert sets[1]["cad"] == ["80"] and sets[1]["hr"] == ["-"]
    assert _FakeDevice.calls[-2][0] == "/debug/sim/stop"
    assert sent.updates == 3 and sent.errors == 0
    assert "Strecke (m)" in table
