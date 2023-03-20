# T-RGB Bike Computer

A bike computer based on [LilyGO T-RGB](https://www.lilygo.cc/products/t-rgb):

- 480px round RGB TouchDisplay
- ESP32-S3 microcontroller with Bluetooth LE and WiFi
- SD Card
- Battery connector incl. charging logic

## Build

Compile and upload with platformio. 

## Prio 0 Features - work in progress or already working

### BLE Devices
- [X] Connect to CSC Cadence
- [X] Connect to heart rate
- [X] Connect to komoot App
- [X] connect to CSC speed
- [X] connect to Forumslader Classic via my [BLE gateway](https://github.com/euphi/ESP32_FLClassic2BLE)

### Logging

- [X] Write debug information to logfiles
- [X] Write raw forumslader data to logfiles
- [X] Replay FL log file
- [X] User Interface for replay
- [ ] Write summary in binary logfile (not yet integrated, to be merged from [ESP32_BTTacho](https://github.com/euphi/ESP32_BTTacho)

### Console

- [X] Integrate SimpleCLI
- [X] Set loglevel by console and store them
- [ ] Configure WiFi from console
- [ ] basic debug/monitoring support (free heap etc.)

### Webserver
- [X] Connect to WiFi
- [X] Give static overview and server logfiles
- [X] Delete/Cleanup logfiles (integrated, but does not work yet)

### User interface

- [X] Initialize RGB display
- [X] Merge and refactor existing UI from my [T-RGB Experiments](https://github.com/euphi/T-RGB-Experiments) (speed, FL batterie, heart rate, FL details, IP Adress)
- [ ] WIP: Add Navigation Screen
- [ ] WIP: Show basic statistics
- [ ] Add very simple screen
- [ ] Add screen for more detailed information
- [ ] Show connection information on screeen

### Case

- [ ] Create case model for 3D-printer
- [ ] Create mount for trekking handle bar
- [ ] Create mount for Canyon double decker bar


## Prio 1 Features -- Help wanted

- [X] robust webserver for data download
- [ ] Improve statistics (e.g. graphs )
- [ ] better User Interface
- [ ] Animations on display (e.g driving state, heart rate)
- [ ] Configuration via Display (wheel diameter etc.)
- [ ] manage BLE devices via display
- [ ] improve handling of CSC devices: "Lock" connected devices and only allow adding new devices on demand. Also check if max 1 speed and max 1 cadence is connected
- [ ] read battery state of BLE devices
- [ ] add additional sensors (barometer/temperature, gyro/inertial)


## Prio 2 Features -- Help NEEDED (I don't have the time for them)

- [ ] AP Mode for Wifi for first setup (simple AP mode for download may be prio 1)
- [ ] even better User Interface
