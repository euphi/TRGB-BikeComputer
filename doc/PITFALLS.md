# Known pitfalls

Problems that have already cost time on this hardware or with this state of the
framework -- with symptom, cause and rule.

## PSRAM and display flicker

The RGB panel reads its framebuffer by DMA in real time straight from PSRAM, **without a
bounce buffer** (`fb_in_psram = 1`, no `bounce_buffer_size_px` in
`TRGBArduinoSupport/src/TRGBSuppport.cpp`). Every competing PSRAM access can starve the
DMA for a moment and shows as a shifted picture. Two independent causes:

**1. Frequent PSRAM access from a hot path** -- symptom: pixels shifted **to the right**,
wrapping around, irregular. Example: `Statistics::timeData.currentMinMax` was in PSRAM
and written on every BLE sensor notification → flicker about once a second.

Rule: before moving a data structure into PSRAM, check **how often it is accessed and
what triggers the access**, not only its size.

- OK: large but rarely touched (e.g. a history ring, every 5-15 s).
- Not OK: even small fields that are written or read from BLE callbacks, per LVGL frame
  or otherwise unpredictably often -- including arrays that LVGL references directly
  (`lv_chart_set_ext_y_array`).

**2. PSRAM allocation before `trgb.init()`** -- symptom: picture shifted **to the left**,
slightly irregular, with pauses of up to ~2 s. `TRGBSuppport.cpp` allocates the two
460 KB LVGL draw buffers with a plain `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` without
alignment. Everything that takes PSRAM before that moves their position. Static objects
(see `Singletons.cpp`) are constructed before `main()`, i.e. before `trgb.init()`.

Rule: **never allocate PSRAM in the constructor of a statically created object**, no
matter how rarely the data is used. Do it from a `setup()` that runs after `trgb.init()`
(pattern: `Statistics::allocPsramBuffers()`).

Diagnosis: the serial command `mem` prints the addresses and alignment of the draw
buffers via `WebInstr::reportDisplayBuffers()`. Flash the diagnostic build **before** the
fix and measure -- measuring only the repaired state shows the good state and leads to
the wrong conclusion.

Open: whether the real lever is the alignment (then a patch to
`heap_caps_aligned_alloc(64, ...)` via `apply_patches.py` would settle the whole class)
or only the position has not been proven.

Heavy flicker during OTA updates is, independently of that, a property of the hardware:
while the flash is written or erased, the bus over which the panel reads its picture
from PSRAM stands still. The same thing on a small scale is the third cause:

**3. Writing to NVS** -- symptom: a single short flicker, regularly (before 2026-10
every 45-75 s while riding, 2-3 times per 10 min at standstill). See the next section.

## Writing to NVS and display flicker

It is not the single write that flickers but the **erase of an NVS page** (sector erase,
~100 ms measured). NVS only appends new values; when the page is full, it copies the
entries that are still valid and erases a page. How often that happens can be calculated:

- A page holds 126 entries. A number (up to 64 bit) costs 1 entry, a string or blob
  `2 + size/32` (rounded up). **A `putFloat()` is a blob, i.e. 3 entries.**
- Each erase frees about `126 × (1 − fill level)` entries. The fill level is shown on
  `/debug/nvs`.
- Erases per time = entries written per time / entries freed.

Example before 2026-10: fill level ~70 % → ~36 entries per erase (measured: every twelfth
`putFloat()` took ~100 ms longer). The statistics wrote ~48 entries/min while riding →
one erase every ~45 s.

Rules:

- **Nothing periodic and fast into NVS.** The lever is the write rate, not the partition
  size: a larger partition only lowers the fill level and gains a factor of 126/36 at most.
- Store values that belong together as **one struct blob** (`NvsUtil::loadBlob()`/
  `saveBlob()`, `src/NvsUtil.h`), not as single keys -- above all no single floats.
- NVS does not write unchanged values (ESP-IDF compares before writing). An own "only if
  changed" only saves the read.
- The ride statistics (`Statistics::persistNow()`) write a blob of 8 entries every 5 min,
  when stopping and before powering off -- that is about one erase in 30-50 min. Whoever
  adds something there does the sum again.
