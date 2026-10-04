"""Session reports and training data for the web pages: cached, kept up to date.

A report (bikelog.report) costs a full pass over the log -- on the home server a
second or so per ride -- and the training pages need all of them at once. So
each is stored in the ``reports`` table under a cache key made of
REPORT_VERSION, the L_ file's hash and the rider data it was computed with;
whatever no longer matches is computed again: after a pull (refresh(), called
next to the GPX export), when the report code changes, when the rider data
change. The refresh runs at the lowest CPU priority -- the service is always on,
a ride report may take as long as it likes.

Rider data (athlete.json) and target events (events.json, course GPX files in
events/) live as files in the data directory, editable on the web pages and by
hand.
"""

from __future__ import annotations

import datetime
import hashlib
import json
import logging
import os
import sqlite3
import threading
from dataclasses import asdict

from bikelog import report, training
from bikelog.record import ReadStats

from .storage import Session, Storage

log = logging.getLogger("bikelog.analysis")

_lock = threading.Lock()        # one refresh at a time


def athlete(store: Storage) -> report.Athlete:
    path = store.settings.athlete_path
    return report.Athlete.load(path) if path.exists() else report.Athlete()


def save_athlete(store: Storage, data: report.Athlete) -> None:
    path = store.settings.athlete_path
    values = asdict(data)
    values["zones_pct"] = list(data.zones_pct)
    tmp = path.with_name(path.name + ".part")
    tmp.write_text(json.dumps(values, indent=2), encoding="utf-8")
    tmp.replace(path)


def _athlete_key(store: Storage) -> str:
    path = store.settings.athlete_path
    return hashlib.sha256(path.read_bytes()).hexdigest()[:12] if path.exists() else "default"


def cache_key(session: Session, athlete_key: str) -> str | None:
    log_file = session.file("L")
    if log_file is None:
        return None
    return f"{report.REPORT_VERSION}:{log_file.sha256[:16]}:{athlete_key}"


def compute(store: Storage, session: Session, rider: report.Athlete | None = None) -> dict:
    stats = ReadStats()
    return report.compute(list(store.records(session, stats, types=None)),
                          rider or athlete(store), stats)


def report_for(store: Storage, session: Session) -> dict:
    """The session's report, from the cache if it is current."""
    key = cache_key(session, _athlete_key(store))
    cached = store.cached_report(session.id)
    if cached and cached[0] == key:
        return cached[1]
    rep = compute(store, session)
    store.store_report(session.id, key, rep)
    return rep


def _lower_priority() -> None:
    try:        # Linux: a thread is a task, its native id works with setpriority
        os.setpriority(os.PRIO_PROCESS, threading.get_native_id(), 19)
    except (AttributeError, OSError):
        pass


def refresh(store: Storage, background: bool = True) -> int:
    """Compute every missing or stale report; returns how many."""
    with _lock:
        if background:
            _lower_priority()
        try:
            rider = athlete(store)
        except (OSError, ValueError, TypeError) as exc:
            log.warning("athlete file unusable, reports not refreshed: %s", exc)
            return 0
        akey = _athlete_key(store)
        have = store.cache_keys()
        done = 0
        for session in store.sessions_with_log():
            key = cache_key(session, akey)
            if have.get(session.id) == key:
                continue
            try:
                store.store_report(session.id, key, compute(store, session, rider))
                done += 1
            except sqlite3.ProgrammingError:    # storage closed (shutdown): stop quietly
                return done
            except Exception as exc:            # unreadable log: skip, the page says so
                log.warning("report for session %d failed: %s", session.id, exc)
        if done:
            log.info("%d session report(s) computed", done)
        return done


def rides(store: Storage) -> list[training.Ride]:
    """Every session that counts as training, as training.Ride, oldest first."""
    out = []
    for session in store.sessions_with_log():
        try:
            rep = report_for(store, session)
        except Exception:
            continue
        ride = training.Ride.from_report(rep, session.id)
        if ride:
            out.append(ride)
    return sorted(out, key=lambda r: r.start)


# --- events ----------------------------------------------------------------------

def events_path(store: Storage):
    return store.settings.data_dir / "events.json"


def events_dir(store: Storage):
    path = store.settings.data_dir / "events"
    path.mkdir(parents=True, exist_ok=True)
    return path


def events(store: Storage) -> list[training.Event]:
    return training.load_events(events_path(store))


def save_event(store: Storage, event: training.Event) -> None:
    all_events = [e for e in events(store) if e.id != event.id] + [event]
    training.save_events(events_path(store), all_events)


def delete_event(store: Storage, event_id: str) -> bool:
    all_events = events(store)
    keep = [e for e in all_events if e.id != event_id]
    if len(keep) == len(all_events):
        return False
    gone = next(e for e in all_events if e.id == event_id)
    if gone.gpx:
        (events_dir(store) / gone.gpx).unlink(missing_ok=True)
    training.save_events(events_path(store), keep)
    return True


def set_event_gpx(store: Storage, event_id: str, data: bytes) -> training.Event:
    """Store the course GPX of an event and its profile. ValueError if unusable,
    KeyError for an unknown event."""
    event = next((e for e in events(store) if e.id == event_id), None)
    if event is None:
        raise KeyError(event_id)
    profile = training.event_profile(data)
    name = f"{event.id}.gpx"
    (events_dir(store) / name).write_bytes(data)
    event.gpx, event.profile = name, profile
    save_event(store, event)
    return event


def today() -> datetime.date:
    return datetime.date.today()
