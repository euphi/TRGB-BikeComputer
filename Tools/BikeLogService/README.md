# BikeLog-Dienst

Holt die Sitzungen des Fahrradcomputers automatisch ab, sobald er im WLAN
auftaucht, bewahrt sie unverändert auf und liefert GPX/CSV daraus.

Eine **Sitzung** ist alles, was ein Boot des BC schreibt: `L_` (Binärlog),
`I_` (Kurzstatistik), `D_` (Debug-Log), `S_`/`R_` (IMU-Rohdaten), `T_`
(Zeit-Hinweise), `N_` (Forumslader-NMEA) -- alle mit demselben Stamm
(`_143012`), siehe [`../README.md`](../README.md). Die Dateien liegen im Dienst
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

**Wann** ([`puller.py`](bikelogservice/puller.py)):

- **mDNS**: Die Firmware meldet sich nach dem WLAN-Connect als
  `TRGB-BC._http._tcp.local`. Ein ServiceBrowser sieht das nach ein, zwei
  Sekunden und stößt den Abruf an.
- **Polling** als Netz: alle `BIKELOG_PULL_INTERVAL_S` (120 s) eine
  mDNS-Adressanfrage -- ist der BC weg, kostet das nichts, taucht er (wieder)
  auf, wird abgeholt. Nötig, weil der Browser einen Dienst, den er noch im
  Cache hat, nicht erneut meldet, und ein ausgeschalteter BC sich nicht
  abmeldet. Zusätzlich spätestens alle 30 min, solange er online ist.
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

Zu jeder Sitzung mit Binärlog schreibt der Dienst automatisch
`data/export/gpx/2026-09-27_164545_trgb.gpx` (lokale Startzeit + Gerät; mtime
= Start der Fahrt) -- als Übergabestelle für Nextcloud-Sync, Komoot/Strava
usw. ([`exporter.py`](bikelogservice/exporter.py)). Neu geschrieben wird,
wenn `L_` oder `I_` der Sitzung ankommt oder sich ändert, und für **alle**
Sitzungen, wenn `EXPORT_VERSION` erhöht wird -- das bei jeder inhaltlichen
Änderung am GPX tun, dann bekommen auch alte Fahrten die bessere Fassung.
Sitzungen ohne verwertbaren GPS-Fix bekommen keine Datei (`gpx_status`
`no-gps`), unlesbare Logs `error: …`.

Inhalt (Details in [`../bikelog/gpx.py`](../bikelog/gpx.py)):

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

## Installation (ia216: `~/bikelog`)

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
kein `enable-linger` (auf ia216 ist logind ohnehin kaputt: „Unit
dbus-org.freedesktop.login1.service failed to load properly“).

**Update**: nach `git pull` wieder `install.sh`, dann `sudo systemctl restart bikelog`.

Oberfläche: <http://ia216:8080/> (Fahrtenliste), API-Doku <http://ia216:8080/docs>.

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
| `BIKELOG_PUBLIC_URL` | -- | z. B. `http://ia216:8080`, für den Link im GPX |
| `BIKELOG_REQUIRE_AUTH` | `0` | Auth an/aus |
| `BIKELOG_TOKENS` | -- | `token:name`, kommagetrennt |
| `BIKELOG_MAX_UPLOAD_BYTES` | 64 MiB | Obergrenze pro `PUT` |

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
| `DELETE` | `/api/v1/sessions/{id}` | Dateien löschen, Grabstein behalten |
| `PUT` | `/api/v1/devices/{gerät}/files/{tag}/{name}` | eine Datei einliefern (Body = Datei) |
| `GET`/`POST` | `/api/v1/pull` | Status des Abholens / jetzt abholen (`device`) |

`PUT` ist idempotent: dieselben Bytes noch einmal ergeben `unchanged`, andere
Bytes unter demselben Namen `replaced` (der BC schreibt `I_`-Dateien neu,
wenn sich ihr Format ändert). Ein `L_`, das sich nicht lesen lässt, wird
trotzdem aufbewahrt und in `log_error` begründet -- es sind die Daten des
Geräts. `CUR/` wird mit `409` abgelehnt.

```bash
curl -T L_143012.bin http://ia216:8080/api/v1/devices/trgb/files/20260920/L_143012.bin
```

## Auth nachrüsten

Auth ist aus, aber vollständig verdrahtet. **Jede** Route hängt schon heute
an der Dependency `require_principal` ([`auth.py`](bikelogservice/auth.py)),
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
