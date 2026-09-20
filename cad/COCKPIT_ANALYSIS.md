# Canyon CP0007 Cockpit — Fotoanalyse als Grundlage für das neue Gehäuse

Auswertung der 19 Fotos aus `cad/TEMP_Fotos/` (aufgenommen 2026-09-19).
Zweck: belastbare Geometriegrundlage für den Gehäuse-Neuentwurf in
`dimensions.py` / `model.py`.

Jeder Wert ist mit einer Konfidenz markiert:
**[GEMESSEN]** = direkt aus Messmittel im Bild abgelesen ·
**[GESCHÄTZT]** = aus Bildproportionen abgeleitet, mit Messschieber zu prüfen ·
**[VORGABE]** = vom Nutzer genannt

---

## 1. Identifiziertes Bauteil

Aufdruck auf dem Vorbau, lesbar in `IMG_3908.JPG` **[GEMESSEN]**:

```
CP0007
GRAVEL COCKPIT CF
STEM SIZE L
WIDTH  440 mm
REACH   70 mm
DROP   130 mm
FLARE  7.5°
CATEGORY 1
```

Einteiliges Carbon-Cockpit ("Hover Bar" / Double-Decker). Stem Size L
entspricht laut Canyon-Angaben ca. 90 mm Vorbaulänge. Steuerrohr/Gabelschaft
ist 1 1/4" (nicht 1 1/8"), d.h. Schaftklemmung ca. Ø31,75 mm.

## 2. Aufbau des Cockpits (entscheidend fürs Gehäuse)

Von oben nach unten, in Fahrrad-Koordinaten (+X vorne, +Y links, +Z oben):

1. **Oberer Lenkerbügel ("Tops")** — flaches Aero-Profil, läuft in Y
   (links/rechts). Sitzt **vorne** und schwebt frei über dem Vorbau.
2. **Hover-Spalt** — offener Zwischenraum, hier verlaufen die Züge.
3. **Vorbau ("Trapez")** — breites, flaches Carbonteil, läuft in X
   (vorne/hinten), vom Gabelschaft nach vorne zum Lenkeranschluss.
   **Hier sitzt das Gehäuse.**
4. **Schaftklemmung** — runder Kragen um den Gabelschaft, darunter Steuerrohr.

### FLEX AREA — kritisch

Auf dem oberen Bügel steht beidseitig neben der Mitte aufgedruckt
`> FLEX AREA <` (lesbar in `IMG_3904`, `IMG_3912`) **[GEMESSEN]**.
Das ist Canyons Kennzeichnung der bewusst nachgiebigen Zone. In der Seitenansicht
(`IMG_3915`, `IMG_3917`) ist gut zu sehen, dass der Bügel dort zu einem dünnen
flachen Blatt auslaufend wird.

→ **Dort darf nichts geklemmt werden.** Das Gehäuse muss sich am Vorbau
abstützen, nicht am oberen Bügel. Zusätzlich muss nach oben Luft bleiben,
weil der Bügel unter Last nach unten durchfedert.

## 3. Maße

### Gemessen

| Maß | Wert | Quelle |
|---|---|---|
| Lichter Abstand Unterkante Tops-Bügel → Oberkante Vorbau (am Lenkeranschluss, vorne) | **33,56 mm** | Messschieber, `IMG_3906` **[GEMESSEN]** |
| Höhe des Tops-Bügels (Z) im Mittelbereich | ca. **16 mm** | Zollstock, `IMG_3905` **[GEMESSEN]** |
| Abstand Oberkante Vorbau (hinten, an der Schaftklemmung) → Unterkante Tops-Bügel | ca. **57 mm** | Zollstock, `IMG_3905` **[GEMESSEN]** |
| Oberkante Vorbau → Schaftklemmschraube | ca. **90 mm** | Zollstock, `IMG_3902` **[GEMESSEN]** |

**Wichtige Folgerung aus 33,56 mm vorne vs. ~57 mm hinten:** Der Spalt wird nach
hinten deutlich größer, weil der Vorbau zum Gabelschaft hin abfällt und der
Tops-Bügel vorne sitzt. **Hinter dem Lenkeranschluss ist über dem Vorbau nach
oben offener Raum** — kein Bügel mehr darüber.

Das ist der eigentliche Grund, warum der Versatz nach hinten funktioniert:
dort ist Platz in Z *und* das Display rückt näher ans Auge.

### Geschätzt — mit Messschieber zu prüfen

