# Sensor-Simulator (Debug-Build)

Statistik, Fahrzustände und Distanz am Schreibtisch testen, ohne zu fahren.
Nur im Build `trgb-esp32-s3-sim` (`-DBC_SIM`) enthalten – die normalen
Builds (`trgb-esp32-s3`, `-FL`, `-ota`) enthalten nichts davon.

## Was simuliert wird

`src/SimSensors.*` erzeugt einmal pro Sekunde die BLE-Notifications, die ein
echter CSC-Speed-Sensor (Slot CSC_1), ein Cadence-Sensor (CSC_2) und ein
Pulsgurt schicken würden – kumulierte Umdrehungen mit der Zeit der letzten
Umdrehung in 1/1024 s – und schickt sie durch
`BLEDevices::notifyCallbackCSC()`. Ab da läuft alles wie am Rad:
`Distance::updateRevs()` (Umdrehungen, Radumfang, Reconnect/Lost Distance),
`Statistics` (Zustände Stop/Pause/Fahrt/Rollen, Zeiten, Durchschnitte,
Trittfrequenz), Binärlog, UI.

- Radumfang: der eingestellte (`/stat/calibration`), geteilt mit der normalen
  Firmware.
- Umdrehungszähler starten bei jedem Boot bei 0 (wie ein Sensor, der seinen
  Zähler neu beginnt).
- `sim stop` = Sensoren aus (Disconnect → `DS_NO_CONN`); das nächste `sim …`
  ist ein Reconnect.
- Trittfrequenz 0 bei Fahrt = Rollen; Trittfrequenz/Puls weglassen (`-`) =
  dieser Sensor ist nicht da.
- Solange der Simulator aktiv ist, werden echte Speed/Cadence/HR-Sensoren
  ignoriert (Daten und Disconnects). Trotzdem besser ausgeschaltet lassen: Nach
  `sim stop` übernimmt ein echter Speed-Sensor mit ganz anderem Zählerstand.

## Speisung durch TrailBridge (Testfahrt)

Die Android-App kann ihre GPX-Route "abfahren" (Button "Testfahrt") und schickt
dabei Position, Geschwindigkeit und -- aus der GPX oder emuliert -- Puls und
Trittfrequenz im GPS-Positions-Frame, gekennzeichnet mit `SIM_FLAGS`
(`PROTOCOL.md` im TrailBridge-Repo, "Sensorwerte und Simulationsmodus"). Der
Simulator-Build speist das wie ein `sim <km/h> <rpm> <bpm>` pro Sekunde ein
(`SimSensors::feedFromTrailBridge()`): Statistik, Distanz und Binärlog laufen
mit simulierten Sensoren, die Position kommt zusätzlich ins Log -- die erste
Strecke mit GPS-Spur *und* Statistik ohne Fahren.

- Fehlt im Frame ein Puls oder eine Trittfrequenz, gibt es diesen Sensor nicht.
- **Höhe:** `BARO_HEIGHT_DM` ersetzt solange das Barometer (`I2CSensors::getHeight()`).
  Die Steigung kommt aus der normalen Rechnung (`Statistics::calculateGradient()`:
  Höhen- durch Streckenänderung, Strecke aus den simulierten Radumdrehungen) --
  sie wird nicht übertragen. Ohne Höhe im Frame (GPX ohne Höhendaten) bleibt das
  echte Barometer.
- **Leistung:** `POWER_W` wird geparst und gehalten (`SimSensors::getPower()`,
  `/debug/sim.json`), aber noch nirgends angezeigt oder geloggt -- der Binärlog
  hat kein Feld dafür (Satz voll, 64 Byte) und die UI keine Anzeige.
- Ende: erster Frame ohne `SIM_SENSORS`, `POSITION_NONE`, ein Fix älter als 5 s oder
  Trennung vom Handy -> Sensoren aus (`sim stop`). Nur eine von TrailBridge gestartete
  Simulation wird so beendet; ein manuelles `sim ...` bleibt.
- Die normale Firmware ignoriert die Sensorwerte (keine Fake-Kilometer im Odometer),
  markiert aber Log-Sätze mit simulierter Position als `LOG_SIMULATED`.
- `/debug/sim.json` zeigt unter `source`, woher die laufende Simulation kommt
  (`trailbridge` / `manual`).

## Eigene Statistik-Ablage

Der Simulator-Build legt die Fahrstatistik in einem eigenen NVS-Namespace ab
(`S_Stats` statt `Stats`, `NVS_STAT_PREFIX` in
`include/global_settings.h`). Gesamt-/Tour-/Trip-km der normalen Firmware
bleiben unberührt; nach dem Zurückflashen sind die echten Werte wieder da.
Geteilt bleiben WLAN, BLE-Adressen, Radumfang, Log-Einstellungen, IMU-Kalibrierung.

