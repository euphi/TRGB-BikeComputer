"""Command line front end: bikelog csv|gpx|info|fixture.

Run as ``python3 -m bikelog ...`` from Tools/, or through the thin
ReadTachoBin.py wrapper for the CSV export.
"""

from __future__ import annotations

import argparse
import datetime
import sys

from . import csvexport, fixtures, gpx, raw, replay
from .record import (CURRENT_VERSION, IF_NO_SPEED, IF_TOO_SLOW, IF_UNCALIBRATED,
                     ROAD_CLASS_NAMES, SURFACE_NAMES, ReadStats, UnknownLogFormat, label_at,
                     labels_of, read_file, split)

__version__ = "0.4"


def _read(path: str, stats: ReadStats):
    """All record types, in file order."""
    try:
        return list(read_file(path, stats, types=None))
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
    everything = _read(opts.infile, stats)
    records, road, shocks = split(everything)
    labels = labels_of(everything)
    rows = csvexport.write(opts.outfile, records, start_time=opts.starttime,
                           add_offset=opts.add_flag, with_gps=opts.gps,
                           with_roadq=opts.roadq)
    _warn_trailing(opts.infile, stats)
    print("%d Datensatz/Datensätze (Format v%d) -> %s" % (rows, stats.version, opts.outfile))
    if opts.roadq_out:
        n = csvexport.write_road_quality(opts.roadq_out, road, labels=labels)
        print("%d Wegequalitäts-Intervall(e) -> %s" % (n, opts.roadq_out))
    if opts.shocks_out:
        n = csvexport.write_shocks(opts.shocks_out, shocks)
        print("%d Stoß/Stöße -> %s" % (n, opts.shocks_out))
    if opts.labels_out:
        n = csvexport.write_labels(opts.labels_out, labels)
        print("%d Label-Datensatz/-sätze -> %s" % (n, opts.labels_out))
    return 0


def cmd_gpx(opts) -> int:
    stats = ReadStats()
    everything = _read(opts.infile, stats)
    options = gpx.GpxOptions(
        max_fix_age_ms=opts.max_fix_age,
        segment_gap_s=opts.segment_gap,
        ele_source=opts.ele,
        max_accuracy_m=opts.max_accuracy,
        track_name=opts.name,
        shocks=not opts.no_shocks,
        min_shock_severity=opts.min_severity,
        labels=not opts.no_labels,
        rich=not opts.plain,
        device=opts.device,
    )
    xml, result = gpx.from_records(everything, options)
    with open(opts.outfile, "w", encoding="utf-8") as fh:
        fh.write(xml)
    _warn_trailing(opts.infile, stats)
    print(result.summary())
    if result.written == 0:
        print("Keine verwertbare Position im Log -- GPX ist leer.", file=sys.stderr)
        return 1
    print("-> %s" % opts.outfile)
    return 0


