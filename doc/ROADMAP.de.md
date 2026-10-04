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
| Höhenkalibrierung ([Doku](HEIGHT.md)) | Presets, manuelle Eingabe und Webseite am Gerät getestet; die GPS-Taste braucht einen TrailBridge-Build mit `MSL_ALTITUDE_DM` (0x0E) und einen echten Fix und ist ungetestet |
| Forumslader-Variante | baut, selten getestet; ihr Screen ist der letzte aus der alten SquareLine-Oberfläche |

## Bekannte Fehler

Offen, zu beheben.

- **Lücken im FIFO des Beschleunigungssensors**: Auf der ersten Testfahrt hatte etwa ein
  Drittel der Wegequalitäts-Intervalle Lücken, auch im Stand. Möglicherweise dieselbe
  Ursache wie der Watchdog-Reset durch den I²C-Bus, der behoben ist (siehe
  [Fallstricke](PITFALLS.md)): auf der nächsten Fahrt prüfen.

## Als Nächstes: unterwegs bedienbar

Sitzt das Gerät im Gehäuse, gibt es kein USB und meist kein WLAN. Alles, was man auf
einer Fahrt braucht, muss am Display gehen. Details: [Bedienung unterwegs](USABILITY-TODO.md).

- Wege-Labels, die sich auf schlechtem Weg treffen lassen, und eine Regel für Fehltipps.
- „Rad senkrecht" (Fahrstuhl) und „Rad auf dem Auto" erkennen und aus der Fahrt herauslassen.
- Durchschnittsgeschwindigkeit und die übrige Statistik auf dem Display. Sie wird
  berechnet und in der Web-Oberfläche gezeigt; der Hauptscreen hat kein Widget dafür.
- Restdistanz und Restzeit bis zum Ziel auf dem Navigations-Screen (werden empfangen,
  nicht angezeigt).
- Konfiguration am Gerät (Radumfang, Sensoren koppeln) -- heute nur in der Web-Oberfläche.
- Batteriestand der BLE-Sensoren auf dem Display (wird schon gelesen).
- Fortschrittsanzeige beim Firmware-Update.

## Geplante Funktionen

Noch nicht entworfen.

