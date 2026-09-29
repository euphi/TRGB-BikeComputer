# Ride-Zustandsautomat

Zustandsmodell für RimRidge: welcher Zustand liegt hinter dem StateIcon
(direkt über `rr_line_rq`) und der Pause/Start-Taste (`rr_btn_pause`), und
wie das mit der Statistik (Tour/Trip/Start-Ride, Ø-Geschwindigkeit ohne
Stops/Breaks/Cruise) zusammenhängt.

Diagramm: [`ride-state-machine.svg`](ride-state-machine.svg).

**Status (2026-09-28):** Design + Statistics-Kern umgesetzt, Statistik-Review
(§5) eingearbeitet
(`src/Stats/Statistics.{h,cpp}`, `src/Stats/Distance.{h,cpp}`). UI-Anbindung
(Tap-Handler, neue Icons) siehe §6 — Icons sind Platzhalter, vom Nutzer im
EEZ-Canvas zu prüfen.

Exportiert, gebaut und auf dem Gerät getestet (Tap/Long-Press per
`Tools/uishot.py`); eine echte Fahrt steht noch aus.

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
(`"TIME_IN_FREE"` neu, JSON `timeIn.FREE`, andere unverändert) -- alte gespeicherte Werte
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
  bereits offene Session). In diesem Moment: `Statistics::reset
  (SUM_ESP_START)` (nullt Zeiten, Cruise-Distanz, Max-Speed, Cadence und
  merkt sich den Distanzstand als Session-Basis).
* Bleibt `true`, solange man zwischen Ride und Cruise hin- und herwechselt
  -- genau das ist die Anforderung "Cruise beendet den Ride nicht".
* Wird `false` beim Long-Press-Stop. Ab dann bleibt `SUM_ESP_START` auf
  den Werten der beendeten Fahrt stehen (Zeit läuft nicht weiter, Distanz
  wird eingefroren), bis der nächste Start-Tap eine neue Session öffnet.

`Distance`s eigener `SUM_ESP_START` ist und bleibt "seit Einschalten"
und wird nie zurückgesetzt -- Binärlog (`dist_m`), Navigations-Restdistanz
und Steigungsberechnung brauchen einen monotonen Zähler. Die Session ist
ein Fenster darauf (`Statistics::sessionBase*`/`sessionEnd*`,
`Statistics::getDistance(SUM_ESP_START)`).

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

## 5. Statistik: Zeit, Distanz, Durchschnitte

Grundregel: **Distanz zählt immer, Zeit nur mit verbundenem
Speed-Sensor.** Was der Radzähler vorrückt, während der BC aus oder der
Sensor getrennt ist, landet in der Distanz und zusätzlich in
`lostDistanceFromNVS[]` ("ohne Sensor"). Die **Netto-Distanz**
(`getDistance(t, false)`) ist genau der Teil, zu dem es Zeit gibt -- sie
wird für alle Durchschnitte verwendet. `DS_NO_CONN`-Zeit wird mitgezählt
(Diagnose), geht aber in keine Zeit und keinen Durchschnitt ein.

```
enum EAvgType {
    AVG_ALL,        // Fahren + Stops + Pausen (Breaks)
    AVG_DRIVE,      // nur Fahren (Ride, Coast und FreeRide/Cruise)
    AVG_NOBREAK,    // Fahren + kurze Stops, ohne Pausen > 2 min
    AVG_NOCRUISE,   // nur Fahren im Ride-Modus: Cruise-Zeit UND -Distanz raus
    EAvgTypeMax
};
```

* Fahrzeit = Power + Coast + FreeRide. Coast/Power ist reine Anzeige
  (StateIcon) bzw. für spätere Auswertung, für die Statistik egal.
* Stops/Pausen fügen Zeit, aber (fast) keine Distanz hinzu -- deshalb
  unterscheiden sich `AVG_DRIVE`/`AVG_NOBREAK`/`AVG_ALL` nur in der Zeit,
  eine Distanz je Zustand ist dafür nicht nötig.
* `AVG_NOCRUISE` braucht die Cruise-Distanz: `Statistics::distFree[]`
  (nur Distanz in `DS_FREE_RIDE`), wie `time_in[][]` in NVS persistiert
  (`DIST_FREE`). Für Tour/Trip/Total heißt das "nur Ride-Anteile aller
  Fahrten"; wer nie Start drückt, hat dort keinen Wert.
* Stop -> Break (> 2 min): verschoben wird genau die Zeit dieses Stopps
  (`stopEpisodeMs[]`), nicht pauschal `now - timestamp_stop` -- das hat
  früher nach einem Reconnect oder Reset Zeit älterer Stopps umgebucht.
