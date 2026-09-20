"""Command line front end: bikelog csv|gpx|info|fixture.

Run as ``python3 -m bikelog ...`` from Tools/, or through the thin
ReadTachoBin.py wrapper for the CSV export.
"""

from __future__ import annotations

import argparse
import datetime
import sys

from . import csvexport, fixtures, gpx
from .record import ReadStats, UnknownLogFormat, read_file

__version__ = "0.3"


def _read(path: str, stats: ReadStats):
    try:
        return list(read_file(path, stats))
    except UnknownLogFormat as exc:
        raise SystemExit("Unbekanntes Logformat in %s: %s" % (path, exc))
    except FileNotFoundError:
        raise SystemExit("Datei nicht gefunden: %s" % path)


def _warn_trailing(path: str, stats: ReadStats) -> None:
    if stats.trailing_bytes:
        print("Warnung: %d angebrochene(s) Byte(s) am Ende von %s ignoriert "
              "(Aufzeichnung abgebrochen?)" % (stats.trailing_bytes, path),
              file=sys.stderr)


def cmd_csv(opts) -> int:
    stats = ReadStats()
    records = _read(opts.infile, stats)
    rows = csvexport.write(opts.outfile, records, start_time=opts.starttime,
                           add_offset=opts.add_flag, with_gps=opts.gps)
    _warn_trailing(opts.infile, stats)
    print("%d Datensatz/Datensätze (Format v%d) -> %s" % (rows, stats.version, opts.outfile))
    return 0


def cmd_gpx(opts) -> int:
    stats = ReadStats()
    records = _read(opts.infile, stats)
    options = gpx.GpxOptions(
        max_fix_age_ms=opts.max_fix_age,
        segment_gap_s=opts.segment_gap,
        ele_source=opts.ele,
        max_accuracy_m=opts.max_accuracy,
        track_name=opts.name,
    )
    result = gpx.write(opts.outfile, records, options)
    _warn_trailing(opts.infile, stats)
    print(result.summary())
    if result.written == 0:
        print("Keine verwertbare Position im Log -- GPX ist leer.", file=sys.stderr)
        return 1
    print("-> %s" % opts.outfile)
    return 0


def cmd_info(opts) -> int:
    stats = ReadStats()
    records = _read(opts.infile, stats)
    _warn_trailing(opts.infile, stats)
    if not records:
        print("Leeres Log.")
        return 0
    first, last = records[0], records[-1]
    with_fix = [r for r in records if r.gps_valid]
    fresh = [r for r in with_fix if r.gps_fix_age_ms <= 5000]
    print("Datei:        %s" % opts.infile)
    print("Format:       v%d, %d Byte/Datensatz" % (stats.version, stats.record_size))
    print("Datensätze:   %d" % len(records))
    print("Zeitraum:     %s .. %s (%s)" % (
        first.utc().isoformat(sep=" ", timespec="seconds"),
        last.utc().isoformat(sep=" ", timespec="seconds"),
        datetime.timedelta(seconds=last.timestamp - first.timestamp)))
    if first.utc().year < gpx.MIN_PLAUSIBLE_YEAR:
        print("              ACHTUNG: Startzeit vor %d -- Uhr war beim Aufzeichnen "
              "nicht gesetzt (NTP)." % gpx.MIN_PLAUSIBLE_YEAR)
    print("Distanz:      %.2f km" % ((last.distance - first.distance) / 1000.0))
    print("GPS:          %d mit Fix, davon %d frisch (<=5 s)" % (len(with_fix), len(fresh)))
    if fresh:
        lats = [r.latitude for r in fresh]
        lons = [r.longitude for r in fresh]
        print("Bounding Box: %.5f..%.5f N, %.5f..%.5f E"
              % (min(lats), max(lats), min(lons), max(lons)))
    hrs = [r.hr for r in records if r.hr]
    cads = [r.cadence for r in records if r.cadence]
    print("Puls:         %s" % ("%d Werte, %d..%d bpm" % (len(hrs), min(hrs), max(hrs))
                                if hrs else "keine"))
    print("Trittfrequenz:%s" % (" %d Werte, %d..%d rpm" % (len(cads), min(cads), max(cads))
                                if cads else " keine"))
    return 0


def cmd_fixture_synth(opts) -> int:
    records = fixtures.synthetic(
        seconds=opts.seconds,
        start_lat=opts.lat, start_lon=opts.lon,
        pause=None if opts.no_pause else (120, 300),
        tunnel=None if opts.no_tunnel else (60, 90),
        no_fix_start_s=opts.no_fix_start,
        unset_clock_records=opts.unset_clock,
    )
    count = fixtures.write_bin(opts.outfile, records)
    print("%d synthetische(r) Datensatz/Datensätze -> %s" % (count, opts.outfile))
    return 0


