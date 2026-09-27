# Tools -- Auswertung der Binärlogs

Der Fahrradcomputer schreibt seine Messdaten als Sequenz fester Binärsätze
auf die SD-Karte (Format v2, siehe [`../src/LogRecords.h`](../src/LogRecords.h)).
Hier liegt alles, was damit offline weiterarbeitet.

| Format | Satzgröße | Inhalt |
|---|---|---|
| v1 | 56 Byte | nur Fahrdaten (alle 5 s) -- ältere Logs, wird weiter gelesen |
| v2 | 64 Byte | Typ-Byte an Offset 30: 0 = Fahrdaten, 1 = Wegequalität (pro Intervall, 1..10 s), 2 = Stoß, 3 = manuelles Wege-Label |

Die Versionsnummer steht in jedem Satz an Offset 31. Unbekannte Satztypen
werden übersprungen (und in `bikelog info` gezählt).

**Manuelle Wege-Labels** (Typ 3, ohne Versionssprung nachgerüstet): Untergrund
(1 Asphalt, 2 Schotter, 3 Waldweg, 4 Feldweg, 5 Pflaster, 6 Sonstiges) und
Qualität 1 (am besten) bis 4, gesetzt auf dem RQ-Ride-Screen oder per
`rq label <untergrund>,<qualität>`. Ein Satz bei jedem Wechsel, alle 60 s zur
Sicherheit wiederholt und bei Start/Stopp eines Rohmitschnitts; das Label gilt
bis zum nächsten Satz. Zusätzlich steht es in jedem Stoß-Satz und jedem
Rohdaten-Block. `bikelog info` stellt die Labels der automatischen Klasse
gegenüber (Strecke, Rauheit-Median und Klassenverteilung je Label). Die Zuordnung
zu OSM `surface`/`smoothness` steht in `bikelog/record.py` (`SURFACE_OSM`,
`LABEL_QUALITY_OSM`).

```
bikelog/            Bibliothek + CLI (reine Standardbibliothek, kein venv nötig)
BikeLogService/     Dienst: holt Sitzungen vom BC ab (mDNS), GPX/CSV (FastAPI, eigene venv)
pyproject.toml      macht Tools/ installierbar (für den Dienst, siehe BikeLogService/install.sh)
tests/              pytest-Suite für beides
ReadTachoBin.py     Altbekannter CSV-Konverter, jetzt Wrapper um "bikelog csv"
csv2influx.py       Unverändert: CSV nach InfluxDB
```

## Dateien auf der SD-Karte

Jeder Boot ist eine Sitzung. Sie schreibt zunächst ins Arbeitsverzeichnis
`/BIKECOMP/CUR/`, benannt nach einer laufenden Nummer -- beim Booten ist die
Uhr meist noch nicht gestellt:

| Datei | Inhalt |
|---|---|
| `L_0042.bin` | Binärlog (dieses Format) |
| `D_0042.log`, `N_0042.log` | Debug-Log, Forumslader-NMEA |
| `S_0042.bin`, `R_0042_NN.bin` | Rohdaten, siehe unten |
| `T_0042.txt` | Zeit-Hinweise: `start <epochMs> <gültig 0/1> <uptimeMs>`, dann je Uhrsprung `step <ntp\|gps\|?> <offsetMs> <neueEpochMs> <uptimeMs>` |

Nach dem nächsten Boot räumt ein Hintergrund-Task (`src/LogSessions.cpp`) jede
ältere Sitzung aus `CUR/` weg:

- Startzeit aus `T_*`; lief die Sitzung ohne Uhr (1970) an, wird sie um den
  Sprung korrigiert, mit dem NTP oder die GPS-Zeit von TrailBridge die Uhr
  gültig gemacht hat.
- Ziel `/BIKECOMP/JJJJMMTT/X_HHMMSS.*`, ohne ermittelbare Zeit
  `/BIKECOMP/NO_TIME/X_0042.*`. Im `L_*.bin` werden dabei die
  1970-Zeitstempel umgeschrieben; Rohdaten und Debug-Log behalten ihre
  Originalzeiten.
- `I_HHMMSS.txt`: Kurzstatistik als `schlüssel=wert`-Zeilen (Strecke, Dauer,
  Fahrzeit, Ø/max. Geschwindigkeit, GPS-Anteil, Anzahl Wegequalitäts-Intervalle,
  Stöße, Labels, Rohmitschnitte; `corr_ms` = angewandte Zeitkorrektur). Die
  Logfile-Seite des Webservers zeigt sie je Sitzung an.
- Leere Dateien werden gelöscht.

Ältere Logs (vor dieser Umstellung) liegen weiter als `L0042.bin` in
`NO_TIME/` bzw. datiert ohne `I_*.txt`.

## CLI

Ohne Installation, direkt aus diesem Verzeichnis:

```bash
python3 -m bikelog info -i /pfad/L0001.bin          # Übersicht über ein Log
python3 -m bikelog csv  -i /pfad/L0001.bin -o out.csv [--gps] [--roadq]
python3 -m bikelog csv  -i /pfad/L0001.bin -o out.csv --roadq-out wege.csv --shocks-out stoesse.csv --labels-out labels.csv
python3 -m bikelog gpx  -i /pfad/L0001.bin -o tour.gpx [--no-shocks] [--min-severity 2]
./ReadTachoBin.py -i /pfad/L0001.bin -o out.csv     # wie bisher
```

