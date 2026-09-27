# Usability-TODO

Gesammelt nach der ersten Testfahrt mit eingebautem Gerät (2026-09-27, Sitzung
`20260927/L_125751.bin`). Ausgangslage: Das Gerät ist im Gehäuse, **unterwegs gibt
es weder USB/Terminal noch zuverlässig Web**. Alles, was man auf einer Fahrt
braucht, muss am Gerät selbst bedienbar sein.

## 1. Fahrt starten, pausieren, beenden

**Ist:**
- Jede Sitzung (`L_*.bin` usw.) beginnt mit dem Booten und endet mit dem nächsten
  Boot (`src/LogSessions.cpp`). Eine Fahrt lässt sich nicht bewusst starten oder beenden.
- Der Pause-Knopf (`rr_btn_pause`) hat nur einen Long-Press, und der schaltet die
  automatische Abschaltung ein oder aus (`DSE_toggleStandbyMode` → `Statistics::toggleStandbyMode()`).
  Mit einer Pause der Fahrt hat er nichts zu tun. Ein kurzer Tipp macht nichts.
- Ausschalten ist auf RimRidge nicht möglich. Deep Sleep gab es nur auf dem alten
  Settings-Screen (`EvDeepSleep`). Automatisch schaltet sich das Gerät erst nach
  5 Minuten im Zustand BREAK ab (`Statistics.cpp`, mit Meldungsfenster).
- Folge: Fahrstuhlfahrt (Rad senkrecht) und Transport auf dem Heckträger wurden
  als Teil der Fahrt aufgezeichnet.

**Wunsch:**
- Fahrt am Gerät **starten / pausieren / beenden**.
- **Ausschalten** am Gerät.

**Ideen:**
- Pause-Knopf: Ein Tipp schaltet Pause an und aus, Long-Press beendet die Fahrt (mit Rückfrage).
- Automatisch erkennen und die Wegequalität dabei aussetzen:
  - **Rad senkrecht oder gekippt:** Die Schwerkraft weicht stark von der kalibrierten Lage `g0` ab.
    Das lässt sich im ImuTask direkt prüfen.
  - **Transport:** GPS-Tempo über 10 km/h bei stehendem Rad (keine Umdrehungen am Speed-Sensor).
- Das Logformat braucht dafür einen Marker für Fahrt, Pause und Ende, etwa einen
  neuen Satztyp oder ein Flag im Label- oder Datensatz.

## 2. Kalibrierung und Werkzeuge ohne Web

**Ist:**
- IMU-Kalibrierung (`rq cal`), Referenzfahrt (`rq ref start`) und Rohmitschnitte
  gehen nur über `/debug/imu` oder die serielle Konsole. Ohne WLAN war unterwegs
  **keine Kalibrierung und keine Referenzfahrt** möglich.
  Die Aufnahme-Taste auf dem RQ-Screen startet immerhin den Rohmitschnitt.

**Wunsch:** Kalibrieren (im Stand) und Referenzfahrt (60 s glatter Asphalt, ≥ 15 km/h)
am Gerät starten, mit Fortschritt und Ergebnis.

**Idee:** Beides passt auf den RQ-Screen oder einen künftigen Settings-Screen.
Fortschritt und Status liefert `I2CSensors` schon für die Debug-Seite:
`imuSnap.calState`/`calProgress`, `rqSnap.refState`/`refProgressS`.

## 3. WLAN unterwegs

**Ist** (`src/WifiWebserver.cpp`):
- Es ist nur **ein** Zugangspunkt gespeichert (`StrSSID[0]`).
- Kommt nach dem Booten innerhalb von 100 s keine Verbindung zustande, wird das WLAN
  ganz abgeschaltet. Der Kommentar dort sagt fälschlich 10 s.
- Reißt die Verbindung ab, wird das WLAN ebenfalls abgeschaltet
  (bei der Fahrt: 12:59:19 „Wifi connection lost - disabling it to save power“).
- Wieder an geht es nur durch einen Neustart. Einen anderen Zugangspunkt, etwa den
  Handy-Hotspot, versucht das Gerät nie.
- Den AP-Modus (`TRGB-BC` / `123456`, `enableAPMode()`) gibt es im Code, auf RimRidge
  lässt er sich aber nicht einschalten. Der alte SWLAN-Screen ist abgeschaltet.
- mDNS `TRGB-BC.local` wird angemeldet. Die IP muss man also nicht unbedingt kennen,
  Android-Browser lösen `.local` aber nicht zuverlässig auf.

