# Wegequalität

Eine Spezialfunktion neben der Navigation: Der Fahrradcomputer misst, wie rau der Weg
ist, und zeichnet das entlang der Fahrt auf. Die Daten kommen von einem
Beschleunigungssensor BMI160 (IMU) im Gehäuse, ausgelesen mit 400 Hz. Er ist optional --
ohne den Sensor funktioniert alles andere.

Wozu: hinterher sehen, wo eine Strecke glatt war und wo nicht, die schlimmsten Stellen
finden (Schlaglöcher, Bordsteine) und auf lange Sicht Daten zum Untergrund aus vielen
Fahrten sammeln.

## Was gemessen wird

- **Rauheitsklasse 1 bis 5** je Intervall (Standard 2 s): 1 = glatt, 5 = sehr rau. Sie
  ist die senkrechte Vibration zwischen 2 und 80 Hz, auf die Geschwindigkeit bezogen,
  im Verhältnis zu einem Grundwert für glatten Asphalt. Unter 6 km/h wird nicht bewertet.
- **Stöße**: einzelne harte Schläge (Standard ab 3 g), mit ihrer Stärke. Ein zweiter
  Peak kurz danach wird als das Hinterrad an derselben Kante erkannt.
- **Steigung aus dem Beschleunigungssensor** als Alternative zum Barometer. Sie reagiert
  in etwa einer Sekunde. Standardmäßig aus (`rq gradsrc imu` schaltet sie ein); die
  Anzeige nutzt das Barometer.

Auf dem Display ist die Klasse die Farbe der dünnen Linie am unteren Rand jedes Screens:
blau = glatt bis rot = sehr rau, Messing = gerade nicht bewertet.

## Kalibrierung

Beides steht auf dem Einstellungs-Screen, Seite „IMU":

- **Kalibrieren** (Rad steht still und aufrecht): speichert die Einbaulage des Sensors.
- **Referenzfahrt**: 60 s auf glattem Asphalt mit mindestens 12 km/h. Sie legt den
  Grundwert fest, an dem „glatt" gemessen wird. Er hängt von Rad und Reifendruck ab, also
  nach einer Änderung wiederholen. Ohne Referenzfahrt gilt ein Standardwert, und die
  Klassen sind nur ein grober Anhalt.

## Wege-Labels: den Untergrund von Hand markieren

![Wege-Label-Screen](screenshots/roadlabels.png){ width="240" }

Ein eigener Screen, um Vergleichsdaten zu sammeln: Während der Fahrt markierst du den
Untergrund (Asphalt, Schotter, Waldweg, Feldweg, Pflaster, Sonstiges) und deine eigene
Bewertung von 1 bis 4. Die Labels werden neben der automatischen Klasse aufgezeichnet;
so lassen sich beide hinterher vergleichen und die Schwellen abstimmen. Ein Tipp auf das
Fahrzustand-Icon am unteren Rand eines beliebigen Screens öffnet ihn, derselbe Tipp führt
zurück.

Die runde Taste in der Mitte startet einen Rohmitschnitt des Beschleunigungssensors
(alle Messwerte, etwa 150 KB je Minute) für einen Abschnitt, den du genau auswerten
willst.

Was an diesem Screen auf schlechtem Weg noch hakt, steht unter
[Bedienung unterwegs](USABILITY-TODO.md).

## Was im Log landet

- je Intervall: Klasse, Rauheit, Vibrationswerte, Geschwindigkeit, Strecke
- jeder Stoß mit Position und Stärke, dazu ein kurzer Rohdaten-Ausschnitt um ihn herum
- jeder Wechsel des manuellen Labels
- Rohmitschnitte auf Anforderung

Die [Werkzeuge](TOOLS.md) machen daraus CSV und GPX: Jeder Trackpunkt trägt Klasse und
Rauheit, Stöße und Label-Wechsel werden Wegpunkte. `bikelog raw replay` schickt
aufgezeichnete Rohdaten mit geänderten Parametern durch den Algorithmus der Firmware
selbst; so sollen die Schwellen abgestimmt werden.

## Einstellungen und Diagnose

```
rq                      Zustand und Einstellungen
rq interval 2           Länge eines Intervalls in s (1..10)
rq shock 3              Stoß-Schwelle in g
rq wheelbase 1.05       Radstand in m (Zuordnung Vorder-/Hinterrad bei Stößen)
rq gradsrc baro|imu     Quelle der angezeigten Steigung
rq ref start|stop       Referenzfahrt
rq raw 120 | stop       Rohmitschnitt für 120 s
rq label 2,3            Label von Hand: Untergrund 2 (Schotter), Qualität 3
```

Ohne USB: Die Seite `/debug/imu` zeigt Zustand, Kalibrierung, die letzten Intervalle und
Stöße und die Fehlerzähler des Sensors.

## Stand

Die Schwellen (Klassengrenzen, Stoß-Schwelle, Standard-Grundwert) sind erste Schätzwerte
aus der Literatur und einem Host-Test und noch nicht an aufgezeichneten Fahrten
abgestimmt. Siehe [Roadmap](ROADMAP.md).

Für Entwickler: Der Algorithmus ist `src/RoadQuality.*` (rein, Host-Test
`test/native_roadquality/`), die Anbindung `src/I2CSensors.cpp`; die Satzlayouts stehen in
[`src/LogRecords.h`](https://github.com/euphi/TRGB-BikeComputer/blob/main/src/LogRecords.h)
und [`src/RawCapture.h`](https://github.com/euphi/TRGB-BikeComputer/blob/main/src/RawCapture.h).