| Maß | Schätzung | Anmerkung |
|---|---|---|
| Tiefe (X) des Tops-Bügels in der Mitte | ca. 33 mm | evtl. ist der 33,56-Messwert genau dies statt des Spalts — bitte klären |
| Vorbau-Breite (Y) an der Schaftklemmung | ca. 50–55 mm | **[GESCHÄTZT]** |
| Vorbau-Breite (Y) vorne am Lenkeranschluss | ca. 40–45 mm | **[GESCHÄTZT]** |
| Vorbau-Höhe (Z) hinten | ca. 30–34 mm | deckt sich mit der Nutzervorgabe 32 mm |
| Vorbau-Höhe (Z) vorne | ca. 25–30 mm | **[GESCHÄTZT]** |

Der Vorbau-Querschnitt ist **kein Trapez**, sondern eine gerundete, sich
verändernde Aero-Form. Trapez = bewusste grobe Näherung (Nutzervorgabe).

### Vorgaben des Nutzers **[VORGABE]**

| Zone | Budget |
|---|---|
| Oberteil (Display **inkl. Sonnenschutz**) | max. 48 mm, plus Reserve weil die Flex-Zone durchfedert |
| Vorbau-Zone (Zwischenstück umschließt ihn) | ca. 32 mm |
| Unterteil (Akku + Verschraubung) | max. 18 mm |

Weitere Vorgaben:
- Schmaler in **Y** — Breite ≈ Displaybreite + Stabilität. Das ist der Grund,
  warum der Akku von der Seite nach unten wandert.
- In **X** nach **hinten** verlängerbar; leichter Versatz nach hinten sogar
  erwünscht (Sichtbarkeit).
- **Oberkante tiefer** als bisher. Gesamthöhe darf dabei wachsen — aber nach unten.
- Sonnenschutz über dem Display kann etwa so bleiben wie bisher.

## 4. Das alte Gehäuse und das Kollisionsproblem

Sichtbar in `IMG_3909`, `IMG_3911`, `IMG_3912`, `IMG_3918`: das alte runde
Gehäuse sitzt als Zylinder rittlings auf dem Vorbau, direkt hinter dem
Lenkeranschluss, Oberkante fast am Tops-Bügel.

Problem laut Nutzer: Das Gehäuse dreht beim Lenken mit. Ein **in Y breites**
Gehäuse, das nach hinten ragt, überstreicht beim Einschlagen einen weiten
Bogen und kollidiert mit Oberrohr/Rahmen. In `IMG_3914` ist ein leicht
eingeschlagener Lenker zu sehen — beim Wenden geht der Einschlag deutlich weiter.

→ **Schmal in Y ist die Voraussetzung dafür, in X nach hinten bauen zu dürfen.**
Der überstrichene Bogen der hinteren Ecken ist das Kriterium, nicht die Länge allein.

## 5. Konsequenzen für die Konstruktion

1. Abstützung **ausschließlich am Vorbau**, nie am Tops-Bügel (FLEX AREA).
2. Vorbau-Ausschnitt als **Loft über Querschnitts-Skizzen** (zunächst Trapeze),
   damit die Form später ohne Umbau verfeinert werden kann.
3. Keine harte Klemmung auf Carbon — Übermaß im Ausschnitt plus
   **Moosgummi/Isoliermaterial** als weiche Zwischenlage (Nutzervorgabe).
4. Gehäuse schwerpunktmäßig **hinter** den Lenkeranschluss legen, dort ist
   nach oben Luft und der Einbauraum am größten.
5. Y so schmal wie möglich halten — das ist gleichzeitig Bau- und
   Lenkfreiheitsbedingung.
6. Verschraubung von unten durch den Stapel, damit oben keine Schraubenköpfe
   sichtbar sind und der Akku von unten zugänglich bleibt.

## 6. Dritter Entwurf (aktuell)

Positionen aus dem vom Nutzer im STEP verschobenen Stand übernommen:
Displaymitte **X +11** (aus der Deckelposition), Träger und Akku je
**X +8 … +64** (aus der Akkuposition).

**Fünf benannte Teile in einer Datei** `cad/export/TRGB_Gehaeuse.step`:
`traeger`, `display_gehaeuse`, `deckel`, `akku_wanne`, `akku_deckel`.
Das Displaygehäuse ist jetzt vom Träger **getrennt** — beides lässt sich so
in der jeweils günstigsten Lage und ohne Stützmaterial drucken.

### Verschraubung: M3 mit Heatset-Inserts

Kein Gewinde mehr direkt im Druckteil. Drei Schraubenfamilien, alle geprüft:

