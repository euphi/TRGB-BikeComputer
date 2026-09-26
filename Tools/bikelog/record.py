"""Reader for the binary datalog the bike computer writes to its SD card.

Authoritative source of the format: src/LogRecords.h (format v2; v1 was
``BCLogger::LogData``). A log file is a bare sequence of fixed-size records --
no file header, no index, no terminator; a file that was cut short (card
pulled mid-ride) simply ends in a partial record.

Every record carries the format version the firmware wrote it with, always at
byte 31, so this module dispatches on that byte via the FORMATS registry
rather than assuming a single layout. Reading older logs later means adding
one entry there, not rewriting the reader.

v1  56-byte records, ride data only.
v2  64-byte records of several types, the type at byte 30:
      0  Record             ride data every 5 s (v1 fields + gradient sources, road class)
      1  RoadQualityRecord  road-surface metrics per interval (1..10 s)
      2  ShockEvent         a hard hit
    Unknown types are skipped by their size, so a newer firmware can add one
    without breaking this reader.
"""

from __future__ import annotations

import datetime
import struct
from dataclasses import dataclass, field, fields
from typing import BinaryIO, Iterable, Iterator

# Bits of Record.gps_flags, mirroring LogRec::GpsFlags.
# LOG_GPS_VALID means "a POSITION_UPDATE was received at all" -- NOT that the
# fix is fresh. Freshness is gps_fix_age_ms, see SGpsFix in BikeGpsProtocol.h.
LOG_GPS_VALID = 0x01
LOG_GPS_HAS_ALTITUDE = 0x02
LOG_GPS_HAS_SPEED = 0x04
LOG_GPS_HAS_BEARING = 0x08
LOG_GPS_HAS_ACCURACY = 0x10

# Record types (LogRec::Type)
TYPE_DATA = 0
TYPE_ROAD_QUALITY = 1
TYPE_SHOCK = 2

CURRENT_VERSION = 2
VERSION_OFFSET = 31
TYPE_OFFSET = 30

#: Sentinels of the v2 records (LogRec::GRAD_INVALID, LogRec::U16_INVALID)
GRAD_INVALID = -32768
U16_INVALID = 0xFFFF

# RoadQualityRecord.flags (RQ::IntervalFlags in src/RoadQuality.h)
IF_TOO_SLOW = 0x01
IF_DATA_GAP = 0x02
IF_CLIPPED = 0x04
IF_UNCALIBRATED = 0x08
IF_REF_RIDE = 0x10
IF_SPEED_FROM_GPS = 0x20
IF_NO_SPEED = 0x40
IF_GPS_VALID = 0x80

# ShockEvent.flags (RQ::ShockFlags)
SF_CLIPPED = 0x01
SF_WHEELBASE_MATCH = 0x02
SF_STANDSTILL = 0x04
SF_GPS_VALID = 0x08
SF_AFTER_SUPPRESSED = 0x10
SF_NO_SPEED = 0x20

#: Road classes, and the OSM smoothness value each roughly corresponds to
ROAD_CLASS_NAMES = {
    0: "nicht bewertet",
    1: "glatt",
    2: "gut",
    3: "mäßig",
    4: "rau",
    5: "sehr rau",
}
ROAD_CLASS_OSM = {1: "excellent", 2: "good", 3: "intermediate", 4: "bad", 5: "very_bad"}


def _utc(timestamp: int, ms: int = 0) -> datetime.datetime:
    return datetime.datetime.fromtimestamp(timestamp + ms / 1000.0, datetime.timezone.utc)


def _grad(x100: int) -> float | None:
    return None if x100 == GRAD_INVALID else x100 / 100.0


