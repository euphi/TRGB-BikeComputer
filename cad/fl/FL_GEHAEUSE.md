# FL-Gehäuse (Forumslader-Variante) am Diamant Nhoma

Erster Entwurf, Stand 2026-10-21. Modell: `model_fl.py` (Teile, Selbsttest, STEP),
Maße mit Herkunft: `dimensions_fl.py`, Bilder: `render_fl.py`.

```
cad/.venv/bin/python cad/fl/model_fl.py    # Bericht, Selbsttest, export/TRGB_Gehaeuse_FL.step
cad/.venv/bin/python cad/fl/render_fl.py   # export/fl_*.png (am Rad), export/teil_*.png (einzeln)
```

Markierung wie in `../COCKPIT_ANALYSIS.md`: **[VORGABE]** vom Nutzer,
**[ALT-FL]** aus dem alten Modell, **[DATENBLATT]** Herstellerangabe,
**[GESCHÄTZT]** nachzumessen.

## 1. Ausgangslage

- **Rad:** Diamant Nhoma, Dropbar. Laut Händler-Datenblatt **[DATENBLATT]**:
  Vorbau Bontrager Comp, 31,8 mm, 7°, 4-Schrauben-Frontplatte, 1 1/8"-Schaft,
  Länge je nach Rahmengröße 60–100 mm; Lenker Bontrager Elite Gravel, Klemmung 31,8.
- **Display:** das ovale T-RGB 2.1" („Half Circle“). Umriss aus dem alten Modell
  `doc/FreeCAD/TRGB-Handlebar-FL.FCStd` **[ALT-FL]**: Kreis R 30,5, nach hinten ein
  trapezförmiges Kinn mit 35 mm breiter Abflachung (32,5 mm hinter der Kreismitte),
  19,8 mm tief, Fenster Ø 55. Das Kinn zeigt zum Fahrer (Foto
  `doc/IMG20230406075242-cropped.jpg`). Der kleine Akku sitzt im Modul **[VORGABE]**.
- **ESP32 (Bluetooth Classic zum Forumslader):** Wemos ESP32 38-pin, **ohne
  Stiftleisten** **[VORGABE]**. Gezeichnet 55 × 28,5 mm, 1,6 mm Platine + 3,5 mm
  Bauteile oben **[GESCHÄTZT]**: Händler nennen 48 × 26 bis 55 × 28. Bisher war er auf
  eine Plexiglasplatte quer unter dem T-RGB geklebt **[VORGABE]**.
- **Versorgung:** Kabel 3 × 1,5 mm² vom Forumslader (5 V, 12 V Akku, GND) **[VORGABE]**,
  gezeichnet Ø 8,5 **[GESCHÄTZT]**, wasserdicht über eine **Kabelverschraubung M16 × 1,5**
  **[VORGABE]**.
- **USB-C-Einbaubuchse**, kleine Bauform **[VORGABE]**; gezeichnet als M12-Gewinde,
  Bohrung Ø 12,3 (wie beim Schalter 0,3 mm Zugabe, weil gedruckte Löcher schrumpfen).
- **Schalter** für die Versorgung vom Forumslader, Bohrung Ø 12,3 (wie im runden Gehäuse).

Das alte Modell rechnet in FreeCAD 1.1 nicht mehr neu, die gespeicherten Formen und
Skizzen lassen sich aber auslesen. Übernommen sind nur Displayumriss, Fenster, Lage
des USB-C am Modul und die Idee der Wago-Klemmen; die Klickfix-Anbindung entfällt.

## 2. Konzept

Das Gehäuse sitzt **oben auf Vorbau und Lenker**. Gebaut ist im Vorbau-System (Ursprung
Lenkerachse, x entlang des Vorbaus); mit −7°-Vorbau und 71,5° Lenkwinkel steigt der
Vorbau um 11,5°, um diesen Winkel ist das Display zum Fahrer geneigt.

- **Vorne:** Der Träger umgreift den Lenker links und rechts neben der Frontplatte
  (je eine Schale Y ±24 … ±35, 47 mm lang), darunter je eine Klemmschale mit 2× M3
  von unten. Sitz Ø 32,8 für 0,5 mm Gummiband, 1 mm Klemmspalt.
