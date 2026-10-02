# Ride state machine

State model for RimRidge: which state is behind the state icon (directly above
`rr_line_rq`) and the pause/start button (`rr_btn_pause`), and how that relates to the
statistics (tour/trip/start-ride, average speed without stops/breaks/cruise).

Diagram: [`ride-state-machine.svg`](ride-state-machine.svg).

**Status:** implemented (`src/Stats/Statistics.{h,cpp}`, `src/Stats/Distance.{h,cpp}`, UI
integration §6). Tested on the device with tap/long press via `Tools/uishot.py` and the
simulator build; a real ride is still pending
([cheat sheet](../RIDE-TEST-CHEATSHEET.md)). The icons for free ride and start are
placeholders.

## 1. Principle

Recording (binary log, `L_*.bin`) always runs, independent of the ride state -- this
state machine only influences which parts of the ride count into the "start/ride"
statistics and what icon and button show. GPX and the like are still produced afterwards
in the tooling (`Tools/bikelog/`).

## 2. Two axes, six leaf states

`Statistics::EDrivingState` stays a single flat enum (one leaf = one state-icon image,
one `time_in[][]` slot), but the transitions follow two independent axes:

* **Ride mode** (`rideMode`, manual by a tap on the pause/start button): `false` =
  free ride/cruise, `true` = ride (with coast as a sub-state, cadence hysteresis as in
  the old `Statistics::cycle()` logic: <40 rpm -> coasting, >50 rpm -> power).
* **Movement** (automatic, from the speed): riding / `DS_STOP` (v < 0.3 km/h) /
  `DS_BREAK` (stop > 2 min in one piece, as before). Applies **in every ride mode** -- a
  stop at a traffic light is the same `DS_STOP` whether one was in "ride" or
  "free ride/cruise"; when moving on (v > 5.5 km/h) it goes back into the mode that was
  active before the stop (the value of `rideMode` decides, not the cadence).

```
enum EDrivingState {
    DS_NO_CONN,
    DS_BREAK,
    DS_STOP,
    DS_FREE_RIDE,        // new: rideMode == false, moving
    DS_DRIVE_COASTING,    // rideMode == true, cadence low
    DS_DRIVE_POWER,       // rideMode == true, cadence normal
    EDrivingStateMax
};
```

`DS_FREE_RIDE` covers both the initial state (before the first start tap) and "cruise"
-- **free ride and cruise are exactly the same leaf state**, icon and button symbol do
not differ. The only difference is invisible: whether a ride session is open (§4).

Persistence: times, cadence counters and distances for total, tour and trip are one blob
in NVS (`src/Stats/StatsStore.h`, times by state index). It is written every 5 min, on
the change to stop or no-connection and before powering off
(`Statistics::persistNow()`). A new state changes the layout and needs a new
`StatsStore::VERSION`.

## 3. Button and icon

| State | State icon | Pause/start button (`rr_ic_pause`) | Tap (short) | Long press |
|---|---|---|---|---|
| `DS_FREE_RIDE` | free-ride icon (§6) | start arrow | -> ride (start if no session is open; otherwise resume from cruise) | stop if a session is open; otherwise as before toggle the standby timer (`toggleStandbyMode()`) |
| `DS_DRIVE_POWER` / `DS_DRIVE_COASTING` | "ride"/"coast" icon (`power`/`coasting`) | pause symbol | -> cruise (`DS_FREE_RIDE`, session stays open) | stop (close the session, back to real free ride) |
| `DS_STOP` / `DS_BREAK` | `stop`/`break` | as the value of `rideMode` before the stop (start arrow or pause symbol) | acts on the mode, not on the movement (see below) | as above |

A tap during `DS_STOP`/`DS_BREAK` toggles `rideMode` just like while riding -- so one
can tap pause at the traffic light already, it takes effect when moving on. The movement
axis (stop/break/riding) is unaffected.

