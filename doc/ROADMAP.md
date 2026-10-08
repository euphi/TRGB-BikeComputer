# Roadmap

What is open, roughly by priority. What already works is described in the
[overview](index.md); this page says how well it has been tested and what comes next.
Help is welcome, especially where marked.

## Where things stand

| Area | State |
|---|---|
| Sensors, main screen, logging, web interface | in regular use |
| Navigation from OsmAnd | ridden |
| GPX route navigation and climb screen | ridden once (2026-10-04). After that ride the elevation profile was changed to a rolling window and the climb screen reworked; that version is only tested with the host test, not on the device |
| Ride states and statistics | ridden once (2026-10-04, [cheat sheet](RIDE-TEST-CHEATSHEET.md)). Fixed since: without a cadence sensor the whole ride counted as coasting |
| Log sessions, clock from GPS | ridden once. Fixed since: a wrong GPS time moved the clock by hours; a session now also ends when the ride is ended. Both fixes are untested on the device |
| [Road quality](ROADQUALITY.md) and manual road labels | two rides; the thresholds are still the first guesses |
| WiFi setup on the device, hotspot ([doc](WIFI.md)) | tested on the device; choosing the network by signal strength is new and untested |
| Settings pages, BLE device page, log service | working |
| [Route overview](ROUTE.md) (destination, waypoints, climbs of the GPX route) | new, not run on the device: host test, simulator build and a host rendering of the screen exist; the long read of the 512-byte frame over a real BLE link is untried |
| Height calibration ([doc](HEIGHT.md)) | presets, manual entry and web page tested on the device; the GPS button shows the phone's GPS height; calibrating with it has not been tried yet |
| Forumslader variant | builds, rarely tested; its screen is the last one from the old SquareLine UI |

## Known bugs

Open, to be fixed.

- **A fourth BLE device does not connect**: with speed sensor, heart-rate strap and
  TrailBridge connected, the cadence sensor stayed out on the test ride. The suspected
  cause is the BLE stack's limit of three connections ([pitfalls](PITFALLS.md)). A build
  with four (`trgb-esp32-s3-ble4`) runs on the device, but four peers at once have not been tried yet.

## Next: usable on the road

Once the device is in its case there is no USB and usually no WiFi, so everything a
ride needs must work on the display. Details: [usability backlog](USABILITY-TODO.md).

- Road labels that can be hit on a rough road, and a rule for mis-taps.
- Detect "bike upright" (lift) and "bike on the car" and leave them out of the ride.
- Average speed and the other statistics on the display. They are computed and shown
  in the web interface; the main screen has no widget for them.
- Remaining distance and time to the destination on the navigation screen itself (they are
  on the [route overview](ROUTE.md) screen now, one swipe to the left).
- Configuration on the device (wheel circumference, sensor pairing) -- today only in
  the web interface.
- BLE sensor battery levels on the display (they are read already).
- Progress display during a firmware update.

## Planned features

Not designed yet.

- **Several bikes per bike computer** (the log service already knows bikes and which bike
  computer rides on which bike, see [log service](LOGSERVICE.md#bikes)): one bike computer
  moved between bikes, the bike recognised by its sensors (cadence/speed sensor address),
  with statistics, odometer and settings (wheel circumference, accelerometer calibration)
  per bike on the device.
- **Service intervals**: a page that remembers when chain oil, chain, sprockets,
  derailleur and tyres were last serviced, and shows the distance ridden since.
  Builds on the per-bike odometer.
- **Route overview, the rest** ([done so far](ROUTE.md)): the height still to climb in a
  climb that has begun (needs a protocol extension, see the TrailBridge roadmap); a way to
  the screen from the main screen (today only a swipe to the left on the navigation screen);
  and a pop-up shortly before a waypoint, as the navigation screen does before a turn.
- **Direction back to the route**: off the route TrailBridge sends only the distance
  to it. Wanted: the bearing as well, shown as an arrow. Needs a protocol extension.
- **Pairing with the phone**: TrailBridge's BLE services can be read without pairing
  or encryption, so any BLE device in range can connect and read the position. With
  bonding the link would be encrypted, and the bike computer could recognise its phone
  despite the changing address (see the known limitations below). Needs changes in
  the app and in the firmware.
- **Backup and restore** of odometer, statistics and settings over the web interface.
  They live only in the device's flash and are lost when it is erased.
- **Touch lock** against ghost touches from rain drops.
- **FIT export** in the log service, next to GPX.
- **Evaluating the logs further** in the log service (builds on the session report and
  the training pages and the ride in words from the local LLM): training suggestions for
  the target events and a weekly review from the local LLM, fed only with the JSON of report
  and training; road
  quality matched onto OSM ways (map matching) and a JOSM file with `smoothness`/`surface`
  suggestions to check by hand.

## TrailBridge app

The Android app has its own roadmap (in German):
[TrailBridge ROADMAP.md](https://github.com/euphi/TrailBridge/blob/main/ROADMAP.md).
It lists the app's side of the features on this page and what concerns only the app
(English translation, time lapse for the simulated ride, reports to OsmAnd).

## Later

- Power on the main screen: the widget exists, there is no data source (BLE cycling
  power service or an estimate). The binary log has no free field for it either.
- Distance ring on the main screen (the speed ring's mechanism is reusable).
- Charts on the device; the old chart screen is gone and will be designed anew.
- A road-quality screen with more than the coloured line, and tuning the thresholds
  on recorded rides.
- Forumslader screen in the Rim & Ridge design; Forumslader distance in the statistics.
- Route line on the display: the course of a GPX route around the current position,
  to see turns coming and to find the way back. Needs a new BLE service for the
  geometry.
- Back channel from the bike computer to TrailBridge (there is no writable
  characteristic today): buttons on the device for the app, ride data to the phone,
  WiFi settings from the phone.
- Binary log replay on the device (only the Forumslader text log can be replayed).
- New case for the Canyon CP0007 gravel cockpit -- parametric model in [`cad/`](https://github.com/euphi/TRGB-BikeComputer/tree/main/cad),
  measurements from photos still to be checked with a caliper.

Low priority:

- Reminder to eat and drink, by time or distance.
- Radar (Garmin Varia over BLE).
- Remaining battery runtime. The battery is a plain LiPo and its state of charge is
  only estimated from the voltage, which is inaccurate; it lasts about six hours.
- Real sensor values passed on by TrailBridge (e.g. a heart-rate strap paired with the
  phone). The protocol tags are reserved; the bike computer pairs its sensors itself,
  so the benefit is small.

On the back burner:

- Several riders: rider data, goals and training per person in the log service, rides
  assigned by bike computer or bike.
- Elevation profile and route line during OsmAnd navigation (today only with a GPX
  route). OsmAnd's AIDL interface provides neither the route geometry nor altitudes,
  so this needs an extension in OsmAnd itself.
- Starting TrailBridge automatically when the phone boots. Not needed at the moment.

## Help wanted

- Better / more 3D-printable mounts, also for other handlebars.
- Animations on the display (driving state, heart rate).
- More use of the logged road quality, e.g. surface maps / heatmaps from many rides.
- Smaller flash footprint of the boot logo (stored raw, 460 KB; would compress to
  about 11 KB, needs a change in TRGBArduinoSupport).

## Known limitations

- The bike computer connects to the first TrailBridge instance it finds. Android
  rotates its BLE address, so the phone cannot be locked like the sensors. Pairing
  would solve this, see the planned features.
- A log session ends with a restart or when the ride is ended on the device. A break
  with the device switched off therefore splits a ride into two sessions.
- Climbs longer than 5 km come in a coarser raster, see [climbs](CLIMB.md).
