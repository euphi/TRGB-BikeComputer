# Canyon CP0007 Cockpit — Fotoanalyse als Grundlage für das neue Gehäuse

Auswertung der 19 Fotos aus `cad/TEMP_Fotos/` (aufgenommen 2026-09-19),
ergänzt um die Anprobe am Rad vom 2026-10-03.
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
4. **Schaftklemmung** — runder Kragen um den Gabelschaft.
5. **Übergang zum Steuerrohr** — unter dem Vorbau, gehört zum Vorbau und dreht
   beim Lenken mit, 18 mm hoch **[VORGABE]**.
6. **Steuerrohr** — fest am Rahmen, läuft unter dem Display mit auffallend
   großem Winkel nach vorne unten **[VORGABE]**.
7. **Lampenhalter** — inzwischen unter dem Vorbau montiert, 50 mm vor dem
   Steuerrohr **[VORGABE]**.

Diese Teile sind als Bezugskörper `ref_*` im Modell (`model.reference_solids()`)
und im STEP: Vorbau, Lenker mit Übergang, Oberlenker, Übergang zum Steuerrohr,
Steuerrohr, Lampenhalter, dazu Displaymodul und Akku. Der Selbsttest prüft die
Druckteile gegen jeden davon. Gezeichnet sind sie nur so genau, wie die Maße
unten es hergeben — Form und Lage von Steuerrohr, Lenkerübergang und
Lampenhalter sind geschätzt.

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

### Nachgemessen am Rad (2026-10-03) **[VORGABE]**

| Maß | Wert |
|---|---|
| Vorbau Breite | 50,6 mm (Ausschnitt 51) |
| Vorbau Höhe | 33,2 mm nah am Steuerrohr, 31 mm vorne (Ausschnitt 35 mit Moosgummi) |
| Vorbau Kanten | oben und unten verrundet, oben enger (Modell: oben R 5, unten Kurve R 5 → R 10) |
| Unter dem Vorbau frei zwischen Steuerrohr und Lampenhalter | **50 mm** |
| Nutzbare Tiefe unter dem Vorbau | **18 mm** (Höhe des mitdrehenden Übergangs; darunter steht das Steuerrohr) |
| Oben nach vorne | höchstens **50 mm vor das Steuerrohr** — weiter vorne verdecken Oberlenker und Leitungen die Sicht |
| Oben nach oben | Sonnenschutz darf höher werden, der Oberlenker ist dort nicht mehr im Weg |

### Geschätzt — mit Messschieber zu prüfen

| Maß | Schätzung | Anmerkung |
|---|---|---|
| Tiefe (X) des Tops-Bügels in der Mitte | ca. 33 mm | evtl. ist der 33,56-Messwert genau dies statt des Spalts — bitte klären |
| Hinteres Vorbau-Ende ab Schaftachse | ca. 22 mm | Bezug für alle X-Lagen (`STEM_REAR_END_X`) |
| Wo „das Steuerrohr“ in X beginnt | X +13,5 (dort begannen bisher die Wangen) | Bezug für die 50 mm oben und unten (`TRAEGER_X_MIN`, `FRONT_LIMIT_X` = +63,5) |
| Steuerrohr: Lage in X, Winkel zur Senkrechten auf den Vorbau | Scheitel X +25, 6° | Kontur ist Vorgabe (50 breit, vorne R 25); Lage geschätzt (`HEADTUBE_CENTER_X`, `STEERER_TILT_DEG`) |
| Lampenhalter | 30 × 30 mm, 18 tief, ab X +63,5 | nur Zeichnung |
| Lenker: Hinterkante ab Schaftachse | 88 mm | nur Zeichnung (`STEM_TO_BAR_X`); Höhe aus den beiden Messwerten zum Oberlenker |

Die früheren Foto-Schätzungen für den Vorbau (50–55 mm breit hinten, nach vorne
ansteigend) sind durch das FreeCAD-Modell des Nutzers ersetzt — siehe unten.

Der Vorbau-Querschnitt ist **kein Trapez**, sondern eine gerundete, sich
verändernde Aero-Form. Trapez = bewusste grobe Näherung (Nutzervorgabe).

### Vorbau laut FreeCAD-Modell des Nutzers **[VORGABE]**

Körper „Vorbau/Cockpit“ in `FreeCAD_SimpleModel.FCStd`: **32 mm hoch**, in der
Draufsicht **40 mm breit** am hinteren Ende (Ecken R10) und über 150 mm
gleichmäßig auf **35 mm** verjüngt, Oberseite eben. Das ist deutlich schmaler
als aus den Fotos geschätzt. **Überholt:** am Rad nachgemessen sind es
50,6 × 33,2 mm (Abschnitt 6); aus dem Modell gelten nur noch die X-Lagen.

