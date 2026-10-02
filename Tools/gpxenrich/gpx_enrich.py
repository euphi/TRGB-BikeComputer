#!/usr/bin/env python3
"""Turn a bare GPX track into one TrailBridge can navigate: real turn hints
from BRouter, the track snapped onto OSM ways, the file's waypoints as named
hints along the route.

    Tools/gpxenrich/gpx_enrich.py Tools/TestData/SB_GF_26_LG.gpx
    -> Tools/TestData/SB_GF_26_LG_nav.gpx

A race course rides against one-ways and through pedestrian zones, so a stock
routing profile leaves it here and there. This script routes it with its own
profile (follow.brf next to this file: no access rules, no barriers, one-ways
passable) through via points taken from the track, and adds via points until
the routed line stays on the track.

BRouter runs locally: the script uses a server already listening on
localhost:17777 (it must know the profile "follow"), else it starts its own
for the run. What that needs -- BRouter itself (7 MB) and the 5x5 degree data
tiles under the track (Tuscany: 80 MB) -- is downloaded once into
--brouter-dir. The tiles are rebuilt from OSM weekly; delete them there to get
fresh ones. Needs java on the PATH.

What ends up in the output:

  <wpt>    the input's waypoints, with <desc> = km along the route -- for map
           apps; TrailBridge ignores them.
  <rte>    the routed line, point by point, with BRouter's elevations and
           without times (the input's are usually made up, and TrailBridge's
           test ride would take its speed from them). The points where
           something happens carry the hint: BRouter's turns as
           <extensions><turn> (OsmAnd codes: TL, TSHR, RNDB2, ...); a waypoint
           as <name> and <turn>C</turn> -- "straight on" is the only manoeuvre
           TrailBridge's GpxParser keeps for its name alone. A waypoint next
           to a turn gives that turn its name instead.

There is no <trk>, on purpose. TrailBridge takes named hints from <rtept>
only, and a file with the line as <trk> and the hints as a separate <rte>
holds two lines for every other program: the track, and the hint points
joined up as the crow flies.

Read the messages: a stretch reported as off the track is where OSM has no way
the course could have taken (new road, building site, a square).
"""

import argparse
import bisect
import contextlib
import math
import pathlib
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
import zipfile
from xml.sax.saxutils import escape

LOCAL_URL = "http://localhost:17777"
BROUTER_ZIP = "https://github.com/abrensch/brouter/releases/download/v1.7.10/brouter-1.7.10.zip"
SEGMENTS_URL = "https://brouter.de/brouter/segments4/"

GPX_NS = "http://www.topografix.com/GPX/1/1"
NS = {"g": GPX_NS}
EARTH_M = 6371000.0

# A hint and a waypoint closer than this are one thing on the display.
MERGE_M = 40.0
# Via points closer together than this gain nothing.
MIN_LEG_M = 30.0
# A stretch further off the track than this is on the wrong way (or on none);
# below it the track and OSM may just disagree about where the road is.
OFF_TRACK_M = 35.0
# ...and for such a disagreement, legs aren't split below this length.
SETTLE_LEG_M = 150.0
# BRouter codes TrailBridge's Maneuver.fromOsmAndXml doesn't know.
TURN_ALIASES = {"EL": "KL", "ER": "KR"}