The behaviour of the long press is deliberately left context-dependent (conflict with
the existing function): with an open ride session it ends the ride, without an open
session it still does what it did before (extend/disable the standby timer -- the only
control for that on RimRidge). If that collides in everyday use (e.g. one wants to touch
the standby timer *during* a running ride session), a second gesture would be needed --
deliberately not anticipated.

## 4. Ride session (`rideSessionOpen`)

A second flag, independent of `rideMode`/`curDriveState`:

* Becomes `true` on the **first** change `free ride -> ride` (start tap without a session
  already open). At that moment: `Statistics::reset(SUM_ESP_START)` (zeroes times, cruise
  distance, maximum speed, cadence and remembers the distance as the base of the session).
* Stays `true` as long as one switches back and forth between ride and cruise -- exactly
  the requirement "cruise does not end the ride".
* Becomes `false` on the long-press stop. From then on `SUM_ESP_START` stays at the
  values of the ended ride (time does not continue, distance is frozen) until the next
  start tap opens a new session.

`Distance`'s own `SUM_ESP_START` is and stays "since switching on" and is never reset --
binary log (`dist_m`), remaining navigation distance and gradient calculation need a
monotonic counter. The session is a window onto it (`Statistics::sessionBase*`/
`sessionEnd*`, `Statistics::getDistance(SUM_ESP_START)`).

`time_in[DS_FREE_RIDE][SUM_ESP_START]` only accumulates while `rideSessionOpen == true`
-- that is exactly the cruise time of the current ride (§5, `AVG_NOCRUISE`). Before the
first start (or after a stop) the start/ride clock does not run, although technically one
is already riding "free ride".

Tour, trip and total are unaffected by all this: `Statistics::cycle()` keeps writing for
them in every 500 ms tick, independent of ride mode and `rideSessionOpen` -- they "run
along anyway". "Counting the ride into trip/tour at the stop" is therefore not a separate
step but has been happening all the time.

## 5. Statistics: time, distance, averages

Basic rule: **distance always counts, time only with a connected speed sensor.** What
the wheel counter advances while the bike computer is off or the sensor is disconnected
ends up in the distance and additionally in `lostDistanceFromNVS[]` ("without sensor").
The **net distance** (`getDistance(t, false)`) is exactly the part for which there is
time -- it is used for all averages. `DS_NO_CONN` time is counted (diagnosis) but goes
into no time and no average.

```
enum EAvgType {
    AVG_ALL,        // riding + stops + breaks
    AVG_DRIVE,      // riding only (ride, coast and free ride/cruise)
    AVG_NOBREAK,    // riding + short stops, without breaks > 2 min
    AVG_NOCRUISE,   // riding in ride mode only: cruise time AND distance removed
    EAvgTypeMax
};
```

* Moving time = power + coast + free ride. Coast/power is pure display (state icon) or
  for later evaluation, irrelevant for the statistics.
* Stops and breaks add time but (almost) no distance -- that is why `AVG_DRIVE`/
  `AVG_NOBREAK`/`AVG_ALL` only differ in the time, a distance per state is not needed.
* `AVG_NOCRUISE` needs the cruise distance: `Statistics::distFree[]` (only distance in
  `DS_FREE_RIDE`), persisted in NVS like `time_in[][]` (`StatsStore::Summary::distFree`).
  For tour/trip/total this means "only the ride parts of all rides"; whoever never
  presses start has no value there.
* Stop -> break (> 2 min): exactly the time of this stop is moved (`stopEpisodeMs[]`),
  not a blanket `now - timestamp_stop` -- that used to rebook the time of older stops
  after a reconnect or reset.
* A reconnect always goes to `DS_STOP` (not into the state before the loss), so that no
  moving time runs until the first speed arrives.
* Maximum speed is persisted per summary (`SPEED_MAX`), reset with the reset, values
  > 120 km/h discarded; average cadence = crank revolutions / time with cadence > 0 while
  riding (`CAD_MREVS`/`CAD_MS`).

