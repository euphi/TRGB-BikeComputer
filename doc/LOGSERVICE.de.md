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
  auf, wird abgeholt. Zusätzlich spätestens alle 30 min, solange er online ist.
- **Nachfassen**: Direkt nach dem Boot verschiebt `LogSessions` die beendete
  Sitzung erst im Hintergrund aus `CUR/`. Liegt dort beim Abruf mehr als die
  laufende Sitzung (oder schlägt der Abruf fehl), wird nach 60 s erneut
  abgeholt, höchstens 10-mal in Folge.
- Von Hand: `POST /api/v1/pull` oder `bikelogservice pull`.

**Was**: jede Datei der Liste, die noch fehlt oder deren Größe sich geändert
hat -- außer `CUR/` (laufende Sitzung) und gelöschten Sitzungen. Neue
Sitzungen entstehen auf dem BC nur beim Booten, ein Abruf ohne Neues ist eine
einzige Anfrage. Die Liste kommt aus `/logfiles.json`, falls die Firmware den
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
  Telefon-GPS. Nichts geht verloren, es landet nur nicht in Nextcloud/Strava.

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

Ausschließlich manuell -- kein automatischer Trigger, anders als der
Nextcloud-Sync unten. `POST /api/v1/sessions/{id}/komoot` lädt die Fahrt über
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

## Sitzungsbericht

`GET /api/v1/sessions/{id}/report.md` (Link „Bericht" auf der Webseite) ist ein Bericht der
Sitzung auf Deutsch, `…/report.json` die Zahlen dahinter. Beides kommt aus
[`bikelog/report.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/report.py)
(Details: [Werkzeuge](TOOLS.md#sitzungsbericht)) und wird bei jedem Abruf neu berechnet; ein
besserer Bericht verbessert also auch alle alten Fahrten.

Fahrerdaten für Pulszonen, TRIMP und W/kg stehen in `<data>/athlete.json` (anderer Pfad:
`BIKELOG_ATHLETE_FILE`). Jeder Schlüssel ist optional; ohne die Datei hat der Bericht
einfach keine Zonen:

```json
{"hr_max": 186, "hr_rest": 48, "mass_kg": 88, "rider_kg": 76, "cda": 0.38, "crr": 0.006}
```

Das JSON ist als Eingabe für einen Textgenerator gedacht (Vorlage oder lokales LLM): Alle
Zahlen werden dort berechnet, ein Modell muss sie nur noch in Worte fassen.

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

Beide BC-Varianten (Gravel und FL) melden sich heute als `TRGB-BC` -- der
Dienst kann sie nicht auseinanderhalten und legt alles unter einem Gerät ab.
Abhilfe, sobald beide im selben Netz sind: eigener mDNS-Name je Variante oder
ein `device`-Feld in `/logfiles.json`.

## API (v1)

| Methode | Pfad | Zweck |
|---|---|---|
| `GET` | `/` | Fahrtenliste (HTML) |
| `GET` | `/api/v1/health` | Erreichbarkeit + Anzahl Sitzungen |
| `GET` | `/api/v1/sessions` | Liste (`limit`, `offset`), neueste zuerst |
| `GET` | `/api/v1/sessions/{id}` | Sitzung mit Dateien, Kennzahlen, `I_`-Statistik |
| `GET` | `/api/v1/sessions/{id}.gpx` | GPX wie im Export (`max_fix_age_ms`, `segment_gap_s`, `ele`, `max_accuracy_m`, `shocks`, `labels`, `rich`) |
| `GET` | `/api/v1/sessions/{id}.csv` | CSV (`with_gps`) |
| `GET` | `/api/v1/sessions/{id}/files/{name}` | eine Datei unverändert |
| `GET` | `/api/v1/sessions/{id}/report.json` | Kennzahlen der Sitzung, siehe [Sitzungsbericht](#sitzungsbericht) |
| `GET` | `/api/v1/sessions/{id}/report.md` | dasselbe als Markdown-Bericht |
| `DELETE` | `/api/v1/sessions/{id}` | Dateien löschen, Grabstein behalten |
| `POST` | `/api/v1/sessions/{id}/komoot` | zur zusammengehörigen Tour hochladen (`force`), siehe unten |
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
