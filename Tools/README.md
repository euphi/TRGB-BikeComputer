# Tools -- Auswertung der Binärlogs

Der Fahrradcomputer schreibt seine Messdaten als Sequenz fester Binärsätze
auf die SD-Karte (`BCLogger::LogData`, siehe [`../src/BCLogger.h`](../src/BCLogger.h)).
Hier liegt alles, was damit offline weiterarbeitet.

```
bikelog/            Bibliothek + CLI (reine Standardbibliothek, kein venv nötig)
BikeLogService/     Upload-Dienst (FastAPI, braucht venv)
tests/              pytest-Suite für beides
ReadTachoBin.py     Altbekannter CSV-Konverter, jetzt Wrapper um "bikelog csv"
csv2influx.py       Unverändert: CSV nach InfluxDB
```

## CLI

Ohne Installation, direkt aus diesem Verzeichnis:

```bash
python3 -m bikelog info -i /pfad/L0001.bin          # Übersicht über ein Log
python3 -m bikelog csv  -i /pfad/L0001.bin -o out.csv [--gps]
python3 -m bikelog gpx  -i /pfad/L0001.bin -o tour.gpx
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

Jeder Lauf meldet, was verworfen wurde:

```
195/243 Punkte in 2 Segment(en), 20 verworfen (ohne Fix), 25 verworfen (veraltet)
```

### Testdaten ohne Hardware

```bash
python3 -m bikelog fixture synth -o ride.bin --unset-clock 3
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

`tests/test_record.py` pinnt das Binärlayout an die Firmware: schlägt
`test_v1_record_is_56_bytes` fehl, hat sich `LogData` geändert und
`bikelog/record.py` braucht einen neuen `LAYOUTS`-Eintrag.