def cmd_fixture_from_gpx(opts) -> int:
    records = fixtures.from_gpx(opts.gpxfile)
    if not records:
        raise SystemExit("Keine <trkpt> mit <time> in %s gefunden" % opts.gpxfile)
    count = fixtures.write_bin(opts.outfile, records)
    print("%d Datensatz/Datensätze aus %s -> %s" % (count, opts.gpxfile, opts.outfile))
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="bikelog",
        description="Werkzeuge für die Binärlogs des Fahrradcomputers")
    parser.add_argument("--version", action="version", version="bikelog " + __version__)
    sub = parser.add_subparsers(dest="command", required=True)

    p_csv = sub.add_parser("csv", help="Binärlog nach CSV konvertieren")
    p_csv.add_argument("-i", "--in", dest="infile", required=True, metavar="FILE")
    p_csv.add_argument("-o", "--out", dest="outfile", default="./out.csv", metavar="FILE")
    p_csv.add_argument("-t", "--timestamp", dest="starttime", type=int, default=0,
                       help="Zeitstempel des ersten Eintrags [Standard: aus den Daten]")
    p_csv.add_argument("-a", "--add", dest="add_flag", action="store_true",
                       help="Zeitstempel addieren statt als ersten setzen")
    p_csv.add_argument("--gps", action="store_true",
                       help="GPS-Spalten zusätzlich ausgeben")
    p_csv.set_defaults(func=cmd_csv)

    p_gpx = sub.add_parser("gpx", help="Binärlog nach GPX 1.1 konvertieren")
    p_gpx.add_argument("-i", "--in", dest="infile", required=True, metavar="FILE")
    p_gpx.add_argument("-o", "--out", dest="outfile", default="./out.gpx", metavar="FILE")
    p_gpx.add_argument("--max-fix-age", type=int, default=5000, metavar="MS",
                       help="Fixes älter als MS verwerfen, 0 = aus [Standard: %(default)s]")
    p_gpx.add_argument("--segment-gap", type=float, default=60.0, metavar="S",
                       help="Neues Segment nach S Sekunden Lücke, 0 = aus "
                            "[Standard: %(default)s]")
    p_gpx.add_argument("--ele", choices=("auto", "baro", "gps"), default="auto",
                       help="Höhenquelle [Standard: %(default)s]")
    p_gpx.add_argument("--max-accuracy", type=float, default=0.0, metavar="M",
                       help="Fixes mit schlechterer Genauigkeit als M Meter verwerfen, "
                            "0 = aus [Standard: %(default)s]")
    p_gpx.add_argument("--name", help="Trackname [Standard: aus der Startzeit]")
    p_gpx.set_defaults(func=cmd_gpx)

    p_info = sub.add_parser("info", help="Binärlog zusammenfassen")
    p_info.add_argument("-i", "--in", dest="infile", required=True, metavar="FILE")
    p_info.set_defaults(func=cmd_info)

    p_fix = sub.add_parser("fixture", help="Testdaten erzeugen (ohne Hardware)")
    fix_sub = p_fix.add_subparsers(dest="fixture_command", required=True)

    p_synth = fix_sub.add_parser("synth", help="synthetische Fahrt erzeugen")
    p_synth.add_argument("-o", "--out", dest="outfile", required=True, metavar="FILE")
    p_synth.add_argument("--seconds", type=int, default=420)
    p_synth.add_argument("--lat", type=float, default=52.4)
    p_synth.add_argument("--lon", type=float, default=8.7)
    p_synth.add_argument("--no-pause", action="store_true", help="ohne Pause/Zeitlücke")
    p_synth.add_argument("--no-tunnel", action="store_true", help="ohne veraltete Fixes")
    p_synth.add_argument("--no-fix-start", type=int, default=20, metavar="S",
                         help="Sekunden ohne GPS-Fix am Anfang [Standard: %(default)s]")
    p_synth.add_argument("--unset-clock", type=int, default=0, metavar="N",
                         help="N Datensätze mit ungesetzter Uhr voranstellen")
    p_synth.set_defaults(func=cmd_fixture_synth)

    p_from = fix_sub.add_parser("from-gpx", help="Binärlog aus einer GPX-Datei bauen")
    p_from.add_argument("gpxfile", metavar="GPX")
    p_from.add_argument("-o", "--out", dest="outfile", required=True, metavar="FILE")
    p_from.set_defaults(func=cmd_fixture_from_gpx)

    return parser


def main(argv=None) -> int:
    opts = build_parser().parse_args(argv)
    return opts.func(opts)
