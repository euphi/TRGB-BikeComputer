# Bekannte Fallstricke

Probleme, die auf dieser Hardware bzw. mit diesem Framework-Stand schon
einmal Zeit gekostet haben -- mit Symptom, Ursache und Regel.

## PSRAM und Display-Flackern

Das RGB-Panel liest seinen Framebuffer per DMA in Echtzeit direkt aus dem
PSRAM, **ohne Bounce-Buffer** (`fb_in_psram = 1`, kein
`bounce_buffer_size_px` in `TRGBArduinoSupport/src/TRGBSuppport.cpp`).
Jeder konkurrierende PSRAM-Zugriff kann den DMA kurz aushungern und wird
als Bildverschiebung sichtbar. Zwei unabhängige Ursachen:

**1. Häufiger PSRAM-Zugriff aus einem Hot-Path** -- Symptom: Pixel
**nach rechts** verschoben, mit Umbruch, unregelmäßig.
Beispiel: `Statistics::timeData.currentMinMax` lag im PSRAM und wurde bei
jeder BLE-Sensor-Notification beschrieben → Flackern etwa 1×/s.

Regel: Vor dem Verschieben einer Datenstruktur ins PSRAM ihre
**Zugriffshäufigkeit und ihren Auslöser** prüfen, nicht nur die Größe.
- OK: groß, aber selten berührt (z. B. Verlaufs-Ring, alle 5-15 s).
- Nicht OK: auch kleine Felder, die aus BLE-Callbacks, pro LVGL-Frame oder
  sonst unvorhersehbar oft geschrieben/gelesen werden -- inkl. Arrays, die
  LVGL direkt referenziert (`lv_chart_set_ext_y_array`).

**2. PSRAM-Allokation vor `trgb.init()`** -- Symptom: Bild **nach links**
verschoben, leicht unregelmäßig, mit Pausen bis ~2 s.
`TRGBSuppport.cpp` legt die beiden 460-KB-LVGL-Draw-Buffer mit einfachem
`heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` ohne Alignment an. Alles, was
vorher PSRAM belegt, verschiebt deren Lage. Statische Objekte (siehe
`Singletons.cpp`) werden vor `main()` konstruiert, also vor `trgb.init()`.

Regel: **Nie PSRAM im Konstruktor eines statisch angelegten Objekts
allokieren**, egal wie selten die Daten benutzt werden. Stattdessen aus
einem `setup()`, das nach `trgb.init()` läuft (Muster:
`Statistics::allocPsramBuffers()`).

Diagnose: Serial-CLI `mem` gibt über `WebInstr::reportDisplayBuffers()`
Adressen und Alignment der Draw-Buffer aus. Den Diagnose-Build **vor** dem
Fix flashen und messen -- nur den reparierten Stand zu messen zeigt den
guten Zustand und führt zum falschen Schluss.

Offen: Ob der eigentliche Hebel das Alignment ist (dann würde ein Patch auf
`heap_caps_aligned_alloc(64, ...)` über `apply_patches.py` die ganze
Klasse erledigen) oder nur die Lage, ist nicht bewiesen.

Seltenes Flackern (2-3×/10 min) und starkes Flackern während OTA-Updates
sind unabhängig davon eine Eigenschaft der Hardware.

## BLE-Stack ist NimBLE, nicht Bluedroid

Mit der pioarduino-Plattform (Arduino-ESP32 3.3.x) läuft der BLE-Stack auf
NimBLE. `BLEAddress` ist eine Kompatibilitätsschicht über beide Stacks und
verhält sich dort asymmetrisch:

- `getNative()` liefert unter NimBLE die Bytes **umgekehrt** (Bluedroid:
  Anzeigereihenfolge).
- `BLEAddress(uint8_t[6])` kopiert unter NimBLE ebenfalls umgekehrt --
  `getNative()` raus / Konstruktor rein ist also **kein** Roundtrip.
- `equals()`/`operator==` vergleicht zusätzlich den Adresstyp.

Regel (`src/BLEDevices.cpp`): mit `getNative()` speichern, beim Laden per
`memcpy` in `getNative()` eines default-konstruierten `BLEAddress`
zurückschreiben; Peers mit dem dateilokalen `sameAddress()` vergleichen,
nicht mit `equals()`. Bei Problemen mit Peer-Identität oder
Adressanzeige zuerst hier suchen.

