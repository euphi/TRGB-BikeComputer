# Ride-Zustandsautomat

Zustandsmodell für RimRidge: welcher Zustand liegt hinter dem StateIcon
(direkt über `rr_line_rq`) und der Pause/Start-Taste (`rr_btn_pause`), und
wie das mit der Statistik (Tour/Trip/Start-Ride, Ø-Geschwindigkeit ohne
Stops/Breaks/Cruise) zusammenhängt.

Diagramm: [`ride-state-machine.svg`](ride-state-machine.svg).

**Status (2026-09-28):** Design + Statistics-Kern umgesetzt
(`src/Stats/Statistics.{h,cpp}`, `src/Stats/Distance.{h,cpp}`). UI-Anbindung
(Tap-Handler, neue Icons) siehe §6 — Icons sind Platzhalter, vom Nutzer im
EEZ-Canvas zu prüfen.

**Offener Schritt (Umgebung dieser Session hatte weder EEZ Studio noch
PlatformIO installiert):** `.eez-project` ist bereits gepatcht
(`EEZStudio/tmp/add_ride_state_assets.py`, JSON-Gates grün) und
`src/UIFacade.cpp`/`RimRidgeCustFunc.cpp` referenzieren bereits
`img_rr_icon_start`/`img_rr_icon_state_freeride` -- diese Symbole existieren
aber erst nach einem Export. Vor dem nächsten Firmware-Build:
`Tools/eez_export_headless.sh` (oder Ctrl+B in EEZ Studio) laufen lassen,
danach `pio run -e trgb-esp32-s3` und ein Gerätetest (`Tools/uishot.py`).

## 1. Grundsatz

Aufzeichnung (Binärlog, `L_*.bin`) läuft immer, unabhängig vom Ride-Zustand
-- dieser Automat beeinflusst nur, welche Anteile der Fahrt in die
"Start/Ride"-Statistik zählen und was Icon/Taste anzeigen. GPX & Co.
entstehen weiterhin erst nachträglich im Tooling (`Tools/bikelog/`).

## 2. Zwei Achsen, sechs Blattzustände

`Statistics::EDrivingState` bleibt ein einzelnes flaches Enum (ein Blatt =
ein StateIcon-Bild, ein `time_in[][]`-Slot), aber die Übergänge folgen
zwei unabhängigen Achsen:

* **Ride-Modus** (`rideMode`, manuell per Tap auf die Pause/Start-Taste):
  `false` = FreeRide/Cruise, `true` = Ride (mit Coast als Sub-Zustand,
  Cadence-Hysterese wie in der alten `Statistics::cycle()`-Logik: <40 rpm
  -> Coasting, >50 rpm -> Power).
* **Bewegung** (automatisch, aus Geschwindigkeit): Fahren / `DS_STOP`
  (v < 0.3 km/h) / `DS_BREAK` (Stop > 2 min am Stück, wie bisher). Gilt
  **in jedem Ride-Modus** -- ein Stopp an der Ampel ist derselbe
  `DS_STOP` egal ob man gerade "Ride" oder "FreeRide/Cruise" war; beim
  Weiterfahren (v > 5.5 km/h) geht es zurück in den Modus, der vor dem
  Stopp aktiv war (`rideMode`-Wert entscheidet, nicht die Cadence).

```
enum EDrivingState {
    DS_NO_CONN,
    DS_BREAK,
    DS_STOP,
    DS_FREE_RIDE,        // NEU: rideMode == false, in Bewegung
    DS_DRIVE_COASTING,    // rideMode == true, Cadence niedrig
    DS_DRIVE_POWER,       // rideMode == true, Cadence normal
    EDrivingStateMax
};
```

`DS_FREE_RIDE` ist neu und deckt sowohl den initialen Zustand (vor dem
ersten Start-Tap) als auch "Cruise" ab -- **FreeRide und Cruise sind
exakt derselbe Blattzustand**, Icon und Tastensymbol unterscheiden sich
nicht. Der einzige Unterschied ist unsichtbar: ob gerade eine
Ride-Session offen ist (§4).

Persistenz: `PREF_TIME_STRING[]`/NVS-Keys sind namensbasiert
(`"TIME_IN_FREERIDE"` neu, andere unverändert) -- alte gespeicherte Werte
bleiben beim Update gültig, kein Migrationsschritt nötig.

## 3. Taste und Icon

| Zustand | StateIcon | Pause/Start-Taste (`rr_ic_pause`) | Tap (kurz) | Long-Press |
|---|---|---|---|---|
| `DS_FREE_RIDE` | FreeRide-Icon (neu, §6) | Start-Pfeil | -> Ride (Start, falls keine Session offen; sonst Resume aus Cruise) | Stop, falls Session offen; sonst wie bisher Standby-Timer toggeln (`toggleStandbyMode()`) |
| `DS_DRIVE_POWER` / `DS_DRIVE_COASTING` | "Ride"/"Coast"-Icon (bestehend: `power`/`coasting`) | Pause-Symbol (bestehend) | -> Cruise (`DS_FREE_RIDE`, Session bleibt offen) | Stop (Session schließen, zurück zu echtem FreeRide) |
| `DS_STOP` / `DS_BREAK` | bestehend (`stop`/`break`) | wie der `rideMode`-Wert vor dem Stopp (Start-Pfeil oder Pause-Symbol) | wirkt auf den Modus, nicht auf die Bewegung (siehe unten) | wie oben |

