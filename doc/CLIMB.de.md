# Anstiege: Höhenprofil und Kletter-Anzeige

TrailBridge schickt beim Abfahren einer GPX-Route immer das Höhenprofil der Strecke voraus und sagt
die Anstiege darin an ([Protokoll](trailbridge/PROTOCOL.md), „Höhenprofil-Service").
Der BikeComputer bewertet den Anstieg und zeigt ihn auf dem Screen
`RimRidgeClimb` ([Design](design/rim-ridge-design-system.md), §6).

![Anstiegs-Screen](screenshots/climb.png){ width="260" }

| Teil | Datei |
|---|---|
| Wire-Format | `src/BikeProfileProtocol.h` |
| Algorithmus (rein, Host-Test) | `src/ClimbProfile.*`, `test/native_climb/climb_test.cpp` |
| Anbindung: BLE, Position, Einstellungen, CLI, Web, Demo | `src/ClimbMonitor.*`, `BLEDevices::subscribeProfile()` |
| Screen | `src/ui/RimRidgeClimbCustFunc.*`, `UIFacade::updateClimb()` |

## Was vom Handy kommt, und was die Firmware daraus macht

**Rollendes Handy (TrailBridge seit 2026-10-04, Frame-Flag `ROLLING`).** Das Profil kommt immer –
auch auf flacher Strecke und bergab – als Fenster der Strecke voraus (5 km im 25-m-Raster; ein
weiter entfernter Anstieg wird bis zu seinem Fuß abgeschnitten). Ein Anstieg darin wird vom Handy
**mit seiner ganzen Ausdehnung angesagt** (Fuß und Gipfel, Restdistanz und Höhe, auch wo sie
hinter oder jenseits des Frames liegen): Sobald der Fuß höchstens 500 m voraus liegt, reicht das
Fenster bis zum Gipfel, bis 5 km im 25-m-Raster, für längere Anstiege gröber (bis 250 m; der 36 km
lange Galibier von Süden kommt mit 200 m). Damit

- **endet der Anstieg nicht vor dem Gipfel**, und Kategorie und Länge sind bis ganz oben die
  des **ganzen** Anstiegs (Testfahrt 2026-10-04: die Kategorie sank unterwegs, weil die Firmware
  nur noch den Rest des Anstiegs im Frame sah),
- öffnet sich der Anstiegs-Screen nur bei Anstiegen von selbst (Rang mindestens `autorank`, Fuß
  näher als `showahead`) und geht **erst nach dem Gipfel** zurück (`summitpass` dahinter, dann
  `hidedelay`) – nicht, wenn die Daten einen Moment ausbleiben (abseits der Route, Frame
  unterwegs); erst nach 2 Minuten ohne jeden Anstieg (Route zu Ende, Handy weg) geht er auch,
- heißt `PROFILE_NONE` nur noch „Route zu Ende / abseits der Route"; der Anstieg, auf dem der
  Fahrer ist, überlebt es.

**Älteres Handy** (ohne Flag): TrailBridge schickt nur den Anstieg voraus, von der aktuellen
Position bis zum Gipfel in einem Frame, und `PROFILE_NONE` am Gipfel. Die Firmware

- **findet den Anstieg**: Fuß = erster Punkt, ab dem die nächsten 100 m im
  Mittel 3 % steigen. Gipfel = der letzte Punkt vor einem Gefälle von mehr als
  30 m oder vor 2 km ohne mittlere Steigung von 0,5 % – oder das Profilende.
  Ein kürzeres Flachstück oder eine Senke beendet den Anstieg also nicht; der
  Galibier von Norden sind zwei Anstiege (Abfahrt vom Télégraphe nach Valloire).
  TrailBridge sucht mit denselben Kriterien (`ElevationProfile.java` dort) –
  die Standardwerte beider Seiten gehören zusammen.
- **fügt Frames zusammen** (gleiches Raster, überlappend) und behält den schon
  gefahrenen Teil, bis zu 320 Schritte. Das braucht es nur, wenn ein Anstieg
  selbst im 250-m-Raster nicht in einen Frame passt.
- **führt einen Anstieg fort**, wenn das Handy `PROFILE_NONE` und kurz danach
  ein neues Profil schickt.

Beide: Die Firmware

- **kennt die Position** aus der Restdistanz der Nav-Frames, zwischen zwei
  Frames fortgeschrieben mit der Strecke des Speed-Sensors.

Lesbarkeit: Die erwartete Steigung („NÄCHSTE 25 m") und die gemessene stehen in der großen
48-px-Schrift; das steilste Band ist ein sattes Rot (`0xE8392A`) im Profil und im Wert.

Höhenangaben sind netto: Gipfel minus Fuß, Gipfel minus Fahrer.

## Kategorien

Punktzahl = Länge [m] × mittlere Steigung [%] = 100 × Höhenunterschied. Das
ist die Skala von Strava und Garmin (ClimbPro) für 4 bis HC; 5 und 6 setzen
sie nach unten durch Halbieren fort.

| Kategorie | Punkte ab | Höhenunterschied ab | Beispiel |
|---|---|---|---|
| 6 | 2 000 | 20 m | 400 m mit 5 % |
| 5 | 4 000 | 40 m | 800 m mit 5 % |
| 4 | 8 000 | 80 m | 1,6 km mit 5 % |
| 3 | 16 000 | 160 m | 3 km mit 5,5 % |
| 2 | 32 000 | 320 m | 5 km mit 6,5 % |
| 1 | 64 000 | 640 m | 9 km mit 7,5 % |
| HC | 80 000 | 800 m | 10 km mit 8 % |

Nicht bewertet (kurzer Hügel, keine automatische Anzeige): kürzer als 300 m,
im Mittel flacher als 3 %, oder unter 20 m Höhenunterschied.

## Anzeige

- Der Screen erscheint von selbst, sobald ein bewerteter Anstieg höchstens
  300 m voraus liegt, und geht 5 s nach seinem Ende zurück zum Hauptscreen.
  Er löst nur den Hauptscreen ab; steht gerade Navigation, RQ oder
  Einstellungen da, ist er danach das Ziel von „zurück".
- Von Hand: Tipp auf Höhe oder Steigung des Hauptscreens öffnet ihn, Wischen
  schließt ihn. Ein weggewischter Anstieg kommt nicht von selbst wieder.
- Profilfarbe nach Steigung des Raster-Abschnitts (25 m, bei langen Anstiegen
  bis 250 m): unter 1 % blau, bis 4 % grün,
  bis 7 % gelb, bis 10 % orange, darüber rot. Der gefahrene Teil ist
  abgedunkelt.

## Einstellungen

Alle Werte sind einzeln einstellbar und werden im NVS (`Climb`) gespeichert:

```
climb                     Zustand und alle Einstellungen
climb cat4 9000           setzen
climb cat4                anzeigen (mit Bereich und Bedeutung)
climb defaults            Standardwerte
```

Ohne USB: `/debug/climb` (Tabelle mit Eingabefeldern), `/debug/climb.json`,
`/debug/climb/set?<name>=<wert>`.

| Name | Standard | Bedeutung |
|---|---|---|
| `startgrade` / `startwindow` | 3 % / 100 m | Fuß: mittlere Steigung über diese Strecke |
| `contgrade` | 0,5 % | mittlere Steigung ab dem bisherigen Gipfel, mit der der Anstieg weitergeht |
| `summitflat` | 2000 m | Flachstück, das den Anstieg beendet; auch der Abstand, in dem ein neuer Anstieg den alten fortsetzt |
| `summitdip` | 30 m | Gefälle, das den Anstieg beendet |
| `summitpass` | 50 m | so weit hinter dem Gipfel ist der Anstieg vorbei |
| `minlength` / `mingrade` | 300 m / 3 % | darunter nicht bewertet |
| `cat6` … `cat1`, `cathc` | siehe oben | Punktzahl je Kategorie |
| `autoshow` | 1 | automatisch zum Kletter-Screen wechseln |
| `autorank` | 1 | ab welchem Rang: 1 = Kategorie 6 … 6 = Kategorie 1, 7 = HC |
| `showahead` | 300 m | Abstand zum Fuß, ab dem der Screen erscheint |
| `hidedelay` | 5 s | Wartezeit vor dem Zurückwechseln |
| `lookahead` | 25 m | Strecke für die „Steigung voraus" |
| `band1` … `band4` | 1 / 4 / 7 / 10 % | Grenzen der Profilfarben |
| `infocycle` | 4 s | Wechsel des Info-Felds (Uhrzeit, Distanz, Temperatur, Trittfrequenz, Höhe) |

## Ohne Handy ausprobieren

```
climb demo                2 km mit 6 % (Standard)
climb demo 4000 9         Länge in m, mittlere Steigung in %
climb pos 800             Fahrer 800 m hinter den Profilanfang setzen
climb show / climb hide   Screen öffnen / schließen
climb demo off
```

Das Demo-Profil läuft als normaler Frame durch den Parser. Der Fahrer bewegt
sich mit dem Speed-Sensor (im Simulator-Build mit `sim <km/h>`) oder per
`climb pos`. Frames vom Handy werden währenddessen ignoriert.

## Grenzen

- Nur mit einer in TrailBridge abgespielten GPX-Route mit Höhendaten. Bei
  OsmAnd-Navigation gibt es kein Profil.
- Anstiege über 5 km kommen in gröberem Raster (bis 250 m); „Steigung voraus"
  und Profilfarben mitteln dann über einen solchen Abschnitt.
- Über 50 km (oder bei kleiner MTU) passt der Anstieg nicht in einen Frame.
  Das Profilende gilt dann vorläufig als Gipfel, bis das nächste Stück kommt.
- Im NVS gespeicherte Werte für `contgrade`, `summitflat` und `summitdip`
  gehen den neuen Standardwerten vor (`climb defaults` setzt sie zurück).
- Verbindet sich der BikeComputer erst im Anstieg, zählt der Anstieg ab dort.
