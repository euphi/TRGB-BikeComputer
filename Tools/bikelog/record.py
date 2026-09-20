"""Reader for the binary datalog the bike computer writes to its SD card.

Authoritative source of the format: ``BCLogger::LogData`` in src/BCLogger.h.
A log file is a bare sequence of fixed-size records -- no file header, no
index, no terminator; a file that was cut short (card pulled mid-ride) simply
ends in a partial record.

Every record carries the layout version the firmware wrote it with
(``BCLogger::LOG_DATA_FORMAT_VERSION``), so this module dispatches on that
byte via the LAYOUTS registry rather than assuming a single format. Reading
older logs later means adding one entry there, not rewriting the reader.
"""

from __future__ import annotations

import datetime
import struct
from dataclasses import dataclass, fields
from typing import BinaryIO, Iterator

# Bits of LogData::gpsFlags, mirroring BCLogger::LogDataGpsFlags.
# LOG_GPS_VALID means "a POSITION_UPDATE was received at all" -- NOT that the
# fix is fresh. Freshness is gps_fix_age_ms, see SGpsFix in BikeGpsProtocol.h.
LOG_GPS_VALID = 0x01
LOG_GPS_HAS_ALTITUDE = 0x02
LOG_GPS_HAS_SPEED = 0x04
LOG_GPS_HAS_BEARING = 0x08
LOG_GPS_HAS_ACCURACY = 0x10


@dataclass
class Record:
    """One logged sample. Units are the firmware's, converted nowhere.

    speed     km/h   (wheel sensor; Distance.cpp multiplies by 3.6)
    temp      °C
    gradient  %
    height    m      (barometric, from the I2C sensor)
    distance  m      (trip distance since start)
    hr        bpm    (0 = no sensor / no reading)
    cadence   rpm    (0 = no sensor / no reading)
    """

    timestamp: int = 0
    speed: float = 0.0
    temp: float = 0.0
    gradient: float = 0.0
    height: float = 0.0
    distance: float = 0.0
    hr: int = 0
    cadence: int = 0
    gps_flags: int = 0
    format_version: int = 0
    gps_lat_e7: int = 0
    gps_lon_e7: int = 0
    gps_altitude_m: int = 0
    gps_speed_cms: int = 0
    gps_bearing_deg_x100: int = 0
    gps_accuracy_m_x10: int = 0
    gps_fix_age_ms: int = 0

    # --- GPS convenience accessors -------------------------------------
    # Each "has" bit gates its value field: the firmware writes 0 when the
    # phone's fix did not carry that value, which is indistinguishable from a
    # real 0 (bearing due north, sea level) without checking the flag.

    @property
    def gps_valid(self) -> bool:
        return bool(self.gps_flags & LOG_GPS_VALID)

    @property
    def has_gps_altitude(self) -> bool:
        return bool(self.gps_flags & LOG_GPS_HAS_ALTITUDE)

    @property
    def has_gps_speed(self) -> bool:
        return bool(self.gps_flags & LOG_GPS_HAS_SPEED)

    @property
    def has_gps_bearing(self) -> bool:
        return bool(self.gps_flags & LOG_GPS_HAS_BEARING)

    @property
    def has_gps_accuracy(self) -> bool:
        return bool(self.gps_flags & LOG_GPS_HAS_ACCURACY)

    @property
    def latitude(self) -> float:
        return self.gps_lat_e7 / 1e7

    @property
    def longitude(self) -> float:
        return self.gps_lon_e7 / 1e7

    @property
    def gps_speed_ms(self) -> float | None:
        return self.gps_speed_cms / 100.0 if self.has_gps_speed else None

    @property
    def gps_bearing_deg(self) -> float | None:
        return self.gps_bearing_deg_x100 / 100.0 if self.has_gps_bearing else None

    @property
    def gps_accuracy_m(self) -> float | None:
        return self.gps_accuracy_m_x10 / 10.0 if self.has_gps_accuracy else None

    @property
    def speed_ms(self) -> float:
        """Wheel-sensor speed in m/s (the log stores km/h)."""
        return self.speed / 3.6

    def utc(self) -> datetime.datetime:
        return datetime.datetime.fromtimestamp(self.timestamp, datetime.timezone.utc)


