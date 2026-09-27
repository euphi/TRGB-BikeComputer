# Roadmap

Open work, roughly by priority. Everything that already works is described in the
[README](README.MD). Help is welcome, especially where marked.

## In progress

- **Settings screen** on the device -- designed
  ([`doc/design/settings.svg`](doc/design/settings.svg)), not yet built in EEZ Studio.
  Should replace the remaining legacy SquareLine screens (WiFi, OTA, settings, chart),
  which are still initialized but no longer shown.
- **Statistics on the device** -- distance, average speed and time per trip/tour/total
  are shown; a screen with more detail (max speed, time in each driving state) is
  missing.
- **Charts** -- the web interface has a first chart page (uPlot); charts on the device
  are still to come back with the new UI.
- **New case** for the Canyon CP0007 gravel cockpit -- parametric model in
  [`cad/`](cad/), measurements from photos still to be checked with a caliper.

## Next

- Power value on the main screen: the widget exists, there is no data source yet
  (BLE cycling power service or an estimate).
- Distance ring on the main screen (the speed ring's mechanism is reusable).
- Configuration on the device (wheel circumference, sensors) -- today only via the
  web interface.
- Show BLE sensor battery levels on the display (they are read already).
- WiFi setup from the serial console.
- First-time setup: access-point mode exists, but there is no guided flow for
  entering the WiFi credentials yet.
- Binary log replay (only the Forumslader text log can be replayed today).

## Help wanted

- Better / more 3D-printable mounts, also for other handlebars.
- Animations on the display (driving state, heart rate).
- More use of the logged road quality, e.g. surface maps / heatmaps from many rides.
- Smaller flash footprint of the boot logo (stored raw, 460 KB; would compress to
  about 11 KB, needs a change in TRGBArduinoSupport).

## Known limitations

- The bike computer connects to the first TrailBridge instance it finds. Android
  rotates its BLE address, so the phone cannot be locked like the sensors.
- The Forumslader variant is built and tested less often than the default one.
