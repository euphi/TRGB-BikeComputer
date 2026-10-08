# BikeLog-Dienst

Holt die Sitzungen des Fahrradcomputers automatisch ab, sobald er im WLAN
auftaucht, bewahrt sie unverändert auf und liefert GPX/CSV daraus.

Eine **Sitzung** ist alles, was ein Boot des BC schreibt: `L_` (Binärlog),
`I_` (Kurzstatistik), `D_` (Debug-Log), `S_`/`R_` (IMU-Rohdaten), `T_`
(Zeit-Hinweise), `N_` (Forumslader-NMEA) -- alle mit demselben Stamm
(`_143012`), siehe [Logformat und CLI](TOOLS.md). Die Dateien liegen im Dienst
in derselben Struktur wie auf der SD-Karte:

```
data/sessions/<gerät>/20260920/L_143012.bin     (Dateien direkt in /BIKECOMP: Ordner "_")
data/index.sqlite3                              Index (Sitzungen, Dateien, Hashes, Kennzahlen)
```

Uhr-Sprünge innerhalb einer Sitzung (die GPS-Zeit des Handys war beim Start des BC falsch,
2026-10-04: aus 2 h Fahrt wurden 17 h mit einer Stunde vom Vorabend) werden beim Lesen
rückgängig gemacht: die `T_`-Datei nennt die Sprünge, die Sprünge der Zeitstempel zeigen, wo
sie waren, die letzte Uhr gilt
([`bikelog.timefix`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/timefix.py)).
Die Kurzstatistik zeigt dann `time=repaired`.

GPX, CSV usw. werden bei Bedarf aus den Rohdaten erzeugt -- eine bessere
Exportlogik verbessert damit rückwirkend alle alten Fahrten.

## Abholen (Pull)

Der BC stellt seine SD-Karte ohnehin per HTTP bereit (`/log/<tag>/<datei>`).
Der Dienst holt ab, statt dass die Firmware hochlädt: kein Upload-Client auf
dem Gerät, keine Zugangsdaten dort, keine Wiederholungslogik in einem Task,
der mit BLE und Display um internen Heap konkurriert.

**Wann** ([`puller.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/puller.py)):

- **mDNS-Probes** (Haupt-Trigger): Nach jedem WLAN-Connect ruft die Firmware
  `MDNS.begin("TRGB-BC")` auf, und der ESP32 *probt* dabei seinen Namen (RFC
  6762 8.1: Anfragen nach `TRGB-BC.local` mit Authority-Sektion). Das tut nur
  ein Gerät, das gerade (neu) ins Netz kommt -- also nach Boot oder
  WLAN-Reconnect, genau dann, wenn eine Sitzung fertig geworden ist. Ein
  eigener Socket lauscht passiv darauf; 10 s später wird abgeholt
  (`BOOT_SETTLE_S`). Gemessen 2026-09-27: vier Probes binnen einer Sekunde
  nach dem Connect.
- Der ServiceBrowser allein reicht **nicht**: Er meldet nur Dienste, die er
  noch nicht im Cache hat, und nach einem schnellen Neustart ist der Eintrag
  noch da (PTR-TTL 75 min). So ist der erste Versuch am 2026-09-27
  gescheitert.
- **Polling** als Netz: alle `BIKELOG_PULL_INTERVAL_S` (120 s) eine
  mDNS-Adressanfrage -- ist der BC weg, kostet das nichts, taucht er (wieder)
  auf, wird abgeholt. Zusätzlich spätestens alle 10 min, solange er online ist (der BC
  beendet eine Sitzung, wenn die Fahrt-Session beendet wird, s. u.).
- **Nachfassen**: Direkt nach dem Boot verschiebt `LogSessions` die beendete
  Sitzung erst im Hintergrund aus `CUR/`. Liegt dort beim Abruf mehr als die
  laufende Sitzung (oder schlägt der Abruf fehl), wird nach 60 s erneut
  abgeholt, höchstens 10-mal in Folge.
- Von Hand: `POST /api/v1/pull` oder `bikelogservice pull`.

**Was**: jede Datei der Liste, die noch fehlt oder deren Größe sich geändert
hat -- außer `CUR/` (laufende Sitzung) und gelöschten Sitzungen. Neue
Sitzungen entstehen auf dem BC beim Booten, beim bewussten Beenden der Fahrt (langer Druck)
oder mit `rotate` auf der seriellen Konsole -- nicht beim WLAN-Connect, der auch per
Handy-Hotspot mitten in einer Tour vorkommt. Ein Abruf ohne Neues ist eine einzige Anfrage. Die Liste kommt aus `/logfiles.json`, falls die Firmware den
Endpunkt hat, sonst aus der HTML-Seite `/logfiles/` (dort ohne Größen: eine
bekannte Datei wird dann nicht neu geholt).

Messwert 2026-09-27: Erstabruf 390 Dateien / 23 MB in 3,5 min (≈110 kB/s,
der ESP32 bremst), Folgeabruf 2 s.

**Löschen** entfernt die Dateien, lässt aber einen Grabstein im Index -- sonst
käme die Sitzung beim nächsten Abruf von der SD-Karte zurück. Ein
ausdrücklicher `PUT` holt sie zurück.

## GPX-Export

Zu jeder Sitzung mit Binärlog schreibt der Dienst automatisch eine Datei
unter `data/export/gpx/` (lokale Startzeit + Gerät; mtime = Start der
Fahrt) -- als Übergabestelle für Nextcloud-Sync, Komoot/Strava usw.
([`exporter.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/exporter.py)):