Nicht getrennt: die Sitzungen auf der SD-Karte (`/BIKECOMP/…`). Simulierte
Fahrten erscheinen als normale Sitzungen und werden vom BikeLogService
abgeholt, wenn er läuft – ohne GPS-Spur (die kommt nur von TrailBridge).

## Markierung im Log

Während der Simulator aktiv ist, markiert die Firmware ihre Log-Sätze
(ohne Formatversionssprung, die Bits waren bisher immer 0):

- Fahrdaten (`L_*.bin`, Typ 0): Bit `LOG_SIMULATED` (0x80) in `gpsFlags`
  (teilt sich nur das Byte mit den GPS-Bits, das Datenlayout hat sonst keine
  freien Bits). Reader: `Record.simulated`.
- Fahrzustand (Typ 4): `RSF_SIMULATED` (0x04) in `flags`,
  `RideStateRecord.simulated`.

Export: `bikelog info` meldet `SIMULIERT: n von m Fahrdaten-Datensätzen`,
GPX bekommt `[SIM] ` vor dem Namen, „SIMULIERT“ in der Beschreibung,
Schlüsselwort `simuliert`, `<bc:simulated>1</bc:simulated>` an Trackpunkten
und Fahrzustands-Segmenten; CSV mit `--roadq` hat die Spalte `Simuliert`.
Eine rein simulierte Sitzung hat allerdings keine GPS-Position (die kommt nur
von TrailBridge), der GPX-Export bleibt dann leer.

## Bauen und flashen

```sh
pio run -e trgb-esp32-s3-sim -t upload        # USB
pio run -e trgb-esp32-s3-sim-ota -t upload    # WLAN (/update)
```

Zurück zur normalen Firmware: `pio run -e trgb-esp32-s3[-ota] -t upload`.
Beim Booten meldet der Simulator-Build sich im Log mit `SIMULATOR BUILD`.

## Bedienung

### Serielle Konsole

```
sim 25 85 140     25 km/h, 85 rpm, Puls 140
sim 25 0          rollen (Trittfrequenz 0), kein Pulsgurt
sim 25 - -        nur Speed-Sensor
sim 0             stehen (Sensoren bleiben verbunden)
sim stop          Sensoren aus
sim               Status
```

### Web: `/debug/sim` (auch im Debug-Menü)

Manuelle Eingabe, Sensoren aus, Live-Statistik (Start/Trip/Tour aus
`/stat/summary`) und ein GPX-Player im Browser. Der Player läuft im Tab –
im Vordergrund lassen, Hintergrund-Tabs werden gedrosselt.

Direkt: `/debug/sim/set?speed=25&cad=85&hr=140` (`cad`/`hr` leer oder `-` =
aus), `/debug/sim/stop`, `/debug/sim.json`.

### GPX abspielen vom Rechner: `bikelog sim`

```sh
cd Tools
python3 -m bikelog sim fahrt.gpx --http TRGB-BC.local
python3 -m bikelog sim fahrt.gpx --serial /dev/ttyACM0 --summary-host TRGB-BC.local
python3 -m bikelog sim fahrt.gpx --dry-run 30        # nur ansehen, was gesendet würde
```

Optionen: `--cadence 80` (wenn die GPX keine hat), `--max-gap 150` (lange
Aufzeichnungspausen kürzen – 150 s reicht, um den Übergang Stopp → Pause bei
2 min zu sehen), `--start MIN`, `--duration MIN`, `--echo` (serielle Ausgabe
zeigen). Die serielle Konsole kann nur ein Programm gleichzeitig offen haben
– Monitor vorher schließen.

Regeln (gleich im Browser-Player):

- Geschwindigkeit aus der Geometrie, Distanzdifferenz über ±2 s,
- Puls/Trittfrequenz aus der TrackPointExtension, falls vorhanden,
- sonst Trittfrequenz: Vorgabe beim Fahren, 0 unter 3 km/h und bergab
  steiler als 4 % (Rollen).

Alles in Echtzeit: Die Statistik rechnet mit der Uhr des Geräts, schneller
abspielen geht nicht.

Am Ende druckt das Tool, was es gesendet hat (Strecke, Fahrzeit mit der
Hysterese der Firmware > 5,5 / < 0,3 km/h, Ø, max, Ø Trittfrequenz), neben der
Änderung der Trip-Statistik des Geräts. Für einen sauberen Vergleich vorher
Trip zurücksetzen (max und Ø Trittfrequenz sind Trip-Gesamtwerte). Kleine
Abweichungen sind normal: Der Sensor meldet nur ganze Umdrehungen, und die
Firmware sieht jede Änderung erst mit der nächsten Notification.