### GPX-Export

Das XML ist der einfache Teil; entscheidend ist, was *nicht* hineinkommt:

| Filter | Standard | Option |
|---|---|---|
| Datensätze ohne GPS-Fix | immer raus | -- |
| veraltete Fixes (Heartbeat-Wiederholung im Tunnel) | > 5000 ms raus | `--max-fix-age` |
| Zeitstempel vor 2020 (Uhr nicht per NTP gesetzt) | immer raus | -- |
| Position exakt (0, 0) | immer raus | -- |
| zu ungenaue Fixes | aus | `--max-accuracy` |
| Zeitlücke → neues `<trkseg>` | > 60 s | `--segment-gap` |

Höhenquelle über `--ele=auto|baro|gps`; `auto` nimmt den Barometer, wenn sein
Wert plausibel ist, sonst die GPS-Höhe. Sensordaten (Puls, Trittfrequenz,
Temperatur, Geschwindigkeit) reisen in Garmins `TrackPointExtension` mit --
das Format, das Strava und Komoot tatsächlich lesen. Steigung und
Trip-Distanz haben dort kein Element und bleiben bewusst CSV-exklusiv.

Stöße (v2) werden als `<wpt>` ausgegeben -- Name `Stoß 5,2 g`, `<type>`
`shock-1..3` nach Schwere, in `<desc>` zweiter Peak (Hinterrad) und
Geschwindigkeit. Für sie gelten dieselben Positions- und Zeitfilter wie für
die Trackpunkte. Die Wegequalitäts-Intervalle bleiben CSV-exklusiv
(`--roadq-out`), aus demselben Grund wie die Steigung.

Jeder Lauf meldet, was verworfen wurde:

```
195/243 Punkte in 2 Segment(en), 20 verworfen (ohne Fix), 25 verworfen (veraltet)
```

### Rohdaten des Beschleunigungssensors

Neben dem Log einer Sitzung `L_HHMMSS.bin` können zwei Rohdaten-Dateien liegen
(Format: [`../src/RawCapture.h`](../src/RawCapture.h), 400 Hz, rohe LSB):

| Datei | Entsteht | Größe |
|---|---|---|
| `R_HHMMSS_NN.bin` | auf Anforderung: `rq raw <s>` (Serial), Buttons auf `/debug/imu` oder Aufnahme-Taste des RQ-Ride-Screens, bis 1800 s | ca. 150 KB/min |
| `S_HHMMSS.bin` | immer: 0,25 s vor bis 0,5 s nach jedem geloggten Stoß | ca. 1,8 KB/Stoß |

```bash
python3 -m bikelog raw info   -i R_143012_01.bin
python3 -m bikelog raw csv    -i R_143012_01.bin -o raw.csv          # Frames in g
python3 -m bikelog raw replay -i R_143012_01.bin shock=2.5 interval=1 --rq-out rq.csv
python3 -m bikelog raw replay -i S_143012.bin                        # jeden Stoß nachrechnen
```

`replay` rechnet mit dem **Firmware-Code selbst** (`src/RoadQuality.cpp`),
nicht mit einer Python-Nachbildung: Es baut beim ersten Aufruf
[`rqreplay/rq_replay.cpp`](rqreplay/rq_replay.cpp) mit g++ nach `.build/` und
übergibt die Parameter (`python3 -m bikelog raw replay -h` listet sie). So
lassen sich Schwellen und Filter an echten Aufnahmen durchprobieren, bevor
sie in die Firmware wandern. Das manuelle Label jedes Blocks läuft mit durch:
`raw info` listet die Label-Abschnitte, `raw replay` zeigt Rauheit und Klassen
je Label -- der direkte Vergleich „gefühlt“ gegen „gemessen“.

### Testdaten ohne Hardware

```bash
python3 -m bikelog fixture synth -o ride.bin --unset-clock 3 [--roadq]
python3 -m bikelog fixture from-gpx tour.gpx -o ride.bin
```

`synth` erzeugt absichtlich die unangenehmen Fälle (Start ohne Fix, Tunnel
mit veralteten Fixes, Pause als Zeitlücke, ungesetzte Uhr). `from-gpx` baut
aus einer echten Tour -- z. B. einem Komoot-Export -- ein Binärlog mit
realistischer Geometrie, solange es noch keine echten Aufzeichnungen gibt.

## Tests

```bash
python3 -m venv .venv
.venv/bin/pip install -r BikeLogService/requirements.txt
.venv/bin/python -m pytest
```

`tests/test_logformat.py` pinnt das Binärlayout an die Firmware:
`test_layout_matches_firmware_header` kompiliert `src/LogRecords.h` mit g++
auf dem Host ([`../test/native_roadquality/logrecords_dump.cpp`](../test/native_roadquality/logrecords_dump.cpp)),
lässt je einen Satz pro Typ schreiben und liest ihn zurück. Schlägt er fehl,
haben sich die Firmware-Structs geändert und `bikelog/record.py` braucht
neue Einträge in `FORMATS` (und die Firmware eine neue `FORMAT_VERSION`).
Ohne g++ wird der Test übersprungen.
