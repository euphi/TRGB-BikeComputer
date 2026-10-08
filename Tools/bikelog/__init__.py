"""Offline tooling for the bike computer's binary datalog.

record.py      read (and write) the binary records (src/LogRecords.h)
csvexport.py   records -> CSV: ride data, road-quality intervals, shocks, manual labels
gpx.py         records -> GPX 1.1 with Garmin TrackPointExtension, shocks as waypoints
report.py      records -> all key figures of a session (JSON) and a Markdown report
fixtures.py    build synthetic .bin logs for testing without hardware
"""

from .record import (CURRENT_VERSION, FORMATS, LAYOUTS, LabelRecord, ReadStats, Record,
                     RoadQualityRecord, ShockEvent, UnknownLogFormat, label_at, labels_of,
                     read_file, read_stream, split)

__all__ = [
    "CURRENT_VERSION", "FORMATS", "LAYOUTS", "ReadStats", "Record", "RoadQualityRecord",
    "ShockEvent", "LabelRecord", "UnknownLogFormat", "read_file", "read_stream", "split",
    "labels_of", "label_at",
]