_RECORD_FIELDS = {f.name for f in fields(Record)}


class Layout:
    """One binary record layout, tied to a format version.

    ``fmt`` is a little-endian, explicitly packed struct format ('<'), which
    is what the ESP32-S3 produces: the firmware struct has no padding gaps,
    a property BCLogger.h pins down with a static_assert on sizeof(LogData).

    ``version_offset`` has to be stated separately rather than derived from
    the field list, because the version byte must be located before the
    record can be unpacked at all.
    """

    def __init__(self, fmt: str, names: tuple[str, ...], version_offset: int):
        unknown = set(names) - _RECORD_FIELDS
        if unknown:
            raise ValueError("Layout names not present on Record: %s" % sorted(unknown))
        self.fmt = fmt
        self.names = names
        self.version_offset = version_offset
        self.size = struct.calcsize(fmt)

    def build(self, raw: bytes) -> Record:
        """Unpack one record. Fields the layout lacks keep their default."""
        return Record(**dict(zip(self.names, struct.unpack(self.fmt, raw))))


LAYOUTS: dict[int, Layout] = {
    # v1 -- the GPS-era record: 8-byte time_t, five floats, four single bytes
    # (hr/cadence/gpsFlags/formatVersion), then the GPS block. 56 bytes.
    1: Layout(
        "<q5f4B3iIHHI",
        (
            "timestamp", "speed", "temp", "gradient", "height", "distance",
            "hr", "cadence", "gps_flags", "format_version",
            "gps_lat_e7", "gps_lon_e7", "gps_altitude_m",
            "gps_speed_cms", "gps_bearing_deg_x100", "gps_accuracy_m_x10",
            "gps_fix_age_ms",
        ),
        version_offset=31,
    ),
}

MAX_VERSION_OFFSET = max(layout.version_offset for layout in LAYOUTS.values())


class UnknownLogFormat(Exception):
    """The version byte matches no layout in LAYOUTS."""


def detect_layout(head: bytes) -> tuple[int, Layout]:
    """Pick the layout whose version byte matches the start of a log file."""
    for version, layout in LAYOUTS.items():
        if len(head) > layout.version_offset and head[layout.version_offset] == version:
            return version, layout
    raise UnknownLogFormat(
        "no layout in bikelog.record.LAYOUTS matches this file. If the firmware "
        "bumped BCLogger::LOG_DATA_FORMAT_VERSION (src/BCLogger.h), add the new "
        "layout there."
    )


@dataclass
class ReadStats:
    version: int = 0
    record_size: int = 0
    records: int = 0
    trailing_bytes: int = 0


def read_stream(stream: BinaryIO, stats: ReadStats | None = None) -> Iterator[Record]:
    """Yield every complete record of an open binary log.

    A short final read is reported through ``stats.trailing_bytes`` instead of
    raising: an aborted ride (SD card pulled, battery empty) leaves exactly
    that, and the records before it are perfectly good.
    """
    stats = stats if stats is not None else ReadStats()
    head = stream.read(MAX_VERSION_OFFSET + 1)
    if not head:
        return
    version, layout = detect_layout(head)
    stats.version = version
    stats.record_size = layout.size
    stream.seek(0)
    while True:
        raw = stream.read(layout.size)
        if len(raw) < layout.size:
            stats.trailing_bytes = len(raw)
            return
        yield layout.build(raw)
        stats.records += 1


def read_file(path, stats: ReadStats | None = None) -> Iterator[Record]:
    """Yield every complete record of a log file given by path."""
    with open(path, "rb") as fh:
        yield from read_stream(fh, stats)


def write_records(path, records, version: int = 1) -> int:
    """Write records back out in a given layout version (for test fixtures)."""
    layout = LAYOUTS[version]
    count = 0
    with open(path, "wb") as fh:
        for rec in records:
            values = []
            for name in layout.names:
                values.append(version if name == "format_version" else getattr(rec, name))
            fh.write(struct.pack(layout.fmt, *values))
            count += 1
    return count