class Line:
    """A polyline in local metres with the distance along it."""

    def __init__(self, latlon, origin=None):
        self.latlon = latlon
        self.lat0, self.lon0 = origin or latlon[0]
        self.kx = EARTH_M * math.cos(math.radians(self.lat0)) * math.pi / 180.0
        self.ky = EARTH_M * math.pi / 180.0
        self.xy = [self.to_xy(la, lo) for la, lo in latlon]
        self.cum = [0.0]
        for (x0, y0), (x1, y1) in zip(self.xy, self.xy[1:]):
            self.cum.append(self.cum[-1] + math.hypot(x1 - x0, y1 - y0))
        self.total = self.cum[-1]

    def to_xy(self, lat, lon):
        return ((lon - self.lon0) * self.kx, (lat - self.lat0) * self.ky)

    def nearest(self, p, lo=0.0, hi=None):
        """(distance along, distance to the line, segment index) of the point
        on the line closest to p, searched between lo and hi metres along."""
        hi = self.total if hi is None else hi
        first = max(0, bisect.bisect_left(self.cum, lo) - 1)
        last = min(len(self.xy) - 1, bisect.bisect_right(self.cum, hi))
        best = (self.cum[first], float("inf"), first)
        px, py = p
        for i in range(first, last):
            ax, ay = self.xy[i]
            bx, by = self.xy[i + 1]
            dx, dy = bx - ax, by - ay
            len2 = dx * dx + dy * dy
            t = 0.0 if len2 == 0 else max(0.0, min(1.0, ((px - ax) * dx + (py - ay) * dy) / len2))
            d = math.hypot(ax + t * dx - px, ay + t * dy - py)
            if d < best[1]:
                best = (self.cum[i] + t * (self.cum[i + 1] - self.cum[i]), d, i)
        return best

    def at(self, along):
        """(lat, lon) at a distance along the line."""
        i = max(0, min(len(self.cum) - 2, bisect.bisect_right(self.cum, along) - 1))
        span = self.cum[i + 1] - self.cum[i]
        t = 0.0 if span <= 0 else max(0.0, min(1.0, (along - self.cum[i]) / span))
        (la0, lo0), (la1, lo1) = self.latlon[i], self.latlon[i + 1]
        return (la0 + t * (la1 - la0), lo0 + t * (lo1 - lo0))


# ---- input ----------------------------------------------------------------

def load(path):
    """-> (track name, [(lat, lon, ele|None)], [(name, lat, lon, time|None)],
    [time|None per track point])"""
    root = ET.parse(path).getroot()
    pts, times = [], []
    for p in root.iterfind(".//g:trkpt", NS):
        ele = p.findtext("g:ele", namespaces=NS)
        pts.append((float(p.get("lat")), float(p.get("lon")), float(ele) if ele else None))
        times.append(p.findtext("g:time", namespaces=NS))
    if len(pts) < 2:
        sys.exit(f"{path}: kein Track (trkpt) gefunden")
    wpts = []
    for w in root.iterfind("g:wpt", NS):
        # TCX Converter puts the time of the track point a waypoint sits on
        # into <desc>; that pins it down even where the course passes twice.
        stamp = w.findtext("g:time", namespaces=NS) or w.findtext("g:desc", namespaces=NS)
        wpts.append((w.findtext("g:name", default="", namespaces=NS).strip(),
                     float(w.get("lat")), float(w.get("lon")), stamp))
    name = root.findtext("g:trk/g:name", default="", namespaces=NS).strip()
    return name, pts, wpts, times


def waypoint_indices(track, wpts, times):
    """Track point index each waypoint belongs to."""
    by_time = {}
    for i, t in enumerate(times):
        if t:
            by_time.setdefault(t, i)
    out = []
    prev_along = 0.0
    for name, lat, lon, stamp in wpts:
        if stamp in by_time:
            i = by_time[stamp]
        else:
            # In file order = in riding order: first look ahead of the last one.
            p = track.to_xy(lat, lon)
            along, dist, i = track.nearest(p, prev_along)
            if dist > 100:
                along, dist, i = track.nearest(p)
            if along - track.cum[i] > track.cum[i + 1] - along:
                i += 1
        prev_along = track.cum[i]
        out.append(i)
    return out


# ---- BRouter --------------------------------------------------------------

def tiles(pts):
    """Names of the 5x5 degree BRouter data tiles the track touches."""
    names = set()
    for lat, lon, _ in pts:
        la, lo = math.floor(lat / 5) * 5, math.floor(lon / 5) * 5
        names.add(f"{'E' if lo >= 0 else 'W'}{abs(lo)}_{'N' if la >= 0 else 'S'}{abs(la)}.rd5")
    return sorted(names)


def download(url, dest, log):
    log(f"lade {url}")
    part = dest.with_name(dest.name + ".part")
    with urllib.request.urlopen(url, timeout=60) as r, open(part, "wb") as f:
        shutil.copyfileobj(r, f)
    part.rename(dest)


