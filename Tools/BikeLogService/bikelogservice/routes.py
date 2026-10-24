"""Planned and regular routes (GPX uploaded on /routes), and the one handed to TrailBridge.

<data>/routes.json is the list, <data>/routes/<id>.gpx the files, unchanged. Every
upload is checked with bikelog.routecheck (can TrailBridge navigate it, key
figures); the check is stored with the route so the page need not parse the file
again -- recheck() redoes it when the check itself changed (CHECK_VERSION).

At most one route is marked for TrailBridge. The app polls GET
/api/v1/trailbridge/route (at its start, then every 15 min while it neither
navigates nor plays a route back) and loads the marked route when its revision is
new to it. The revision changes with the file and with every marking, so marking the
same route again sends it once more. The app confirms the load (fetched()), which
the page shows. Contract: doc/LOGSERVICE.md "Routes for TrailBridge".
"""

from __future__ import annotations

import datetime
import hashlib
import json
import threading
import uuid
from dataclasses import asdict, dataclass, field
from pathlib import Path

from bikelog import routecheck

from .storage import Storage

#: Bump when routecheck's findings or stats change -- stored checks are redone on start.
CHECK_VERSION = 2
KINDS = {"planned": "Geplant", "regular": "Regelmäßig"}
TAGS = {"commute": "Pendel/Alltag", "training": "Training"}

_lock = threading.Lock()


@dataclass
class Route:
    name: str
    id: str = field(default_factory=lambda: uuid.uuid4().hex[:8])
    kind: str = "planned"
    notes: str = ""
    #: keys of TAGS
    tags: list[str] = field(default_factory=list)
    #: done with (a planned route after the ride): in the archive, never marked
    archived: bool = False
    #: file name below routes/, and its SHA-256
    gpx: str = ""
    sha256: str = ""
    size_bytes: int = 0
    uploaded_at: str = ""
    #: routecheck.check() of the file, with "version" = CHECK_VERSION
    check: dict = field(default_factory=dict)
    #: marked for TrailBridge since (ISO time, UTC); None = not marked
    marked_at: str | None = None
    #: how often it was marked -- part of the revision (two markings may share a second)
    mark_count: int = 0
    #: the revision TrailBridge confirmed last, when and as whom
    fetched_revision: str | None = None
    fetched_at: str | None = None
    fetched_by: str | None = None

    @property
    def revision(self) -> str:
        """Changes with the file and with each marking (the app reloads on a new one)."""
        return hashlib.sha256(f"{self.id}|{self.sha256}|{self.mark_count}".encode()).hexdigest()[:16]

    @property
    def verdict(self) -> str:
        return self.check.get("verdict", "error")

    @property
    def stats(self) -> dict:
        return self.check.get("stats", {})

    @property
    def on_phone(self) -> bool:
        """TrailBridge confirmed the marked version."""
        return bool(self.marked_at) and self.fetched_revision == self.revision

    def as_dict(self) -> dict:
        data = asdict(self)
        data["revision"] = self.revision
        data["on_phone"] = self.on_phone
        return data

    @classmethod
    def from_dict(cls, data: dict) -> "Route":
        known = cls.__dataclass_fields__
        return cls(**{k: v for k, v in data.items() if k in known})


def now_iso() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")


def _path(store: Storage) -> Path:
    return store.settings.data_dir / "routes.json"


def routes_dir(store: Storage) -> Path:
    path = store.settings.data_dir / "routes"
    path.mkdir(parents=True, exist_ok=True)
    return path


def load(store: Storage) -> list[Route]:
    path = _path(store)
    if not path.exists():
        return []
    routes = [Route.from_dict(r) for r in json.loads(path.read_text(encoding="utf-8"))]
    # marked first, then by kind and name; the archive last
    return sorted(routes, key=lambda r: (r.archived, r.marked_at is None, r.kind, r.name.lower()))


def _save(store: Storage, routes: list[Route]) -> None:
    path = _path(store)
    tmp = path.with_name(path.name + ".part")
    tmp.write_text(json.dumps([r.as_dict() for r in routes], ensure_ascii=False, indent=2),
                   encoding="utf-8")
    tmp.replace(path)


def get(store: Storage, route_id: str) -> Route | None:
    return next((r for r in load(store) if r.id == route_id), None)


def gpx_path(store: Storage, route: Route) -> Path:
    return routes_dir(store) / route.gpx


def _checked(data: bytes, fallback_name: str) -> dict:
    result = routecheck.check(data, fallback_name)
    result["version"] = CHECK_VERSION
    return result