| Familie | Weg | Buchse sitzt in |
|---|---|---|
| Stapel (4×) | akku_deckel (Senkkopf) → akku_wanne → Wange | `traeger` |
| Becher (2×) | von unten durch die Trägerplatte | `display_gehaeuse` |
| Deckel (3×) | von oben durch den Deckel | `display_gehaeuse` |

Maßgeblich für die Wandstärke ist nicht die Schraube, sondern die Buchse:
Bohrung Ø 4,2 + 2 × 2,4 mm Wand = **9,0 mm Mindestdicke** an jeder
Verschraubung (`INSERT_BOSS_D`). Wo die Wand das nicht hergibt (Becher, Deckel),
sitzen Augen außen an statt die ganze Wand zu verdicken.

**Folge für die Breite:** Der Träger wächst dadurch auf **69,4 mm**
(= Ausschnitt 56,0 + 2 × 9,0 Buchsendom). Vorher waren es 62,5 mm mit
direkt geschnittenem Gewinde. Der Hebel bleibt derselbe wie bisher: jeder
Millimeter, den der echte Vorbau schmaler ist als geschätzt, geht direkt ab.

### Steuerrohr-Freihaltung — offener Konflikt

Der Akku sitzt jetzt auf X +8 … +64. Gegen meine **geschätzte** Freihaltung
(Steuersatz-Oberkante −32 mm, Radius 28 mm) ragt er mit ca. 5,4 cm³ hinein.

Das ist bewusst nur ein **Hinweis**, kein Fehler: Du hast die Position am
realen Rad gewählt, meine beiden Zahlen sind geraten. Zu messen wären der
Durchmesser des Steuersatz-Deckels und sein Abstand unter der
Vorbau-Unterkante. Stimmen meine Annahmen, müsste der Akku nach vorne;
liegt der Steuersatz tiefer, passt alles.

### Selbsttest

Läuft bei jedem Export: 9 Stichpunkte (Ausschnitt, Wangen, Platte,
Displaytasche, Becherwand, Sichtfenster) plus alle 9 Verschraubungen — jede
Schraube muss durch alle Teile darüber und darunter frei durchgehen und in
genau einer Buchse enden.

## 6a. Zweiter Entwurf (überholt)

Nach Rückmeldung umgebaut. Export: **eine** Datei `cad/export/TRGB_Gehaeuse.step`
mit vier benannten Teilen (`traeger`, `deckel`, `akku_wanne`, `akku_deckel`)
unter der Baugruppe `TRGB_Gehaeuse_CP0007`. Kein STL mehr.

### Rundes Display

Maße aus dem LilyGO-STEP (`references/T-RGB-FULL-3D-2.1-Inches.stp`),
selbst ausgemessen: **Ø 59,5 mm × 18,3 mm hoch**, Körper darunter Ø 56,4 mm,
einseitiger Überstand ca. 2,3 mm, Stecker unten bei z −11 … −14,3.
Gehäuse außen damit Ø 65,1 mm.

### Trägerstruktur

Wie besprochen: das Trapez läuft **voll durch** den Träger, es bleibt also kein
Material davor oder dahinter. Übrig bleiben zwei Wangen links/rechts des
Vorbaus, oben verbunden durch eine **4 mm dünne Platte** über dem Vorbau.
Auf dieser Platte sitzt das runde Displaygehäuse, das nach hinten über das
Steuerrohr auskragt.

### Steuerrohr-Freihaltung

Formuliert als **Zylinder um die Schaftachse** unterhalb der Steuersatz-Oberkante
(`headtube_keepout()`). Da das Gehäuse um genau diese Achse dreht, gilt die
Bedingung automatisch für jeden Lenkeinschlag — auch den vollen. Der Selbsttest
prüft jedes Teil dagegen.

Daraus folgt die Aufteilung: der Träger klammert nur **vor** der Schaftachse
(X +28 … +64), der Akku hängt ebenfalls davor (X +30 … +86), und nur das
Displaygehäuse ragt nach hinten — dort aber hoch genug, um frei zu schwenken.

| Maß | Entwurf | Vorgabe |
|---|---|---|
| Oberkante (Sonnenschutz) | **+38,3 mm** | max. 48 ✓ |
| Akku unter dem Vorbau | 18,0 mm | max. 18 ✓ |
| Trägerbreite Y | 62,5 mm | so schmal wie möglich |
| Displaygehäuse Ø | 65,1 mm | durch Modul Ø 59,5 bestimmt |
| Auskragung nach hinten | bis X −58,5 | erwünscht ✓ |