@dataclass
class Record:
    """Ride data, logged every 5 s. Units are the firmware's, converted nowhere.

    speed     km/h   (wheel sensor; Distance.cpp multiplies by 3.6)
    temp      °C
    gradient  %      (the value shown on the display -- barometric, or the
                      accelerometer's if selected; see grad_baro/grad_imu)
    height    m      (barometric, from the I2C sensor)
    distance  m      (trip distance since start)
    hr        bpm    (0 = no sensor / no reading)
    cadence   rpm    (0 = no sensor / no reading)
    road_class       0 = not rated, 1 (smooth) .. 5 (very rough)   [v2]
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
    # v2
    record_type: int = TYPE_DATA
    road_class: int = 0
    timestamp_ms: int = 0
    grad_baro_x100: int = GRAD_INVALID
    grad_imu_x100: int = GRAD_INVALID

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

    @property
    def grad_baro(self) -> float | None:
        """Barometric gradient in %, None if unknown (and always in v1 logs)."""
        return _grad(self.grad_baro_x100)

    @property
    def grad_imu(self) -> float | None:
        """Accelerometer gradient in %, None if unknown (and always in v1 logs)."""
        return _grad(self.grad_imu_x100)

    @property
    def time(self) -> float:
        return self.timestamp + self.timestamp_ms / 1000.0

    def utc(self) -> datetime.datetime:
        return datetime.datetime.fromtimestamp(self.timestamp, datetime.timezone.utc)


@dataclass
class RoadQualityRecord:
    """Road-surface metrics over one interval (v2, type 1).

    The accelerations are band-passed (2..80 Hz); "vertical" is along gravity.
    Raw integer fields are kept as written; the properties convert them.
    """

    timestamp: int = 0                  # end of the interval
    timestamp_ms: int = 0
    interval_ms: int = 0
    sample_count: int = 0
    speed_cms: int = U16_INVALID
    rms_vert_mg: int = 0
    rms_horiz_mg: int = 0
    peak_vert_max_mg: int = 0
    peak_vert_min_mg: int = 0
    peak_total_mg: int = 0
    roughness_x100: int = U16_INVALID
    road_class: int = 0
    flags: int = 0
    record_type: int = TYPE_ROAD_QUALITY
    format_version: int = 0
    vdv_vert: float = 0.0               # m/s^1.75
    distance_m: float = 0.0
    count_over_t1: int = 0              # crossings of 1 g
    count_over_t2: int = 0              # crossings of 2 g
    events_logged: int = 0
    events_suppressed: int = 0
    gps_lat_e7: int = 0
    gps_lon_e7: int = 0
    gps_fix_age_ms: int = 0
    gps_accuracy_m_x10: int = 0
    grad_imu_x100: int = GRAD_INVALID

    @property
    def rated(self) -> bool:
        return self.road_class != 0

    @property
    def roughness(self) -> float | None:
        return None if self.roughness_x100 == U16_INVALID else self.roughness_x100 / 100.0

    @property
    def speed_kmh(self) -> float | None:
        return None if self.speed_cms == U16_INVALID else self.speed_cms * 0.036

    @property
    def gps_valid(self) -> bool:
        return bool(self.flags & IF_GPS_VALID)

    @property
    def latitude(self) -> float:
        return self.gps_lat_e7 / 1e7

    @property
    def longitude(self) -> float:
        return self.gps_lon_e7 / 1e7

    @property
    def grad_imu(self) -> float | None:
        return _grad(self.grad_imu_x100)

    @property
    def time(self) -> float:
        return self.timestamp + self.timestamp_ms / 1000.0

    def utc(self) -> datetime.datetime:
        return _utc(self.timestamp, self.timestamp_ms)


@dataclass
class ShockEvent:
    """A hard hit (v2, type 2). Time and position are those of the (first) peak."""

    timestamp: int = 0
    timestamp_ms: int = 0
    duration_ms: int = 0
    peak_total_mg: int = 0
    peak_vert_max_mg: int = 0
    peak_vert_min_mg: int = 0
    peak_horiz_mg: int = 0
    pre_rms_mg: int = 0                 # vertical RMS of the second before
    speed_cms: int = U16_INVALID
    second_peak_mg: int = 0             # rear wheel, 0 = none
    second_peak_delay_ms: int = 0
    severity: int = 0                   # 1 (3..5 g), 2 (5..8 g), 3 (> 8 g or clipped)
    flags: int = 0
    record_type: int = TYPE_SHOCK
    format_version: int = 0
    vdv: float = 0.0
    samples_over_threshold: int = 0
    threshold_mg: int = 0
    gps_lat_e7: int = 0
    gps_lon_e7: int = 0
    gps_fix_age_ms: int = 0
    gps_accuracy_m_x10: int = 0
    reserved1: int = 0
    event_seq: int = 0                  # running number since boot; gaps = records lost
    reserved2: int = 0

    @property
    def peak_g(self) -> float:
        return self.peak_total_mg / 1000.0

    @property
    def speed_kmh(self) -> float | None:
        return None if self.speed_cms == U16_INVALID else self.speed_cms * 0.036

    @property
    def gps_valid(self) -> bool:
        return bool(self.flags & SF_GPS_VALID)

    @property
    def wheelbase_match(self) -> bool:
        return bool(self.flags & SF_WHEELBASE_MATCH)

    @property
    def latitude(self) -> float:
        return self.gps_lat_e7 / 1e7

    @property
    def longitude(self) -> float:
        return self.gps_lon_e7 / 1e7

    @property
    def time(self) -> float:
        return self.timestamp + self.timestamp_ms / 1000.0

    def utc(self) -> datetime.datetime:
        return _utc(self.timestamp, self.timestamp_ms)


AnyRecord = Record | RoadQualityRecord | ShockEvent


class Layout:
    """One binary record layout: a struct format and the dataclass it fills.

    ``fmt`` is a little-endian, explicitly packed struct format ('<'), which
    is what the ESP32-S3 produces: the firmware structs have no padding gaps,
    a property src/LogRecords.h pins down with static_asserts.
    """

    def __init__(self, cls, fmt: str, names: tuple[str, ...], version_offset: int = VERSION_OFFSET):
        known = {f.name for f in fields(cls)}
        unknown = set(names) - known
        if unknown:
            raise ValueError("Layout names not present on %s: %s" % (cls.__name__, sorted(unknown)))
        self.cls = cls
        self.fmt = fmt
        self.names = names
        self.version_offset = version_offset
        self.size = struct.calcsize(fmt)

    def build(self, raw: bytes):
        """Unpack one record. Fields the layout lacks keep their default."""
        return self.cls(**dict(zip(self.names, struct.unpack(self.fmt, raw))))

    def pack(self, rec, version: int) -> bytes:
        values = [version if name == "format_version" else getattr(rec, name)
                  for name in self.names]
        return struct.pack(self.fmt, *values)


@dataclass
class Format:
    """All record layouts of one format version."""

    version: int
    size: int
    layouts: dict[int, Layout]
    #: None: a single record type, no type byte (v1)
    type_offset: int | None = None

    def layout_for(self, rec) -> tuple[int, Layout]:
        for rtype, layout in self.layouts.items():
            if isinstance(rec, layout.cls):
                return rtype, layout
        raise TypeError("format v%d has no layout for %s" % (self.version, type(rec).__name__))


_V1_DATA = Layout(
    Record,
    # 8-byte time_t, five floats, four single bytes
    # (hr/cadence/gpsFlags/formatVersion), then the GPS block. 56 bytes.
    "<q5f4B3iIHHI",
    (
        "timestamp", "speed", "temp", "gradient", "height", "distance",
        "hr", "cadence", "gps_flags", "format_version",
        "gps_lat_e7", "gps_lon_e7", "gps_altitude_m",
        "gps_speed_cms", "gps_bearing_deg_x100", "gps_accuracy_m_x10",
        "gps_fix_age_ms",
    ),
)

_V2_DATA = Layout(
    Record,
    # v1 with recordType where gpsFlags was, gpsFlags moved behind the GPS
    # block, then road class, milliseconds and the two gradient sources.
    "<q5f4B3iIHHIBBHhh",
    (
        "timestamp", "speed", "temp", "gradient", "height", "distance",
        "hr", "cadence", "record_type", "format_version",
        "gps_lat_e7", "gps_lon_e7", "gps_altitude_m",
        "gps_speed_cms", "gps_bearing_deg_x100", "gps_accuracy_m_x10",
        "gps_fix_age_ms",
        "gps_flags", "road_class", "timestamp_ms", "grad_baro_x100", "grad_imu_x100",
    ),
)

_V2_ROAD_QUALITY = Layout(
    RoadQualityRecord,
    "<qHHHHHHhhHHBBBBffHHHHiiIHh",
    (
        "timestamp", "timestamp_ms", "interval_ms", "sample_count", "speed_cms",
        "rms_vert_mg", "rms_horiz_mg", "peak_vert_max_mg", "peak_vert_min_mg",
        "peak_total_mg", "roughness_x100",
        "road_class", "flags", "record_type", "format_version",
        "vdv_vert", "distance_m",
        "count_over_t1", "count_over_t2", "events_logged", "events_suppressed",
        "gps_lat_e7", "gps_lon_e7", "gps_fix_age_ms", "gps_accuracy_m_x10",
        "grad_imu_x100",
    ),
)

_V2_SHOCK = Layout(
    ShockEvent,
    "<qHHHhhHHHHHBBBBfHHiiIHHII",
    (
        "timestamp", "timestamp_ms", "duration_ms", "peak_total_mg",
        "peak_vert_max_mg", "peak_vert_min_mg", "peak_horiz_mg", "pre_rms_mg",
        "speed_cms", "second_peak_mg", "second_peak_delay_ms",
        "severity", "flags", "record_type", "format_version",
        "vdv", "samples_over_threshold", "threshold_mg",
        "gps_lat_e7", "gps_lon_e7", "gps_fix_age_ms", "gps_accuracy_m_x10",
        "reserved1", "event_seq", "reserved2",
    ),
)

FORMATS: dict[int, Format] = {
    1: Format(1, _V1_DATA.size, {TYPE_DATA: _V1_DATA}),
    2: Format(2, 64, {TYPE_DATA: _V2_DATA, TYPE_ROAD_QUALITY: _V2_ROAD_QUALITY,
                      TYPE_SHOCK: _V2_SHOCK}, type_offset=TYPE_OFFSET),
}

#: The ride-data layout of each version (the only record type v1 had).
LAYOUTS: dict[int, Layout] = {version: fmt.layouts[TYPE_DATA] for version, fmt in FORMATS.items()}

MAX_VERSION_OFFSET = VERSION_OFFSET


class UnknownLogFormat(Exception):
    """The version byte matches no format in FORMATS."""


def detect_format(head: bytes) -> Format:
    """Pick the format whose version byte matches the start of a log file."""
    if len(head) > VERSION_OFFSET and head[VERSION_OFFSET] in FORMATS:
        return FORMATS[head[VERSION_OFFSET]]
    raise UnknownLogFormat(
        "no format in bikelog.record.FORMATS matches this file. If the firmware "
        "bumped LogRec::FORMAT_VERSION (src/LogRecords.h), add the new layouts there."
    )


@dataclass
class ReadStats:
    version: int = 0
    record_size: int = 0
    records: int = 0                    # records yielded
    trailing_bytes: int = 0
    by_type: dict[int, int] = field(default_factory=dict)   # all complete records read, per type
    unknown_type: int = 0               # skipped: type this reader does not know


def read_stream(stream: BinaryIO, stats: ReadStats | None = None,
                types: Iterable[int] | None = (TYPE_DATA,)) -> Iterator[AnyRecord]:
    """Yield the complete records of an open binary log.

    ``types`` selects the record types to yield; the default is the ride data
    only (``Record``), which is what every exporter up to v1 consumed. Pass
    None for all types, in file order.

    A short final read is reported through ``stats.trailing_bytes`` instead of
    raising: an aborted ride (SD card pulled, battery empty) leaves exactly
    that, and the records before it are perfectly good.
    """
    stats = stats if stats is not None else ReadStats()
    wanted = None if types is None else set(types)
    head = stream.read(MAX_VERSION_OFFSET + 1)
    if not head:
        return
    fmt = detect_format(head)
    stats.version = fmt.version
    stats.record_size = fmt.size
    stream.seek(0)
    while True:
        raw = stream.read(fmt.size)
        if len(raw) < fmt.size:
            stats.trailing_bytes = len(raw)
            return
        rtype = raw[fmt.type_offset] if fmt.type_offset is not None else TYPE_DATA
        stats.by_type[rtype] = stats.by_type.get(rtype, 0) + 1
        layout = fmt.layouts.get(rtype)
        if layout is None:
            stats.unknown_type += 1
            continue
        if wanted is not None and rtype not in wanted:
            continue
        yield layout.build(raw)
        stats.records += 1


def read_file(path, stats: ReadStats | None = None,
              types: Iterable[int] | None = (TYPE_DATA,)) -> Iterator[AnyRecord]:
    """Yield the complete records of a log file given by path (see read_stream)."""
    with open(path, "rb") as fh:
        yield from read_stream(fh, stats, types)


def split(records: Iterable[AnyRecord]) -> tuple[list[Record], list[RoadQualityRecord], list[ShockEvent]]:
    """Sort a mixed record stream into (ride data, road quality, shocks)."""
    data, road, shocks = [], [], []
    for rec in records:
        if isinstance(rec, RoadQualityRecord):
            road.append(rec)
        elif isinstance(rec, ShockEvent):
            shocks.append(rec)
        else:
            data.append(rec)
    return data, road, shocks


def write_records(path, records, version: int = CURRENT_VERSION) -> int:
    """Write records in a given format version (for test fixtures).

    v2 takes a mix of Record, RoadQualityRecord and ShockEvent; each is
    written with its own layout and type byte, in the order given.
    """
    fmt = FORMATS[version]
    count = 0
    with open(path, "wb") as fh:
        for rec in records:
            rtype, layout = fmt.layout_for(rec)
            if fmt.type_offset is not None:
                rec.record_type = rtype
            fh.write(layout.pack(rec, version))
            count += 1
    return count
