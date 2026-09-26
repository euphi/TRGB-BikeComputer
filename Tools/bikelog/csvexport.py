"""Records -> CSV, the spreadsheet export this tooling started out as.

Defaults are kept bit-for-bit compatible with the pre-package script: same
columns in the same order, timestamps as Excel/LibreOffice day serials, and
a comma as the decimal separator (German locale). The GPS and road-quality
columns are opt-in, so existing sheets keep working.

Road-quality intervals and shocks (log format v2) are separate record types
at their own pace, so they get their own CSV files: write_road_quality() and
write_shocks().
"""

from __future__ import annotations

import csv

from .record import LABEL_REASON_NAMES, LF_CAPTURING, SURFACE_NAMES, label_at

#: Legacy column set and order. The German names are load-bearing: existing
#: spreadsheets reference them.
BASE_COLUMNS = ("Timestamp", "Speed", "Temperatur", "Gradient", "Höhe",
                "distance", "Puls", "Cadence")

GPS_COLUMNS = ("GPS_Valid", "GPS_Lat", "GPS_Lon", "GPS_Höhe", "GPS_Speed",
               "GPS_Kurs", "GPS_Genauigkeit", "GPS_FixAlter_ms")

#: Extra columns of the ride-data CSV from log format v2 (empty for v1 logs).
ROADQ_COLUMNS = ("Wegeklasse", "Gradient_Baro", "Gradient_IMU")

ROAD_QUALITY_COLUMNS = (
    "Timestamp", "Intervall_ms", "Klasse", "Rauheit", "RMS_vert_mg", "RMS_hor_mg",
    "Peak_max_mg", "Peak_min_mg", "Peak_total_mg", "VDV", "Speed", "Strecke_m",
    "Ueber_1g", "Ueber_2g", "Stoesse", "Stoesse_unterdrueckt", "Flags",
    "GPS_Lat", "GPS_Lon", "GPS_FixAlter_ms", "Gradient_IMU",
    "Untergrund", "Qualitaet_manuell",
)

SHOCK_COLUMNS = (
    "Timestamp", "Nr", "Schwere", "Peak_g", "Vert_max_g", "Vert_min_g", "Horiz_g",
    "Dauer_ms", "Zweiter_Peak_g", "Verzoegerung_ms", "Radstand_passt", "RMS_vorher_mg",
    "Schwelle_g", "Speed", "Flags", "GPS_Lat", "GPS_Lon", "GPS_FixAlter_ms",
    "Untergrund", "Qualitaet_manuell",
)

LABEL_COLUMNS = (
    "Timestamp", "Nr", "Anlass", "Untergrund", "Qualitaet_manuell", "Mitschnitt",
    "Vorher_Untergrund", "Vorher_Qualitaet", "Vorher_Strecke_m", "Vorher_Dauer_s",
    "Speed", "GPS_Lat", "GPS_Lon", "GPS_FixAlter_ms",
)

#: Days between the Unix epoch and the Excel/LibreOffice epoch (1899-12-30).
EXCEL_EPOCH_OFFSET = 25569


def to_excel_timestamp(unix_timestamp: float) -> float:
    return unix_timestamp / (60 * 60 * 24) + EXCEL_EPOCH_OFFSET


def localize_floats(row: dict) -> dict:
    """Comma as decimal separator -- what LibreOffice expects here."""
    return {
        key: (str(value).replace(".", ",") if isinstance(value, float) else value)
        for key, value in row.items()
    }


def _blank(value):
    """None -> empty cell (not 0, which would read as a measurement)."""
    return "" if value is None else value