- `Tours/` -- echte Fahrten, mindestens `MIN_EXPORT_DISTANCE_M` (1 km)
  reale Bewegung (GPS-Sprünge rund um einen Standpunkt zählen nicht mit,
  siehe `bikelog.gpx.GpxStats.real_distance_m`).
- `Debug_Archive/` -- alles andere mit verwertbarem GPS-Fix: kurze
  Testfahrten, ein am Rollentrainer stehendes Rad mit jitterndem
  Telefon-GPS, und jede [Testfahrt](#testfahrten) unabhängig von der Länge
  (`gpx_status` `test`). Nichts geht verloren, es landet nur nicht in
  Nextcloud/Strava/Komoot.

Sitzungen ganz ohne verwertbaren GPS-Fix bekommen gar keine Datei
(`gpx_status` `no-gps`), unlesbare Logs `error: …`.

Neu geschrieben (und ggf. zwischen den beiden Ordnern verschoben) wird, wenn
`L_` oder `I_` der Sitzung ankommt oder sich ändert, und für **alle**
Sitzungen, wenn `EXPORT_VERSION` erhöht wird -- das bei jeder inhaltlichen
Änderung am GPX tun, dann bekommen auch alte Fahrten die bessere Fassung.

Vor dem Export läuft [`bikelog/sanitize.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/sanitize.py) über
die Rohdaten (per Default an, `GpxOptions.sanitize` bzw. `?sanitize=false`
am Download): der GPS-Fix wandert im Stand (Ampel, Pause, vor der ersten
Umdrehung) ein paar Meter um den Standpunkt -- sanitize erkennt das am
Radsensor (`speed < MOVING_KMH`) und lässt von jedem Stand nur den einen Fix
direkt an der Pause übrig, statt eines Gekritzels auf der Karte.

Inhalt (Details in [`bikelog/gpx.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/gpx.py)):

| Wo | Was |
|---|---|
| `<metadata>` | Name, Beschreibung mit Kennzahlen (Strecke, Fahrzeit, Ø/max., Höhenmeter, Puls, Trittfrequenz, Temperatur, Stöße, Wegequalität, Labels), Link zur Sitzung (`BIKELOG_PUBLIC_URL`), Bounds |
| `<metadata><extensions><bc:Ride>` | dieselben Kennzahlen maschinenlesbar, Strecke je Wegeklasse und je Label, dazu die `I_`-Statistik des Geräts unverändert (`bc:deviceSummary`) |
| Trackpunkt, Garmin TPX v2 | `atemp`, `hr`, `cad`, `speed`, `course` -- das lesen Strava, Komoot, Garmin Connect |
| Trackpunkt, `bc:TrackPoint` | Trip-Distanz, Radgeschwindigkeit, Steigung (Anzeige/Baro/IMU), Baro- und GPS-Höhe, GPS-Genauigkeit und Fix-Alter, Wegeklasse + OSM-`smoothness`, Rauheit und RMS des Wegequalitäts-Intervalls, manuelles Label |
| `<wpt>` | Stöße (`sym` „Danger Area“, Details in `bc:Shock`) und Label-Wechsel (`sym` „Flag, Blue“) |

`bc` = `https://github.com/euphi/TRGB-BikeComputer/gpx/v1`. Viewer
überspringen unbekannte Erweiterungen; das GPX validiert gegen `gpx.xsd`,
die TPX-Elemente gegen `TrackPointExtensionv2.xsd`. Schlank ohne `bc:`:
`GET /api/v1/sessions/{id}.gpx?rich=false` bzw. `bikelog gpx --plain`.

Von Hand: `bikelogservice export` (fehlende/veraltete), `export --all` (alle).

## Komoot-Upload