Das Modell liegt in FreeCAD auf dem Rücken (Display unten, Akku oben), also
180° um Y gedreht. Umrechnung: `x = STEM_REAR_END_X + 25 − x_freecad`, Z
gespiegelt. Daraus übernommen, jeweils ab dem hinteren Vorbau-Ende:
Displaymitte 40,5 mm davor, Beginn der Wangen 35,5 mm davor.

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
6. Verschraubung von unten, damit oben keine Schraubenköpfe sichtbar sind.
7. **Unter dem Vorbau nur das Nötigste** (50 × 18 mm Raum): der Akku liegt
   oben, unten bleibt eine Klemmplatte.
8. **Nichts vor X +63,5** — weder oben noch unten.

## 6. Sechster Entwurf (aktuell)

Anlass: Anprobe am Rad (siehe „Nachgemessen am Rad“). Unter dem Vorbau ist nur
ein Raum von 50 × 18 mm frei, der Akku passt dort nicht; nach vorne geht es
weder unten (Lampenhalter) noch oben (Sicht) weiter als 50 mm vor das
Steuerrohr. Der fünfte Entwurf (Akkufach im Träger, Display Ø 88,8, M3×30) war
ein Zwischenstand vom selben Tag und wurde nicht gedruckt; geändert wurde
danach: Träger und Displaygehäuse mit ebener Fuge getrennt, Gehäuse kleiner,
M2-Schrauben, Sensorfach vorne, offene Taschen.

Vier Druckteile, von oben nach unten: `deckel`, `display_gehaeuse`, `traeger`,
`unterseite`.

### Aufbau

- **Fuge eben, 2,7° geneigt (vorne höher):** Die Keilplatte des Trägers ist
  hinten 6 mm und vorne 10 mm dick **[VORGABE]**; das Displaygehäuse sitzt mit
  ebener Unterkante darauf.
- **Akku waagerecht in der Keilplatte, 1,4 mm Boden [VORGABE]:** Mulde
  55 × 52,5 mm, mittig unter dem Modul (X −14,4 … +39,6). Weil die Fuge
  geneigt ist, steht der Akku hinten 2,3 mm ins Displaygehäuse und vorne gar
  nicht; dessen Aussparungen für die Akkuecken sind entsprechend flach und
  laufen unter 45° in die runde Tasche aus. Über dem Akku mindestens 2 mm Luft
  für die Stecker unter der Platine, dann das Modul.
- **Displaygehäuse:** unten offener Ring, **21,7 mm hoch**.
- **Träger:** Keilplatte, darunter die Wangen, **45 mm lang** (X +13,5 …
  +58,5) **[VORGABE]**, und die Taschen für Buchse und Schalter. Vorne endet
  alles bei X +58,5; frei wären am Rad 50 mm (bis +63,5).
- **Unterseite:** Klemmplatte 5 mm, 45 mm lang, 5,9 mm unter dem Vorbau
  (erlaubt 18), mit Kurven (R 5 → R 10), je Seite einer Führungszunge
  10 × 3 mm, 10 mm Klemmweg (Ausschnitt 31 … 41 mm hoch, gezeichnet 35 mit
  4 mm Spalt). Vordere Kanten R 5.

### Displaygehäuse so klein wie möglich, keine Schraube sichtbar **[VORGABE]**

Umriss: Rechteck **78,8 × 77,2 mm** mit Eckradien R 24,3. Vorne und hinten
bleiben 1,6 mm Wand neben der Glasstufe, an den Seiten 0,8 mm. Die Glasstufe
ist jetzt **Ø 75,6** (vorher 75,2 — das Glas ging nur knapp hinein).

Alle Schrauben sitzen **in den vier Ecken innerhalb des Umrisses**, außen ist
nichts zu sehen: je Ecke eine Deckelschraube (bei ±57°/±123°, Bohrung von
unten durch das Gehäuse) und eine Buchse für die Schraube aus dem Träger (bei
±33°/±147°), beide auf R 40,8. Der Eckradius ist der größte, bei dem um die
Schrauben noch 1 mm Wand bleibt — kleiner wird der Umriss nur mit dünneren
Wänden oder ohne Schrauben in den Ecken.

Unverändert: Modul hängt am Glasrand, Aussparung für die Displayfahne hinten
(ab Oberkante Akku), Fenster Ø 58 im Deckel, zwei Abluftschlitze hinten.
Deckel **4,8 mm** dick **[VORGABE]** (Buchse in 4 mm Bohrung + 0,8 Deckschicht), Fase 30,5° (oben Ø 71,6). **Sonnenschutz 15 mm**, innen
Ø 72,6 → Ø 60.

### Sensorfach vorne **[VORGABE]**

Platine 13 × 18 mm liegend, 8 mm hoch; an ihrer Vorderkante stehen Stifte und
Kabel weitere 10 mm nach oben (zusammen 18 mm in Z).

- Die Platine sitzt **dicht vor dem Modul** (ab 29,5 mm vor der Displaymitte),
  zum größten Teil in der durchbrochenen Wand des Gehäuses. Vorne steht das
  Fach nur noch **6,1 mm** über (vorher 16).
