# BikeLog service

Fetches the bike computer's sessions automatically as soon as it shows up in the WiFi,
keeps them unchanged and serves GPX/CSV made from them.

A **session** is everything one boot of the bike computer writes: `L_` (binary log), `I_`
(short statistics), `D_` (debug log), `S_`/`R_` (raw IMU data), `T_` (time hints), `N_`
(Forumslader NMEA) -- all with the same stem (`_143012`), see
[log format and CLI](TOOLS.md). In the service the files lie in the same structure as on
the SD card:

```
data/sessions/<device>/20260920/L_143012.bin    (files directly in /BIKECOMP: folder "_")
data/index.sqlite3                              index (sessions, files, hashes, key figures)
```

Clock steps inside a session (the phone's GPS time was wrong when the bike computer
started, 2026-10-04: a 2 h ride became 17 h with an hour of the evening before) are undone
when reading: the `T_` file lists the steps, the jumps of the record times show where they
happened, the last clock counts
([`bikelog.timefix`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/timefix.py)).
The summary of such a session shows `time=repaired`.

GPX, CSV etc. are generated from the raw data on demand -- a better export logic thus
improves all old rides retroactively.

## Fetching (pull)

The bike computer serves its SD card by HTTP anyway (`/log/<day>/<file>`). The service
fetches instead of the firmware uploading: no upload client on the device, no credentials
there, no retry logic in a task that competes with BLE and display for internal heap.

**When**
([`puller.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/puller.py)):

- **mDNS probes** (main trigger): after every WiFi connect the firmware calls
  `MDNS.begin("TRGB-BC")`, and the ESP32 *probes* its name while doing so (RFC 6762 8.1:
  queries for `TRGB-BC.local` with an authority section). Only a device that is just
  (newly) joining the network does that -- i.e. after boot or WiFi reconnect, exactly
  when a session has been finished. A socket of its own listens passively for it; 10 s
  later the fetch runs (`BOOT_SETTLE_S`). Measured 2026-09-27: four probes within one
  second after the connect.
- The ServiceBrowser alone is **not** enough: it only reports services it does not have
  in its cache yet, and after a quick restart the entry is still there (PTR TTL 75 min).
  That is how the first attempt on 2026-09-27 failed.
- **Polling** as a safety net: an mDNS address query every `BIKELOG_PULL_INTERVAL_S`
  (120 s) -- if the bike computer is gone this costs nothing, if it (re)appears the fetch
  runs. Additionally every 10 min at the latest while it is online (the bike computer finishes a
  session when the ride session is ended, see below).
- **Follow-up**: right after boot `LogSessions` only moves the finished session out of
  `CUR/` in the background. If there is more than the running session there at the time
  of the fetch (or the fetch fails), it is fetched again after 60 s, at most 10 times in
  a row.
- By hand: `POST /api/v1/pull` or `bikelogservice pull`.

**What**: every file of the list that is still missing or whose size has changed --
except `CUR/` (running session) and deleted sessions. New sessions only come into being
on the bike computer when the ride session is ended on purpose (long press), at boot, or when
`rotate` is typed on the serial console -- not when WiFi connects, which also happens on a phone
hotspot in the middle of a tour, a fetch without anything new is a single request. The list
comes from `/logfiles.json` if the firmware has the endpoint, otherwise from the HTML
page `/logfiles/` (there without sizes: a known file is then not fetched again).

Measured 2026-09-27: first fetch 390 files / 23 MB in 3.5 min (≈110 kB/s, the ESP32 is
the brake), follow-up fetch 2 s.

**Deleting** removes the files but leaves a tombstone in the index -- otherwise the
session would come back from the SD card on the next fetch. An explicit `PUT` brings it
back.

## GPX export

For every session with a binary log the service automatically writes a file under
`data/export/gpx/` (local start time + device; mtime = start of the ride) -- as the
hand-over point for Nextcloud sync, Komoot/Strava etc.
([`exporter.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/exporter.py)):

- `Tours/` -- real rides, at least `MIN_EXPORT_DISTANCE_M` (1 km) of real movement (GPS
  jumps around a standing position don't count, see
  `bikelog.gpx.GpxStats.real_distance_m`).
