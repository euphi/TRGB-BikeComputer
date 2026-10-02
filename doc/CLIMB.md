# Anstiege: Höhenprofil und Kletter-Anzeige

TrailBridge schickt beim Abfahren einer GPX-Route das Höhenprofil voraus
(`PROTOCOL.md` im TrailBridge-Repo, „Höhenprofil-Service"). Der BikeComputer
erkennt darin Anstiege, bewertet sie und zeigt sie auf dem Screen
`RimRidgeClimb` ([Design](design/rim-ridge-design-system.md), §6).

| Teil | Datei |
|---|---|
| Wire-Format | `src/BikeProfileProtocol.h` |
| Algorithmus (rein, Host-Test) | `src/ClimbProfile.*`, `test/native_climb/climb_test.cpp` |
| Anbindung: BLE, Position, Einstellungen, CLI, Web, Demo | `src/ClimbMonitor.*`, `BLEDevices::subscribeProfile()` |
| Screen | `src/ui/RimRidgeClimbCustFunc.*`, `UIFacade::updateClimb()` |

## Was vom Handy kommt, und was die Firmware daraus macht

TrailBridge schickt keinen „Anstieg", sondern ein Fenster der Route: bis zu
5 km Höhen im 25-m-Raster **ab der aktuellen Position**, sobald die nächsten
200 m im Mittel 4 % steigen – also auch für jeden kleinen Hügel. Bei langen
Anstiegen folgt das nächste Fenster, wenn die Mitte des letzten erreicht ist.
`PROFILE_NONE` kommt, wenn die nächsten 200 m flacher als 2 % sind.

Die Firmware

- **fügt die Fenster zusammen** (gleiches Raster, überlappend) und behält den
  schon gefahrenen Teil, bis zu 8 km. So zeigt das Profil den Anstieg ab dem
  Fuß, nicht ab der Position des letzten Fensters.
- **findet den Anstieg**: Fuß = erster Punkt, ab dem die nächsten 100 m im
  Mittel 3 % steigen. Gipfel = der letzte Punkt vor einem Gefälle von mehr als
  10 m oder vor 500 m ohne mittlere Steigung von 1,5 %. Ein kurzes Flachstück
  oder eine Senke beendet den Anstieg also nicht.
- **führt einen Anstieg fort**, wenn das Handy an so einer Stufe `PROFILE_NONE`
  und kurz danach ein neues Profil schickt.
- **kennt die Position** aus der Restdistanz der Nav-Frames, zwischen zwei
  Frames fortgeschrieben mit der Strecke des Speed-Sensors.

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
- Profilfarbe nach Steigung des 25-m-Abschnitts: unter 1 % blau, bis 4 % grün,
  bis 7 % gelb, bis 10 % orange, darüber rot. Der gefahrene Teil ist
  abgedunkelt.
- `>` vor einer Zahl: der Gipfel liegt noch nicht im Profil, der Wert ist ein
  Mindestwert. Die Kategorie kann dann noch steigen.

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
| `contgrade` | 1,5 % | mittlere Steigung ab dem bisherigen Gipfel, mit der der Anstieg weitergeht |
| `summitflat` | 500 m | Flachstück, das den Anstieg beendet; auch der Abstand, in dem ein neuer Anstieg den alten fortsetzt |
| `summitdip` | 10 m | Gefälle, das den Anstieg beendet |
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
- Das Handy schickt höchstens 5 km voraus. Bei längeren Anstiegen sind
  Gipfel, Rest-Höhenmeter und Kategorie zunächst Mindestwerte (`>`).
- `PROFILE_NONE` kommt je nach Gelände 70–130 m vor dem Gipfel. Die Anzeige
  endet dort, die letzten Höhenmeter zählt sie nicht herunter.
- Verbindet sich der BikeComputer erst im Anstieg, zählt der Anstieg ab dort.