## Arduino `String` und `c_str()`

BLE-`readValue()`/`getValue()` liefern unter Arduino-ESP32 3.x `String`.
Ein `c_str()`-Zeiger ist nur gültig, solange das `String`-Objekt lebt und
nicht verändert wird. Beim Weiterreichen an LVGL oder über
Task-/Queue-/Callback-Grenzen prüfen, dass niemand den Zeiger behält.
`lv_label_set_text()` kopiert, `lv_label_set_text_static()` **nicht**.

## Zwei USB-Ports

Das Board hat einen nativen USB-CDC-Port (verschwindet bei Hard-Reset kurz
vom Bus) und einen externen USB-Seriell-Wandler (bleibt enumeriert). Ob ein
Reset stattgefunden hat, daher an Boot-Markern im Log erkennen (z. B.
`"👨‍🏭 Start"` aus `BLEDevices::scanAndConnectTask()`), nicht an
USB-Disconnect-Events.

## EEZ Studio / LVGL-Build

- `src/ui_eez/` wird bei jedem Export komplett überschrieben -- eigene
  Logik gehört nach `src/ui/RimRidge*CustFunc.*`.
- EEZ-generierter Code inkludiert `<lvgl/lvgl.h>`; der Shim
  `include/lvgl/lvgl.h` leitet auf `<lvgl.h>` um.
- Der Font `ui_font_by7x128` existiert doppelt (SquareLine-Altbestand in
  `src/ui/font/` und EEZ-Export). `build_src_filter` in `platformio.ini`
  schließt die EEZ-Kopie aus. Bei weiteren gemeinsamen Fonts dort
  ergänzen, sonst "multiple definition" beim Linken.
- Bei der Umrechnung von SquareLine-Layouts gilt: Positionen von Kindern
  eines Flex-Containers sind bedeutungslos, LVGL rechnet sie zur Laufzeit.

## LVGL-Widgets auf EEZ-Screens

Alle Punkte hier scheitern still: kein Build-Fehler, nur falsches Verhalten
auf dem Gerät.

- **Gesten kommen nicht an.** Zwei Bedingungen, beide nötig:
  1. `SCROLLABLE` auf dem Screen-Root und auf dem Container löschen. EEZ
     lässt es auf Page-Roots standardmäßig an; ein scrollbarer Vorfahre
     unterdrückt die Gesten-Erkennung für den ganzen Druck.
  2. `GESTURE_BUBBLE` auf dem **Container mit dem Handler** löschen
     (Kinder dürfen es behalten). LVGL reicht die Geste an den ersten
     Vorfahren *ohne* dieses Flag weiter -- sonst landet sie am
     Screen-Root. Handler direkt auf einem Screen-Root brauchen das nicht.
- **Große Arcs/Slider schlucken Touches.** Nur-Anzeige-Arcs (Speed-,
  Distanz-Ring) brauchen `clickableFlag: false`, sonst beanspruchen sie
  jeden Druck für ihr eigenes Ziehen, und Gesten/Klicks darunter feuern nie.
- **`zoom`/`angle` wirkt nicht** auf Bilder im Format `INDEXED_*` oder
  `ALPHA_1/2/4BIT` (LVGL 8.4 transformiert nur Formate, die der Decoder
  komplett dekodiert: `TRUE_COLOR*`, `ALPHA_8BIT`, `RGB565A8`). Betrifft
  die alten Nav-Icons in `src/ui/img/ui_img_nav_*allimages.c` (1 bit) --
  diese nur in nativer Größe verwenden. Umrechnen auf `ALPHA_8BIT` kostet
  ~4 KB pro 64-px-Icon; Flash ist knapp.
- **Icons rendern schwarz** ohne `img_recolor` im `localStyles`, weil die
  RimRidge-Icons reine Alpha-Masken sind. Bei jedem neuen Icon-Widget
  prüfen. Ebenso: Das Platzhalter-Bitmap im Canvas sollte ungefähr die
  native Auflösung des Bitmaps haben, das zur Laufzeit eingesetzt wird --
  `zoom` skaliert relativ dazu.
- **Platzhalter bleibt sichtbar.** Widgets, die zur Laufzeit per
  `lv_img_set_src()` befüllt werden, starten mit dem sichtbaren
  EEZ-Platzhalter. Wenn die C-Logik nur auf Übergänge reagiert
  (`static bool shown`), beim ersten Aufruf einmal explizit in den
  richtigen Anfangszustand zwingen.
