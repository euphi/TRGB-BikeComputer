"""Planned and regular routes: is a GPX good for TrailBridge's navigation, and its key figures.

Reads the file the way TrailBridge's GpxParser does (TrailBridge repo,
app/src/main/java/com/euphi/trailbridge/GpxParser.java), so what this says is what
the phone will do with it:

    geometry     the track (all trkpt) if it has >= 2 points, else the route points (rtept)
    manoeuvres   1. OsmAnd's route segments (trk/extensions/route/segment turnType)
                 2. rtept with a turn extension or turn words in type/sym/desc/name
                 3. none of these -> TrailBridge derives them from the geometry
                    (TurnDetector: angles only, no street names)
                 A track whose rtept only name the way points (bikerouter.de: start, vias,
                 destination) counts as 3, like in the app.
    waypoints    wpt within WAYPOINT_MAX_OFF_ROUTE_M of the route, for the route overview;
                 for the page all wpt up to WPT_LIST_MAX_OFF_M, with distance from start
                 (projected onto the track) -- stats["wpts"], and stats["track"] for the map
    planner      the link back to the route planner in the file, if any -- stats["planner_url"]

check() returns verdict + findings (German, for the page) + stats; the climbs and the
elevation profile come from training.event_profile(), the same as a goal's course.
Pure: no service, no storage.
"""

from __future__ import annotations

import datetime
import math
import re
import urllib.parse
import xml.etree.ElementTree as ET

from . import training
from .geo import haversine_m

#: TrailBridge: a wpt further away is a place beside the route (GpxParser).
WAYPOINT_MAX_OFF_ROUTE_M = 100.0
#: TrailBridge: further from the route than this is "off route" (RouteNavigator.OFF_ROUTE_M).
OFF_ROUTE_M = 50.0
#: TrailBridge refuses bigger files (MainActivity.MAX_GPX_BYTES).
MAX_BYTES = 20 * 1024 * 1024
#: Start and end this close: a loop.
LOOP_M = 200.0
#: Points of a track further apart than this: simplified, not a router's output.
SIMPLIFIED_M = 2000.0
#: Between two trkseg this much is a hole in the route, not a GPS hiccup.
SEGMENT_GAP_M = 200.0
#: Less than this share of points with <ele>: no usable height profile.
MIN_ELE_SHARE = 0.9

#: Page only: a wpt further away than this is not shown on the route (a place elsewhere).
WPT_LIST_MAX_OFF_M = 1000.0
#: Points of the track kept for the map (every n-th, plus the last).
MAP_POINTS = 400
#: Same distance to the route within this: the later one wins for the destination.
_TIE_M = 1.0

VERDICTS = ("ok", "warn", "error")


class _Pt:
    __slots__ = ("lat", "lon", "ele", "time", "text", "turn", "seg")

    def __init__(self, el: ET.Element, seg: int = 0):
        self.lat = _float(el.get("lat"))
        self.lon = _float(el.get("lon"))
        self.ele = self.time = self.turn = None
        self.seg = seg
        texts = []
        for c in el.iter():
            tag = _local(c.tag)
            value = (c.text or "").strip()
            if c is el or not value:
                continue
            if tag == "ele" and c in list(el):
                self.ele = _float(value)
            elif tag == "time" and c in list(el):     # not BRouter's seconds in the extensions
                self.time = _time(value)
            elif tag in ("turn", "turntype"):
                self.turn = value
            elif tag in ("name", "desc", "cmt", "type", "sym"):
                texts.append(value)
        self.text = " ".join(texts)


def _local(tag: str) -> str:
    return tag.rsplit("}", 1)[-1].lower()


def _float(value) -> float | None:
    try:
        v = float(value)
    except (TypeError, ValueError):
        return None
    return v if math.isfinite(v) else None


def _time(value: str) -> float | None:
    try:
        t = datetime.datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError:
        return None
    if t.tzinfo is None:
        t = t.replace(tzinfo=datetime.timezone.utc)
    return t.timestamp()


def _is_turn(text: str) -> bool:
    """Maneuver.fromText() says a turn (not start, destination, straight on or nothing)."""
    s = text.lower()
    left = "left" in s or "links" in s
    right = "right" in s or "rechts" in s
    if any(w in s for w in ("u-turn", "uturn", "u turn", "wenden", "umkehren",
                            "roundabout", "rotary", "kreisverkehr", "kreisel")):
        return True
    if any(w in s for w in ("arrive", "destination", "finish", "ziel", "depart")):
        return False
    return left != right


def _name(root: ET.Element) -> str | None:
    for parent in ("rte", "trk", "metadata"):
        for el in root:
            if _local(el.tag) != parent:
                continue
            for c in el:
                if _local(c.tag) == "name" and (c.text or "").strip():
                    return c.text.strip()
    return None


