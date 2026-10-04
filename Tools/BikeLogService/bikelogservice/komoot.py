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

import logging
from dataclasses import dataclass, field

from bikelog import gpx, ridestats
from bikelog.record import labels_of, split

from . import exporter, tours
from .storage import Session, Storage

log = logging.getLogger("bikelog.komoot")


# The grouping lives in tours.py (training and the pages use it too); these names stay
# for the callers that know them from here.
group_into_tours = tours.group_into_tours
group_for = tours.group_for
_rebased_records = tours.rebased_records


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
    everything = tours.rebased_records(store, sessions)
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


def prompt_eligible(session: Session) -> bool:
    """A ride the page should ask about: a real ride (Tours export), not uploaded yet, and
    neither declined nor already asked-and-ignored."""
    return (session.gpx_status == "ok" and session.komoot_status != "uploaded"
            and session.komoot_prompt is None)


def pending_prompts(store: Storage) -> list[list[Session]]:
    """The tours (merge-groups) to ask "upload to Komoot?" about, oldest first. Empty if
    Komoot is not configured. The question stays until it is answered -- uploaded, or
    ignored (Storage.set_komoot_prompt): closing the page does not answer it."""
    if not store.settings.komoot_enabled:
        return []
    out: list[list[Session]] = []
    for device in store.devices():
        sessions = store.sessions_for_device(device)
        for group in group_into_tours(sessions, store.settings.komoot_merge_gap_s):
            if any(prompt_eligible(s) for s in group) and not any(s.komoot_status == "uploaded" for s in group):
                out.append(group)
    out.sort(key=lambda g: g[0].first_time or 0)
    return out