def cmd_info(opts) -> int:
    stats = ReadStats()
    everything = _read(opts.infile, stats)
    records, road, shocks = split(everything)
    labels = labels_of(everything)
    _warn_trailing(opts.infile, stats)
    if not records:
        print("Leeres Log.")
        return 0
    first, last = records[0], records[-1]
    with_fix = [r for r in records if r.gps_valid]
    fresh = [r for r in with_fix if r.gps_fix_age_ms <= 5000]
    print("Datei:        %s" % opts.infile)
    print("Format:       v%d, %d Byte/Datensatz" % (stats.version, stats.record_size))
    print("Datensätze:   %d Fahrdaten%s" % (len(records), ", %d Wegequalität, %d Stoß/Stöße" % (len(road), len(shocks))
                                            if stats.version >= 2 else ""))
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
    if stats.unknown_type:
        print("              %d Datensatz/Datensätze unbekannten Typs übersprungen "
              "(neuere Firmware?)" % stats.unknown_type)
    if road:
        rated = [r for r in road if r.rated]
        dist = sum(r.distance_m for r in road)
        print("Wegequalität: %d Intervalle, %d bewertet%s" % (
            len(road), len(rated),
            "" if not any(r.flags & IF_UNCALIBRATED for r in road)
            else " (noch keine Referenzfahrt -- Klassen beruhen auf der Standard-Baseline)"))
        by_class: dict[int, float] = {}
        for r in road:
            by_class[r.road_class] = by_class.get(r.road_class, 0.0) + r.distance_m
        for cls in sorted(by_class):
            share = by_class[cls] / dist * 100 if dist else 0.0
            print("              Klasse %d (%s): %.2f km (%.0f %%)" % (
                cls, ROAD_CLASS_NAMES.get(cls, "?"), by_class[cls] / 1000.0, share))
        slow = sum(1 for r in road if r.flags & (IF_TOO_SLOW | IF_NO_SPEED))
        if slow:
            print("              %d Intervall(e) zu langsam oder ohne Geschwindigkeit" % slow)
    if labels:
        _print_labels(labels, road)
    if shocks or road:
        suppressed = sum(r.events_suppressed for r in road)
        by_sev = {s: sum(1 for x in shocks if x.severity == s) for s in (1, 2, 3)}
        print("Stöße:        %d (Schwere 1/2/3: %d/%d/%d)%s" % (
            len(shocks), by_sev[1], by_sev[2], by_sev[3],
            ", %d wegen Ratenlimit nicht geloggt" % suppressed if suppressed else ""))
        seqs = [x.event_seq for x in shocks]
        missing = (max(seqs) - min(seqs) + 1 - len(set(seqs))) if seqs else 0
        if missing > 0:
            print("              %d Stoß-Datensatz/-sätze fehlen (Lücken in der Nummerierung)" % missing)
        for x in sorted(shocks, key=lambda e: e.peak_total_mg, reverse=True)[:5]:
            where = "%.5f, %.5f" % (x.latitude, x.longitude) if x.gps_valid else "ohne Position"
            print("              %s  %.1f g  Schwere %d  %s%s" % (
                x.utc().isoformat(sep=" ", timespec="seconds"), x.peak_g, x.severity, where,
                "  (Vorder- + Hinterrad)" if x.wheelbase_match else ""))
    return 0


