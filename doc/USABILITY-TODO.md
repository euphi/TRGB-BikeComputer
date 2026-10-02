# Usability-TODO: Bedienung unterwegs

Das Gerät sitzt im Gehäuse am Lenker. **Unterwegs gibt es weder USB/Terminal noch
zuverlässig Web.** Alles, was man auf einer Fahrt braucht, muss am Gerät selbst
bedienbar sein. Hier steht, was dafür noch fehlt; die Übersicht über alle offenen
Arbeiten ist [`../ROADMAP.md`](../ROADMAP.md).

Schon am Gerät möglich: Fahrt starten, pausieren und beenden (Start/Pause-Taste,
[`design/ride-state-machine.md`](design/ride-state-machine.md)), Neustart und
Ausschalten, IMU-Kalibrierung und Referenzfahrt, WLAN wieder einschalten und IP
ablesen (alles auf dem Settings-Screen).

## 1. Fahrt und Statistik

- **Durchschnitt und Höchstgeschwindigkeit sind am Gerät nicht zu sehen.** Wischen
  hoch/runter auf dem Distanzfeld wechselt die Ø-Art, aber es gibt kein Widget
  dafür. Die Werte stehen nur auf `/stat/statistics.html`.
- **Auto-Off lässt sich mit offener Session nicht umschalten**, weil der Long-Press
  auf die Start/Pause-Taste dann die Fahrt beendet. Dafür braucht es eine zweite
  Geste oder einen Schalter auf dem Settings-Screen.
- **Rad senkrecht oder auf dem Heckträger** wird als Fahrt aufgezeichnet
  (Fahrstuhl, Transport). Ideen:
  - Senkrecht oder gekippt: Die Schwerkraft weicht stark von der kalibrierten Lage
    `g0` ab. Das lässt sich im ImuTask direkt prüfen; die Wegequalität dabei aussetzen.
  - Transport: GPS-Tempo über 10 km/h bei stehendem Rad (keine Umdrehungen am
    Speed-Sensor).
- Die echte Kalibrierung wurde über den Settings-Screen noch nie ausgelöst (nur
  Referenzfahrt starten/abbrechen ist auf dem Gerät getestet).

## 2. WLAN unterwegs

**Ist** (`src/WifiWebserver.cpp`):
- Es gibt nur **einen** Zugangspunkt, und der ist einkompiliert. Die per
  `/wifi/connect` gespeicherten Zugangsdaten werden überschrieben.
- Kommt nach dem Booten innerhalb von 100 s keine Verbindung zustande oder reißt
  sie ab, wird das WLAN abgeschaltet. Wieder an geht es über „WLAN verbinden" auf
  dem Settings-Screen oder `wifi on`.
- Einen anderen Zugangspunkt, etwa den Handy-Hotspot, versucht das Gerät nie.
- Den AP-Modus (`enableAPMode()`) gibt es im Code, am Gerät lässt er sich nicht
  einschalten.
- mDNS `TRGB-BC.local` wird angemeldet, Android-Browser lösen `.local` aber nicht
  zuverlässig auf. Die IP steht auf dem Settings-Screen.

**Wunsch:** Unterwegs per Handy auf den Webserver kommen.

**Ideen:**
- Mehrere Zugangspunkte speichern (WiFiMulti), darunter den Handy-Hotspot.
- AP-Modus am Gerät einschalten.
- Alternativ: TrailBridge meldet die IP oder übernimmt Einstellungen per BLE. Dafür
  muss das Protokoll in `../TrailBridge/PROTOCOL.md` abgestimmt werden.

## 3. Wege-Labels auf dem RQ-Screen

**Beobachtet auf der ersten Testfahrt:**
- Auf schlechten Wegen trifft man die Knöpfe schwer. Labels kommen deshalb
  verspätet (erstes Label 3,5 min nach dem Start) oder falsch.
- Echte Wechsel können sehr kurz sein (ein gutes Pflasterstück von wenigen
  Sekunden). **Als Fehltipp gilt ein Label, das innerhalb von 5 s wieder geändert
  wird** (Festlegung des Nutzers). Nach dieser Regel gab es vier Fehltipps,
  korrigiert nach 1 bis 3 s.
- Qualität: Bei Asphalt ist 2 gegen 3 schwer zu unterscheiden; fast alles war 2.
- Untergrund: Der „Waldweg" war geschottert. Die Kategorie wird nach der Lage (im
  Wald) statt nach dem Belag gewählt.

**Ideen:**
- Größere Trefferflächen, oder weniger Knöpfe gleichzeitig (erst Belag, dann
  Qualität).
- **Fehltipp-Schwelle 5 s:** Ein Label, das innerhalb von 5 s ersetzt wird, gilt
  nicht als Abschnitt; das Nachfolge-Label gilt ab dem ersten Tipp. Entweder in
  der Firmware (Übernahme erst nach 5 s ohne weitere Änderung, zurückdatiert auf
  den ersten Tipp; hält das Log sauber) oder in der Auswertung (`bikelog` verwirft
  Abschnitte unter 5 s; behält die Rohdaten).
- Rückdatieren: Ein Label gilt ab einem Zeitpunkt, der ein paar Sekunden
  zurückliegt (im Label-Satz über `prevDurationMs` oder ein neues Feld).
- Kategorien nach Belag benennen: „Waldweg" → „Naturboden/Erde"; Schotter
  unterteilen in „wassergebunden/fein" und „grob".
- Qualitätsstufen mit Ankerbeschreibungen: 1 = neu/glatt, 2 = gut, vereinzelt
  Flicken, 3 = viele Flicken/Risse, 4 = kaputt.
- Anzeige, welches Label gerade aktiv ist, auch auf dem Mainscreen.

## 4. Weitere Anzeigen und Einstellungen am Gerät

- Radumfang und Sensor-Kopplung (heute nur im Web).
- Batteriestand der BLE-Sensoren (wird gelesen, nicht angezeigt).
- Restdistanz und Restzeit der Navigation (werden empfangen, nicht angezeigt).
- Fortschritt beim Firmware-Update.