Die Oberkante liegt jetzt bei +38,3 mm statt +42 mm im ersten Entwurf; das alte
Gehäuse reichte praktisch bis an den Bügel.

### Offen bei diesem Entwurf

- Das Displaygehäuse kragt ca. 85 mm hinter den Klammerbereich aus. Das ist
  konstruktiv der heikelste Punkt (Hebel, Vibration) — evtl. braucht es eine
  zweite Abstützung oder eine dickere Verbindungsplatte.
- `HEADSET_TOP_Z` (−32 mm) und `HEADTUBE_KEEPOUT_R` (28 mm) sind **geschätzt**.
  Beide bestimmen direkt, wie weit vorne Träger und Akku sitzen müssen.
- Der Bauraum **unter** dem Vorbau ist auf keinem Foto zu sehen und damit
  ungeprüft — dort liegt jetzt der Akku.

## 6b. Erster Entwurf (überholt)

Umgesetzt in `dimensions.py` / `model.py`, Export nach `cad/export/`.
Vier Druckteile: `traeger` (Displayträger + Zwischenstück in einem),
`deckel` (Sonnenschutz), `akku_wanne`, `akku_deckel`.

| Maß | Entwurf | Vorgabe |
|---|---|---|
| Oberteil über Vorbau (inkl. Sonnenschutz) | 42,0 mm | max. 48 mm ✓ |
| Unterteil unter Vorbau | 18,0 mm | max. 18 mm ✓ |
| Länge X | 90 mm (−70 … +20) | nach hinten verlängerbar ✓ |
| Breite Y | **67,6 mm** | Wunsch 60,8 mm ✗ |
| Gesamthöhe | 92 mm | darf nach unten wachsen ✓ |

Zum Vergleich das alte Gehäuse: 103 × 115 × 58 mm. In **Y** — der für die
Lenkfreiheit entscheidenden Richtung — also 115 → 67,6 mm, das sind **41 %
schmaler**. Die Oberkante liegt bei +42 mm statt vorher praktisch am Bügel.

### Warum 67,6 mm statt der gewünschten 60,8 mm

Nicht das Display bestimmt die Breite, sondern der Vorbau: breitester
angenommener Querschnitt 50 mm + 2 × 3 mm Moosgummi = 56 mm Ausschnitt, und
seitlich daneben müssen noch die Schraubkanäle durch.

Das Modell rechnet das selbst aus (`case_width_y()` in `model.py`) und meldet
die Aufweitung beim Ausführen. Es wird also automatisch schmaler, sobald die
echten Vorbau-Maße eingetragen sind — **jeder Millimeter, den der Vorbau
schmaler ist als angenommen, macht das Gehäuse einen Millimeter schmaler.**
Deshalb ist Messpunkt 2 unten der mit Abstand lohnendste.

Zwei Hebel, falls es noch schmaler werden muss:
- Moosgummi dünner (2 mm statt 3 mm) → ca. 2 mm schmaler
- vorderes Schraubenpaar entfallen lassen und stattdessen eine Rastnase/Klemmung
  vorne — dann bestimmt nur noch der Ausschnitt die Breite

### Selbsttest

`model.py` prüft bei jedem Lauf per Punkt-im-Volumen-Stichproben, dass
Ausschnitt, Displaytasche und Vollmaterial dort sind, wo sie hingehören.
Beim Ändern der `STEM_SECTIONS` fällt so sofort auf, wenn die Geometrie kippt.

### Was bewusst noch fehlt

Fasen/Radien, Hohlkammern (der `traeger` ist noch weitgehend massiv und damit
unnötig schwer), Taster-/Kabelausschnitte im Detail, Dichtungsnut.
Das ergibt erst Sinn, wenn die Vorbau-Geometrie steht.

## 7. Offene Punkte für die nächste Messrunde

1. **33,56 mm** — ist das der lichte Spalt Bügel↔Vorbau oder die X-Tiefe des
   Bügels? Das ändert die nutzbare Bauhöhe vorne erheblich.
2. Vorbau-Breite (Y) an 3–4 X-Stationen, jeweils oben und unten gemessen
   (für die Loft-Querschnitte).
3. Vorbau-Höhe (Z) an denselben Stationen.
4. Wie weit darf es nach hinten gehen, bevor beim vollen Lenkeinschlag das
   Oberrohr berührt wird? Am besten am Rad mit eingeschlagenem Lenker
   nachmessen — das ist die eigentliche Grenze für die X-Länge.
5. Freiraum unter dem Vorbau für den Akku (bis Steuerrohr/Zugverlegung).