def listening(url):
    u = urllib.parse.urlparse(url)
    try:
        socket.create_connection((u.hostname, u.port or 80), 1).close()
        return True
    except OSError:
        return False


@contextlib.contextmanager
def local_brouter(home, pts, log):
    """A BRouter server of our own for the duration of the run."""
    home = pathlib.Path(home).expanduser()
    home.mkdir(parents=True, exist_ok=True)
    if not list(home.glob("brouter-*/brouter-*-all.jar")):
        download(BROUTER_ZIP, home / "brouter.zip", log)
        with zipfile.ZipFile(home / "brouter.zip") as z:
            z.extractall(home)
        (home / "brouter.zip").unlink()
    jar = sorted(home.glob("brouter-*/brouter-*-all.jar"))[-1]
    segments = home / "segments4"
    segments.mkdir(exist_ok=True)
    for tile in tiles(pts):
        if not (segments / tile).exists():
            download(SEGMENTS_URL + tile, segments / tile, log)
    with tempfile.TemporaryDirectory() as profiles:
        shutil.copy(jar.parent / "profiles2" / "lookups.dat", profiles)
        shutil.copy(pathlib.Path(__file__).with_name("follow.brf"), profiles)
        port = str(urllib.parse.urlparse(LOCAL_URL).port)
        server = subprocess.Popen(
            ["java", "-cp", str(jar), "btools.server.RouteServer",
             str(segments), profiles, profiles, port, "2", "localhost"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            for _ in range(100):
                if listening(LOCAL_URL) or server.poll() is not None:
                    break
                time.sleep(0.2)
            if server.poll() is not None or not listening(LOCAL_URL):
                sys.exit("BRouter-Server startet nicht (java installiert?)")
            yield
        finally:
            server.terminate()
            server.wait(10)


class BRouter:
    def __init__(self, url, profile):
        self.url = url.rstrip("/")
        self.profile = profile
        self.requests = 0

    def route(self, latlons):
        """-> ([(lat, lon, ele|None)], [(track index, turn code, angle)])"""
        lonlats = "|".join(f"{lo:.6f},{la:.6f}" for la, lo in latlons)
        url = (f"{self.url}/brouter?lonlats={lonlats}&profile={self.profile}"
               "&alternativeidx=0&format=gpx&timode=3")
        self.requests += 1
        try:
            with urllib.request.urlopen(url, timeout=300) as r:
                body = r.read()
        except urllib.error.HTTPError as e:
            # 400 + a message: no route between these points. Anything else
            # (500 without a word: the profile is missing or broken) won't
            # get better with other points.
            body = e.read()
            if e.code != 400 or b"datafile" in body:
                sys.exit(f"BRouter ({self.url}): HTTP {e.code} {body.decode('utf-8', 'replace').strip()}"
                         f" -- Profil '{self.profile}' und Kartendaten vorhanden?")
        except urllib.error.URLError as e:
            sys.exit(f"BRouter nicht erreichbar ({self.url}): {e}")
        if not body.lstrip().startswith(b"<"):
            # The caller decides what "no route" means for its leg.
            raise RuntimeError(body.decode("utf-8", "replace").strip())
        root = ET.fromstring(body)
        trk = []
        for p in root.iterfind(".//g:trkpt", NS):
            ele = p.findtext("g:ele", namespaces=NS)
            trk.append((float(p.get("lat")), float(p.get("lon")), float(ele) if ele else None))
        hints = []
        for p in root.iterfind(".//g:rtept", NS):
            turn = p.findtext("g:extensions/g:turn", namespaces=NS)
            offset = p.findtext("g:extensions/g:offset", namespaces=NS)
            if turn and offset is not None:
                angle = p.findtext("g:extensions/g:turn-angle", default="0", namespaces=NS)
                hints.append((int(offset), turn.strip(), float(angle)))
        return trk, hints


def leg_error(track, a, b, routed):
    """How far the routed leg strays from the track between a and b metres along.

    -> (worst distance, distance along the track to split at)
    """
    worst, at = 0.0, None
    # Track points off the routed line.
    first = bisect.bisect_left(track.cum, a)
    last = bisect.bisect_right(track.cum, b)
    for i in range(first, last):
        d = routed.nearest(track.xy[i])[1]
        if d > worst:
            worst, at = d, track.cum[i]
    # Routed line off the track: an excursion all track points are far from.
    # Where the track is coarse (hundreds of metres between points on a
    # winding road), the road legitimately leaves the chord.
    step = 15.0
    n = max(1, int(routed.total / step))
    for k in range(n + 1):
        la, lo = routed.at(routed.total * k / n)
        along, d, seg = track.nearest(track.to_xy(la, lo), a, b)
        seg_len = track.cum[seg + 1] - track.cum[seg]
        d -= 0.5 * seg_len if seg_len > 40 else 0.0
        if d > worst:
            worst, at = d, along
    return worst, at


def spurs(routed):
    """Distances along the line where it goes into a side road and straight
    back out: BRouter snapped a via point onto the wrong way at a junction."""
    # BRouter repeats the point where a via point sits.
    idx = [i for i, p in enumerate(routed.xy) if i == 0 or p != routed.xy[i - 1]]
    xy = routed.xy
    return [routed.cum[i] for h, i, j in zip(idx, idx[1:], idx[2:])
            if math.hypot(xy[j][0] - xy[h][0], xy[j][1] - xy[h][1]) < 0.5]


def route_line(track, router, alongs):
    trk, hints = router.route([track.at(a) for a in alongs])
    return Line([p[:2] for p in trk], (track.lat0, track.lon0)), trk, hints


def find_vias(track, router, tolerance, first_step_m, log):
    """Distances along the track to route through so the result follows it.

    -> (vias, [(from, to, error)] of stretches that stay off the track)
    """
    vias = [min(k * first_step_m, track.total) for k in range(int(track.total / first_step_m) + 1)]
    if track.total - vias[-1] > MIN_LEG_M:
        vias.append(track.total)
    else:
        vias[-1] = track.total

    off = []
    todo = list(zip(vias, vias[1:]))
    done = set()
    while todo:
        a, b = todo.pop()
        try:
            err, at = leg_error(track, a, b, route_line(track, router, [a, b])[0])
        except RuntimeError as e:
            log(f"  km {a / 1000:.2f}-{b / 1000:.2f}: {e}")
            err, at = float("inf"), None
        if err <= tolerance:
            done.add((a, b))
            continue
        # A short leg that is still a little off: there is no way closer to
        # the track (it was drawn on another map, or OSM has moved since).
        # More via points won't change that.
        if b - a < (SETTLE_LEG_M if err < OFF_TRACK_M else 2 * MIN_LEG_M):
            off.append((a, b, err))
            done.add((a, b))
            continue
        # Split where it strays most, but not right at an end.
        if at is None or not a + MIN_LEG_M <= at <= b - MIN_LEG_M:
            at = (a + b) / 2
        todo.append((a, at))
        todo.append((at, b))
    return sorted({a for a, _ in done} | {track.total}), off


def fix_spurs(track, router, vias, tolerance, log):
    """Move via points that BRouter snapped onto a side road.

    A via point right on a junction can land on any of the ways meeting
    there; the route then dips into the side road and turns round. Shifting
    the point a little along the track puts it on the right way.
    """
    for attempt in range(6):
        routed, _, _ = route_line(track, router, vias)
        found = spurs(routed)
        if not found:
            break
        via_pos = []
        pos = 0.0
        for v in vias:
            pos = routed.nearest(track.to_xy(*track.at(v)), pos - 30, pos + 5000)[0]
            via_pos.append(pos)
        vias = list(vias)
        for s in found:
            k = min(range(len(vias)), key=lambda k: abs(via_pos[k] - s))
            if abs(via_pos[k] - s) > 50 or k in (0, len(vias) - 1):
                log(f"  WARNUNG km {s / 1000:.2f}: Linie kehrt um, kein Stützpunkt in der Nähe")
                continue
            if None in vias[k - 1:k + 2]:
                continue    # a neighbour just changed: next round
            a, v, b = vias[k - 1:k + 2]
            # Elsewhere on the track, or not at all (None): whatever follows
            # the track best without turning round.
            best = None
            for shift in (8, -8, 15, -15, 25, -25, 40, -40, 60, -60, None):
                if shift is None:
                    line = route_line(track, router, [a, b])[0]
                elif a + MIN_LEG_M / 2 <= v + shift <= b - MIN_LEG_M / 2:
                    line = route_line(track, router, [a, v + shift, b])[0]
                else:
                    continue
                err = leg_error(track, a, b, line)[0]
                if not spurs(line) and err < OFF_TRACK_M and (best is None or err < best[0] - 0.5):
                    best = (err, shift)
            if best:
                vias[k] = None if best[1] is None else v + best[1]
            elif attempt == 5:
                log(f"  WARNUNG km {s / 1000:.2f}: Linie kehrt um (Stützpunkt auf Nebenweg), "
                    "nicht behebbar")
        vias = [v for v in vias if v is not None]
    return vias


# ---- output ---------------------------------------------------------------

def match_progress(track, routed):
    """Distance along the routed line for every track point, in riding order."""
    out = []
    pos = 0.0
    for p in track.xy:
        along, dist, _ = routed.nearest(p, pos - 30, pos + 600)
        if dist > 60:
            along, dist, _ = routed.nearest(p, pos - 30, pos + 3000)
        pos = max(pos, along) if dist <= 60 else pos
        out.append((pos, dist))
    return out


SHARPNESS = {"KL": 0, "KR": 0, "TSLL": 1, "TSLR": 1, "TL": 2, "TR": 2,
             "TSHL": 3, "TSHR": 3, "TU": 4, "TRU": 4}


def side(turn):
    """-1 left, +1 right, 0 neither (straight on, roundabout)."""
    if turn in ("TL", "TSLL", "TSHL", "KL", "TU"):
        return -1
    if turn in ("TR", "TSLR", "TSHR", "KR", "TRU"):
        return 1
    return 0


def heading_change(routed, a, b):
    """Degrees the line turns between a and b metres along (positive = right),
    measured over 25 m before and after."""
    def bearing(p, q):
        (x0, y0), (x1, y1) = routed.to_xy(*routed.at(p)), routed.to_xy(*routed.at(q))
        return math.degrees(math.atan2(x1 - x0, y1 - y0))
    d = bearing(b + 5, b + 30) - bearing(a - 30, a - 5)
    return (d + 180) % 360 - 180


def tidy_hints(routed, hints):
    """BRouter's hints as [[along, turn code, name]], with the ones that are
    one manoeuvre on the road made one on the display.

    BRouter gives a hint per junction node. A corner drawn as two or three
    nodes a few metres apart (a slip lane, a traffic island, a via point in
    between) comes out as "slight right, slight right, slight right".
    """
    steps = sorted([routed.cum[min(idx, len(routed.cum) - 1)], TURN_ALIASES.get(turn, turn), ""]
                   for idx, turn, _ in hints)
    out = []
    for s in steps:
        prev = out[-1] if out else None
        if prev is None or s[0] - prev[0] > MERGE_M or prev[1] == "C" or s[1] == "C":
            out.append(s + [s[0]])      # [3]: where the manoeuvre ends
            continue
        if s[1].startswith("RN"):
            # The bend into a roundabout is part of the roundabout.
            out[-1] = s + [s[0]]
        elif side(s[1]) == side(prev[1]) != 0:
            prev[3] = s[0]
            turn = abs(heading_change(routed, prev[0], prev[3]))
            left = side(s[1]) < 0
            strongest = max(prev[1], s[1], key=SHARPNESS.get)
            if turn >= 120:
                prev[1] = "TSHL" if left else "TSHR"
            elif turn >= 60:
                prev[1] = strongest if SHARPNESS[strongest] > 2 else "TL" if left else "TR"
            else:
                prev[1] = strongest
        else:
            out.append(s + [s[0]])
    return [s[:3] for s in out]


def build_steps(steps, wpts, wpt_along):
    """Adds the waypoints to the hints -> [(along, turn code, name)] sorted along the route."""
    for (name, *_), along in zip(wpts, wpt_along):
        if not name:
            continue
        near = [s for s in steps if abs(s[0] - along) <= MERGE_M]
        if near:
            s = min(near, key=lambda s: abs(s[0] - along))
            s[2] = f"{s[2]} / {name}" if s[2] else name
        else:
            steps.append([along, "C", name])
    steps.sort(key=lambda s: s[0])
    return steps


def route_points(routed, eles, steps):
    """The line's points with the steps on them -> [(lat, lon, ele, turn|None, name)].
    A step between two points gets a point of its own."""
    def ele_at(along, i):
        if eles[i - 1] is None or eles[i] is None:
            return eles[i]
        span = routed.cum[i] - routed.cum[i - 1]
        t = 0.0 if span <= 0 else (along - routed.cum[i - 1]) / span
        return eles[i - 1] + t * (eles[i] - eles[i - 1])

    out = []
    k = 0
    for i, ((lat, lon), ele) in enumerate(zip(routed.latlon, eles)):
        while k < len(steps) and steps[k][0] < routed.cum[i] - 0.5:
            along, turn, name = steps[k]
            out.append((*routed.at(along), ele_at(along, i), turn, name))
            k += 1
        if k < len(steps) and steps[k][0] <= routed.cum[i] + 0.5:
            out.append((lat, lon, ele, steps[k][1], steps[k][2]))
            k += 1
        else:
            out.append((lat, lon, ele, None, ""))
    # Anything left sits on the last point.
    out.extend((*routed.latlon[-1], eles[-1], turn, name) for _, turn, name in steps[k:])
    return out


def write_gpx(path, name, routed, eles, steps, wpts, wpt_along, source):
    w = []
    w.append('<?xml version="1.0" encoding="UTF-8"?>')
    w.append(f'<gpx xmlns="{GPX_NS}" creator="TRGB-BikeComputer gpx_enrich.py" version="1.1">')
    w.append(" <metadata>")
    w.append(f"  <name>{escape(name)}</name>")
    w.append(f"  <desc>{escape(source)}: auf OSM-Wege gelegt, Abbiegehinweise von BRouter</desc>")
    w.append(" </metadata>")
    for (wname, lat, lon, _), along in zip(wpts, wpt_along):
        w.append(f' <wpt lat="{lat:.6f}" lon="{lon:.6f}">')
        w.append(f"  <name>{escape(wname)}</name>")
        w.append(f"  <desc>km {along / 1000:.1f}</desc>")
        w.append(" </wpt>")
    w.append(" <rte>")
    w.append(f"  <name>{escape(name)}</name>")
    for lat, lon, ele, turn, sname in route_points(routed, eles, steps):
        inner = "" if ele is None else f"<ele>{ele:.1f}</ele>"
        if sname:
            inner += f"<name>{escape(sname)}</name>"
        if turn:
            inner += f"<extensions><turn>{turn}</turn></extensions>"
        w.append(f'  <rtept lat="{lat:.6f}" lon="{lon:.6f}">{inner}</rtept>' if inner
                 else f'  <rtept lat="{lat:.6f}" lon="{lon:.6f}"/>')
    w.append(" </rte>")
    w.append("</gpx>")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(w) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("gpx")
    ap.add_argument("-o", "--output", help="Standard: <Eingabe>_nav.gpx")
    ap.add_argument("--brouter", default=LOCAL_URL,
                    help="BRouter-Server (Standard: %(default)s, wird bei Bedarf gestartet)")
    ap.add_argument("--brouter-dir", default="~/.cache/brouter",
                    help="BRouter und Kartendaten für den eigenen Server (Standard: %(default)s)")
    ap.add_argument("--profile", default="follow")
    ap.add_argument("--tolerance", type=float, default=12.0,
                    help="so weit darf die geroutete Linie vom Track weg (m)")
    ap.add_argument("--skip", default="",
                    help="Wegpunkte, deren Name auf diesen regulären Ausdruck passt, "
                         "nicht als Hinweis einbauen (z.B. '^KM_')")
    ap.add_argument("--name", help="Name der Route (Standard: Dateiname)")
    args = ap.parse_args()

    log = lambda s: print(s, file=sys.stderr)
    trk_name, pts, wpts, times = load(args.gpx)
    track = Line([p[:2] for p in pts])
    log(f"{args.gpx}: {len(pts)} Punkte, {track.total / 1000:.2f} km, {len(wpts)} Wegpunkte")
    wpt_idx = waypoint_indices(track, wpts, times)

    with contextlib.ExitStack() as stack:
        if not listening(args.brouter):
            if args.brouter != LOCAL_URL:
                sys.exit(f"BRouter nicht erreichbar: {args.brouter}")
            stack.enter_context(local_brouter(args.brouter_dir, pts, log))
        router = BRouter(args.brouter, args.profile)
        vias, off = find_vias(track, router, args.tolerance, 2000.0, log)
        vias = fix_spurs(track, router, vias, args.tolerance, log)
        log(f"{len(vias)} Stützpunkte nach {router.requests} Teilrouten")

        try:
            routed, trk, hints = route_line(track, router, vias)
        except RuntimeError as e:
            sys.exit(f"BRouter findet keine durchgehende Route: {e}")
        progress = match_progress(track, routed)
        dists = sorted(d for _, d in progress)
        log(f"geroutet: {len(trk)} Punkte, {routed.total / 1000:.2f} km; Track-Punkte neben der Linie: "
            f"Median {dists[len(dists) // 2]:.1f} m, 99 % {dists[int(len(dists) * 0.99)]:.1f} m, max {dists[-1]:.1f} m")
        for a, b, err in sorted(off):
            if err >= OFF_TRACK_M:
                log(f"  WARNUNG km {a / 1000:.2f}-{b / 1000:.2f}: Route weicht {err:.0f} m ab und "
                    "lässt sich nicht näher heranholen (Weg fehlt in OSM?)")
        slightly = [o for o in off if o[2] < OFF_TRACK_M]
        if slightly:
            log(f"  {len(slightly)} Abschnitte ({sum(b - a for a, b, _ in slightly):.0f} m) liegen "
                f"{min(e for *_, e in slightly):.0f}-{max(e for *_, e in slightly):.0f} m neben dem Track, "
                "ohne näheren Weg in OSM: km " + ", ".join(f"{k:.1f}" for k in sorted({round(a / 1000, 1) for a, *_ in slightly})))
        for along in spurs(routed):
            log(f"  WARNUNG km {along / 1000:.2f}: Linie kehrt um (Stützpunkt auf Nebenweg?)")

    wpt_along = [progress[i][0] for i in wpt_idx]
    skip = re.compile(args.skip) if args.skip else None
    shown = [(w, a) for w, a in zip(wpts, wpt_along) if not (skip and skip.search(w[0]))]
    raw_turns = sum(1 for _, t, _ in hints if t != "C")
    steps = build_steps(tidy_hints(routed, hints), [w for w, _ in shown], [a for _, a in shown])

    out = args.output or re.sub(r"\.gpx$", "", args.gpx, flags=re.I) + "_nav.gpx"
    stem = re.sub(r".*/|\.gpx$", "", args.gpx, flags=re.I)
    write_gpx(out, args.name or stem, routed, [p[2] for p in trk], steps, wpts, wpt_along,
              trk_name or stem)
    turns = sum(1 for s in steps if s[1] != "C")
    log(f"{out}: {turns} Abbiegehinweise (von BRouter: {raw_turns}), "
        f"{sum(1 for s in steps if s[1] == 'C' and s[2])} Wegpunkt-Hinweise, "
        f"{sum(1 for s in steps if s[1] != 'C' and s[2])} Wegpunkte an einem Abbiegehinweis")


if __name__ == "__main__":
    main()
