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
Der Dienst holt ab, statt dass die Firmware hochlädt: keine Zugangsdaten auf dem
Gerät, keine Wiederholungslogik in einem Task, der mit BLE und Display um internen
Heap konkurriert. Für ein Rad, das nicht in Reichweite des heimischen WLANs steht,
gibt es zusätzlich den [Push](#hochladen-vom-fahrradcomputer-push).

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

## Hochladen vom Fahrradcomputer (Push)

Der Pull braucht den Fahrradcomputer im heimischen WLAN. Ein Rad, das woanders steht (Arbeit,
Keller ohne Empfang), kann stattdessen hochladen: Sobald es online ist, schickt es jede fertige
Sitzungsdatei per WebDAV in einen Nextcloud-Ordner, und der Dienst holt sie dort ab. Push ergänzt
den Pull, ersetzt ihn nicht -- beide können für dasselbe Gerät laufen:

- Die Dateien bleiben unverändert auf der SD-Karte, der Pull findet sie weiterhin.
- Der Dienst legt eine hochgeladene Datei über dasselbe `put_file()` ab wie eine abgeholte:
  dieselben Bytes noch einmal sind `unchanged`, nichts liegt doppelt.
- Eine im Dienst gelöschte oder archivierte Sitzung kommt durch einen Push nicht zurück (der
  Eingang verwirft ihre Dateien; ein direkter Upload trägt den Header `X-BikeLog-Auto: 1`, siehe
  API).

**Fahrradcomputer** ([`LogPush.cpp`](https://github.com/euphi/TRGB-BikeComputer/blob/main/src/LogPush.cpp),
Seite `/push`, verlinkt von der Startseite und von `/wifi`):

| Ziel | URL | User / Password |
|---|---|---|
| **Upload-Link** (am einfachsten) | der Freigabe-Link eines als „Dateien ablegen“ (File drop) geteilten Ordners, `https://<cloud>/s/<token>` | leer / das Freigabe-Passwort, falls eins gesetzt ist |
| WebDAV-Ordner mit Konto | `https://<cloud>/remote.php/dav/files/<nutzer>/BikeLog-Inbox/<netzwerkname>` | Nextcloud-Nutzer / ein eigenes **App-Passwort** (Einstellungen -> Sicherheit -> „Geräte & Sitzungen“), allein widerrufbar |

Ein Upload-Link braucht kein Konto auf dem Fahrradcomputer. Die Firmware macht daraus die
WebDAV-Seite (`/public.php/webdav`, der Token als Nutzer). Ein File Drop nimmt nur `PUT` und behält
nur den Dateinamen (Nextcloud 31, `FilesDropPlugin`), deshalb kommen die Dateien flach als
`<netzwerkname>~<tag>~<datei>` an (`TRGB-FL~20261018~L_143012.bin`, eine Datei direkt in
`/BIKECOMP`: `TRGB-FL~~L0042.bin`); bei einer „Dateianfrage“ legt Nextcloud sie in einen Ordner je
Nickname (die Firmware schickt ihren Netzwerknamen). Ein schon vorhandener Name wird nicht ersetzt,
sondern als `... (2).bin` abgelegt -- der Dienst nimmt solche Kopien älteste zuerst. Mit Konto
werden fehlende Ordner angelegt (`MKCOL`), und die Tagesordner der Karte bleiben erhalten. *Test
target* prüft URL und Zugang (bei einem Upload-Link schreibt er `<netzwerkname>~push-test.txt`, das
der Dienst löscht), *Upload now*
wartet nicht auf die nächste Runde, *Mark all as uploaded* trägt ein, was auf der Karte liegt,
ohne es hochzuladen -- für eine Karte, deren Sitzungen der Dienst schon per Pull hat. Das
Passwort kommt nur über das Formular herein, wird nie wieder angezeigt und nie geloggt.

Wann: 20 s nach jedem WLAN-Connect, danach alle 10 min, solange er online ist (eine mit langem
Druck beendete Fahrt erscheint ohne Reconnect), nach einem Fehler nach 5 min. Nur fertige
Sitzungen, nie `CUR/`. Was hochgeladen ist, steht in `/bcpush.txt` auf der SD-Karte
(`<pfad> <größe>`); eine Datei geht nur dann noch einmal hoch, wenn sich ihre Größe ändert (neues
Format der Kurzstatistik). Die Datei löschen lädt alles neu hoch. HTTPS wird gegen die gängigen
Stamm-Zertifizierungsstellen geprüft (Let's Encrypt und die großen CAs: ESP-IDFs eigenes
Bundle der gängigen CAs, `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_CMN`, erneuert sich mit jedem
IDF-Update); ein selbst signiertes Zertifikat geht nicht. Serielle Konsole:
`push [status|now|test|mark|on|off]`.

Das WLAN selbst folgt seinen eigenen Regeln ([WLAN](WIFI.md)): Das Netz muss eingetragen sein,
und ohne Netz in Reichweite geht es 5 Minuten nach dem Start aus. Der Upload läuft auch über
einen Handy-Hotspot mitten in der Tour; er schickt nur fertige Sitzungen, ein paar hundert KB
pro Fahrt.

**Dienst** ([`inbox.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/inbox.py)):
braucht den Nextcloud-Zugang des [Syncs](#nextcloud-sync) und den Eingangsordner:

```
BIKELOG_NEXTCLOUD_INBOX=BikeLog-Inbox
```

`BIKELOG_NEXTCLOUD_INBOX` ist der Ordner hinter dem Upload-Link (bzw. über den Geräteordnern),
vom Nextcloud-Konto des Dienstes aus gesehen. Er schaut alle `BIKELOG_INBOX_INTERVAL_S` (5 min) in den Eingang, legt jede Datei ab und löscht
sie dort; ein geleerter Tagesordner verschwindet, sobald eine Stunde nichts mehr hineingeschrieben
wurde. Der Ordner unter dem Eingang ist der Netzwerkname des Fahrradcomputers;
`BIKELOG_PULL_TARGETS` ordnet ihn dem Gerätenamen zu (`pendler=TRGB-FL` legt
`BikeLog-Inbox/TRGB-FL/...` unter `pendler` ab, dieselben Sitzungen wie der Pull von
`TRGB-FL.local`), ein Name ohne Ziel wird so übernommen. Von Hand: `POST /api/v1/inbox` oder
`bikelogservice inbox`.

Ohne Nextcloud kann der Fahrradcomputer auch direkt zum Dienst hochladen, wenn er ihn erreicht
(VPN, Portfreigabe): Folder URL `http://<server>:8080/api/v1/devices/<gerät>/files`, User leer,
als Passwort ein Token aus `BIKELOG_TOKENS`, falls Auth an ist.

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

## Geplante und regelmäßige Strecken

Die Seite **Strecken** (`/routes`) hält Strecken zum Fahren: **geplante** (eine anstehende
Tour) und **regelmäßige** (Pendelstrecke, Feierabendrunde). Eine oder mehrere GPX-Dateien
hochladen; jede wird darauf geprüft, was die Navigation von TrailBridge daraus macht, und zwar
so gelesen wie der `GpxParser` der App
([`bikelog/routecheck.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/bikelog/routecheck.py)):

| Prüfung | Was sie sagt |
|---|---|
| Geometrie | Track (`trk`) oder nur Routenpunkte (`rte`: dazwischen kennt TrailBridge den Weg nicht, Warnung); Punkte ohne gültige Koordinate, weniger als zwei Punkte, unter 100 m, kein XML/GPX, ein `DOCTYPE` oder über 20 MB = **nicht nutzbar** |
| Abbiegehinweise | aus OsmAnds Routensegmenten, aus den Routenpunkten (`rtept` mit Abbiegen) oder -- keine in der Datei oder nur Start/Zwischenziel/Ziel wie bei bikerouter.de -- von der App aus den Winkeln des Tracks abgeleitet (ohne Straßennamen) |
| Höhen | ohne `<ele>` an (fast) jedem Punkt kein Höhenprofil und keine Anstiege auf dem Fahrradcomputer (Warnung) |
| Lücken | Punkte mehr als 2 km auseinander (vereinfachter Track) oder Löcher über 200 m zwischen Track-Segmenten (Warnung) |
| Zeiten | Zeitstempel in der Datei: Die Restzeit kommt aus der geplanten Fahrzeit |
| Wegpunkte | `wpt` bis 100 m neben der Strecke erscheinen in der Streckenübersicht, die anderen werden gezählt |

Die Liste ist kurz: oben die **aktuellen** (geplanten) Strecken, darunter aufklappbar die
**regelmäßigen** (offen) und das **Archiv** (zu) -- je Zeile Name, Tags, Distanz, Höhenmeter und
ein kleines Höhenprofil (gleicher Maßstab in jeder Zeile bis 300 m Höhenunterschied, eine flache
Strecke bleibt flach, am Handy ausgeblendet) und ein Knopf **In TrailBridge laden** (bei der
bereitgestellten Strecke steht dort, ob sie schon auf dem Handy ist); das Prüfurteil steht nur
da, wenn etwas nicht stimmt. Darüber eine Zeile, welche Strecke TrailBridge hat und wann die
App zuletzt gefragt hat, aufklappbar mit der Einrichtung. Ein Klick auf die
Strecke öffnet ihre Seite (`/routes/{id}`) mit Distanz, Höhenmeter (mit Min./Max.), den Anstiegen
mit Kategorie (dieselben Kriterien wie im Fahrtbericht), geplanter Zeit und Geschwindigkeit, wenn
die Datei Zeiten hat, dem Höhenprofil (Höhenachse über mindestens 200 m, wie bei allen Höhenprofilen des Dienstes)
und den Befunden. Dort lässt sich eine Strecke umbenennen, anders einsortieren, mit den Tags
**Pendel/Alltag** und **Training** versehen, archivieren (das nimmt sie TrailBridge weg), durch
eine neue Version ihrer GPX ersetzen, herunterladen und löschen. Eine
Datei, die TrailBridge gar nicht lesen kann, wird abgelehnt und nicht aufbewahrt. Ablage in
`<data>/routes.json`, die Dateien unverändert in `<data>/routes/`
([`routes.py`](https://github.com/euphi/TRGB-BikeComputer/blob/main/Tools/BikeLogService/bikelogservice/routes.py)); ändert sich die Prüfung selbst,
wird `CHECK_VERSION` erhöht, und der Dienst prüft die gespeicherten Strecken beim Start neu.

**Wegpunkte** (`<wpt>` in der GPX, z. B. in BRouter-Web per Rechtsklick benannt): Die Liste zeigt
die benannten nur als schmale senkrechte Striche im kleinen Höhenprofil, an ihrer Distanz ab
Start. Die Seite der Strecke zeigt eine Karte (die Strecke als Linie, ohne Kartenkacheln) und das
Höhenprofil mit jedem Wegpunkt markiert und benannt, darunter eine Tabelle mit Name, Distanz ab
Start und bis zum Ziel. Ein `wpt` mit `type` (`from`, `via`, `to`) liegt auf einem Routenpunkt;
einer ohne (ein Ort von Interesse) kann neben der Strecke liegen, seine Distanz ist deshalb die
seiner Projektion auf den Track (ein Hinweis nennt den Abstand; weiter als 1 km entfernt wird er
weggelassen). Die Maschinennamen von BRouter-Web (`from`, `via1`, `to`) sind Start, Ziel und
unbenannte Zwischenpunkte: in der Tabelle ausgegraut, ohne Strich in der Liste, ohne Namen in den
Diagrammen. Die OsmAnd-Abbiegehinweise im `<rte>` sind keine Wegpunkte. Namen werden für HTML
maskiert.

### Strecken für TrailBridge

„Für TrailBridge bereitstellen" markiert **eine** Strecke (eine andere zu markieren nimmt die
Markierung weg). Die App holt sie sich selbst. Vertrag zwischen Dienst und App:

- **Einrichten in der App:** nur die Adresse des Dienstes (`BIKELOG_PUBLIC_URL`, z. B.
  `https://bikelog.example.org`). Kein Passwort: Wie die Seiten begrenzt den Abruf das Netz --
  der Dienst ist nur aus dem Heimnetz und dem VPN erreichbar (Reverse Proxy mit
  `allow`/`deny`). Wird `BIKELOG_REQUIRE_AUTH` eingeschaltet, braucht der Abruf wie jeder
  andere Pfad ein Token.
- **Wann die App fragt:** beim Start, danach alle 15 Minuten -- aber nur, solange sie weder
  navigiert noch eine Testfahrt abspielt (eine geladene Route beendet beides).
- `GET /api/v1/trailbridge/route` -- `204` keine Strecke markiert; `200` JSON
  `{id, name, kind, revision, marked_at, sha256, size_bytes, distance_km, ascent_m, verdict,
  gpx_url}` mit `ETag: "<revision>"`; mit `If-None-Match: "<revision>"` `304`.
  `gpx_url` ist ein Pfad relativ zum Dienst.
- **Neu = andere Revision.** Die Revision ändert sich mit der Datei (neue Version
  hochgeladen) und mit jeder Markierung -- dieselbe Strecke noch einmal zu markieren schickt
  sie noch einmal. Die App merkt sich die zuletzt geladene Revision und tut nichts, solange
  sie gleich bleibt.
- `GET /api/v1/trailbridge/routes/{id}.gpx` -- die Datei unverändert
  (`X-Bikelog-Revision`). Die App lädt sie wie eine von Hand geöffnete GPX (startet die
  Navigation nicht).
- `POST /api/v1/trailbridge/route/fetched?revision=<revision>` -- nachdem die App sie
  fehlerfrei gelesen hat. Die Seite zeigt dann „Von TrailBridge geladen" mit Uhrzeit; `409`,
  wenn die Revision nicht mehr aktuell ist (die nächste Abfrage bringt die neue).

Die Seite zeigt außerdem, wann die App zuletzt gefragt hat (seit dem Start des Dienstes).

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

**Wind.** Leistungsschätzung und aerobe Entkopplung rechnen den Wind der Fahrt ein
([Details](TOOLS.md#sitzungsbericht)). Der Dienst fragt bei Open-Meteo den Stundenwind für den
Fahrtag und die 0,1°-Zelle ab -- nichts sonst verlässt den Rechner, die Spur nie -- und hebt
jede Antwort unter `<data>/weather/` auf (`2026-10-24_49.4_10.9.json`); ein neu gerechneter
Bericht braucht so kein Netz und liefert dasselbe Ergebnis. Die Fahrtseite zeigt Ø Wind, Böen,
den Anteil von Gegen- und Rückenwind und neben der Leistung den Wert ohne Wind. Wo die
Entkopplung nur den Wind widerspiegeln würde (hin dagegen, zurück damit), steht das da statt
einer Zahl. Ist Open-Meteo nicht erreichbar oder hat noch keine Daten, wird der Bericht ohne
Wind gerechnet (die Seite sagt warum) und eine Stunde später neu versucht; Tage aus der
„Historical Forecast“-API oder jünger als eine Woche werden erneut aus dem Archiv geholt,
sobald es endgültig ist (`wind.retry_after` im Berichts-JSON). `BIKELOG_WIND=0` schaltet die
Abfrage ab; der Faktor vom 10-m-Wind zum Wind am Fahrer (Standard 0,55) ist `wind_factor` auf
der Seite **Fahrer**. Abschattung (Wald, Bebauung) wird nicht modelliert.

**Leerlauf.** Zeit vor der ersten und nach der letzten wirklichen Bewegung (Gerät in der Garage
eingeschaltet, zu Hause weitergelaufen) ist kein Stopp und zählt weder zur Dauer noch zu
Temperatur oder GPS-Statistik; Stopps mitten in der Fahrt bleiben. Höhenänderungen im Stand
(Fahrstuhlfahrt mit dem Rad) zählen nicht als Steigen.

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
bikelogservice inbox                                                  # Push-Eingang einmal abholen
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
| `BIKELOG_NEXTCLOUD_INBOX` | -- | Ordner, in den die Fahrradcomputer [hochladen](#hochladen-vom-fahrradcomputer-push) (WebDAV-Pfad); leer = wird nicht abgeholt |
| `BIKELOG_INBOX_INTERVAL_S` | `300` | wie oft der Eingang angeschaut wird |
| `BIKELOG_ATHLETE_FILE` | `<data>/athlete.json` | Fahrerdaten für den Sitzungsbericht |
| `BIKELOG_WIND` | `1` | Wind von Open-Meteo im Fahrtbericht; `0` = Berichte ohne Wind, kein Netzzugriff |
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
| `GET` | `/routes` | geplante und regelmäßige Strecken (HTML) |
| `GET` | `/routes/{id}` | eine Strecke mit Höhenprofil, Anstiegen und Befunden (HTML) |
| `GET`/`PUT` | `/api/v1/routes` | Strecken mit Prüfung und Kennzahlen (`tags`, `archived`) / eine hochladen (`kind=planned|regular`, `tags=commute,training`, `name`, `filename`; Body = GPX) |
| `GET`/`DELETE` | `/api/v1/routes/{id}` | eine Strecke / löschen |
| `GET` | `/api/v1/routes/{id}.gpx` | ihre GPX unverändert |
| `PUT` | `/api/v1/routes/{id}/gpx` | neue Version der Datei (`filename`) |
| `POST` | `/api/v1/routes/{id}/trailbridge` | TrailBridge anbieten (`on=false`: zurücknehmen) |
| `GET` | `/api/v1/trailbridge/route` | die für TrailBridge markierte Strecke, siehe [oben](#strecken-fur-trailbridge) |
| `GET` | `/api/v1/trailbridge/routes/{id}.gpx` | ihre Datei, für die App |
| `POST` | `/api/v1/trailbridge/route/fetched` | die App bestätigt eine Revision (`revision`) |
| `DELETE` | `/api/v1/sessions/{id}` | Dateien löschen, Grabstein behalten |
| `POST` | `/api/v1/sessions/{id}/komoot` | zur zusammengehörigen Tour hochladen (`force`), siehe unten |
| `POST` | `/api/v1/sessions/{id}/komoot/ignore` | die Frage der Seite mit „nein“ beantworten (`ask_again=true`: wieder fragen) |
| `PUT` | `/api/v1/devices/{gerät}/files/{tag}/{name}` | eine Datei einliefern (Body = Datei) |
| `GET`/`POST` | `/api/v1/pull` | Status des Abholens / jetzt abholen (`device`) |
| `GET`/`POST` | `/api/v1/inbox` | Status des Push-Eingangs / jetzt hineinschauen |

`PUT` ist idempotent: dieselben Bytes noch einmal ergeben `unchanged`, andere
Bytes unter demselben Namen `replaced` (der BC schreibt `I_`-Dateien neu,
wenn sich ihr Format ändert). Ein `L_`, das sich nicht lesen lässt, wird
trotzdem aufbewahrt und in `log_error` begründet -- es sind die Daten des
Geräts. `CUR/` wird mit `409` abgelehnt. Eine gelöschte Sitzung kommt mit einem `PUT`
zurück -- außer mit dem Header `X-BikeLog-Auto: 1` (der automatische Push des
Fahrradcomputers): Dann lautet die Antwort `{"status": "deleted"}`, und nichts wird abgelegt,
wie beim Pull.

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
