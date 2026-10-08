"""Rides recorded elsewhere (Garmin, Strava, Komoot, ...) as GPX -> log records.

So that an old ride counts like one of the bike computer's: the same report, the same
training load, the same climbs. fixtures.from_gpx does the conversion (position, time,
height, and heart rate/cadence/temperature from Garmin's TrackPointExtension); on top
of that the gradient is smoothed over GRADE_WINDOW_M, because GPX heights at one point
per second jump by a few decimetres and their raw gradient would make the power
estimate noise.

A GPX without <time> is a planned route, not a ride -- that one belongs to a goal
(training.event_profile), not here.
"""

from __future__ import annotations

import bisect
import io

from . import fixtures
from .record import Record

#: half the window over which the gradient is taken (metres before and after a point)
GRADE_WINDOW_M = 50.0


class NotARide(ValueError):
    """The GPX holds no recorded ride (no track points with time)."""


def smooth_gradient(records: list[Record], window_m: float = GRADE_WINDOW_M) -> None:
    """Gradient [%] of each record from the heights window_m before and after it, in place."""
    dist = [r.distance for r in records]
    for i, rec in enumerate(records):
        lo = bisect.bisect_left(dist, rec.distance - window_m)
        hi = min(len(records) - 1, bisect.bisect_right(dist, rec.distance + window_m) - 1)
        span = dist[hi] - dist[lo]
        if span >= window_m and records[lo].height and records[hi].height:
            grade = (records[hi].height - records[lo].height) / span * 100
            rec.gradient = max(-30.0, min(30.0, grade))
        else:
            rec.gradient = 0.0


def records_from_gpx(data: bytes) -> list[Record]:
    try:
        records = fixtures.from_gpx(io.BytesIO(data))
    except ValueError as exc:
        raise NotARide("GPX ohne Zeitstempel -- das ist eine geplante Route, keine aufgezeichnete "
                       "Fahrt (Routen gehören zu einem Ziel)") from exc
    if len(records) < 2:
        raise NotARide("GPX ohne Track-Punkte")
    smooth_gradient(records)
    return records