Ein Tap während `DS_STOP`/`DS_BREAK` toggelt `rideMode` genauso wie im
Fahren -- man kann also z.B. an der Ampel schon auf Pause tippen, das
wirkt sich beim Weiterfahren aus. Die Bewegungs-Achse (Stop/Break/Fahren)
bleibt davon unberührt.

Long-Press-Verhalten ist bewusst kontextabhängig belassen (Konflikt mit
der bestehenden Funktion): mit offener Ride-Session beendet er die Fahrt
(neue Anforderung), ohne offene Session tut er weiterhin das, was er
vorher schon tat (Standby-Timer verlängern/deaktivieren -- die einzige
Bedienung dafür auf RimRidge). Falls das im Alltag kollidiert (z.B. man
will den Standby-Timer *während* einer laufenden Ride-Session anfassen),
bräuchte es eine zweite Geste -- bewusst nicht vorgegriffen.

## 4. Ride-Session (`rideSessionOpen`)

Ein zweites, von `rideMode`/`curDriveState` unabhängiges Flag:

* Wird `true` beim **ersten** Wechsel `FreeRide -> Ride` (Start-Tap ohne
  bereits offene Session). In diesem Moment: `Distance::resetDistToZero
  (SUM_ESP_START)` (nullt Start-Distanz und, via `Statistics::reset()`,
  `time_in[][SUM_ESP_START]` sowie `dist_in[][SUM_ESP_START]`, §5).
* Bleibt `true`, solange man zwischen Ride und Cruise hin- und herwechselt
  -- genau das ist die Anforderung "Cruise beendet den Ride nicht".
* Wird `false` beim Long-Press-Stop. Ab dann läuft `SUM_ESP_START`
  wieder bei 0, bis der nächste Start-Tap eine neue Session öffnet.

`time_in[DS_FREE_RIDE][SUM_ESP_START]` akkumuliert nur, während
`rideSessionOpen == true` -- das ist exakt die Cruise-Zeit der aktuellen
Fahrt (§5, `AVG_NOCRUISE`). Vor dem ersten Start (oder nach einem Stop)
läuft die Start/Ride-Uhr nicht mit, obwohl man technisch schon
"FreeRide" fährt.

Tour/Trip/Total sind von alldem unberührt: `Statistics::cycle()` schreibt
für sie unabhängig vom Ride-Modus/`rideSessionOpen` in jedem 500ms-Tick
weiter (bestehendes Verhalten) -- sie laufen "sowieso unabhängig mit".
Ein "beim Stop noch in Trip/Tour einrechnen" ist dadurch kein separater
Schritt, sondern bereits die ganze Zeit über passiert.

## 5. Statistik: Tour / Trip / Start-Ride, Stops/Breaks/Cruise herausrechnen

Bestehende Matrix `ESummaryType x EAvgType` (`Statistics::getTime()`/
`getAvg()`) deckt "in allen Modi Stops/Breaks/Cruise herausrechnen" ab,
ohne dass Tour/Trip/Start eigene Sonderfälle brauchen -- dieselbe
Achse gilt für alle vier `ESummaryType`s (Total/Tour/Trip/Start), per
Geste (hoch/runter auf `rr_tour_pill`) durchschaltbar
(`Statistics::getNextTimeMode()`).

```
enum EAvgType {
    AVG_ALL,        // alles: Fahren + Cruise + Stop + Break
    AVG_DRIVE,      // nur echtes Treten (Power+Coasting)
    AVG_NOBREAK,    // Fahren + Stop, ohne lange Break
    AVG_NOCRUISE,   // NEU: Fahren + Stop + Break, ohne Cruise/FreeRide-Zeit
    EAvgTypeMax
};
```

Treppenförmig ineinander verschachtelt: `AVG_ALL` ⊃ `AVG_NOCRUISE` ⊃
`AVG_NOBREAK` ⊃ `AVG_DRIVE`. Für "Start/Ride" ist `AVG_NOCRUISE` der
interessante neue Fall (Cruise-Strecke/-Zeit raus); für Tour/Trip/Total
ist er größtenteils identisch mit `AVG_ALL`, solange dort nie eine
FreeRide-Zeit reingezählt wurde (`DS_FREE_RIDE` zählt für Tour/Trip/Total
unabhängig von `rideSessionOpen` immer mit -- nur bei `SUM_ESP_START` ist
das Gate aktiv, §4) -- bewusst so: "FreeRide" vor dem ersten Start soll
nicht in die langfristige Tour-Statistik als Lücke fehlen.

