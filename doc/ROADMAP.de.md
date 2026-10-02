# Roadmap

Was offen ist, grob nach Priorität. Was schon funktioniert, steht im
[Überblick](index.md); diese Seite sagt, wie gut es getestet ist und was als Nächstes
kommt. Hilfe ist willkommen, besonders an den markierten Stellen.

## Wo wir stehen

| Bereich | Stand |
|---|---|
| Sensoren, Hauptscreen, Aufzeichnung, Web-Oberfläche | im Alltag genutzt |
| Navigation aus OsmAnd | gefahren |
| Wegequalität und manuelle Wege-Labels | eine Testfahrt; die Schwellen sind noch die ersten Schätzwerte |
| Fahrzustände und Statistik | mit dem Simulator-Build getestet; eine echte Fahrt steht aus ([Cheatsheet](RIDE-TEST-CHEATSHEET.md)) |
| Navigation mit GPX-Route und Anstiegs-Screen | mit der simulierten Fahrt von TrailBridge und dem Demo-Profil getestet; noch kein echter Anstieg gefahren |
| Einstellungen, Log-Sitzungen, Log-Dienst | funktioniert; das Stellen der Uhr per GPS wurde nicht einzeln geprüft |
| Forumslader-Variante | baut, selten getestet; ihr Screen ist der letzte aus der alten SquareLine-Oberfläche |

## Bekannte Fehler

Offen, zu beheben.

- **Watchdog-Reset durch den I²C-Bus**: BME280 und BMI160 werden aus zwei Tasks ohne
  gemeinsame Sperre gelesen. Einmal aufgetreten, aus dem Core-Dump analysiert.
- **Lücken im FIFO des Beschleunigungssensors**: Auf der ersten Testfahrt hatte etwa ein
  Drittel der Wegequalitäts-Intervalle Lücken, auch im Stand.
- **Zwei CSC-Sensoren**: Wird der erste entfernt, während der zweite verbunden ist, gerät
  die Zuordnung bis zum nächsten Neustart durcheinander.

## Als Nächstes: unterwegs bedienbar

Sitzt das Gerät im Gehäuse, gibt es kein USB und meist kein WLAN. Alles, was man auf
einer Fahrt braucht, muss am Display gehen. Details: [Bedienung unterwegs](USABILITY-TODO.md).

- Wege-Labels, die sich auf schlechtem Weg treffen lassen, und eine Regel für Fehltipps.
- „Rad senkrecht" (Fahrstuhl) und „Rad auf dem Auto" erkennen und aus der Fahrt herauslassen.
- **WLAN-Konfiguration**: Das Netz ist heute in die Firmware einkompiliert, die über die
  Web-Oberfläche gespeicherten Zugangsdaten werden ignoriert. Gewünscht: die gespeicherten
  verwenden, mehrere Netze (Handy-Hotspot), Access-Point-Modus vom Gerät aus mit geführter
  Ersteinrichtung, Zugangsdaten über die serielle Konsole.
- Durchschnittsgeschwindigkeit und die übrige Statistik auf dem Display. Sie wird
  berechnet und in der Web-Oberfläche gezeigt; der Hauptscreen hat kein Widget dafür.
- Restdistanz und Restzeit bis zum Ziel auf dem Navigations-Screen (werden empfangen,
  nicht angezeigt).
- Konfiguration am Gerät (Radumfang, Sensoren koppeln) -- heute nur in der Web-Oberfläche.
- Batteriestand der BLE-Sensoren auf dem Display (wird schon gelesen).
- Fortschrittsanzeige beim Firmware-Update.

## Geplante Funktionen

Noch nicht entworfen.

- **Mehrere Fahrräder**: Statistik, Kilometerstand und Einstellungen (Radumfang, Sensoren,
  Kalibrierung des Beschleunigungssensors) je Rad, mit Auswahl des Rads am Gerät.
- **Service-Intervalle**: eine Seite, die sich merkt, wann Kettenöl, Kette, Ritzel,
  Umwerfer und Reifen zuletzt gewartet wurden, und die seither gefahrene Strecke zeigt.
  Baut auf dem Kilometerstand je Rad auf.
- **Wegpunkte in der Navigation**: die Wegpunkte einer Route (Gipfel, Verpflegung,
  Schotter-Sektor) mit Name und Entfernung anzeigen. Braucht eine Erweiterung des
  [TrailBridge-Protokolls](trailbridge/PROTOCOL.md); heute kann `Tools/gpxenrich` sie nur
  als benanntes „geradeaus" weiterreichen.
- **Anstiegsübersicht für die ganze Route**: der wievielte Anstieg von wie vielen, und
  die noch zu fahrenden Höhenmeter. Braucht ebenfalls eine Protokollerweiterung.
- **Sichern und Wiederherstellen** von Kilometerstand, Statistik und Einstellungen über
  die Web-Oberfläche. Sie liegen nur im Flash des Geräts und gehen verloren, wenn er
  gelöscht wird.
- **Touch-Sperre** gegen Geistertipps durch Regentropfen.
- **FIT-Export** im Log-Dienst, neben GPX.

## Später

- Leistung auf dem Hauptscreen: Das Widget gibt es, eine Datenquelle nicht (BLE Cycling
  Power Service oder eine Schätzung). Das Binärlog hat dafür auch kein freies Feld.
- Distanzring auf dem Hauptscreen (der Mechanismus des Geschwindigkeitsrings lässt sich
  wiederverwenden).
- Diagramme auf dem Gerät; den alten Chart-Screen gibt es nicht mehr, er wird neu gestaltet.
- Ein Screen für die Wegequalität mit mehr als der farbigen Linie, und das Abstimmen der
  Schwellen an aufgezeichneten Fahrten.
- Forumslader-Screen im Rim-&-Ridge-Design; Forumslader-Strecke in der Statistik.
- Höhenprofil auch bei OsmAnd-Navigation (heute nur mit einer GPX-Route).
- Binärlog auf dem Gerät abspielen (heute geht das nur mit dem Forumslader-Textlog).
- Neues Gehäuse für das Canyon CP0007 Gravel-Cockpit -- parametrisches Modell in
  [`cad/`](https://github.com/euphi/TRGB-BikeComputer/tree/main/cad), die Maße aus Fotos
  sind noch mit dem Messschieber zu prüfen.

Niedrige Priorität:

- Erinnerung ans Essen und Trinken, nach Zeit oder Strecke.
- Radar (Garmin Varia über BLE).
- Restlaufzeit des Akkus. Der Akku ist ein einfacher LiPo, sein Ladezustand wird nur aus
  der Spannung geschätzt und ist ungenau; er hält etwa sechs Stunden.

## Hilfe gesucht

- Bessere und weitere Halter für den 3D-Druck, auch für andere Lenker.
- Animationen auf dem Display (Fahrzustand, Puls).
- Mehr aus der aufgezeichneten Wegequalität machen, z. B. Karten des Untergrunds oder
  Heatmaps aus vielen Fahrten.
- Kleineres Boot-Logo im Flash (liegt roh vor, 460 KB; komprimiert wären es etwa 11 KB,
  braucht eine Änderung in TRGBArduinoSupport).

## Bekannte Grenzen

- Der Fahrradcomputer verbindet sich mit der ersten TrailBridge-Instanz, die er findet.
  Android wechselt seine BLE-Adresse, das Handy lässt sich deshalb nicht wie die Sensoren
  fest zuordnen.
- Eine Log-Sitzung ist ein Boot, nicht eine Fahrt. Start und Ende der Fahrt sind darin
  markiert.
- Anstiege über 5 km kommen in gröberem Raster, siehe [Anstiege](CLIMB.md).