def row(rec, with_gps: bool = False, with_roadq: bool = False) -> dict:
    row = {
        "Timestamp": rec.timestamp,
        "Speed": rec.speed,
        "Temperatur": rec.temp,
        "Gradient": rec.gradient,
        "Höhe": rec.height,
        "distance": rec.distance,
        "Puls": rec.hr,
        "Cadence": rec.cadence,
    }
    if with_gps:
        # Empty cell rather than 0 where the flag says "no value": a 0 would
        # read as a real measurement (sea level, due north, this instant).
        row.update({
            "GPS_Valid": int(rec.gps_valid),
            "GPS_Lat": rec.latitude if rec.gps_valid else "",
            "GPS_Lon": rec.longitude if rec.gps_valid else "",
            "GPS_Höhe": rec.gps_altitude_m if rec.has_gps_altitude else "",
            "GPS_Speed": rec.gps_speed_ms if rec.has_gps_speed else "",
            "GPS_Kurs": rec.gps_bearing_deg if rec.has_gps_bearing else "",
            "GPS_Genauigkeit": rec.gps_accuracy_m if rec.has_gps_accuracy else "",
            "GPS_FixAlter_ms": rec.gps_fix_age_ms if rec.gps_valid else "",
        })
    if with_roadq:
        row.update({
            "Wegeklasse": rec.road_class or "",
            "Gradient_Baro": _blank(rec.grad_baro),
            "Gradient_IMU": _blank(rec.grad_imu),
        })
    return row


def write_stream(stream, records, start_time: int = 0, add_offset: bool = False,
                 with_gps: bool = False, excel_time: bool = True,
                 with_roadq: bool = False) -> int:
    """Write records as CSV into an open text stream, returning the row count.

    ``start_time`` rebases the timestamps: by default the first record is
    moved to it (and the rest shifted along), with ``add_offset`` it is added
    to every timestamp instead. That exists because a log written before the
    clock was set (no WiFi at boot -> epoch 1970) can still be salvaged by
    hand.
    """
    columns = BASE_COLUMNS + (GPS_COLUMNS if with_gps else ()) + (ROADQ_COLUMNS if with_roadq else ())
    first_timestamp = 0
    count = 0
    writer = csv.DictWriter(stream, fieldnames=columns, dialect="excel")
    writer.writeheader()
    for rec in records:
        values = row(rec, with_gps, with_roadq)
        if start_time > 0:
            if first_timestamp == 0 and not add_offset:
                first_timestamp = values["Timestamp"]
            values["Timestamp"] = start_time + values["Timestamp"] - first_timestamp
        if excel_time:
            values["Timestamp"] = to_excel_timestamp(values["Timestamp"])
        writer.writerow(localize_floats(values))
        count += 1
    return count


def write(path, records, **kwargs) -> int:
    """Write records to a CSV file, returning the number of rows."""
    with open(path, "w", newline="", encoding="utf-8") as fh:
        return write_stream(fh, records, **kwargs)