* Reconnect geht immer nach `DS_STOP` (nicht in den Zustand vor dem
  Abbruch), damit bis zur ersten Geschwindigkeit keine Fahrzeit läuft.
* Max-Speed je Summary persistiert (`SPEED_MAX`), mit Reset zurückgesetzt,
  Werte > 120 km/h verworfen; Ø-Cadence = Kurbelumdrehungen / Zeit mit
  Cadence > 0 während der Fahrt (`CAD_MREVS`/`CAD_MS`).

### Zählerstand des Sensors (`Distance::updateRevs()`)

* Die meisten CSC-Sensoren zählen kumulativ weiter; billige (CYCPLUS)
  starten nach jedem Aufwachen bei 0. Weil der Connect dauert, steht beim
  ersten Wert meist schon eine kleine Zahl im Zähler -- die ist gefahren
  und wird gezählt.
* Erster Wert nach dem Einschalten: Zähler >= gespeichert -> Differenz ist
  "ohne Sensor"; Zähler < gespeichert (Neustart des Sensors) -> der ganze
  aktuelle Zählerstand ist "ohne Sensor".
* Zähler kleiner als der letzte Wert im Betrieb: bei Reconnect "ohne
  Sensor", bei laufender Verbindung normal gefahrene Strecke; die Basis
  wird neu gesetzt, ohne Bisheriges zu verlieren (früher fiel dabei die
  Start-Distanz auf 0 und der neue Zählerstand wurde verworfen).
* "Noch kein Wert" ist ein eigenes Flag (`revsInitialized`, `revsKnown[]`
  per `isKey()`), nicht mehr `lastRevs == 0` -- 0 ist ein gültiger
  Zählerstand.

## 6. UI-Anbindung

* `UIFacade::updateStateIcon()` bekommt ein drittes Argument (`rideMode`)
  und mappt `DS_FREE_RIDE` auf ein neues Bild (`img_rr_icon_state_freeride`,
  über den `eezstudio`-Skill als EEZ-Ressource angelegt -- **Platzhalter**,
  vom Nutzer im Canvas zu prüfen). Der bisherige Debug-Override (immer
  Coasting-Icon, `UIFacade.cpp` TODO 2026-09-19) entfällt damit.
* `rr_btn_pause` bekommt zusätzlich zum bestehenden `LV_EVENT_LONG_PRESSED`
  einen `LV_EVENT_SHORT_CLICKED`-Handler (`action_pause_click()`; nicht
  `CLICKED` -- das schickt LVGL 8 auch nach einem Long-Press, der Stop hätte
  sofort eine neue Session gestartet); das
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
* **Manuell gesetzter Gesamtkilometerstand** (Odometry-Seite) zählt als
  Netto-Distanz ohne zugehörige Zeit -- der Total-Durchschnitt ist dadurch
  zu hoch, solange die Differenz nicht als "lost" eingetragen ist.
* Kein Host-Test für `Statistics`/`Distance`: beide sind eng mit
  Arduino/ESP32 (Ticker, Preferences, `heap_caps_*`, Singletons)
  verzahnt, anders als `SessionStats`/`RoadQuality`, die bewusst als
  reine Algorithmen ausgelagert sind. Verifiziert wurde stattdessen per
  PlatformIO-Build (`pio run`) + Codelese; für echte Laufzeit-Tests bliebe
  nur ein Gerätetest.

## 8. Binärlog und GPX

`Statistics::logRideState()` schreibt einen `LogRec::RideState`-Satz (Typ 4,
`src/LogRecords.h`) bei jeder Änderung von `curDriveState`, `rideMode` oder
der Ride-Session -- nicht nur bei Zustandswechseln, denn ein Tap im Stand
oder ein Long-Press im Cruise ändern nur Modus bzw. Session. Der Satz trägt
Zustand, Ride-Modus, Flags (`RSF_SESSION_OPEN`, `RSF_SESSION_START`) und die
Distanz seit dem Einschalten; `DS_FREE_RIDE` mit offener Session ist Cruise.

`Tools/bikelog/gpx.py` beginnt bei jedem Wechsel ein neues `<trkseg>` und
hängt den Zustand als `<extensions><bc:RideState>` an das Segment
(`GpxOptions.ride_states`, CLI `bikelog gpx --ride-states`). Standardmäßig
aus: Coast/Power und jeder Stopp zerlegen den Track in viele kurze Segmente,
sinnvoll nur für die Analyse von Zeitverlust im Verkehr.
