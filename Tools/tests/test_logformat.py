"""Log format v2: mixed record types, and the layouts pinned to src/LogRecords.h."""

import io
import shutil
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

import pytest

from bikelog import csvexport, fixtures, gpx
from bikelog.cli import main as cli_main
from bikelog.record import (DS_DRIVE_POWER, DS_FREE_RIDE, FORMATS, IF_GPS_VALID, LAYOUTS,
                            SF_GPS_VALID, SF_WHEELBASE_MATCH, TYPE_DATA, TYPE_ROAD_QUALITY,
                            TYPE_SHOCK, TYPE_OFFSET, U16_INVALID, VERSION_OFFSET, ReadStats,
                            Record, RoadQualityRecord, ShockEvent, LabelRecord, TYPE_LABEL,
                            label_at, labels_of, RideStateRecord, TYPE_RIDESTATE, ride_state_at,
                            ride_states_of, read_file, read_stream, split, write_records)

REPO = Path(__file__).resolve().parents[2]


def _ride(seconds=400):
    return fixtures.with_road_quality(fixtures.synthetic(seconds=seconds, pause=None, tunnel=None,
                                                         no_fix_start_s=0))


def test_v2_records_are_64_bytes_with_type_and_version_bytes():
    fmt = FORMATS[2]
    for rtype, layout in fmt.layouts.items():
        assert layout.size == 64
        names = layout.names
        # position of a field = size of the struct prefix before it
        import struct
        def offset(name):
            idx = names.index(name)
            prefix = layout.fmt[0] + "".join(_codes(layout.fmt)[:idx])
            return struct.calcsize(prefix)
        assert offset("record_type") == TYPE_OFFSET
        assert offset("format_version") == VERSION_OFFSET


def _codes(fmt):
    import re
    out = []
    for num, code in re.findall(r"(\d*)([a-zA-Z])", fmt[1:]):
        out += [code] * (int(num) if num else 1)
    return out


@pytest.mark.skipif(shutil.which("g++") is None, reason="needs a host C++ compiler")
def test_layout_matches_firmware_header(tmp_path):
    """Compile src/LogRecords.h on the host, let it write one record of each
    type, and read them back. Catches any drift between firmware and reader."""
    exe = tmp_path / "dump"
    src = REPO / "test" / "native_roadquality" / "logrecords_dump.cpp"
    subprocess.run(["g++", "-std=c++17", "-I", str(REPO / "src"), str(src), "-o", str(exe)],
                   check=True)
    out = tmp_path / "dump.bin"
    subprocess.run([str(exe), str(out)], check=True)
    data, road, shocks = split(read_file(out, types=None))
    d, r, s = data[0], road[0], shocks[0]

    assert (d.timestamp, d.timestamp_ms, d.speed, d.temp, d.gradient) == (1790000000, 789, 21.5, 17.25, -3.5)
    assert (d.height, d.distance, d.hr, d.cadence) == (123.5, 4567.0, 140, 85)
    assert (d.gps_lat_e7, d.gps_lon_e7, d.gps_altitude_m, d.gps_speed_cms) == (524000000, 87000000, 95, 600)
    assert (d.gps_bearing_deg_x100, d.gps_accuracy_m_x10, d.gps_fix_age_ms, d.gps_flags) == (4500, 45, 350, 0x1F)
    assert d.road_class == 3 and d.grad_baro == -3.25 and d.grad_imu is None

    assert (r.timestamp, r.timestamp_ms, r.interval_ms, r.sample_count) == (1790000002, 12, 2003, 800)
    assert r.speed_kmh == pytest.approx(20.0, abs=0.02)
    assert (r.rms_vert_mg, r.rms_horiz_mg, r.peak_vert_max_mg, r.peak_vert_min_mg) == (234, 99, 1500, -1400)
    assert (r.peak_total_mg, r.roughness, r.road_class, r.flags) == (1800, 2.75, 3, 0x88)
    assert (r.vdv_vert, r.distance_m) == (1.25, 11.5)
    assert (r.count_over_t1, r.count_over_t2, r.events_logged, r.events_suppressed) == (7, 2, 1, 4)
    assert (r.gps_lat_e7, r.gps_lon_e7, r.gps_fix_age_ms, r.gps_accuracy_m_x10) == (524000100, 87000100, 400, 50)
    assert r.grad_imu == 4.2 and r.gps_valid

    assert (s.timestamp, s.timestamp_ms, s.duration_ms, s.peak_total_mg) == (1790000003, 456, 8, 5790)
    assert (s.peak_vert_max_mg, s.peak_vert_min_mg, s.peak_horiz_mg, s.pre_rms_mg) == (5700, -2100, 800, 90)
    assert s.speed_kmh is None
    assert (s.second_peak_mg, s.second_peak_delay_ms, s.severity, s.flags) == (2820, 188, 2, 0x0A)
    assert (s.vdv, s.samples_over_threshold, s.threshold_mg) == (0.5, 3, 3000)
    assert (s.gps_lat_e7, s.gps_lon_e7, s.gps_fix_age_ms, s.gps_accuracy_m_x10) == (524000200, 87000200, 500, 60)
    assert s.event_seq == 42 and s.wheelbase_match and s.gps_valid
    assert (s.label_surface, s.label_quality) == (5, 3)

    (lab,) = labels_of(read_file(out, types=None))
    assert (lab.timestamp, lab.timestamp_ms, lab.surface, lab.quality) == (1790000004, 321, 2, 2)
    assert (lab.reason, lab.flags, lab.prev_surface, lab.prev_quality) == (0, 0x03, 1, 1)
    assert (lab.prev_distance_m, lab.prev_duration_ms, lab.label_seq) == (1234.5, 180000, 7)
    assert (lab.gps_lat_e7, lab.gps_lon_e7, lab.gps_fix_age_ms, lab.gps_accuracy_m_x10) == (524000300, 87000300, 600, 70)
    assert lab.speed_kmh == pytest.approx(18.0, abs=0.02) and lab.gps_valid
    assert lab.surface_name == "Schotter"

    (rs,) = ride_states_of(read_file(out, types=None))
    assert (rs.timestamp, rs.timestamp_ms, rs.state, rs.prev_state) == (1790000005, 654, 4, 3)
    assert (rs.ride_mode, rs.state_seq) == (1, 9)
    assert rs.state_name == "Rollen"


