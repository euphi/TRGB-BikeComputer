# T-RGB Fahrradcomputer

Ein Fahrradcomputer auf dem [LilyGO T-RGB](https://www.lilygo.cc/products/t-rgb): rundes
Touch-Display mit 480×480 Pixeln, ESP32-S3 mit BLE und WLAN, SD-Karte und LiPo-Lader auf
der Platine. Er liest übliche BLE-Fahrradsensoren, zeigt Abbiegehinweise und den nächsten
Anstieg vom Handy, zeichnet jede Fahrt auf SD-Karte auf und bewertet den Untergrund mit
einem eigenen Beschleunigungssensor.

<div class="shots" markdown>
<figure markdown>![Hauptscreen](screenshots/main.png)<figcaption>Hauptscreen</figcaption></figure>
<figure markdown>![Navigation](screenshots/nav.png)<figcaption>Navigation</figcaption></figure>
<figure markdown>![Anstieg](screenshots/climb.png)<figcaption>Anstieg</figcaption></figure>
<figure markdown>![Wege-Labels](screenshots/roadlabels.png)<figcaption>Wege-Labels</figcaption></figure>
<figure markdown>![Einstellungen](screenshots/settings.png)<figcaption>Einstellungen</figcaption></figure>
</div>

Screenshots vom Gerät, aufgenommen während einer simulierten Fahrt von Villach nach Bovec.

## Funktionen

**Anzeige** -- Oberfläche „Rim & Ridge" (LVGL 8, entworfen in EEZ Studio) mit fünf Screens:

- **Hauptscreen**: Geschwindigkeit (Zahl und äußerer Ring), Trittfrequenz, Puls mit
  Zonenband, Temperatur, Höhe, Steigung, Strecke (Fahrt / Trip / Tour / gesamt), Fahrzeit
  oder Uhrzeit, Fahrzustand, Linie für die Wegequalität, Symbole für WLAN, GPS-Fix und Akku
- **Navigation**: öffnet sich vor einem Manöver von selbst und schließt danach wieder.
  Großer Abbiegepfeil, Distanzring, Straßenname, übernächstes Manöver, Fahrspuren,
  Kreisverkehr-Ausfahrten
- **Anstieg**: Höhenprofil des Anstiegs voraus, gefärbt nach Steigung, mit Kategorie;
  öffnet sich von selbst am Fuß eines bewerteten Anstiegs ([Anstiege](CLIMB.md))
- **Wege-Labels**: Untergrund und Qualität während der Fahrt von Hand markieren, als
  Vergleichswert für die automatische Wegequalität
- **Einstellungen**: IP-Adresse, WLAN an/aus, Hotspot, WLAN einrichten (suchen, Netz
  auswählen, Passwort mit der Display-Tastatur tippen), Kalibrierung des
  Beschleunigungssensors und Referenzfahrt, Neustart, Ausschalten

**Fahrten und Statistik**

- Eine Taste startet eine Fahrt, pausiert sie („Cruise") und beendet sie; Stopps und
  Pausen werden aus der Geschwindigkeit erkannt
- Strecke, Zeit, Durchschnitts- und Höchstgeschwindigkeit und mittlere Trittfrequenz je
  Fahrt, Trip, Tour und gesamt; Durchschnitte mit oder ohne Stopps, Pausen und Cruise-Zeit
  ([Fahrten und Statistik](design/ride-state-machine.md))

**Sensoren**

- BLE: Geschwindigkeit und Trittfrequenz (CSC), Puls, Batteriestand jedes Sensors;
  Sensoren werden einmal gekoppelt und an ihre Adresse gebunden
- I²C: BME280 (barometrische Höhe und Steigung, Temperatur), Beschleunigungssensor BMI160
- [Forumslader](https://www.forumslader.de/) (Lader am Nabendynamo) über mein
  [BLE-Gateway](https://github.com/euphi/ESP32_FLClassic2BLE) (Build-Variante `-FL`)

**Navigation und GPS** von der Android-App [TrailBridge](trailbridge/index.md) über BLE
([Protokoll](trailbridge/PROTOCOL.md)):

- Abbiegehinweise von OsmAnd oder von einer GPX-Route, die TrailBridge selbst abspielt --
  dann mit dem Höhenprofil für den Anstiegs-Screen
- Die GPS-Position des Handys für das Log und seine Zeit für die Uhr, wenn kein WLAN da ist
- [`Tools/gpxenrich`](https://github.com/euphi/TRGB-BikeComputer/tree/main/Tools/gpxenrich)
  macht aus einem einfachen GPX-Track eine solche Route (Abbiegehinweise von BRouter)

**Wegequalität** aus dem BMI160 mit 400 Hz: Rauheitsklasse je Intervall, Stoßerkennung
(mit dem zweiten Peak vom Hinterrad), eine Referenzfahrt auf glattem Asphalt und die
Steigung aus dem Beschleunigungssensor als Alternative zum Barometer.

**Aufzeichnung**

- Binärlog auf SD-Karte, eine Sitzung je Boot in datierten Ordnern: Fahrdaten alle 5 s
  mit GPS, Wegequalität je Intervall, jeder Stoß, manuelle Wege-Labels, Fahrzustände;
  Rohdaten des Beschleunigungssensors auf Anforderung
- Debug-Log und rohes Forumslader-Log (auf dem Gerät wieder abspielbar)
- Python-[Werkzeuge](TOOLS.md), die Logs in CSV und GPX umwandeln, und ein
  [Dienst](LOGSERVICE.md), der die Sitzungen abholt, sobald der Fahrradcomputer im WLAN
  auftaucht, sie archiviert, GPX exportiert, mit Nextcloud abgleicht und zu Komoot hochlädt

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
- [Anstiege](CLIMB.md) und [Fahrten und Statistik](design/ride-state-machine.md): wie sich
  das Gerät unterwegs verhält
- [Logformat und Werkzeuge](TOOLS.md), [Log-Dienst](LOGSERVICE.md): was mit den
  aufgezeichneten Fahrten passiert
- [Debugging](DEBUG.md), [Simulator](SIMULATOR.md), [Fallstricke](PITFALLS.md),
  [Design-System](design/rim-ridge-design-system.md): für die Arbeit an der Firmware
- [TrailBridge](trailbridge/index.md): die Android-App und ihr BLE-Protokoll

Quellcode: [github.com/euphi/TRGB-BikeComputer](https://github.com/euphi/TRGB-BikeComputer).
Mitarbeit ist willkommen; die Roadmap zeigt, wo Hilfe gesucht wird.

## Lizenzen und Quellen

- [Cycling icons created by Futuer - Flaticon](https://www.flaticon.com/free-icons/cycling)
- [Parked bicycles - icon by Prashanth Rapolu 15](https://de.freepik.com/icon/fahrrad_7764338#fromView=resource_detail&position=7)
