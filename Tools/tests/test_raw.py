"""Raw accelerometer files (src/RawCapture.h) and the replay through the firmware algorithm."""

import math
import random
import shutil
from array import array

import pytest

from bikelog import raw, replay
from bikelog.cli import main as cli_main

ODR = 400
LSB = 2048.0


def _bump(i, start, peak_g, ms=12):
    n = int(ms * ODR / 1000)
    return peak_g * math.sin(math.pi * (i - start) / n) if start <= i < start + n else 0.0


def _capture(tmp_path, seconds=10, kmh=20.0, bumps=True):
    """A continuous capture as the firmware writes it: 20-frame blocks, sensor mounted
    level (z up), light road noise, and a pothole hit by both wheels at t = 5 s."""
    rng = random.Random(3)
    header = raw.RawHeader(kind=raw.KIND_CAPTURE, scale=1.0, g0=(0.0, 0.0, 1.0),
                           start_epoch_ms=1790000000000)
    front = 5 * ODR
    rear = front + int(1.05 / (kmh / 3.6) * ODR)
    blocks = []
    total = seconds * ODR
    for b0 in range(0, total, 20):
        frames = array("h")
        for i in range(b0, b0 + 20):
            z = 1.0 + rng.gauss(0, 0.05)
            if bumps:
                z += _bump(i, front, 7.0) + _bump(i, rear, 3.5)
            frames.extend((int(rng.gauss(0, 0.02) * LSB), int(rng.gauss(0, 0.02) * LSB), int(z * LSB)))
        blocks.append(raw.RawBlock(type=raw.BLOCK_CONTINUOUS, speed_cms=round(kmh / 3.6 * 100),
                                   epoch_ms=header.start_epoch_ms + b0 * 1000 // ODR, ref=b0 // 20,
                                   speed_age_ms=(b0 * 1000 // ODR) % 1000, frames=frames))
    path = tmp_path / "R_120000_01.bin"
    raw.write_raw(path, header, blocks)
    return path, header, blocks


def test_roundtrip_and_sizes(tmp_path):
    path, header, blocks = _capture(tmp_path)
    assert raw.HEADER_SIZE == 64 and raw.BLOCK_SIZE == 24
    # ~150 KB per minute, as documented
    assert path.stat().st_size == 64 + len(blocks) * (24 + 20 * 6)
    stats = raw.RawStats()
    h, back = raw.read_raw(path, stats)
    assert (h.kind, h.odr_hz, h.start_epoch_ms, h.pre_ms, h.post_ms) == (
        header.kind, header.odr_hz, header.start_epoch_ms, header.pre_ms, header.post_ms)
    assert h.wheelbase_m == pytest.approx(header.wheelbase_m)      # float32 on disk
    assert stats.blocks == len(blocks) and stats.frames == 10 * ODR
    assert back[0].frames == blocks[0].frames
    assert back[3].speed_kmh == pytest.approx(20.0, abs=0.02)
    x, y, z = back[0].g(h)[0]
    assert z == pytest.approx(1.0, abs=0.3)


def test_label_bytes_roundtrip(tmp_path, capsys):
    path, header, blocks = _capture(tmp_path, seconds=4, bumps=False)
    for b in blocks[len(blocks) // 2:]:
        b.label_surface, b.label_quality = 5, 4          # Pflaster, worst
    raw.write_raw(path, header, blocks)
    _, back = raw.read_raw(path)
    assert (back[0].label_surface, back[0].label_quality) == (0, 0)
    assert (back[-1].label_surface, back[-1].label_quality) == (5, 4)
    assert cli_main(["raw", "info", "-i", str(path)]) == 0
    assert "Pflaster Q4" in capsys.readouterr().out


def test_damaged_block_is_skipped_not_fatal(tmp_path):
    path, _, blocks = _capture(tmp_path, seconds=1)
    data = bytearray(path.read_bytes())
    data[64 + 144] ^= 0xFF                     # break the sync word of the second block
    stats = raw.RawStats()
    _, back = raw.parse(bytes(data) + b"\x0c\xb1\x00", stats)
    assert len(back) == len(blocks) - 1
    assert stats.resyncs >= 1 and stats.trailing_bytes == 3


def test_not_a_raw_file(tmp_path):
    p = tmp_path / "L_1.bin"
    p.write_bytes(b"\0" * 128)
    with pytest.raises(raw.NotARawFile):
        raw.read_raw(p)


needs_cxx = pytest.mark.skipif(not (shutil.which("g++") or shutil.which("clang++")),
                               reason="needs a host C++ compiler")


@needs_cxx
def test_replay_capture_finds_the_pothole(tmp_path):
    path, _, _ = _capture(tmp_path)
    intervals, shocks, summary = replay.run(path)
    assert len(intervals) == 5
    assert all(i.road_class >= 1 for i in intervals)
    assert len(shocks) == 1
    s = shocks[0]
    assert s.severity == 2 and s.flags & 0x02            # front + rear wheel: wheelbase match
    assert s.t_ms == pytest.approx(5000 + 15, abs=10)   # peak of the 12 ms bump
    # a parameter changes the outcome
    _, shocks_hi, _ = replay.run(path, {"shock": 9.0})
    assert shocks_hi == []


@needs_cxx
def test_replay_passes_the_label_through(tmp_path):
    path, header, blocks = _capture(tmp_path, bumps=True)
    for b in blocks:
        b.label_surface, b.label_quality = 3, 2          # Waldweg
    raw.write_raw(path, header, blocks)
    intervals, shocks, _ = replay.run(path)
    assert {(i.label_surface, i.label_quality) for i in intervals} == {(3, 2)}
    assert [(s.label_surface, s.label_quality) for s in shocks] == [(3, 2)]


@needs_cxx
def test_replay_snippets_file(tmp_path):
    rng = random.Random(5)
    header = raw.RawHeader(kind=raw.KIND_SNIPPETS, scale=1.0, start_epoch_ms=1790000000000)
    blocks = []
    # after the 80 Hz low-pass a 12 ms bump keeps ~85 % of its peak: 11 g -> ~9 g, severity 3
    for n, peak in enumerate((4.0, 11.0), start=1):
        frames = array("h")
        for i in range(300):
            z = 1.0 + rng.gauss(0, 0.05) + _bump(i, 95, peak)
            frames.extend((0, 0, int(z * LSB)))
        blocks.append(raw.RawBlock(type=raw.BLOCK_SHOCK, speed_cms=500, ref=n,
                                   epoch_ms=header.start_epoch_ms + n * 60000, frames=frames))
    path = tmp_path / "S_120000.bin"
    raw.write_raw(path, header, blocks)
    _, shocks, _ = replay.run(path)
    assert [s.ref for s in shocks] == [1, 2]
    assert [s.severity for s in shocks] == [1, 3]


@needs_cxx
def test_cli_raw_commands(tmp_path, capsys):
    path, _, _ = _capture(tmp_path)
    assert cli_main(["raw", "info", "-i", str(path)]) == 0
    assert "Mitschnitt" in capsys.readouterr().out
    assert cli_main(["raw", "csv", "-i", str(path), "-o", str(tmp_path / "r.csv")]) == 0
    assert len((tmp_path / "r.csv").read_text().splitlines()) == 10 * ODR + 1
    assert cli_main(["raw", "replay", "-i", str(path), "shock=2.5",
                     "--shocks-out", str(tmp_path / "s.csv")]) == 0
    assert (tmp_path / "s.csv").exists()
    with pytest.raises(SystemExit):
        cli_main(["raw", "replay", "-i", str(path), "bogus=1"])