- `Debug_Archive/` -- everything else with a usable GPS fix: short test rides, a bike
  standing on the trainer with jittering phone GPS, and every [test session](#test-sessions)
  however long (`gpx_status` `test`). Nothing is lost, it just does not end up in
  Nextcloud/Strava/Komoot.

Sessions without any usable GPS fix get no file at all (`gpx_status` `no-gps`),
unreadable logs `error: …`.

It is rewritten (and moved between the two folders if necessary) when `L_` or `I_` of the
session arrives or changes, and for **all** sessions when `EXPORT_VERSION` is raised --
do that with every change to the content of the GPX, then old rides get the better
version too.

Before the export,
[`bikelog/sanitize.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/sanitize.py)
runs over the raw data (on by default, `GpxOptions.sanitize` or `?sanitize=false` on the
download): at standstill (traffic light, break, before the first revolution) the GPS fix
wanders a few metres around the position -- sanitize recognises that by the wheel sensor
(`speed < MOVING_KMH`) and leaves only the one fix directly at the break from every
standstill, instead of a scribble on the map.

Content (details in
[`bikelog/gpx.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/gpx.py)):

| Where | What |
|---|---|
| `<metadata>` | name, description with key figures (distance, moving time, average/maximum, altitude gain, heart rate, cadence, temperature, shocks, road quality, labels), link to the session (`BIKELOG_PUBLIC_URL`), bounds |
| `<metadata><extensions><bc:Ride>` | the same key figures machine-readable, distance per road class and per label, plus the device's `I_` statistics unchanged (`bc:deviceSummary`) |
| track point, Garmin TPX v2 | `atemp`, `hr`, `cad`, `speed`, `course` -- read by Strava, Komoot, Garmin Connect |
| track point, `bc:TrackPoint` | trip distance, wheel speed, gradient (display/baro/IMU), barometric and GPS altitude, GPS accuracy and fix age, road class + OSM `smoothness`, roughness and RMS of the road-quality interval, manual label |
| `<wpt>` | shocks (`sym` "Danger Area", details in `bc:Shock`) and label changes (`sym` "Flag, Blue") |

`bc` = `https://github.com/euphi/TRGB-BikeComputer/gpx/v1`. Viewers skip unknown
extensions; the GPX validates against `gpx.xsd`, the TPX elements against
`TrackPointExtensionv2.xsd`. Lean, without `bc:`:
`GET /api/v1/sessions/{id}.gpx?rich=false` or `bikelog gpx --plain`.

By hand: `bikelogservice export` (missing/outdated), `export --all` (all).

## Komoot upload

Never automatic -- but the web page **asks**: every finished real ride that is neither
uploaded nor declined shows up in a box at the top, "New ride ready -- upload to Komoot?",
with the buttons *Zu Komoot hochladen* and *Nicht hochladen*. The question comes back on
every page view until one of the two is pressed (closing the page answers nothing);
declining is stored (`komoot_prompt = ignored`, per merged tour) and can be undone with
`POST /api/v1/sessions/{id}/komoot/ignore?ask_again=true`. A failed upload keeps asking.
Sessions that existed when this was introduced (schema 7) are not asked about; their
"Komoot" button in the table is still there.

While files are being fetched the page shows a progress box (file n of m, MB so far,
then "processing": GPX export, Nextcloud) and reloads itself every 3 s until done; the
journal logs every file, the number of sessions to fetch and each GPX export.

