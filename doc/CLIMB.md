# Climbs: elevation profile and climb screen

When TrailBridge plays a GPX route, it sends the elevation profile of the next climb
([protocol](trailbridge/PROTOCOL.md), "elevation
profile service"). The bike computer finds foot and summit in it, rates the climb and
shows it on the screen `RimRidgeClimb` ([design](design/rim-ridge-design-system.md), §6).

![Climb screen](screenshots/climb.png){ width="260" }

| Part | File |
|---|---|
| Wire format | `src/BikeProfileProtocol.h` |
| Algorithm (pure, host test) | `src/ClimbProfile.*`, `test/native_climb/climb_test.cpp` |
| Integration: BLE, position, settings, CLI, web, demo | `src/ClimbMonitor.*`, `BLEDevices::subscribeProfile()` |
| Screen | `src/ui/RimRidgeClimbCustFunc.*`, `UIFacade::updateClimb()` |

## What the phone sends, and what the firmware does with it

TrailBridge sends the climb ahead: the altitudes **from the current position to the
summit**, as soon as the foot is at most 500 m away -- in one frame. Up to 5 km on a 25 m
raster, coarser for longer climbs (up to 250 m; the 36 km Galibier from the south comes
with 200 m). `PROFILE_NONE` follows at the summit. Small hills (from 10 m of altitude
gain) are sent as well.

The firmware

- **finds the climb**: the foot is the first point from which the next 100 m rise by 3 %
  on average. The summit is the last point before a descent of more than 30 m or before
  2 km without an average gradient of 0.5 % -- or the end of the profile. A shorter flat
  section or a dip does not end the climb; the Galibier from the north is two climbs
  (descent from the Télégraphe to Valloire). TrailBridge searches with the same criteria
  (`ElevationProfile.java` there) -- the defaults of both sides belong together.
- **joins frames** (same raster, overlapping) and keeps the part already ridden, up to
  320 steps. This is only needed when a climb does not fit into one frame even on the
  250 m raster.
- **continues a climb** when the phone sends `PROFILE_NONE` and a new profile shortly
  after.
- **knows the position** from the remaining distance in the navigation frames, advanced
  between two frames by the distance from the speed sensor.

Altitude figures are net: summit minus foot, summit minus rider.

## Categories

Score = length [m] × average gradient [%] = 100 × altitude gain. This is the scale of
Strava and Garmin (ClimbPro) for 4 to HC; 5 and 6 continue it downwards by halving.

| Category | Score from | Altitude gain from | Example |
|---|---|---|---|
| 6 | 2 000 | 20 m | 400 m at 5 % |
| 5 | 4 000 | 40 m | 800 m at 5 % |
| 4 | 8 000 | 80 m | 1.6 km at 5 % |
| 3 | 16 000 | 160 m | 3 km at 5.5 % |
| 2 | 32 000 | 320 m | 5 km at 6.5 % |
| 1 | 64 000 | 640 m | 9 km at 7.5 % |
| HC | 80 000 | 800 m | 10 km at 8 % |

Not rated (short hill, no automatic display): shorter than 300 m, flatter than 3 % on
average, or less than 20 m of altitude gain.

## Display

- The screen appears by itself as soon as a rated climb is at most 300 m ahead, and
  returns to the main screen 5 s after its end. It only replaces the main screen; if
  navigation, road labels or settings are showing, it becomes the target of "back".
- By hand: a tap on altitude or gradient on the main screen opens it, a swipe closes it.
  A climb that was swiped away does not come back by itself.
- Profile colour by the gradient of the raster section (25 m, up to 250 m for long
  climbs): below 1 % blue, up to 4 % green, up to 7 % yellow, up to 10 % orange, red
  above. The part already ridden is dimmed.

## Settings

Every value can be set individually and is stored in NVS (`Climb`):

```
climb                     state and all settings
climb cat4 9000           set
climb cat4                show (with range and meaning)
climb defaults            default values
```

Without USB: `/debug/climb` (table with input fields), `/debug/climb.json`,
`/debug/climb/set?<name>=<value>`.

| Name | Default | Meaning |
|---|---|---|
| `startgrade` / `startwindow` | 3 % / 100 m | foot: average gradient over this distance |
| `contgrade` | 0.5 % | average gradient from the summit so far with which the climb continues |
| `summitflat` | 2000 m | flat section that ends the climb; also the distance within which a new climb continues the old one |
| `summitdip` | 30 m | descent that ends the climb |
| `summitpass` | 50 m | this far behind the summit the climb is over |
| `minlength` / `mingrade` | 300 m / 3 % | not rated below |
| `cat6` … `cat1`, `cathc` | see above | score per category |
| `autoshow` | 1 | switch to the climb screen automatically |
| `autorank` | 1 | from which rank: 1 = category 6 … 6 = category 1, 7 = HC |
| `showahead` | 300 m | distance to the foot at which the screen appears |
| `hidedelay` | 5 s | wait before switching back |
| `lookahead` | 25 m | distance for the "gradient ahead" |
| `band1` … `band4` | 1 / 4 / 7 / 10 % | limits of the profile colours |
| `infocycle` | 4 s | cycle of the info field (time, distance, temperature, cadence, altitude) |

## Trying it without a phone

```
climb demo                2 km at 6 % (default)
climb demo 4000 9         length in m, average gradient in %
climb pos 800             put the rider 800 m behind the start of the profile
climb show / climb hide   open / close the screen
climb demo off
```

The demo profile runs through the parser as a normal frame. The rider moves with the
speed sensor (in the simulator build with `sim <km/h>`) or by `climb pos`. Frames from
the phone are ignored meanwhile.

## Limits

- Only with a GPX route with altitude data played in TrailBridge. There is no profile
  with OsmAnd navigation.
- Climbs longer than 5 km come on a coarser raster (up to 250 m); "gradient ahead" and
  the profile colours then average over such a section.
- Above 50 km (or with a small MTU) the climb does not fit into one frame. The end of the
  profile then counts as a provisional summit until the next piece arrives.
- Values for `contgrade`, `summitflat` and `summitdip` stored in NVS take precedence over
  new defaults (`climb defaults` resets them).
- If the bike computer connects in the middle of a climb, the climb counts from there.