def test_mixed_roundtrip_and_default_filter(tmp_path):
    records = _ride()
    path = tmp_path / "ride.bin"
    write_records(path, records)
    stats = ReadStats()
    everything = list(read_file(path, stats, types=None))
    data, road, shocks = split(everything)
    assert len(everything) == len(records)
    assert stats.by_type == {TYPE_DATA: len(data), TYPE_ROAD_QUALITY: len(road), TYPE_SHOCK: len(shocks)}
    assert len(road) == 200 and len(shocks) == 2
    # file order is kept: each shock follows the data record of its second
    idx = next(i for i, r in enumerate(everything) if isinstance(r, ShockEvent))
    assert isinstance(everything[idx - 1], Record)
    assert everything[idx].timestamp == everything[idx - 1].timestamp
    # default: ride data only, like every exporter up to v1
    assert [type(r) for r in read_file(path)] == [Record] * len(data)


def test_v1_logs_stay_readable(tmp_path):
    path = tmp_path / "old.bin"
    write_records(path, fixtures.synthetic(seconds=20, no_fix_start_s=0), version=1)
    stats = ReadStats()
    back = list(read_file(path, stats, types=None))
    assert stats.version == 1 and stats.record_size == 56 and len(back) == 20
    assert all(isinstance(r, Record) for r in back)
    assert back[0].grad_imu is None and back[0].road_class == 0


def test_unknown_record_type_is_skipped(tmp_path):
    path = tmp_path / "ride.bin"
    write_records(path, fixtures.synthetic(seconds=5, no_fix_start_s=0))
    payload = bytearray(path.read_bytes())
    future = bytearray(payload[:64])
    future[TYPE_OFFSET] = 9               # a type a newer firmware might add
    payload[64:64] = future
    stats = ReadStats()
    back = list(read_stream(io.BytesIO(bytes(payload)), stats, types=None))
    assert len(back) == 5
    assert stats.unknown_type == 1


def test_truncated_v2_tail_is_reported(tmp_path):
    path = tmp_path / "cut.bin"
    write_records(path, _ride(20))
    payload = path.read_bytes()[:-10]
    stats = ReadStats()
    back = list(read_stream(io.BytesIO(payload), stats, types=None))
    assert stats.trailing_bytes == 54
    assert len(back) == len(_ride(20)) - 1