`POST /api/v1/sessions/{id}/komoot` uploads the ride to Komoot via
[kompy](https://github.com/Tsadoq/kompy) (or, in the web interface, the "Komoot" button
next to every real ride). A reboot in the middle of a ride (short BLE dropout, break with
the bike computer switched off) produces several sessions for one tour -- sessions of the
same device with at most `BIKELOG_KOMOOT_MERGE_GAP_S` of pause between them are therefore
merged into one ride and uploaded as one GPX
([`komoot.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/komoot.py));
all sessions involved show the same `komoot_status` afterwards. A second attempt without
`?force=true` is rejected with `409`. The rides list shows such a ride as one row with the
figures of the whole ride (distance, moving time, average) and its parts smaller below it
("Teil 1/2", no rule between them); the head row links to the report of the whole ride.

Needs `pip install "bikelog[komoot]"` (kompy + gpxpy, not part of `[service]`) and
`BIKELOG_KOMOOT_EMAIL`/`BIKELOG_KOMOOT_PASSWORD` in `bikelog.env` -- without both the
endpoint answers `409` ("not-configured"), everything else stays as it is. kompy does not
report a tour ID after the upload (limitation of the Komoot API), hence no direct link to
the new tour -- only the status (`uploaded`/`error`).

Unlike the normal export, the uploaded GPX has no `<wpt>` waypoints (shocks, label
changes): Komoot's import endpoint rejects every file with `<wpt>`
(`400 query is required for type=tour_planned`, verified against the real API on
2026-09-27 -- presumably their parser takes it for a planned route instead of a recording
for that reason alone). The rich `bc:` extensions per track point are not affected and
stay in.

## Nextcloud sync

Unlike Komoot: runs automatically, no button, no endpoint. Every session with
`gpx_status` `ok` (i.e. in `Tours/`, see above) is uploaded by WebDAV into a configurable
directory on a Nextcloud instance
([`nextcloud.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/nextcloud.py));
if a session is later no longer classified as "real" (export logic improved,
`EXPORT_VERSION` raised), its Nextcloud copy is deleted again instead of being left
orphaned. `Debug_Archive` sessions are never synchronised.

Needs `pip install "bikelog[nextcloud]"` (`requests`, not part of `[service]`) and in
`bikelog.env`:

```
BIKELOG_NEXTCLOUD_URL=https://cloud.example.com
BIKELOG_NEXTCLOUD_USER=bikelog
BIKELOG_NEXTCLOUD_PASSWORD=<app password>
BIKELOG_NEXTCLOUD_DIR=BikeLog          # default, freely selectable
```

