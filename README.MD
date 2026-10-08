# T-RGB Bike Computer

A bike computer built on the [LilyGO T-RGB](https://www.lilygo.cc/products/t-rgb): a round
480×480 touch display, ESP32-S3 with BLE and WiFi, SD card, and a LiPo charger on board.
Its main job is **navigation on the handlebar**: turn-by-turn instructions from
[OsmAnd](https://osmand.net/), or along your own GPX route, sent by the Android app
[TrailBridge](https://github.com/euphi/TrailBridge) over BLE. On a GPX route it also
shows the climb ahead. Besides that it reads standard BLE bike sensors and logs every
ride to SD card.

<p>
  <img src="doc/screenshots/nav.png" width="240" alt="Navigation screen">
  <img src="doc/screenshots/main.png" width="240" alt="Main screen">
  <img src="doc/screenshots/climb.png" width="240" alt="Climb screen">
</p>

Screenshots from the device, taken during a simulated ride: navigation, main screen,
climb.

**Documentation in English and German: <https://euphi.github.io/TRGB-BikeComputer/>**

<!-- features-start -->
## Features

**Navigation** from the phone, through the Android app
[TrailBridge](https://github.com/euphi/TrailBridge) over BLE
([protocol](https://github.com/euphi/TrailBridge/blob/main/PROTOCOL.md)). Two sources:
- **OsmAnd**: TrailBridge passes on OsmAnd's turn-by-turn navigation -- maneuver,
  distance, street name, the maneuver after next, lane guidance, roundabout exits
- **Your own GPX route**: TrailBridge navigates along a GPX file itself, without OsmAnd.
  Turn hints come from the file or from the geometry of the track. With elevation data
  in the file the bike computer also gets the profile of the road ahead
- The navigation screen opens by itself before a maneuver and closes after it: large
  turn arrow, distance ring, street name, next maneuver, lanes. In between, the main
  screen shows the next maneuver and its distance in a small pill
- [`Tools/gpxenrich`](https://github.com/euphi/TRGB-BikeComputer/tree/main/Tools/gpxenrich)
  turns a plain GPX track into a route with turn hints (from BRouter)

**Climbs** on a GPX route: the climb screen shows the elevation profile ahead, coloured
by gradient, with category, altitude and distance to the summit. It opens by itself at
the foot of a rated climb ([doc/CLIMB.md](doc/CLIMB.md)).

**Main screen**: speed (number and outer ring), cadence, heart rate with zone band,
temperature, altitude, gradient, distance (ride / trip / tour / total), ride time or
clock, driving state, status icons for WiFi, GPS fix and battery.

**Rides and statistics**
- One button starts a ride, pauses it ("cruise") and ends it; stops and breaks are
  detected from the speed
- Distance, time, average and maximum speed and average cadence per ride, trip, tour
  and in total; averages with or without stops, breaks and cruise time
  ([doc/design/ride-state-machine.md](doc/design/ride-state-machine.md))

**Sensors**
- BLE: cycling speed & cadence (CSC), heart rate, battery level of each sensor;
  sensors are paired once and locked to their address
- The phone's GPS position (through TrailBridge) for the ride log, and its time for the
  clock when there is no WiFi
- BME280: barometric altitude and gradient, temperature
  ([height calibration](doc/HEIGHT.md))
- BMI160 IMU sensor to record the [road quality](doc/ROADQUALITY.md)
- [Forumslader](https://www.forumslader.de/) hub-dynamo charger via my
  [BLE gateway](https://github.com/euphi/ESP32_FLClassic2BLE) (build variant `-FL`)

**Settings on the device**: WiFi (on/off, hotspot, network setup with the display
keyboard, [doc/WIFI.md](doc/WIFI.md)), BLE devices (connection, battery level, forget a
sensor), height calibration, IMU calibration, restart and power off.

**Logging**
- Binary ride log on SD card in dated folders: ride data every 5 s incl. GPS position
  and ride states
- Debug log and raw Forumslader log (replayable on the device)
- Python [tools](doc/TOOLS.md) to convert logs to CSV and GPX, and a
  [service](doc/LOGSERVICE.md) that fetches the sessions when the bike
  computer shows up in the WiFi, archives them, exports GPX, syncs to Nextcloud and
  uploads to Komoot
- Session report: all key figures of a ride (climbs, heart-rate zones, TRIMP, estimated
  power, road quality, sensor dropouts and other technical findings) as a web page,
  Markdown or JSON; optionally also in words, written by a local LLM (Ollama) on the home
  server, with a check of every number it uses
- Training analysis in the log service, in the Rim & Ridge design: fitness, fatigue and
  form over time, weeks by heart-rate zone, target events with countdown, training phase
  and the course's climbs, best times on recurring climbs
- Test sessions (sensor simulator, TrailBridge GPX test ride) are recognised, marked and
  kept out of the ride list, training, Nextcloud and Komoot; idle sessions (switched on,
  no ride) are archived and can be deleted on the bike computer from the service
- Bikes with weight and aerodynamics, assigned to the bike computers by date (several bike
  computers in one network: `TRGB-BC`, `TRGB-FL`); rides recorded elsewhere imported as
  GPX, also as earlier editions of a goal

**Web interface** (when on WiFi)
- Download, replay and delete log files; live log with adjustable log levels
- Ride statistics with a chart, odometer and wheel calibration
- Manage BLE sensors; debug pages for sensors, accelerometer, climbs and crashes
- WiFi: saved networks and their priority, hotspot settings ([doc/WIFI.md](doc/WIFI.md))
- Firmware and filesystem update over the air

**Serial console** with line editing, history and Tab completion
([doc/DEBUG.md](doc/DEBUG.md)).

**For development**: a simulator build with fake sensors
([doc/SIMULATOR.md](doc/SIMULATOR.md)), screenshots and touch input over HTTP
(`Tools/uishot.py`), host tests for the pure algorithms (`test/`, `Tools/tests/`).

## Hardware

- LilyGO T-RGB (the two builds differ in the touch controller variant, `TRGB_ROUND` /
  `TRGB_OVAL`, and in Forumslader support)
- Optional: BME280 and BMI160 on the I²C bus
- LiPo battery
- 3D-printed case and handlebar mounts -- a parametric model for the Canyon CP0007
  gravel cockpit is in [`cad/`](https://github.com/euphi/TRGB-BikeComputer/tree/main/cad)

![T-RGB Bike Computer on the handlebar (older UI)](doc/IMG20230406075242-cropped.jpg)

## Build

[PlatformIO](https://platformio.org/), Arduino framework (pioarduino platform):

```bash
pio run -e trgb-esp32-s3 -t upload        # default: BLE + I²C sensors
pio run -e trgb-esp32-s3-FL -t upload     # Forumslader variant
pio run -e trgb-esp32-s3 -t uploadfs      # web interface files (data/site)
```

`trgb-esp32-s3-ota` and `trgb-esp32-s3-FL-ota` are the same builds, uploaded over WiFi
to the device's `/update` endpoint. Target is `TRGB-BC.local`; if mDNS doesn't resolve,
pass the IP: `pio run -e trgb-esp32-s3-ota -t upload --upload-port 192.168.x.y`.
`trgb-esp32-s3-sim` is the simulator build.

The build applies small patches to two libraries (`apply_patches.py`), which needs
the `patch` tool.

<!-- features-end -->

## Documentation

Everything is on <https://euphi.github.io/TRGB-BikeComputer/> in English and German. The
sources are in [`doc/`](doc/): `X.md` is English, `X.de.md` German.

| Document | Content |
|---|---|
| [ROADMAP](doc/ROADMAP.md) | state of the features, known bugs, planned features |
| [USABILITY-TODO](doc/USABILITY-TODO.md) | what is still missing on the device for use on the road |
| [CLIMB](doc/CLIMB.md) | climbs: categories, settings, demo |
| [ROADQUALITY](doc/ROADQUALITY.md) | road quality from the IMU sensor, road labels |
| [WIFI](doc/WIFI.md) | WiFi: networks, hotspot, web page |
| [HEIGHT](doc/HEIGHT.md) | height calibration |
| [Ride states](doc/design/ride-state-machine.md) | ride states and how the statistics count |
| [Ride test cheat sheet](doc/RIDE-TEST-CHEATSHEET.md) | what to check on a test ride |
| [Design system](doc/design/rim-ridge-design-system.md) | UI design system and screens |
| [TOOLS](doc/TOOLS.md) | log format, files on the SD card, CLI |
| [LOGSERVICE](doc/LOGSERVICE.md) | log service: fetching, GPX export, Nextcloud, Komoot |
| [DEBUG](doc/DEBUG.md) | debug pages, remote UI testing, logging, serial console, core dumps |
| [SIMULATOR](doc/SIMULATOR.md) | simulator build |
| [PITFALLS](doc/PITFALLS.md) | hardware and framework pitfalls (PSRAM and display flicker, NimBLE, internal heap, ...) |

Contributions are welcome, see the roadmap for where help is wanted.

## Licences and attributions

- [Cycling icons created by Futuer - Flaticon](https://www.flaticon.com/free-icons/cycling)
- [Parked bicycles - icon by Prashanth Rapolu 15](https://de.freepik.com/icon/fahrrad_7764338#fromView=resource_detail&position=7)