- Innen 20 breit, 19 hoch über die ganze Fläche.
- **Kabelmulde im Träger:** unter dem Fach, 20 mm breit, bis auf 1,4 mm Boden,
  geht in die Akkumulde über.
- Je zwei Lüftungsschlitze 1,5 × 8 mm in den Seitenwänden (dicht
  nebeneinander, die Seitenwand ist außen nur 6 mm lang).
- Halterung für die Platine fehlt noch.

Die Displaymitte liegt bei X +11,6, das Gehäuse reicht hinten bis X −28,0.

### Taschen für Buchse und Schalter **[VORGABE]**

Außen an den Wangen unter der Keilplatte, Öffnung nach hinten bei X +13,5,
Y ±35,1, Z −7,1. Nur die 3,2 mm dicke Stirnwand trägt die Bohrung (Ø 10,8 bzw.
Ø 12,3); dahinter ist ein Quader 16 × 16 × 16,8 mm frei, in dem sich die Mutter
drehen lässt. Die Taschen stehen seitlich über das Displaygehäuse hinaus
(Breite über beide 89,4 mm).

**Nach oben offen** auf **14,4 × 10,5 mm** je Seite **[VORGABE: so groß wie
möglich]**. Dafür ist die Wand des Displaygehäuses über den Taschen innen bis
auf 1 mm ausgenommen (8 mm hoch, darüber unter 45° geschlossen, die
Glasauflage bleibt). Begrenzt wird die Öffnung vorne durch die Deckelschraube
in der Ecke, außen durch die 1 mm Wand, innen durch die Wand zum Vorbau.
Außerhalb des Gehäuses hat die Tasche ein Dach.

### Lüftung und Ablauf

- **Zuluft:** je ein Kanal Ø 4 von der Vorderseite der Wange (X +57,5,
  Y ±28,7, Z −12, Kante R 1,5), 16° ansteigend, mündet in der Vorderwand der
  Tasche und von dort nach oben ins Gehäuse. Dazu die Schlitze des Sensorfachs.
- **Abluft:** zwei Schlitze 14 × 2,5 mm hinten im Displaygehäuse.
- **Ablauf:** Kerbe 3 × 1,5 mm unten hinten im Displaygehäuse, an der
  tiefsten Stelle der geneigten Fuge.

### Verschraubung **[VORGABE]**

Oben M2 Innensechskant in Heatset-Buchsen M2×3 (Bohrung Ø 3,2 × 4), unten M3.
Alles von unten, von oben nichts sichtbar. Durchgänge: M2 Ø 2,6, M3 Ø 3,8
(vorher 3,4 — zu stramm).

| Schrauben | Weg | Kopf |
|---|---|---|
| 4× M2×10 | Displaygehäuse → Deckel, in den Ecken | 15,5 mm tief im Gehäuse versenkt (Bohrung Ø 4,4 von unten) |
| 2× M2×8 | Träger → Displaygehäuse, hinten | 2,1 mm in der Platte versenkt, bündig |
| 2× M2×10 | Träger → Displaygehäuse, vorne (über dem Vorbau) | 3,3 mm versenkt, vom Ausschnitt aus |
| 4× M3×16 | Unterseite → Wange | 4,0 … 14,0 mm Gewinde je nach Klemmweg |

Die Deckelschrauben brauchen einen langen 1,5-mm-Sechskant. Die vier
Trägerschrauben sitzen über dem Vorbau und sind nur ohne Vorbau erreichbar:
erst Deckel und Gehäuse, dann Gehäuse auf den Träger, dann das Ganze aufs Rad.

### Maße

| Maß | Entwurf | Vorgabe |
|---|---|---|
| Länge | X −28,0 … **+58,5** | vorne max. +63,5 ✓ |
| Unter dem Vorbau | **5,9 mm** | max. 18 ✓ |
| Oberkante Sonnenschutz über Vorbau | **51,7 mm** | — |
| Glas hinten / Mitte / vorne über Vorbau | 28,6 / 30,5 / 32,3 mm | — |
| Breite Displaygehäuse / Wangen / über die Taschen | 77,2 / 66,6 / 89,4 mm | — |
| Volumen (massiv gerechnet) | Träger 43, Gehäuse 32, Deckel 15, Unterseite 16 cm³ | — |

**Druck:** Displaygehäuse aufrecht (Fuge auf dem Bett), Deckel mit der
Unterseite auf dem Bett, Unterseite wie eingebaut, Träger auf dem Kopf mit der
ebenen Oberseite auf dem Bett. Stützen brauchen nur Akku- und Kabelmulde im Träger
**[VORGABE]**; Brücken sind das Dach des Sensorfachs (20 mm), die Böden der
Taschen (16 mm) und die Schlitze.

### Bezugskörper und Farben im STEP

- Der Lenker liegt auf gleicher Höhe wie der Vorbau **[VORGABE]** (Z −31 … 0),
  der Oberlenker 33,56 mm darüber.
