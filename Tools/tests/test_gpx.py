"""What ends up in the GPX -- and more importantly, what does not."""

import datetime
import math
import xml.etree.ElementTree as ET

from bikelog import fixtures, gpx, record
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
    # GPX 1.1 fixes the order inside <trk>: name, cmt, desc, src, link, number, type.
    children = [child.tag.split("}")[1] for child in root.find("gpx:trk", NS)]
    assert children[:4] == ["name", "desc", "src", "type"]
    meta = [child.tag.split("}")[1] for child in root.find("gpx:metadata", NS)]
    assert meta == ["name", "desc", "time", "keywords", "bounds", "extensions"]


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


# --- real_distance_m: the "was this an actual ride" figure ------------

def _jittering_in_place(n: int, radius_m: float = 3.0):
    """n records at one spot, the phone's fix wandering within radius_m of it
    -- what an idle bike computer with GPS on sees, never what a ride is."""
    records = fixtures.synthetic(seconds=1, no_fix_start_s=0)
    base = records[0]
    out = []
    for i in range(n):
        rec = fixtures.synthetic(seconds=1, no_fix_start_s=0)[0]
        rec.timestamp = base.timestamp + i
        angle = i * 0.7
        rec.gps_lat_e7 = base.gps_lat_e7 + round(radius_m * math.cos(angle) / 111320.0 * 1e7)
        rec.gps_lon_e7 = base.gps_lon_e7 + round(radius_m * math.sin(angle) / 111320.0 * 1e7)
        out.append(rec)
    return out


def test_real_distance_ignores_gps_jitter_around_one_spot():
    _, stats = _tree(_jittering_in_place(30))
    assert stats.written == 30
    assert stats.real_distance_m == 0.0


def test_real_distance_counts_actual_movement():
    records = fixtures.synthetic(seconds=60, no_fix_start_s=0, pause=None, tunnel=None)
    _, stats = _tree(records)
    assert stats.written == 60
    assert stats.real_distance_m > 350                # ~24 km/h for 60 s


def test_real_distance_needs_steps_past_the_jitter_radius():
    moving = _tree(fixtures.synthetic(seconds=60, no_fix_start_s=0, pause=None, tunnel=None))[1]
    tighter = _tree(fixtures.synthetic(seconds=60, no_fix_start_s=0, pause=None, tunnel=None),
                    jitter_radius_m=1000.0)[1]
    assert tighter.real_distance_m == 0.0
    assert moving.real_distance_m > tighter.real_distance_m


# --- the rich export --------------------------------------------------

BC = {"gpx": gpx.GPX_NS, "tpx": gpx.TPX_NS, "bc": gpx.BC_NS}


def _rich_ride():
    """Synthetic ride with road quality, shocks and two label changes."""
    from bikelog.record import LABEL_CHANGE, LF_GPS_VALID, LabelRecord
    records = fixtures.synthetic(no_fix_start_s=0)
    everything = fixtures.with_road_quality(records)
    for second, surface, quality in ((10, 1, 1), (200, 2, 3)):
        rec = records[second]
        everything.insert(everything.index(rec) + 1, LabelRecord(
            timestamp=rec.timestamp, surface=surface, quality=quality, reason=LABEL_CHANGE,
            flags=LF_GPS_VALID, prev_surface=1 if second == 200 else 0,
            prev_quality=1 if second == 200 else 0, prev_distance_m=1234.0,
            gps_lat_e7=rec.gps_lat_e7, gps_lon_e7=rec.gps_lon_e7,
            gps_fix_age_ms=rec.gps_fix_age_ms))
    return everything


def test_rich_trackpoints_carry_road_quality_and_label():
    xml, stats = gpx.from_records(_rich_ride(), gpx.GpxOptions(device="gravel"))
    root = ET.fromstring(xml)
    points = root.findall(".//gpx:trkpt", BC)
    own = [p.find("gpx:extensions/bc:TrackPoint", BC) for p in points]
    assert all(o is not None for o in own)
    assert {o.findtext("bc:label", namespaces=BC) for o in own} >= {"Asphalt Q1", "Schotter Q3"}
    classes = {o.findtext("bc:roadClass", namespaces=BC) for o in own} - {None}
    assert len(classes) >= 2                 # smooth road and cobbles
    assert any(o.find("bc:roughness", BC) is not None for o in own)
    assert all(o.find("bc:dist", BC) is not None for o in own)
    # TPX v2 still first in <extensions>, so importers that only look there find it.
    first_ext = points[50].find("gpx:extensions", BC)[0]
    assert first_ext.tag == f"{{{gpx.TPX_NS}}}TrackPointExtension"


def test_rich_waypoints_for_shocks_and_labels():
    xml, stats = gpx.from_records(_rich_ride())
    root = ET.fromstring(xml)
    assert stats.shocks_written == 1 and stats.labels_written == 2   # 2nd fixture shock lies past the ride
    types = [w.findtext("gpx:type", namespaces=BC) for w in root.findall("gpx:wpt", BC)]
    assert types.count("label") == 2
    label = [w for w in root.findall("gpx:wpt", BC) if w.findtext("gpx:type", namespaces=BC) == "label"][1]
    assert label.findtext("gpx:name", namespaces=BC) == "Schotter Q3"
    assert "vorher Asphalt Q1" in label.findtext("gpx:desc", namespaces=BC)
    shock = root.find("gpx:wpt/gpx:extensions/bc:Shock", BC)
    assert shock.findtext("bc:severity", namespaces=BC) == "2"