The password is an **app password** (Nextcloud: Settings -> Security -> "Devices &
sessions" -> create a new app password), not the normal login password -- revocable on
its own without touching the main access. Without URL/user/password the sync simply stays
off.

Known gap: if a session is deleted (`DELETE /api/v1/sessions/{id}`), only the local copy
disappears -- the Nextcloud copy stays and has to be removed by hand.

## Test sessions

Sessions with emulated data are recognised and kept apart from the rides
([`bikelog/testride.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/testride.py)):

| Marker | Recognised by |
|---|---|
| **Simuliert** | `LOG_SIMULATED` in the log: the sensor simulator (`sim` on the serial console, simulator build) or a TrailBridge test ride (`SIM_FLAGS`, firmware since 2026-10-02) |
| **GPS-Wiedergabe?** | older TrailBridge test rides without the flag: GPS travels at least 500 m, the wheel sensor less than 15 % of that. A real ride with a dead wheel sensor looks the same, hence the question mark |

Test sessions are **hidden** in the ride list (and in `GET /api/v1/sessions`), the filter
says how many; "Testfahrten zeigen" shows them with their marker. They do not count in
training, recurring climbs or goals, go to `Debug_Archive/` and are never synchronised or
offered for Komoot. The ride page says why a session counts as a test and has a button
to overrule the detection in either direction ("Doch eine echte Fahrt", "Als Testfahrt
markieren"; API: `POST /api/v1/sessions/{id}/test?mark=test|real|auto`).

## Idle sessions and archive

Every boot of the bike computer is a session, also when it was only switched on at home.
A session in which the wheel stood and the position got nowhere (wheel < 50 m, all GPS
fixes within 300 m), or which has no binary log at all or only an empty one (nothing but
zero bytes), is **idle**
([`bikelog/testride.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/testride.py)).
A log in an unknown format, on the other hand, stays in the list marked "Log unlesbar"
(the hover text names the version byte found) and is not tried again for the report.
Idle sessions never appear in the ride list (it links to them: "N Leerlauf-Sitzungen im
Archiv") or in `GET /api/v1/sessions` (`idle=true` includes them), get no GPX and count
nowhere.

`BIKELOG_IDLE_ARCHIVE_DAYS` (default 7) days after they were fetched, their files move
from `<data>/sessions/` to `<data>/archive/` (same tree) -- debug logs stay available
with a shell
([`archive.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/archive.py)).
The index keeps a tombstone of them for as long as the files are still on the SD card,
otherwise the next pull would fetch them again; the pull drops it once they are gone
from the card's listing (this applies to deleted sessions as well).

The page **Archiv** (`/archive`) lists idle sessions not yet archived and archived ones
still on the card. "Auf dem BC löschen" (one session, or all) deletes their files on the
bike computer right away through the firmware's `/del/` endpoint -- only while it is
reachable; nothing is queued. A session whose files are all gone from the card is moved
to the archive and dropped from the index. API: `POST /api/v1/archive/device-delete`
(`session_id`, all if omitted; 409 when the device is not reachable).

## Bikes

The page **Räder** (`/bikes`) holds the bikes -- name, type, weight ready to ride, CdA,
Crr -- and which bike computer rides on which bike from which day on
([`bikelog/bikes.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/bikes.py),
stored in `<data>/bikes.json`). A session gets the bike its device was assigned to on the
day of the ride; on the ride page one ride can be given another bike. The power estimate
of the report then uses rider weight (page **Fahrer**) + bike weight, and the bike's CdA
and Crr; rides without a bike keep the rider page's defaults. Changing a bike or an
assignment recomputes the reports concerned. The page shows the kilometres per bike.

## Importing rides (GPX)

"GPX-Fahrten importieren" on the ride list takes rides recorded elsewhere (Garmin, Strava,
Komoot ...): GPX with times; heart rate, cadence and temperature from Garmin's
TrackPointExtension are taken over, the gradient is smoothed over ±50 m
([`bikelog/gpximport.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/gpximport.py)).
An imported ride is a session of the device `import` (the log made from it next to the
original GPX) and counts like any other ride -- report, training, climbs -- but is neither
exported nor uploaded. Optionally with a bike and as an **earlier edition of a goal**: the
goal then lists "Deine bisherigen Teilnahmen" with time, speed, heart rate, power and TRIMP.
Importing the same file again replaces the session. API: `PUT /api/v1/import/gpx`
(`bike_id`, `event_id`, body = the file).

## Session report

`/ride/{id}` (click on a ride in the list) shows the report of a session: key figures,
elevation profile with the climbs, heart-rate zones, estimated power, road quality and
shocks, technical findings. `GET /api/v1/sessions/{id}/report.md` is the same as German
Markdown, `…/report.json` the figures behind it. All of it comes from
[`bikelog/report.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/report.py)
(details: [tools](TOOLS.md#session-report)).

Reports are kept in the `reports` table of the index, under a key made of the report
version, the hash of the `L_` file and the rider data. After a pull (and at start) the
service computes whatever is missing or stale, at the lowest CPU priority
([`analysis.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/analysis.py)).
A new report version or changed rider data therefore reach every past ride by themselves.

Rider data for heart-rate zones, TRIMP and W/kg are entered on the page **Fahrer**
(`/athlete`) and end up in `<data>/athlete.json` (other path: `BIKELOG_ATHLETE_FILE`).
Every key is optional; without the file the report simply has no zones:

```json
{"hr_max": 186, "hr_rest": 48, "mass_kg": 88, "rider_kg": 76, "cda": 0.38, "crr": 0.006}
```

The JSON is meant as the input for a text generator (template or local LLM): every
number is computed there, a model only has to put it into words -- see
[the ride in words](#the-ride-in-words-local-llm).

## The ride in words (local LLM)

With `BIKELOG_LLM_URL` set (an [Ollama](https://ollama.com) server, e.g.
`http://localhost:11434`) the ride page gets a section **In Worten**: two or three
paragraphs on what kind of ride it was, how load and heart rate compare with the recent
rides, whether that fits the training phase of the next goal, and technical warnings.
Everything stays on the home server.

- The model only puts figures into words. The prompt
  ([`bikelog/narrate.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/narrate.py)) gives it labelled lines with spelled-out
  units and their meaning ("Höhenmeter bergauf: 480 m", "Form (TSB): -12 (negativ =
  ermüdet)"), plus the comparison with the rides of the 42 days before, the load on the
  morning of the ride and the next goal with its phase -- no abbreviations, nothing to
  compute. (In the first test qwen3:8b read "480 hm" as hectometres and added them to the
  distance.)
- Every number in the answer that is not in the prompt (allowing for rounding) is listed
  under the text: not proof of a mistake, a reason to look.
- Thinking is switched off (`"think": false`): qwen3 otherwise reasons for several hundred
  tokens first -- minutes on a CPU.
- Texts are written in the background after the reports, newest ride first, one at a time
  ([`llm.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/llm.py)), and kept in the table
  `narratives`. A ride without a text always gets one; if its facts change (new goal,
  another bike, new prompt) it is rewritten only for rides of the last
  `BIKELOG_LLM_REFRESH_DAYS`. **Neu schreiben** on the page writes it again, before any other.
  Test sessions and rides under 1 km get none.

On the Orange Pi 5 (16 GB, CPU only) `qwen3:8b` writes about 2.7 tokens/s, a text takes
2--4 minutes. A 14B model needs about 9 GB while loading (Ollama repacks the weights for
the ARM CPU) -- with other services on the machine that ends in the OOM killer. Recommended
Ollama settings (`sudo systemctl edit ollama`):

```ini
[Service]
Nice=10
OOMScoreAdjust=500
Environment=OLLAMA_NUM_PARALLEL=1
Environment=OLLAMA_CONTEXT_LENGTH=4096
Environment=OLLAMA_FLASH_ATTENTION=1
Environment=OLLAMA_KV_CACHE_TYPE=q8_0
```

## Training, goals, climbs

The web pages are in the bike computer's Rim & Ridge design
([design system](design/rim-ridge-design-system.md)). Besides the ride list and the ride
report there are:

| Page | Content |
|---|---|
| **Training** (`/training`) | fitness (CTL, 42-day mean of the TRIMP), fatigue (ATL, 7 days) and form (TSB) over time; kilometres and hours per heart-rate zone per week; weekly table. Needs `hr_max`, rides without heart rate count as 0 |
| **Ziele** (`/goals`) | target events with date and priority (A/B/C), countdown and training phase (base, build, peak, taper); upload the course as GPX (button in the goal, or right away in the form for a new one) for distance, elevation and climbs. Compared with the last 6 weeks: longest ride against the race distance, biggest weekly elevation against the race's; for every climb of the course an estimated time from the best VAM on comparable climbs of the last 90 days |
| **Anstiege** (`/climbs`) | climbs ridden more than once (foot and summit at most 150 m apart, needs GPS), every effort with time, gap to the best, VAM, heart rate, power |
| **Fahrer** (`/athlete`) | rider data, see above |

Code: [`bikelog/training.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/training.py)
(pure, works on the report JSON only, tests `tests/test_training.py`), pages in
[`webui.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/webui.py)
and [`charts.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/charts.py)
(SVG on the server, no JS library), tests `tests/test_pages.py`. Target events live in
`<data>/events.json`, their GPX files in `<data>/events/`.

### One ride, several sessions

When the bike computer reboots on the way, one ride becomes several sessions. Sessions of
the same device whose gap (end of one to start of the next) is at most
`BIKELOG_KOMOOT_MERGE_GAP_S` (30 min) are one ride -- the same grouping as for the Komoot
upload
([`tours.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/tours.py)).
Training, goals and climbs count the ride, its report runs over the joined records (the
distance continues, the reboot is a stop). The ride list marks the parts ("Teil 1/2"),
the session page links to the report of the whole ride (`/tour/{id}`). This relies on the
times: after a reset without clock the firmware takes the GPS time from TrailBridge and
corrects the timestamps written before; a session that never got a time (no WLAN, no
TrailBridge) stays on its own. Test and idle sessions never join a ride.

The pages load the fonts from Google Fonts; without internet the browser falls back to
system fonts.

## Installation (home server: `~/bikelog`)

```bash
Tools/BikeLogService/install.sh            # or: install.sh /other/path
```

creates: `~/bikelog/venv` (a venv of its own, the code is copied into it -- switching
branches in the repo does not touch the service), `data/`, `bikelog.env` (configuration,
never overwritten) and `bikelog.service`. Once as root:

```bash
sudo install -m 644 ~/bikelog/bikelog.service /etc/systemd/system/
sudo systemctl daemon-reload && sudo systemctl enable --now bikelog
journalctl -u bikelog -f
```

A system unit with `User=` instead of a user unit: needs no login session and no
`enable-linger`.

**Update**: after `git pull` run `install.sh` again, then `sudo systemctl restart bikelog`.

Interface: `http://<server>:8080/` (list of rides), API documentation
`http://<server>:8080/docs`.

## Development

```bash
cd Tools
python3 -m venv .venv
.venv/bin/pip install -r BikeLogService/requirements.txt
.venv/bin/python -m pytest
PYTHONPATH=.:BikeLogService .venv/bin/python -m bikelogservice serve --port 8081
```

## Command line

```bash
bikelogservice serve [--host 0.0.0.0] [--port 8080]
bikelogservice pull  [TRGB-BC.local | 192.168.0.171] [--device trgb]   # fetch once
bikelogservice import /media/sd/BIKECOMP [--device trgb]              # SD card in a card reader
bikelogservice export [--all]                                         # write GPX files
```

`pull` and `import` work on the same data directory (`BIKELOG_DATA_DIR`) and may run in
parallel to the service.

## Configuration

Environment variables (in the service: `~/bikelog/bikelog.env`):

| Variable | Default | Meaning |
|---|---|---|
| `BIKELOG_DATA_DIR` | `$STATE_DIRECTORY` or `~/.local/state/bikelog` | sessions + SQLite index |
| `BIKELOG_PORT` | -- | only in the unit: port for `serve` |
| `BIKELOG_PULL` | `0` | fetching on/off |
| `BIKELOG_PULL_TARGETS` | `trgb=TRGB-BC` | `device=mdns-name`, comma-separated |
| `BIKELOG_PULL_INTERVAL_S` | `120` | polling interval |
| `BIKELOG_PULL_MDNS` | `1` | listen for mDNS announcements |
| `BIKELOG_EXPORT_GPX` | `1` | GPX export on/off |
| `BIKELOG_EXPORT_DIR` | `<data>/export/gpx` | target directory (must be writable in the unit: `ReadWritePaths=`) |
| `BIKELOG_PUBLIC_URL` | -- | e.g. `http://server:8080`, for the link in the GPX |
| `BIKELOG_REQUIRE_AUTH` | `0` | auth on/off |
| `BIKELOG_TOKENS` | -- | `token:name`, comma-separated |
| `BIKELOG_MAX_UPLOAD_BYTES` | 64 MiB | upper limit per `PUT` |
| `BIKELOG_KOMOOT_EMAIL` / `BIKELOG_KOMOOT_PASSWORD` | -- | Komoot login; without both the upload is off |
| `BIKELOG_KOMOOT_ACTIVITY` | `touringbicycle` | a `SupportedActivities` constant from kompy |
| `BIKELOG_KOMOOT_STATUS` | `friends` | visibility of the uploaded tour (`public`/`private`/`friends`) |
| `BIKELOG_KOMOOT_MERGE_GAP_S` | `1800` | sessions of the same device with at most this much pause between them count as one interrupted ride |
| `BIKELOG_NEXTCLOUD_URL` / `_USER` / `_PASSWORD` | -- | Nextcloud login (app password); without all three the sync is off |
| `BIKELOG_NEXTCLOUD_DIR` | `BikeLog` | target directory (WebDAV path, created if needed) |
| `BIKELOG_ATHLETE_FILE` | `<data>/athlete.json` | rider data for the session report |
| `BIKELOG_IDLE_ARCHIVE_DAYS` | `7` | idle sessions to `archive/` after this many days, 0 = never |
| `BIKELOG_LLM_URL` | -- | Ollama server for the ride in words; unset = off |
| `BIKELOG_LLM_MODEL` | `qwen3:8b` | Ollama model |
| `BIKELOG_LLM_TIMEOUT_S` | `1800` | longest wait for one answer |
| `BIKELOG_LLM_REFRESH_DAYS` | `14` | texts whose facts changed are rewritten for rides of this many days only |

Several bike computers: each build variant has a network name of its own (mDNS host and
default hotspot SSID, `BC_HOSTNAME` in `platformio.ini`): the gravel build `TRGB-BC`, the
Forumslader build `TRGB-FL`; changeable on the bike computer's page `/wifi`
([WiFi](WIFI.md#device-name)). One pull target per bike computer, e.g.
`BIKELOG_PULL_TARGETS=trgb=TRGB-BC,pendler=TRGB-FL`; the part before `=` is the device name
the sessions are filed under and that bikes are assigned to (see [bikes](#bikes)). Keep the
name of an existing target, the stored sessions are filed under it.

## API (v1)

| Method | Path | Purpose |
|---|---|---|
| `GET` | `/` | list of rides (HTML) |
| `GET` | `/api/v1/health` | reachability + number of sessions |
| `GET` | `/api/v1/sessions` | list (`limit`, `offset`, `tests`: include test sessions, `idle`: include idle sessions), newest first |
| `GET` | `/api/v1/sessions/{id}` | session with files, key figures, `I_` statistics |
| `GET` | `/api/v1/sessions/{id}.gpx` | GPX as in the export (`max_fix_age_ms`, `segment_gap_s`, `ele`, `max_accuracy_m`, `shocks`, `labels`, `rich`) |
| `GET` | `/api/v1/sessions/{id}.csv` | CSV (`with_gps`) |
| `GET` | `/api/v1/sessions/{id}/files/{name}` | one file unchanged |
| `GET` | `/api/v1/sessions/{id}/report.json` | key figures of the session, see [session report](#session-report) |
| `GET` | `/api/v1/sessions/{id}/report.md` | the same as a German Markdown report |
| `POST` | `/api/v1/sessions/{id}/test` | overrule the test detection (`mark=test|real|auto`) |
| `POST` | `/api/v1/archive/device-delete` | delete idle/archived sessions on the bike computer now (`session_id`) |
| `PUT` | `/api/v1/import/gpx` | import a recorded ride (`bike_id`, `event_id`; body = GPX) |
| `GET` | `/api/v1/training` | daily load (TRIMP, CTL, ATL, TSB), weeks, recurring climbs (`days`, `weeks`) |
| `GET` | `/api/v1/events` | target events with readiness |
| `PUT` | `/api/v1/events/{id}/gpx` | course GPX of a target event (body = file) |
| `DELETE` | `/api/v1/sessions/{id}` | delete the files, keep the tombstone |
| `POST` | `/api/v1/sessions/{id}/komoot` | upload as part of its tour (`force`), see above |
| `POST` | `/api/v1/sessions/{id}/komoot/ignore` | answer the page's question with "no" (`ask_again=true`: ask again) |
| `PUT` | `/api/v1/devices/{device}/files/{day}/{name}` | deliver one file (body = file) |
| `GET`/`POST` | `/api/v1/pull` | status of fetching / fetch now (`device`) |

`PUT` is idempotent: the same bytes once more give `unchanged`, other bytes under the
same name `replaced` (the bike computer rewrites `I_` files when their format changes).
An `L_` that cannot be read is kept anyway and explained in `log_error` -- it is the
device's data. `CUR/` is rejected with `409`.

```bash
curl -T L_143012.bin http://server:8080/api/v1/devices/trgb/files/20260920/L_143012.bin
```

## Adding auth later

Auth is off but fully wired. **Every** route already depends on `require_principal`
([`auth.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/auth.py)),
which currently returns an anonymous principal. Switching it on:

```
BIKELOG_REQUIRE_AUTH=1
BIKELOG_TOKENS=<token>:gravel
```

No route change, no migration; clients send `Authorization: Bearer <token>`. The HTML
page is then no longer readily reachable in the browser -- for a home server without
port forwarding auth therefore stays off. The test
`test_every_route_requires_a_token_once_auth_is_on` records that no route bypasses the
dependency.
