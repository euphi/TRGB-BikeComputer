# BikeLog-Dienst

Nimmt die **Rohlogs** des Fahrradcomputers per HTTP entgegen, indiziert sie
und liefert sie als GPX oder CSV wieder aus.

Rohdaten werden unverändert aufbewahrt, alles andere wird bei Bedarf daraus
erzeugt. Damit verbessert eine bessere Exportlogik rückwirkend auch alte
Fahrten -- und die Verteilung an Nextcloud/Komoot/Strava sowie die Heatmap
können später auf demselben Bestand aufsetzen.

## Start

```bash
cd Tools
python3 -m venv .venv
.venv/bin/pip install -r BikeLogService/requirements.txt
PYTHONPATH=.:BikeLogService .venv/bin/uvicorn bikelogservice.app:app --port 8080
```

Interaktive API-Doku: <http://localhost:8080/docs>

Als Dienst: [`bikelog.service`](bikelog.service) nach
`~/.config/systemd/user/` kopieren, Pfade anpassen,
`systemctl --user enable --now bikelog`. Kein Container -- venv plus
systemd-Unit, überall gleich (Arbeitsrechner, Heimserver, Camperserver).

## Konfiguration

Alles über Umgebungsvariablen, alles mit brauchbarem Default:

| Variable | Default | Bedeutung |
|---|---|---|
| `BIKELOG_DATA_DIR` | `$STATE_DIRECTORY` bzw. `~/.local/state/bikelog` | Ablage für Rohlogs + SQLite-Index |
| `BIKELOG_REQUIRE_AUTH` | `0` | Auth an/aus |
| `BIKELOG_TOKENS` | -- | `token:name`, kommagetrennt |
| `BIKELOG_MAX_UPLOAD_BYTES` | 64 MiB | Obergrenze pro Upload |

## API (v1)

| Methode | Pfad | Zweck |
|---|---|---|
| `GET` | `/api/v1/health` | Erreichbarkeit + Anzahl Fahrten |
| `POST` | `/api/v1/rides?filename=&device=` | Rohlog hochladen (Body = Datei) |
| `GET` | `/api/v1/rides` | Liste (`limit`, `offset`) |
| `GET` | `/api/v1/rides/{id}` | Metadaten einer Fahrt |
| `GET` | `/api/v1/rides/{id}.gpx` | GPX (`max_fix_age_ms`, `segment_gap_s`, `ele`, `max_accuracy_m`) |
| `GET` | `/api/v1/rides/{id}.csv` | CSV (`with_gps`) |
| `GET` | `/api/v1/rides/{id}.bin` | Rohlog zurück |
| `DELETE` | `/api/v1/rides/{id}` | Fahrt löschen |

```bash
curl --data-binary @L0001.bin \
     -H 'Content-Type: application/octet-stream' \
     'http://localhost:8080/api/v1/rides?filename=L0001.bin&device=trgb-gravel'
```

**Der Upload ist idempotent.** Die Fahrt-ID ist der Inhalts-Hash: dieselbe
Datei ein zweites Mal hochzuladen liefert `200` mit derselben ID und
`"duplicate": true`, keinen Fehler und keine zweite Fahrt. Das ist hier keine
Feinheit, sondern die Voraussetzung dafür, dass die Firmware nach einem
Verbindungsabbruch blind wiederholen darf, ohne selbst zuverlässig Buch
führen zu müssen, was schon übertragen wurde.

Roher Request-Body statt `multipart/form-data`, weil der ESP32 genau das mit
`HTTPClient` direkt aus einem `File`-Stream senden kann -- und weil es dem
Dienst eine Abhängigkeit erspart.

## Auth nachrüsten

Auth ist aus, aber vollständig verdrahtet. **Jede** Route hängt schon heute
an der Dependency `require_principal` ([`auth.py`](bikelogservice/auth.py)),
die aktuell einen anonymen Principal zurückgibt. Einschalten:

```
BIKELOG_REQUIRE_AUTH=1
BIKELOG_TOKENS=<token>:gravel
```

Keine Routenänderung, keine Migration. Clientseitig kommt genau ein Header
dazu: `Authorization: Bearer <token>`. Bewusst Bearer-Token und nicht mTLS
oder signierte Requests -- der ESP32 soll dafür zwei Zeilen im
`HTTPClient`-Aufruf brauchen, ohne Zertifikatsspeicher, ohne
Uhrzeitabhängigkeit und ohne HMAC über einen Body, den er gar nicht puffern
kann. Der Test `test_every_route_requires_a_token_once_auth_is_on` hält das
Versprechen fest: eine neue Route ohne Dependency lässt ihn fehlschlagen.