### Distanz pro Zustand (neu: `dist_in[EDrivingStateMax][ESummaryTypeMax]`)

Bisher gab es nur eine **Zeit**-Aufschlüsselung nach Zustand
(`time_in[][]`); Distanz war ein einziger laufender Wert pro
`ESummaryType` (`Distance::curTotalDistance[]`, NVS-persistiert,
inklusive "verlorener" Distanz bei Sensor-Aussetzern). Für "Cruise-Strecke
herausrechnen" reicht Zeit-Filtern allein nicht -- sonst verzerrt
Frei-Rollen während Cruise die Ø-Geschwindigkeit. Deshalb:
`Distance::updateRevs()` meldet jedes Delta zusätzlich an
`Statistics::addDistanceDelta()`, die es (gleiches Muster wie
`cycle()`s `time_in[][] += delta`, gleiches `SUM_ESP_START`-Gate) in
`dist_in[curDriveState][summaryType]` einsortiert.

`getAvg()` nutzt für `AVG_ALL` weiterhin die robuste, NVS-persistierte
`Distance::getDistance()` (verlustkorrigiert); für die drei
Filter-Varianten (`AVG_DRIVE`/`AVG_NOBREAK`/`AVG_NOCRUISE`) die Summe aus
`dist_in[][]`. Bewusste Einschränkung: `dist_in[][]` ist **nicht**
NVS-persistiert (nur `time_in[][]` ist es, wie bisher) -- nach einem
Neustart starten die gefilterten Ø-Geschwindigkeiten für Tour/Trip bei 0
und bauen sich neu auf, bis der nächste Neustart. Für die "Start/Ride"-
Ansicht (bei jedem Start ohnehin neu) fällt das nicht ins Gewicht.

## 6. UI-Anbindung

* `UIFacade::updateStateIcon()` bekommt ein drittes Argument (`rideMode`)
  und mappt `DS_FREE_RIDE` auf ein neues Bild (`img_rr_icon_state_freeride`,
  über den `eezstudio`-Skill als EEZ-Ressource angelegt -- **Platzhalter**,
  vom Nutzer im Canvas zu prüfen). Der bisherige Debug-Override (immer
  Coasting-Icon, `UIFacade.cpp` TODO 2026-09-19) entfällt damit.
* `rr_btn_pause` bekommt zusätzlich zum bestehenden `LV_EVENT_LONG_PRESSED`
  einen `LV_EVENT_CLICKED`-Handler (`action_pause_click()`); das
  Icon-Kind (`img_rr_icon_pause` vs. neues `img_rr_icon_start`) wechselt
  in `ui_RimRidgeUpdateStateIcon()` mit dem StateIcon zusammen, da beide
  vom selben `rideMode`/`curDriveState` abhängen.
* Neue `UIDriveStateEvent`-Werte `DSE_pauseButtonTap`/`DSE_pauseButtonStop`
  in `src/ui/ui.h`/`ui_events.cpp`, analog zu den bestehenden
  Standby-Events.

## 7. Bewusst nicht umgesetzt / bekannte Lücken

* **Power (Watt) am Coast-Kriterium:** Es gibt keinen Leistungsmesser im
  Projekt (kein Cycling-Power-Service, siehe `BLEDevices.cpp`) --
  "Trittfrequenz oder Wattzahl niedrig" ist also aktuell nur
  Cadence-gated. Der Anforderungstext lässt das offen ("nur wenn Sensor
  verbunden") -- ohne Powermeter bleibt der Coast/Power-Split exakt die
  alte Cadence-Hysterese (40/50 rpm).
* **Cadence-"verbunden"-Flag:** Es gibt noch kein eigenes
  "Cadence-Sensor verbunden"-Signal getrennt von der Speed-Verbindung
  (siehe Explore-Notiz zu `BLEDevices.cpp`). Ohne Cadence-Sensor bleibt
  `cadence` auf ihrem letzten Wert bzw. `-1` (ungültig) stehen --
  `cycle()`s Vergleich `cadence < 40`/`> 50` behandelt `-1` weiterhin als
  "niedrig" (Coasting), wie schon in der alten Logik. Ein eigenes
  "kein Cadence-Sensor -> gar keinen Coast-Split, immer Power" wurde
  nicht gebaut, um den Explore-Fund nicht auf Verdacht zu erweitern --
  bei Bedarf sauber nachrüstbar.
* **`dist_in[][]` nicht persistiert** (§5) -- Konsequenz oben beschrieben.
* Kein Host-Test für `Statistics`/`Distance`: beide sind eng mit
  Arduino/ESP32 (Ticker, Preferences, `heap_caps_*`, Singletons)
  verzahnt, anders als `SessionStats`/`RoadQuality`, die bewusst als
  reine Algorithmen ausgelagert sind. Verifiziert wurde stattdessen per
  PlatformIO-Build (`pio run`) + Codelese; für echte Laufzeit-Tests bliebe
  nur ein Gerätetest.
