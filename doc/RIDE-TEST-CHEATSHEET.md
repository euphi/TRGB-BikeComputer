# Testfahrt-Cheatsheet: Ride-Zustandsautomat und Statistik

Für die erste echte Fahrt mit `feature/ride-state-machine`. Unterwegs gibt es
nur Tap, Long-Press und Wischen. Die Durchschnittswerte stehen nicht auf dem
Display, du prüfst sie zu Hause auf `/stat/statistics.html`. Unterwegs
kontrollierst du deshalb nur, was du **sehen** kannst, und notierst die
**Uhrzeit** jeder Aktion (z. B. als Sprachnotiz auf dem Handy).

Modell: [`design/ride-state-machine.md`](design/ride-state-machine.md).

## Was du siehst

| Element | Bedeutung |
|---|---|
| StateIcon (über der RQ-Linie) | Pedaling / Coasting (Ride), FreeRide (auch Cruise), Stop, Break (> 2 min), Kettenglied unterbrochen (kein Speed-Sensor) |
| Taste unten | **Start-Pfeil** = gerade nicht im Ride-Modus, **Pause-Symbol** = Ride-Modus |
| Zeit-Widget | **Uhr-Icon + Uhrzeit** = keine Session offen, **Stoppuhr + Laufzeit** = Session läuft |
| Stoppuhr-Wert | Session-Zeit **mit** Stops und Breaks, **ohne** Zeit ohne Sensor |
| Distanzfeld mit Label | Wischen links/rechts: `START` / `TRIP` / `TOUR` / `TOTAL`. `START` = aktuelle Session |
| StateIcon-Farbe bei Break/kein Sensor | **grün** = Auto-Off aus, neutral = Auto-Off an (blinkt in der letzten Minute) |

Wischen hoch/runter wechselt die Ø-Art, hat auf RimRidge aber noch kein
Anzeige-Widget. Du siehst davon also nichts.

## Bedienung

| Geste | Keine Session offen | Session offen |
|---|---|---|
| **Tap** Start/Pause | Neue Session: `START` auf 0, Stoppuhr ab 0:00, Ride-Modus | Ride ↔ Cruise umschalten, Session bleibt offen |
| **Long-Press** Start/Pause | ⚠ Auto-Off umschalten (**kein** Stop!) | Session beenden (Stop) |

Achtung: Ein Long-Press ohne offene Session schaltet den Auto-Off **ein**. Das
erkennst du bei Break am neutralen statt grünen Icon. Ein zweiter Long-Press
schaltet ihn wieder aus.

## Vorbereitung zu Hause

1. `/stat/statistics.html` öffnen und einen **Screenshot** machen. Die Spalten
   Total, Tour und Trip dienen als Ausgangsstand. Nach der Fahrt muss die
   Differenz genau diese Fahrt sein.
2. Optional das Textlog auf der SD einschalten, per Serial:
   `showloglevel` prüfen, sonst `loglevel STAT INFO -file`. Danach stehen
   `Ride session started/stopped` und `Connected/Disconnected from speed
   sensor` mit Zeitstempel auf der Karte.
3. Das Handy mit TrailBridge mitnehmen. GPS im Binärlog ist der unabhängige
   Vergleich für Distanz und Geschwindigkeit.

## Szenarien

Die Reihenfolge ergibt eine sinnvolle Tour. Rechts steht, was das Display
zeigen muss.

