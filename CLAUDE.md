# TRGB-BikeComputer -- Projektkontext für Claude Code

ESP32-S3-Fahrradcomputer auf LilyGO T-RGB (480×480 rundes RGB-Display,
LVGL 8.4). Sensoren: Herzfrequenz, CSC (Speed/Cadence), Forumslader
(nur FL-Variante), BME280, BMI160. Navigation und GPS-Position kommen von
der Android-Companion-App [TrailBridge](../TrailBridge/) (Paket
`com.euphi.trailbridge`).

**Stand und offene Arbeit:** [`doc/ROADMAP.md`](doc/ROADMAP.md) (Reifegrad je Feature,
bekannte Fehler, geplante Features), Bedienung unterwegs:
[`doc/USABILITY-TODO.md`](doc/USABILITY-TODO.md). Beide aktuell halten, wenn ein
Punkt erledigt ist -- erledigte Punkte dort löschen, Features im
[`README.MD`](README.MD) und in `doc/index.de.md` nachtragen.

**Doku ist zweisprachig** und wird als Seite veröffentlicht
(<https://euphi.github.io/TRGB-BikeComputer/>, MkDocs, `mkdocs.yml`,
`.github/workflows/docs.yml`): in `doc/` ist `X.md` Englisch und `X.de.md` Deutsch.
**Jede inhaltliche Änderung in beiden Dateien machen.** Die Startseite `doc/index.md`
bindet die Feature-Liste aus `README.MD` ein (zwischen den `features-start`/`-end`-
Kommentaren), `doc/index.de.md` ist deren Übersetzung. Das TrailBridge-Protokoll wird beim
Bauen aus dem TrailBridge-Repo kopiert (`doc/fetch_trailbridge.sh`; dort `PROTOCOL.md`
deutsch, `PROTOCOL.en.md` englisch). Links auf Quelldateien in `doc/` als GitHub-URL
schreiben, relative Links nur auf andere Seiten in `doc/`. Lokal prüfen:
`doc/fetch_trailbridge.sh && mkdocs build --strict`.

**Bekannte Fallstricke (PSRAM/Display-Flackern, NimBLE, USB, ...):
[`doc/PITFALLS.md`](doc/PITFALLS.md) -- vor Änderungen an Speicherlayout,
BLE-Adressen oder beim Debuggen von Anzeige-Artefakten lesen.**

## BLE-Anbindung an TrailBridge

**Vor jeder Änderung an Navigation oder GPS lesen:**
[`../TrailBridge/PROTOCOL.md`](../TrailBridge/PROTOCOL.md) -- verbindlicher
Wire-Format-Vertrag. Nicht auf eigene Faust vom Protokoll abweichen --
Änderungswünsche zuerst mit dem Nutzer und im TrailBridge-Repo klären, dort
ist das Protokoll die Quelle der Wahrheit.

- Rollen: **TrailBridge (Handy) = Peripheral/GATT-Server, dieser ESP32 =
  Central/GATT-Client** -- wie bei allen anderen Sensoren in
  `src/BLEDevices.cpp`. Nicht umdrehen ohne Rücksprache.
- Drei unabhängige Services auf demselben Peer, alle Indicate + Read, je
  eigene Subscription:
  - **Navigation** (`f7ac2b76-986b-45fd-8e44-f116a61f319d` /
    `7473da02-2de8-4f48-9e46-21b36380c176`) -- wird beworben, darüber wird
    der Peer gefunden. Parser: `BLEDevices::handleNavData()`, Konstanten in
    `src/BikeNavProtocol.h`, Manöver→Icon in `src/ui/img/nav_icons.h`.
  - **GPS-Position** (`66b5835c-9be6-43d1-b24a-f9337c0fcb7f` /
    `10c49e7b-4808-4d63-9b68-9ba6c385db0d`) -- wird **nicht** beworben,
    taucht erst bei der Service-Discovery nach dem Connect auf.
    `subscribeGpsPosition()`/`handleGpsData()`, Konstanten in
    `src/BikeGpsProtocol.h`. `FIX_AGE_MS` beachten, sonst wird ein
    veralteter Heartbeat-Fix als aktuell angezeigt.
  - **Höhenprofil** (`3c1f6a90-5b2e-4d7a-9c48-e0a1b7d25f63` /
    `a84e0d17-6f3b-4c52-8e9d-1b70c2f4a596`) -- nicht beworben, kein Heartbeat,
    nur bei abgespielter GPX-Route. `subscribeProfile()`, Konstanten in
    `src/BikeProfileProtocol.h`, Auswertung in `ClimbMonitor`. Position im
    Profil = `REMAINING_DISTANCE_M` des Nav-Frames.