- **Steuerrohr und Übergang haben dieselbe Kontur und liegen in einer Linie
  [VORGABE]:** 50 mm breit mit geraden Flanken, vorne ein Halbkreis R 25,
  6° nach vorne unten geneigt. Hinten sind die Flanken 50 mm lang gezeichnet
  und gerade abgeschlossen (dort folgt der Rahmen, es wird nichts angebaut).
  Der Scheitel des Halbkreises liegt an der Vorbau-Unterkante bei X +25 —
  **geschätzt** (`HEADTUBE_CENTER_X`), vom Nutzer nur als „etwas weiter
  hinten“ als zuvor (+31,6) angegeben.
- **Unterseite ausgeschnitten:** Der Übergang reicht damit 11,5 mm in die
  Klemmzone. Die Unterseite ist dort mit 1 mm Luft um die Kontur
  ausgeschnitten, die obere Kante des Ausschnitts mit R 3 verrundet (zwischen
  Übergang und Vorbau liegt ebenfalls ein Radius). Der Träger ist unverändert.
- **STEP:** Die Druckteile sind einzeln und fest eingefärbt (Träger blau,
  Displaygehäuse gelb, Deckel grün, Unterseite türkis; `PART_COLORS`). Die
  Rahmenteile bilden das Part „Rahmen“ (dunkelgrau), Display, Akku und Sensor
  das Part „Elektronik“ (rot).

### Selbsttest

81 Stichpunkte, 12 Verschraubungen (freier Weg, Wand ringsum um Buchsen und
Senkungen, Auflage der Köpfe, kein Kopf steht unter der Platte vor,
Gewindelänge über den ganzen Klemmweg), Einbauraum (nichts vor X +63,5, nichts
tiefer als 18 mm unter dem Vorbau, hinter den Wangen nichts unter der Platte),
paarweise Durchdringung der Druckteile untereinander und mit allen
Bezugskörpern (Cockpit, Displaymodul, Akku, Sensor samt Kabelraum), Einschieben
der Unterseite bis auf Anschlag, Stirnflächen von Buchse und Schalter.

### Offen

- **Bezug der 50 mm:** „Steuerrohr“ = X +13,5 (`TRAEGER_X_MIN`); vom Nutzer als
  passend bestätigt, lässt sich verschieben.
- Nach hinten reicht das Gehäuse bis X −28,0 — Lenkeinschlag prüfen.
- Halterung der Sensorplatine.
- Tiefe der Displayfahne unter dem Glas ist nicht gemessen.
- Untere Kurve an der Unterseite nach Anprobe anpassen.

## 6a. Vierter Entwurf (überholt)

**Überholt nach der Anprobe am Rad:** Der Akkukasten unter dem Vorbau passt
nicht (Steuerrohr, Lampenhalter), der Träger war mit 77 mm zu lang. Das
Displaygehäuse dieses Entwurfs wurde einmal gedruckt. Modell und Maße liegen
als `archiv/model_entwurf4.py` und `archiv/dimensions_entwurf4.py` (nie
committet). Was unten zu Displaymodul, Buchse/Schalter, Deckel und Klemmweg
steht, gilt im fünften Entwurf weiter, soweit dort nicht anders beschrieben.

Anlass: Im dritten Entwurf ragten 44 % der Displayfläche frei hinter die
Trägerkante, gehalten von zwei Schrauben, die beide *vor* dem Schwerpunkt
saßen (21 mm Hebel). Dazu kamen aus dem FreeCAD-Modell des Nutzers das echte
Displaymaß, der längs liegende Akku und das Sensorfach.

### Vorbau nachgemessen, Displaygehäuse eingefroren

Am Rad gemessen **[VORGABE]**: Vorbau **50,6 mm breit** (FreeCAD-Modell: 40),
**33,2 mm hoch** nah am Steuerrohr, vorne 31 mm; oben und unten verrundet
(ungleichmäßige Kurven, oben enger als unten).

- **Ausschnitt 51 × 35 mm**, gerade durchlaufend. Die Luft zum Vorbau füllt
  Moosgummi (im Modell oben und unten je 0,9 mm); die Höhe wird später über
  den Akkudeckel genau eingestellt.
- **Oben R 5** zwischen Teller und Wangen — versteift den Träger.
- **Unten sitzt die Rundung am Akkudeckel**, damit der Träger sich von oben
  aufstecken lässt: an beiden Wangen eine Kurve, die senkrecht mit R 5 beginnt
  und waagerecht mit R 10 ausläuft, Radius dazwischen linear mit dem Winkel.
  Sie ist 8,2 mm breit und 6,8 mm hoch, 0,2 mm Luft zur Wange.

**Klemmweg [VORGABE]:** Die Wangen sind nur 30 mm tief. Der Akkudeckel steckt
mit seinen Kurven zwischen den Wangen und lässt sich um 10 mm verschieben
(Ausschnitt 30 … 40 mm hoch, gezeichnet bei 35 mm mit 5 mm Spalt). Über den
Spalt klemmen die Stapelschrauben den Vorbau; die Kurven verdecken ihn von der
Seite bis 6,8 mm. Dafür sind die Stapelschrauben jetzt **M3×25** (4,1 … 14,1 mm
in der Wange), über jeder Buchse ist die Wange 14 mm tief frei gebohrt. Über
den hinteren Schrauben bleiben 0,9 mm bis zur Bohrung des Schalters.