- Write NVS only from tasks with enough stack, never from the UI task (see "UI task:
  hardly any stack left"). Pattern: `Statistics::requestPersist()` only sets a flag, the
  write happens in the next `cycle()`.

## The BLE stack is NimBLE, not Bluedroid

With the pioarduino platform (Arduino-ESP32 3.3.x) the BLE stack runs on NimBLE.
`BLEAddress` is a compatibility layer over both stacks and behaves asymmetrically there:

- `getNative()` returns the bytes **reversed** under NimBLE (Bluedroid: display order).
- `BLEAddress(uint8_t[6])` also copies reversed under NimBLE -- `getNative()` out /
  constructor in is therefore **not** a round trip.
- `equals()`/`operator==` additionally compares the address type.

Rule (`src/BLEDevices.cpp`): store with `getNative()`, on loading write back by `memcpy`
into `getNative()` of a default-constructed `BLEAddress`; compare peers with the
file-local `sameAddress()`, not with `equals()`. With problems of peer identity or
address display, look here first.

## TrailBridge: no stored address, and no disconnect to rely on

Android advertises under an address that changes -- on this phone with every restart of
the app and after a failed connect, not only every few minutes. The TrailBridge slot
therefore remembers no address, not even in RAM (it did until 2026-10, and refused the
phone after the first lost connection until the next restart). It takes the first phone
found while it has no connection and refuses every other TrailBridge address while it
has one: the connected phone keeps advertising, possibly under a new address already.

When the app is stopped or restarted, Android keeps the link itself up; no disconnect
arrives. The dead connection is detected by the heartbeat of the protocol instead: no
nav or GPS frame for 30 s ends it (`BLEDevices::checkNavAlive()`), the next scan
connects again. Measured 2026-10-02: 46 s from restarting the app to the new connection.

## Arduino `String` and `c_str()`

BLE `readValue()`/`getValue()` return `String` under Arduino-ESP32 3.x. A `c_str()`
pointer is only valid as long as the `String` object lives and is not modified. When
passing it on to LVGL or across task, queue or callback boundaries, check that nobody
keeps the pointer. `lv_label_set_text()` copies, `lv_label_set_text_static()` does **not**.

## Two USB ports

The board has a native USB CDC port (disappears from the bus for a moment on a hard
reset) and an external USB serial converter (stays enumerated). Whether a reset has
happened is therefore to be seen from boot markers in the log (e.g. `"👨‍🏭 Start"` from
`BLEDevices::scanAndConnectTask()`), not from USB disconnect events.

When opening `/dev/ttyACM0` from own scripts, do **not** set `dtr = False`/`rts = False`
before `open()`: pyserial then switches DTR off first, and DTR=0 with RTS=1 triggers a
reset on the USB-JTAG port ("Reset reason: USB"). With the defaults (both set) the board
stays on.

## UI task: hardly any stack left

The UI task (`UIFacade::initDisplay()`, 4096 bytes) has less than 1 KB free in operation
(`mem` → `STACK UI Task`). EEZ actions and LVGL timers run in this task. Don't run
anything heavy there directly but start a short-lived task of its own: NVS writes, SD
access, `delay()`, rendering a whole screen (`lv_snapshot` puts another ~500 bytes of
display structures on the stack). Pattern: `shutdownTask()` in
`src/ui/RimRidgeSettingsCustFunc.cpp`, `snapTask()` in `src/UiDebug.cpp` (with
`UIFacade::runLocked()`).

## Missing lines in the debug log

`BCLogger::log()` waits at most 100 ms for the file mutex and drops the line after that
(only `"File Log output blocked"` on `printf`). This happens while an SD flush or an NVS
write is running, e.g. when the distance is saved. A missing log line therefore does not
prove that the code did not run; rather check a consequence (reset reason, stored value,
later line).

## EEZ Studio / LVGL build

- `src/ui_eez/` is overwritten completely on every export -- own logic belongs into
  `src/ui/RimRidge*CustFunc.*`.
- EEZ-generated code includes `<lvgl/lvgl.h>`; the shim `include/lvgl/lvgl.h` redirects
  to `<lvgl.h>`.
- When converting SquareLine layouts: positions of children of a flex container are
  meaningless, LVGL computes them at run time.

## LVGL widgets on EEZ screens

All points here fail silently: no build error, only wrong behaviour on the device.

- **Gestures don't arrive.** Two conditions, both needed:
    1. Clear `SCROLLABLE` on the screen root and on the container. EEZ leaves it on for
       page roots by default; a scrollable ancestor suppresses gesture detection for the
       whole press.
    2. Clear `GESTURE_BUBBLE` on the **container with the handler** (children may keep
       it). LVGL passes the gesture on to the first ancestor *without* this flag --
       otherwise it ends up at the screen root. Handlers directly on a screen root don't
       need this.
- **Large arcs/sliders swallow touches.** Display-only arcs (speed ring, distance ring)
  need `clickableFlag: false`, otherwise they claim every press for their own dragging,
  and gestures/clicks underneath never fire.
- **`zoom`/`angle` has no effect** on images in the format `INDEXED_*` or
  `ALPHA_1/2/4BIT` (LVGL 8.4 only transforms formats that the decoder decodes completely:
  `TRUE_COLOR*`, `ALPHA_8BIT`, `RGB565A8`). Concerns the old nav icons in
  `src/ui/img/ui_img_nav_*allimages.c` (1 bit) -- use these in native size only.
  Converting to `ALPHA_8BIT` costs ~4 KB per 64 px icon; flash is tight.
- **Icons render black** without `img_recolor` in `localStyles`, because the RimRidge
  icons are pure alpha masks. Check for every new icon widget. Likewise: the placeholder
  bitmap in the canvas should have roughly the native resolution of the bitmap that is
  set at run time -- `zoom` scales relative to it.
- **Placeholder stays visible.** Widgets that are filled at run time by
  `lv_img_set_src()` start with the visible EEZ placeholder. If the C logic only reacts
  to transitions (`static bool shown`), force the right initial state explicitly once on
  the first call.
- **Geometry at run time.** Basic rule: position and size belong into the
  `.eez-project`, not into C. Deliberate exception: the "pushed" state of `rr_nav_pill`
  when the lane display is visible (EEZ cannot express state-dependent geometry). The
  rest position in the canvas stays authoritative.

## `xUIDrawMutex` and EEZ actions

EEZ action callbacks run synchronously in `lv_timer_handler()`, and
`UIFacade::updateHandler()` calls that while holding `xUIDrawMutex`. The mutex is not
recursive; a second `xSemaphoreTake` from the same task runs into the timeout. Every
`UIFacade` method that an action can reach needs the pattern
`bool uiTask = isDrawTask(); if (uiTask || xSemaphoreTake(...))`.

## LVGL only under `xUIDrawMutex` -- in the UI task itself, too

- Symptom: endless lines `lcd_panel: esp_lcd_panel_draw_bitmap(35): start position must
  be smaller than end position` on the serial console, then "Task watchdog … async_tcp",
  running `UI Task` (core dump of 2026-10-02 00:22).
- Cause: two tasks enter an invalid area at the same time (`_lv_inv_area()`), what
  remains is one mixed from both with `y2 < y1`. On that, `refr_area()` in LVGL 8.4 runs
  in a loop with a negative line count; every pass calls the display driver with an
  invalid area.
- The fast and the slow block in `UIFacade::updateHandler()` ran without the mutex
  because they run in the UI task. But that only protects against `lv_timer_handler()`,
  not against the BLE task (navigation frames) and the `esp_timer` task (driving state),
  which draw under the mutex. It only showed with the TrailBridge test ride: navigation
  frame and simulated speed both arrive once a second.
- Rule: every LVGL call outside `lv_timer_handler()` needs the mutex, also in the UI
  task. The core dump only contains stacks; the area is in `sub_area` in the frame of
  `refr_area`.

## Struct layouts: `time_t` is 8 bytes

On this toolchain `time_t` is 8 bytes, not 4 -- relevant for `BCLogger::LogData` and
everything that is written in binary and read in `Tools/`. Determine real offsets instead
of counting: a deliberately wrong `static_assert(offsetof(T, field) == 999, "x")` -- GCC
reports the real value in "the comparison reduces to ...".

## Serial console

`src/SerialConsole.*` keeps the input line (prompt, half-typed command) as the last line
on the screen. Two rules:

- **Serial output from other tasks** goes through `bclog` or is wrapped in
  `SerialConsole::Output out(console);` -- otherwise it lands in the middle of the input
  line. Command callbacks run in the loop task without a visible prompt and may use
  `Serial.print` directly, but should end with a line break.
- **Don't output ANSI escape sequences or `\a`.** The default filter of miniterm and
  `pio device monitor` shows ESC and most control characters as a symbol (`␛[K`). The
  console therefore draws with `\r`, `\b` and spaces only.

Register commands with `console.addCmd()`, not with `cli.addCmd()`, otherwise there is no
Tab completion (SimpleCLI does not offer a list of its commands).

## Web server: regex routes and stack size

`ASYNCWEBSERVER_REGEX` deliberately stays off: `AsyncCallbackWebHandler::canHandle()`
builds a `std::regex` on the internal heap for every regex route on **every** request.
Create routes as prefix routes (see `src/WifiWebserver.cpp`).

`CONFIG_ASYNC_TCP_STACK_SIZE` is set to the measured usage plus a reserve (details in
the comment in `platformio.ini`). After changes to web handlers or OTA, measure again
(`mem` on the serial console) before lowering the value further.

## The internal heap is the scarce resource

There is plenty of PSRAM, but everything below `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL`
(4 KB) as well as task stacks, FreeRTOS queues and buffers, open SD files (~4 KB per
file), WiFi, BLE connections and every TCP connection of the web server come from the
**internal** heap. Measured 2026-09-26 (`mem`, line `MEM int=`): after boot with WiFi +
TrailBridge + heart rate about 50–64 KB free; 8 parallel HTTP requests take ~45–60 KB of
that. If the internal heap runs empty, web server and mDNS hang, and the device can crash.

Rule: measure every new permanent buffer, stack or open file in internal RAM with `mem`
before and after (idle and under web load). Don't keep files that are rarely written open
for the whole session. The middleware in `WifiWebserver.cpp` rejects requests below 20 KB
of free internal heap with 503 -- but that only protects after the connection has been
accepted.

## SD card: don't delete or rename open files

`CONFIG_FATFS_FS_LOCK` is 0: FATFS does not prevent a file from being deleted or renamed
while another task still has it open -- the result is a damaged file system, not an error
code. The files of the running session in `/BIKECOMP/CUR/` are open for the whole ride.
`BCLogger::deleteFile()` and the cleanup therefore skip them (`isActiveSessionFile()`),
as well as everything in `CUR/` while the finalizer (`LogSessions`) is running. New ways
of deleting or moving files need the same check.

## I²C: `Wire` only under `I2CBus::Guard`

Touch controller (UI task), BME280 (esp_timer task and the task that delivers the wheel
revolutions) and BMI160 (ImuTask) share `Wire`. `TwoWire`'s own lock only covers the bus
transfer; the receive buffer is read after it is released -- by `available()`/`read()`
and by the return value of `requestFrom()`. If another task starts a transfer in
between, the first one gets the other device's bytes or a wrong length. Once the read
index is ahead of the length, `available()` stays true while `read()` returns nothing:
`while (Wire.available()) Wire.read();` in the SparkFun BME280 library then never ends.
In the esp_timer task that was a task-watchdog reset (core dump of 2026-10-02), in the
ImuTask it shows up as "short read" errors.

Rule: hold an `I2CBus::Guard` (`src/I2CBus.h`) from the start of a transfer until its
bytes are out of `Wire`; wrap library calls as a whole. The touch read callback of
TRGBArduinoSupport gets the lock from `I2CBus::guardTouch()`. A new I²C device or a new
library call needs the same.

## BMI160: never read registers unchecked

`BMI160Gen::serial_buffer_transfer()` does not check whether `requestFrom()` delivered
all bytes, and leaves old buffer bytes in place on a short read. For the FIFO fill level
(2 bytes) this sporadically gave values of ~80 instead of ~2 frames; the empty FIFO then
delivers `0x8000` frames (-16 g on all axes), which were detected as 31 g shocks. Rule:
read FIFO count and data through `I2CSensors::imuRead()` (repeated start, length check)
and discard `0x8000/0x8000/0x8000` frames -- both are implemented there, the counters
are on `/debug/imu`.

## Crashes without USB: reset reason and core dump

- At boot the debug log contains `Reset reason: …`. After a PANIC or a watchdog there is
  a core dump in the partition `coredump`. Every crash overwrites the previous one.
- Shown on `/debug/coredump`: task, PC, backtrace and whether the dump matches the
  running firmware.
- The dump is read **only on request**, never at boot. An earlier attempt to copy it at
  boot caused an endless loop of crashes (TODO in `BCLogger::setup()`). Without USB the
  device would then be unreachable.
- Resolving it needs the **ELF of exactly the firmware that crashed**. Before every new
  build, save `.pio/build/trgb-esp32-s3/firmware.elf` if a dump is still pending. Whether
  the file matches is shown by the first characters of `sha256sum firmware.elf`. Command:

    ```
    curl -o coredump.bin http://<ip>/debug/coredump.elf
    esp-coredump info_corefile -t raw \
        --gdb ~/.platformio/packages/tool-xtensa-esp-elf-gdb/bin/xtensa-esp32s3-elf-gdb \
        -c coredump.bin firmware.elf
    ```

    `-t raw` because the partition has a header in front of the ELF.
- With "Task watchdog … IDLE0" the task of the crash is usually just the one that
  happened to run. What matters is the thread list: who is **not** in a wait function
  (`0x400559e0 in ??` = blocked)?

## Never `vTaskDelay(0)` in a task loop with high priority

- `vTaskDelay(0)` only yields to tasks of the same or higher priority, never to IDLE0.
- The UI task (priority 20) kept computing with `vTaskDelay(next_ms)` for as long as LVGL
  returned `0` (rendering could not keep up, many updates after BLE reconnects). After
  5 s the task watchdog restarted the device (core dump of 2026-09-27).
- Now a minimum of 2 ms applies in `UIFacade::updateHandler()`. The same holds for every
  loop of one's own: always wait at least 1 tick.
