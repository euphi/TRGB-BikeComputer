"""Ride storage: the raw .bin on disk, a SQLite row as index.

Raw uploads are kept verbatim and never rewritten. Everything else -- GPX,
CSV, later the heatmap -- is derived on demand, so improving the exporter
improves every past ride instead of only future ones.

The ride id is the content hash. That makes upload idempotent, which matters
more here than it sounds: the uploader is an ESP32 on flaky WiFi that will
be interrupted mid-transfer and will retry files it already sent. Re-sending
a known file is a 200 with the same id, not a duplicate and not an error, so
the firmware needs no reliable "already uploaded" bookkeeping of its own.
"""

from __future__ import annotations

import datetime
import hashlib
import sqlite3
from dataclasses import asdict, dataclass
from pathlib import Path

from bikelog.record import ReadStats, UnknownLogFormat, read_stream

SCHEMA = """
CREATE TABLE IF NOT EXISTS rides (
    id              TEXT PRIMARY KEY,
    filename        TEXT NOT NULL,
    device          TEXT,
    size_bytes      INTEGER NOT NULL,
    sha256          TEXT NOT NULL,
    uploaded_at     TEXT NOT NULL,
    uploaded_by     TEXT,
    format_version  INTEGER,
    record_count    INTEGER,
    trailing_bytes  INTEGER,
    first_time      INTEGER,
    last_time       INTEGER,
    distance_m      REAL,
    gps_points      INTEGER,
    clock_unset     INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS rides_first_time ON rides (first_time);
"""

#: Timestamps below this mean the ESP32 clock was never set by NTP -- see
#: bikelog.gpx.MIN_PLAUSIBLE_YEAR. Flagged at upload so the ride can be shown
#: as "needs a manual time offset" instead of silently landing in 1970.
MIN_PLAUSIBLE_TIMESTAMP = int(
    datetime.datetime(2020, 1, 1, tzinfo=datetime.timezone.utc).timestamp())


@dataclass
class Ride:
    id: str
    filename: str
    device: str | None
    size_bytes: int
    sha256: str
    uploaded_at: str
    uploaded_by: str | None
    format_version: int | None
    record_count: int | None
    trailing_bytes: int | None
    first_time: int | None
    last_time: int | None
    distance_m: float | None
    gps_points: int | None
    clock_unset: int

    def as_dict(self) -> dict:
        data = asdict(self)
        data["clock_unset"] = bool(self.clock_unset)
        data["duration_s"] = (
            self.last_time - self.first_time
            if self.first_time and self.last_time else None)
        return data


class DuplicateRide(Exception):
    """Content already stored; carries the existing ride."""

    def __init__(self, ride: Ride):
        super().__init__(ride.id)
        self.ride = ride


class InvalidLog(Exception):
    """Uploaded bytes are not a log this build can read."""


class Storage:
    def __init__(self, settings):
        self.settings = settings
        settings.ensure_dirs()
        self._db = sqlite3.connect(settings.db_path, check_same_thread=False)
        self._db.row_factory = sqlite3.Row
        self._db.executescript(SCHEMA)
        self._db.commit()

    def close(self) -> None:
        self._db.close()

    # --- paths ---------------------------------------------------------

    def path_for(self, ride_id: str) -> Path:
        return self.settings.rides_dir / f"{ride_id}.bin"

    # --- queries -------------------------------------------------------

    def get(self, ride_id: str) -> Ride | None:
        row = self._db.execute("SELECT * FROM rides WHERE id = ?", (ride_id,)).fetchone()
        return Ride(**dict(row)) if row else None

    def list(self, limit: int = 100, offset: int = 0) -> list[Ride]:
        rows = self._db.execute(
            "SELECT * FROM rides ORDER BY COALESCE(first_time, 0) DESC, uploaded_at DESC "
            "LIMIT ? OFFSET ?", (limit, offset)).fetchall()
        return [Ride(**dict(row)) for row in rows]

    def count(self) -> int:
        return self._db.execute("SELECT COUNT(*) FROM rides").fetchone()[0]

    def records(self, ride_id: str, stats: ReadStats | None = None):
        """Parsed records of a stored ride -- the input for every export."""
        with open(self.path_for(ride_id), "rb") as fh:
            yield from read_stream(fh, stats)

    # --- mutations -----------------------------------------------------

    def add(self, payload: bytes, filename: str, device: str | None = None,
            uploaded_by: str | None = None) -> Ride:
        """Store an uploaded log. Raises DuplicateRide if it is already here."""
        digest = hashlib.sha256(payload).hexdigest()
        ride_id = digest[:16]
        existing = self.get(ride_id)
        if existing:
            raise DuplicateRide(existing)

        summary = _summarise(payload)
        path = self.path_for(ride_id)
        # Write to a temp name first: an interrupted write must not leave a
        # half file under an id the index claims is complete.
        tmp = path.with_suffix(".bin.part")
        tmp.write_bytes(payload)
        tmp.replace(path)

        ride = Ride(
            id=ride_id,
            filename=filename,
            device=device,
            size_bytes=len(payload),
            sha256=digest,
            uploaded_at=datetime.datetime.now(datetime.timezone.utc).isoformat(
                timespec="seconds"),
            uploaded_by=uploaded_by,
            **summary,
        )
        self._db.execute(
            "INSERT INTO rides VALUES (:id, :filename, :device, :size_bytes, :sha256, "
            ":uploaded_at, :uploaded_by, :format_version, :record_count, :trailing_bytes, "
            ":first_time, :last_time, :distance_m, :gps_points, :clock_unset)",
            asdict(ride))
        self._db.commit()
        return ride

    def delete(self, ride_id: str) -> bool:
        ride = self.get(ride_id)
        if ride is None:
            return False
        self.path_for(ride_id).unlink(missing_ok=True)
        self._db.execute("DELETE FROM rides WHERE id = ?", (ride_id,))
        self._db.commit()
        return True


def _summarise(payload: bytes) -> dict:
    """Parse an upload once, at upload time, to fill the index.

    A file that parses to zero records is rejected here rather than stored:
    the alternative is an index full of entries that every later export
    chokes on.
    """
    import io

    stats = ReadStats()
    try:
        records = list(read_stream(io.BytesIO(payload), stats))
    except UnknownLogFormat as exc:
        raise InvalidLog(str(exc)) from exc
    if not records:
        raise InvalidLog("no complete record in upload (%d byte)" % len(payload))

    gps_points = sum(1 for rec in records if rec.gps_valid)
    first, last = records[0], records[-1]
    return {
        "format_version": stats.version,
        "record_count": len(records),
        "trailing_bytes": stats.trailing_bytes,
        "first_time": first.timestamp,
        "last_time": last.timestamp,
        "distance_m": last.distance - first.distance,
        "gps_points": gps_points,
        "clock_unset": int(first.timestamp < MIN_PLAUSIBLE_TIMESTAMP),
    }