**Führung:** Je Seite eine Zunge 24 × 3 mm, 12 mm hoch, oben angefast, auf dem
Akkudeckel; sie läuft in einem Schlitz in der Unterseite der Wange (0,2 mm Luft
je Seite, 2,2 mm Wand innen und außen). Sie sitzt bei X +58 zwischen der Nut
der vorderen langen Schraube und der vorderen Stapelschraube und steckt auch
bei größtem Spalt noch 2 mm tief. Der Selbsttest schiebt den Deckel auf
Anschlag und prüft, dass nichts anstößt.

**Das Displaygehäuse ist gedruckt [VORGABE]** — seine Schnittstelle (Umriss,
lange Schrauben bei Y ±34,8, Steigbohrungen bei X +30, Kammern) ist jetzt in
`dimensions.py` fest (`UPPER_SCREW_Y`, `CABLE_RISER_X`) statt aus der
Wangenbreite abgeleitet. Folgen am Träger:

- Die vorderen langen Schrauben liegen 1,8 mm in der Flanke der breiteren
  Wangen. Dort läuft eine **senkrechte Nut Ø 6,6** durch Wange, Akkudeckel und
  Wanne — die Schrauben sind damit auch bei montiertem Akkukasten erreichbar.
- Die Zuluftkanäle sitzen dicht am Ausschnitt (1,2 mm Wand), innen neben der Nut.

### Aussparungen für die Displayelektronik **[VORGABE]**

Nach dem ersten Druck des Displaygehäuses:

- **Fahne der Anzeige:** Direkt unter dem Glas reicht die Elektronik nach
  hinten bis 0,8 mm an den Glasrand (im Hersteller-STEP nur bis R 31,6). Die
  Tasche ist dort über 36,2 mm Breite bis R 37,0 aufgeweitet, über die ganze
  Höhe; die Glasstufe fehlt an dieser Stelle. Die Becherwand ist dort noch
  7,4 mm dick. **Das ändert das Displaygehäuse** (sonst unverändert, die
  Schnittstelle zum Träger bleibt).
- **I2C-Buchse:** Die beiden Steckbuchsen unter der Platine (vorne links, im
  Modul bei X +4 … +19, Y +14 … +27) bauen ca. 1 mm höher als im STEP. Der
  Teller hat dort eine **Mulde 2 mm tief** (darunter bleiben 3 mm), ringsum
  1,5 mm größer. Sie gilt für die Einbaulage „Fahne nach hinten“.

### Displaymodul: hängt am Glasrand

Aus der Baugruppe „T-RGB-FULL-3D_ASM“ im FreeCAD-Modell; das Deckglas hat der
Nutzer dort auf das echte Maß skaliert **[VORGABE]**:

- **Deckglas Ø 74,8 × 1,0 mm** (Dicke vom Nutzer; im STEP 1,9), steht ringsum
  über den Körper hinaus
- Körper Ø 56,4 mm (FPC-Block hinten bis R 28,75), Gesamthöhe 17,4 mm

Die früheren Ø 59,5 stammten aus einer zweiten, falschen Baugruppe im
Hersteller-STEP. Das Displaygehäuse hat deshalb jetzt oben eine **Stufe
Ø 75,2 × 1,1 mm** für das Glas und darunter eine **Tasche Ø 70**, 18 mm tief ab
Glasoberseite, für den Körper. Die Anzeige selbst hat nur
Ø 55 **[VORGABE]**; der Deckel lässt unten **Ø 58** frei und hält das Modul am
8,4 mm breiten Glasrand nieder. Nach oben öffnet er sich als **flache Fase**:
36,6° gegen die Waagerechte, oben Ø 78,4, über einem 0,8 mm hohen Absatz am
Glas. Flacher geht es nicht — an den Buchsen bleiben so gerade 1,2 mm Wand
(`DECKEL_INSERT_WALL_MIN`).

Lage der Anschlüsse am Modul, so wie es im FreeCAD-Modell eingebaut ist
(FPC nach hinten), in Fahrradkoordinaten — aus dem STEP abgelesen:

| Anschluss | Lage | Höhe über dem Teller |
|---|---|---|
| USB-C | rechts hinten, bei X ≈ +10, zeigt schräg nach rechts-hinten | ca. 3 … 6 mm |
| SD-Karte | links, X ≈ +2 … +21 | ca. 4 … 6 mm |
| Stecker (Akku, I2C) | unter der Platine, vorne links | — |

### Teller unter dem ganzen Display

Die Lage bleibt wie im FreeCAD-Modell: Display über der Schaftklemmung, Wangen
erst davor (wegen des Steuerrohrs). Geändert ist, was das Display trägt:

