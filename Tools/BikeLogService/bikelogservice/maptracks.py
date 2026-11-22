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
           fetches tours not yet here. Tours of others (Komoot lists those one is tagged in)
           are skipped, and a deleted tour stays deleted (komoot-deleted.json).

An upload marked "mixed" is a phone's all-day recording (walking, car, train): add_mixed()
keeps only the trips bikelog.tripmode judges as cycling, each stored as an upload of its own.

index.json lists the tracks, points/<id>.bin holds each one's thinned points (int32 lat/lon
in 1e-6 degrees, explorer.THIN_M apart). A ride recorded at the same time as one from a
better source is a duplicate (a BikeLog ride uploaded to Komoot comes back with the import)
and counts once: explorer.duplicates(), ride before upload before komoot. Every recording
is still drawn: a fragment of a tour must not take the rest of the tour off the map.

Tiles: drawn on demand from explorer.HeatIndex. The heat tiles up to STORE_ZOOM (few, and each
full of track) are also kept in heat-cache.sqlite and filled in by a background thread
(start()): after a restart they are there, after a new ride only the tiles it runs through
are drawn again (HeatIndex.key()). The file is derived data and may be deleted any time.
All drawing happens in one thread: it is pure CPU, so request threads would only take the
interpreter from each other, and on big.LITTLE a thread that does a tile now and then is
kept on a small core at a low clock (measured: a view 5 s instead of under 1 s).

The tile layers have two colourings (tile(colour=), COLOURS): cluster and max square, or the
distance ridden in each tile (MapState.metres_at(): recordings of one ride count once).

A period (tile(first=, last=), days in local time, both included) is a selection of the rides:
MapState.period(). A ride belongs to the day it began with all its recordings, a track without a
time only to the whole. A selection draws from the whole's index and takes the stored heat tiles
it shares with it; its own are only kept in memory.