def _planner_url(root: ET.Element) -> str | None:
    """BRouter-Web (and bikerouter.de) writes the URL of the route as trk/link, with the route
    points in the fragment. Only such a link: it ends up on the page, the file could hold anything."""
    for el in root.iter():
        if _local(el.tag) != "link":
            continue
        href = (el.get("href") or "").strip()
        try:
            url = urllib.parse.urlsplit(href)
        except ValueError:
            continue
        if url.scheme in ("http", "https") and url.netloc and "lonlats=" in url.fragment:
            return href
    return None


def _cumulative(pts: list[_Pt]) -> list[float]:
    cum = [0.0]
    for a, b in zip(pts, pts[1:]):
        cum.append(cum[-1] + haversine_m(a.lat, a.lon, b.lat, b.lon))
    return cum


def _off_route_m(pts: list[_Pt], lat: float, lon: float) -> float:
    """Distance of (lat, lon) from the polyline (flat projection, fine at these scales)."""
    k = 111320.0 * math.cos(math.radians(lat))
    best = math.inf
    for a, b in zip(pts, pts[1:]):
        ax, ay = (a.lon - lon) * k, (a.lat - lat) * 110540.0
        bx, by = (b.lon - lon) * k, (b.lat - lat) * 110540.0
        dx, dy = bx - ax, by - ay
        len2 = dx * dx + dy * dy
        t = 0.0 if len2 == 0 else max(0.0, min(1.0, -(ax * dx + ay * dy) / len2))
        best = min(best, math.hypot(ax + t * dx, ay + t * dy))
    return best


def _project(pts: list[_Pt], cum: list[float], lat: float, lon: float,
             prefer_last: bool = False) -> tuple[float, float]:
    """(distance from the start along the route, distance off it) of the nearest point on the
    polyline (flat projection). On a tie -- a loop passes its start again -- the first match,
    or the last one for the destination."""
    k = 111320.0 * math.cos(math.radians(lat))
    best_off, best_at = math.inf, 0.0
    for i, (a, b) in enumerate(zip(pts, pts[1:])):
        ax, ay = (a.lon - lon) * k, (a.lat - lat) * 110540.0
        bx, by = (b.lon - lon) * k, (b.lat - lat) * 110540.0
        dx, dy = bx - ax, by - ay
        len2 = dx * dx + dy * dy
        t = 0.0 if len2 == 0 else max(0.0, min(1.0, -(ax * dx + ay * dy) / len2))
        off = math.hypot(ax + t * dx, ay + t * dy)
        at = cum[i] + t * (cum[i + 1] - cum[i])
        if off < best_off - _TIE_M:
            best_off, best_at = off, at
        elif off <= best_off + _TIE_M:
            best_off = min(best_off, off)
            if prefer_last:
                best_at = at
    return best_at, best_off


def _wpt_info(el: ET.Element) -> dict | None:
    """name and type of a <wpt> (None without a valid coordinate)."""
    lat, lon = _float(el.get("lat")), _float(el.get("lon"))
    if lat is None or lon is None or not (-90 <= lat <= 90 and -180 <= lon <= 180):
        return None
    name = kind = ""
    for c in el:
        tag = _local(c.tag)
        if tag == "name":
            name = (c.text or "").strip()
        elif tag == "type":
            kind = (c.text or "").strip().lower()
    return {"lat": lat, "lon": lon, "name": name, "type": kind}


def _is_auto_name(name: str, kind: str) -> bool:
    """BRouter-Web's "Waypoints" option names the route points from, via1, via2, ..., to."""
    n = name.lower()
    return not n or n == kind or (kind == "via" and re.fullmatch(r"via\d+", n) is not None)


def waypoints(geometry: list[_Pt], cum: list[float], infos: list[dict]) -> tuple[list[dict], int]:
    """The wpt for the page, nearest the start first, and how many lie too far from the route.
    Each: name, kind ("poi": a place without type; "named": a named route point; "auto": from/via/to
    with a machine name -- start, destination or an unnamed via), type, lat, lon, km (from the
    start), to_end_km, off_m."""
    total = cum[-1]
    out, far = [], 0
    for w in infos:
        km_m, off = _project(geometry, cum, w["lat"], w["lon"], prefer_last=w["type"] == "to")
        if off > WPT_LIST_MAX_OFF_M:
            far += 1
            continue
        if not w["type"]:
            kind = "poi"
        elif _is_auto_name(w["name"], w["type"]):
            kind = "auto"
        else:
            kind = "named"
        out.append({"name": w["name"], "kind": kind, "type": w["type"],
                    "lat": round(w["lat"], 6), "lon": round(w["lon"], 6),
                    "km": round(km_m / 1000, 3), "to_end_km": round(max(0.0, total - km_m) / 1000, 3),
                    "off_m": round(off)})
    out.sort(key=lambda w: (w["km"], w["name"]))
    return out, far


