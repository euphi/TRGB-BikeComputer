#!/usr/bin/python3
"""Convert the binary datalog files written by the bike computer into CSV.

The binary format is BCLogger::LogData (src/BCLogger.h) -- a sequence of
fixed-size records, no file header. Since the GPS-era change the records
carry a format version byte, so this script dispatches on that byte instead
of assuming one fixed layout: see LAYOUTS below when a new version is added
firmware-side.

Step 1 (this version) writes exactly the columns the pre-GPS script wrote.
The GPS fields are parsed and kept in every record, but not exported yet --
they are the input for the planned GPX/FIT export.
"""

__version__ = 0.2
__date__ = '2022-11-21'
__updated__ = '2026-09-20'

import argparse
import csv
import os
import struct
import sys

# --- Binary layouts -------------------------------------------------------
#
# Keyed by BCLogger::LOG_DATA_FORMAT_VERSION. '<' (little endian, packed)
# matches the ESP32-S3 layout: the firmware struct happens to have no padding
# gaps, which BCLogger.h asserts via static_assert on sizeof(LogData).
#
# 'version_offset' is where the format version byte sits inside the record.
# It has to be known before the record can be unpacked, which is why it is
# listed separately rather than derived from the field list.


class Layout:
    def __init__(self, fmt, names, version_offset):
        self.fmt = fmt
        self.names = names
        self.version_offset = version_offset
        self.size = struct.calcsize(fmt)

    def unpack(self, raw):
        return dict(zip(self.names, struct.unpack(self.fmt, raw)))


LAYOUTS = {
    # v1: time_t is 8 byte on this toolchain, followed by 5 floats, 4 single
    # bytes and the GPS block.
    1: Layout(
        '<q5f4B3iIHHI',
        ('Timestamp', 'Speed', 'Temperatur', 'Gradient', 'Höhe', 'distance',
         'Puls', 'Cadence', 'gpsFlags', 'formatVersion',
         'gpsLatitudeE7', 'gpsLongitudeE7', 'gpsAltitudeM',
         'gpsSpeedCms', 'gpsBearingDegX100', 'gpsAccuracyMX10',
         'gpsFixAgeMs'),
        version_offset=31,
    ),
}

# The columns exported in this step -- same set, same order as the old script.
CSV_COLUMNS = ('Timestamp', 'Speed', 'Temperatur', 'Gradient', 'Höhe',
               'distance', 'Puls', 'Cadence')

# GPS flag bits, mirroring BCLogger::LogDataGpsFlags. Unused for CSV export,
# but needed as soon as the GPX export lands.
LOG_GPS_VALID = 0x01
LOG_GPS_HAS_ALTITUDE = 0x02
LOG_GPS_HAS_SPEED = 0x04
LOG_GPS_HAS_BEARING = 0x08
LOG_GPS_HAS_ACCURACY = 0x10


def localize_floats(row):
    """Write floats with a comma as decimal separator (German locale)."""
    return dict(
        (key, str(value).replace('.', ',')) if isinstance(value, float)
        else (key, value)
        for key, value in row.items()
    )


def detect_layout(raw):
    """Pick the layout matching the format version byte of a record."""
    for version, layout in LAYOUTS.items():
        if len(raw) > layout.version_offset and raw[layout.version_offset] == version:
            return version, layout
    raise SystemExit(
        "Unknown log format: no layout in LAYOUTS matches this file. "
        "If the firmware bumped LOG_DATA_FORMAT_VERSION (see src/BCLogger.h), "
        "add the new layout here."
    )


def read_records(path, verbose=0):
    """Yield one dict per record; stops on a short (truncated) trailing read."""
    layout = None
    with open(path, 'rb') as fh:
        head = fh.read(max(l.version_offset for l in LAYOUTS.values()) + 1)
        if not head:
            return
        version, layout = detect_layout(head)
        if verbose:
            print("Log format version %d, %d bytes per record"
                  % (version, layout.size), file=sys.stderr)
        fh.seek(0)
        index = 0
        while True:
            raw = fh.read(layout.size)
            if len(raw) < layout.size:
                if raw:
                    print("Warning: %d trailing byte(s) after record %d ignored "
                          "(file truncated?)" % (len(raw), index), file=sys.stderr)
                return
            yield layout.unpack(raw)
            index += 1


def to_excel_timestamp(unix_timestamp):
    """Unix epoch seconds -> Excel/LibreOffice day serial."""
    return unix_timestamp / (60 * 60 * 24) + 25569


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog=os.path.basename(sys.argv[0]),
        description="Convert binary datalog files from the bike computer to CSV",
    )
    parser.add_argument('--version', action='version',
                        version='%%(prog)s v%s (%s)' % (__version__, __updated__))
    parser.add_argument('-i', '--in', dest='infile', default='./LOG_0160.BIN',
                        metavar='FILE', help="input path [default: %(default)s]")
    parser.add_argument('-o', '--out', dest='outfile', default='./out.csv',
                        metavar='FILE', help="output path [default: %(default)s]")
    parser.add_argument('-t', '--timestamp', dest='starttime', type=int, default=0,
                        help="timestamp of first entry [default: use data timestamp]")
    parser.add_argument('-v', '--verbose', dest='verbose', action='count', default=0,
                        help="raise verbosity level")
    parser.add_argument('-a', '--add', dest='add_flag', action='store_true',
                        help="add the timestamp instead of using it as the first one")
    opts = parser.parse_args(argv)

    if opts.verbose:
        print("infile = %s, outfile = %s, starttime = %s"
              % (opts.infile, opts.outfile, opts.starttime), file=sys.stderr)

    start_timestamp = 0
    count = 0
    with open(opts.outfile, 'w', newline='') as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=CSV_COLUMNS,
                                extrasaction='ignore', dialect='excel')
        writer.writeheader()
        for record in read_records(opts.infile, opts.verbose):
            if opts.verbose > 1:
                print(record, file=sys.stderr)
            if opts.starttime > 0:
                if start_timestamp == 0 and not opts.add_flag:
                    start_timestamp = record['Timestamp']
                record['Timestamp'] = (opts.starttime + record['Timestamp']
                                       - start_timestamp)
            record['Timestamp'] = to_excel_timestamp(record['Timestamp'])
            writer.writerow(localize_floats(record))
            count += 1
    print("%d record(s) written to %s" % (count, opts.outfile))
    return 0


if __name__ == '__main__':
    sys.exit(main())
