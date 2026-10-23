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
    waypoints    wpt within WAYPOINT_MAX_OFF_ROUTE_M of the route, for the route overview

check() returns verdict + findings (German, for the page) + stats; the climbs and the
elevation profile come from training.event_profile(), the same as a goal's course.
Pure: no service, no storage.
"""

from __future__ import annotations

import datetime
import math
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
    return result(stats, name)