- Der Träger hat oben einen **Teller mit dem Umriss des Displaygehäuses**, der
  oberhalb des Vorbaus nach hinten durchläuft. Über dem Vorbau dreht er beim
  Lenken mit und stört das Steuerrohr nicht. Die Abdeckkappe über dem Schaft
  ist versenkt **[VORGABE]**.
- Der Teller liegt über das Moosgummi **auf dem Vorbau auf**. Hinter den Wangen
  ist er eine blanke 5-mm-Platte: **unter ihm darf dort nichts sitzen**, neben
  dem Vorbau ist das Steuerrohr im Weg **[VORGABE]** (Rippen aus einem
  Zwischenstand wieder entfernt; der Selbsttest prüft das). Steif wird er
  durch Becher und Deckel, die mit vier Schrauben daraufgeklemmt sind.
- Das Displaygehäuse ist ein **Ring ohne Boden**; der Teller schließt Tasche
  und Sensorfach nach unten ab.

| | dritter Entwurf | vierter Entwurf |
|---|---|---|
| Träger unter der Grundfläche des Displaygehäuses | 56 % | **99 %** (Rest sind Bohrungen) |
| Schrauben am Displaygehäuse | 2, beide vor dem Schwerpunkt | **4, um den Schwerpunkt** |
| Schwerpunkt zur Schraubenbasis | 21 mm außerhalb | 15 mm innerhalb |

Der Teller steht 39 mm hinter die Wangen hinaus — er hängt dort aber nicht
frei, sondern liegt auf dem Vorbau auf.

### Dicke Wände, glatte Außenflächen **[VORGABE]**

Lieber dicke Wände und wenig Infill; das ist auch strömungsgünstiger. Die
Buchsen sitzen deshalb **in der Wand** statt in angesetzten Domen:

- Displaygehäuse: glatter Kreis **Ø 88,8** **[VORGABE]** (FreeCAD-Modell: Ø 90),
  Wand 6,8 mm an der Glasstufe, 9,4 mm darunter. Kleiner geht kaum: die
  Schrauben sitzen schon 0,8 mm neben der Glasstufe, die Buchsen im Deckel
  haben außen 2,2 mm Wand.
- Wangen: ebene Flanken (bis auf die Nut der vorderen langen Schrauben),
  7,8 mm dick, senkrechte Kanten R 4,5.
  Träger und Akkukasten sind bündig **66,6 mm** breit.
- Teller 5 mm, Sensorfach-Wände 3,2 mm, vordere Ecken des Fachs R 6.

### Verschraubung: alles von unten, nichts von oben sichtbar **[VORGABE]**

M3 **Linsenkopf** (ISO 7380) **[VORGABE]** in Heatset-Buchsen, keine Muttern,
kein Gewinde im Kunststoff. Die Köpfe sitzen in zylindrischen Senkungen Ø 6,2
und stehen nicht über:

| Schrauben | Weg | Senkung | Gewinde in der Buchse |
|---|---|---|---|
| 4× M3×25 | Teller → Becher → Deckel | 2,0 mm (3,0 mm darunter) | 4,0 mm, Buchse im `deckel` |
| 4× M3×25 | Akkuwanne → Akkudeckel → Wange | 2,0 mm | 4,1 … 14,1 mm je nach Klemmweg, Buchse im `traeger` |

Die langen Schrauben klemmen Teller, Becher und Deckel in einem Zug; der Deckel
ist oben geschlossen. Deckeldicke: Buchse 6 mm tief plus 2,4 mm Deckschicht =
**8,4 mm**.

### Akkukasten: Wanne unten, Deckel dazwischen **[VORGABE]**

Die `akku_wanne` ist das unterste Teil und nach oben offen (Boden 2 mm, an den
Enden massiv für die Schrauben). Der `akku_deckel` (2,4 mm) steckt zwischen
Wanne und Wangen: die Unterseite ist eben (druckt ohne Stützen, kein Absatz
mehr in die Wanne; die Lage halten die Schrauben, Wannenboden dafür 3 mm),
oben drückt er über das Moosgummi gegen den Vorbau und trägt die beiden
Kurven für dessen untere Rundung (siehe oben).

### Ladebuchse rechts, Schalter links **[VORGABE]**

Wie im alten Gehäuse: innen ein kleiner Winkelstecker im USB-C des Moduls,
zweipolig auf eine Einbaubuchse. Die SD-Karte braucht keinen Zugang.

Die Buchse ist Ø 10,5 mm und 15 mm tief, die Kabel gehen axial hinten heraus;
der Schalter ist gleich gebaut, aber Ø 12. Radial im Displaygehäuse ist dafür
kein Platz (dort sitzt der Modulkörper). Beide liegen deshalb **waagerecht in
Taschen am Träger**: unter dem Teller, außen an den Wangen, Öffnung nach hinten
zum Fahrer. Der Teller ist das Dach, die Wange schirmt nach vorne ab.