def _map_track(geometry: list[_Pt]) -> list[list[float]]:
    step = max(1, len(geometry) // MAP_POINTS)
    pts = geometry[::step] + ([geometry[-1]] if (len(geometry) - 1) % step else [])
    return [[round(p.lat, 5), round(p.lon, 5)] for p in pts]


def check(data: bytes, fallback_name: str = "Route") -> dict:
    """Verdict ("ok", "warn": usable with limits, "error": TrailBridge cannot use it),
    findings [{"level": "error"|"warn"|"info", "text"}] and stats of a GPX."""
    findings: list[dict] = []

    def say(level: str, text: str) -> None:
        findings.append({"level": level, "text": text})

    def result(stats: dict | None = None, name: str | None = None) -> dict:
        levels = {f["level"] for f in findings}
        verdict = "error" if "error" in levels else "warn" if "warn" in levels else "ok"
        return {"verdict": verdict, "name": name or fallback_name, "findings": findings,
                "stats": stats or {}}

    if len(data) > MAX_BYTES:
        say("error", f"Datei größer als {MAX_BYTES >> 20} MB – TrailBridge lädt sie nicht.")
        return result()
    if b"<!DOCTYPE" in data[:4096].upper():
        say("error", "Datei mit DOCTYPE – TrailBridge lehnt das ab (Schutz vor XML-Entitäten).")
        return result()
    try:
        root = ET.fromstring(data)
    except ET.ParseError as exc:
        say("error", f"Keine gültige XML-Datei: {exc}")
        return result()
    if _local(root.tag) != "gpx":
        say("error", "Keine GPX-Datei (Wurzelelement ist nicht <gpx>).")
        return result()
    name = _name(root)

    trk: list[_Pt] = []
    rte: list[_Pt] = []
    wpt: list[_Pt] = []
    wpt_infos: list[dict] = []
    osmand_turns = 0
    seg = 0
    for el in root.iter():
        tag = _local(el.tag)
        if tag == "trkseg":
            seg += 1
            trk.extend(_Pt(p, seg) for p in el if _local(p.tag) == "trkpt")
        elif tag == "rtept":
            rte.append(_Pt(el))
        elif tag == "wpt":
            wpt.append(_Pt(el))
            info = _wpt_info(el)
            if info:
                wpt_infos.append(info)
        elif tag == "segment" and el.get("turnType") and el.get("startTrkptIdx") \
                and (el.get("skipTurn") or "").lower() != "true" and el.get("turnType").strip() != "C":
            osmand_turns += 1               # trk/extensions/route/segment: OsmAnd's own turns

    geometry = trk if len(trk) >= 2 else rte
    if len(geometry) < 2:
        say("error", "Weder Track noch Route mit mindestens zwei Punkten.")
        return result(name=name)
    bad = sum(1 for p in geometry if p.lat is None or p.lon is None
              or not (-90 <= p.lat <= 90 and -180 <= p.lon <= 180) or (p.lat == 0 and p.lon == 0))
    if bad:
        say("error", f"{bad} Punkte ohne gültige Koordinate – TrailBridge setzt sie auf 0°/0°, "
                     "die Strecke springt dann in den Golf von Guinea.")
        return result(name=name)

    cum = _cumulative(geometry)
    dist = cum[-1]
    gaps = [b - a for a, b in zip(cum, cum[1:])]
    stats: dict = {
        "geometry": "trk" if geometry is trk else "rte",
        "points": len(geometry),
        "distance_km": round(dist / 1000, 1),
        "max_spacing_m": round(max(gaps)),
        "median_spacing_m": round(sorted(gaps)[len(gaps) // 2], 1),
        "start": [round(geometry[0].lat, 6), round(geometry[0].lon, 6)],
        "end": [round(geometry[-1].lat, 6), round(geometry[-1].lon, 6)],
        "loop": haversine_m(geometry[0].lat, geometry[0].lon,
                            geometry[-1].lat, geometry[-1].lon) <= LOOP_M,
    }
    if dist < 100:
        say("error", f"Strecke nur {dist:.0f} m lang.")
        return result(stats, name)

    # --- geometry ---
    if geometry is rte:
        say("warn", f"Nur Routenpunkte (rte, {len(rte)} Punkte), kein Track: zwischen den Punkten "
                    "kennt TrailBridge den Weg nicht, nur die Luftlinie.")
    elif stats["max_spacing_m"] > SIMPLIFIED_M:
        # Routers (BRouter, OsmAnd) put points only where the way bends, a few hundred metres
        # apart on a straight road are normal. Kilometres are a simplified track.
        say("warn", f"Größter Punktabstand {stats['max_spacing_m'] / 1000:.1f} km – vereinfachter Track? "
                    f"Wo der Weg dazwischen nicht gerade ist, liegt er leicht mehr als {OFF_ROUTE_M:.0f} m "
                    "neben der Linie, und TrailBridge meldet „abseits der Route“.")
    if geometry is trk and seg > 1:
        holes = [haversine_m(a.lat, a.lon, b.lat, b.lon) for a, b in zip(trk, trk[1:]) if a.seg != b.seg]
        big = [h for h in holes if h > SEGMENT_GAP_M]
        stats["segments"] = seg
        if big:
            say("warn", f"{len(big)} Lücke(n) zwischen Track-Segmenten, größte {max(big) / 1000:.1f} km – "
                        "TrailBridge verbindet sie mit einer Geraden.")
        else:
            say("info", f"{seg} Track-Segmente, lückenlos aneinander.")

    # --- manoeuvres ---
    rte_turns = sum(1 for p in rte if (p.turn and p.turn.strip().upper() != "C") or _is_turn(p.text))
    if geometry is trk and osmand_turns:
        stats["turns"], stats["turn_source"] = osmand_turns, "osmand"
        say("info", f"{osmand_turns} Abbiegehinweise von OsmAnd in der Datei.")
    elif rte and (geometry is rte or rte_turns):
        stats["turns"], stats["turn_source"] = rte_turns, "rtept"
        if rte_turns:
            say("info", f"{rte_turns} Abbiegehinweise in den Routenpunkten (rtept).")
        else:
            say("warn", "Die Routenpunkte sagen nichts übers Abbiegen – ohne Track leitet TrailBridge "
                        "auch keine Hinweise aus der Geometrie ab.")
    else:
        stats["turns"], stats["turn_source"] = None, "geometry"
        say("info", "Keine Abbiegehinweise in der Datei: TrailBridge leitet sie aus den Winkeln der "
                    "Strecke ab (ohne Straßennamen)."
            + (" Die Routenpunkte nennen nur Start, Zwischenziele und Ziel." if rte else ""))

    # --- elevation ---
    with_ele = sum(1 for p in geometry if p.ele is not None)
    stats["ele_share"] = round(with_ele / len(geometry), 2)
    profile = None
    if with_ele / len(geometry) >= MIN_ELE_SHARE:
        try:
            profile = training.event_profile(data)
        except ValueError:
            profile = None
    if profile:
        stats.update({k: profile[k] for k in ("ascent_m", "min_ele_m", "max_ele_m", "climbs", "profile")})
        if profile["climbs"]:
            say("info", f"{len(profile['climbs'])} Anstieg(e) mit Kategorie – TrailBridge sagt sie an.")
    else:
        say("warn", "Keine (vollständigen) Höhen: kein Höhenprofil und keine Anstiege auf dem "
                    "Fahrradcomputer.")

    # --- times ---
    times = [p.time for p in geometry if p.time is not None]
    if len(times) >= 2 and len(times) >= 0.9 * len(geometry) and times[-1] > times[0]:
        stats["duration_s"] = round(times[-1] - times[0])
        say("info", f"Zeitstempel in der Datei: Restzeit aus der geplanten Fahrzeit "
                    f"({stats['duration_s'] // 3600}:{stats['duration_s'] % 3600 // 60:02d} h).")

    # --- waypoints ---
    if wpt:
        on = [p for p in wpt if p.lat is not None and p.lon is not None
              and _off_route_m(geometry, p.lat, p.lon) <= WAYPOINT_MAX_OFF_ROUTE_M]
        stats["waypoints"], stats["waypoints_off_route"] = len(on), len(wpt) - len(on)
        text = f"{len(on)} Wegpunkt(e) an der Strecke für die Streckenübersicht"
        if len(wpt) > len(on):
            text += (f", {len(wpt) - len(on)} weiter als {WAYPOINT_MAX_OFF_ROUTE_M:.0f} m entfernt "
                     "(zeigt TrailBridge nicht)")
        say("info", text + ".")
    if wpt_infos:
        stats["wpts"], far = waypoints(geometry, cum, wpt_infos)
        if far:
            stats["waypoints_far"] = far
    stats["track"] = _map_track(geometry)
    planner_url = _planner_url(root)
    if planner_url:
        stats["planner_url"] = planner_url
    return result(stats, name)