MAP_VERSION: raise it when how a track is read or thinned changes -- the store then reads
every track again from its source on the next start.
"""

from __future__ import annotations

import base64
import datetime
import gzip
import hashlib
import http.client
import io
import json
import logging
import sqlite3
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from array import array
from collections import OrderedDict
from concurrent.futures import ThreadPoolExecutor
from dataclasses import asdict, dataclass, field
from pathlib import Path

import xml.etree.ElementTree as ET

from bikelog import explorer, tripmode

from .storage import Session, Storage

log = logging.getLogger("bikelog.map")

MAP_VERSION = 1
SOURCES = {"ride": "BikeLog", "upload": "Hochgeladen", "komoot": "Komoot"}
PRIORITY = {"ride": 0, "upload": 1, "komoot": 2}
#: layer -> the zoom of the tiles it counts; "zoom": follows the map, see ZOOM_TILES_FINER
LAYERS = {"visited": explorer.EXPLORER_ZOOM, "squadrats": explorer.SQUADRAT_ZOOM, "zoom": None, "heat": None}
#: below this zoom the squadrats layer stays empty (a 200 m tile would be under 2 px)
MIN_ZOOM = {"visited": 5, "squadrats": 10, "zoom": 3, "heat": 3}
MAX_ZOOM = 20
#: the "zoom" layer's grid is this many zooms finer than the map: 2 = a quarter of a map
#: tile's side (64 px), a 16th of its area
ZOOM_TILES_FINER = 2
#: the finest grid of the "zoom" layer: closer in (100 m and less a tile) GPS scatter would
#: decide what counts as visited, so from there on it shows the squadrats
ZOOM_TILES_MAX = explorer.SQUADRAT_ZOOM
#: rendered tiles kept in memory (a few KB each)
TILE_CACHE = 4000
#: heat tiles up to this zoom are drawn ahead and kept on disk
STORE_ZOOM = 14
#: how the tile layers are coloured; distance-dark = distance for a dark base map
COLOURS = ("cluster", "distance", "distance-dark")
#: periods kept ready: moving a slider asks for many tiles of few periods
PERIOD_CACHE = 8
#: drawing ahead waits until no tile has been asked for this long
QUIET_S = 1.0

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
    heat: explorer.HeatIndex
    #: per track: the day its ride began (None = no time) and its zoom 17 tiles with the metres in each
    days: list[datetime.date | None]
    squadrats: list[dict]
    #: (south, west, north, east) of everything, None without tracks
    bounds: tuple[float, float, float, float] | None = None
    #: a period's tracks, as numbers in heat.tracks; None = all of them
    chosen: frozenset[int] | None = None
    periods: OrderedDict = field(default_factory=OrderedDict)
    metres: dict[int, dict] = field(default_factory=dict)

    def period(self, first: datetime.date | None, last: datetime.date | None) -> MapState:
        """Only the rides that began first..last (each may be open); self if that is all."""
        if first is None and last is None:
            return self
        chosen = frozenset(no for no, day in enumerate(self.days) if day is not None
                           and (first is None or day >= first) and (last is None or day <= last))
        if len(chosen) == len(self.tracks):
            return self
        part = self.periods.pop(chosen, None)
        if part is None:
            numbers = sorted(chosen)
            tracks = [self.tracks[no] for no in numbers]
            revision = hashlib.sha1(f"{self.revision}|{numbers}".encode()).hexdigest()[:12]
            part = MapState(revision, tracks, self.duplicates, {}, self.heat,
                            [self.days[no] for no in numbers], [self.squadrats[no] for no in numbers],
                            _bounds(tracks), chosen)
        self.periods[chosen] = part
        while len(self.periods) > PERIOD_CACHE:
            self.periods.popitem(last=False)
        return part

    def pieces(self, z: int, x: int, y: int) -> list:
        pieces = self.heat.pieces(z, x, y)
        return pieces if self.chosen is None else [p for p in pieces if p[0] in self.chosen]

    def metres_at(self, zoom: int) -> dict[tuple[int, int], float]:
        """The distance ridden in each tile at zoom, worked out on first use."""
        metres = self.metres.get(zoom)
        if metres is None:
            rides: dict[str, list[dict]] = {}
            for track, tiles in zip(self.tracks, self.squadrats):
                rides.setdefault(self.duplicates.get(track.id, track.id), []).append(tiles)
            metres = explorer.tile_metres(rides.values(), explorer.SQUADRAT_ZOOM - zoom)
            self.metres[zoom] = metres
        return metres

    def stats_at(self, zoom: int) -> explorer.TileStats:
        """Visited tiles and figures at any zoom up to SQUADRAT_ZOOM, worked out on first use."""
        stats = self.stats.get(zoom)
        if stats is None:
            if zoom == explorer.SQUADRAT_ZOOM:
                tiles = set().union(*self.squadrats)
            else:
                fine = self.stats_at(explorer.SQUADRAT_ZOOM)
                tiles = explorer.parent_tiles(fine.tiles, fine.zoom - zoom)
            stats = explorer.tile_stats(tiles, zoom)
            self.stats[zoom] = stats
        return stats


def _bounds(tracks: list[explorer.Track]) -> tuple[float, float, float, float] | None:
    if not tracks:
        return None
    north, west = explorer.unmercator(min(t.bbox[0] for t in tracks), min(t.bbox[1] for t in tracks))
    south, east = explorer.unmercator(max(t.bbox[2] for t in tracks), max(t.bbox[3] for t in tracks))
    return south, west, north, east


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


class HeatCache:
    """Drawn heat tiles on disk, each with the key of what it shows. Only a cache: when SQLite
    fails the tiles are simply drawn every time."""

    def __init__(self, path: Path):
        self.path = path
        self._lock = threading.Lock()
        self._db: sqlite3.Connection | None = None
        self._failed = False

    def _open(self) -> sqlite3.Connection:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        db = sqlite3.connect(self.path, check_same_thread=False, isolation_level=None)
        try:
            db.execute("PRAGMA journal_mode=WAL")
            db.execute("PRAGMA synchronous=NORMAL")
            db.execute("CREATE TABLE IF NOT EXISTS tiles (z INT, x INT, y INT, key TEXT, png BLOB,"
                       " PRIMARY KEY (z, x, y))")
        except sqlite3.Error:
            db.close()
            raise
        return db

    def _run(self, sql: str, args=(), many: bool = False) -> list | None:
        """The rows, None if the cache is not to be had."""
        with self._lock:
            if self._failed:
                return None
            try:
                if self._db is None:
                    try:
                        self._db = self._open()
                    except sqlite3.DatabaseError as exc:    # unreadable: it is only a cache
                        log.warning("map: heat cache rebuilt (%s)", exc)
                        self.path.unlink(missing_ok=True)
                        self._db = self._open()
                if many:
                    self._db.executemany(sql, args)
                    return []
                return self._db.execute(sql, args).fetchall()
            except (OSError, sqlite3.Error) as exc:
                log.warning("map: heat cache off until the next start (%s)", exc)
                self._failed = True
                return None

    def get(self, z: int, x: int, y: int, key: str) -> bytes | None:
        rows = self._run("SELECT png FROM tiles WHERE z=? AND x=? AND y=? AND key=?", (z, x, y, key))
        return rows[0][0] if rows else None

    def put(self, z: int, x: int, y: int, key: str, png: bytes) -> None:
        self._run("INSERT OR REPLACE INTO tiles VALUES (?,?,?,?,?)", (z, x, y, key, png))

    def keys(self, z: int) -> dict[tuple[int, int], str] | None:
        rows = self._run("SELECT x, y, key FROM tiles WHERE z=?", (z,))
        return None if rows is None else {(x, y): key for x, y, key in rows}

    def drop(self, z: int, tiles) -> None:
        self._run("DELETE FROM tiles WHERE z=? AND x=? AND y=?", [(z, x, y) for x, y in tiles], many=True)

    def close(self) -> None:
        with self._lock:
            if self._db is not None:
                self._db.close()
                self._db = None


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
        # not the URL: the login's holds the e-mail address, and this text is logged and shown
        raise KomootError(f"Komoot: HTTP {exc.code}") from exc
    except (OSError, http.client.HTTPException) as exc:
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
        self._heat_cache = HeatCache(self.root / "heat-cache.sqlite")
        self._drawing = ThreadPoolExecutor(max_workers=1, thread_name_prefix="bikelog-tiles")
        self._asked = float("-inf")     # when the last tile was asked for
        self._changed = threading.Event()
        self._stop = threading.Event()
        self._filler: threading.Thread | None = None
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

    @property
    def _deleted_path(self) -> Path:
        return self.root / "komoot-deleted.json"

    def _deleted_tours(self) -> set[str]:
        """Komoot tour ids deleted here: an import must not bring them back."""
        try:
            return set(json.loads(self._deleted_path.read_text(encoding="utf-8")))
        except FileNotFoundError:
            return set()

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
            if info.source == "komoot":
                gone = sorted(self._deleted_tours() | {info.ref})
                tmp = self._deleted_path.with_suffix(".part")
                tmp.write_text(json.dumps(gone), encoding="utf-8")
                tmp.replace(self._deleted_path)
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
                           "known": 0, "skipped": 0, "skipped_sports": {}, "foreign": 0, "deleted": 0, "errors": [], "all_sports": all_sports}
        state = self.komoot
        try:
            if not s.komoot_enabled:
                raise KomootError("Komoot nicht eingerichtet (BIKELOG_KOMOOT_EMAIL/-PASSWORD)")
            fetch = lambda url: self._fetch(url, s.komoot_email, s.komoot_password)   # noqa: E731
            account = json.loads(fetch(f"{KOMOOT_API}/v006/account/email/"
                                       f"{urllib.parse.quote(s.komoot_email)}/"))
            user = account["username"]
            found, page, pages, expected = {}, 0, None, None
            while pages is None or page < pages:
                data = json.loads(fetch(f"{KOMOOT_API}/v007/users/{urllib.parse.quote(str(user))}/tours/?"
                                        + urllib.parse.urlencode({"type": "tour_recorded", "page": page,
                                                                  "limit": 100})))
                before = len(found)
                found.update((str(t.get("id")), t) for t in (data.get("_embedded") or {}).get("tours", []))
                info = data.get("page") or {}
                if "totalPages" in info:
                    pages = int(info["totalPages"])
                expected = info.get("totalElements", expected)
                page += 1
                if len(found) == before:        # nothing new: the end, whatever the page count says
                    break
            if expected is not None and len(found) != expected:
                state["errors"].append(f"Komoot meldet {expected} Touren, gelistet wurden {len(found)}")
            state["total"] = len(found)
            deleted = self._deleted_tours()
            for tour in found.values():
                tid, sport = str(tour.get("id")), tour.get("sport")
                state["done"] += 1
                creator = ((tour.get("_embedded") or {}).get("creator") or {}).get("username")
                if creator is not None and str(creator) != str(user):
                    state["foreign"] += 1
                    continue
                if tid in deleted:
                    state["deleted"] += 1
                    continue
                if not all_sports and sport not in BIKE_SPORTS:
                    state["skipped"] += 1
                    state["skipped_sports"][str(sport)] = state["skipped_sports"].get(str(sport), 0) + 1
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
                except (KomootError, ValueError, OSError, http.client.HTTPException) as exc:
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
        """(Track, z14 tiles, z17 tiles with their metres) of a track, built once per signature."""
        key = f"{info.sig}|{info.points}|{info.added_at}"
        have = self._drawn.get(info.id)
        if have and have[0] == key:
            return have[1:]
        try:
            points = _unpack(self._points_path(info.id).read_bytes())
        except OSError:
            points = []
        track = explorer.make_track(info.id, points) if points else None
        t17 = explorer.tile_lengths(track.runs, explorer.SQUADRAT_ZOOM) if track else {}
        t14 = explorer.parent_tiles(t17, explorer.SQUADRAT_ZOOM - explorer.EXPLORER_ZOOM)
        self._drawn[info.id] = (key, track, t14, t17)
        return track, t14, t17

    def state(self, first: datetime.date | None = None, last: datetime.date | None = None) -> MapState:
        """Everything, or only the rides that began in the days first..last."""
        with self._lock:
            return self._whole().period(first, last)

    def _whole(self) -> MapState:
        with self._lock:
            if self._state is not None and not self._dirty:
                return self._state
            infos = list(self._index.values())
            dups = explorer.duplicates([(i.id, PRIORITY[i.source], i.start, i.end) for i in infos])
            began: dict[str, float] = {}    # ride -> when its first recording began
            for info in infos:
                if info.start is not None:
                    ride = dups.get(info.id, info.id)
                    began[ride] = min(began.get(ride, info.start), info.start)
            tracks, days, squadrats, t14, t17 = [], [], [], set(), set()
            for info in infos:
                track, a, b = self._drawable(info)
                if track is None:
                    continue
                tracks.append(track)
                start = began.get(dups.get(info.id, info.id))
                days.append(None if start is None else datetime.date.fromtimestamp(start))
                squadrats.append(b)
                t14 |= a
                t17.update(b)
            for gone in set(self._drawn) - {i.id for i in infos}:
                del self._drawn[gone]
            # a period's revision builds on this one: a ride moved to another day must change it
            digest = hashlib.sha1(json.dumps(sorted((i.id, i.sig, i.points, i.start or 0) for i in infos)).encode()
                                  + json.dumps(sorted(dups.items())).encode() + b"all-drawn"
                                  # the revision is the tiles' ETag: other grid = other revision
                                  + b"|zoom+%d|better-on-top" % ZOOM_TILES_FINER
                                  + repr((explorer.VISITED_PALETTE, explorer.VISITED_PALETTE_CLOSE,
                                          explorer.DISTANCE_PALETTE, explorer.DISTANCE_PALETTE_DARK,
                                          [explorer.km_steps(z) for z in range(ZOOM_TILES_MAX + 1)]
                                          )).encode()).hexdigest()[:12]
            self._state = MapState(digest, tracks, dups,
                                   {explorer.EXPLORER_ZOOM: explorer.tile_stats(t14, explorer.EXPLORER_ZOOM),
                                    explorer.SQUADRAT_ZOOM: explorer.tile_stats(t17, explorer.SQUADRAT_ZOOM)},
                                   explorer.HeatIndex(tracks, dups), days, squadrats,
                                   _bounds(tracks))
            self._dirty = False
            self._changed.set()         # the background thread has tiles to catch up on
            return self._state

    def tile(self, layer: str, z: int, x: int, y: int, first: datetime.date | None = None,
             last: datetime.date | None = None, colour: str = "cluster") -> tuple[bytes, str]:
        """(PNG, revision) of one overlay tile; a transparent tile where there is nothing."""
        self._asked = time.monotonic()
        state = self.state(first, last)
        n = 2 ** z
        if z < MIN_ZOOM[layer] or not (0 <= x < n and 0 <= y < n):
            return explorer.EMPTY_PNG, state.revision
        if layer == "heat":
            return self._heat(state, z, x, y), state.revision
        key = (layer, z, x, y, state.revision, colour)
        png = self._cached(key)
        if png is None:
            png = self._drawing.submit(self._draw_visited, state, key).result()
        return png, state.revision

    def _draw_visited(self, state: MapState, key: tuple) -> bytes:
        layer, z, x, y, _, colour = key
        zoom = min(z + ZOOM_TILES_FINER, ZOOM_TILES_MAX) if layer == "zoom" else LAYERS[layer]
        metres = state.metres_at(zoom) if colour != "cluster" else None
        png = explorer.visited_tile(state.stats_at(zoom), z, x, y, faint=layer == "zoom", metres=metres,
                                    dark=colour == "distance-dark") or explorer.EMPTY_PNG
        self._remember(key, png)
        return png

    def _cached(self, key: tuple) -> bytes | None:
        with self._cache_lock:
            png = self._cache.get(key)
            if png is not None:
                self._cache.move_to_end(key)
            return png

    def _remember(self, key: tuple, png: bytes) -> None:
        with self._cache_lock:
            self._cache[key] = png
            while len(self._cache) > TILE_CACHE:
                self._cache.popitem(last=False)

    def _heat(self, state: MapState, z: int, x: int, y: int) -> bytes:
        pieces = state.pieces(z, x, y)
        if not pieces:
            return explorer.EMPTY_PNG
        # by what the tile shows, not by the revision: a ride elsewhere leaves it valid
        key = ("heat", z, x, y, state.heat.key(pieces))
        png = self._cached(key)
        if png is not None:
            return png
        cacheable = z <= STORE_ZOOM
        png = self._heat_cache.get(z, x, y, key[-1]) if cacheable else None
        if png is None:
            # one stored tile per place: a period must not push out the whole's
            stored = cacheable and state.chosen is None
            png = self._drawing.submit(self._draw_heat, state, pieces, key, stored).result()
        else:
            self._remember(key, png)
        return png

    def _draw_heat(self, state: MapState, pieces: list, key: tuple, stored: bool) -> bytes:
        png = self._cached(key)         # drawn while this request waited?
        if png is None:
            _, z, x, y, shows = key
            png = state.heat.draw(pieces, z, x, y) or explorer.EMPTY_PNG
            if stored:
                self._heat_cache.put(z, x, y, shows, png)
            self._remember(key, png)
        return png

    # --- drawing ahead -----------------------------------------------------------------

    def start(self) -> None:
        """Keep the stored heat tiles up to date in the background (the service does this)."""
        if self._filler and self._filler.is_alive():
            return
        self._stop.clear()
        self._changed.set()
        self._filler = threading.Thread(target=self._fill_loop, name="bikelog-heatmap", daemon=True)
        self._filler.start()

    def stop(self) -> None:
        self._stop.set()
        self._changed.set()
        if self._filler:
            self._filler.join(timeout=10)
            if self._filler.is_alive():
                return                  # still in a tile: leave it the open file
        self._heat_cache.close()

    def _fill_loop(self) -> None:
        while True:
            self._changed.wait()
            if self._stop.is_set():
                return
            self._changed.clear()
            try:
                state = self.state()
                self._changed.clear()   # building this very state is no news
                if state is not self._state:
                    self._changed.set()
                self.fill(state)
            except Exception:           # the next change tries again
                log.exception("map: drawing ahead failed")

    def fill(self, state: MapState | None = None) -> int:
        """Draw the stored heat tiles that are missing or out of date; how many. Gives way to
        requests, and gives up when the tracks change (the loop then starts over)."""
        state = state or self.state()
        began = time.monotonic()
        drawn = 0
        for z in range(MIN_ZOOM["zoom"], ZOOM_TILES_MAX + 1):       # so the first view need not
            self._give_way()
            self._drawing.submit(state.stats_at, z).result()
            if z <= explorer.INDEX_ZOOM:
                self._drawing.submit(state.heat.tiles, z).result()
        for z in range(MIN_ZOOM["heat"], STORE_ZOOM + 1):
            have = self._heat_cache.keys(z)
            if have is None:
                break
            tiles = state.heat.tiles(z)
            for (x, y), pieces in tiles.items():
                self._give_way()        # also from just looking through them
                key = state.heat.key(pieces)
                if have.get((x, y)) == key:
                    continue
                if self._stop.is_set() or state is not self._state or self._dirty:
                    return drawn
                self._drawing.submit(self._draw_ahead, state, pieces, z, x, y, key).result()
                drawn += 1
            gone = have.keys() - tiles.keys()
            if gone:
                self._heat_cache.drop(z, gone)
        if drawn:
            log.info("map: %d heat tile(s) drawn ahead in %.0f s", drawn, time.monotonic() - began)
        return drawn

    def _give_way(self) -> None:
        while time.monotonic() - self._asked < QUIET_S and not self._stop.is_set():
            self._stop.wait(0.2)

    def _draw_ahead(self, state: MapState, pieces: list, z: int, x: int, y: int, key: str) -> None:
        png = state.heat.draw(pieces, z, x, y) or explorer.EMPTY_PNG
        self._heat_cache.put(z, x, y, key, png)

    def first_day(self) -> datetime.date | None:
        return min((day for day in self.state().days if day is not None), default=None)

    def summary(self, first: datetime.date | None = None, last: datetime.date | None = None) -> dict:
        """The figures, of everything or of a period."""
        self._asked = time.monotonic()
        whole = self.state()
        state = self.state(first, last)
        infos = self.tracks()
        if state.chosen is not None:
            ids = {t.id for t in state.tracks}
            infos = [i for i in infos if i.id in ids]
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
        stats = {z: self._drawing.submit(state.stats_at, z).result()
                 for z in (explorer.SQUADRAT_ZOOM, explorer.EXPLORER_ZOOM)}
        return {"revision": state.revision, "from": first.isoformat() if first else None,
                "to": last.isoformat() if last else None,
                "undated": sum(d is None for d, t in zip(whole.days, whole.tracks) if t.id not in whole.duplicates),
                "tracks": sum(t.id not in state.duplicates for t in state.tracks), "sources": by_source,
                "explorer": stats[explorer.EXPLORER_ZOOM].as_dict(),
                "squadrats": stats[explorer.SQUADRAT_ZOOM].as_dict(),
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