| | Bohrung | Lage der Öffnung |
|---|---|---|
| Ladebuchse, rechts | Ø 10,8 × 15 mm + 5 mm Kabelraum | X +13,5, Y −33,5, Z −7,7 |
| Schalter, links | Ø 12,3 × 15 mm + 5 mm Kabelraum | X +13,5, Y +33,5, Z −8,5 |

Beide werden von außen aufgesteckt und brauchen an der Stirnfläche **Ø 15**
**[VORGABE]** (zugleich der Platz für die Finger). Die Bohrungen sitzen deshalb
so tief, dass der Kragen unter dem Teller frei bleibt, und die Tasche überdeckt
die gerundete Hinterkante der Wange: Wange und Tasche bilden eine ebene
Stirnfläche ohne Spalt. Der Selbsttest prüft die Auflage ringsum.

- Die Bohrungen sitzen so weit innen, wie der Kragen Ø 15 neben dem Vorbau
  zulässt (Schalter 6,1 mm in der Wange, 1,9 mm Wand zum Ausschnitt); die Taschen sind auf den Umriss des
  Tellers gestutzt und machen das Gehäuse nicht breiter. Wand 3,2 mm.
- Hinter Buchse bzw. Schalter gehen die Kabel durch ein Loch Ø 6 im Teller nach
  oben in eine **Kammer in der Becherwand** (12 mm hoch, bis R 37,4). Rechts
  reicht sie bis zum USB-C des Moduls bei 251,5°, links ist sie gespiegelt.
- **Der Schalter schaltet den Akku.** Das Akkukabel läuft deshalb: Wanne →
  Loch im Akkudeckel → senkrechter Kanal Ø 6 in der linken Wange (zum Vorbau
  hin offen) → Querbohrung in die Schaltertasche → von dort durch den Teller
  zum Display.
- Länger als 20 mm können die Bohrungen nicht werden: direkt davor sitzen die
  vorderen langen Schrauben.

### Sensorfach (BME280 + BMI160)

Vorne am Displaygehäuse, auf der Platte des Trägers. Für zwei Platinen von ca.
**17 × 12 mm** **[VORGABE]**: innen 40 mm breit, 15 mm vor der früheren
Becherwand, 24 mm hoch (der Deckel ist darüber ausgehöhlt).

**Zwischen Fach und Displaytasche steht keine Wand mehr [VORGABE]** — die
Becherwand ist über die Fachbreite offen, auch die Glasstufe fehlt dort; das
Glas liegt auf den übrigen rund 290° auf. Je zwei Lüftungsschlitze in den
Seitenwänden für den BME280. Halterungen für die Platinen fehlen noch.

### Lüftung und Ablauf **[VORGABE]**

- **Zuluft:** je ein Kanal Ø 4 links und rechts. Der Einlass liegt tief an der
  Vorderseite der Wange (Y ±28,7, Z −12), die Kante ist mit R 1,5 aufgeweitet.
  Von dort steigt der Kanal **gerade mit 18°** nach hinten an und mündet bei
  X +31 auf dem Teller im Ringspalt neben dem Modulkörper — kein Knick, und
  Wasser läuft zum Einlass zurück.
- **Abluft:** zwei Schlitze 14 × 2,5 mm hinten in der Becherwand, im
  Windschatten.
- **Ablauf:** runde Rinne (Halbkreis R 1,5, Enden ausgerundet) auf der
  Oberseite des Tellers, längs durch Sensorfach und Displaytasche, quer dazu zu zwei Löchern Ø 3 seitlich neben
  dem Vorbau (X +10,5, Y ±35,3), die nach unten ins Freie gehen.

Die Zuluft-Einlässe zeigen in Fahrtrichtung; der Anstieg hält Wasser weitgehend
draußen, der Rest läuft über Rinne und Löcher ab.

### Deckel: Fase und Sonnenschutz **[VORGABE]**

Fase siehe oben (36,6°, unten Ø 58). Darauf der Sonnenschutz nach dem Vorbild
des alten „Deggl“ (`Case_Grail_TRGB_Round_v2`): dünne, nach innen geneigte Wand
um die Vorderseite, an den Seiten nach hinten gezogen (±130° ab
Fahrtrichtung), hinten zum Fahrer offen. Wand 2,0 → 0,8 mm, innen Ø 79,4 am
Fuß → Ø 68,8 oben (gleiche Neigung wie beim alten).

Höhe **11 mm** wie beim alten: der Teller liegt jetzt 0,9 statt 3 mm über dem
Vorbau, die Oberkante bleibt mit 43,3 mm unter dem Entwurfsziel von 44 mm
(4 mm Reserve zur Flex-Zone).

### Rundungen **[VORGABE]**

- Senkrechte Vorderkanten von Träger und Akkukasten **R 12**. Die Wange ist
  dünner als das, sie läuft vorne also ganz rund aus. Die Rundung beginnt
  erst auf Höhe der vorderen Buchsen, damit sie diese nicht anschneidet
  (2,4 mm Wand); der Träger reicht deshalb 12 mm vor die Schrauben.
