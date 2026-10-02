# Tools: evaluating the binary logs

The bike computer writes its measurements as a sequence of fixed binary records to the
SD card (format v2, see
[`src/LogRecords.h`](https://github.com/euphi/TRGB-BikeComputer/blob/main/src/LogRecords.h)).
[`Tools/`](https://github.com/euphi/TRGB-BikeComputer/tree/main/Tools) holds everything
that works with them offline.

| Format | Record size | Content |
|---|---|---|
| v1 | 56 bytes | ride data only (every 5 s) -- older logs, still read |
| v2 | 64 bytes | type byte at offset 30: 0 = ride data, 1 = road quality (per interval, 1..10 s), 2 = shock, 3 = manual road label, 4 = ride state |

The version number is in every record at offset 31. Unknown record types are skipped
(and counted in `bikelog info`).

**Manual road labels** (type 3, added without a new version): surface (1 asphalt,
2 gravel, 3 forest track, 4 field track, 5 cobbles, 6 other) and quality 1 (best) to 4,
set on the road-label screen or by `rq label <surface>,<quality>`. One record at every
change, repeated every 60 s to be safe and at start/stop of a raw capture; the label
applies until the next record. It is also in every shock record and every raw data block.
`bikelog info` compares the labels with the automatic class (distance, median roughness
and class distribution per label). The mapping to OSM `surface`/`smoothness` is in
`bikelog/record.py` (`SURFACE_OSM`, `LABEL_QUALITY_OSM`).

```
bikelog/            library + CLI (standard library only, no venv needed)
BikeLogService/     service: fetches sessions from the device (mDNS), GPX/CSV (FastAPI, own venv)
pyproject.toml      makes Tools/ installable (for the service, see BikeLogService/install.sh)
tests/              pytest suite for both
ReadTachoBin.py     the well-known CSV converter, now a wrapper around "bikelog csv"
csv2influx.py       unchanged: CSV to InfluxDB
gpxenrich/          GPX track -> route for TrailBridge: turn hints from BRouter, waypoints as
                    named hints (description in the header of gpx_enrich.py)
```

## Files on the SD card

Every boot is a session. It first writes to the working directory `/BIKECOMP/CUR/`,
named by a running number -- at boot the clock is usually not set yet:

| File | Content |
|---|---|
| `L_0042.bin` | binary log (this format) |
| `D_0042.log`, `N_0042.log` | debug log, Forumslader NMEA |
| `S_0042.bin`, `R_0042_NN.bin` | raw data, see below |
| `T_0042.txt` | time hints: `start <epochMs> <valid 0/1> <uptimeMs>`, then per clock step `step <ntp\|gps\|?> <offsetMs> <newEpochMs> <uptimeMs>` |

After the next boot a background task (`src/LogSessions.cpp`) clears every older session
out of `CUR/`:

- Start time from `T_*`; if the session started without a clock (1970), it is corrected
  by the step with which NTP or the GPS time from TrailBridge made the clock valid.
- Target `/BIKECOMP/YYYYMMDD/X_HHMMSS.*`, without a determinable time
  `/BIKECOMP/NO_TIME/X_0042.*`. In `L_*.bin` the 1970 timestamps are rewritten; raw data
  and debug log keep their original times.
- `I_HHMMSS.txt`: short statistics as `key=value` lines (distance, duration, moving time,
  average/maximum speed, share of GPS, number of road-quality intervals, shocks, labels,
  raw captures; `corr_ms` = applied time correction). The log file page of the web server
  shows them per session.
- Empty files are deleted.

Older logs (from before this change) still lie as `L0042.bin` in `NO_TIME/` or dated
without `I_*.txt`.

## CLI

Without installation, directly from `Tools/`:

```bash
python3 -m bikelog info -i /path/L0001.bin          # overview of a log
python3 -m bikelog csv  -i /path/L0001.bin -o out.csv [--gps] [--roadq]
python3 -m bikelog csv  -i /path/L0001.bin -o out.csv --roadq-out roads.csv --shocks-out shocks.csv --labels-out labels.csv
python3 -m bikelog gpx  -i /path/L0001.bin -o tour.gpx [--no-shocks] [--min-severity 2]
./ReadTachoBin.py -i /path/L0001.bin -o out.csv     # as before
```

### GPX export

"Rich" by default: besides Garmin's TrackPointExtension v2 (heart rate, cadence,
temperature, speed, course) every track point carries an extension of its own with
gradient, road quality, label etc., label changes become waypoints, the metadata contains
the key figures of the ride -- see the [log service](LOGSERVICE.md#gpx-export). `--plain`
leaves out the own extensions, `--no-labels` the label waypoints.

The XML is the easy part; what matters is what does *not* go in:

| Filter | Default | Option |
|---|---|---|
| records without a GPS fix | always out | -- |
| stale fixes (heartbeat repetition in a tunnel) | > 5000 ms out | `--max-fix-age` |
| timestamps before 2020 (clock not set by NTP) | always out | -- |
| position exactly (0, 0) | always out | -- |
| fixes that are too inaccurate | off | `--max-accuracy` |
| time gap → new `<trkseg>` | > 60 s | `--segment-gap` |

Altitude source via `--ele=auto|baro|gps`; `auto` takes the barometer if its value is
plausible, otherwise the GPS altitude. Heart rate 0 and 255 mean "no value" and are left
out.

Shocks (v2) are output as `<wpt>` -- name `Stoß 5,2 g`, `<type>` `shock-1..3` by
severity, in `<desc>` the second peak (rear wheel) and the speed. The same position and
time filters apply to them as to the track points. The road-quality intervals themselves
(with all fields) are still only available as CSV (`--roadq-out`); in the GPX every track
point carries class and roughness of the interval it falls into.

Every run reports what was discarded:

```
195/243 Punkte in 2 Segment(en), 20 verworfen (ohne Fix), 25 verworfen (veraltet)
```

### Raw data of the accelerometer

Next to the log of a session `L_HHMMSS.bin` there can be two raw data files (format:
[`src/RawCapture.h`](https://github.com/euphi/TRGB-BikeComputer/blob/main/src/RawCapture.h),
400 Hz, raw LSB):

| File | Created | Size |
|---|---|---|
| `R_HHMMSS_NN.bin` | on demand: `rq raw <s>` (serial), buttons on `/debug/imu` or the record button of the road-label screen, up to 1800 s | about 150 KB/min |
| `S_HHMMSS.bin` | always: 0.25 s before to 0.5 s after every logged shock | about 1.8 KB/shock |

```bash
python3 -m bikelog raw info   -i R_143012_01.bin
python3 -m bikelog raw csv    -i R_143012_01.bin -o raw.csv          # frames in g
python3 -m bikelog raw replay -i R_143012_01.bin shock=2.5 interval=1 --rq-out rq.csv
python3 -m bikelog raw replay -i S_143012.bin                        # recompute every shock
```

`replay` computes with the **firmware code itself** (`src/RoadQuality.cpp`), not with a
Python imitation: on the first call it builds
[`rqreplay/rq_replay.cpp`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/rqreplay/rq_replay.cpp)
with g++ into `.build/` and passes the parameters (`python3 -m bikelog raw replay -h`
lists them). This way thresholds and filters can be tried on real recordings before they
go into the firmware. The manual label of every block runs along: `raw info` lists the
label sections, `raw replay` shows roughness and classes per label -- the direct
comparison of "felt" against "measured".

### Test data without hardware

```bash
python3 -m bikelog fixture synth -o ride.bin --unset-clock 3 [--roadq]
python3 -m bikelog fixture from-gpx tour.gpx -o ride.bin
```

`synth` deliberately produces the unpleasant cases (start without a fix, tunnel with
stale fixes, break as a time gap, unset clock). `from-gpx` builds a binary log with
realistic geometry from a real tour -- e.g. a Komoot export -- as long as there are no
real recordings yet.

## Tests

```bash
python3 -m venv .venv
.venv/bin/pip install -r BikeLogService/requirements.txt
.venv/bin/python -m pytest
```

`tests/test_logformat.py` pins the binary layout to the firmware:
`test_layout_matches_firmware_header` compiles `src/LogRecords.h` with g++ on the host
([`logrecords_dump.cpp`](https://github.com/euphi/TRGB-BikeComputer/blob/main/test/native_roadquality/logrecords_dump.cpp)),
has one record per type written and reads it back. If it fails, the firmware structs have
changed and `bikelog/record.py` needs new entries in `FORMATS` (and the firmware a new
`FORMAT_VERSION`). Without g++ the test is skipped.
