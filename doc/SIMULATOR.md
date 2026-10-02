# Sensor simulator (debug build)

Test statistics, driving states and distance at the desk, without riding. Only contained
in the build `trgb-esp32-s3-sim` (`-DBC_SIM`) -- the normal builds (`trgb-esp32-s3`,
`-FL`, `-ota`) contain none of it.

## What is simulated

`src/SimSensors.*` produces once a second the BLE notifications that a real CSC speed
sensor (slot CSC_1), a cadence sensor (CSC_2) and a heart-rate strap would send --
cumulative revolutions with the time of the last revolution in 1/1024 s -- and sends them
through `BLEDevices::notifyCallbackCSC()`. From there on everything runs as on the bike:
`Distance::updateRevs()` (revolutions, wheel circumference, reconnect/lost distance),
`Statistics` (states stop/break/ride/coast, times, averages, cadence), binary log, UI.

- Wheel circumference: the configured one (`/stat/calibration`), shared with the normal
  firmware.
- Revolution counters start at 0 on every boot (like a sensor that restarts its counter).
- `sim stop` = sensors off (disconnect → `DS_NO_CONN`); the next `sim …` is a reconnect.
- Cadence 0 while moving = coasting; leaving out cadence or heart rate (`-`) = this
  sensor does not exist.
- While the simulator is active, real speed/cadence/heart-rate sensors are ignored (data
  and disconnects). Still better to leave them switched off: after `sim stop` a real
  speed sensor takes over with a completely different counter value.

## Fed by TrailBridge (test ride)

The Android app can "ride" its GPX route (button "Testfahrt") and sends position, speed
and -- from the GPX or emulated -- heart rate and cadence in the GPS position frame,
marked with `SIM_FLAGS`
([protocol](trailbridge/PROTOCOL.md), "sensor
values and simulation mode"). The simulator build feeds that in like a
`sim <km/h> <rpm> <bpm>` per second (`SimSensors::feedFromTrailBridge()`): statistics,
distance and binary log run with simulated sensors, and the position goes into the log as
well -- the first ride with a GPS track *and* statistics without riding.

- If the frame has no heart rate or no cadence, that sensor does not exist.
- **Altitude:** `BARO_HEIGHT_DM` replaces the barometer for the time being
  (`I2CSensors::getHeight()`). The gradient comes from the normal calculation
  (`Statistics::calculateGradient()`: change of altitude by change of distance, distance
  from the simulated wheel revolutions) -- it is not transmitted. Without altitude in the
  frame (GPX without elevation data) the real barometer stays.
- **Power:** `POWER_W` is parsed and held (`SimSensors::getPower()`, `/debug/sim.json`),
  but not shown or logged anywhere yet -- the binary log has no field for it (record
  full, 64 bytes) and the UI no display.
- End: the first frame without `SIM_SENSORS`, `POSITION_NONE`, a fix older than 5 s or
  disconnection from the phone -> sensors off (`sim stop`). Only a simulation started by
  TrailBridge is ended this way; a manual `sim ...` stays.
- The normal firmware ignores the sensor values (no fake kilometres in the odometer), but
  marks log records with a simulated position as `LOG_SIMULATED`.
- `/debug/sim.json` shows under `source` where the running simulation comes from
  (`trailbridge` / `manual`).

## Separate statistics storage

The simulator build keeps the ride statistics in an NVS namespace of its own (`S_Stats`
instead of `Stats`, `NVS_STAT_PREFIX` in `include/global_settings.h`). Total, tour and
trip kilometres of the normal firmware stay untouched; after flashing back, the real
values are there again. Shared: WiFi, BLE addresses, wheel circumference, log settings,
IMU calibration.

Not separated: the sessions on the SD card (`/BIKECOMP/…`). Simulated rides appear as
normal sessions and are fetched by the log service if it is running -- without a GPS
track (that only comes from TrailBridge).

## Marking in the log

While the simulator is active, the firmware marks its log records (without a new format
version, the bits were always 0 so far):

- Ride data (`L_*.bin`, type 0): bit `LOG_SIMULATED` (0x80) in `gpsFlags` (it only shares
  the byte with the GPS bits, the data layout has no other free bits). Reader:
  `Record.simulated`.
- Ride state (type 4): `RSF_SIMULATED` (0x04) in `flags`, `RideStateRecord.simulated`.

Export: `bikelog info` reports `SIMULIERT: n von m Fahrdaten-Datensätzen`, GPX gets
`[SIM] ` in front of the name, "SIMULIERT" in the description, the keyword `simuliert`,
`<bc:simulated>1</bc:simulated>` on track points and ride-state segments; CSV with
`--roadq` has the column `Simuliert`. A purely simulated session has no GPS position
though (that only comes from TrailBridge), the GPX export then stays empty.

## Build and flash

```sh
pio run -e trgb-esp32-s3-sim -t upload        # USB
pio run -e trgb-esp32-s3-sim-ota -t upload    # WiFi (/update)
```

Back to the normal firmware: `pio run -e trgb-esp32-s3[-ota] -t upload`. At boot the
simulator build announces itself in the log with `SIMULATOR BUILD`.

## Operation

### Serial console

```
sim 25 85 140     25 km/h, 85 rpm, heart rate 140
sim 25 0          coasting (cadence 0), no heart-rate strap
sim 25 - -        speed sensor only
sim 0             standing (sensors stay connected)
sim stop          sensors off
sim               status
```

### Web: `/debug/sim` (also in the debug menu)

Manual input, sensors off, live statistics (start/trip/tour from `/stat/summary`) and a
GPX player in the browser. The player runs in the tab -- keep it in the foreground,
background tabs are throttled.

Directly: `/debug/sim/set?speed=25&cad=85&hr=140` (`cad`/`hr` empty or `-` = off),
`/debug/sim/stop`, `/debug/sim.json`.

### Playing a GPX from the computer: `bikelog sim`

```sh
cd Tools
python3 -m bikelog sim ride.gpx --http TRGB-BC.local
python3 -m bikelog sim ride.gpx --serial /dev/ttyACM0 --summary-host TRGB-BC.local
python3 -m bikelog sim ride.gpx --dry-run 30        # only show what would be sent
```

Options: `--cadence 80` (if the GPX has none), `--max-gap 150` (shorten long recording
pauses -- 150 s is enough to see the transition stop → break at 2 min), `--start MIN`,
`--duration MIN`, `--echo` (show the serial output). Only one program can have the serial
console open at a time -- close the monitor first.

Rules (the same in the browser player):

- speed from the geometry, distance difference over ±2 s,
- heart rate and cadence from the TrackPointExtension, if present,
- otherwise cadence: the given value while riding, 0 below 3 km/h and downhill steeper
  than 4 % (coasting).

Everything in real time: the statistics work with the device's clock, playing faster is
not possible.

At the end the tool prints what it sent (distance, moving time with the firmware's
hysteresis > 5.5 / < 0.3 km/h, average, maximum, average cadence), next to the change of
the device's trip statistics. For a clean comparison reset the trip first (maximum and
average cadence are totals of the trip). Small deviations are normal: the sensor only
reports whole revolutions, and the firmware sees every change only with the next
notification.
