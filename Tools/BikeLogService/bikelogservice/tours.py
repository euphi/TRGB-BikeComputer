"""One ride, several sessions: grouping by time.

One session is one boot of the bike computer. A reboot on the way (a crash, a BLE
hiccup that ends in a reset, the computer switched off at a café stop) splits one ride
into several sessions. Consecutive sessions of the same device whose gap -- the end of
one to the start of the next -- is at most Settings.komoot_merge_gap_s are one tour:
for the Komoot upload, for the training figures (longest ride, load) and for the tour
report. The times are reliable enough for that: after a reset without clock the
firmware takes the phone's GPS time (ClockSync), the next boot corrects the timestamps
written before (time hints), and the service repairs clock steps (bikelog.timefix). A
session whose clock was never set (no WLAN, no TrailBridge) has no usable time and
stays on its own.

Test and idle sessions never join a tour (Storage.sessions_for_device leaves them out).
"""

from __future__ import annotations

import dataclasses

from bikelog.record import Record

from .storage import Session, Storage


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


def rebased_records(store: Storage, sessions: list[Session]) -> list:
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