def _write_rows(path, columns, rows, excel_time: bool) -> int:
    count = 0
    with open(path, "w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=columns, dialect="excel")
        writer.writeheader()
        for values in rows:
            if excel_time:
                values["Timestamp"] = to_excel_timestamp(values["Timestamp"])
            writer.writerow(localize_floats(values))
            count += 1
    return count


def _surface(code: int):
    return SURFACE_NAMES.get(code, code) if code else ""


def road_quality_row(rec, label: tuple[int, int] = (0, 0)) -> dict:
    """One interval; ``label`` = the manual (surface, quality) in effect (record.label_at)."""
    return {
        "Timestamp": rec.time,
        "Intervall_ms": rec.interval_ms,
        "Klasse": rec.road_class or "",
        "Rauheit": _blank(rec.roughness),
        "RMS_vert_mg": rec.rms_vert_mg,
        "RMS_hor_mg": rec.rms_horiz_mg,
        "Peak_max_mg": rec.peak_vert_max_mg,
        "Peak_min_mg": rec.peak_vert_min_mg,
        "Peak_total_mg": rec.peak_total_mg,
        "VDV": rec.vdv_vert,
        "Speed": _blank(rec.speed_kmh),
        "Strecke_m": rec.distance_m,
        "Ueber_1g": rec.count_over_t1,
        "Ueber_2g": rec.count_over_t2,
        "Stoesse": rec.events_logged,
        "Stoesse_unterdrueckt": rec.events_suppressed,
        "Flags": "0x%02X" % rec.flags,
        "GPS_Lat": rec.latitude if rec.gps_valid else "",
        "GPS_Lon": rec.longitude if rec.gps_valid else "",
        "GPS_FixAlter_ms": rec.gps_fix_age_ms if rec.gps_valid else "",
        "Gradient_IMU": _blank(rec.grad_imu),
        "Untergrund": _surface(label[0]),
        "Qualitaet_manuell": label[1] or "",
    }


def shock_row(rec) -> dict:
    return {
        "Timestamp": rec.time,
        "Nr": rec.event_seq,
        "Schwere": rec.severity,
        "Peak_g": rec.peak_g,
        "Vert_max_g": rec.peak_vert_max_mg / 1000.0,
        "Vert_min_g": rec.peak_vert_min_mg / 1000.0,
        "Horiz_g": rec.peak_horiz_mg / 1000.0,
        "Dauer_ms": rec.duration_ms,
        "Zweiter_Peak_g": rec.second_peak_mg / 1000.0 if rec.second_peak_mg else "",
        "Verzoegerung_ms": rec.second_peak_delay_ms if rec.second_peak_mg else "",
        "Radstand_passt": int(rec.wheelbase_match),
        "RMS_vorher_mg": rec.pre_rms_mg,
        "Schwelle_g": rec.threshold_mg / 1000.0,
        "Speed": _blank(rec.speed_kmh),
        "Flags": "0x%02X" % rec.flags,
        "GPS_Lat": rec.latitude if rec.gps_valid else "",
        "GPS_Lon": rec.longitude if rec.gps_valid else "",
        "GPS_FixAlter_ms": rec.gps_fix_age_ms if rec.gps_valid else "",
        "Untergrund": _surface(rec.label_surface),
        "Qualitaet_manuell": rec.label_quality or "",
    }


def label_row(rec) -> dict:
    return {
        "Timestamp": rec.time,
        "Nr": rec.label_seq,
        "Anlass": LABEL_REASON_NAMES.get(rec.reason, rec.reason),
        "Untergrund": _surface(rec.surface),
        "Qualitaet_manuell": rec.quality or "",
        "Mitschnitt": int(bool(rec.flags & LF_CAPTURING)),
        "Vorher_Untergrund": _surface(rec.prev_surface),
        "Vorher_Qualitaet": rec.prev_quality or "",
        "Vorher_Strecke_m": rec.prev_distance_m,
        "Vorher_Dauer_s": rec.prev_duration_ms / 1000.0,
        "Speed": _blank(rec.speed_kmh),
        "GPS_Lat": rec.latitude if rec.gps_valid else "",
        "GPS_Lon": rec.longitude if rec.gps_valid else "",
        "GPS_FixAlter_ms": rec.gps_fix_age_ms if rec.gps_valid else "",
    }


def write_road_quality(path, records, excel_time: bool = True, labels=None) -> int:
    """Road-quality intervals (RoadQualityRecord) to CSV, returning the row count.
    ``labels`` (LabelRecord, time order) fills the manual-label columns."""
    labels = list(labels or [])
    rows = (road_quality_row(r, label_at(labels, r.time)) for r in records)
    return _write_rows(path, ROAD_QUALITY_COLUMNS, rows, excel_time)


def write_shocks(path, records, excel_time: bool = True) -> int:
    """Shock events (ShockEvent) to CSV, returning the row count."""
    return _write_rows(path, SHOCK_COLUMNS, (shock_row(r) for r in records), excel_time)


def write_labels(path, records, excel_time: bool = True) -> int:
    """Manual label records (LabelRecord) to CSV, returning the row count."""
    return _write_rows(path, LABEL_COLUMNS, (label_row(r) for r in records), excel_time)