| # | Aktion | Erwartung |
|---|---|---|
| 1 | Einschalten, losfahren **ohne** Tap | FreeRide-Icon, Start-Pfeil, **Uhrzeit** (keine Stoppuhr), `START` = 0,0 km. `TRIP` zählt trotzdem |
| 2 | Während der Fahrt **Tap** | Pedaling-Icon, Pause-Symbol, Stoppuhr ab 0:00, `START` ab 0 |
| 3 | Bei > 5,5 km/h Beine still halten (rollen) | Coasting-Icon (< 40 rpm), bei wieder > 50 rpm Pedaling. Stoppuhr läuft einfach weiter |
| 4 | Ampel, **< 2 min** stehen | Stop-Icon, Stoppuhr läuft weiter. Beim Anfahren (> 5,5 km/h) wieder Pedaling |
| 5 | **Tap** während der Fahrt (Cruise) | FreeRide-Icon, Start-Pfeil, **Stoppuhr läuft weiter**, `START` zählt weiter. Ein paar hundert Meter fahren, Uhrzeit notieren |
| 6 | Im Cruise anhalten, im Stand **Tap**, dann anfahren | Im Stand: Stop-Icon, Taste wechselt auf Pause. Beim Anfahren: Pedaling (Ride), nicht FreeRide |
| 7 | Pause **> 2 min** | Nach 2 min Break-Icon (grün, solange Auto-Off aus ist). Stoppuhr läuft weiter |
| 8 | **Sensor weg**: in der Pause warten, bis der Speed-Sensor schläft, oder den BC kurz aus- und nach ein paar hundert Metern wieder einschalten | Kettenglied-Icon. **Stoppuhr steht still**. Nach dem Reconnect springt die Distanz um die Strecke ohne Sensor |
| 9 | **Long-Press** während der Fahrt | Uhrzeit statt Stoppuhr, FreeRide-Icon, Start-Pfeil. `START` bleibt auf dem Endstand stehen |
| 10 | Danach **Tap** | Neue Session: `START` = 0, Stoppuhr 0:00 |

Wichtig bei Szenario 8 mit Aus- und Einschalten: Die Ride-Session lebt nur im
RAM. Nach einem Neustart ist keine Session offen (Uhrzeit statt Stoppuhr) und
die Ride-Spalte ist leer. Tour, Trip und Total bleiben erhalten: Sie werden
alle 5 min, bei jedem Anhalten und vor dem Ausschalten im NVS gespeichert.
Das ist bekanntes Verhalten, kein Fehler. Den
BC-Neustart deshalb erst am Ende der Tour oder nach Szenario 10 testen.

## Nach der Fahrt zu Hause

Auf `/stat/statistics.html`:

- **Differenz zum Screenshot** in Tour und Trip ≈ gefahrene Strecke. Total
  steigt um denselben Wert.
- **Ride-Spalte** (letzte Session): Die Fahrzeit („Moving“) muss zu deinen
  Notizen passen. Stops und Breaks tauchen getrennt auf.
- **„of it cruise“** (Distanz und Zeit) ≈ die Strecke aus Szenario 5.
- **„without sensor“** ≈ die Strecke aus Szenario 8. Sonst sollte der Wert
  0 oder sehr klein sein, denn der erste Zählerstand nach dem Connect
  landet dort.
- **Ø-Werte**: „Moving“ ≥ „incl. stops“ ≥ „incl. stops and breaks“. „Ride
  mode only“ lässt die Cruise-Strecke aus Szenario 5 weg.
- **Max speed** ist plausibel (≤ 120 km/h werden gespeichert, mehr wird
  verworfen) und stimmt ungefähr mit GPS überein.
- **Cadence** (Ø beim Treten) liegt im üblichen Bereich, typisch 60 bis 90 rpm.

GPX: `bikelog gpx -i L_….bin -o test.gpx --ride-states`. Jeder Zustandswechsel beginnt ein
neues `<trkseg>` mit `<bc:RideState>` (Fahrt, Rollen, Cruise, FreeRide,
Stopp, Pause, Getrennt). Die Segmente sollten zu deinen notierten Uhrzeiten
passen.

Gegenprobe über das Binärlog (`L_*.bin`, Reader in `Tools/bikelog/`): Alle
5 s stehen dort `speed`, `dist_m` (seit dem Einschalten) und GPS. Zeiten mit
`speed > 0` ergeben die Fahrzeit, der `dist_m`-Endstand die Distanz. Das
Textlog (falls in der Vorbereitung eingeschaltet) ordnet Session-Start und
-Stop sowie Connect und Disconnect den notierten Uhrzeiten zu.

## Bekannte Lücken, auf die du stoßen kannst

- Auto-Off lässt sich mit offener Session nicht umschalten, weil der
  Long-Press dann die Session beendet. Standardmäßig ist Auto-Off beim Boot
  aus.
- Ohne Cadence-Sensor steht das Icon im Ride-Modus immer auf Coasting (siehe
  Design-Doc §7).
- Ein manuell gesetzter Gesamtkilometerstand hebt den Total-Durchschnitt an.
