"""Repair of timestamps after clock steps inside one session.

The ESP32's clock can be stepped while a session is being logged -- by NTP, or by
the phone's GPS time (src/ClockSync.h). A step that moves a clock which was already
valid is not corrected by the firmware (it only repairs the unset 1970 clock, see
src/SessionStats.h), so the records written before the step carry the old time.
On the test ride of 2026-10-04 the phone's GNSS chip reported a time 14.8 h behind
for the first hour; the clock was stepped back after 18 s and forward again after
an hour, and a 2 h ride ended up as one lasting 17 h with an hour of the previous
evening in it.

The time-hint file T_<stem>.txt lists every step (offset in ms). The records show
where they happened: the timestamp jumps. The *last* clock is taken as the truth
(NTP, or GPS after the phone's chip had caught up), so a record written before a
step is moved by the sum of all steps after it:

    corrected = t + sum(step_j  for every step j after the record)

Without a hint file only what cannot be a real pause is repaired: a jump back, and
a jump forward that cancels one.
"""

from __future__ import annotations

from typing import Iterable, Sequence

#: Consecutive records further apart than this are a candidate for a clock step.
#: The logger writes every 1..5 s; a real pause (standby) is possible, hence the
#: matching against the hints.
JUMP_MS = 120_000
#: A jump matches a hinted step if it differs by less than this (the real time
#: between the two records is in it).
MATCH_TOLERANCE_MS = 300_000
#: Steps smaller than this are NTP/GPS jitter -- not worth shifting anything for.
MIN_STEP_MS = JUMP_MS
#: Typical spacing of two records, taken off a jump that no hint explains.
NOMINAL_DT_MS = 2000


def parse_hints(text: str | bytes | None) -> list[int]:
    """Offsets in ms of the steps listed in a T_*.txt ("step <src> <offsetMs> <newEpochMs> <uptimeMs>").

    The firmware writes the offset of the step that makes an unset clock valid as
    the whole epoch (1791028260508); those are no jitter-sized steps but not
    steps of a *valid* clock either, so a step whose result minus offset is not
    a valid time (before 2023) is skipped -- same rule as SessStats::TimeHints.
    """
    if text is None:
        return []
    if isinstance(text, bytes):
        text = text.decode("utf-8", "replace")
    steps: list[int] = []
    for line in text.splitlines():
        parts = line.split()
        if len(parts) < 4 or parts[0] != "step":
            continue
        try:
            offset, new_epoch = int(parts[2]), int(parts[3])
        except ValueError:
            continue
        if new_epoch - offset < 1_672_531_200_000:
            continue
        steps.append(offset)
    return steps


def find_steps(times_ms: Sequence[int], hinted: Iterable[int] = ()) -> list[tuple[int, int]]:
    """[(record index, step in ms)] of the clock steps visible in the record times.

    The step is the hinted offset where one matches the jump, else the jump itself
    (less a nominal NOMINAL_DT_MS of real time).
    """
    pool = [s for s in hinted if abs(s) >= MIN_STEP_MS]
    steps: list[tuple[int, int]] = []
    outstanding = 0            # sum of the steps found so far
    for i in range(1, len(times_ms)):
        jump = times_ms[i] - times_ms[i - 1]
        if abs(jump) < JUMP_MS:
            continue
        match = min(pool, key=lambda s: abs(jump - s), default=None)
        if match is not None and abs(jump - match) < MATCH_TOLERANCE_MS:
            pool.remove(match)
            steps.append((i, match))
            outstanding += match
            continue
        step = jump - NOMINAL_DT_MS     # the real time between the two records is in the jump
        if jump < 0 or (outstanding < 0 and abs(step + outstanding) < MATCH_TOLERANCE_MS):
            steps.append((i, step))
            outstanding += step
    return steps


def shifts(times_ms: Sequence[int], hinted: Iterable[int] = ()) -> list[int]:
    """The shift in ms to add to every record time (all 0 if nothing was stepped)."""
    steps = find_steps(times_ms, hinted)
    out = [0] * len(times_ms)
    if not steps:
        return out
    after = 0
    j = len(steps) - 1
    for i in range(len(times_ms) - 1, -1, -1):
        while j >= 0 and steps[j][0] > i:
            after += steps[j][1]
            j -= 1
        out[i] = after
    return out
