# Height calibration

![Height page](screenshots/height.png){ width="260" }

The BME280 measures air pressure; the height is derived from it and a **reference pressure at
sea level** (QNH). Weather changes that pressure, so the height drifts and has to be set again
now and then at a place whose height you know. Settings → **Höhe**:

| Button | Does |
|---|---|
| **Three presets** (default 359 m, 320 m, 0 m) | Tap: calibrate to the preset height. **Long press**: change the preset (number entry). |
| **GPS** | Calibrates to the height above sea level of TrailBridge's GPS fix. Greyed out without a current fix. A phone GPS is only good to about 10 m vertically -- a rough start, not a replacement for a known place. |
| **Manuell** | Number entry: the **known height** (m), or switch to the **reference pressure at sea level** (hPa, 850 to 1090), e.g. from a weather report. |

The page shows the height, the measured pressure and the reference pressure. The result of the
last action stays on the line below the buttons for a few seconds.

## Web page

`/sensor/` has the same: the presets (button and field to change), GPS, a known height and a
reference pressure. The routes also work for scripts (`GET`, answer `OK` or the reason):

```
/sensor/cal?preset=0..2     /sensor/cal?gps=1
/sensor/cal?height=<m>      /sensor/cal?qnh=<hPa>
/sensor/preset?i=0..2&height=<m>
```

## Notes

- Reference pressure and presets are stored as one NVS blob (`Sensors/HeightCal`), written by
  the next BME280 cycle, not by the caller. An older single value (`RefPressure`) is still read.
- GPS height needs the tag `MSL_ALTITUDE_DM` (0x0E) of the [protocol](trailbridge/PROTOCOL.md)
  and Android 14 or later on the phone. `ALTITUDE_M` is above the ellipsoid (about 47 m off in
  Germany) and is **not** used. Made-up positions (GPX test ride) are refused.
- Code: `I2CSensors::calibrateHeight()` and friends, display `src/ui/RimRidgeAltCustFunc.*`
  (EEZ pages `RimRidgeSettingsAlt`, `RimRidgeSettingsNum`), web routes in `src/WifiWebserver.cpp`.
