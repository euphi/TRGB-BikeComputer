# Roadmap

What is open, roughly by priority. What already works is described in the
[README](README.MD); this file says how well it has been tested and what comes next.
Help is welcome, especially where marked.

## Where things stand

| Area | State |
|---|---|
| Sensors, main screen, logging, web interface | in regular use |
| Navigation from OsmAnd | ridden |
| Road quality and manual road labels | one test ride; the thresholds are still the first guesses |
| Ride states and statistics | tested with the simulator build; a real ride is pending ([cheat sheet](doc/RIDE-TEST-CHEATSHEET.md)) |
| GPX route navigation and climb screen | tested with TrailBridge's simulated ride and the demo profile; no real climb ridden yet |
| Settings screen, log sessions, log service | working; setting the clock from GPS has not been checked on its own |
| Forumslader variant | builds, rarely tested; its screen is the last one from the old SquareLine UI |

## Known bugs

Open, to be fixed.

- **Watchdog reset from the I²C bus**: BME280 and BMI160 are read from two tasks
  without a common lock. Seen once, analysed from the core dump.
- **Gaps in the accelerometer FIFO**: on the first test ride about a third of the
  road-quality intervals had gaps, also at standstill.
- **Two CSC sensors**: removing the first one while the second is connected confuses
  the assignment until the next restart.

## Next: usable on the road

Once the device is in its case there is no USB and usually no WiFi, so everything a
ride needs must work on the display. Details: [doc/USABILITY-TODO.md](doc/USABILITY-TODO.md).

- Road labels that can be hit on a rough road, and a rule for mis-taps.
- Detect "bike upright" (lift) and "bike on the car" and leave them out of the ride.
- **WiFi configuration**: the network is compiled into the firmware today, and the
  credentials stored by the web interface are ignored. Wanted: use the stored ones,
  several networks (phone hotspot), access-point mode from the device with a guided
  first-time setup, credentials from the serial console.
- Average speed and the other statistics on the display. They are computed and shown
  in the web interface; the main screen has no widget for them.
- Remaining distance and time to the destination on the navigation screen (received,
  not shown).
- Configuration on the device (wheel circumference, sensor pairing) -- today only in
  the web interface.
- BLE sensor battery levels on the display (they are read already).
- Progress display during a firmware update.

## Planned features

Not designed yet.

- **Several bikes**: statistics, odometer and settings (wheel circumference, sensors,
  accelerometer calibration) per bike, with a way to choose the bike on the device.
- **Service intervals**: a page that remembers when chain oil, chain, sprockets,
  derailleur and tyres were last serviced, and shows the distance ridden since.
  Builds on the per-bike odometer.
- **Waypoints in navigation**: show the waypoints of a route (summit, feed zone,
  gravel sector) with name and distance. Needs an extension of the
  [TrailBridge protocol](https://github.com/euphi/TrailBridge/blob/main/PROTOCOL.md);
  today `Tools/gpxenrich` can only pass them on as a named "straight on".
- **Climb overview for the whole route**: which climb of how many, and the altitude
  still to gain. Needs a protocol extension as well.
- **Backup and restore** of odometer, statistics and settings over the web interface.
  They live only in the device's flash and are lost when it is erased.
- **Touch lock** against ghost touches from rain drops.
- **FIT export** in the log service, next to GPX.

## Later

- Power on the main screen: the widget exists, there is no data source (BLE cycling
  power service or an estimate). The binary log has no free field for it either.
- Distance ring on the main screen (the speed ring's mechanism is reusable).
- Charts on the device; the old chart screen is gone and will be designed anew.
- A road-quality screen with more than the coloured line, and tuning the thresholds
  on recorded rides.
- Forumslader screen in the Rim & Ridge design; Forumslader distance in the statistics.
- Elevation profile during OsmAnd navigation (today only with a GPX route).
- Binary log replay on the device (only the Forumslader text log can be replayed).
- New case for the Canyon CP0007 gravel cockpit -- parametric model in [`cad/`](cad/),
  measurements from photos still to be checked with a caliper.

Low priority:

- Reminder to eat and drink, by time or distance.
- Radar (Garmin Varia over BLE).
- Remaining battery runtime. The battery is a plain LiPo and its state of charge is
  only estimated from the voltage, which is inaccurate; it lasts about six hours.

## Help wanted

- Better / more 3D-printable mounts, also for other handlebars.
- Animations on the display (driving state, heart rate).
- More use of the logged road quality, e.g. surface maps / heatmaps from many rides.
- Smaller flash footprint of the boot logo (stored raw, 460 KB; would compress to
  about 11 KB, needs a change in TRGBArduinoSupport).

## Known limitations

- The bike computer connects to the first TrailBridge instance it finds. Android
  rotates its BLE address, so the phone cannot be locked like the sensors.
- A log session is one boot, not one ride. Ride start and stop are marked inside it.
- Climbs longer than 5 km come in a coarser raster, see [doc/CLIMB.md](doc/CLIMB.md).