Nie automatisch -- aber die Webseite **fragt**: Jede fertige, echte Fahrt, die weder
hochgeladen noch abgelehnt ist, erscheint oben in einem Kasten „Neue Fahrt bereit -- zu
Komoot hochladen?“ mit den Knöpfen *Zu Komoot hochladen* und *Nicht hochladen*. Die Frage
kommt bei jedem Aufruf wieder, bis einer der beiden gedrückt wird (Seite schließen
beantwortet nichts); die Ablehnung wird gespeichert (`komoot_prompt = ignored`, je
zusammengefasster Tour) und lässt sich mit
`POST /api/v1/sessions/{id}/komoot/ignore?ask_again=true` zurücknehmen. Ein fehlgeschlagener
Upload fragt weiter. Sitzungen, die bei der Einführung (Schema 7) schon da waren, werden
nicht gefragt; ihr „Komoot“-Knopf in der Tabelle bleibt.

Solange Dateien geholt werden, zeigt die Seite einen Fortschrittskasten (Datei n von m,
MB bisher, danach „Verarbeite“: GPX-Export, Nextcloud) und lädt sich alle 3 s neu; das
Journal nennt jede Datei, die Zahl der zu holenden Sitzungen und jeden GPX-Export.

`POST /api/v1/sessions/{id}/komoot` lädt die Fahrt über
[kompy](https://github.com/Tsadoq/kompy) zu Komoot hoch (oder, in der
Web-Oberfläche, der „Komoot“-Knopf neben jeder echten Fahrt). Ein Reboot
mitten in einer Fahrt (kurzer BLE-Aussetzer, Pause mit ausgeschaltetem BC)
erzeugt mehrere Sitzungen für eine Tour -- Sitzungen desselben Geräts mit
höchstens `BIKELOG_KOMOOT_MERGE_GAP_S` Pause dazwischen werden deshalb zu
einer Fahrt zusammengefasst und als ein GPX hochgeladen
([`komoot.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/komoot.py)); alle beteiligten Sitzungen zeigen
danach denselben `komoot_status`. Ein zweiter Versuch ohne `?force=true`
wird mit `409` abgelehnt.

Braucht `pip install "bikelog[komoot]"` (kompy + gpxpy, nicht Teil von
`[service]`) und `BIKELOG_KOMOOT_EMAIL`/`BIKELOG_KOMOOT_PASSWORD` in
`bikelog.env` -- ohne beides antwortet der Endpunkt mit `409`
("not-configured"), sonst bleibt alles wie gehabt. kompy meldet nach dem
Upload keine Tour-ID zurück (Komoot-API-Limitation), daher kein direkter
Link auf die neue Tour -- nur der Status (`uploaded`/`error`).

Das hochgeladene GPX hat -- anders als der normale Export -- keine
`<wpt>`-Wegpunkte (Stöße, Label-Wechsel): Komoots Import-Endpunkt lehnt jede
Datei mit `<wpt>` ab (`400 query is required for type=tour_planned`, gegen
die echte API am 2026-09-27 verifiziert -- vermutlich hält deren Parser das
allein deswegen für eine geplante Route statt einer Aufzeichnung). Die
reichen `bc:`-Erweiterungen je Trackpunkt sind davon nicht betroffen und
bleiben drin.

## Nextcloud-Sync

Anders als Komoot: läuft automatisch, kein Knopf, kein Endpunkt. Jede
Sitzung mit `gpx_status` `ok` (also in `Tours/`, siehe oben) wird per WebDAV
in ein konfigurierbares Verzeichnis auf einer Nextcloud-Instanz hochgeladen
([`nextcloud.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/nextcloud.py)); wird eine Sitzung nachträglich
als nicht mehr „echt“ eingestuft (Export-Logik verbessert, `EXPORT_VERSION`
erhöht), wird ihre Nextcloud-Kopie wieder gelöscht statt verwaist
liegenzubleiben. `Debug_Archive`-Sitzungen werden nie synchronisiert.

Braucht `pip install "bikelog[nextcloud]"` (`requests`, nicht Teil von
`[service]`) und in `bikelog.env`:

```
BIKELOG_NEXTCLOUD_URL=https://cloud.example.com
BIKELOG_NEXTCLOUD_USER=bikelog
BIKELOG_NEXTCLOUD_PASSWORD=<App-Passwort>
BIKELOG_NEXTCLOUD_DIR=BikeLog          # Standard, frei wählbar
```

Das Passwort ist ein **App-Passwort** (Nextcloud: Einstellungen -> Sicherheit
-> „Geräte & Sitzungen“ -> neues App-Passwort anlegen), nicht das normale
Login-Passwort -- eigenständig widerrufbar, ohne den Hauptzugang zu berühren.
Ohne URL/Nutzer/Passwort bleibt der Sync einfach aus, sonst wie gehabt.

Bekannte Lücke: Wird eine Sitzung gelöscht (`DELETE /api/v1/sessions/{id}`),
verschwindet nur die lokale Kopie -- die Nextcloud-Kopie bleibt liegen und
muss von Hand entfernt werden.

## Testfahrten