class Unusable(ValueError):
    """Not a GPX TrailBridge could read at all -- nothing worth keeping."""

    def __init__(self, check: dict):
        super().__init__("; ".join(f["text"] for f in check["findings"] if f["level"] == "error"))
        self.check = check


def _set_file(store: Storage, route: Route, data: bytes, fallback_name: str) -> None:
    check = _checked(data, fallback_name)
    if check["verdict"] == "error":
        raise Unusable(check)
    route.gpx = f"{route.id}.gpx"
    tmp = routes_dir(store) / (route.gpx + ".part")
    tmp.write_bytes(data)
    tmp.replace(gpx_path(store, route))
    route.sha256 = hashlib.sha256(data).hexdigest()
    route.size_bytes = len(data)
    route.uploaded_at = now_iso()
    route.check = check


def _tags(tags) -> list[str]:
    return [t for t in TAGS if t in (tags or ())]


def add(store: Storage, data: bytes, filename: str = "", kind: str = "planned",
        name: str = "", tags: list[str] | None = None) -> Route:
    """A new route from a GPX. Unusable if TrailBridge cannot read it; a usable one with
    warnings is kept (the page shows them)."""
    fallback = Path(filename).stem if filename else "Route"
    route = Route(name="", kind=kind if kind in KINDS else "planned", tags=_tags(tags))
    with _lock:
        _set_file(store, route, data, fallback)
        route.name = name.strip() or route.check["name"]
        _save(store, load(store) + [route])
    return route


def replace_gpx(store: Storage, route_id: str, data: bytes, filename: str = "") -> Route:
    """A new version of the file. KeyError for an unknown route, Unusable as add()."""
    with _lock:
        routes = load(store)
        route = next((r for r in routes if r.id == route_id), None)
        if route is None:
            raise KeyError(route_id)
        _set_file(store, route, data, Path(filename).stem if filename else route.name)
        _save(store, routes)
    return route


def update(store: Storage, route_id: str, name: str | None = None, kind: str | None = None,
           notes: str | None = None, tags: list[str] | None = None,
           archived: bool | None = None) -> Route:
    """Change what is given (None = keep). Archiving takes the route back from TrailBridge."""
    with _lock:
        routes = load(store)
        route = next((r for r in routes if r.id == route_id), None)
        if route is None:
            raise KeyError(route_id)
        if name is not None and name.strip():
            route.name = name.strip()
        if kind in KINDS:
            route.kind = kind
        if notes is not None:
            route.notes = notes
        if tags is not None:
            route.tags = _tags(tags)
        if archived is not None:
            route.archived = archived
            if archived:
                route.marked_at = None
        _save(store, routes)
    return route


def delete(store: Storage, route_id: str) -> bool:
    with _lock:
        routes = load(store)
        gone = next((r for r in routes if r.id == route_id), None)
        if gone is None:
            return False
        if gone.gpx:
            gpx_path(store, gone).unlink(missing_ok=True)
        _save(store, [r for r in routes if r is not gone])
    return True


def mark(store: Storage, route_id: str, on: bool = True) -> Route:
    """Hand this route to TrailBridge (on) or take it back. Marking one unmarks the others,
    and takes an archived one out of the archive. KeyError for an unknown route, Unusable for one TrailBridge cannot use."""
    with _lock:
        routes = load(store)
        route = next((r for r in routes if r.id == route_id), None)
        if route is None:
            raise KeyError(route_id)
        if on and route.verdict == "error":
            raise Unusable(route.check)
        for r in routes:
            if r is not route and on:
                r.marked_at = None
        route.marked_at = now_iso() if on else None
        if on:
            route.archived = False
        route.mark_count += on
        _save(store, routes)
    return route


def marked(store: Storage) -> Route | None:
    return next((r for r in load(store) if r.marked_at), None)


def fetched(store: Storage, revision: str, by: str) -> Route | None:
    """TrailBridge confirms it loaded this revision. None if that is not the marked
    route's current revision (it changed in between: the app loads it next time)."""
    with _lock:
        routes = load(store)
        route = next((r for r in routes if r.marked_at and r.revision == revision), None)
        if route is None:
            return None
        route.fetched_revision, route.fetched_at, route.fetched_by = revision, now_iso(), by
        _save(store, routes)
    return route


def recheck(store: Storage) -> int:
    """Redo stored checks made by an older CHECK_VERSION. Returns how many."""
    with _lock:
        routes = load(store)
        stale = [r for r in routes if r.check.get("version") != CHECK_VERSION and r.gpx
                 and gpx_path(store, r).exists()]
        for r in stale:
            r.check = _checked(gpx_path(store, r).read_bytes(), r.name)
        if stale:
            _save(store, routes)
    return len(stale)
