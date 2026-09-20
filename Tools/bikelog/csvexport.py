"""Records -> CSV, the spreadsheet export this tooling started out as.

Defaults are kept bit-for-bit compatible with the pre-package script: same
columns in the same order, timestamps as Excel/LibreOffice day serials, and
a comma as the decimal separator (German locale). The GPS columns are
opt-in, so existing sheets keep working.
"""

from __future__ import annotations

import csv

#: Legacy column set and order. The German names are load-bearing: existing
#: spreadsheets reference them.
BASE_COLUMNS = ("Timestamp", "Speed", "Temperatur", "Gradient", "Höhe",
                "distance", "Puls", "Cadence")

GPS_COLUMNS = ("GPS_Valid", "GPS_Lat", "GPS_Lon", "GPS_Höhe", "GPS_Speed",
               "GPS_Kurs", "GPS_Genauigkeit", "GPS_FixAlter_ms")

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


def row(rec, with_gps: bool = False) -> dict:
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
    return row


def write_stream(stream, records, start_time: int = 0, add_offset: bool = False,
                 with_gps: bool = False, excel_time: bool = True) -> int:
    """Write records as CSV into an open text stream, returning the row count.

    ``start_time`` rebases the timestamps: by default the first record is
    moved to it (and the rest shifted along), with ``add_offset`` it is added
    to every timestamp instead. That exists because a log written before the
    clock was set (no WiFi at boot -> epoch 1970) can still be salvaged by
    hand.
    """
    columns = BASE_COLUMNS + (GPS_COLUMNS if with_gps else ())
    first_timestamp = 0
    count = 0
    writer = csv.DictWriter(stream, fieldnames=columns, dialect="excel")
    writer.writeheader()
    for rec in records:
        values = row(rec, with_gps)
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