def test_sentinels_read_as_none():
    assert RoadQualityRecord().roughness is None
    assert RoadQualityRecord().speed_kmh is None
    assert ShockEvent().speed_kmh is None
    assert Record().grad_baro is None


def test_road_quality_and_shock_csv(tmp_path):
    data, road, shocks = split(_ride())
    n = csvexport.write_road_quality(tmp_path / "rq.csv", road)
    assert n == len(road)
    lines = (tmp_path / "rq.csv").read_text(encoding="utf-8").splitlines()
    assert lines[0].split(",") == list(csvexport.ROAD_QUALITY_COLUMNS)
    n = csvexport.write_shocks(tmp_path / "shocks.csv", shocks)
    assert n == 2
    text = (tmp_path / "shocks.csv").read_text(encoding="utf-8")
    assert "Radstand_passt" in text.splitlines()[0]
    # the ride data CSV gets the extra columns only on request
    buf = io.StringIO()
    csvexport.write_stream(buf, data, with_roadq=True)
    assert buf.getvalue().splitlines()[0].endswith("Wegeklasse,Gradient_Baro,Gradient_IMU")
    buf = io.StringIO()
    csvexport.write_stream(buf, data)
    assert "Wegeklasse" not in buf.getvalue().splitlines()[0]


def _wpts(xml):
    root = ET.fromstring(xml)
    ns = {"g": gpx.GPX_NS}
    return root, root.findall("g:wpt", ns), ns


def test_shocks_become_waypoints_before_the_track():
    data, _, shocks = split(_ride())
    xml, stats = gpx.to_string(data, gpx.GpxOptions(), shocks)
    root, wpts, ns = _wpts(xml)
    assert len(wpts) == 2 and stats.shocks_written == 2
    children = [c.tag.split("}")[1] for c in root]
    assert children.index("wpt") < children.index("trk")      # GPX 1.1 element order
    assert wpts[0].find("g:name", ns).text == "Stoß 6,2 g"
    assert "Vorder- und Hinterrad" in wpts[0].find("g:desc", ns).text


def test_shock_waypoint_filters():
    data, _, shocks = split(_ride())
    shocks[0].flags &= ~SF_GPS_VALID
    _, stats = gpx.to_string(data, gpx.GpxOptions(), shocks)
    assert stats.shocks_written == 1 and stats.shocks_dropped == 1
    _, stats = gpx.to_string(data, gpx.GpxOptions(min_shock_severity=2), shocks)
    assert stats.shocks_written == 0                         # the remaining one is severity 1
    _, stats = gpx.to_string(data, gpx.GpxOptions(shocks=False), shocks)
    assert stats.shocks_written == 0 and stats.shocks_dropped == 0


def test_cli_info_and_csv_on_a_v2_log(tmp_path, capsys):
    path = tmp_path / "ride.bin"
    assert cli_main(["fixture", "synth", "-o", str(path), "--roadq"]) == 0
    assert cli_main(["info", "-i", str(path)]) == 0
    out = capsys.readouterr().out
    assert "Wegequalität:" in out and "Stöße:" in out
    assert cli_main(["csv", "-i", str(path), "-o", str(tmp_path / "d.csv"),
                     "--roadq-out", str(tmp_path / "rq.csv"),
                     "--shocks-out", str(tmp_path / "sh.csv")]) == 0
    assert (tmp_path / "rq.csv").exists() and (tmp_path / "sh.csv").exists()


def test_fixture_flags_are_consistent():
    _, road, shocks = split(_ride())
    assert all(r.flags & IF_GPS_VALID for r in road)
    assert shocks[0].flags & SF_WHEELBASE_MATCH and not shocks[1].flags & SF_WHEELBASE_MATCH
    rough = [r for r in road if r.rms_vert_mg > 300]
    assert rough and all(r.road_class >= 3 for r in rough if r.rated)
    assert LAYOUTS[2] is FORMATS[2].layouts[TYPE_DATA]
    assert RoadQualityRecord().speed_cms == U16_INVALID