- Frames: Byte 0 Version, Byte 1 Message-Type, danach TLV (Tag 1 Byte |
  Länge 1 Byte | Wert). Unbekannte Tags über die Länge überspringen, nie
  als Fehler behandeln.
- Die TrailBridge-Adresse wird nicht gespeichert (Android rotiert sie), auch nicht im RAM.
  Eine tote Verbindung erkennt `BLEDevices::checkNavAlive()` am ausbleibenden Heartbeat
  (30 s) -- Android meldet beim Beenden der App keinen Disconnect (`doc/PITFALLS.md`).

## WLAN

[`doc/WIFI.md`](doc/WIFI.md). Keine Zugangsdaten in den Sourcen oder im Log -- Netze und
Hotspot-Daten liegen als ein Blob im NVS (`src/WifiConfig.*`, reiner Teil, Host-Test
`test/native_wificonfig/wificonfig_test.cpp`, Build-Befehl im Dateikopf). Zustandsautomat,
Suche, Hotspot, CLI `wifi`: `src/WifiWebserver.cpp`; Seite `/wifi`: `src/WifiRoutes.cpp`.
stärkstes Netz in Reichweite zuerst (Liste nur bei Gleichstand), 5 min ohne Verbindung (oder Hotspot ohne Client) = WLAN aus,
wieder an nur über den Settings-Screen. Geheimnisse nur per POST-Body und nie loggen
(`doc/PITFALLS.md`). Test am Gerät: Netz per `wifi add <ssid> <pw>` auf der seriellen
Konsole eintragen (Passwort wird nicht geloggt). Hotspot vom Handy aus testen (ADB): am Handy
`cmd wifi connect-network TRGB-BC wpa2 <pw>` (davor WLAN am Handy aus/an), HTTP dann mit
`adb exec-out curl --interface wlan0 http://192.168.4.1/...` (sonst nimmt Android den
Mobilfunk); danach das Netz mit `cmd wifi forget-network <id>` wieder vergessen.

## UI

- Aktiver Main-Screen ist **RimRidge** (EEZ Studio), dazu **RimRidgeNav**
  (Navigation), **RimRidgeRQ** (Wege-Labels), **RimRidgeSettings**
  (Einstellungen: Hub mit Version/Sim-Markierung, Neustart/Tiefschlaf; Unterseiten
  **RimRidgeSettingsWifi** / **-Imu** / **-Alt** (Höhenkalibrierung, `doc/HEIGHT.md`) und die
  Zahleneingabe **RimRidgeSettingsNum**, BLE-Geräte **RimRidgeSettingsDev** (`RimRidgeDevCustFunc.*`), Logik in `src/ui/RimRidgeSettingsCustFunc.*` und
  `RimRidgeAltCustFunc.*`),
  **RimRidgeClimb** (Kletter-Anzeige mit Höhenprofil) sowie **RimRidgeWifi** (Netzliste,
  Suche) und **RimRidgeWifiPw** (Passwort mit Tastatur), beide von den Einstellungen aus
  (`src/ui/RimRidgeWifiCustFunc.*`). Projekt:
  `EEZStudio/TRGB-BikeComputer.eez-project`, Design-System:
  [`doc/design/rim-ridge-design-system.md`](doc/design/rim-ridge-design-system.md).
- `src/ui_eez/` ist **generiert** und wird bei jedem EEZ-Export komplett
  ersetzt -- nie von Hand editieren. Handgeschriebene Logik liegt in
  `src/ui/RimRidge*CustFunc.*`, angebunden über `src/UIFacade.cpp`.
- Auf dem Gerät prüfen ohne es anzufassen: `Tools/uishot.py` (Screenshot,
  Tap, Wischen; `doc/DEBUG.md`).
- EEZ-Projekt per Skript bearbeiten: Skill `.claude/skills/eezstudio/`
  (Projekt-Deltas in `PROJECT-NOTES.md` dort). Nach jeder JSON-Änderung
  exportiert der Agent selbst mit `Tools/eez_export_headless.sh` (EEZ Studios
  eigener Exporter) und testet auf dem Gerät; der Nutzer prüft im Canvas und
  exportiert nur noch aus eigenen Gründen manuell.
