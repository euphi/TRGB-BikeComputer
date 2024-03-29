# T-RGB Bike Computer

A bike computer based on [LilyGO T-RGB](https://www.lilygo.cc/products/t-rgb):

- 480px round RGB TouchDisplay
- ESP32-S3 microcontroller with Bluetooth LE and WiFi
- SD Card
- Battery connector incl. charging logic

![Photo of T-RGB Bike Computer with active navigation](/doc/IMG20230406075242-cropped.jpg)

## Build

Compile and upload with platformio. 

## Roadmap

### Prio 0 Features - work in progress or already working

#### BLE Devices
- [X] Connect to CSC Cadence
- [X] Connect to heart rate
- [X] Connect to komoot App
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
- [ ] **WIP:** Implement all navigation items (roundabouts are still missing, all others ok)
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

If you use the komoot App on your mobile and start following a route (load preplanned trip or use the route planner, it will start advertising a simple navigation service. The TRGB-Bikecomputer will connect to the mobile and read navigation instruction. These are shown on display.

#### Limitations (for now):

- TRGB-Bikecomputer will connect to the first komoot App it sees
- font supports standard ASCII only (no "Umlauts").
- no manual change to nav screen yet (automatic change if new navigation instruction arrives or if distance is < 250m)

#### Limitations (of komoot):

- komoot is not a good navigation system.
- It is great for planning routes, but it does not provide information about the turn after the next one and street names are often incomplete or misleading.
- If you are off your route, komoot often asks to confirm new route on mobile even if route recalculation is switched on.
- You must stop and store the route to finish navigation, otherwise your mobile keeps sending navigation instructions.
- Even worse, in this case komoot often seems to freeze navigation information on old state
- Even worse, a combination of the mentioned problems above often occur if you start a round-trip tour and komoot thinks you are already on your way back: Route is not updated or only to the short distance to destination, it takes a long time to restart and bluetooth navigation still shows old data.
- The instructions to connect device to komoot may be misleading. You don't need to manually connect, but you need to activate routing support and then load a route. (To see if komoot advertise its navigation service, use another mobile with a BLE scanner).

### Licences and Attributions

#### Images

[Cycling icons created by Futuer - Flaticon]("https://www.flaticon.com/free-icons/cycling")
[Parked bicycles - Icon von Prashanth Rapolu 15]("https://de.freepik.com/icon/fahrrad_7764338#fromView=resource_detail&position=7")

