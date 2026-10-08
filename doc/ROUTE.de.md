# Streckenübersicht: Ziel, Wegpunkte und Anstiege

Spielt TrailBridge eine GPX-Route ab, sagt es dem Fahrradcomputer auch, was **noch vor ihm auf
der Strecke** liegt: das Ziel, die Wegpunkte der GPX-Datei (`wpt`, z. B. Gipfel,
Verpflegung, Schotter-Sektor) und die Anstiege der ganzen Route
([Protokoll](trailbridge/PROTOCOL.md), „Streckenübersicht-Service"). Der Fahrradcomputer zeigt
sie als eine Liste auf dem Screen `RimRidgeRoute`.

![Streckenübersicht](screenshots/route.png){ width="260" }

Das Bild ist auf dem Rechner mit dem echten LVGL und der Demo-Strecke (`route demo`, siehe
unten) gerendert, kein Foto vom Gerät.

| Teil | Datei |
|---|---|
| Wire-Format | `src/BikeOverviewProtocol.h` (und `OVERVIEW_REVISION` in `src/BikeNavProtocol.h`) |
| Parser, Entfernungen und Zeiten (rein, Host-Test) | `src/RouteOverview.*`, `test/native_routeoverview/routeoverview_test.cpp` |
| Anbindung: BLE-Lesen, Position, CLI, Web, Demo | `src/RouteMonitor.*`, `BLEDevices::findOverview()`/`overviewReaderTask()` |
| Screen | `src/ui/RimRidgeRouteCustFunc.*`, `UIFacade::showRouteScreen()` |

## Bedienung

- **Öffnen**: auf dem Navigations-Screen **nach links wischen**. Das gilt als Wegwischen des
  Navigations-Screens, wie beim Wegequalitäts-Screen; das nächste Manöver holt ihn wieder.
- **Schließen**: jedes Wischen auf dem Streckenscreen geht zurück.
- **Blättern**: die Liste scrollt senkrecht; oben steht der nächste Eintrag.

Die Kopfzeile zeigt das **Ziel** mit der Restentfernung und der **Ankunftszeit**. Darunter
stehen Wegpunkte und Anstiege gemischt in der Reihenfolge, in der der Fahrer sie erreicht
(ein Anstieg zählt an seinem Fuß):

| Zeile | Links | Rechts | Zweite Zeile |
|---|---|---|---|
| Wegpunkt (Punkt) | Name | Entfernung | Ankunftszeit und Restzeit, z. B. `Ankunft 14:32 (18 min)` |
| Anstieg (Balken in der Farbe seiner mittleren Steigung) | `Anstieg 2/7` | Entfernung bis zum Fuß; im Anstieg `noch 2.8 km` bis zum Gipfel | Höhenmeter, Länge, mittlere Steigung, z. B. `450 Hm, 8.2 km, 5.5 %` |

Die Farbe des Balkens folgt denselben Steigungsstufen wie das Profil auf dem
[Anstiegs-Screen](CLIMB.md). Wegpunkte und Anstiege verschwinden aus der Liste, wenn sie
erreicht sind. Die Fußzeile nennt die Zahl der Wegpunkte voraus und „Liste gekürzt", wenn
das ferne Ende fehlt (siehe unten). Ohne Strecke steht „Keine Strecke" da.

## Woher Entfernungen und Zeiten kommen

Das Handy schickt keine Entfernungen, sondern **Anker**: zu jedem Wegpunkt die Restdistanz
(und Restzeit) der Route *an diesem Wegpunkt*. Der Fahrradcomputer zieht sie vom letzten
Nav-Frame ab -- die Liste braucht während der Fahrt keine neue Übertragung:

```
Entfernung = REMAINING_DISTANCE_M - REMAINING_AT_M           (negativ: erreicht)
Zeit       = REMAINING_TIME_S - REMAINING_TIME_AT_S          (GPX hat Zeitstempel)
Zeit       = REMAINING_TIME_S * Entfernung / REMAINING_DISTANCE_M    (hat sie nicht)
```

Zwischen zwei Nav-Frames wird die Entfernung mit dem Radsensor weitergeführt, wie beim
Anstiegs-Screen; die Zeit ist die des letzten Frames, damit die Ankunftsuhrzeit nicht
wandert. Die Ankunftszeit ist die Uhrzeit plus Restzeit; ohne gültige Uhr zeigt der Screen
nur die Restzeit.

Welche der beiden Zeit-Formeln gilt, entscheidet die GPX-Datei: Mit Zeitstempeln (eine
aufgezeichnete Fahrt, oder eine geplante Route, deren Router seine Schätzung hineinschreibt
-- BRouter nur mit `assign showtime = true`) kommen die Zeiten aus der Datei und kennen die
Anstiege. Ohne teilt TrailBridge die Strecke durch die aktuelle Geschwindigkeit; vor einem
langen Anstieg ist das zu optimistisch. Anstiege haben keine Zeit.

## Wie gelesen wird

Der Service ist **nur lesbar**. Das Signal „es gibt etwas Neues" ist der Tag
`OVERVIEW_REVISION` in jedem Nav-Frame: weicht er von der Revision der gehaltenen
Übersicht ab, wird die Characteristic gelesen -- ein Long Read mit bis zu 512 Byte. Das
läuft in einem eigenen Task (`BLEOverview`), nie im Indicate-Callback (ein Long Read
blockiert) und nicht im Scan-Task (der zwischen zwei Runden 20 s schläft). Ein Nav-Frame
ohne den Tag (Navigation aus OsmAnd), `NAV_NONE` oder eine verlorene Verbindung verwirft die
Übersicht. Ein fehlgeschlagener Read wird nach 4 s wiederholt.

Eine neue Übersicht kommt beim Start der Route, wenn ein Wegpunkt erreicht ist und an jedem
Gipfel -- auf einer Fahrt ein paar Dutzend Reads. Dazwischen wird die Liste nur neu
gerechnet.

## Grenzen

- Ein Frame hat höchstens 512 Byte. Liegen mehr Wegpunkte und Anstiege voraus, als hinein
  passen, lässt TrailBridge das ferne Ende weg; es rückt nach, sobald vorn etwas erreicht
  ist. Die Firmware hält 48 Einträge und zeigt die nächsten 24.
- Namen sind 32 Byte UTF-8. Die Schriften des Displays haben ASCII, `äöüÄÖÜß` und `°`;
  andere Zeichen werden nicht gezeichnet.
- Es fehlt: die **noch zu steigenden Höhenmeter** eines schon begonnenen Anstiegs. Dafür
  braucht es eine Protokollerweiterung (TrailBridge-Roadmap).
- **Am Gerät noch nicht probiert**, vor allem der Long Read über eine echte Verbindung (aus
  dem Quelltext der BLE-Bibliothek gelesen, nicht getestet). Es gibt Host-Test, Simulator-Build
  und ein Rendering des Screens auf dem Rechner.

## Demo und Debugging

Den Screen ohne Handy ansehen (die Demo-Strecke ist 42 km lang, mit fünf Wegpunkten und drei
Anstiegen, gefahren mit 20 km/h; Frames vom Handy werden währenddessen ignoriert):

```
route demo            # starten; der Fahrer bewegt sich mit dem Radsensor
route pos 25000       # Fahrer 25 km nach dem Start (im zweiten Anstieg)
route show            # Screen öffnen (Wischen schließt ihn)
route status          # die Liste, wie der Screen sie bekommt
route demo off
```

Im Web: `/debug/route.json` (Zustand und Liste), `/debug/route/demo` (`?off=1`, `?pos=<m>`).
Host-Test: `g++ -std=c++17 -O2 -Isrc src/RouteOverview.cpp test/native_routeoverview/routeoverview_test.cpp -o /tmp/routeoverview_test && /tmp/routeoverview_test`
(der Build-Befehl steht im Dateikopf). Die GPX-Testfahrt von TrailBridge sendet das
Echte, ohne Fahrt.
