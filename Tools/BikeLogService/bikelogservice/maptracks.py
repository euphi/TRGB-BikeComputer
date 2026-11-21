"""The map: every track ridden, as visited tiles and as a heatmap (overlay tiles for BRouter-Web
and the /map page). The drawing is bikelog.explorer; this module keeps the tracks.

Three sources, one store under <data>/map/:

  ride     the service's own rides (and the GPX imports among them), followed automatically:
           sync_rides() after every pull/export. Read from the exported GPX (sanitised) or,
           without one, from the log records. Test and idle sessions stay off the map.
  upload   GPX files uploaded for the map only (PUT /api/v1/map/tracks, also .zip and .gz):
           older rides from Garmin, Strava & Co. that need no report. Kept as upload/<sha>.gpx.
  komoot   the one-off import of all recorded Komoot tours (komoot_import(); bike sports
           unless asked otherwise). Kept as komoot/<tour id>.gpx; importing again only
           fetches tours not yet here.

An upload marked "mixed" is a phone's all-day recording (walking, car, train): add_mixed()
keeps only the trips bikelog.tripmode judges as cycling, each stored as an upload of its own.

index.json lists the tracks, points/<id>.bin holds each one's thinned points (int32 lat/lon
in 1e-6 degrees, explorer.THIN_M apart). A ride recorded at the same time as one from a
better source is a duplicate (a BikeLog ride uploaded to Komoot comes back with the import)
and counts once: explorer.duplicates(), ride before upload before komoot.

MAP_VERSION: raise it when how a track is read or thinned changes -- the store then reads
every track again from its source on the next start.
"""

from __future__ import annotations

import base64
import datetime
import gzip
import hashlib
import io
import json
import logging
import threading
import time
import urllib.parse
import urllib.request
import zipfile
from array import array
from collections import OrderedDict
from dataclasses import asdict, dataclass, field
from pathlib import Path

import xml.etree.ElementTree as ET

from bikelog import explorer, tripmode

from .storage import Session, Storage

log = logging.getLogger("bikelog.map")

MAP_VERSION = 1
SOURCES = {"ride": "BikeLog", "upload": "Hochgeladen", "komoot": "Komoot"}
PRIORITY = {"ride": 0, "upload": 1, "komoot": 2}
LAYERS = {"visited": explorer.EXPLORER_ZOOM, "squadrats": explorer.SQUADRAT_ZOOM, "heat": None}
#: below this zoom the squadrats layer stays empty (a 200 m tile would be under 2 px)
MIN_ZOOM = {"visited": 5, "squadrats": 10, "heat": 3}
MAX_ZOOM = 20
#: rendered tiles kept in memory (a few KB each)
TILE_CACHE = 4000

KOMOOT_API = "https://api.komoot.de"
#: Komoot's sport keys that are a bicycle (kompy.constants.activities)
BIKE_SPORTS = {"racebike", "e_racebike", "touringbicycle", "e_touringbicycle", "mtb", "e_mtb",
               "mtb_easy", "e_mtb_easy", "mtb_advanced", "e_mtb_advanced", "downhillbike", "citybike"}
#: pause between two Komoot downloads -- one import is hundreds of requests
KOMOOT_PAUSE_S = 0.3


@dataclass
class TrackInfo:
    id: str
    source: str
    #: session id (ride), SHA-256 (upload), tour id (komoot)
    ref: str
    name: str
    start: float | None = None
    end: float | None = None
    distance_m: float = 0.0
    points: int = 0
    #: what the points were made from -- a ride whose signature changed is read again
    sig: str = ""
    sport: str | None = None
    added_at: str = ""
    #: the original below <data>/map/ (upload, komoot); rides have theirs in the session store
    file: str | None = None

    def as_dict(self) -> dict:
        return asdict(self)


@dataclass
class MapState:
    revision: str
    tracks: list[explorer.Track]
    duplicates: dict[str, str]
    stats: dict[int, explorer.TileStats]
    #: (south, west, north, east) of everything, None without tracks
    bounds: tuple[float, float, float, float] | None = None


