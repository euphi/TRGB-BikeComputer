# T-RGB Fahrradcomputer

Ein Fahrradcomputer auf dem [LilyGO T-RGB](https://www.lilygo.cc/products/t-rgb): rundes
Touch-Display mit 480×480 Pixeln, ESP32-S3 mit BLE und WLAN, SD-Karte und LiPo-Lader auf
der Platine. Seine Hauptaufgabe ist die **Navigation am Lenker**: Abbiegehinweise aus
[OsmAnd](https://osmand.net/) oder entlang einer eigenen GPX-Route, gesendet von der
Android-App [TrailBridge](trailbridge/index.md) per BLE. Auf einer GPX-Route zeigt er
außerdem den Anstieg voraus. Daneben liest er übliche BLE-Fahrradsensoren und zeichnet
jede Fahrt auf SD-Karte auf.

<div class="shots" markdown>
<figure markdown>![Navigation](screenshots/nav.png)<figcaption>Navigation: die nächste Abbiegung</figcaption></figure>
<figure markdown>![Hauptscreen](screenshots/main.png)<figcaption>Hauptscreen, oben das nächste Manöver</figcaption></figure>
<figure markdown>![Anstieg](screenshots/climb.png)<figcaption>Anstieg auf einer GPX-Route</figcaption></figure>
</div>

Screenshots vom Gerät, aufgenommen während einer simulierten Fahrt von Villach nach Bovec.

## Navigation auf zwei Wegen

| | OsmAnd | Eigene GPX-Route |
|---|---|---|
| Wer die Route berechnet | OsmAnd auf dem Handy | du, vorab (z. B. mit BRouter, Komoot oder einem aufgezeichneten Track) |
| Abbiegehinweise | aus OsmAnd, mit Straßennamen und Fahrspuren | aus der GPX-Datei oder aus der Geometrie des Tracks abgeleitet |
| Höhenprofil und Anstiege | nein | ja, wenn die Datei Höhendaten hat |
| Braucht | OsmAnd und TrailBridge | nur TrailBridge |

In beiden Fällen bleibt das Handy in der Tasche: TrailBridge schickt das nächste Manöver
an den Fahrradcomputer. Der zeigt es in einer Pille auf dem Hauptscreen und wechselt kurz
vor der Abbiegung auf den großen Navigations-Screen. Einrichten:
[TrailBridge](trailbridge/index.md).

## Funktionen

**Navigation** vom Handy, über die Android-App [TrailBridge](trailbridge/index.md) per BLE
([Protokoll](trailbridge/PROTOCOL.md)). Zwei Quellen:

- **OsmAnd**: TrailBridge reicht OsmAnds Turn-by-Turn-Navigation weiter -- Manöver,
  Entfernung, Straßenname, übernächstes Manöver, Fahrspuren, Kreisverkehr-Ausfahrten
- **Eigene GPX-Route**: TrailBridge navigiert selbst entlang einer GPX-Datei, ohne
  OsmAnd. Abbiegehinweise kommen aus der Datei oder aus der Geometrie des Tracks. Hat die
  Datei Höhendaten, bekommt der Fahrradcomputer auch das Profil der Strecke voraus
- Der Navigations-Screen öffnet sich vor einem Manöver von selbst und schließt danach
  wieder: großer Abbiegepfeil, Distanzring, Straßenname, übernächstes Manöver, Fahrspuren.
  Dazwischen zeigt der Hauptscreen das nächste Manöver und seine Entfernung in einer
  kleinen Pille
- [`Tools/gpxenrich`](https://github.com/euphi/TRGB-BikeComputer/tree/main/Tools/gpxenrich)
  macht aus einem einfachen GPX-Track eine Route mit Abbiegehinweisen (von BRouter)

**Anstiege** auf einer GPX-Route: Der Anstiegs-Screen zeigt das Höhenprofil voraus,
gefärbt nach Steigung, mit Kategorie, Höhenmetern und Strecke bis zum Gipfel. Er öffnet
sich von selbst am Fuß eines bewerteten Anstiegs ([Anstiege](CLIMB.md)).

**Hauptscreen**: Geschwindigkeit (Zahl und äußerer Ring), Trittfrequenz, Puls mit
Zonenband, Temperatur, Höhe, Steigung, Strecke (Fahrt / Trip / Tour / gesamt), Fahrzeit
oder Uhrzeit, Fahrzustand, Symbole für WLAN, GPS-Fix und Akku.

**Fahrten und Statistik**

- Eine Taste startet eine Fahrt, pausiert sie („Cruise") und beendet sie; Stopps und
  Pausen werden aus der Geschwindigkeit erkannt
- Strecke, Zeit, Durchschnitts- und Höchstgeschwindigkeit und mittlere Trittfrequenz je
  Fahrt, Trip, Tour und gesamt; Durchschnitte mit oder ohne Stopps, Pausen und Cruise-Zeit
  ([Fahrten und Statistik](design/ride-state-machine.md))

**Sensoren**

- BLE: Geschwindigkeit und Trittfrequenz (CSC), Puls, Batteriestand jedes Sensors;
  Sensoren werden einmal gekoppelt und an ihre Adresse gebunden
- Die GPS-Position des Handys (über TrailBridge) für das Fahrtenlog und seine Zeit für
  die Uhr, wenn kein WLAN da ist
- BME280: barometrische Höhe und Steigung, Temperatur
  ([Höhenkalibrierung](HEIGHT.md))
- IMU-Sensor BMI160 zur Erfassung der [Straßenqualität](ROADQUALITY.md)
- [Forumslader](https://www.forumslader.de/) (Lader am Nabendynamo) über mein
  [BLE-Gateway](https://github.com/euphi/ESP32_FLClassic2BLE) (Build-Variante `-FL`)

**Einstellungen am Gerät**: WLAN (an/aus, Hotspot, Netze einrichten mit der
Display-Tastatur, [WLAN](WIFI.md)), BLE-Geräte (Verbindung, Batteriestand, Sensor
vergessen), Höhenkalibrierung, IMU-Kalibrierung, Neustart und Ausschalten.

**Aufzeichnung**

- Binärlog auf SD-Karte in datierten Ordnern: Fahrdaten alle 5 s mit GPS-Position und
  Fahrzuständen
- Debug-Log und rohes Forumslader-Log (auf dem Gerät wieder abspielbar)
- Python-[Werkzeuge](TOOLS.md), die Logs in CSV und GPX umwandeln, und ein
  [Dienst](LOGSERVICE.md), der die Sitzungen abholt, sobald der Fahrradcomputer im WLAN
  auftaucht, sie archiviert, GPX exportiert, mit Nextcloud abgleicht und zu Komoot hochlädt
- Sitzungsbericht: alle Kennzahlen einer Fahrt (Anstiege, Pulszonen, TRIMP, geschätzte
  Leistung, Wegequalität, Sensor-Aussetzer und andere technische Auffälligkeiten) als
  Webseite, Markdown oder JSON; auf Wunsch auch in Worten, geschrieben von einem lokalen LLM
  (Ollama) auf dem Heimserver, mit Prüfung jeder Zahl, die es verwendet
- Trainingsauswertung im Log-Dienst, im Rim-&-Ridge-Design: Fitness, Ermüdung und Form im
  Verlauf, Wochen nach Pulszonen, Zielrennen mit Countdown, Trainingsphase und den
  Anstiegen der Strecke, Bestzeiten auf wiederkehrenden Anstiegen
- Testfahrten (Sensor-Simulator, GPX-Testfahrt von TrailBridge) werden erkannt, markiert und
  aus Fahrtenliste, Training, Nextcloud und Komoot herausgehalten; Leerlauf-Sitzungen (an, aber
  nicht gefahren) werden archiviert und lassen sich vom Dienst aus auf dem BC löschen
- Räder mit Gewicht und Aerodynamik, den Fahrradcomputern nach Datum zugeordnet (mehrere
  Fahrradcomputer im selben Netz: `TRGB-BC`, `TRGB-FL`); anderswo aufgezeichnete Fahrten als
  GPX importieren, auch als frühere Teilnahmen an einem Ziel

**Web-Oberfläche** (im WLAN)

- Logdateien herunterladen, abspielen und löschen; Live-Log mit einstellbaren Log-Leveln
- Fahrstatistik mit Diagramm, Kilometerstand und Radumfang
- BLE-Sensoren verwalten; Debug-Seiten für Sensoren, Beschleunigungssensor, Anstiege und
  Abstürze
- WLAN: gespeicherte Netze und ihre Priorität, Hotspot-Einstellungen ([WLAN](WIFI.md))
- Firmware und Dateisystem über WLAN aktualisieren

**Serielle Konsole** mit Zeileneditor, Verlauf und Tab-Vervollständigung
([Debugging](DEBUG.md)).

**Für die Entwicklung**: ein Simulator-Build mit simulierten Sensoren
([Simulator](SIMULATOR.md)), Screenshots und Touch-Eingaben über HTTP
(`Tools/uishot.py`), Host-Tests für die reinen Algorithmen (`test/`, `Tools/tests/`).

## Hardware

- LilyGO T-RGB (die beiden Builds unterscheiden sich im Touch-Controller, `TRGB_ROUND` /
  `TRGB_OVAL`, und in der Forumslader-Unterstützung)
- Optional: BME280 und BMI160 am I²C-Bus
- LiPo-Akku
- Gehäuse und Lenkerhalter aus dem 3D-Drucker -- ein parametrisches Modell für das Canyon
  CP0007 Gravel-Cockpit liegt in
  [`cad/`](https://github.com/euphi/TRGB-BikeComputer/tree/main/cad)

![T-RGB Fahrradcomputer am Lenker (ältere Oberfläche)](IMG20230406075242-cropped.jpg)

## Bauen

[PlatformIO](https://platformio.org/), Arduino-Framework (pioarduino-Plattform):

```bash
pio run -e trgb-esp32-s3 -t upload        # Standard: BLE- und I²C-Sensoren
pio run -e trgb-esp32-s3-FL -t upload     # Forumslader-Variante
pio run -e trgb-esp32-s3 -t uploadfs      # Dateien der Web-Oberfläche (data/site)
```

`trgb-esp32-s3-ota` und `trgb-esp32-s3-FL-ota` sind dieselben Builds, hochgeladen per
WLAN an den `/update`-Endpunkt des Geräts. Ziel ist `TRGB-BC.local`; löst mDNS nicht auf,
die IP angeben: `pio run -e trgb-esp32-s3-ota -t upload --upload-port 192.168.x.y`.
`trgb-esp32-s3-sim` ist der Simulator-Build.

Der Build wendet kleine Patches auf zwei Bibliotheken an (`apply_patches.py`); dafür wird
das Programm `patch` gebraucht.

## Wie es weitergeht

- [Roadmap](ROADMAP.md): wie gut jede Funktion getestet ist, bekannte Fehler, geplante
  Funktionen
- [TrailBridge](trailbridge/index.md): die Android-App, die die Navigation auf den
  Fahrradcomputer bringt, und ihr BLE-Protokoll
- [Anstiege](CLIMB.md) und [Fahrten und Statistik](design/ride-state-machine.md): wie sich
  das Gerät unterwegs verhält
- [Wegequalität](ROADQUALITY.md): die Spezialfunktion rund um den IMU-Sensor
- [Logformat und Werkzeuge](TOOLS.md), [Log-Dienst](LOGSERVICE.md): was mit den
  aufgezeichneten Fahrten passiert
- [Debugging](DEBUG.md), [Simulator](SIMULATOR.md), [Fallstricke](PITFALLS.md),
  [Design-System](design/rim-ridge-design-system.md): für die Arbeit an der Firmware

Quellcode: [github.com/euphi/TRGB-BikeComputer](https://github.com/euphi/TRGB-BikeComputer).
Mitarbeit ist willkommen; die Roadmap zeigt, wo Hilfe gesucht wird.

## Lizenzen und Quellen

- [Cycling icons created by Futuer - Flaticon](https://www.flaticon.com/free-icons/cycling)
- [Parked bicycles - icon by Prashanth Rapolu 15](https://de.freepik.com/icon/fahrrad_7764338#fromView=resource_detail&position=7)