- Vordere Ecken des Sensorfachs R 10, Oberkante des Deckels R 2,5, Unterkante
  der Akkuwanne R 2.

### Maße

| Maß | Entwurf | Vorgabe / FreeCAD-Modell |
|---|---|---|
| Breite Wangen und Akkukasten | **66,6 mm** | 55 (bei 40 mm Vorbau) |
| Länge der Wangen | 77,3 mm (X +13,5 … +90,8) | 60 |
| Oberkante (Sonnenschutz) über Vorbau | **43,3 mm** | max. 48 ✓ |
| Akkukasten unter dem Vorbau | **13,8 mm** | max. 18 ✓ (Modell: 12) |
| Displaygehäuse | Ø 88,8 mm | Modell: Ø 90 |
| Volumen (massiv gerechnet) | Träger 75, Becher 35, Deckel 28, Akkukasten 44 cm³ | — |

**Warum 77 statt 60 mm lang:** Der Akku (54 längs, 51,5 quer) lässt seitlich
keinen Platz für Schrauben. Die vier Stapelschrauben sitzen deshalb vor und
hinter ihm, davor kommt die Rundung. Die hintere Kante ist dort geblieben, wo
sie im FreeCAD-Modell liegt; gewachsen ist der Träger nach vorne.

**Montage:** Becher auf den Teller, Modul einlegen, Buchse und Schalter in die
Taschen, Kabel durch den Teller nach oben, Deckel drauf, vier M3×25 von unten.
Das Ganze von oben auf den Vorbau setzen, Akku anstecken, Akkudeckel und Wanne
von unten mit vier M3×25 anziehen, bis das Moosgummi klemmt. Die beiden vorderen langen Schrauben
bleiben über die Nut in der Flanke erreichbar.

**Druck:** Träger auf dem Kopf, Displaygehäuse und Akkuwanne aufrecht, Deckel
mit der Unterseite auf dem Bett — ohne Stützmaterial bis auf die 3,5 mm
breiten Decken der beiden Kammern; die waagerechten Bohrungen überbrücken sich
selbst, die Decke über dem Sensorfach ist eine 40-mm-Brücke.

### Selbsttest

70 Stichpunkte, 8 Verschraubungen (frei durch alle Teile, Buchse mit Wand
ringsum, von unten zugänglich, von oben nicht sichtbar, Gewindelänge),
paarweise Durchdringung aller Teile, des Vorbaus und des Displaymoduls,
Auflage des Displays, nichts unter dem Teller hinter den Wangen, Stirnflächen
von Buchse und Schalter. `render.py`
zeichnet Vorbau und Modul als Bezugskörper mit.

### Offen

- Ist rechts und links neben den Wangen, hinter X +13,5, Platz für Ladestecker
  und Finger am Schalter — oder ist auch dort das Steuerrohr im Weg?
- Steht der Teller selbst hinten (bis X −26) dem Steuerrohr im Weg?
- Steuerrohr-Freihaltung: Akkukasten liegt mit ca. 5 cm³ in der
  **geschätzten** Zone, ist aber dort, wo ihn das FreeCAD-Modell hinsetzt.
  Der Nutzer prüft das am Rad.
- Befestigung der Sensorplatinen im Fach (Lochbild).
- Ist vor X +73,5 (Vorderkante im FreeCAD-Modell) am Vorbau noch 17 mm Platz?
- Untere Kurve am Akkudeckel nach Anprobe an den Vorbau anpassen.
- Tiefe der Displayfahne unter dem Glas ist nicht gemessen; die Aussparung
  geht deshalb über die ganze Höhe.
- Der Vorbau wird nach vorne niedriger (31 mm); der Ausschnitt ist überall
  35 mm hoch.

## 6b. Dritter Entwurf (überholt)

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

## 6c. Zweiter Entwurf (überholt)

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

## 6d. Erster Entwurf (überholt)

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

1. Wo beginnt unter dem Vorbau die freie Zone vor dem Steuerrohr, gemessen ab
   der Schaftachse oder dem hinteren Vorbau-Ende? (Annahme: X +13,5.)
2. Abstand Schaftachse → hinteres Vorbau-Ende (alle X-Lagen hängen daran,
   Annahme 22 mm).
3. Steuerrohr: Durchmesser und Winkel gegen den Vorbau — bisher nur gezeichnet.
4. Lampenhalter: Breite und Höhe, falls er seitlich oder nach hinten übersteht.
5. Wie breit ist der Übergang zum Steuerrohr neben X +13,5 (Platz für
   Ladestecker und Schalter)?
6. Wie weit darf es nach hinten gehen, bevor beim vollen Lenkeinschlag das
   Oberrohr berührt wird? Das Gehäuse reicht jetzt bis X −31.
7. **33,56 mm** — lichter Spalt Oberlenker↔Vorbau oder X-Tiefe des Bügels?
   Nur noch für die Zeichnung wichtig.
