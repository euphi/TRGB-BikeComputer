"""Offline tooling for the bike computer's binary datalog.

record.py      read (and write) the binary records (src/LogRecords.h)
csvexport.py   records -> CSV: ride data, road-quality intervals, shocks
gpx.py         records -> GPX 1.1 with Garmin TrackPointExtension, shocks as waypoints
fixtures.py    build synthetic .bin logs for testing without hardware
"""

from .record import (CURRENT_VERSION, FORMATS, LAYOUTS, ReadStats, Record,
                     RoadQualityRecord, ShockEvent, UnknownLogFormat, read_file,
                     read_stream, split)

__all__ = [
    "CURRENT_VERSION", "FORMATS", "LAYOUTS", "ReadStats", "Record", "RoadQualityRecord",
    "ShockEvent", "UnknownLogFormat", "read_file", "read_stream", "split",
]
