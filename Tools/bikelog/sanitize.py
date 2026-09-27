"""Remove GPS jitter from a record stream before it becomes a GPX.

The bike computer keeps logging at 1 Hz while stopped -- at a red light, a
coffee break, or simply before the first pedal stroke of the ride -- and the
phone's fix keeps wandering a few metres around that one spot the whole
time. Left in, every stop turns into a little scribble on the map instead of
a clean point. This has nothing to do with GpxOptions.segment_gap_s (which
only decides where a <trkseg> break goes, for a viewer to draw a dashed
line): trim_jitter() only ever *removes* points, never joins or splits
anything, so it is safe to run before any GPX export -- a single session or
several concatenated ones (see bikelogservice/komoot.py).

"Is the bike actually moving" is not something to reconstruct from noisy GPS
deltas when the log already has the answer: the wheel sensor's speed. A
short run of points below MOVING_KMH is a real stop, not a guess -- and it
is immune to the pathological case a pure GPS-distance heuristic has, where
a short/slow track looks like nothing but jitter from end to end.

Standalone (no dependency on bikelog.gpx) so it can be unit tested on plain
Record lists and reused wherever a record stream is turned into a GPX.
"""

from __future__ import annotations

from dataclasses import dataclass

from .record import Record
from .ridestats import MOVING_KMH


@dataclass
class SanitizeOptions:
    #: A record slower than this (wheel-sensor speed, same field and
    #: threshold ridestats.compute() uses for "moving") counts as stopped.
    moving_kmh: float = MOVING_KMH
    #: Only clean up around gaps in usable fixes at least this long. A
    #: normal 1 Hz stream has no gap at all between consecutive fixes, so any
    #: gap already means the ride paused there.
    min_pause_s: float = 10.0


def _has_fix(rec: Record) -> bool:
    return rec.gps_valid and not (rec.gps_lat_e7 == 0 and rec.gps_lon_e7 == 0)


def trim_jitter(records: list[Record], opts: SanitizeOptions | None = None) -> list[Record]:
    """Collapse the stopped run of fixes at the very start and end of
    ``records``, and around every pause of at least ``min_pause_s`` in
    between, down to the one fix on each side closest to when the bike was
    actually moving -- so the track starts/stops/resumes at a single clean
    point instead of a jittering blob.

    ``records`` must be in time order (one log, or several concatenated
    session logs). Records without a usable fix are left alone and do not
    themselves break up a stopped run.
    """
    opts = opts or SanitizeOptions()
    records = list(records)
    positions = [i for i, rec in enumerate(records) if _has_fix(rec)]
    n = len(positions)
    if n < 2:
        return records

    stopped = [records[idx].speed < opts.moving_kmh for idx in positions]
    drop: set[int] = set()

    def collapse(lo: int, hi: int, keep: int) -> None:
        """positions[lo..hi] is one stopped run pressed against a boundary;
        keep only positions[keep] (the one right at the boundary)."""
        for k in range(lo, hi + 1):
            if k != keep:
                drop.add(positions[k])

    k = 0
    while k < n and stopped[k]:
        k += 1
    if k > 0:                               # stopped before the ride got going
        collapse(0, k - 1, k - 1)

    k = n - 1
    while k >= 0 and stopped[k]:
        k -= 1
    if k < n - 1:                           # stopped after it was well and truly over
        collapse(k + 1, n - 1, k + 1)

    for k in range(1, n):
        gap = records[positions[k]].time - records[positions[k - 1]].time
        if gap < opts.min_pause_s:
            continue
        j = k - 1
        while j >= 0 and stopped[j]:
            j -= 1
        if j < k - 1:
            collapse(j + 1, k - 1, k - 1)   # settling in before the pause
        j = k
        while j < n and stopped[j]:
            j += 1
        if j > k:
            collapse(k, j - 1, k)           # wandering before moving off again

    return [rec for i, rec in enumerate(records) if i not in drop]
