# T-RGB Bike Computer

A bike computer based on [LilyGO T-RGB](https://www.lilygo.cc/products/t-rgb):

- 480px round RGB TouchDisplay
- ESP32-S3 microcontroller with Bluetooth LE and WiFi
- SD Card
- Battery connector incl. charging logic

![Photo of T-RGB Bike Computer with active navigation](/doc/IMG20230406075242-cropped.jpg)

## Build

Compile and upload with PlatformIO:

- `trgb-esp32-s3` (default) -- round T-RGB, sensors via BLE + I2C
- `trgb-esp32-s3-FL` -- oval T-RGB variant with Forumslader support
- `trgb-esp32-s3-ota`, `trgb-esp32-s3-FL-ota` -- same builds, uploaded over WiFi to the
  device's `/update` endpoint (curl). Target is `TRGB-BC.local`; if mDNS doesn't resolve,
  pass the IP: `pio run -e trgb-esp32-s3-ota -t upload --upload-port 192.168.x.y`

## Documentation

- [doc/PITFALLS.md](doc/PITFALLS.md) -- known pitfalls (PSRAM/display flicker, NimBLE, ...), German
- [doc/DEBUG.md](doc/DEBUG.md) -- serial console, reading core dumps
- [doc/design/rim-ridge-design-system.md](doc/design/rim-ridge-design-system.md) -- UI design system, German
- [Tools/README.md](Tools/README.md) -- binary log tools (CSV/GPX export, upload service), German

## Roadmap

### Prio 0 Features - work in progress or already working

#### BLE Devices
- [X] Connect to CSC Cadence
- [X] Connect to heart rate
- [X] Connect to [TrailBridge](https://github.com/euphi/TrailBridge) app (navigation + GPS position)
- [X] connect to CSC speed
- [X] connect to Forumslader Classic via my [BLE gateway](https://github.com/euphi/ESP32_FLClassic2BLE)

#### Logging

- [X] Write debug information to logfiles
- [X] Write raw forumslader data to logfiles
- [X] Replay FL log file
- [X] User Interface for replay
- [X] Write summary in binary logfile
- [X] Python Scripts to convert binary logfile to CSV and to LineProtocol (influxdb)

#### Console

- [X] Integrate SimpleCLI
- [X] Line editing, history and Tab completion (miniterm, picocom, `pio device monitor`) -- see [doc/DEBUG.md](doc/DEBUG.md#serial-console)
- [X] Set loglevel by console and store them
- [ ] Configure WiFi from console
- [X] basic debug/monitoring support (free heap etc.)

#### Webserver
- [X] Connect to WiFi
- [X] Give static overview and server logfiles
- [X] Delete/Cleanup logfiles (integrated, but does not work yet)
- [ ] WIP: Charts on Webserver
- [X] Debug Information on Webserver

#### User interface

- [X] Initialize RGB display
- [X] Merge and refactor existing UI from my [T-RGB Experiments](https://github.com/euphi/T-RGB-Experiments) (speed, FL batterie, heart rate, FL details, IP Adress)
- [X] Add Navigation Screen
- [X] Implement all navigation items (incl. roundabouts and lane guidance)
- [ ] **WIP:** Show basic statistics
- [X] Add very simple screen (in bright theme for better readability in direct sunlight)
- [ ] Add screen for more detailed information
- [ ] Show connection information on screeen

#### Case

- [X] Create case model for 3D-printer (done for standard handlebar incl. space for 2 USB-A sockets ("Forumslader variant")
- [X] Create mount for trekking handle bar
- [X] Create mount for Canyon double decker bar

### Prio 1 Features -- Help wanted

- [X] robust webserver for data download
- [ ] Improve statistics (e.g. graphs )
- [ ] better User Interface
- [ ] Animations on display (e.g driving state, heart rate)
- [X] Configuration via Webserver (wheel diameter etc.)
- [ ] Configuration via Display (wheel diameter etc.)
- [X] manage BLE devices via Webserver (show connection state, batterie level, delete connection)
- [X] improve handling of CSC devices: "Lock" connected devices and only allow adding new devices on demand. Also check if max 1 speed and max 1 cadence is connected
- [X] read battery state of BLE devices (works fine, but no UI yet)
- [X] manage BLE devices and show info via webserver 
- [X] add additional sensors (barometer/temperature, gyro/inertial)
- [ ] use gyro/inertial sensor data
- [ ] improve 3D-printable mounts 


### Prio 2 Features -- Help NEEDED (I don't have the time for them)

- [ ] AP Mode for Wifi for first setup (simple AP mode for download may be prio 1)
- [ ] even better User Interface
- [ ] add better 3D-printable mounts, also for other handlebars
- [ ] even better use of gyro/inertial sensor data

## Howto

.. documentation work in progress ..


### Use Navigation

Navigation comes from [TrailBridge](https://github.com/euphi/TrailBridge), a companion
Android app that reads OsmAnd's turn-by-turn instructions and forwards them to the bike
computer over BLE using a documented TLV protocol (see TrailBridge's
[PROTOCOL.md](https://github.com/euphi/TrailBridge/blob/main/PROTOCOL.md)).

TrailBridge also exposes the phone's raw GPS position over a second, independent BLE
service (works even without OsmAnd running), which the bike computer logs to its binary
datalog alongside its own sensors.

#### Limitations (for now):

- TRGB-Bikecomputer connects to the first TrailBridge instance it sees (its BLE address
  isn't persisted/locked, since Android peripherals use a rotating address).
- The nav screen opens automatically when the next maneuver comes close. Manually: tap the
  nav pill on the main screen to open it, swipe right to close it.

### Licences and Attributions

#### Images

[Cycling icons created by Futuer - Flaticon]("https://www.flaticon.com/free-icons/cycling")
[Parked bicycles - Icon von Prashanth Rapolu 15]("https://de.freepik.com/icon/fahrrad_7764338#fromView=resource_detail&position=7")

