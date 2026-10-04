"""Rides recorded elsewhere, imported as GPX: Garmin/Strava/Komoot exports, earlier races.

An imported ride becomes a session of the pseudo device IMPORT_DEVICE, filed like the
bike computer's own (``import/<day>/L_<hhmmss>.bin`` from the ride's local start time),
with the original next to it as ``G_<hhmmss>.gpx``. From there it is a ride like any
other: report, training load, climbs, a past participation of a goal. Importing the
same file again replaces the session instead of adding a second one.

Not exported (no GPX in Tours/, so neither Nextcloud nor Komoot): the original is
already wherever it came from.
"""

from __future__ import annotations

import datetime
import os
import tempfile

from bikelog import gpximport
from bikelog.record import write_records

from . import sdlayout
from .storage import Session, Storage

IMPORT_DEVICE = "import"


def import_gpx(store: Storage, data: bytes, bike_id: str | None = None) -> Session:
    """Store a recorded ride from a GPX. Raises gpximport.NotARide (or an XML error)."""
    records = gpximport.records_from_gpx(data)
    start = datetime.datetime.fromtimestamp(records[0].timestamp)       # local, like the SD card
    day, stem = start.strftime("%Y%m%d"), start.strftime("%H%M%S")
    fd, tmp = tempfile.mkstemp(suffix=".bin")
    os.close(fd)
    try:
        write_records(tmp, records)
        with open(tmp, "rb") as fh:
            payload = fh.read()
    finally:
        os.unlink(tmp)
    store.put_file(IMPORT_DEVICE, sdlayout.parse_path(f"{day}/G_{stem}.gpx"), data, source="import")
    result = store.put_file(IMPORT_DEVICE, sdlayout.parse_path(f"{day}/L_{stem}.bin"), payload,
                            source="import")
    if bike_id is not None:
        store.set_bike(result.session.id, bike_id or None)
    return store.get(result.session.id)