def _print_labels(labels, road) -> None:
    """Manual labels against the automatic classification: per label, the distance
    and how the intervals under it were classified -- the ground-truth comparison."""
    changes = sum(1 for lab in labels if lab.reason == 0)
    print("Labels:       %d Datensätze, %d Wechsel" % (len(labels), changes))
    groups: dict[tuple[int, int], list] = {}
    for r in road:
        key = label_at(labels, r.time)
        if key != (0, 0):
            groups.setdefault(key, []).append(r)
    for (surf, q), rs in sorted(groups.items()):
        dist = sum(r.distance_m for r in rs)
        rated = [r for r in rs if r.rated]
        by_cls = {c: sum(1 for r in rated if r.road_class == c) for c in range(1, 6)}
        rough = [r.roughness for r in rated if r.roughness is not None]
        print("              %-9s Q%s: %.2f km, %d Intervalle, R Median %s, Klassen 1-5: %s" % (
            SURFACE_NAMES.get(surf, "?") or "-", q or "-", dist / 1000.0, len(rs),
            "%.2f" % sorted(rough)[len(rough) // 2] if rough else "-",
            "/".join(str(by_cls[c]) for c in range(1, 6))))


def cmd_fixture_synth(opts) -> int:
    records = fixtures.synthetic(
        seconds=opts.seconds,
        start_lat=opts.lat, start_lon=opts.lon,
        pause=None if opts.no_pause else (120, 300),
        tunnel=None if opts.no_tunnel else (60, 90),
        no_fix_start_s=opts.no_fix_start,
        unset_clock_records=opts.unset_clock,
    )
    if opts.roadq:
        if opts.format_version < 2:
            raise SystemExit("--roadq braucht Format v2")
        records = fixtures.with_road_quality(records)
    count = fixtures.write_bin(opts.outfile, records, opts.format_version)
    print("%d synthetische(r) Datensatz/Datensätze (Format v%d) -> %s" % (count, opts.format_version, opts.outfile))
    return 0


def cmd_fixture_from_gpx(opts) -> int:
    records = fixtures.from_gpx(opts.gpxfile)
    if not records:
        raise SystemExit("Keine <trkpt> mit <time> in %s gefunden" % opts.gpxfile)
    count = fixtures.write_bin(opts.outfile, records)
    print("%d Datensatz/Datensätze aus %s (Format v%d) -> %s" % (count, opts.gpxfile, CURRENT_VERSION, opts.outfile))
    return 0


def _read_raw(path: str, stats: raw.RawStats):
    try:
        return raw.read_raw(path, stats)
    except raw.NotARawFile as exc:
        raise SystemExit("%s: %s" % (path, exc))
    except FileNotFoundError:
        raise SystemExit("Datei nicht gefunden: %s" % path)


def cmd_raw_info(opts) -> int:
    stats = raw.RawStats()
    header, blocks = _read_raw(opts.infile, stats)
    kind = "Stoß-Ausschnitte" if header.kind == raw.KIND_SNIPPETS else "Mitschnitt"
    print("Datei:        %s" % opts.infile)
    print("Art:          %s (Rohformat v%d), %d Hz, ±%d g, Skala %.4f" % (
        kind, header.version, header.odr_hz, header.range_g, header.scale))
    print("Start:        %s" % raw.utc_ms(header.start_epoch_ms).isoformat(sep=" ", timespec="milliseconds"))
    print("Blöcke:       %d, %d Frames = %.1f s%s" % (
        stats.blocks, stats.frames, stats.frames / header.odr_hz,
        ", %d mit Lücke davor" % stats.gaps if stats.gaps else ""))
    if stats.resyncs or stats.trailing_bytes:
        print("              %d beschädigte Stelle(n) übersprungen, %d Byte angebrochen am Ende"
              % (stats.resyncs, stats.trailing_bytes))
    speeds = [b.speed_kmh for b in blocks if b.speed_kmh is not None]
    if speeds:
        print("Speed:        %.1f..%.1f km/h" % (min(speeds), max(speeds)))
    if header.kind == raw.KIND_CAPTURE and any(b.label_surface or b.label_quality for b in blocks):
        # consecutive blocks with the same manual label
        print("Labels:")
        seg = None
        for b in blocks + [None]:
            key = None if b is None else (b.label_surface, b.label_quality)
            if seg and key != seg[0]:
                (surf, q), start, frames = seg
                print("              ab %8.1f s  %.1f s  %s Q%s" % (
                    max(0, start - header.start_epoch_ms) / 1000.0, frames / header.odr_hz,
                    SURFACE_NAMES.get(surf, "?") or "-", q or "-"))
                seg = None
            if b is not None:
                if seg is None:
                    seg = [key, b.epoch_ms, 0]
                seg[2] += b.count
    if header.kind == raw.KIND_SNIPPETS:
        for b in blocks:
            g = b.g(header)
            peak = max(max(abs(x - header.g0[0] * header.scale), abs(y - header.g0[1] * header.scale),
                           abs(z - header.g0[2] * header.scale)) for x, y, z in g) if g else 0.0
            print("              Stoß #%d  %s  %d Frames, max. Abweichung von g0 %.1f g" % (
                b.ref, raw.utc_ms(b.epoch_ms).isoformat(sep=" ", timespec="milliseconds"), b.count, peak))
    return 0


def cmd_raw_csv(opts) -> int:
    stats = raw.RawStats()
    header, blocks = _read_raw(opts.infile, stats)
    period = 1000.0 / header.odr_hz
    rows = 0
    with open(opts.outfile, "w", newline="", encoding="utf-8") as fh:
        writer = csvexport.csv.writer(fh, dialect="excel")
        writer.writerow(("t_ms", "Block", "Ref", "x_g", "y_g", "z_g", "Speed", "Untergrund", "Qualitaet_manuell"))
        for n, b in enumerate(blocks):
            t0 = b.epoch_ms - header.start_epoch_ms
            speed = "" if b.speed_kmh is None else str(round(b.speed_kmh, 2)).replace(".", ",")
            surf = SURFACE_NAMES.get(b.label_surface, b.label_surface) if b.label_surface else ""
            qual = b.label_quality or ""
            for i, (x, y, z) in enumerate(b.g(header)):
                writer.writerow(("%.1f" % (t0 + i * period), n, b.ref,
                                 ("%.4f" % x).replace(".", ","), ("%.4f" % y).replace(".", ","),
                                 ("%.4f" % z).replace(".", ","), speed, surf, qual))
                rows += 1
    print("%d Frames -> %s" % (rows, opts.outfile))
    return 0


def _params(pairs: list[str]) -> dict[str, float]:
    params = {}
    for pair in pairs:
        key, sep, value = pair.partition("=")
        if not sep or key not in replay.PARAMS:
            raise SystemExit("Parameter '%s' unbekannt -- möglich: %s" % (pair, ", ".join(replay.PARAMS)))
        params[key] = float(value)
    return params


def cmd_raw_replay(opts) -> int:
    try:
        intervals, shocks, summary = replay.run(opts.infile, _params(opts.param))
    except replay.ReplayUnavailable as exc:
        raise SystemExit(str(exc))
    print("Replay:       %s (%s)" % (opts.infile, summary))
    if intervals:
        rated = [i for i in intervals if i.road_class]
        print("Intervalle:   %d, davon %d bewertet" % (len(intervals), len(rated)))
        for cls in sorted({i.road_class for i in intervals}):
            n = sum(1 for i in intervals if i.road_class == cls)
            print("              Klasse %d (%s): %d" % (cls, ROAD_CLASS_NAMES.get(cls, "?"), n))
        labelled: dict[tuple[int, int], list] = {}
        for i in rated:
            if i.label_surface or i.label_quality:
                labelled.setdefault((i.label_surface, i.label_quality), []).append(i)
        for (surf, q), items in sorted(labelled.items()):
            rs = sorted(i.roughness for i in items if i.roughness is not None)
            print("              Label %-9s Q%s: %d Intervalle, R Median %s, Klassen 1-5: %s" % (
                SURFACE_NAMES.get(surf, "?") or "-", q or "-", len(items),
                "%.2f" % rs[len(rs) // 2] if rs else "-",
                "/".join(str(sum(1 for i in items if i.road_class == c)) for c in range(1, 6))))
        rms = sorted(i.rms_vert_g for i in intervals)
        print("RMS vertikal: min %.0f / Median %.0f / max %.0f mg" % (
            rms[0] * 1000, rms[len(rms) // 2] * 1000, rms[-1] * 1000))
    print("Stöße:        %d" % len(shocks))
    for s in shocks:
        print("              t=%8.0f ms  #%d  %.2f g  Schwere %d%s" % (
            s.t_ms, s.ref, s.peak_g, s.severity,
            "  2. Peak %.2f g nach %.0f ms" % (s.second_g, s.second_delay_ms) if s.second_g else ""))
    if opts.rq_out and intervals:
        with open(opts.rq_out, "w", newline="", encoding="utf-8") as fh:
            writer = csvexport.csv.DictWriter(fh, fieldnames=list(replay.ReplayInterval.__dataclass_fields__), dialect="excel")
            writer.writeheader()
            for i in intervals:
                writer.writerow(csvexport.localize_floats({k: ("" if v is None else v) for k, v in vars(i).items()}))
        print("-> %s" % opts.rq_out)
    if opts.shocks_out and shocks:
        with open(opts.shocks_out, "w", newline="", encoding="utf-8") as fh:
            writer = csvexport.csv.DictWriter(fh, fieldnames=list(replay.ReplayShock.__dataclass_fields__), dialect="excel")
            writer.writeheader()
            for s in shocks:
                writer.writerow(csvexport.localize_floats(vars(s)))
        print("-> %s" % opts.shocks_out)
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
    p_csv.add_argument("--roadq", action="store_true",
                       help="Spalten Wegeklasse/Gradient_Baro/Gradient_IMU zusätzlich ausgeben")
    p_csv.add_argument("--roadq-out", metavar="FILE",
                       help="Wegequalitäts-Intervalle in diese CSV schreiben")
    p_csv.add_argument("--shocks-out", metavar="FILE",
                       help="Stöße in diese CSV schreiben")
    p_csv.add_argument("--labels-out", metavar="FILE",
                       help="manuelle Wege-Labels (Untergrund/Qualität) in diese CSV schreiben")
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
    p_gpx.add_argument("--no-shocks", action="store_true",
                       help="Stöße nicht als Wegpunkte ausgeben")
    p_gpx.add_argument("--min-severity", type=int, choices=(1, 2, 3), default=1,
                       help="Stöße ab dieser Schwere als Wegpunkt [Standard: %(default)s]")
    p_gpx.add_argument("--no-labels", action="store_true",
                       help="Wechsel der manuellen Wege-Labels nicht als Wegpunkte ausgeben")
    p_gpx.add_argument("--plain", action="store_true",
                       help="nur Garmin-TrackPointExtension, ohne die eigenen Erweiterungen "
                            "(Steigung, Wegequalität, Labels, Zusammenfassung)")
    p_gpx.add_argument("--device", help="Gerätename für <src> und die Zusammenfassung")
    p_gpx.set_defaults(func=cmd_gpx)

    p_info = sub.add_parser("info", help="Binärlog zusammenfassen")
    p_info.add_argument("-i", "--in", dest="infile", required=True, metavar="FILE")
    p_info.set_defaults(func=cmd_info)

    p_raw = sub.add_parser("raw", help="Rohdaten des Beschleunigungssensors (R_*.bin, S_*.bin)")
    raw_sub = p_raw.add_subparsers(dest="raw_command", required=True)
    p_rinfo = raw_sub.add_parser("info", help="Übersicht über eine Rohdatendatei")
    p_rinfo.add_argument("-i", "--in", dest="infile", required=True, metavar="FILE")
    p_rinfo.set_defaults(func=cmd_raw_info)
    p_rcsv = raw_sub.add_parser("csv", help="Frames als CSV (in g, skaliert)")
    p_rcsv.add_argument("-i", "--in", dest="infile", required=True, metavar="FILE")
    p_rcsv.add_argument("-o", "--out", dest="outfile", default="./raw.csv", metavar="FILE")
    p_rcsv.set_defaults(func=cmd_raw_csv)
    p_rrep = raw_sub.add_parser(
        "replay", help="Firmware-Algorithmus auf die Aufnahme anwenden (baut rq_replay mit g++)",
        epilog="Parameter: " + "; ".join("%s = %s" % kv for kv in replay.PARAMS.items()))
    p_rrep.add_argument("-i", "--in", dest="infile", required=True, metavar="FILE")
    p_rrep.add_argument("param", nargs="*", metavar="KEY=VALUE", help="Algorithmus-Parameter, z. B. shock=2.5")
    p_rrep.add_argument("--rq-out", metavar="FILE", help="Intervalle als CSV")
    p_rrep.add_argument("--shocks-out", metavar="FILE", help="Stöße als CSV")
    p_rrep.set_defaults(func=cmd_raw_replay)

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
    p_synth.add_argument("--roadq", action="store_true",
                         help="Wegequalitäts-Intervalle und Stöße einstreuen")
    p_synth.add_argument("--format-version", type=int, choices=(1, 2), default=CURRENT_VERSION,
                         help="Binärformat [Standard: %(default)s]")
    p_synth.set_defaults(func=cmd_fixture_synth)

    p_from = fix_sub.add_parser("from-gpx", help="Binärlog aus einer GPX-Datei bauen")
    p_from.add_argument("gpxfile", metavar="GPX")
    p_from.add_argument("-o", "--out", dest="outfile", required=True, metavar="FILE")
    p_from.set_defaults(func=cmd_fixture_from_gpx)

    return parser


def main(argv=None) -> int:
    opts = build_parser().parse_args(argv)
    return opts.func(opts)