**Wunsch:** Unterwegs per Handy auf den Webserver kommen.

**Ideen:**
- Mehrere Zugangspunkte speichern (WiFiMulti), darunter den Handy-Hotspot.
- WLAN und AP-Modus am Gerät einschalten, die IP auf dem Display anzeigen.
- Alternativ: TrailBridge meldet die IP oder übernimmt Kalibrierung und Referenzfahrt
  per BLE. Dafür muss das Protokoll in `../TrailBridge/PROTOCOL.md` abgestimmt werden.

## 4. Wege-Labels auf dem RQ-Screen

**Beobachtet:**
- Auf schlechten Wegen trifft man die Knöpfe schwer. Labels kommen deshalb verspätet
  (Beispiel: Die Fahrt begann 13:00, das erste Label kam 13:03:30).
- Echte Wechsel können sehr kurz sein: Das gute Pflasterstück war nur wenige
  Sekunden lang, Wechsel nach 12 s oder 22 s waren echt. **Als Fehltipp gilt ein
  Label, das innerhalb von 5 s wieder geändert wird** (Festlegung des Nutzers).
  Nach dieser Regel zeigt das Log vier Fehltipps:

  | Uhrzeit | Label | korrigiert nach |
  |---|---|---|
  | 13:03:30 | Asphalt Q0 | 1 s |
  | 13:19:42 | Schotter → keiner → Schotter | 2 s |
  | 13:21:36 | Pflaster 3 → 1 | 3 s |
  | 13:26:05 | Asphalt 4 → 3 | 1 s |

- Qualität: Bei Asphalt ist 2 gegen 3 schwer zu unterscheiden. 1 wäre „sehr glatt/neu“,
  das kam auf der Route nicht vor. Fast alles war 2: gut mit gelegentlichen
  Ausbesserungen. Eine etwas schlechtere Straße blieb trotzdem auf 2.
- Untergrund: Der „Waldweg“ war geschottert. Einen echten Waldweg (Naturboden)
  gab es nur kurz. Die Kategorie wird also nach der Lage (im Wald) statt nach dem
  Belag gewählt.

**Ideen:**
- Größere Trefferflächen, oder weniger Knöpfe gleichzeitig (erst Belag, dann Qualität).
- **Fehltipp-Schwelle 5 s:** Ein Label, das innerhalb von 5 s ersetzt wird, gilt nicht als
  Abschnitt. Das Nachfolge-Label gilt dann ab dem ersten Tipp. Das lässt sich in der
  Firmware lösen (Übernahme erst nach 5 s ohne weitere Änderung, zurückdatiert auf den
  ersten Tipp) oder in der Auswertung (`bikelog` verwirft Abschnitte unter 5 s). Die
  Firmware-Lösung hält das Log sauber; die Auswertung behält die Rohdaten.
- Rückdatieren: Ein Label gilt ab einem Zeitpunkt, der ein paar Sekunden zurückliegt.
  Das ließe sich im Label-Satz über `prevDurationMs` oder ein neues Feld abbilden.
- Kategorien nach Belag benennen:
  - „Waldweg“ → „Naturboden/Erde“.
  - Schotter unterteilen in „wassergebunden/fein“ und „grob“.
- Die Qualitätsstufen mit kurzen Ankerbeschreibungen versehen:
  - 1 = neu/glatt
  - 2 = gut, vereinzelt Flicken
  - 3 = viele Flicken/Risse
  - 4 = kaputt
- Optional eine Anzeige, welches Label gerade aktiv ist, auch auf dem Mainscreen.

## Nicht Usability, aber bei der Testfahrt aufgefallen

Diese Punkte gehören zur Firmware und sind in der Auswertung dieser Fahrt dokumentiert,
nicht hier umzusetzen:
- **FIFO-Lücken:** Etwa ein Drittel der Intervalle haben Lücken im Sensor-FIFO (`IF_DATA_GAP`), auch im Stand.
- **ImuTask-Stack:** Nur noch 832 Byte frei, mit Stoß-Ausschnitten und Rohmitschnitt.
- **Neustarts ohne Logeintrag:** 13:45:33 und 13:46:09. Vor dem zweiten war die
  UI-Sperre mehrere Sekunden blockiert („… blocked by mutex“).
  Der Grund des Neustarts wird nicht geloggt (`esp_reset_reason()`).
