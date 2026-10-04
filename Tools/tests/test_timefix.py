"""Clock steps inside a session (a wrong GPS time on a valid clock, 2026-10-04):
the records before the step are shifted, the last clock is the truth."""

import io

from bikelog import fixtures, timefix
from bikelog.record import ReadStats, read_stream, write_records

HINTS = (
    "start 1791109663952 1 3042\n"
    "step gps -53350119 1791056328806 18015\n"
    "step gps 53317328 1791113242307 3614187\n"
)


def _times(n=3000, step_at=(5, 2000), jumps=(-53_348_000, 53_319_000), dt=2000, t0=1_791_109_666_000):
    t, out = t0, []
    for i in range(n):
        if i in step_at:
            t += jumps[step_at.index(i)]
        out.append(t)
        t += dt
    return out


def test_parse_hints_skips_the_step_that_set_an_unset_clock():
    text = ("start 3158 0 3157\nstep gps 1791029528356 1791029546481 18123\n"
            "step ntp 1237 1791029570000 38026\nstep gps -53350119 1791056328806 18015\n")
    assert timefix.parse_hints(text) == [1237, -53350119]


def test_hinted_steps_are_found_at_the_jumps():
    steps = timefix.find_steps(_times(), timefix.parse_hints(HINTS))
    assert steps == [(5, -53350119), (2000, 53317328)]


def test_records_before_the_step_move_by_the_steps_after_them():
    times = _times()
    shift = timefix.shifts(times, timefix.parse_hints(HINTS))
    fixed = [t + s for t, s in zip(times, shift)]
    assert shift[3000 - 1] == 0                         # after the last step: the truth
    assert fixed[1999] + 2000 - fixed[2000] in range(-3000, 3000)    # continuous at the second step
    assert all(b >= a for a, b in zip(fixed, fixed[1:]))
    assert fixed[0] - times[0] == -32_791               # the RTC was 33 s ahead of GPS


def test_without_hints_only_a_jump_back_and_the_jump_that_undoes_it_are_repaired():
    times = _times()
    shift = timefix.shifts(times)
    fixed = [t + s for t, s in zip(times, shift)]
    assert all(b >= a for a, b in zip(fixed, fixed[1:]))
    assert max(fixed) - min(fixed) < 3000 * 2000 + 10_000


def test_a_real_pause_is_not_a_step():
    times = [1_791_000_000_000 + i * 2000 for i in range(100)]
    times += [t + 3_600_000 for t in times[-1:]] + [times[-1] + 3_600_000 + i * 2000 for i in range(1, 50)]
    assert timefix.find_steps(times, []) == []
    assert not any(timefix.shifts(times))


def test_small_jitter_steps_are_ignored():
    times = [1_791_000_000_000 + i * 2000 + (3000 if i > 50 else 0) for i in range(100)]
    assert timefix.find_steps(times, [3000]) == []


def test_read_stream_repairs_timestamps(tmp_path):
    records = fixtures.synthetic(seconds=3000, no_fix_start_s=0)
    for i, rec in enumerate(records):
        if 5 <= i < 2000:
            rec.timestamp -= 53350
        rec.timestamp_ms = 0
    path = tmp_path / "L_0001.bin"
    write_records(path, records)
    raw = path.read_bytes()
    hints = "step gps -53350000 1791056328806 18015\nstep gps 53350000 1791113242307 3614187\n"

    plain = list(read_stream(io.BytesIO(raw)))
    assert plain[5].timestamp < plain[4].timestamp - 1000

    stats = ReadStats()
    fixed = list(read_stream(io.BytesIO(raw), stats, repair_time=True, time_hints=hints))
    assert stats.time_steps == 2
    assert all(b.timestamp >= a.timestamp for a, b in zip(fixed, fixed[1:]))
    assert fixed[-1].timestamp == plain[-1].timestamp
    assert fixed[0].timestamp == plain[0].timestamp
