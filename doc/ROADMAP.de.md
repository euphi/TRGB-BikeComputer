# Roadmap

Was offen ist, grob nach Priorität. Was schon funktioniert, steht im
[Überblick](index.md); diese Seite sagt, wie gut es getestet ist und was als Nächstes
kommt. Hilfe ist willkommen, besonders an den markierten Stellen.

## Wo wir stehen

| Bereich | Stand |
|---|---|
| Sensoren, Hauptscreen, Aufzeichnung, Web-Oberfläche | im Alltag genutzt |
| Navigation aus OsmAnd | gefahren |
| Navigation mit GPX-Route und Anstiegs-Screen | einmal gefahren (2026-10-04). Danach wurde das Höhenprofil auf ein rollendes Fenster umgestellt und der Anstiegs-Screen überarbeitet; dieser Stand ist nur mit dem Host-Test geprüft, nicht am Gerät |
| Fahrzustände und Statistik | einmal gefahren (2026-10-04, [Cheatsheet](RIDE-TEST-CHEATSHEET.md)). Seitdem behoben: Ohne Trittfrequenz-Sensor zählte die ganze Fahrt als Rollen |
| Log-Sitzungen, Uhr per GPS | einmal gefahren. Seitdem behoben: Eine falsche GPS-Zeit verstellte die Uhr um Stunden; eine Sitzung endet jetzt auch, wenn die Fahrt beendet wird. Beide Korrekturen sind am Gerät ungetestet |
| [Wegequalität](ROADQUALITY.md) und manuelle Wege-Labels | zwei Fahrten; die Schwellen sind noch die ersten Schätzwerte |
| WLAN-Einrichtung am Gerät, Hotspot ([Doku](WIFI.md)) | am Gerät getestet; die Wahl des Netzes nach Empfangsstärke ist neu und ungetestet |
| Einstellungs-Seiten, BLE-Geräteseite, Log-Dienst | funktioniert |
| [Streckenübersicht](ROUTE.md) (Ziel, Wegpunkte, Anstiege der GPX-Route) | neu, am Gerät nicht gelaufen: Host-Test, Simulator-Build und ein Rendering des Screens auf dem Rechner gibt es; der Long Read des 512-Byte-Frames über eine echte BLE-Verbindung ist unerprobt |
| Höhenkalibrierung ([Doku](HEIGHT.md)) | Presets, manuelle Eingabe und Webseite am Gerät getestet; die GPS-Taste zeigt die GPS-Höhe des Handys an; das Kalibrieren damit wurde noch nicht ausprobiert |
| Forumslader-Variante | baut, selten getestet; ihr Screen ist der letzte aus der alten SquareLine-Oberfläche |

## Bekannte Fehler

Offen, zu beheben.

- **Ein viertes BLE-Gerät verbindet sich nicht**: Mit Speed-Sensor, Pulsgurt und
  TrailBridge blieb auf der Testfahrt der Trittfrequenz-Sensor außen vor. Vermutete
  Ursache ist die Grenze des BLE-Stacks von drei Verbindungen
  ([Fallstricke](PITFALLS.md)). Ein Build mit vier (`trgb-esp32-s3-ble4`) lässt sich bauen,
  lief aber noch nicht auf dem Gerät.

## Als Nächstes: unterwegs bedienbar

Sitzt das Gerät im Gehäuse, gibt es kein USB und meist kein WLAN. Alles, was man auf
einer Fahrt braucht, muss am Display gehen. Details: [Bedienung unterwegs](USABILITY-TODO.md).

- Wege-Labels, die sich auf schlechtem Weg treffen lassen, und eine Regel für Fehltipps.
- „Rad senkrecht" (Fahrstuhl) und „Rad auf dem Auto" erkennen und aus der Fahrt herauslassen.
- Durchschnittsgeschwindigkeit und die übrige Statistik auf dem Display. Sie wird
  berechnet und in der Web-Oberfläche gezeigt; der Hauptscreen hat kein Widget dafür.
- Restdistanz und Restzeit bis zum Ziel auf dem Navigations-Screen selbst (auf dem
  [Streckenscreen](ROUTE.md) stehen sie jetzt, einmal nach links wischen).
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
- **Streckenübersicht, der Rest** ([bisher fertig](ROUTE.md)): die noch zu steigenden
  Höhenmeter in einem begonnenen Anstieg (braucht eine Protokollerweiterung, siehe
  TrailBridge-Roadmap); ein Weg zum Screen vom Hauptscreen aus (heute nur Wischen nach
  links auf dem Navigations-Screen); und ein Pop-up kurz vor einem Wegpunkt, wie ihn der
  Navigations-Screen vor einer Abbiegung hat.
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
  und die Fahrt in Worten vom lokalen LLM auf): Trainingsvorschläge für die Zielrennen und
  ein Wochenrückblick vom lokalen LLM, das nur das JSON von Bericht und Training bekommt;
  Wegequalität auf
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
- Eine Log-Sitzung endet mit einem Neustart oder wenn die Fahrt am Gerät beendet wird.
  Eine Pause mit ausgeschaltetem Gerät teilt eine Fahrt deshalb in zwei Sitzungen.
- Anstiege über 5 km kommen in gröberem Raster, siehe [Anstiege](CLIMB.md).