def test_rich_metadata_summary():
    summary = {"v": 1, "src": "ntp", "shocks_supp": 4}
    xml, _ = gpx.from_records(_rich_ride(), gpx.GpxOptions(device="gravel", link="http://x/1"),
                              device_summary=summary)
    root = ET.fromstring(xml)
    desc = root.findtext("gpx:metadata/gpx:desc", namespaces=BC)
    assert " km" in desc and "1 Stoß (0/1/0)" in desc and "Labels:" in desc and "max. " in desc
    ride = root.find("gpx:metadata/gpx:extensions/bc:Ride", BC)
    assert ride.findtext("bc:device", namespaces=BC) == "gravel"
    assert float(ride.findtext("bc:distance", namespaces=BC)) > 0
    assert ride.find("bc:roadClassDistance", BC) is not None
    assert ride.findtext("bc:deviceSummary/bc:shocks_supp", namespaces=BC) == "4"
    assert root.find("gpx:metadata/gpx:link", BC).get("href") == "http://x/1"
    assert root.findtext("gpx:trk/gpx:src", namespaces=BC) == "TRGB-BikeComputer (gravel)"


def _ride_with_states():
    """Synthetic ride with two ride-state changes: FreeRide at list index 30
    (pre-pause segment, second 30) and Ride/DriveCoasting at list index 200
    (post-pause segment, second 380 -- the pause at 120..300 s removes those
    records outright, so list index and second diverge after it). Both away
    from the pause-caused time gap, so they split segments the time-gap check
    alone would not."""
    from bikelog.record import DS_DRIVE_COASTING, DS_FREE_RIDE, RideStateRecord
    records = fixtures.synthetic(no_fix_start_s=0)
    everything = []
    for i, rec in enumerate(records):
        if i == 30:
            everything.append(RideStateRecord(timestamp=rec.timestamp, state=DS_FREE_RIDE,
                                              state_seq=1))
        if i == 200:
            everything.append(RideStateRecord(timestamp=rec.timestamp, state=DS_DRIVE_COASTING,
                                              prev_state=DS_FREE_RIDE, ride_mode=1, state_seq=2))
        everything.append(rec)
    return everything


def test_ride_state_changes_split_segments_and_tag_them():
    xml, stats = gpx.from_records(_ride_with_states(), gpx.GpxOptions(ride_states=True))
    root = ET.fromstring(xml)
    segs = root.findall(".//gpx:trkseg", BC)
    # 2 segments from the pause (see test_pause_splits_the_track_into_segments)
    # plus 2 more from the ride-state changes, each inside one of those.
    assert stats.segments == len(segs) == 4
    names = [s.findtext("gpx:extensions/bc:RideState/bc:name", namespaces=BC) for s in segs]
    # seg 1: no state yet: seg 2 (state change): FreeRide; seg 3 (pause split, state
    # unchanged since the change at index 200 comes later): still FreeRide; seg 4: Rollen.
    assert names == [None, "FreeRide", "FreeRide", "Rollen"]


def test_ride_state_split_is_off_by_default():
    xml, stats = gpx.from_records(_ride_with_states())
    root = ET.fromstring(xml)
    assert stats.segments == 2                       # back to the pause-only count
    assert root.find(f".//{{{gpx.BC_NS}}}RideState") is None


def test_plain_export_has_no_own_namespace():
    xml, _ = gpx.from_records(_rich_ride(), gpx.GpxOptions(rich=False))
    assert gpx.BC_NS not in xml
    assert "TrackPointExtension" in xml


def test_hr_255_means_no_reading():
    rec = fixtures.synthetic(seconds=1, no_fix_start_s=0)[0]
    rec.hr = 255
    root, _ = _tree([rec])
    assert root.find(".//tpx:hr", NS) is None


def test_ridestats():
    from bikelog import ridestats
    from bikelog.record import split
    records, road, shocks = split(fixtures.with_road_quality(fixtures.synthetic()))
    st = ridestats.compute(records, road, shocks)
    assert st.distance_m > 1000
    assert 0 < st.moving_s <= st.duration_s
    assert st.avg_moving_kmh and st.max_speed_kmh >= st.avg_moving_kmh
    assert st.shocks == {2: 1}
    assert st.avg_hr and st.max_hr >= st.avg_hr


def test_simulated_ride_is_marked():
    records = fixtures.synthetic()
    for rec in records[100:200]:
        rec.gps_flags |= record.LOG_SIMULATED
    root, _ = _tree(records)
    assert root.find("gpx:metadata/gpx:name", NS).text.startswith("[SIM] ")
    assert "simuliert" in root.find("gpx:metadata/gpx:keywords", NS).text
    marked = root.findall(".//gpx:trkpt/gpx:extensions/bc:TrackPoint/bc:simulated", {**NS, "bc": gpx.BC_NS})
    assert 0 < len(marked) <= 100


def test_real_ride_is_not_marked():
    root, _ = _tree(fixtures.synthetic())
    assert not root.find("gpx:metadata/gpx:name", NS).text.startswith("[SIM]")
    assert not root.findall(".//bc:simulated", {"bc": gpx.BC_NS})