- Rest der alten SquareLine-UI: nur noch der FL-Screen `src/ui/ui_FL.c`/
  `uiFLmodel.cpp` (nur in der FL-Variante kompiliert, `#ifdef BC_FL_SUPPORT`;
  Quelle für die spätere EEZ-Umstellung, nicht löschen) und die MsgBox in
  `src/ui/ui_custFunc.c`. `src/ui/ui.c` hält nur noch den Zeiger
  `ui_MainScreen`. Einen Chart-Screen gibt es nicht mehr, er wird neu gestaltet.
  `src/ui/img/` enthält nur die von RimRidge genutzten Nav-/Spur-Icons
  (`nav_icons`, `lane_icon`, `roundabout-icon`).

## Binärlog und Wegequalität

- Binärlog `L_*.bin`: Format v2, 64-Byte-Sätze mit Typ-Byte (Fahrdaten /
  Wegequalität / Stoß / manuelles Label). Layout in `src/LogRecords.h`, Reader
  `Tools/bikelog/record.py`. Beide nur gemeinsam ändern; `FORMAT_VERSION`
  erhöhen, wenn sich ein bestehendes Feld verschiebt oder seine Bedeutung
  ändert (ein neuer Satztyp oder bisher reservierte, nullgefüllte Bytes
  brauchen das nicht). `Tools/tests/test_logformat.py` kompiliert den
  Header auf dem Host und fängt Abweichungen.
- Sitzungen: jeder Boot schreibt nach `/BIKECOMP/CUR/` (laufende Nummer,
  `T_*.txt` = Zeit-Hinweise), der nächste Boot sortiert sie datiert ein,
  korrigiert 1970-Zeitstempel und legt die Kurzstatistik `I_*.txt` für die
  Logfile-Seite ab: `src/LogSessions.*`, reiner Teil `src/SessionStats.*`
  (Host-Test `test/native_sessionstats/`). Uhr: NTP oder GPS-Zeit von
  TrailBridge (Tag `UTC_TIME_MS`), `src/ClockSync.*`.
- Manuelles Wege-Label (Untergrund + Qualität 1–4, Ground Truth für die
  automatische Klasse): API `sensors.setRoadLabel*()`/`getRoadLabelState()`/
  `startRoadCapture()` in `src/I2CSensors.h` für den RQ-Ride-Screen; geloggt
  als Satztyp 3, dazu in Stoß-Sätzen und Rohdaten-Blöcken.
- BMI160 → `src/RoadQuality.*` (reiner Algorithmus: Rauheit, Stöße,
  Referenzfahrt, Steigung aus Beschleunigung). Host-Test:
  `test/native_roadquality/roadquality_test.cpp` (Build-Befehl im
  Dateikopf). Anbindung, CLI `rq` und Debug-Seite `/debug/imu` in
  `src/I2CSensors.cpp`.
- Rohdaten (`R_*.bin` auf Anforderung, `S_*.bin` Stoß-Ausschnitte): Format
  `src/RawCapture.h`, Reader `Tools/bikelog/raw.py`, Replay mit dem
  unveränderten Firmware-Algorithmus `Tools/rqreplay/rq_replay.cpp`
  (`bikelog raw replay`).
- Log-Dienst `Tools/BikeLogService/` (läuft auf ia216 aus `~/bikelog`, nicht
  aus dem Repo): holt Sitzungen per mDNS-Trigger vom BC ab (`/logfiles/` bzw.
  `/logfiles.json` + `/log/...`). Ändert sich das Namensschema auf der SD
  oder die HTML-Liste, `bikelogservice/sdlayout.py`/`puller.py` mitziehen.
- Auswertung: `bikelog/report.py` (Kennzahlen einer Sitzung als JSON, `bikelog report`),
  `bikelog/training.py` (Trainingslast, Wochen, Zielrennen, wiederkehrende Anstiege; arbeitet
  nur auf dem Bericht-JSON, nie auf dem Binärformat). Im Dienst: Bericht-Cache und
  Hintergrundberechnung `analysis.py`, Seiten im Rim-&-Ridge-Design `webui.py` + `charts.py`
  (SVG auf dem Server). Ändert sich ein Feld des Bericht-JSON, `REPORT_VERSION` erhöhen --
  das berechnet alle gespeicherten Berichte neu.