def _now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")


def _pack(points: list[tuple[float, float]]) -> bytes:
    a = array("i")
    for lat, lon in points:
        a.append(round(lat * 1e6))
        a.append(round(lon * 1e6))
    return a.tobytes()


def _unpack(data: bytes) -> list[tuple[float, float]]:
    a = array("i")
    a.frombytes(data)
    return [(a[i] / 1e6, a[i + 1] / 1e6) for i in range(0, len(a) - 1, 2)]


class KomootError(RuntimeError):
    pass


def _komoot_get(url: str, email: str, password: str, timeout: float = 60) -> bytes:
    """GET with Komoot's basic auth (e-mail + password, as kompy does it)."""
    token = base64.b64encode(f"{email}:{password}".encode()).decode()
    req = urllib.request.Request(url, headers={"Authorization": "Basic " + token,
                                               "Accept": "application/hal+json, application/gpx+xml, */*",
                                               "User-Agent": "BikeLog"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            return resp.read()
    except urllib.error.HTTPError as exc:
        if exc.code in (401, 403):
            raise KomootError("Komoot lehnt die Anmeldung ab (BIKELOG_KOMOOT_EMAIL/-PASSWORD prüfen)") from exc
        raise KomootError(f"Komoot: HTTP {exc.code} für {url.split('?')[0]}") from exc
    except OSError as exc:
        raise KomootError(f"Komoot nicht erreichbar: {exc}") from exc


class MapStore:
    def __init__(self, settings, fetch=_komoot_get):
        self.settings = settings
        self.root: Path = settings.data_dir / "map"
        self._fetch = fetch
        self._lock = threading.RLock()
        self._index: dict[str, TrackInfo] = {}
        self._drawn: dict[str, tuple[str, explorer.Track | None, set, set]] = {}
        self._state: MapState | None = None
        self._dirty = True
        self._cache: OrderedDict = OrderedDict()
        self._cache_lock = threading.Lock()
        self.komoot = {"running": False}
        self._load()

    # --- index -------------------------------------------------------------------------

    @property
    def _index_path(self) -> Path:
        return self.root / "index.json"

    def _load(self) -> None:
        try:
            data = json.loads(self._index_path.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return
        stale = data.get("version") != MAP_VERSION
        for raw in data.get("tracks", []):
            info = TrackInfo(**{k: v for k, v in raw.items() if k in TrackInfo.__dataclass_fields__})
            if stale:
                info.sig = ""           # read again from the source: sync/_reread
            self._index[info.id] = info
        if stale:
            self._reread_files()

    def _save(self) -> None:
        self.root.mkdir(parents=True, exist_ok=True)
        tmp = self._index_path.with_suffix(".part")
        tmp.write_text(json.dumps({"version": MAP_VERSION,
                                   "tracks": [t.as_dict() for t in self._index.values()]},
                                  ensure_ascii=False, indent=0), encoding="utf-8")
        tmp.replace(self._index_path)

    def _points_path(self, track_id: str) -> Path:
        return self.root / "points" / f"{track_id}.bin"

    def _put(self, info: TrackInfo, gpx: explorer.GpxTrack) -> TrackInfo:
        """Store a track's points and index entry (under the lock)."""
        points = explorer.thin(gpx.points)
        path = self._points_path(info.id)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(_pack(points))
        info.points = len(points)
        info.distance_m = round(explorer.distance_m(points), 1)
        if info.start is None:
            info.start, info.end = gpx.start, gpx.end
        info.added_at = info.added_at or _now()
        self._index[info.id] = info
        self._dirty = True
        return info

    def _remove(self, track_id: str) -> None:
        info = self._index.pop(track_id, None)
        self._points_path(track_id).unlink(missing_ok=True)
        if info and info.file:
            (self.root / info.file).unlink(missing_ok=True)
        self._dirty = True

    def _reread_files(self) -> None:
        """New MAP_VERSION: read uploads and Komoot tours again from their originals."""
        for info in list(self._index.values()):
            if not info.file:
                continue
            try:
                gpx = explorer.read_gpx((self.root / info.file).read_bytes())
            except (OSError, ValueError) as exc:
                log.warning("map: %s unreadable (%s), dropped", info.file, exc)
                self._remove(info.id)
                continue
            info.sig = "file"
            self._put(info, gpx)
        self._save()

    def tracks(self) -> list[TrackInfo]:
        with self._lock:
            return sorted(self._index.values(), key=lambda t: (t.start or 0, t.id))

    # --- rides -------------------------------------------------------------------------

    @staticmethod
    def _ride_eligible(s: Session) -> bool:
        if s.is_test or s.idle:
            return False
        if s.device == "import" or s.gpx_status == "ok":
            return True
        # GPX export switched off: the wheel distance decides like the exporter would
        return s.gpx_status is None and (s.distance_m or 0) >= 1000 and bool(s.gps_points)

    def _ride_gpx(self, store: Storage, s: Session) -> explorer.GpxTrack | None:
        if s.gpx_file:
            path = self.settings.gpx_dir / s.gpx_file
            if path.exists():
                return explorer.read_gpx(path.read_bytes())
        if s.device == "import":
            original = next((f for f in s.files if f.name.startswith("G_")), None)
            if original:
                return explorer.read_gpx(store.path_for(s, original.name).read_bytes())
        points = [(r.latitude, r.longitude) for r in store.records(s)
                  if r.gps_valid and (r.gps_lat_e7 or r.gps_lon_e7)]
        return explorer.GpxTrack(points, None, None, None) if points else None

    def sync_rides(self, store: Storage) -> int:
        """Bring the rides on the map in line with the session store; the number of changes."""
        wanted = {f"r{s.id}": s for s in store.sessions_with_log() if self._ride_eligible(s)}
        changes = 0
        with self._lock:
            for tid in [t for t, i in self._index.items() if i.source == "ride" and t not in wanted]:
                self._remove(tid)
                changes += 1
            for tid, s in wanted.items():
                sig = f"{s.updated_at}|{s.gpx_file}|{s.gpx_version}"
                have = self._index.get(tid)
                if have and have.sig == sig:
                    continue
                try:
                    gpx = self._ride_gpx(store, s)
                except (OSError, ValueError) as exc:
                    log.warning("map: session %d unreadable: %s", s.id, exc)
                    gpx = None
                if gpx is None or not gpx.points:
                    if have:
                        self._remove(tid)
                        changes += 1
                    continue
                start = datetime.datetime.fromtimestamp(s.start_time) if s.start_time else None
                name = f"Fahrt {start:%d.%m.%Y %H:%M}" if start else f"Sitzung {s.id}"
                self._put(TrackInfo(tid, "ride", str(s.id), name, start=s.first_time, end=s.last_time,
                                    sig=sig, added_at=have.added_at if have else ""), gpx)
                changes += 1
            if changes:
                self._save()
        if changes:
            log.info("map: %d ride(s) updated", changes)
        return changes

    # --- uploads -----------------------------------------------------------------------

    def _add_file(self, data: bytes, source: str, ref: str, name: str, sport: str | None = None,
                  start: float | None = None) -> tuple[str, TrackInfo]:
        """("new" | "known", info). Raises ValueError for a file without track."""
        tid = ("u" if source == "upload" else "k") + ref[:16]
        with self._lock:
            if tid in self._index:
                return "known", self._index[tid]
        gpx = explorer.read_gpx(data)
        rel = f"{source}/{ref[:16] if source == 'upload' else ref}.gpx"
        with self._lock:
            path = self.root / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
            info = TrackInfo(tid, source, ref, gpx.name or name, sig="file", sport=sport, file=rel)
            if start is not None and gpx.start is None:
                info.start = info.end = start
            self._put(info, gpx)
            self._save()
        return "new", info

    @staticmethod
    def _files(data: bytes, filename: str) -> list[tuple[str, bytes]]:
        """The GPX files of an upload: the file itself, or every .gpx/.gpx.gz in a ZIP (an
        export of Strava, Garmin, ...), unpacked. Raises ValueError for a ZIP without any."""
        files: list[tuple[str, bytes]] = []
        if data[:4] == b"PK\x03\x04":
            try:
                with zipfile.ZipFile(io.BytesIO(data)) as z:
                    for member in z.infolist():
                        low = member.filename.lower()
                        if not member.is_dir() and (low.endswith(".gpx") or low.endswith(".gpx.gz")):
                            files.append((member.filename, z.read(member)))
            except zipfile.BadZipFile as exc:
                raise ValueError(f"ZIP unlesbar: {exc}") from exc
            if not files:
                raise ValueError("keine GPX im ZIP")
        else:
            files.append((filename, data))
        return [(name, gzip.decompress(payload) if payload[:2] == b"\x1f\x8b" else payload)
                for name, payload in files]

    def add_upload(self, data: bytes, filename: str = "") -> list[dict]:
        """A GPX, a .gpx.gz or a .zip of them: one result per file found,
        {"file", "status": new|known|error, "track"|"error"}."""
        try:
            files = self._files(data, filename)
        except (ValueError, OSError, EOFError) as exc:
            return [{"file": filename, "status": "error", "error": str(exc)}]
        out = []
        for name, payload in files:
            try:
                sha = hashlib.sha256(payload).hexdigest()
                stem = Path(name).name
                for suffix in (".gz", ".gpx"):
                    if stem.lower().endswith(suffix):
                        stem = stem[:-len(suffix)]
                status, info = self._add_file(payload, "upload", sha, stem or "GPX")
                out.append({"file": name, "status": status, "track": info.as_dict()})
            except (ValueError, OSError) as exc:
                out.append({"file": name, "status": "error", "error": str(exc)})
        return out

    def add_mixed(self, uploads: list[tuple[bytes, str]], track_prefix: str | None = None,
                  name: str = "Handy") -> dict:
        """An all-day recording (a phone: walking, car, train and bike mixed), in as many files
        as it comes (day files, overlapping exports, ZIPs): merged into one, cut into trips,
        and only the trips bikelog.tripmode judges as cycling go on the map. With
        track_prefix only the <trk> of that device. {"trips": [...], "counts": {...},
        "errors": [...]}; every trip with its verdict, the cycling ones with "status"."""
        recordings, errors = [], []
        for data, filename in uploads:
            try:
                files = self._files(data, filename)
            except (ValueError, OSError, EOFError) as exc:
                errors.append(f"{filename}: {exc}")
                continue
            for fname, payload in files:
                try:
                    recordings.append(tripmode.read_fixes(payload, track_prefix))
                except (ET.ParseError, ValueError) as exc:
                    errors.append(f"{fname}: {exc}")
        trips = tripmode.trips(tripmode.merge(*recordings))
        out, counts = [], {k: 0 for k in tripmode.KINDS}
        for trip in trips:
            counts[trip.kind] += 1
            entry = trip.as_dict()
            if trip.kind == "bike":
                when = datetime.datetime.fromtimestamp(trip.start)
                title = f"{name} {when:%d.%m.%Y %H:%M}"
                gpx = tripmode.to_gpx(trip, title)
                ref = hashlib.sha256(f"{track_prefix}|{trip.start}|{trip.end}|{len(trip.fixes)}"
                                     .encode()).hexdigest()
                status, info = self._add_file(gpx, "upload", ref, title)
                entry.update(status=status, track=info.id)
            if trip.kind != "short":
                out.append(entry)
        return {"trips": out, "counts": counts, "errors": errors}

    def delete(self, track_id: str) -> bool:
        """Remove an uploaded or imported track. Rides follow their session: False."""
        with self._lock:
            info = self._index.get(track_id)
            if info is None or info.source == "ride":
                return False
            self._remove(track_id)
            self._save()
        return True

    # --- Komoot ------------------------------------------------------------------------

    def komoot_import(self, all_sports: bool = False) -> dict:
        """Fetch every recorded Komoot tour not yet here (runs in the calling thread; the
        page starts it in one of its own). Progress in self.komoot."""
        s = self.settings
        with self._lock:
            if self.komoot.get("running"):
                return self.komoot
            self.komoot = {"running": True, "started_at": _now(), "total": 0, "done": 0, "new": 0,
                           "known": 0, "skipped": 0, "errors": [], "all_sports": all_sports}
        state = self.komoot
        try:
            if not s.komoot_enabled:
                raise KomootError("Komoot nicht eingerichtet (BIKELOG_KOMOOT_EMAIL/-PASSWORD)")
            fetch = lambda url: self._fetch(url, s.komoot_email, s.komoot_password)   # noqa: E731
            account = json.loads(fetch(f"{KOMOOT_API}/v006/account/email/"
                                       f"{urllib.parse.quote(s.komoot_email)}/"))
            user = account["username"]
            tours, page, pages = [], 0, 1
            while page < pages:
                data = json.loads(fetch(f"{KOMOOT_API}/v007/users/{urllib.parse.quote(str(user))}/tours/?"
                                        + urllib.parse.urlencode({"type": "tour_recorded", "page": page,
                                                                  "limit": 100})))
                tours += (data.get("_embedded") or {}).get("tours", [])
                pages = int((data.get("page") or {}).get("totalPages", 0))
                page += 1
            state["total"] = len(tours)
            for tour in tours:
                tid, sport = str(tour.get("id")), tour.get("sport")
                state["done"] += 1
                if not all_sports and sport not in BIKE_SPORTS:
                    state["skipped"] += 1
                    continue
                with self._lock:
                    if "k" + tid[:16] in self._index:
                        state["known"] += 1
                        continue
                try:
                    gpx = fetch(f"{KOMOOT_API}/v007/tours/{urllib.parse.quote(tid)}.gpx")
                    start = explorer._epoch(tour.get("date"))
                    status, _ = self._add_file(gpx, "komoot", tid, tour.get("name") or f"Komoot {tid}",
                                               sport=sport, start=start)
                    state["new" if status == "new" else "known"] += 1
                except (KomootError, ValueError) as exc:
                    state["errors"].append(f"{tour.get('name') or tid}: {exc}")
                if KOMOOT_PAUSE_S:
                    time.sleep(KOMOOT_PAUSE_S)
        except (KomootError, KeyError, ValueError) as exc:
            state["errors"].append(str(exc))
            log.error("map: Komoot import failed: %s", exc)
        finally:
            state["running"] = False
            state["finished_at"] = _now()
            log.info("map: Komoot import: %d new, %d known, %d other sports, %d errors",
                     state.get("new", 0), state.get("known", 0), state.get("skipped", 0), len(state["errors"]))
        return state

    # --- drawing -----------------------------------------------------------------------

    def _drawable(self, info: TrackInfo):
        """(Track, z14 tiles, z17 tiles) of a track, built once per signature."""
        key = f"{info.sig}|{info.points}|{info.added_at}"
        have = self._drawn.get(info.id)
        if have and have[0] == key:
            return have[1:]
        try:
            points = _unpack(self._points_path(info.id).read_bytes())
        except OSError:
            points = []
        track = explorer.make_track(info.id, points) if points else None
        t14 = explorer.tiles_of(track.runs, explorer.EXPLORER_ZOOM) if track else set()
        t17 = explorer.tiles_of(track.runs, explorer.SQUADRAT_ZOOM) if track else set()
        self._drawn[info.id] = (key, track, t14, t17)
        return track, t14, t17

    def state(self) -> MapState:
        with self._lock:
            if self._state is not None and not self._dirty:
                return self._state
            infos = list(self._index.values())
            dups = explorer.duplicates([(i.id, PRIORITY[i.source], i.start, i.end) for i in infos])
            tracks, t14, t17 = [], set(), set()
            for info in infos:
                track, a, b = self._drawable(info)
                if track is None or info.id in dups:
                    continue
                tracks.append(track)
                t14 |= a
                t17 |= b
            for gone in set(self._drawn) - {i.id for i in infos}:
                del self._drawn[gone]
            bounds = None
            if tracks:
                minx = min(t.bbox[0] for t in tracks)
                miny = min(t.bbox[1] for t in tracks)
                maxx = max(t.bbox[2] for t in tracks)
                maxy = max(t.bbox[3] for t in tracks)
                north, west = explorer.unmercator(minx, miny)
                south, east = explorer.unmercator(maxx, maxy)
                bounds = (south, west, north, east)
            digest = hashlib.sha1(json.dumps(sorted((i.id, i.sig, i.points) for i in infos)).encode()
                                  + json.dumps(sorted(dups.items())).encode()).hexdigest()[:12]
            self._state = MapState(digest, tracks, dups,
                                   {explorer.EXPLORER_ZOOM: explorer.tile_stats(t14, explorer.EXPLORER_ZOOM),
                                    explorer.SQUADRAT_ZOOM: explorer.tile_stats(t17, explorer.SQUADRAT_ZOOM)},
                                   bounds)
            self._dirty = False
            return self._state

    def tile(self, layer: str, z: int, x: int, y: int) -> tuple[bytes, str]:
        """(PNG, revision) of one overlay tile; a transparent tile where there is nothing."""
        state = self.state()
        key = (state.revision, layer, z, x, y)
        with self._cache_lock:
            hit = self._cache.get(key)
            if hit is not None:
                self._cache.move_to_end(key)
                return hit, state.revision
        n = 2 ** z
        png = None
        if z >= MIN_ZOOM[layer] and 0 <= x < n and 0 <= y < n:
            if layer == "heat":
                png = explorer.heat_tile(state.tracks, z, x, y)
            else:
                png = explorer.visited_tile(state.stats[LAYERS[layer]], z, x, y)
        png = png or explorer.EMPTY_PNG
        with self._cache_lock:
            self._cache[key] = png
            while len(self._cache) > TILE_CACHE:
                self._cache.popitem(last=False)
        return png, state.revision

    def summary(self) -> dict:
        state = self.state()
        infos = self.tracks()
        by_source = {k: {"tracks": 0, "duplicates": 0, "distance_km": 0.0} for k in SOURCES}
        for i in infos:
            entry = by_source[i.source]
            if i.id in state.duplicates:
                entry["duplicates"] += 1
            else:
                entry["tracks"] += 1
                entry["distance_km"] += i.distance_m / 1000
        for entry in by_source.values():
            entry["distance_km"] = round(entry["distance_km"], 1)
        return {"revision": state.revision, "tracks": len(state.tracks), "sources": by_source,
                "explorer": state.stats[explorer.EXPLORER_ZOOM].as_dict(),
                "squadrats": state.stats[explorer.SQUADRAT_ZOOM].as_dict(),
                "bounds": [round(v, 6) for v in state.bounds] if state.bounds else None,
                "komoot": dict(self.komoot), "komoot_enabled": self.settings.komoot_enabled}

    def new_tiles(self, points: list[tuple[float, float]]) -> dict:
        """What a planned route would add: {"explorer": n, "squadrats": n} new tiles."""
        state = self.state()
        runs = explorer.runs_of(points)
        return {"explorer": len(explorer.tiles_of(runs, explorer.EXPLORER_ZOOM)
                                - state.stats[explorer.EXPLORER_ZOOM].tiles),
                "squadrats": len(explorer.tiles_of(runs, explorer.SQUADRAT_ZOOM)
                                 - state.stats[explorer.SQUADRAT_ZOOM].tiles)}
