# Usability-TODO: Bedienung unterwegs

Das Gerät sitzt im Gehäuse am Lenker. **Unterwegs gibt es weder USB/Terminal noch
zuverlässig Web.** Alles, was man auf einer Fahrt braucht, muss am Gerät selbst
bedienbar sein. Hier steht, was dafür noch fehlt; die Übersicht über alle offenen
Arbeiten ist die [Roadmap](ROADMAP.md).

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

WLAN wird am Gerät eingerichtet (Settings-Screen → „Netzwerke": suchen, auswählen,
Passwort tippen) oder auf der Webseite `/wifi`: mehrere Netze in Prioritätsreihenfolge,
ein eigener Hotspot. Siehe [WLAN](WIFI.md). Offen:

- **Kein Netz gefunden = WLAN nach 5 min aus.** Danach schaltet nur der Settings-Screen es
  wieder ein. Unterwegs mit dem Handy-Hotspot heißt das: den Hotspot *vor* dem Booten
  einschalten, oder danach „WLAN an" drücken.
- Eine Suche dauert etwa 10 s, solange BLE läuft (das Funkmodul ist geteilt).
- Das Hotspot-Passwort steht nur auf dem Display. Ein QR-Code auf dem Display würde dem
  Handy das Tippen ersparen.
- Gespeicherte Netze lassen sich nur auf der Webseite sortieren und löschen, nicht am
  Display (Entscheidung des Nutzers).
- mDNS `TRGB-BC.local` wird angemeldet, Android-Browser lösen `.local` aber nicht
  zuverlässig auf. Die IP steht auf dem Settings-Screen; im Hotspot-Modus öffnet ein
  Captive Portal die Seite von selbst.
- Alternativ: TrailBridge meldet die IP oder übernimmt Einstellungen per BLE. Dafür
  muss das [Protokoll](trailbridge/PROTOCOL.md) abgestimmt werden, und es braucht den
  Rückkanal vom Fahrradcomputer zur App (siehe „Später" in der [Roadmap](ROADMAP.de.md)).

## 3. Wege-Labels auf dem RQ-Screen

![Wege-Label-Screen](screenshots/roadlabels.png){ width="240" }

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
