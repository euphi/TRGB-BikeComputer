"""Upload a ride to Komoot via kompy (https://github.com/Tsadoq/kompy).

One session is one boot of the bike computer, but a stop long enough to
reboot -- a brief BLE hiccup, parking somewhere for a few minutes with the
computer switched off -- splits what was really one ride into several
sessions. Uploading each of those separately would give Komoot a handful of
disconnected fragments instead of one tour, so sessions of the same device
that are only a short pause apart (Settings.komoot_merge_gap_s) are grouped
and rendered as a single combined GPX before upload -- through the same
bikelog.gpx/bikelog.sanitize pipeline as everything else, so the jitter at
the join is trimmed exactly like a pause inside one session would be.

kompy is an optional dependency (``pip install "bikelog[komoot]"``); it, and
its own dependency gpxpy, are only imported once an upload is actually
attempted, so the rest of the service works without it installed.
"""

from __future__ import annotations

import dataclasses
import logging
from dataclasses import dataclass, field

from bikelog import gpx, ridestats
from bikelog.record import Record, labels_of, split

from . import exporter
from .storage import Session, Storage

log = logging.getLogger("bikelog.komoot")


def group_into_tours(sessions: list[Session], merge_gap_s: float) -> list[list[Session]]:
    """``sessions`` of one device, in ride order (Storage.sessions_for_device).
    Consecutive sessions end up in the same group when the gap between one
    ending and the next starting is at most merge_gap_s; a session with no
    known start/end (unreadable log) never merges with its neighbours."""
    groups: list[list[Session]] = []
    for session in sessions:
        prev = groups[-1][-1] if groups else None
        gap = (session.first_time - prev.last_time
               if prev and prev.last_time and session.first_time else None)
        if (prev is not None and prev.device == session.device
                and gap is not None and 0 <= gap <= merge_gap_s):
            groups[-1].append(session)
        else:
            groups.append([session])
    return groups


def group_for(store: Storage, session: Session, merge_gap_s: float | None = None) -> list[Session]:
    """The merge-group ``session`` currently belongs to."""
    merge_gap_s = store.settings.komoot_merge_gap_s if merge_gap_s is None else merge_gap_s
    for group in group_into_tours(store.sessions_for_device(session.device), merge_gap_s):
        if any(s.id == session.id for s in group):
            return group
    return [session]                # unreachable in practice: session always lists itself


def _rebased_records(store: Storage, sessions: list[Session]) -> list:
    """Concatenated record streams of ``sessions`` in time order, with each
    session's cumulative trip distance (Record.distance, the wheel sensor's
    running total since ITS boot) continuing on from the previous session's
    instead of restarting at zero -- otherwise a merged tour's distance and
    per-point BC:dist extension would collapse to roughly the last
    session's distance alone."""
    combined = []
    offset = 0.0
    for session in sessions:
        last_distance = 0.0
        for rec in store.records(session, types=None):
            if isinstance(rec, Record):
                last_distance = rec.distance
                rec = dataclasses.replace(rec, distance=rec.distance + offset)
            combined.append(rec)
        offset += last_distance
    return combined


@dataclass
class TourRender:
    xml: str
    stats: "gpx.GpxStats"
    name: str
    moving_s: float | None


def render_tour(store: Storage, sessions: list[Session]) -> TourRender:
    """The combined GPX (sanitized, real_distance_m already computed) for one
    merge-group -- shared by the upload and anything that wants to preview it
    first.

    No <wpt> waypoints: Komoot's tour-import endpoint rejects a GPX that has
    any (400 "query is required for type=tour_planned" -- it seems to decide
    from their presence alone that this must be a planned route, not a
    recorded activity, and demands the query parameters a planned-route
    import needs instead). Confirmed 2026-09-27 against the live API: the
    per-trackpoint bc: extensions upload just fine, only shocks/labels as
    waypoints make Komoot reject the file. So this drops just those two --
    they are exactly the current, and only, source of <wpt> elements.
    """
    everything = _rebased_records(store, sessions)
    options = exporter.gpx_options(sessions[0], store.settings)
    options.shocks = False
    options.labels = False
    device_summary = sessions[0].summary if len(sessions) == 1 else None
    xml, stats = gpx.from_records(everything, options, device_summary=device_summary)
    records, road, shocks = split(everything)
    ride = ridestats.compute(records, road, shocks, labels_of(everything))
    return TourRender(xml=xml, stats=stats, name=gpx.default_track_name(stats.first_time),
                      moving_s=ride.moving_s or None)


@dataclass
class UploadResult:
    status: str      # "uploaded" | "duplicate" | "too-short" | "no-gps" | "not-configured" | "error"
    message: str | None = None
    session_ids: list[int] = field(default_factory=list)


def upload_group(store: Storage, sessions: list[Session]) -> UploadResult:
    settings = store.settings
    ids = [s.id for s in sessions]
    if not settings.komoot_enabled:
        return UploadResult("not-configured", "BIKELOG_KOMOOT_EMAIL/BIKELOG_KOMOOT_PASSWORD not set", ids)
    try:
        import gpxpy
        import kompy
    except ImportError:
        return UploadResult("not-configured",
                            'the "kompy" package is not installed -- pip install "bikelog[komoot]"', ids)

    render = render_tour(store, sessions)
    if render.stats.written == 0:
        return UploadResult("no-gps", "no usable GPS fix in this tour", ids)
    if render.stats.real_distance_m < exporter.MIN_EXPORT_DISTANCE_M:
        return UploadResult("too-short", "%.0f m of real movement" % render.stats.real_distance_m, ids)

    try:
        connector = kompy.KomootConnector(email=settings.komoot_email, password=settings.komoot_password)
        ok = connector.upload_tour(
            tour_object=gpxpy.parse(render.xml),
            activity_type=settings.komoot_activity,
            tour_name=render.name,
            time_in_motion=int(render.moving_s) if render.moving_s else None,
            status=settings.komoot_status,
        )
    except Exception as exc:                    # network/auth/API failure -- report, don't crash the request
        log.error("Komoot upload failed for sessions %s: %s", ids, exc)
        return UploadResult("error", str(exc), ids)

    # kompy reports "already present" (HTTP 202) the same as a fresh upload
    # (HTTP 201) -- both count as success here, we cannot tell them apart or
    # recover the tour id from either (see the module docstring).
    status = "uploaded" if ok else "error"
    store.set_komoot(ids, status, None)
    return UploadResult(status, None if ok else "Komoot rejected the upload", ids)