Sitzungen mit emulierten Daten werden erkannt und von den Fahrten getrennt
([`bikelog/testride.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/testride.py)):

| Markierung | Erkannt an |
|---|---|
| **Simuliert** | `LOG_SIMULATED` im Log: der Sensor-Simulator (`sim` auf der seriellen Konsole, Simulator-Build) oder eine TrailBridge-Testfahrt (`SIM_FLAGS`, Firmware seit 2.10.2026) |
| **GPS-Wiedergabe?** | ältere TrailBridge-Testfahrten ohne Flag: GPS legt mindestens 500 m zurück, der Radsensor weniger als 15 % davon. Eine echte Fahrt mit ausgefallenem Radsensor sieht genauso aus, daher das Fragezeichen |

Testfahrten sind in der Fahrtenliste (und in `GET /api/v1/sessions`) **ausgeblendet**, der
Filter sagt, wie viele; „Testfahrten zeigen" blendet sie mit ihrer Markierung ein. Sie zählen
nicht im Training, bei den Anstiegen und Zielen, landen in `Debug_Archive/` und werden nie
synchronisiert oder für Komoot angeboten. Die Fahrtseite sagt, warum eine Sitzung als Test
gilt, und hat einen Knopf, um die Erkennung in beide Richtungen zu überstimmen („Doch eine
echte Fahrt", „Als Testfahrt markieren"; API: `POST /api/v1/sessions/{id}/test?mark=test|real|auto`).

## Leerlauf-Sitzungen und Archiv

Jeder Start des Fahrradcomputers ist eine Sitzung, auch wenn er nur zu Hause an war. Eine
Sitzung, in der das Rad stand und die Position nirgendwohin kam (Rad < 50 m, alle GPS-Fixe
innerhalb von 300 m), oder die gar kein oder nur ein leeres Binärlog hat (nur Nullbytes),
ist **Leerlauf**
([`bikelog/testride.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/testride.py)).
Ein Log in unbekanntem Format bleibt dagegen mit „Log unlesbar" in der Liste (der Hover-Text
nennt das gefundene Versions-Byte) und wird für den Bericht nicht erneut versucht.
Leerlauf-Sitzungen erscheinen nie in der Fahrtenliste (sie verweist darauf: „N
Leerlauf-Sitzungen im Archiv") oder in `GET /api/v1/sessions` (`idle=true` schließt sie ein),
bekommen kein GPX und zählen nirgends.

`BIKELOG_IDLE_ARCHIVE_DAYS` (Standard 7) Tage nach dem Abholen wandern ihre Dateien von
`<data>/sessions/` nach `<data>/archive/` (gleicher Baum) -- Debug-Logs bleiben mit der Shell
erreichbar
([`archive.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/archive.py)).
Der Index behält einen Grabstein, solange die Dateien noch auf der SD-Karte liegen, sonst
holt der nächste Abruf sie wieder; der Abruf entfernt ihn, sobald sie aus der Dateiliste der
Karte verschwunden sind (das gilt auch für gelöschte Sitzungen).

Die Seite **Archiv** (`/archive`) zeigt die noch nicht archivierten Leerlauf-Sitzungen und
die archivierten, die noch auf der Karte liegen. „Auf dem BC löschen" (eine Sitzung oder alle)
löscht ihre Dateien sofort auf dem Fahrradcomputer über den `/del/`-Endpunkt der Firmware --
nur solange er erreichbar ist, es wird nichts vorgemerkt. Eine Sitzung, deren Dateien alle von
der Karte sind, wandert ins Archiv und verschwindet aus dem Index. API:
`POST /api/v1/archive/device-delete` (`session_id`, ohne = alle; 409, wenn das Gerät nicht
erreichbar ist).

## Räder

Die Seite **Räder** (`/bikes`) führt die Räder -- Name, Typ, Gewicht fahrbereit, CdA, Crr --
und welcher Fahrradcomputer ab welchem Tag an welchem Rad fährt
([`bikelog/bikes.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/bikes.py),
gespeichert in `<data>/bikes.json`). Eine Sitzung bekommt das Rad, dem ihr Gerät am Fahrtag
zugeordnet war; auf der Fahrtseite kann eine einzelne Fahrt ein anderes Rad bekommen. Die
Leistungsschätzung des Berichts nimmt dann Körpergewicht (Seite **Fahrer**) + Radgewicht und
CdA/Crr des Rads; Fahrten ohne Rad behalten die Standardwerte der Fahrer-Seite. Eine Änderung an
Rad oder Zuordnung berechnet die betroffenen Berichte neu. Die Seite zeigt die Kilometer je Rad.

## Fahrten importieren (GPX)

„GPX-Fahrten importieren" in der Fahrtenliste nimmt anderswo aufgezeichnete Fahrten (Garmin,
Strava, Komoot …): GPX mit Zeiten; Puls, Trittfrequenz und Temperatur aus Garmins
TrackPointExtension werden übernommen, die Steigung über ±50 m geglättet
([`bikelog/gpximport.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/gpximport.py)).
Eine importierte Fahrt ist eine Sitzung des Geräts `import` (das daraus gebaute Log neben der
Original-GPX) und zählt wie jede andere Fahrt -- Bericht, Training, Anstiege --, wird aber weder
exportiert noch hochgeladen. Optional mit Rad und als **frühere Teilnahme an einem Ziel**: Das
Ziel zeigt dann „Deine bisherigen Teilnahmen" mit Zeit, Tempo, Puls, Leistung und TRIMP. Dieselbe
Datei noch einmal ersetzt die Sitzung. API: `PUT /api/v1/import/gpx` (`bike_id`, `event_id`,
Body = die Datei).

## Sitzungsbericht

`/ride/{id}` (Klick auf eine Fahrt in der Liste) zeigt den Bericht einer Sitzung: Kennzahlen,
Höhenprofil mit den Anstiegen, Pulszonen, geschätzte Leistung, Wegequalität und Stöße,
technische Auffälligkeiten. `GET /api/v1/sessions/{id}/report.md` ist dasselbe als Markdown,
`…/report.json` die Zahlen dahinter. Alles kommt aus
[`bikelog/report.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/report.py)
(Details: [Werkzeuge](TOOLS.md#sitzungsbericht)).

Die Berichte liegen in der Tabelle `reports` des Index, unter einem Schlüssel aus
Berichtsversion, Hash der `L_`-Datei und Fahrerdaten. Nach dem Abholen (und beim Start)
berechnet der Dienst, was fehlt oder veraltet ist, mit niedrigster CPU-Priorität
([`analysis.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/analysis.py)).
Eine neue Berichtsversion oder geänderte Fahrerdaten erreichen so von selbst alle alten Fahrten.

Fahrerdaten für Pulszonen, TRIMP und W/kg werden auf der Seite **Fahrer** (`/athlete`)
eingetragen und landen in `<data>/athlete.json` (anderer Pfad: `BIKELOG_ATHLETE_FILE`).
Jeder Schlüssel ist optional; ohne die Datei hat der Bericht einfach keine Zonen:

```json
{"hr_max": 186, "hr_rest": 48, "mass_kg": 88, "rider_kg": 76, "cda": 0.38, "crr": 0.006}
```

Das JSON ist als Eingabe für einen Textgenerator gedacht (Vorlage oder lokales LLM): Alle
Zahlen werden dort berechnet, ein Modell muss sie nur noch in Worte fassen -- siehe
[die Fahrt in Worten](#die-fahrt-in-worten-lokales-llm).

## Die Fahrt in Worten (lokales LLM)

Mit `BIKELOG_LLM_URL` (ein [Ollama](https://ollama.com)-Server, z. B.
`http://localhost:11434`) bekommt die Fahrtseite einen Abschnitt **In Worten**: zwei, drei
Absätze dazu, was für eine Fahrt das war, wie Belastung und Puls zu den letzten Fahrten
stehen, ob das zur Trainingsphase des nächsten Ziels passt, und technische Warnungen. Alles
bleibt auf dem Heimserver.

- Das Modell fasst nur Zahlen in Worte. Der Prompt
  ([`bikelog/narrate.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/narrate.py)) gibt ihm beschriftete Zeilen mit
  ausgeschriebenen Einheiten und ihrer Bedeutung („Höhenmeter bergauf: 480 m“, „Form (TSB):
  -12 (negativ = ermüdet)“), dazu den Vergleich mit den Fahrten der 42 Tage davor, die
  Belastung am Morgen der Fahrt und das nächste Ziel mit seiner Phase -- keine Abkürzungen,
  nichts zu rechnen. (Im ersten Test las qwen3:8b „480 hm“ als Hektometer und zählte sie zur
  Strecke.)
- Jede Zahl der Antwort, die nicht im Prompt steht (Rundung erlaubt), steht unter dem Text:
  kein Beweis für einen Fehler, ein Grund hinzusehen.
- Das Denken ist abgeschaltet (`"think": false`): qwen3 überlegt sonst erst mehrere hundert
  Tokens lang -- auf der CPU Minuten.
- Die Texte entstehen im Hintergrund nach den Berichten, neueste Fahrt zuerst, einer nach dem
  anderen ([`llm.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/llm.py)), und liegen in der Tabelle
  `narratives`. Eine Fahrt ohne Text bekommt immer einen; ändern sich ihre Fakten (neues Ziel,
  anderes Rad, neuer Prompt), wird er nur für Fahrten der letzten `BIKELOG_LLM_REFRESH_DAYS`
  neu geschrieben. **Neu schreiben** auf der Seite schreibt ihn neu, vor allen anderen.
  Testfahrten und Fahrten unter 1 km bekommen keinen.

Auf dem Orange Pi 5 (16 GB, nur CPU) schreibt `qwen3:8b` etwa 2,7 Tokens/s, ein Text dauert
2--4 Minuten. Ein 14B-Modell braucht beim Laden etwa 9 GB (Ollama packt die Gewichte für die
ARM-CPU um) -- mit anderen Diensten auf der Maschine endet das beim OOM-Killer. Empfohlene
Ollama-Einstellungen (`sudo systemctl edit ollama`):

```ini
[Service]
Nice=10
OOMScoreAdjust=500
Environment=OLLAMA_NUM_PARALLEL=1
Environment=OLLAMA_CONTEXT_LENGTH=4096
Environment=OLLAMA_FLASH_ATTENTION=1
Environment=OLLAMA_KV_CACHE_TYPE=q8_0
```

## Training, Ziele, Anstiege

Die Webseiten sind im Rim-&-Ridge-Design des Fahrradcomputers gestaltet
([Design-System](design/rim-ridge-design-system.md)). Neben der Fahrtenliste und dem
Fahrtbericht gibt es:

| Seite | Inhalt |
|---|---|
| **Training** (`/training`) | Fitness (CTL, 42-Tage-Mittel des TRIMP), Ermüdung (ATL, 7 Tage) und Form (TSB) im Verlauf; Kilometer und Stunden je Pulszone pro Woche; Wochentabelle. Braucht `hr_max`, Fahrten ohne Puls zählen als 0 |
| **Ziele** (`/goals`) | Zielrennen mit Datum und Priorität (A/B/C), Countdown und Trainingsphase (Grundlage, Aufbau, Spitze, Tapering); die Strecke als GPX hochladen (Knopf im Ziel, oder gleich im Formular für ein neues) für Distanz, Höhenmeter und Anstiege. Verglichen mit den letzten 6 Wochen: längste Fahrt gegen die Renndistanz, meiste Wochenhöhenmeter gegen die des Rennens; für jeden Anstieg der Strecke eine geschätzte Zeit aus der besten VAM auf vergleichbaren Anstiegen der letzten 90 Tage |
| **Anstiege** (`/climbs`) | mehrmals gefahrene Anstiege (Fuß und Gipfel höchstens 150 m auseinander, braucht GPS), jede Fahrt mit Zeit, Abstand zur besten, VAM, Puls, Leistung |
| **Fahrer** (`/athlete`) | Fahrerdaten, siehe oben |

Code: [`bikelog/training.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/training.py)
(rein, arbeitet nur auf dem Bericht-JSON, Tests `tests/test_training.py`), Seiten in
[`webui.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/webui.py)
und [`charts.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/charts.py)
(SVG auf dem Server, keine JS-Bibliothek), Tests `tests/test_pages.py`. Die Zielrennen liegen in
`<data>/events.json`, ihre GPX-Dateien in `<data>/events/`.

### Eine Fahrt, mehrere Sitzungen

Startet der Fahrradcomputer unterwegs neu, wird aus einer Fahrt mehrere Sitzungen.
Sitzungen desselben Geräts, deren Abstand (Ende der einen bis Start der nächsten) höchstens
`BIKELOG_KOMOOT_MERGE_GAP_S` (30 min) beträgt, sind eine Fahrt -- dieselbe Gruppierung wie
für den Komoot-Upload
([`tours.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/tours.py)).
Training, Ziele und Anstiege zählen die Fahrt, ihr Bericht läuft über die verbundenen Daten
(die Strecke läuft weiter, der Neustart ist ein Stopp). Die Fahrtenliste zeigt eine Fahrt aus
mehreren Teilen als eine Zeile mit den Summen der ganzen Fahrt (Strecke, Fahrzeit, Ø) und den Teilen
kleiner darunter („Teil 1/2", ohne Trennlinie dazwischen); die Sitzungsseite verlinkt den Bericht der ganzen Fahrt (`/tour/{id}`). Das
hängt an den Uhrzeiten: Nach einem Reset ohne Uhr übernimmt die Firmware die GPS-Zeit von
TrailBridge und korrigiert die vorher geschriebenen Zeitstempel; eine Sitzung, die nie eine
Uhrzeit bekam (kein WLAN, kein TrailBridge), bleibt für sich. Test- und Leerlauf-Sitzungen
gehören nie zu einer Fahrt.

Die Seiten laden die Schriften von Google Fonts; ohne Internet nimmt der Browser
Systemschriften.

## Installation (Heimserver: `~/bikelog`)

```bash
Tools/BikeLogService/install.sh            # oder: install.sh /anderer/pfad
```

legt an: `~/bikelog/venv` (eigene venv, der Code wird hineinkopiert --
Branch-Wechsel im Repo berühren den Dienst nicht), `data/`, `bikelog.env`
(Konfiguration, wird nie überschrieben) und `bikelog.service`. Einmalig als
root:

```bash
sudo install -m 644 ~/bikelog/bikelog.service /etc/systemd/system/
sudo systemctl daemon-reload && sudo systemctl enable --now bikelog
journalctl -u bikelog -f
```

System-Unit mit `User=` statt User-Unit: braucht keine Login-Sitzung und
kein `enable-linger`.

**Update**: nach `git pull` wieder `install.sh`, dann `sudo systemctl restart bikelog`.

Oberfläche: `http://<server>:8080/` (Fahrtenliste), API-Doku `http://<server>:8080/docs`.

## Entwicklung

```bash
cd Tools
python3 -m venv .venv
.venv/bin/pip install -r BikeLogService/requirements.txt
.venv/bin/python -m pytest
PYTHONPATH=.:BikeLogService .venv/bin/python -m bikelogservice serve --port 8081
```

## Kommandozeile

```bash
bikelogservice serve [--host 0.0.0.0] [--port 8080]
bikelogservice pull  [TRGB-BC.local | 192.168.0.171] [--device trgb]   # einmal abholen
bikelogservice import /media/sd/BIKECOMP [--device trgb]              # SD-Karte im Kartenleser
bikelogservice export [--all]                                         # GPX-Dateien schreiben
```

`pull` und `import` arbeiten auf demselben Datenverzeichnis
(`BIKELOG_DATA_DIR`) und dürfen parallel zum Dienst laufen.

## Konfiguration

Umgebungsvariablen (im Dienst: `~/bikelog/bikelog.env`):

| Variable | Default | Bedeutung |
|---|---|---|
| `BIKELOG_DATA_DIR` | `$STATE_DIRECTORY` bzw. `~/.local/state/bikelog` | Sitzungen + SQLite-Index |
| `BIKELOG_PORT` | -- | nur in der Unit: Port für `serve` |
| `BIKELOG_PULL` | `0` | Abholen an/aus |
| `BIKELOG_PULL_TARGETS` | `trgb=TRGB-BC` | `gerät=mdns-name`, kommagetrennt |
| `BIKELOG_PULL_INTERVAL_S` | `120` | Polling-Intervall |
| `BIKELOG_PULL_MDNS` | `1` | auf mDNS-Ankündigungen hören |
| `BIKELOG_EXPORT_GPX` | `1` | GPX-Export an/aus |
| `BIKELOG_EXPORT_DIR` | `<data>/export/gpx` | Zielverzeichnis (muss in der Unit beschreibbar sein: `ReadWritePaths=`) |
| `BIKELOG_PUBLIC_URL` | -- | z. B. `http://server:8080`, für den Link im GPX |
| `BIKELOG_REQUIRE_AUTH` | `0` | Auth an/aus |
| `BIKELOG_TOKENS` | -- | `token:name`, kommagetrennt |
| `BIKELOG_MAX_UPLOAD_BYTES` | 64 MiB | Obergrenze pro `PUT` |
| `BIKELOG_KOMOOT_EMAIL` / `BIKELOG_KOMOOT_PASSWORD` | -- | Komoot-Login; ohne beide ist der Upload aus |
| `BIKELOG_KOMOOT_ACTIVITY` | `touringbicycle` | eine `SupportedActivities`-Konstante aus kompy |
| `BIKELOG_KOMOOT_STATUS` | `friends` | Sichtbarkeit der hochgeladenen Tour (`public`/`private`/`friends`) |
| `BIKELOG_KOMOOT_MERGE_GAP_S` | `1800` | Sitzungen desselben Geräts mit höchstens so viel Pause dazwischen gelten als eine unterbrochene Fahrt |
| `BIKELOG_NEXTCLOUD_URL` / `_USER` / `_PASSWORD` | -- | Nextcloud-Login (App-Passwort); ohne alle drei ist der Sync aus |
| `BIKELOG_NEXTCLOUD_DIR` | `BikeLog` | Zielverzeichnis (WebDAV-Pfad, wird bei Bedarf angelegt) |
| `BIKELOG_ATHLETE_FILE` | `<data>/athlete.json` | Fahrerdaten für den Sitzungsbericht |
| `BIKELOG_IDLE_ARCHIVE_DAYS` | `7` | Leerlauf-Sitzungen nach so vielen Tagen nach `archive/`, 0 = nie |
| `BIKELOG_LLM_URL` | -- | Ollama-Server für die Fahrt in Worten; leer = aus |
| `BIKELOG_LLM_MODEL` | `qwen3:8b` | Ollama-Modell |
| `BIKELOG_LLM_TIMEOUT_S` | `1800` | längste Wartezeit auf eine Antwort |
| `BIKELOG_LLM_REFRESH_DAYS` | `14` | Texte mit geänderten Fakten werden nur für Fahrten so vieler Tage neu geschrieben |

Mehrere Fahrradcomputer: Jede Build-Variante hat einen eigenen Netzwerknamen (mDNS-Host und
Standard-Hotspot-SSID, `BC_HOSTNAME` in `platformio.ini`): der Gravel-Build `TRGB-BC`, der
Forumslader-Build `TRGB-FL`; änderbar auf der Seite `/wifi` des Fahrradcomputers
([WLAN](WIFI.md#geratename)). Ein Abhol-Ziel je Fahrradcomputer, z. B.
`BIKELOG_PULL_TARGETS=trgb=TRGB-BC,pendler=TRGB-FL`; der Teil vor `=` ist der Gerätename, unter
dem die Sitzungen abgelegt und dem Räder zugeordnet werden (siehe [Räder](#rader)). Den Namen
eines bestehenden Ziels beibehalten, die gespeicherten Sitzungen liegen darunter.

## API (v1)

| Methode | Pfad | Zweck |
|---|---|---|
| `GET` | `/` | Fahrtenliste (HTML) |
| `GET` | `/api/v1/health` | Erreichbarkeit + Anzahl Sitzungen |
| `GET` | `/api/v1/sessions` | Liste (`limit`, `offset`, `tests`: Testfahrten, `idle`: Leerlauf-Sitzungen einschließen), neueste zuerst |
| `GET` | `/api/v1/sessions/{id}` | Sitzung mit Dateien, Kennzahlen, `I_`-Statistik |
| `GET` | `/api/v1/sessions/{id}.gpx` | GPX wie im Export (`max_fix_age_ms`, `segment_gap_s`, `ele`, `max_accuracy_m`, `shocks`, `labels`, `rich`) |
| `GET` | `/api/v1/sessions/{id}.csv` | CSV (`with_gps`) |
| `GET` | `/api/v1/sessions/{id}/files/{name}` | eine Datei unverändert |
| `GET` | `/api/v1/sessions/{id}/report.json` | Kennzahlen der Sitzung, siehe [Sitzungsbericht](#sitzungsbericht) |
| `GET` | `/api/v1/sessions/{id}/report.md` | dasselbe als Markdown-Bericht |
| `POST` | `/api/v1/sessions/{id}/test` | Test-Erkennung überstimmen (`mark=test|real|auto`) |
| `POST` | `/api/v1/archive/device-delete` | Leerlauf-/archivierte Sitzungen sofort auf dem BC löschen (`session_id`) |
| `PUT` | `/api/v1/import/gpx` | aufgezeichnete Fahrt importieren (`bike_id`, `event_id`; Body = GPX) |
| `GET` | `/api/v1/training` | Tageslast (TRIMP, CTL, ATL, TSB), Wochen, wiederkehrende Anstiege (`days`, `weeks`) |
| `GET` | `/api/v1/events` | Zielrennen mit Stand der Vorbereitung |
| `PUT` | `/api/v1/events/{id}/gpx` | Strecken-GPX eines Zielrennens (Body = Datei) |
| `DELETE` | `/api/v1/sessions/{id}` | Dateien löschen, Grabstein behalten |
| `POST` | `/api/v1/sessions/{id}/komoot` | zur zusammengehörigen Tour hochladen (`force`), siehe unten |
| `POST` | `/api/v1/sessions/{id}/komoot/ignore` | die Frage der Seite mit „nein“ beantworten (`ask_again=true`: wieder fragen) |
| `PUT` | `/api/v1/devices/{gerät}/files/{tag}/{name}` | eine Datei einliefern (Body = Datei) |
| `GET`/`POST` | `/api/v1/pull` | Status des Abholens / jetzt abholen (`device`) |

`PUT` ist idempotent: dieselben Bytes noch einmal ergeben `unchanged`, andere
Bytes unter demselben Namen `replaced` (der BC schreibt `I_`-Dateien neu,
wenn sich ihr Format ändert). Ein `L_`, das sich nicht lesen lässt, wird
trotzdem aufbewahrt und in `log_error` begründet -- es sind die Daten des
Geräts. `CUR/` wird mit `409` abgelehnt.

```bash
curl -T L_143012.bin http://server:8080/api/v1/devices/trgb/files/20260920/L_143012.bin
```

## Auth nachrüsten

Auth ist aus, aber vollständig verdrahtet. **Jede** Route hängt schon heute
an der Dependency `require_principal` ([`auth.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/auth.py)),
die aktuell einen anonymen Principal zurückgibt. Einschalten:

```
BIKELOG_REQUIRE_AUTH=1
BIKELOG_TOKENS=<token>:gravel
```

Keine Routenänderung, keine Migration; Clients schicken
`Authorization: Bearer <token>`. Die HTML-Seite ist dann im Browser nicht
mehr ohne Weiteres erreichbar -- für den Heimserver ohne Portfreigabe
bleibt Auth deshalb aus. Der Test
`test_every_route_requires_a_token_once_auth_is_on` hält fest, dass keine
Route an der Dependency vorbeigeht.