- **Mehrere Räder je Fahrradcomputer** (der Log-Dienst kennt Räder und welcher
  Fahrradcomputer an welchem Rad fährt schon, siehe [Log-Dienst](LOGSERVICE.md#rader)): ein
  Fahrradcomputer wird umgebaut, das Rad an seinen Sensoren erkannt (Adresse des
  Trittfrequenz-/Speed-Sensors), mit Statistik, Kilometerstand und Einstellungen (Radumfang,
  Kalibrierung des Beschleunigungssensors) je Rad auf dem Gerät.
- **Service-Intervalle**: eine Seite, die sich merkt, wann Kettenöl, Kette, Ritzel,
  Umwerfer und Reifen zuletzt gewartet wurden, und die seither gefahrene Strecke zeigt.
  Baut auf dem Kilometerstand je Rad auf.
- **Wegpunkte in der Navigation**: die Wegpunkte einer Route (Gipfel, Verpflegung,
  Schotter-Sektor) mit Name, Entfernung und Ankunftszeit anzeigen. TrailBridge liest die
  Wegpunkte (`wpt`) einer GPX-Datei noch nicht, und das
  [TrailBridge-Protokoll](trailbridge/PROTOCOL.md) hat keinen Tag dafür; heute kann
  `Tools/gpxenrich` sie nur als benanntes „geradeaus" weiterreichen.
- **Anstiegsübersicht für die ganze Route**: der nächste Anstieg, bevor er beginnt
  (Entfernung bis zum Fuß, Höhenmeter), der wievielte Anstieg von wie vielen, und die
  noch zu fahrenden Höhenmeter. Heute kommt das Profil erst 500 m vor dem Fuß.
  TrailBridge kennt alle Anstiege, sobald die Route geladen ist; braucht ebenfalls eine
  Protokollerweiterung.
- **Richtung zurück zur Route**: Abseits der Route sendet TrailBridge nur die Entfernung
  zu ihr. Gewünscht: auch die Peilung, als Pfeil angezeigt. Braucht eine
  Protokollerweiterung.
- **Kopplung mit dem Handy**: Die BLE-Services von TrailBridge lassen sich ohne Kopplung
  und Verschlüsselung lesen; jedes BLE-Gerät in Reichweite kann sich verbinden und die
  Position mitlesen. Mit Bonding wäre die Verbindung verschlüsselt, und der
  Fahrradcomputer könnte sein Handy trotz der wechselnden Adresse wiedererkennen (siehe
  die bekannten Grenzen unten). Braucht Änderungen in der App und in der Firmware.
- **Sichern und Wiederherstellen** von Kilometerstand, Statistik und Einstellungen über
  die Web-Oberfläche. Sie liegen nur im Flash des Geräts und gehen verloren, wenn er
  gelöscht wird.
- **Touch-Sperre** gegen Geistertipps durch Regentropfen.
- **FIT-Export** im Log-Dienst, neben GPX.
- **Logs weiter auswerten** im Log-Dienst (baut auf Sitzungsbericht und Trainingsseiten
  auf): ein Bericht in Textform und Trainingsvorschläge für die Zielrennen von einem lokalen
  LLM auf dem Heimserver, das nur das JSON von Bericht und Training bekommt; Wegequalität auf
  OSM-Wege abgebildet (Map-Matching) und eine JOSM-Datei mit `smoothness`/`surface`-Vorschlägen
  zum Prüfen von Hand.

## TrailBridge-App

Die Android-App hat ihre eigene Roadmap:
[TrailBridge ROADMAP.md](https://github.com/euphi/TrailBridge/blob/main/ROADMAP.md).
Dort stehen die App-Seite der Funktionen auf dieser Seite und alles, was nur die App betrifft
(englische Übersetzung, Zeitraffer für die simulierte Fahrt, Meldungen an OsmAnd).

## Später

- Leistung auf dem Hauptscreen: Das Widget gibt es, eine Datenquelle nicht (BLE Cycling
  Power Service oder eine Schätzung). Das Binärlog hat dafür auch kein freies Feld.
- Distanzring auf dem Hauptscreen (der Mechanismus des Geschwindigkeitsrings lässt sich
  wiederverwenden).
- Diagramme auf dem Gerät; den alten Chart-Screen gibt es nicht mehr, er wird neu gestaltet.
- Ein Screen für die Wegequalität mit mehr als der farbigen Linie, und das Abstimmen der
  Schwellen an aufgezeichneten Fahrten.
- Forumslader-Screen im Rim-&-Ridge-Design; Forumslader-Strecke in der Statistik.
- Streckenlinie auf dem Display: der Verlauf einer GPX-Route um die aktuelle Position,
  um Abbiegungen kommen zu sehen und zurückzufinden. Braucht einen neuen BLE-Service
  für die Geometrie.
- Rückkanal vom Fahrradcomputer zu TrailBridge (heute gibt es keine schreibbare
  Characteristic): Tasten am Gerät für die App, Fahrtdaten aufs Handy,
  WLAN-Einstellungen vom Handy.
- Binärlog auf dem Gerät abspielen (heute geht das nur mit dem Forumslader-Textlog).
- Neues Gehäuse für das Canyon CP0007 Gravel-Cockpit -- parametrisches Modell in
  [`cad/`](https://github.com/euphi/TRGB-BikeComputer/tree/main/cad), die Maße aus Fotos
  sind noch mit dem Messschieber zu prüfen.

Niedrige Priorität:

- Erinnerung ans Essen und Trinken, nach Zeit oder Strecke.
- Radar (Garmin Varia über BLE).
- Restlaufzeit des Akkus. Der Akku ist ein einfacher LiPo, sein Ladezustand wird nur aus
  der Spannung geschätzt und ist ungenau; er hält etwa sechs Stunden.
- Echte Sensorwerte, die TrailBridge weiterreicht (z. B. ein Pulsgurt am Handy). Die
  Protokoll-Tags sind reserviert; der Fahrradcomputer koppelt seine Sensoren selbst,
  der Nutzen ist deshalb klein.

Auf der langen Bank:

- Mehrere Personen: Fahrerdaten, Ziele und Training je Person im Log-Dienst, Fahrten über
  Fahrradcomputer oder Rad zugeordnet.
- Höhenprofil und Streckenlinie auch bei OsmAnd-Navigation (heute nur mit einer
  GPX-Route). Die AIDL-Schnittstelle von OsmAnd liefert weder die Routengeometrie noch
  Höhen; das braucht eine Erweiterung in OsmAnd selbst.
- TrailBridge beim Start des Handys automatisch starten. Im Moment kein Bedarf.

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
  fest zuordnen. Eine Kopplung würde das lösen, siehe die geplanten Funktionen.
- Eine Log-Sitzung ist ein Boot, nicht eine Fahrt. Start und Ende der Fahrt sind darin
  markiert.
- Anstiege über 5 km kommen in gröberem Raster, siehe [Anstiege](CLIMB.md).
