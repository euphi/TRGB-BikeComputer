"""Session reports and training data for the web pages: cached, kept up to date.

A report (bikelog.report) costs a full pass over the log -- on the home server a
second or so per ride -- and the training pages need all of them at once. So
each is stored in the ``reports`` table under a cache key made of
REPORT_VERSION, the L_ file's hash and the rider data it was computed with;
whatever no longer matches is computed again: after a pull (refresh(), called
next to the GPX export), when the report code changes, when the rider data
change. The refresh runs at the lowest CPU priority -- the service is always on,
a ride report may take as long as it likes.

Rider data (athlete.json), bikes and their bike computers (bikes.json, see
bikelog.bikes) and target events (events.json, course GPX files in events/) live as
files in the data directory, editable on the web pages and by hand. A report is
computed with the rider on the session's bike (weight, CdA, Crr), so the cache key
includes those.
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

from bikelog import bikes, report, training
from bikelog.record import ReadStats

from . import tours
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


def registry(store: Storage) -> bikes.Registry:
    """Bikes and which device rode on which bike when (bikes.json in the data directory)."""
    return bikes.Registry.load(bikes_path(store))


def bikes_path(store: Storage):
    return store.settings.data_dir / "bikes.json"


def session_day(session: Session) -> datetime.date | None:
    start = session.start_time
    return datetime.datetime.fromtimestamp(start).date() if start else None


def bike_for(store: Storage, session: Session, reg: bikes.Registry | None = None) -> bikes.Bike | None:
    """The session's own bike (set on import), else the device's bike on the ride's day."""
    reg = reg or registry(store)
    return reg.bike(session.bike_id) or reg.bike_for(session.device, session_day(session))


def rider_for(store: Storage, session: Session, base: report.Athlete | None = None,
              reg: bikes.Registry | None = None) -> report.Athlete:
    """Rider data as the report of this session needs them: the rider on this session's bike."""
    return bikes.effective_athlete(base or athlete(store), bike_for(store, session, reg))


def _rider_key(rider: report.Athlete) -> str:
    return hashlib.sha256(json.dumps(asdict(rider), sort_keys=True).encode()).hexdigest()[:12]


def cache_key(session: Session, rider_key: str) -> str | None:
    log_file = session.file("L")
    if log_file is None:
        return None
    return f"{report.REPORT_VERSION}:{log_file.sha256[:16]}:{rider_key}"


def _with_bike(rep: dict, bike: bikes.Bike | None) -> dict:
    if not rep.get("empty"):
        rep["meta"]["bike"] = {"id": bike.id, "name": bike.name, "type": bike.type} if bike else None
    return rep


def compute(store: Storage, session: Session, rider: report.Athlete | None = None) -> dict:
    stats = ReadStats()
    return report.compute(list(store.records(session, stats, types=None)),
                          rider or rider_for(store, session), stats)


def report_for(store: Storage, session: Session) -> dict:
    """The session's report, from the cache if it is current."""
    reg = registry(store)
    bike = bike_for(store, session, reg)
    rider = bikes.effective_athlete(athlete(store), bike)
    key = cache_key(session, _rider_key(rider))
    cached = store.cached_report(session.id)
    if cached and cached[0] == key:
        return cached[1]
    rep = _with_bike(compute(store, session, rider), bike)
    store.store_report(session.id, key, rep)
    return rep


def tour_key(store: Storage, group: list[Session], base: report.Athlete | None = None,
             reg: bikes.Registry | None = None) -> str:
    base, reg = base or athlete(store), reg or registry(store)
    return "tour|" + "|".join(
        f"{s.id}={cache_key(s, _rider_key(rider_for(store, s, base, reg)))}" for s in group)


def tour_report_for(store: Storage, group: list[Session], base: report.Athlete | None = None,
                    reg: bikes.Registry | None = None) -> dict:
    """The report of a tour: one session's own report, or -- for several sessions (a
    reboot on the way) -- one report over their concatenated records, so that stops,
    decoupling, best efforts and climbs run across the joins. The bike is the first
    session's (one device, one day)."""
    if len(group) == 1:
        return report_for(store, group[0])
    base, reg = base or athlete(store), reg or registry(store)
    key = tour_key(store, group, base, reg)
    cached = store.cached_tour_report(key)
    if cached is not None:
        return cached
    bike = bike_for(store, group[0], reg)
    rep = _with_bike(report.compute(tours.rebased_records(store, group),
                                    bikes.effective_athlete(base, bike)), bike)
    rep["meta"]["sessions"] = [s.id for s in group]
    store.store_tour_report(key, rep)
    return rep


def tours_of(store: Storage) -> list[list[Session]]:
    """All tours (groups of sessions a short pause apart) of all devices, oldest first;
    test, idle and unreadable sessions are not part of any."""
    out = []
    for device in store.devices():
        sessions = [s for s in store.sessions_for_device(device) if not s.log_error]
        out.extend(tours.group_into_tours(sessions, store.settings.komoot_merge_gap_s))
    return sorted(out, key=lambda g: g[0].first_time or 0)


def _lower_priority() -> None:
    try:        # Linux: a thread is a task, its native id works with setpriority
        os.setpriority(os.PRIO_PROCESS, threading.get_native_id(), 19)
    except (AttributeError, OSError):
        pass


def refresh(store: Storage, background: bool = True) -> int:
    """Compute every missing or stale report; returns how many."""
    try:
        return _refresh(store, background)
    except sqlite3.ProgrammingError:        # storage closed under us (shutdown): stop quietly
        return 0


def _refresh(store: Storage, background: bool) -> int:
    with _lock:
        if background:
            _lower_priority()
        try:
            base = athlete(store)
            reg = registry(store)
        except (OSError, ValueError, TypeError) as exc:
            log.warning("athlete/bikes file unusable, reports not refreshed: %s", exc)
            return 0
        have = store.cache_keys()
        done = 0
        for session in store.sessions_with_log():
            if session.log_error:               # known unreadable (the list says so): don't retry
                continue
            bike = bike_for(store, session, reg)
            rider = bikes.effective_athlete(base, bike)
            key = cache_key(session, _rider_key(rider))
            if have.get(session.id) == key:
                continue
            try:
                store.store_report(session.id, key, _with_bike(compute(store, session, rider), bike))
                done += 1
            except sqlite3.ProgrammingError:    # storage closed (shutdown): stop quietly
                return done
            except Exception as exc:            # unreadable log: skip, the page says so
                log.warning("report for session %d failed: %s", session.id, exc)
        for group in tours_of(store):
            if len(group) > 1:
                try:
                    if store.cached_tour_report(tour_key(store, group, base, reg)) is None:
                        tour_report_for(store, group, base, reg)
                        done += 1
                except sqlite3.ProgrammingError:
                    return done
                except Exception as exc:
                    log.warning("report for tour %s failed: %s", [s.id for s in group], exc)
        if done:
            log.info("%d session/tour report(s) computed", done)
        return done


def rides(store: Storage) -> list[training.Ride]:
    """Every tour that counts as training, as training.Ride, oldest first: sessions a
    short pause apart (a reboot on the way) are one ride. Test sessions are left out;
    one the rider marked as real counts even if the log says test."""
    out = []
    for group in tours_of(store):
        try:
            rep = tour_report_for(store, group)
        except Exception:
            continue
        ride = training.Ride.from_report(rep, group[0].id,
                                         allow_test=any(s.test_override == "real" for s in group))
        if ride:
            ride.session_ids = [s.id for s in group]
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
