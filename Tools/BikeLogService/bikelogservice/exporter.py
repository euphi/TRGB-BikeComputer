"""Automatic GPX export: one file per session, kept up to date.

Every session with a binary log gets ``<export dir>/<Tours|Debug_Archive>/
2026-09-27_164545_trgb.gpx`` -- the rich export from bikelog.gpx (Garmin
TrackPointExtension plus this project's own extension with gradient, road
quality and labels, shocks and label changes as waypoints, ride summary in
the metadata). SUBDIR_TOURS is meant to be picked up by whatever comes next
(Nextcloud sync, a Komoot/Strava uploader); SUBDIR_DEBUG holds everything
that is not a real ride -- a session parked somewhere with the phone's GPS
wandering a few metres, a five-minute test -- kept for reference but out of
the uploaders' way. File names sort chronologically and each file's mtime is
the ride's start.

A file is (re)written -- and, if its distance moved it from one subdirectory
to the other, moved -- when the session's L_ or I_ file arrived or changed,
or when EXPORT_VERSION is bumped -- raise it whenever the GPX content or the
Tours/Debug_Archive split changes so that old rides are re-sorted too.

Sessions without a single usable GPS fix get no file at all (status
"no-gps"): an empty track helps nobody and every importer chokes on it.
"""

from __future__ import annotations

import datetime
import logging
import os
import threading

from bikelog import gpx

from .storage import Session, Storage

log = logging.getLogger("bikelog.export")

EXPORT_VERSION = 3

#: Below this, a session goes to Debug_Archive instead of Tours -- see the
#: module docstring. Raise/lower it here, not per-session; the automatic
#: export has no UI to configure it from.
MIN_EXPORT_DISTANCE_M = 1000.0

SUBDIR_TOURS = "Tours"
SUBDIR_DEBUG = "Debug_Archive"

_lock = threading.Lock()        # puller thread, request threads and startup may all call in


def gpx_options(session: Session, settings) -> gpx.GpxOptions:
    link = f"{settings.public_url}/#s{session.id}" if settings.public_url else None
    return gpx.GpxOptions(device=session.device, link=link)


def render(store: Storage, session: Session, options: gpx.GpxOptions | None = None):
    """(xml, GpxStats) of a session -- shared by the export and the download route."""
    return gpx.from_records(store.records(session, types=None),
                            options or gpx_options(session, store.settings),
                            device_summary=session.summary)


def file_name(session: Session, first_fix: datetime.datetime | None) -> str:
    """Local start time + device; the SD card's day/stem as a fallback."""
    start = session.start_time
    when = (datetime.datetime.fromtimestamp(start) if start
            else first_fix.astimezone() if first_fix else None)
    stamp = when.strftime("%Y-%m-%d_%H%M%S") if when else f"{session.day or 'root'}{session.stem}"
    return f"{stamp}_{session.device}.gpx"


def export_session(store: Storage, session: Session) -> str:
    settings = store.settings
    root = settings.gpx_dir
    try:
        xml, stats = render(store, session)
    except Exception as exc:                    # unreadable log: record it, keep going
        store.set_export(session.id, None, f"error: {exc}", EXPORT_VERSION)
        return "error"
    old = session.gpx_file            # previous relative path, for cleanup below
    if stats.written == 0:
        rel, status = None, "no-gps"
    else:
        real_ride = stats.real_distance_m >= MIN_EXPORT_DISTANCE_M
        subdir = SUBDIR_TOURS if real_ride else SUBDIR_DEBUG
        status = "ok" if real_ride else "debug"
        rel = f"{subdir}/{file_name(session, stats.first_time)}"
        path = root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp = path.with_name(path.name + ".part")
        tmp.write_text(xml, encoding="utf-8")
        if stats.first_time:
            ts = stats.first_time.timestamp()
            os.utime(tmp, (ts, ts))
        tmp.replace(path)
    if old and old != rel:
        (root / old).unlink(missing_ok=True)     # renamed, moved between Tours/Debug_Archive, or no longer valid
    store.set_export(session.id, rel, status, EXPORT_VERSION)
    log.info("session %d (%s%s): GPX %s%s", session.id, session.day, session.stem, status,
             f" -> {rel} ({stats.written} points)" if rel else "")
    return status


def export_pending(store: Storage) -> dict[str, int]:
    if not store.settings.export_gpx:
        return {}
    counts: dict[str, int] = {}
    with _lock:
        for session in store.pending_exports(EXPORT_VERSION):
            status = export_session(store, session)
            counts[status] = counts.get(status, 0) + 1
    if counts:
        log.info("GPX export to %s: %s", store.settings.gpx_dir,
                 ", ".join(f"{n} {k}" for k, n in sorted(counts.items())))
    return counts
