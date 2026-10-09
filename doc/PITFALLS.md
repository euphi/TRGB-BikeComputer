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

## Probably at most 3 BLE connections (suspected, not yet measured)

The test ride of 2026-10-04: TrailBridge, heart-rate belt and speed sensor connected, the cadence
sensor (`CYCPLUS C3`) was found 7 times in a row and every `connect()` failed at once ("Can't
connect"); after a minute it was gone for good (asleep). On 2026-09-27 the 4th connection failed
the same way, there it was the speed sensor. NimBLE's default is `CONFIG_BT_NIMBLE_MAX_CONNECTIONS
= 3`; the bike computer wants four peers (nav+GPS+profile on one, HR, two CSC). Deleting the slot
changes nothing, as the log shows. To check: look at what `BLEClient::connect()` returns / log the
NimBLE error code and the number of connected clients when a connect fails. A fix needs a different
sdkconfig (pioarduino `custom_sdkconfig`, which compiles the IDF libraries) or the NimBLE-Arduino
library with `-DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=4` -- every connection costs internal RAM.
The log line of a failed connect now says how many other peers were connected; failures always at 3
would confirm it. Since 2026-10-09 every environment builds with
`CONFIG_BT_NIMBLE_MAX_CONNECTIONS=5` through `custom_sdkconfig` (next section; the fifth is for a
power meter); it runs on the device, more than one peer at once has not been tried yet.

## `custom_sdkconfig` (hybrid compile) rebuilds the shared framework package

`custom_sdkconfig` in the `[env]` section of `platformio.ini` (five BLE connections) makes pioarduino
compile the IDF libraries itself. That downloads ESP-IDF, cmake and ninja once (about 1 GB more in
the core directory) and takes about 11 minutes (18 with the downloads). The result is written
**into the shared package** `framework-arduinoespressif32-libs` of the PlatformIO core directory,
marked by a file `sdkconfig` there. The matching hash sits in the first line of `sdkconfig.defaults`
in the project directory.

Measured 2026-10-09 (full build of the application each time): without `custom_sdkconfig` 3:16 min,
with it and the libraries already compiled 2:39 to 3:05 min. So once the libraries exist, a build
takes as long as before. They are compiled again only when

- the value of `custom_sdkconfig`, the board's memory type or the platform version changes,
- `sdkconfig.defaults` is missing or does not match. That is why the file is **checked in**: a fresh
  checkout or worktree with the file builds in 2:44 min, without it the framework is reinstalled and
  the libraries are compiled again;
- the core directory was last used by a project (or an older branch of this one) **without**
  `custom_sdkconfig`. Such a build sees the marker, prints `*** Reinstall Arduino framework ***`,
  deletes both framework packages and downloads them again; the next build here compiles the
  libraries again. So every environment carries the same value (it lives in `[env]`), and switching
  between `trgb-esp32-s3` and `trgb-esp32-s3-sim` reinstalls nothing.

A reinstall deletes packages another build may be reading, and the platform also re-checks its tool
packages (`tool-scons` among them) from inside the running build. Two builds sharing one core
directory must therefore not run at the same time while one of them reinstalls the framework. A
build that dies with a missing module of a tool package (`ModuleNotFoundError: No module named
'SCons.Tool.FortranCommon'` while linking, seen on 2026-10-08) fits that; it could not be reproduced
with a core directory used by one build only, neither a fresh one nor a copy of an existing
installation. For a branch that still needs the stock libraries, use a core directory of its own
(`PLATFORMIO_CORE_DIR=...`) -- and a build directory of its own with it (`PLATFORMIO_BUILD_DIR=...`),
because PlatformIO empties `.pio/build` completely as soon as a build runs with a different core
directory, and with it the `firmware.elf` needed to resolve a core dump of the firmware on the
device.

**A hybrid build needs the PSRAM boot settings spelled out.** The libraries pioarduino ships for
`qio_opi` set `CONFIG_SPIRAM_BOOT_HW_INIT`, `CONFIG_SPIRAM_BOOT_INIT` and
`CONFIG_SPIRAM_IGNORE_NOTFOUND`; a hybrid build starts from the generic ESP32-S3 configuration, where
they are off. Without them the firmware panics while booting. Seen from outside that looked like
this on 2026-10-09: the OTA upload ends with "OK", the device answers again, **but it runs the
previous firmware** -- rollback is enabled (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`), an image that
does not reach a clean start is dropped. The only traces are "Last reset PANIC" on `/debug/coredump`
(without a new core dump) and the old version on the settings screen. So after every OTA of a build
with changed `custom_sdkconfig`, check the version and the build tags there (`5xBLE I2C`:
connection limit of the libraries, I2C sensors; `STD_BLE` are the stock libraries, `FL` the
Forumslader variant). To see
what else differs from the stock libraries, diff `esp32s3/qio_opi/include/sdkconfig.h` of the two
`framework-arduinoespressif32-libs` packages; left as they are: static instead of dynamic WiFi TX
buffers, no TinyUSB.