- **Hinten:** Ein Fuß (12 × 34 mm) liegt mit 1 mm Gummi auf dem Vorbau. Er nimmt das
  Gewicht und verhindert das Abkippen nach hinten unten; nach oben hält die Klemmung.
- **Nichts hinter X −46,6:** Die Rückwand bleibt 9 mm vor der Ahead-Kappe (bei 70 mm
  Vorbau; bei 60 mm wird es eng, der Selbsttest meldet das).

Von oben nach unten (Höhen senkrecht zum Vorbau, über der Lenkerachse):

| Teil | Höhe | Inhalt |
|---|---|---|
| `fl_deckel` | 59,1 … 63,9 | Platte 4,8 mm, Fenster Ø 55 mit 45°-Fase, Glasstufe 3,2 mm |
| `fl_ring` | 26,0 … 59,1 | oben Tasche für das Modul, unten Fach 16,5 mm hoch |
| `fl_traeger` | 22,0 … 26,0 | Boden 4 mm, darunter Lenkerschalen und Fuß |
| `fl_klemme_*` | −21,0 … −0,5 | untere Lenkerschalen |

Grundriss 74,6 × 80 mm (vorne Ecken R 18, hinten R 8). Glas 62,3 mm über der
Lenkerachse, Oberkante 63,9.

### Fach unter dem Modul

- **ESP32** vorne, quer, flach auf den Boden des Trägers geklebt (wie bisher auf die
  Plexiglasplatte, nur ohne eigene Platte). Oben bei 31,1, darüber 11,4 mm frei bis zum
  Modul. Eine Mulde zum Ausrichten gibt es nicht: Der Träger wird kopfüber gedruckt, ihr
  Boden wäre eine Brücke über die ganze Platine.
- **Wago 221** dahinter in der Mitte (Platz 13 × 22 × 9).
- **Rückwand (zum Fahrer):** Schalter links (+Y), USB-C rechts (−Y), je auf Y ±21,5,
  Achse 8,25 mm über dem Boden; dahinter je 14 mm Tiefe und Ø 16 für die Mutter.
- **Kabelverschraubung M16 × 1,5** in der **linken Seitenwand hinten** (X −27, Achse
  38,5 über der Lenkerachse), Bohrung Ø 16,5, ebene Dichtfläche Ø 24 über der Fuge zum
  Träger. Innen dreht sich die Mutter (SW 19, über Eck 22) in einer Tasche Ø 23 × 7 im
  Ring, die nach unten ins Fach offen ist. In der Rückwand passt sie nicht: Die Mutter
  reicht dort bis unter das Kinn des Moduls. Außen stehen Verschraubung und Kabel nach
  links ab (gezeichnet 22 mm).
- **Winkelstecker am USB-C des Moduls:** Kammer rechts neben dem Modul, 7,5 mm breit,
  nach unten ins Fach offen. Reicht nur für einen flachen 90°-Stecker.
- **Ablauf:** zwei Löcher Ø 3 hinten im Boden (tiefste Stelle), neben dem Vorbau.

### Modul

Liegt vorne auf einem Block (Y ±12) und hinten auf einer Leiste an der Rückwand, je
2,5 mm breit; der Deckel drückt es am Glasrand nieder (Schaumstoff auf den Auflagen).
Das Glas deckt den ganzen Umriss, eine Stufe unter dem Glas gibt es nicht.

## 3. Verschraubung

Alles von unten, von oben ist keine Schraube zu sehen; Gewinde nur in Heatset-Buchsen.

| Schrauben | Weg | Kopf |
|---|---|---|
| 3× M2×20 | Ring → Deckel (vorne Mitte, hinten in beiden Ecken) | 16,9 mm tief im Ring versenkt (Bohrung Ø 4,4 von unten) |
| 4× M2×6 | Träger → Ring | im Träger versenkt |
| 4× M3×16 | Klemmschale → Lenkerschale im Träger | 5,5 mm in der Buchse |