- Testfahrten: `bikelog/testride.py` (Sim-Flag `LOG_SIMULATED` oder GPS bewegt sich ohne
  Radsensor), im Index `sessions.test_kind` + `test_override` (Nutzer-Urteil), SQL `TEST_SQL`.
  Testfahrten sind in Liste und API ausgeblendet, zählen nicht im Training, gehen nach
  `Debug_Archive` (Status `test`), also nie nach Nextcloud/Komoot.
- Leerlauf-Sitzungen (`testride.idle()`, Spalte `sessions.idle`, auch Sitzungen ohne `L_`):
  nie gelistet, kein GPX, nach `BIKELOG_IDLE_ARCHIVE_DAYS` nach `<data>/archive/`
  (`archive.py`). Der Grabstein im Index bleibt, bis die Dateien nicht mehr in der Liste der
  SD-Karte stehen (`Storage.purge_gone()` beim Abruf). Löschen auf dem BC nur sofort über
  `/del/` der Firmware (`Puller.delete_files()`), nie vorgemerkt.
- Neustart unterwegs: Sitzungen mit Abstand <= `komoot_merge_gap_s` sind eine Fahrt
  (`tours.py`, auch für Komoot). Training rechnet pro Fahrt, Bericht über die verbundenen
  Sätze (`analysis.tour_report_for()`, Tabelle `tour_reports`), Seite `/tour/{id}`.

## Anstiege (Höhenprofil)

[`doc/CLIMB.md`](doc/CLIMB.md): Kategorien, Einstellungen, Demo.

- `src/ClimbProfile.*` ist der reine Algorithmus (Fenster zusammenfügen, Fuß
  und Gipfel finden, bewerten) samt aller Einstellwerte (`Climb::Config`,
  Tabelle `PARAMS`). Host-Test: `test/native_climb/climb_test.cpp`
  (Build-Befehl im Dateikopf) -- er füttert die Fahrten so, wie TrailBridge
  sie schickt. Ändert sich dort `RouteNavigator`/`ElevationProfile`, den
  Test mitziehen.
- `src/ClimbMonitor.*`: Anbindung (BLE-Task rein, UI-Task raus, eigener
  Mutex), NVS-Namespace `Climb`, CLI `climb`, `/debug/climb`. Ein neuer
  Einstellwert braucht nur ein Feld in `Climb::Config` und eine Zeile in
  `PARAMS`.
- Das Profil wird im Draw-Event von `rrclimb_profile` gezeichnet
  (`src/ui/RimRidgeClimbCustFunc.cpp`), ohne Canvas-Puffer.

## Serielle Konsole

`src/SerialConsole.*`: Zeileneditor mit Verlauf und Tab-Completion vor SimpleCLI.
Neue Befehle mit `console.addCmd(name, callback, completer)` anlegen, Serial-Ausgaben
aus anderen Tasks in `SerialConsole::Output` einschließen, keine ANSI-Sequenzen
ausgeben (Details: `doc/PITFALLS.md`). Host-Test:
`test/native_console/console_test.cpp` (Build-Befehl im Dateikopf).

## Build

PlatformIO (`platformio.ini`, Board `esp32s3box`, pioarduino-Plattform,
Arduino-Framework). Environments:

* `trgb-esp32-s3` -- Default, "Gravel"-Variante (runder Touch-Controller).
* `trgb-esp32-s3-FL` -- Forumslader am Touren-/Pendlerrad. Zweite
  Priorität, nur auf explizite Anfrage bauen.
* `trgb-esp32-s3-ota`, `trgb-esp32-s3-FL-ota` -- dieselben Builds, Upload per
  WLAN über den eigenen `/update`-Endpunkt (curl, nicht espota). Flashen nur auf
  ausdrückliche Anfrage des Nutzers.
* `trgb-esp32-s3-sim`, `trgb-esp32-s3-sim-ota` -- Debug-Build mit
  Sensor-Simulator (`-DBC_SIM`, `src/SimSensors.*`): Fake-Speed/Cadence/Puls
  über `sim` (seriell), `/debug/sim` oder `bikelog sim <gpx>`, eigene
  NVS-Namespaces für die Statistik. Alles Simulator-Spezifische hinter
  `#ifdef BC_SIM`, die normalen Builds bleiben frei davon.
  [`doc/SIMULATOR.md`](doc/SIMULATOR.md).

## Programmiersprachen-Präferenz

C++/PlatformIO für die Firmware. Python für Tools unter `Tools/` und
Skripte. Kein Java/Kotlin hier -- das ist die Android-Seite in
`../TrailBridge/`.
