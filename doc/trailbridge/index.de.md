# TrailBridge

[TrailBridge](https://github.com/euphi/TrailBridge) ist die Android-App zum
Fahrradcomputer. Sie reicht Navigation, GPS-Position und das Höhenprofil des nächsten
Anstiegs per BLE an den Fahrradcomputer weiter -- als Ersatz für Komoots eingestellten
BLE-Navigationsdienst.

<div class="shots phone" markdown>
<figure markdown>![TrailBridge Hauptseite](../screenshots/trailbridge-main.png)<figcaption>Modus, Verbindung, Route</figcaption></figure>
<figure markdown>![TrailBridge Testfahrt](../screenshots/trailbridge-testride.png)<figcaption>Testfahrt</figcaption></figure>
<figure markdown>![TrailBridge Navi-Modus](../screenshots/trailbridge-navi.png)<figcaption>Navi-Modus</figcaption></figure>
</div>

## Was die App macht

- **Navigation aus OsmAnd**: liest OsmAnds Turn-by-Turn-Navigation über dessen AIDL-API
  (Manöver, Straßennamen, übernächstes Manöver, Fahrspuren, Restdistanz und -zeit) und
  funkt sie als kompaktes TLV-Protokoll per BLE -- siehe [BLE-Protokoll](PROTOCOL.md).
- **GPS-Position**: ein zweiter, unabhängiger BLE-Service mit der rohen GPS-Position des
  Handys, direkt vom GPS-Chip. Er funktioniert ohne OsmAnd, landet im Fahrtenlog des
  Fahrradcomputers und stellt dessen Uhr, wenn kein WLAN da ist.
- **GPX-Routen**: GPX-Datei öffnen, teilen oder laden und die Route starten --
  TrailBridge navigiert dann selbst (GPS-Position auf die Route gematcht, kein OsmAnd
  nötig) und sendet dieselben Nav-Frames. Abbiegehinweise kommen aus der Datei (`rtept`,
  OsmAnd-Routensegmente), sonst aus der Track-Geometrie.
- **Anstiege**: Hat die Route Höhendaten, geht kurz vor jedem Anstieg (500 m vor dem Fuß)
  sein Höhenprofil bis zum Gipfel raus -- auch bei langen Pässen in einem Stück. Der
  Fahrradcomputer zeigt es auf seinem [Anstiegs-Screen](../CLIMB.md).
- **Testfahrt**: Die geladene GPX-Route lässt sich zum Testen „abspielen". TrailBridge
  schickt dann statt der echten eine erfundene Position entlang der Strecke -- mit
  passender Geschwindigkeit (aus den GPX-Zeitstempeln, sonst berechnet: langsamer
  bergauf, schneller bergab, vorsichtig in Kurven), Puls, Trittfrequenz und Leistung (aus
  der GPX, sonst auf Wunsch emuliert) und der Höhe der Route. Über ein Höhenprofil mit
  Manöver-Markern lässt sich vor- und zurückspringen. Die Sensorwerte wertet nur der
  [Simulator-Build](../SIMULATOR.md) des Fahrradcomputers aus; sie sind im Protokoll als
  simuliert gekennzeichnet.
- **Navi-Modus**: ein reduzierter Screen für unterwegs -- nur Navigation, Position und,
  solange eines an den Fahrradcomputer gesendet ist, das Höhenprofil der Steigung voraus.
  Der Bildschirm bleibt dabei an.

## Modi

| Modus | Was läuft |
|---|---|
| OsmAnd | Navigation aus OsmAnd, echte GPS-Position |
| GPS/GPX-Navigation | TrailBridge navigiert entlang der geladenen GPX-Route, echte GPS-Position |
| GPX-Simulation | die geladene GPX-Route wird als Testfahrt abgespielt, mit simulierter Position |
| All-in-one | alles auf einem Screen: OsmAnd, GPX-Navigation und Simulation |

## Einrichten

1. OsmAnd installieren und eine Offline-Karte für die eigene Gegend laden.
2. TrailBridge installieren ([Releases](https://github.com/euphi/TrailBridge/releases))
   und öffnen. Der Status zeigt, dass die App in OsmAnd noch nicht freigeschaltet ist.
3. In OsmAnd: Menü > Plugins > TrailBridge > aktivieren.
4. Zurück in TrailBridge wird der Status „verbunden, warte auf Navigationsdaten".
5. In OsmAnd eine Route starten -- oder in TrailBridge eine GPX-Datei laden und dort
   starten.

Der Fahrradcomputer findet TrailBridge von selbst: Er sucht nach dem Navigations-Service
und verbindet sich mit dem ersten Handy, das ihn bewirbt.

### Testfahrt

1. GPX laden und das Testfahrt-Panel mit dem Höhenprofil öffnen.
2. Es zeigt, woher Geschwindigkeit, Puls und Trittfrequenz kommen (GPX oder nicht
   vorhanden); wo sie fehlen, lassen sie sich per Häkchen emulieren.
3. „Abspielen" startet die Fahrt am Anfang bzw. an der zuletzt gewählten Stelle; die
   Navigation startet dabei von selbst. Ins Höhenprofil tippen oder ziehen springt an die
   Stelle, „« Manöver" / „Manöver »" springt 400 m vor das vorige/nächste Manöver.
   „Stopp" beendet die Testfahrt, der echte GPS-Fix gilt wieder.
4. Sie läuft in Echtzeit.

Ohne Fahrradcomputer lässt sich der BLE-Teil mit jeder BLE-Scanner-App prüfen, z. B. nRF
Connect -- UUIDs und Frame-Format stehen im [BLE-Protokoll](PROTOCOL.md).

## Mehr

- Quellcode, Bauen, Design und Lizenz:
  [github.com/euphi/TrailBridge](https://github.com/euphi/TrailBridge)
- TrailBridge steht unter der GPL-3.0-or-later.