Measured 2026-10-09 on the device (WiFi + TrailBridge navigating, line `MEM int=`): stock libraries
(three connections) 45 KB free internal heap, low mark 27 KB, 6745 KB PSRAM free; hybrid build with
five connections 46 KB, low mark 35 KB, 6279 KB PSRAM free. The two extra connection slots cost no
internal heap worth mentioning, the hybrid libraries take about 470 KB more PSRAM. What four or five
connected peers cost is still open.

The library build also leaves `sdkconfig.<env>`, `managed_components/` (570 MB) and `.dummy/` in the
project directory (ignored by git).

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

## Reading a characteristic of the TrailBridge peer (route overview)

The route overview service is read only; a revision tag in the nav frames says when to read
it. `readValue()` blocks until the whole value is in (a long read of up to 512 byte, several
round trips), and the BLE host task that delivers the answer is the one that calls
`onDisconnect()`. Rules (`BLEDevices::overviewReaderTask()`):

- **Never read in an indicate callback** (it runs in the host task: deadlock), and not in the
  scan task either (it sleeps 20 s between its rounds, so a revision would be read half a
  minute late). The read has a task of its own, woken by the nav frame.
- `xNavIoMutex` keeps that read away from the connect/subscribe sequence of the same link
  and from the **deletion of the client**: the characteristic pointer belongs to the
  `BLEClient`, so `onDisconnect()` and `checkNavAlive()` clear it under this mutex before they
  delete the client. **Lock order: `xNavIoMutex` before `xDevMutex`**, never the other way.
- `onDisconnect()` takes `xNavIoMutex` only for the TrailBridge client (pointer comparison).
  For any other peer it would stall the host task while a read waits for that very task.
- Not tried on the device: the long read over a real link (read from the library source),
  and that the pending read really ends before `onDisconnect()` runs (otherwise the 1.5 s
  timeout there applies and the client is deleted anyway).

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

- **Keyboard (`lv_keyboard`) with the RimRidge fonts**: those fonts have no `LV_SYMBOL_*` glyphs
  (backspace, OK), so the keyboard items use the built-in `MONTSERRAT_22`. The default layout has
  12 keys per row (30 px here); `RimRidgeWifiCustFunc.cpp` sets own maps and its own key handler,
  so the EEZ canvas shows the LVGL default layout, the device the QWERTZ one.
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
- **`LV_LABEL_LONG_DOT` wraps first.** It breaks the text at a word and dots the last line
  that fits: a one-line row with a long name shows "Kreuzung Alte..." although more would
  fit. Needs a fixed height, too, else the label just wraps. For a single line, cut the text
  yourself (`fitText()` in `src/ui/RimRidgeRouteCustFunc.cpp`) and use `LV_LABEL_LONG_CLIP`.
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

## WiFi: secrets, hotspot memory, scans

- **Passwords only in POST bodies and never in a log line.** `WebInstr` and the console log the
  URL / the command line to the SD card, which the log service uploads. `/wifi/add` and
  `/wifi/ap` therefore take POST bodies, and `SerialConsole::run()` logs `wifi add|apset`
  without its arguments. Anything new that handles a secret needs the same.
- **The hotspot costs ~14 KB of internal heap** (AP + STA mode, DNS task, clients). Idle that
  leaves ~26 KB; with a phone attached, `/debug/ui/snap` (screenshot over HTTP) took the
  minimum down to 3.8 KB (2026-10-03). Don't take screenshots over the hotspot; normal pages
  are fine.
- **Don't hold `WifiWebserver::cfgMutex` while calling `ui.*`**: the UI task takes
  `xUIDrawMutex` first and then asks `webserver` for status, so the reverse order deadlocks.
- A **scan takes ~10 s** while BLE scans (shared radio) and fails to start while a connect
  attempt is still running -- the autoconnect then just uses the previous result.
- Android keeps using mobile data for the browser on a WiFi without internet unless the network
  announces itself as a captive portal. The hotspot's DNS + redirect (`startCaptiveDns()`,
  not-found handler) is what makes `http://192.168.4.1/wifi` reachable from a phone.

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
- Resolving it needs the **ELF of exactly the firmware that crashed**. Every
  build is archived automatically (`archive_firmware.py`) in `firmware-archive/builds/<id>/`;
  `<id>` is the "Firmware ELF" shown on `/debug/coredump`. `Tools/fwarchive.sh <id>` unpacks
  the ELF and prints its path (no argument: list the builds). Command:

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