### Counter value of the sensor (`Distance::updateRevs()`)

* Most CSC sensors keep counting cumulatively; cheap ones (CYCPLUS) start at 0 after
  every wake-up. Because the connect takes a while, the first value usually already
  holds a small number -- that was ridden and is counted.
* First value after switching on: counter >= stored -> the difference is "without
  sensor"; counter < stored (restart of the sensor) -> the whole current counter value is
  "without sensor".
* Counter smaller than the last value in operation: "without sensor" on a reconnect,
  normally ridden distance with a running connection; the base is set anew without losing
  what was there (the start distance used to drop to 0 and the new counter value was
  discarded).
* "No value yet" is a flag of its own (`revsInitialized`, `revsKnown[]` via `isKey()`),
  no longer `lastRevs == 0` -- 0 is a valid counter value.

## 6. UI integration

* `UIFacade::updateStateIcon()` takes a third argument (`rideMode`) and maps
  `DS_FREE_RIDE` to its own image (`img_rr_icon_state_freeride`, created as an EEZ
  resource -- a **placeholder**, to be checked by the user in the canvas).
* `rr_btn_pause` has, in addition to `LV_EVENT_LONG_PRESSED`, a handler for
  `LV_EVENT_SHORT_CLICKED` (`action_pause_click()`; not `CLICKED` -- LVGL 8 also sends
  that after a long press, the stop would immediately have started a new session); the
  icon child (`img_rr_icon_pause` vs. `img_rr_icon_start`) changes in
  `ui_RimRidgeUpdateStateIcon()` together with the state icon, since both depend on the
  same `rideMode`/`curDriveState`.
* `UIDriveStateEvent` values `DSE_pauseButtonTap`/`DSE_pauseButtonStop` in
  `src/ui/ui.h`/`ui_events.cpp`, analogous to the standby events.

## 7. Deliberately not implemented / known gaps

* **Power (watts) in the coast criterion:** there is no power meter in the project (no
  cycling power service, see `BLEDevices.cpp`) -- "cadence or power low" is therefore
  gated by cadence only. Without a power meter the coast/power split stays exactly the
  old cadence hysteresis (40/50 rpm).
* **Cadence "connected" flag:** there is no separate "cadence sensor connected" signal
  apart from the speed connection yet. Without a cadence sensor `cadence` stays at its
  last value or at `-1` (invalid) -- `cycle()`'s comparison `cadence < 40`/`> 50` keeps
  treating `-1` as "low" (coasting), as in the old logic. A separate "no cadence sensor
  -> no coast split at all, always power" was not built; it can be added cleanly if
  needed.
* **Manually set total odometer** (odometry page) counts as net distance without
  matching time -- the total average is too high as long as the difference is not
  entered as "lost".
* No host test for `Statistics`/`Distance`: both are tightly coupled to Arduino/ESP32
  (Ticker, Preferences, `heap_caps_*`, singletons), unlike `SessionStats`/`RoadQuality`,
  which were deliberately factored out as pure algorithms. Verification was by the
  PlatformIO build (`pio run`) and code reading; for real run-time tests only a test on
  the device remains.

## 8. Binary log and GPX

`Statistics::logRideState()` writes a `LogRec::RideState` record (type 4,
`src/LogRecords.h`) on every change of `curDriveState`, `rideMode` or the ride session --
not only on state changes, because a tap at standstill or a long press in cruise only
change mode or session. The record carries state, ride mode, flags (`RSF_SESSION_OPEN`,
`RSF_SESSION_START`) and the distance since switching on; `DS_FREE_RIDE` with an open
session is cruise.

`Tools/bikelog/gpx.py` starts a new `<trkseg>` at every change and attaches the state as
`<extensions><bc:RideState>` to the segment (`GpxOptions.ride_states`, CLI
`bikelog gpx --ride-states`). Off by default: coast/power and every stop split the track
into many short segments, useful only for analysing time lost in traffic.
