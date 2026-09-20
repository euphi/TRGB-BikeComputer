"""What ends up in the GPX -- and more importantly, what does not."""

import datetime
import xml.etree.ElementTree as ET

from bikelog import fixtures, gpx
from bikelog.record import Record

NS = {"gpx": gpx.GPX_NS, "tpx": gpx.TPX_NS}


def _tree(records, **kwargs):
    xml, stats = gpx.to_string(records, gpx.GpxOptions(**kwargs))
    return ET.fromstring(xml), stats


def test_full_synthetic_ride_drops_exactly_the_bad_records():
    records = fixtures.synthetic(unset_clock_records=3)
    root, stats = _tree(records)
    assert stats.dropped_no_fix == 20        # GPS still searching at the start
    # The tunnel is 30 s long, but its fix age starts at 1 s and only crosses
    # the 5 s threshold after five records -- those five are still fresh
    # enough to keep, which is the behaviour we want.
    assert stats.dropped_stale == 25
    assert stats.dropped_bad_time == 3       # clock not yet set by NTP
    assert stats.written == stats.total - 48
    assert len(root.findall(".//gpx:trkpt", NS)) == stats.written


def test_pause_splits_the_track_into_segments():
    root, stats = _tree(fixtures.synthetic())
    assert stats.segments == 2
    assert len(root.findall(".//gpx:trkseg", NS)) == 2


def test_segment_split_can_be_disabled():
    _, stats = _tree(fixtures.synthetic(), segment_gap_s=0)
    assert stats.segments == 1


def test_stale_fixes_kept_when_the_check_is_off():
    _, with_check = _tree(fixtures.synthetic())
    _, without = _tree(fixtures.synthetic(), max_fix_age_ms=0)
    assert without.dropped_stale == 0
    assert without.written > with_check.written


def test_null_island_is_not_a_position():
    rec = fixtures.synthetic(seconds=1, no_fix_start_s=0)[0]
    rec.gps_lat_e7 = 0
    rec.gps_lon_e7 = 0
    _, stats = _tree([rec])
    assert stats.dropped_null_island == 1
    assert stats.written == 0


def test_accuracy_filter_is_opt_in():
    rec = fixtures.synthetic(seconds=1, no_fix_start_s=0)[0]
    rec.gps_accuracy_m_x10 = 500                      # 50 m
    assert _tree([rec])[1].written == 1               # off by default
    assert _tree([rec], max_accuracy_m=20.0)[1].dropped_inaccurate == 1


def test_duplicate_timestamps_collapse():
    records = fixtures.synthetic(seconds=3, no_fix_start_s=0)
    for rec in records:
        rec.timestamp = records[0].timestamp
    _, stats = _tree(records)
    assert stats.written == 1
    assert stats.dropped_duplicate_time == 2


def test_sensor_values_land_in_the_garmin_extension():
    root, _ = _tree(fixtures.synthetic(seconds=30, no_fix_start_s=0))
    point = root.find(".//gpx:trkpt", NS)
    tpx = point.find("gpx:extensions/tpx:TrackPointExtension", NS)
    assert tpx is not None
    assert tpx.find("tpx:hr", NS).text.isdigit()
    assert tpx.find("tpx:cad", NS).text.isdigit()
    assert float(tpx.find("tpx:atemp", NS).text) > 0


def test_zero_sensor_values_are_omitted_not_written_as_zero():
    rec = fixtures.synthetic(seconds=1, no_fix_start_s=0)[0]
    rec.hr = 0
    rec.cadence = 0
    root, _ = _tree([rec])
    tpx = root.find(".//gpx:extensions/tpx:TrackPointExtension", NS)
    assert tpx.find("tpx:hr", NS) is None
    assert tpx.find("tpx:cad", NS) is None


def test_elevation_source_selection():
    rec = fixtures.synthetic(seconds=1, no_fix_start_s=0)[0]
    rec.height = 123.5
    rec.gps_altitude_m = 99

    def ele(source):
        root, _ = _tree([rec], ele_source=source)
        return root.find(".//gpx:trkpt/gpx:ele", NS).text

    assert ele("baro") == "123.5"
    assert ele("gps") == "99.0"
    assert ele("auto") == "123.5"          # barometer wins when plausible


def test_auto_elevation_falls_back_to_gps_without_a_barometer():
    rec = fixtures.synthetic(seconds=1, no_fix_start_s=0)[0]
    rec.height = 0.0                       # no I2C sensor on this build
    rec.gps_altitude_m = 77
    root, _ = _tree([rec], ele_source="auto")
    assert root.find(".//gpx:trkpt/gpx:ele", NS).text == "77.0"


def test_track_name_and_metadata_time():
    records = fixtures.synthetic(seconds=30, no_fix_start_s=0)
    root, stats = _tree(records)
    assert root.find("gpx:metadata/gpx:name", NS).text.startswith("Fahrt ")
    assert root.find("gpx:metadata/gpx:time", NS).text.endswith("Z")
    # <name> must come before <type> inside <trk> for a schema-valid GPX.
    children = [child.tag.split("}")[1] for child in root.find("gpx:trk", NS)]
    assert children[:2] == ["name", "type"]


def test_gpx_roundtrips_through_the_fixture_builder(tmp_path):
    original = fixtures.synthetic(seconds=60, no_fix_start_s=0, tunnel=None,
                                  pause=None)
    gpx_path = tmp_path / "ride.gpx"
    gpx.write(gpx_path, original)
    rebuilt = fixtures.from_gpx(gpx_path)
    assert len(rebuilt) == len(original)
    for before, after in zip(original, rebuilt):
        assert after.timestamp == before.timestamp
        assert abs(after.latitude - before.latitude) < 1e-6
        assert after.hr == before.hr


def test_empty_when_nothing_has_a_fix():
    records = [Record(timestamp=int(datetime.datetime(2026, 5, 1).timestamp()))]
    root, stats = _tree(records)
    assert stats.written == 0
    assert root.find(".//gpx:trkpt", NS) is None