- **Geometrie zur Laufzeit.** Grundregel: Position/Größe gehören ins
  `.eez-project`, nicht in C. Bewusste Ausnahme: der "geschobene" Zustand
  von `rr_nav_pill` bei sichtbarer Spur-Anzeige (EEZ kann keine
  zustandsabhängige Geometrie ausdrücken). Die Ruheposition im Canvas
  bleibt maßgeblich.

## `xUIDrawMutex` und EEZ-Actions

EEZ-Action-Callbacks laufen synchron in `lv_timer_handler()`, und das ruft
`UIFacade::updateHandler()` bei gehaltenem `xUIDrawMutex` auf. Der Mutex
ist nicht rekursiv; ein zweites `xSemaphoreTake` aus demselben Task läuft
in den Timeout. Jede `UIFacade`-Methode, die eine Action erreichen kann,
braucht das Muster
`bool uiTask = isDrawTask(); if (uiTask || xSemaphoreTake(...))`.

## Struct-Layouts: `time_t` ist 8 Byte

Auf dieser Toolchain ist `time_t` 8 Byte, nicht 4 -- relevant für
`BCLogger::LogData` und alles, was binär geschrieben und in `Tools/`
gelesen wird. Echte Offsets ermitteln statt zählen: absichtlich falsches
`static_assert(offsetof(T, feld) == 999, "x")` -- GCC meldet den echten
Wert in "the comparison reduces to ...".

## Serielle Konsole

`src/SerialConsole.*` hält die Eingabezeile (Prompt, halb getippter Befehl) als letzte
Bildschirmzeile. Zwei Regeln:

- **Serial-Ausgaben aus anderen Tasks** gehen über `bclog` oder werden in
  `SerialConsole::Output out(console);` eingeschlossen -- sonst landen sie mitten
  in der Eingabezeile. Befehls-Callbacks laufen im Loop-Task ohne sichtbaren
  Prompt und dürfen direkt `Serial.print` benutzen, sollten aber mit
  Zeilenumbruch enden.
- **Keine ANSI-Escape-Sequenzen und kein `\a` ausgeben.** Der Standardfilter von
  miniterm und `pio device monitor` zeigt ESC und die meisten Steuerzeichen als
  Symbol (`␛[K`). Die Konsole zeichnet deshalb nur mit `\r`, `\b` und Leerzeichen.

Befehle mit `console.addCmd()` registrieren, nicht mit `cli.addCmd()`, sonst gibt
es keine Tab-Completion (SimpleCLI bietet keine Liste der Befehle an).

## Webserver: Regex-Routen und Stack-Größe

`ASYNCWEBSERVER_REGEX` bleibt bewusst aus: `AsyncCallbackWebHandler::canHandle()`
baut für jede Regex-Route bei **jedem** Request ein `std::regex` auf dem
internen Heap. Routen als Präfix-Routen anlegen (siehe
`src/WifiWebserver.cpp`).

`CONFIG_ASYNC_TCP_STACK_SIZE` ist auf gemessene Auslastung plus Reserve
eingestellt (Details im Kommentar in `platformio.ini`). Nach Änderungen an
Web-Handlern oder OTA neu messen (`mem` auf der Serial-CLI), bevor der
Wert weiter gesenkt wird.

## Interner Heap ist das knappe Gut

PSRAM ist reichlich da, aber alles unter `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL`
(4 KB) sowie Task-Stacks, FreeRTOS-Queues/-Puffer, offene SD-Dateien (~4 KB
je Datei), WLAN, BLE-Verbindungen und jede TCP-Verbindung des Webservers
kommen aus dem **internen** Heap. Gemessen 2026-09-26 (`mem`, Zeile `MEM int=`):
nach dem Boot mit WLAN + TrailBridge + HR ca. 50–64 KB frei; 8 parallele
HTTP-Requests ziehen davon ~45–60 KB ab. Läuft der interne Heap leer,
hängen Webserver/mDNS, und das Gerät kann abstürzen.

