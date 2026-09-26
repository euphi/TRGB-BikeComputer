"""The binary layout must stay nailed to the firmware struct -- these tests
are the thing that catches a silent desync after a firmware change."""

import io
import struct

import pytest

from bikelog import fixtures
from bikelog.record import (CURRENT_VERSION, LAYOUTS, ReadStats, Record, UnknownLogFormat,
                            read_stream, write_records)


def test_v1_record_is_56_bytes():
    # The historic v1 layout (BCLogger::LogData before format v2) -- still
    # read, never written by the firmware any more. v2: test_logformat.py.
    assert LAYOUTS[1].size == 56


def test_version_byte_sits_where_the_reader_looks():
    raw = struct.pack(LAYOUTS[1].fmt, *[1] * len(LAYOUTS[1].names))
    assert raw[LAYOUTS[1].version_offset] == 1


def test_roundtrip_preserves_every_field(tmp_path):
    original = fixtures.synthetic(seconds=30, no_fix_start_s=5)
    path = tmp_path / "ride.bin"
    write_records(path, original)
    stats = ReadStats()
    with open(path, "rb") as fh:
        back = list(read_stream(fh, stats))
    assert stats.version == CURRENT_VERSION
    assert len(back) == len(original)
    for before, after in zip(original, back):
        assert after.timestamp == before.timestamp
        assert after.hr == before.hr
        assert after.gps_lat_e7 == before.gps_lat_e7
        assert after.gps_flags == before.gps_flags
        assert after.speed == pytest.approx(before.speed, rel=1e-6)


def test_truncated_tail_is_reported_not_raised(tmp_path):
    path = tmp_path / "cut.bin"
    write_records(path, fixtures.synthetic(seconds=10, no_fix_start_s=0))
    payload = path.read_bytes() + b"\x01\x02\x03"
    stats = ReadStats()
    records = list(read_stream(io.BytesIO(payload), stats))
    assert len(records) == 10
    assert stats.trailing_bytes == 3


def test_unknown_version_is_rejected():
    raw = bytearray(struct.pack(LAYOUTS[1].fmt, *[0] * len(LAYOUTS[1].names)))
    raw[LAYOUTS[1].version_offset] = 99
    with pytest.raises(UnknownLogFormat):
        list(read_stream(io.BytesIO(bytes(raw))))


def test_empty_file_yields_nothing():
    assert list(read_stream(io.BytesIO(b""))) == []


def test_gps_flags_gate_their_values():
    # A cleared flag must read as "no value", not as a measurement of zero:
    # 0 is a legal bearing (north) and a legal altitude (sea level).
    rec = Record(gps_flags=0, gps_bearing_deg_x100=0, gps_altitude_m=0)
    assert rec.gps_bearing_deg is None
    assert rec.gps_speed_ms is None
    assert rec.gps_accuracy_m is None


def test_speed_unit_conversion():
    assert Record(speed=36.0).speed_ms == pytest.approx(10.0)