Buchsen: 3 + 4 × M2×3 (Bohrung Ø 3,2 × 4), 4 × M3 (Ø 4,2 × 6).

**Reihenfolge:** Modul in den Ring, Deckel drauf, von unten verschrauben. ESP32,
Wago, Schalter, Buchse und Kabelverschraubung einbauen (ESP32 auf den Träger kleben),
Träger von unten anschrauben. Dann aufs Rad und die Klemmschalen von unten anziehen.

## 4. Druck

| Teil | Lage | Hinweise |
|---|---|---|
| `fl_deckel` | Oberseite aufs Bett | Fensterfase und Außenfase 45° |
| `fl_ring` | **kopfüber** (Fuge zum Deckel aufs Bett) | aufrecht wären die Decken über dem Fach freie Überhänge; kopfüber hängen nur die 2,5 mm breiten Modulauflagen |
| `fl_traeger` | **kopfüber** (ebene Oberseite aufs Bett) | Lenkerschalen nach oben offen |
| `fl_klemme_*` | wie eingebaut | Sitz nach oben offen |

Der Selbsttest prüft das: keine nach unten zeigende Fläche über 100 mm² außerhalb des
Betts, dazu paarweise Durchdringung aller Teile und Bezugskörper, Gewindelängen und
den Abstand zur Ahead-Kappe.

## 5. Offen — bitte am Rad bzw. am Teil messen

Vorerst gelten die Annahmen unten **[VORGABE 2026-10-21]**; nachmessen, bevor gedruckt
wird, was am Rad anliegt (Vorbau, Lenker).

| Maß | Annahme | Wirkt auf |
|---|---|---|
| Vorbau-Länge | 70 mm | Abstand Rückwand ↔ Ahead-Kappe |
| Vorbau-Winkel montiert (±7°), Lenkwinkel | −7°, 71,5° → steigt 11,5° | Neigung des Displays |
| Höhe der Frontplatte über der Lenkerachse | 20,4 (Klemmung Ø 40,8) | Unterkante Träger (22,0) |
| Breite der Frontplatte | 46 mm | Lage der Lenkerschalen (ab ±24) |
| Lenker 31,8 mm bis | ±55 mm von der Mitte | Lenkerschalen ±24 … ±35 |
| Vorbau-Oberkante, Breite | 17 über der Achse, 36 breit | Fuß |
| Spacer über dem Vorbau, Ahead-Kappe | keine, Kappe 6 mm | Freigang hinten |
| ESP32: Maße | 55 × 28,5 × 5,1 | Lage im Fach |
| Kabel-Ø 3 × 1,5 mm² | 8,5 | Klemmbereich der Verschraubung (M16: ca. 4,5 … 10) |
| Kabelverschraubung: Gewinde-Ø, Mutter, Dichtring | M16 × 1,5, SW 19, Dichtfläche Ø 24 | linke Seitenwand |
| USB-C-Einbaubuchse: Gewinde, Mutter, Tiefe | M12, Ø 16, 14 tief | Rückwand |
| Schalter: Bohrung, Tiefe | Ø 12, 14 tief | Rückwand |
| Winkelstecker am Modul-USB-C | 7,5 mm neben dem Modul | Kammer rechts |
| Lage des Modul-USB-C (rechts?) | rechts, aus dem alten Modell gelesen | Kammer |

Fragen zum Konzept:

- **Flacher bauen:** Ohne Stiftleisten bestimmen nur noch die Muttern von Schalter und
  USB-C (Ø 16) die Fachhöhe von 16,5 mm. Säßen beide in den hinteren Ecken neben dem
  Kinn des Moduls (dort ist bis oben Platz), reichten etwa 10 mm für ESP32 und Wago —
  das Gehäuse würde rund 6 mm niedriger. Dafür müssten die hinteren Deckelschrauben
  umziehen.
- Sonnenschutz wie beim CP0007-Deckel?
- Form: Der Grundriss ist ein abgerundetes Rechteck, weil die Deckelschrauben in den
  Ecken außerhalb des Moduls sitzen müssen. Rundere Form nur mit anderen Schraubenlagen.