Regel: Jeden neuen dauerhaften Puffer/Stack/offenen File im internen RAM mit
`mem` vorher/nachher messen (Leerlauf und unter Web-Last). Dateien, die selten
geschrieben werden, nicht die ganze Sitzung offen halten. Die Middleware in
`WifiWebserver.cpp` lehnt Requests unter 20 KB freiem internem Heap mit 503
ab -- das schützt aber erst nach dem Annehmen der Verbindung.

## SD-Karte: offene Dateien nicht löschen oder umbenennen

`CONFIG_FATFS_FS_LOCK` ist 0: FATFS verhindert nicht, dass eine Datei gelöscht
oder umbenannt wird, die ein anderer Task noch offen hat -- das Ergebnis ist ein
beschädigtes Dateisystem, kein Fehlercode. Die Dateien der laufenden Sitzung in
`/BIKECOMP/CUR/` sind die ganze Fahrt offen. `BCLogger::deleteFile()` und der
Cleanup lassen sie deshalb aus (`isActiveSessionFile()`), ebenso alles in `CUR/`,
solange der Finalizer (`LogSessions`) läuft. Neue Wege, Dateien zu löschen oder
zu verschieben, brauchen dieselbe Prüfung.

## BMI160: Register-Reads nie ungeprüft

`BMI160Gen::serial_buffer_transfer()` prüft nicht, ob `requestFrom()` alle
Bytes geliefert hat, und lässt bei einem kurzen Read alte Pufferbytes stehen.
Beim FIFO-Füllstand (2 Byte) ergab das sporadisch Werte von ~80 statt ~2
Frames; der leere FIFO liefert dann `0x8000`-Frames (-16 g auf allen Achsen),
die als 31-g-Stöße erkannt wurden. Regel: FIFO-Zähler und -Daten über
`I2CSensors::imuRead()` lesen (Repeated Start, Längenprüfung) und
`0x8000/0x8000/0x8000`-Frames verwerfen -- beides ist dort umgesetzt, die
Zähler stehen auf `/debug/imu`.

## Abstürze ohne USB: Neustart-Grund und Core-Dump

- Beim Boot steht im Debug-Log `Reset reason: …`. Bei PANIC oder einem Watchdog
  liegt ein Core-Dump in der Partition `coredump`. Jeder Absturz überschreibt den vorigen.
- Anzeige unter `/debug/coredump`: Task, PC, Backtrace und ob der Dump zur laufenden
  Firmware passt.
- Der Dump wird **nur auf Anfrage** gelesen, nie beim Boot. Ein früherer
  Kopierversuch beim Boot hat eine Endlosschleife aus Abstürzen ausgelöst
  (TODO in `BCLogger::setup()`). Ohne USB wäre das Gerät dann nicht mehr erreichbar.
- Auflösen geht nur mit der **ELF genau der abgestürzten Firmware**. Vor jedem neuen Build
  `.pio/build/trgb-esp32-s3/firmware.elf` sichern, wenn noch ein Dump aussteht.
  Ob die Datei passt, zeigen die ersten Zeichen von `sha256sum firmware.elf`.
  Befehl:
  ```
  curl -o coredump.bin http://<ip>/debug/coredump.elf
  esp-coredump info_corefile -t raw \
      --gdb ~/.platformio/packages/tool-xtensa-esp-elf-gdb/bin/xtensa-esp32s3-elf-gdb \
      -c coredump.bin firmware.elf
  ```
  `-t raw`, weil die Partition vor der ELF noch einen Kopf hat.
- Bei „Task watchdog … IDLE0“ ist der Task mit dem Absturz meist nur der, der gerade lief.
  Wichtig ist die Thread-Liste: Wer steht **nicht** in einer Wartefunktion
  (`0x400559e0 in ??` = blockiert)?

## Nie `vTaskDelay(0)` in einer Task-Schleife mit hoher Priorität

- `vTaskDelay(0)` gibt nur an Tasks gleicher oder höherer Priorität ab, nie an IDLE0.
- Der UI-Task (Priorität 20) hat mit `vTaskDelay(next_ms)` so lange gerechnet, wie LVGL
  `0` zurückgab (Rendering kam nicht hinterher, viele Updates nach BLE-Reconnects).
  Nach 5 s hat der Task-Watchdog das Gerät neu gestartet (Core-Dump 2026-09-27).
- Jetzt gilt ein Minimum von 2 ms in `UIFacade::updateHandler()`. Dasselbe gilt für
  jede eigene Schleife: immer mindestens 1 Tick warten.