def _labelled_ride():
    """The synthetic ride with two manual labels: Asphalt Q1 from t0+60 s, Schotter Q3 from t0+200 s."""
    records = _ride()
    t0 = records[0].timestamp
    labels = [LabelRecord(timestamp=t0 + 60, surface=1, quality=1, label_seq=1),
              LabelRecord(timestamp=t0 + 200, surface=2, quality=3, prev_surface=1, prev_quality=1,
                          prev_distance_m=700.0, prev_duration_ms=140000, label_seq=2)]
    out = []
    for rec in records:
        while labels and labels[0].timestamp <= rec.timestamp:
            out.append(labels.pop(0))
        out.append(rec)
    return out, t0


def test_labels_roundtrip_and_lookup(tmp_path):
    records, t0 = _labelled_ride()
    path = tmp_path / "ride.bin"
    write_records(path, records)
    stats = ReadStats()
    back = list(read_file(path, stats, types=None))
    labels = labels_of(back)
    assert stats.by_type[TYPE_LABEL] == 2 and stats.unknown_type == 0
    assert [(lab.surface, lab.quality) for lab in labels] == [(1, 1), (2, 3)]
    data, road, shocks = split(back)                # labels stay out of the ride data
    assert all(type(r) is Record for r in data)
    assert label_at(labels, t0 + 10) == (0, 0)
    assert label_at(labels, t0 + 60) == (1, 1)
    assert label_at(labels, t0 + 199.5) == (1, 1)
    assert label_at(labels, t0 + 1000) == (2, 3)
    # the default filter (ride data only) is unchanged
    assert all(type(r) is Record for r in read_file(path))


def _ride_states(t0):
    """Two ride-state changes: FreeRide from t0+30 s, Ride (rideMode on) from t0+90 s."""
    return [RideStateRecord(timestamp=t0 + 30, state=DS_FREE_RIDE, state_seq=1),
            RideStateRecord(timestamp=t0 + 90, state=DS_DRIVE_POWER, prev_state=DS_FREE_RIDE,
                            ride_mode=1, state_seq=2)]


def test_ride_states_roundtrip_and_lookup(tmp_path):
    records = _ride()
    t0 = records[0].timestamp
    states = _ride_states(t0)
    out = []
    for rec in records:
        while states and states[0].timestamp <= rec.timestamp:
            out.append(states.pop(0))
        out.append(rec)
    path = tmp_path / "ride.bin"
    write_records(path, out)
    stats = ReadStats()
    back = list(read_file(path, stats, types=None))
    states = ride_states_of(back)
    assert stats.by_type[TYPE_RIDESTATE] == 2 and stats.unknown_type == 0
    assert [(s.state, s.ride_mode) for s in states] == [(DS_FREE_RIDE, 0), (DS_DRIVE_POWER, 1)]
    assert states[1].state_name == "Fahrt"
    data, road, shocks = split(back)                # ride states stay out of the ride data
    assert all(type(r) is Record for r in data)
    assert ride_state_at(states, t0 + 10) is None
    assert ride_state_at(states, t0 + 30) == DS_FREE_RIDE
    assert ride_state_at(states, t0 + 89.5) == DS_FREE_RIDE
    assert ride_state_at(states, t0 + 1000) == DS_DRIVE_POWER
    # the default filter (ride data only) is unchanged
    assert all(type(r) is Record for r in read_file(path))


def test_labels_in_csv_and_info(tmp_path, capsys):
    records, t0 = _labelled_ride()
    path = tmp_path / "ride.bin"
    write_records(path, records)
    rq_csv, lab_csv = tmp_path / "rq.csv", tmp_path / "labels.csv"
    assert cli_main(["csv", "-i", str(path), "-o", str(tmp_path / "d.csv"),
                     "--roadq-out", str(rq_csv), "--labels-out", str(lab_csv)]) == 0
    rows = rq_csv.read_text(encoding="utf-8").splitlines()
    assert rows[0].endswith("Untergrund,Qualitaet_manuell")
    assert rows[1].endswith(",,")                  # before the first label
    assert rows[-1].endswith(",Schotter,3")
    assert any(r.endswith(",Asphalt,1") for r in rows)
    lab_rows = lab_csv.read_text(encoding="utf-8").splitlines()
    assert len(lab_rows) == 3 and "Schotter" in lab_rows[2] and "Asphalt" in lab_rows[2]
    capsys.readouterr()
    assert cli_main(["info", "-i", str(path)]) == 0
    out = capsys.readouterr().out
    assert "Labels:       2 Datensätze, 2 Wechsel" in out
    assert "Asphalt   Q1" in out and "Schotter  Q3" in out
