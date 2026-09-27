# Rim & Ridge — Design System

Visuelle Identität des TRGB-BikeComputers: ein rundes 480×480-Display
(GC9A01-Klasse-TFT), LVGL-UI. Zurückhaltend, dünne Linien, ein Akzentton
(Messing) auf Anthrazit. Named nach dem Bootlogo-Motiv — der Bildschirmrand
selbst ist die Felge.

**Status:**

| Screen | Datei | Zustand |
|---|---|---|
| Mainscreen | [`mainscreen.svg`](mainscreen.svg) | **implementiert**, `EEZStudio/TRGB-BikeComputer.eez-project`, Screen `rim_ridge`, inkl. Straßenqualität-Indikator (`rr_line_rq`, §4) |
| Navigationsscreen | [`navscreen.svg`](navscreen.svg) | **implementiert**, Screen `rim_ridge_nav` (`SCREEN_ID_RIM_RIDGE_NAV`) |
| Einstellungen | [`settings.svg`](settings.svg) | **implementiert**, Screen `RimRidgeSettings` (`SCREEN_ID_RIM_RIDGE_SETTINGS`), gegenüber der SVG um WLAN-Reconnect, Kalibrierung und Referenzfahrt erweitert (§6) |
| RQ-Ride-Screen | [`rqscreen.svg`](rqscreen.svg) | **Umgesetzt** (Screen `RimRidgeRQ`) — Nav-Pille, Speed, Puls, Wegzähler (Tourstrecke) mit echten Daten; RQ-Index aus `ui_RimRidgeUpdateRoadQuality()`; Untergrund-Pillen, Qualitäts-Regler und Aufnahme-Taste live über `I2CSensors::setRoadLabel*()`/`startRoadCapture()` verdrahtet (Tap togglet, zweiter Tap auf den aktiven Wert setzt ihn zurück) |

Jede SVG-Datei ist 1:1 im Ziel-Koordinatensystem (480×480 Einheiten =
480×480 physische Pixel) und lässt sich direkt im Browser oder per
`rsvg-convert -w 480 -h 480 mainscreen.svg -o preview.png` in echter
Pixelgröße prüfen — bei einem runden 480px-Display mit wenig Fläche ist das
kein optionaler Schritt, siehe „Lektion: Icon-Größe" unten.

Bei Abweichungen zwischen SVG und implementiertem Screen zählt der
EEZ-Screen. Dieses Dokument beschreibt den aktuellen Stand des Designs. Es soll reichen, um einen weiteren Screen im selben System zu
bauen, ohne das gesamte Bild neu erfinden zu müssen.

## 1. Farbpalette

Alle Werte sind in `screens.h`/`theme_colors[]` als `COLOR_ID_RR_*` bereits
angelegt und verifiziert (1:1 mit den Werten unten) — beim Bauen neuer
Screens diese Theme-Farben referenzieren, nicht neu hex-codieren.

| Token (`COLOR_ID_RR_…`) | Hex | Verwendung |
|---|---|---|
| `BACKGROUND` | `#161B1F` | Screen-Hintergrund (Anthrazit) |
| `RIM_OUTLINE` | `#3A362E` | äußerer Bezel-Ring, r=234, 1.5px, jeder Screen |
| `ARC_TRACK` | `#332F28` | unbefüllter Teil des Speed-/Distanz-Rings |
| `BRASS` | `#CBA36B` | Primärakzent — Linien, Icons, Ring-Füllung, Zahlen-Highlights |
| `PARCHMENT_BRIGHT` | `#F3ECDF` | Hero-Wert (Speed-Zahl) und Ø-Marker |
| `PARCHMENT` | `#E7E2D6` | sekundäre Werte: Distanz, Temp, Höhe, Steigung, HF, Button-Labels |
| `MUTED` | `#9BA097` | Einheiten, kleine Caption-Labels |
| `SAGE` | `#7FA08F` | Naturbezug: Höhen-/Steigungs-Icon, Akku-Füllstand „gut" |
| `PANEL_BG` | `#1E252B` | Flächen von Chips/Buttons |
| `TOUR_BG` | `#282019` | Fläche des Modus-Chips |
| `ZONE_BLUE` … `ZONE_RED` | `#6C90B0` `#6FA98C` `#D7B463` `#CE8A4C` `#C1604A` | HF-Zonen 1–5, einzige bewusst bunte Stelle im System |

`Feldweg`/Straßenname und die kleinere Entfernung des übernächsten
Hinweises laufen auf `#A39D8E` (gedämpftes Pergament, kein eigenes Token in
EEZ — beim Anlegen ergänzen oder auf `MUTED` mappen).

In `theme_colors[]` liegen zusätzlich 16 nicht-`RR_`-präfixte Farb-IDs
(`NAV_ICON_RECOLOR`, `ARC_SPEED_TRACK`, `GRADIENT_GOOD_GREEN` …) mit
grellen Platzhalterwerten (reines Grün/Rot/Blau) — nicht Teil dieses
Systems, bei Gelegenheit aufräumen.

## 2. Typografie

Drei Fonts, klar nach Rolle getrennt — nie nach Geschmack mischen:

| Rolle | Font | Schnitt | Beispiele |
|---|---|---|---|
| Hero-/Sekundärzahl | **Big Shoulders Display** | 700 (hero), 600 (sekundär) | Speed (130px Main / 48px Nav), Entfernung (84px Nav), Distanz/Cadence/Watt/Temp/Höhe/Steigung/HF-Wert (20–28px) |
| Feld-/Button-Label | **IBM Plex Sans** | 600 | Button-Beschriftung („Neustart", „Tiefschlaf"), Straßenname |
| Einheit / technisches Label | **IBM Plex Mono** | 400–500 | Einheiten (KM/H, rpm, W), Modus-Chip-Text, Screen-Titel, Build/IP-Werte |

Google Fonts (`fonts.googleapis.com/css2?family=Big+Shoulders+Display:wght@600;700&family=IBM+Plex+Sans:wght@400;600&family=IBM+Plex+Mono:wght@400;500`)
für Vorschau/Browser. Für LVGL über den Font-Converter als Bitmap-Font
exportieren (`ui_font_by7x128` ist bereits die 128px-Variante von Big
Shoulders Display für den Speed-Wert) — pro tatsächlich benutzter
Kombination aus Font+Gewicht+Größe ein eigener Bitmap-Font, LVGL kann
Schriften nicht laufzeit-skalieren.

### Lektion: Icon-Größe

Piktogramm-Icons (Fahrrad, Stop-Schild etc.) brauchen **mindestens 32–34px**
Kantenlänge. Bei 20px verschwimmen Details wie die Speiche-Löcher eines
Laufrads praktisch vollständig — getestet sowohl kantengeglättet als auch
als simuliertes 1-Bit-Rendering (dem härtesten Fall: kein Alpha-Blending).
Die bereits exportierten Ride-State-Icons (`ui_image_rr_icon_state_*.c`)
nutzen 34×34px bei `LV_IMG_CF_ALPHA_8BIT` — das ist die richtige
Referenzgröße und das richtige Format (kantengeglätteter Alpha-Kanal, per
`lv_obj_set_style_img_recolor` zur Laufzeit auf `RR_BRASS` eingefärbt, statt
Farbe fest ins Bitmap zu backen). Neue Icons nach demselben Muster
exportieren, nicht als `LV_IMG_CF_INDEXED_1BIT` wie die älteren
SquareLine-Assets (2026-09-27 entfernt).

## 3. Koordinatensystem

480×480, Zentrum `(240, 240)`. Kein Layout-Grid — die Kreisform bestimmt,
wie viel horizontaler Platz an einer bestimmten Höhe verfügbar ist:

```
halbe Breite(dy) = √(r_sicher² − dy²)      r_sicher ≈ 205
```

`dy` = vertikaler Abstand von 240. Bei `dy = 0` (Bildschirmmitte) sind
±205px verfügbar; bei `dy = 148` (Höhe der Nav-Pille/Statuszeile) nur noch
±141px. Das ist der Grund, warum breite Elemente (HF-Zonenleiste,
Speed-Zahl) auf Zentrumshöhe sitzen und schmale, kurze Inhalte
(Statuszeile, Buttons) oben/unten. Vor jedem neuen Element an dessen Y-Höhe
kurz nachrechnen, nicht nach Gefühl platzieren — das war die häufigste
Fehlerquelle in der Entstehung dieses Designs.

`r_sicher ≈ 205` ergibt sich aus dem Arc-Ring bei `r=222` mit 16px Dicke
(Innenkante ≈ 214) minus Sicherheitsabstand. Bezel-Ring bei `r=234` ist rein
dekorativ und beansprucht keinen Inhaltsraum.

## 4. Wiederverwendbare Komponenten

### Gapped Arc (Speed-/Distanz-Ring)

Ein `lv_arc`-taugliches Muster: zwei konzentrische Kreise (Track + Füllung),
270° Bogen mit fester 90°-Lücke unten, Nullpunkt fix unten links.

```
r = 222, Strichdicke = 16
Umfang C = 2·π·r ≈ 1394.87
Track-dasharray   = "1046.15 348.72"           (0.75·C / 0.25·C)
Füllung-dasharray = "{0.75·C·wert/max} {C−…}"  z. B. 55 % → "575.38 819.49"
beide: transform="rotate(135 240 240)"
```

Bedeutung ist **screen-abhängig**: auf dem Mainscreen befüllt sich der Ring
mit der Geschwindigkeit (0 = leer, `max` = voller Ring). Auf dem
Navigationsscreen zeigt derselbe Mechanismus die Distanz zum Manöver
(befüllt sich beim Näherkommen). Ø-Marker: ein `<line>` quer über den Ring
bei `r ± 10`, Winkel `225° + 270°·(ø-wert/max)` im Uhrzeigersinn ab 12 Uhr —
Polarkoordinaten-Hilfsformel dafür:

```
x = cx + r·sin(θ)      y = cy − r·cos(θ)      (θ in Grad, 0° = oben, im Uhrzeigersinn)
```

### HF-Zonenband

Fünf `52×14`-Rechtecke nebeneinander (äußere mit `rx=7` abgerundet), feste
Zonenfarben (§1), plus ein kleines Dreieck als Positions-Marker über dem
Segment, das dem aktuellen Wert entspricht. Bewusst der einzige Ort mit
mehr als zwei gleichzeitigen Farben — hier trägt Farbe echte Funktion.

### Pille / Chip

`rx = Höhe/2` (voller Pill), Fläche `PANEL_BG` (Nav-Chip) oder `TOUR_BG`
(Modus-Chip), Rahmen `BRASS` bei 30–40% Deckkraft, 1.5px. Für Container mit
mehreren Elementen (Spur-Anzeige) `rx≈9` statt vollem Pill — unterscheidet
„Container" optisch von „Schalter/Hinweis".

### Icon-Buttons (rund) vs. Buttons mit Label (Pille)

Mainscreen-Buttons (Pause, Einstellungen) sind reine Icon-Kreise, `r=25`,
Fläche `PANEL_BG`, Rahmen `BRASS` 2px — genug Kontext durch Position, kein
Text nötig. Der Einstellungen-Screen nutzt breitere Pillen (`210×40`,
`rx=20`) mit Icon **und** Label, weil „Neustart"/„Tiefschlaf" ohne Text
nicht eindeutig genug wären und hier deutlich mehr Platz ist als im
gedrängten Mainscreen-Fuß.

### Segmentierter Wähler (diskrete Stufen)

Für eine kleine, feste Anzahl Stufen (z. B. Qualität 1–4) statt eines
kontinuierlichen `lv_slider`: `n` Kreise (`r=20`, Fläche `PANEL_BG`,
Rahmen `BRASS` 2px) im festen Abstand `60px` nebeneinander, verbunden durch
eine dünne Linie (`BRASS` 25 % Deckkraft, `stroke-width=3`) als optische
Klammer. Gewählte Stufe: Kreis voll `BRASS`, Ziffer in `BACKGROUND`
(dunkel-auf-hell statt hell-auf-dunkel) statt Rahmen — derselbe
Auswahl-Kontrast wie beim gefüllten Modus-Chip unten. `r=20` ist bewusst
in derselben Größenordnung wie die runden Icon-Buttons (`r=25`, §4) — große
Ziele, einhändig bedienbar. Erster Einsatzort: RQ-Ride-Screen (§6).

### Stat-Gruppe (Icon + Wert + Einheit)

Wiederkehrendes Muster für Cadence/Watt/Temp/Höhe/Steigung: kleines
Linien-Icon (`stroke-width` 1.6–2.2, `BRASS`), Wert direkt darunter oder
daneben in Big Shoulders Display 600, Einheit klein in IBM Plex Mono,
`MUTED`. Bei zwei symmetrischen Gruppen (Cadence/Watt) als schmale Spalte
außerhalb der Speed-Zahl platzieren, nicht auf gleicher Höhe daneben — sonst
Kollisionsgefahr mit der großen Zahl (siehe §3).

### Fahrzeit / Uhrzeit

Ein Slot, zwei mögliche Inhalte — welcher gerade gilt, ist App-Logik, nicht
Teil dieser Spezifikation. Icon und Wert wechseln immer zusammen:

- **Fahrzeit** (Stoppuhr-Icon: Kreis + Krone oben + Zeiger) — verstrichene
  Zeit der aktuellen Fahrt/Tour, Format `H:MM:SS` bzw. `MM:SS`
- **Uhrzeit** (schlichtes Zifferblatt-Icon: Kreis + zwei Zeiger, keine
  Krone) — aktuelle Tageszeit, Format `HH:MM`

Sitzt zwischen Nav-Pille/Spur-Anzeige (y≈132) und der Speed-Zahl (Ziffern
beginnen ≈y177). Icon fest bei
`x=210`, Wert linksbündig ab `x=224` (nicht als Gruppe zentriert), damit die
unterschiedliche Zeichenbreite von `1:24:07` gegenüber `14:32` nicht bei
jedem Wechsel neu zentriert werden muss — dieselbe Technik wie beim
Nav-Chip-Icon+Text. Big Shoulders Display 600, 28px, `PARCHMENT` — wie alle
anderen Werte auf dem Screen, keine Sonderbehandlung.

### Spur-Anzeige (Lane-Widget)

Ein gemeinsamer Rahmen (`rx≈9`, `PANEL_BG`, `BRASS`-Kontur 30–45%
Deckkraft) um alle Fahrspur-Pfeile — **kein** Rahmen pro Pfeil, das kostet
nur Platz ohne Mehrwert. Ein Pfeil pro echter Fahrspur:

- eindeutige Richtung → einfacher Pfeil, `BRASS` voll (empfohlen) oder
  40–45% Deckkraft (nicht deine Spur)
- mehrere Richtungen in einer Spur → **ein** Pfeil mit gemeinsamem Schaft,
  der sich in Äste aufspaltet (nicht zwei separate Pfeil-Icons
  nebeneinander — kollidiert bei kleiner Kachelgröße). Nur der Ast, der der
  tatsächlichen Route entspricht, in voller Deckkraft.

Konditional: nur sichtbar, wenn OsmAnd Spurdaten liefert. Auf dem
Mainscreen sitzt sie links der Nav-Pille, die dafür um 46px nach rechts
rutscht (animiert, ~600ms, mit Ruhephasen an beiden Enden — kein abruptes
Springen). Ohne Spurdaten bleibt die Nav-Pille an ihrer zentrierten
Grundposition (`x=150`).

### Fahrzustand-Icon

Vier Zustände, alle fahrradbezogen (nicht abstrakt — ein einzelnes Laufrad
allein liest sich bei 34px nicht zuverlässig als „Fahrrad"):

| Zustand | Motiv | Gedanke |
|---|---|---|
| Fahren | Fahrrad (2 Laufräder + Rahmen) | Grundzustand |
| Cruise | dasselbe Fahrrad, kleiner + Lupe oben rechts | bewusst langsam, am Suchen (Sightseeing, Eisdiele, Brunnen) statt am Fahren |
| Stop | Stoppschild (Oktagon) | kurzer, unfreiwilliger Halt (Ampel) |
| Pause | Tasse | längere, selbst gewählte Rast |

EEZ-Assets bereits vorhanden: `img_rr_icon_state_power` (Fahren),
`img_rr_icon_state_coasting` (Cruise), `img_rr_icon_state_stop`,
`img_rr_icon_state_break` (Pause) — 34×34, `ALPHA_8BIT`. Pfad-Referenz für
das Fahrrad-Grundmotiv (Konstruktion, falls ein Icon neu gebaut werden
muss):

```
Laufräder: Kreis r=3.2 bei (-5.5, 3) und (5.5, 3)
Rahmen:    M-5.5,3 L-0.8,3  M-0.8,3 L-3,-4.3  M-0.8,3 L3.8,-4.8
           M3.8,-4.8 L5.5,3  M-3,-4.3 L3.8,-4.8
Sattel/Lenker-Kappen: M-4.2,-4.6 L-1.8,-4   M2.8,-5.6 L4.8,-4.2
```

### Straßenqualität-Indikator

**Umgesetzt** als `rr_line_rq`, ein einzelnes `LVGLLineWidget`, vom Nutzer
direkt in EEZ Studio angelegt (2026-09-26) — ersetzt eine ältere,
nie gebaute Spezifikation mit fünf kleinen Quadraten unter dem
Fahrzustand-Icon (siehe Git-Historie dieser Datei, falls die als Referenz
noch mal interessant ist). Kein Icon, keine Beschriftung — die Farbe *ist*
die Information, bewusst minimal wie im Konzept vorgesehen („nur die
Qualitätsklasse als kleiner farbiger Indikator", BMI160-basiert,
Konzeptdokument liegt außerhalb des Repos unter
`~/.claude/plans/plane-mir-ein-konzept-linear-crayon.md`).

- Sitzt im Container `rr_group_rq_mode` direkt unter dem Fahrzustand-Icon
  (`align=BOTTOM_MID`, Widget `140×5`, Linie `0,0 → 115,0`, `line_width=5`) —
  ein dünner Balken knapp über der Bildschirmkante.
- Klasse `n` (1–5) → `line_color` auf die entsprechende der fünf
  HF-Zonenfarben (§1, `ZONE_BLUE`…`ZONE_RED`, `theme_colors[26..30]`) —
  glatt = blau, sehr rau = rot, dieselbe Reihenfolge wie die HF-Leiste.
- Klasse 0 (noch keine Daten oder zu langsam/Stillstand) → Widget bleibt
  sichtbar, `line_color` auf `RRBrass` (derselbe Messington wie der Rest
  der UI) statt einer der fünf Zonenfarben — bewusst kein Ein-/Ausblenden
  bei jeder Ampelpause, die Linie ist durchgehend präsent, nur die Farbe
  trägt die Information. Identisch mit dem JSON-Default in EEZ Studio, vor
  der ersten echten Klasse.
- **Auf allen vier Screens identisch** (seit 2026-09-27): `rr_group_rq_mode`
  (Container `BOTTOM_MID`, Offset `(0,-5)`, `197×40`, darin Fahrzustand-Icon
  `TOP_MID` und `rr_line_rq` `BOTTOM_MID`) ist 1:1 als `rq_group_rq_mode`
  (RQ-Ride), `rrnav_group_rq_mode` (Navigation) und `rrset_group_rq_mode`
  (Einstellungen) kopiert. Icon und Linienfarbe setzen
  `ui_RimRidgeUpdateStateIcon()`/`ui_RimRidgeUpdateRoadQuality()` auf allen
  Kopien. Tipp auf die Gruppe öffnet den RQ-Ride-Screen, auf dem RQ-Ride-Screen
  selbst geht es damit zurück zum Mainscreen (Action `GoToRq`). Änderungen an
  der Gruppe immer auf allen vier Screens gleich machen.
- Verdrahtet in `ui_RimRidgeUpdateRoadQuality()`
  (`src/ui/RimRidgeCustFunc.cpp`) — Datenkette
  `RoadQuality`/`I2CSensors` → `Statistics::updateRoadQualityUi()` →
  `UIFacade::updateRoadQuality()` stand schon vorher, nur der Widget-Teil
  war der offene Punkt.

## 5. Icon-Bibliothek

| Icon | EEZ-Asset | Konstruktion |
|---|---|---|
| WLAN | `img_rr_icon_wifi` | Punkt + zwei konzentrische offene Bögen |
| GPS | `img_rr_icon_gps` | Pin-Kontur (Bézier) + Kreis |
| Akku | `img_rr_icon_battery` | Rechteck + Pol-Nase, Füllstands-Rechteck innen (`SAGE` = gut) |
| Nav-Abbiegepfeil | `img_rr_icon_turn` | gewinkelte Linie + Pfeilspitze, 64×64 (größer, da Hero-Element auf dem Nav-Screen) |
| Cadence | `img_rr_icon_cadence` | offener Kreisbogen + Pfeilspitze (rotierende Bewegung) |
| Watt | `img_rr_icon_power` | Blitz-Polygon |
| Temperatur | `img_rr_icon_temp` | Thermometer (Rechteck + Kreis-Bulb) |
| Höhe | `img_rr_icon_height` | Bergsilhouette (Polyline) — Rückgriff auf das Bootlogo-Ridge-Motiv, `SAGE` |
| Steigung | `img_rr_icon_gradient` | diagonale Linie + Winkel-Pfeilspitze |
| Herzfrequenz | `img_rr_icon_heart` | zusammengesetzte Bézier-Herzform |
| Pause (Button) | `img_rr_icon_pause` | zwei vertikale Balken |
| Einstellungen (Button) | — (noch kein eigenes `rr_icon_`-Asset, siehe unten) | Kreis + Nabe + 6 radiale Zähne, 60°-Abstand |
| Fahrzustand ×4 | `img_rr_icon_state_{power,coasting,stop,break}` | siehe §4 |
| Neustart (Settings) | `img_rr_icon_cadence` (wiederverwendet) | Cadence-Icon (Kreisbogen + Pfeilspitze) als „rotierend/zurücksetzen" |
| Tiefschlaf (Settings) | `img_rr_icon_moon` | Vollkreis r=8 minus versetzter Kreis r=7 bei (4.5,−2.5) („ausgestanzte" Mondsichel), `ALPHA_8BIT` |
| Stoppuhr (Fahrzeit) | `img_rr_icon_stopwatch` | Kreis + kleine Krone/Taste oben + zwei Zeiger — siehe §4 „Fahrzeit / Uhrzeit" |
| Uhr (Uhrzeit) | `img_rr_icon_clock` | wie Stoppuhr, aber ohne Krone — teilt sich denselben Widget-Slot mit dem Stoppuhr-Icon |
| Wegzähler (RQ-Ride) | noch nicht exportiert | Lineal-Motiv: waagrechte Linie + vier senkrechte Teilstriche (bewusst abstrakt wie das Cadence-/Steigungs-Icon, nicht wörtlich „Kilometerzähler") |
| Aufnahme-Taste (RQ-Ride) | noch nicht exportiert | Bereit-Zustand: Kreis + Ring + kleiner roter Punkt (Kamera-/Record-Sprache); Aufnahme-Zustand (nicht als Bild gezeichnet, nur beschrieben): Kreis voll rot + helles Stopp-Quadrat, dickerer roter Rahmen statt `BRASS`. Rot = `ZONE_RED`/`#C1604A`, dieselbe Zweitverwendung wie bei den Log-Level-Badges im Web-Dashboard (`--rr-err`) |

`img_settings_icon` existiert bereits (48×48, `TRUE_COLOR_ALPHA` — anderes
Format als der Rest, volltonig statt Alpha-Maske), das Icon des
Settings-Buttons auf dem Mainscreen. Vor Wiederverwendung in EEZ Studio
prüfen, ob der Export sauber ist (ein Test-Dekodieren ergab Streifen im
oberen Teil); die Zahnrad-Konstruktion oben ist der Ersatz, falls nötig.

## 6. Screens

### Mainscreen — [`mainscreen.svg`](mainscreen.svg)

Reihenfolge von innen nach außen: Speed (Zentrum, 130px, dominant) →
Cadence/Watt (flankierend, schmale Spalten bei x=80/400) →
Temp/Höhe/Steigung (Reihe bei y≈325) → HF-Wert + Zonenband (y≈353–380) →
Fußzeile (Pause-Taste links, Settings-Taste rechts, dazwischen eine
gemeinsame Spalte `rr_tour_pill` bei x173–307/y378–480: Distanz →
Modus-Chip → Fahrzustand-Icon 34px → Straßenqualität-Indikator, alle
vier senkrecht gestapelt statt nebeneinander — siehe §4 „Straßenqualität-
Indikator" für die Begründung) → außen der Speed-Arc mit Ø-Marker → oben
Statuszeile (WLAN/GPS/Akku) und Nav-Pille, links davon konditional die
Spur-Anzeige, darunter die Fahrzeit/Uhrzeit (y≈157, siehe §4) in der Lücke
zwischen Nav-Pille und Speed-Zahl.

### Navigationsscreen — [`navscreen.svg`](navscreen.svg)

Blendet sich automatisch als Vollbild ein, wenn eine Nav-Anweisung
ansteht, und danach wieder aus (zurück zum Mainscreen). Manuell: Tipp auf
die Nav-Pille öffnet ihn, Wischen nach rechts schließt ihn.

Aufbau: Distanz-Ring (gleicher Mechanismus wie der Speed-Arc, siehe §4) →
großes Abbiege-Icon + Entfernung (Zahl in zwei Größen: `84px` Wert + `34px`
Einheit als `tspan`, damit die Einheit nicht gleich groß wie die Zahl
wirkt) → Straßenname darunter, gedämpft → übernächster Hinweis als kleines
Badge oben rechts, bewusst außerhalb der Hauptlesespur → Geschwindigkeit +
Steigung als mittelgroßes Paar → Herzfrequenz ganz unten, hier nur Farbe +
Zahl, kein Zonenband (auf diesem Screen sekundär). Spur-Anzeige oben,
gleiches Widget wie auf dem Mainscreen, hier bei voller Größe (`scale
1.2` statt `0.75`, mehr Platz vorhanden).

Bewusst **ohne** Fahrzeit/Uhrzeit-Widget: der Screen ist eng auf die
anstehende Abbiegung zugeschnitten, und für die ambiente Info „wie lange
fahre ich schon" ist der kurz eingeblendete Nav-Screen der falsche Ort —
die gehört auf den Mainscreen, wo sie durchgehend sichtbar ist.

### Einstellungen — [`settings.svg`](settings.svg)

**Umgesetzt** als `RimRidgeSettings` (2026-09-27). Gegenüber der SVG-Studie
(Zahnrad, Titel, Build, IP, Neustart, Tiefschlaf) kamen aus
[`USABILITY-TODO.md`](../USABILITY-TODO.md) §2/§3 WLAN-Reconnect,
IMU-Kalibrierung und Referenzfahrt dazu. Neustart und Tiefschlaf stehen deshalb
nebeneinander statt untereinander. Kein Statuszeilen-Kopf, aber unten die
gemeinsame Fahrzustand/RQ-Gruppe (§4). Öffnen: Tipp auf `rr_btn_settings`;
zurück: Wischen in beliebige Richtung. Logik: `src/ui/RimRidgeSettingsCustFunc.cpp`.

| Element | Box (x, y, w, h) | Font/Farbe | Inhalt |
|---|---|---|---|
| `rrset_ic_gear` | 216, 24, 48, 48 | `SettingsIcon`, `BRASS` | |
| `rrset_title` | TOP_MID, y=78 | 14, Abstand 3, `BRASS` | „EINSTELLUNGEN" |
| `rrset_build_caption` / `_val` | TOP_MID, y=108 / 126 | 14 `MUTED` / 18 `PARCHMENT` | `v0.0.2-88 (51c4300) #1277` = Tag-Commits (Hash) #Build-Nr. |
| `rrset_ip_caption` / `_val` | TOP_MID, y=156 / 174 | 14 `MUTED` / 22 `PARCHMENT` | IP, „… (AP)", oder „WLAN aus" / „verbinde ..." / „Verbindung verloren" / „kein WLAN gefunden" |
| `rrset_btn_wifi` | 135, 208, 210, 40 | Pille, 18 | „WLAN verbinden"; `DISABLED` („verbinde ...", „WLAN verbunden") solange nicht offline |
| `rrset_btn_cal` / `rrset_btn_ref` | 50/245, 258, 185, 40 | Pille, 18 | „Kalibrieren" / „Referenzfahrt" (läuft: „Abbrechen") |
| `rrset_cal_status` / `rrset_ref_status` | 50/245, 302, 185, 2 Zeilen | 14, `MUTED`; läuft `PARCHMENT`, Fehler `ZONE_RED` | „kalibriert 25.09.", „still halten ... 67 %", „Fehler: bewegt" / „Standard (150 mg)", „23 / 60 s ab 12 km/h", „142 mg 27.09." |
| `rrset_btn_reset` / `rrset_btn_sleep` | 85/245, 346, 150, 40 | Pille mit Icon + 18 | Long-Press |
| `rrset_hint` | TOP_MID, y=392 | 14 `MUTED` | „lange drücken" → „loslassen: Neustart" |

Pillen: `PANEL_BG` voll deckend, Rahmen `BRASS` 2 px bei `border_opa` 90,
`PRESSED`: Rahmen voll + Fläche `TOUR_BG`, `DISABLED`: Rahmen `border_opa` 40,
Text `MUTED`.

**Entschiedene Punkte:**

- Neustart/Tiefschlaf lösen per **Long-Press** aus und werden erst **nach dem
  Loslassen** ausgeführt: Tiefschlaf weckt über den Touch-Interrupt, ein noch
  aufliegender Finger würde sofort wieder wecken. Vorher werden Distanz (NVS) und
  Log-Dateien gesichert.
- Kalibrieren und Referenzfahrt starten per einfachem Tipp. Beides überschreibt
  erst bei Erfolg etwas; die Referenzfahrt lässt sich mit demselben Knopf
  abbrechen.
- Der WLAN-Knopf verbindet nur neu, wenn das WLAN aus ist (nach 100 s ohne
  Verbindung oder nach Verbindungsverlust schaltet es sich ab). AP-Modus und
  mehrere Zugangspunkte sind noch offen.
- Keine Helligkeitsregelung (Entscheidung 2026-09-27: das dunkle Design ist auch
  nachts nicht zu hell; ggf. später ein kontraststärkeres Tag-Design).

### RQ-Ride-Screen — [`rqscreen.svg`](rqscreen.svg)

Ein eigener Ride-Screen fürs gezielte Sammeln von Referenzdaten zur
Straßenqualität: Untergrund und subjektive Qualität von Hand markieren,
während optional eine Detailaufzeichnung läuft — ergänzt die automatische
Klassifikation (§4 „Straßenqualität-Indikator") um echte Ground-Truth-Daten,
z. B. für `osm_validate.py` (Konzeptdokument, siehe dort) oder zum
Nachschärfen der Schwellwerte in `RQ::Config::classThr`. Wie beim
Einstellungen-Screen bewusst **ohne** Statuszeile (WLAN/GPS/Akku) — der
Platz geht an die Bedienelemente unten, die auf diesem Screen den Vorrang
haben.

Aufbau von oben nach unten:

| Element | y (Zentrum/Baseline) | Größe/Wert | Hinweis |
|---|---|---|---|
| Nav-Pille | 30–70 | wie Mainscreen | von y=92 auf y=30 verschoben (§3-Formel an der neuen Höhe geprüft) |
| Geschwindigkeit | 140 (Wert) / 163 (Einheit) | 60px, `PARCHMENT_BRIGHT` | „mittelgroß", gleichrangig mit RQ-Index |
| RQ-Index | 140 (Wert) / 163 (Caption) | 60px, Zonenfarbe (§1) | Ziffer 1–5 in der Farbe der aktuellen Klasse, kein Ausblenden bei 0 (Konvention wie `rr_line_rq`) |
| Puls | 185 (Icon) / 215 (Wert) | 22px | Herz-Icon wiederverwendet, kein Zonenband (zu klein für diesen Screen) |
| Wegzähler | 184 (Icon) / 215 (Wert) | 20px | neues Lineal-Icon (§5), Wert+Einheit ein String wie die Mainscreen-Distanz |
| Untergrund-Pillen | 240–318 | 6× `100×36`, `rx=18` | Text-Pillen ohne Icon (Begründung: §2-Lektion zur Icon-Mindestgröße), 2×3-Raster: Asphalt/Schotter/Waldweg/Feldweg/Pflaster/Sonstiges |
| Qualität | 360 | 4× `r=20` | Segmentierter Wähler (§4), Stufen 1–4 |
| Aufnahme-Taste | 166 | `r=28` | seit 2026-09-27 in der freien Mittelspalte zwischen Geschwindigkeit und RQ-Index (`212,138,56,56`); unten sitzt jetzt die gemeinsame Fahrzustand/RQ-Gruppe (§4) |

**Warum ein 4-stufiger Wähler statt der automatischen 5 Klassen:** die
automatische Klassifikation (`RQ::IntervalResult::roadClass`, 1–5) und die
subjektive Nutzerbewertung hier sind bewusst getrennte Skalen — vermischen
würde verschleiern, ob eine Abweichung an der Wahrnehmung oder am Algorithmus
liegt. 4 statt 5 Stufen, weil eine Nutzerbewertung „exakt in der Mitte"
ohnehin selten trennscharf ist; falls sich in der Praxis zeigt, dass 5
Stufen für den Soll/Ist-Vergleich nötig sind, lässt sich ein fünfter Kreis
mit gleichem Abstand (60px) ergänzen, ohne das Muster zu ändern.

**Stand 2026-09-27:** Speicherung und Verdrahtung sind umgesetzt. Das
manuelle Label landet als eigener Satztyp (`LogRec::Label`, Satztyp 3) im
Binärlog, zusätzlich in jedem Stoß-Satz und jedem Rohdaten-Block (siehe
`src/LogRecords.h`, `Tools/bikelog/record.py`); Firmware-API dafür ist
`I2CSensors::setRoadLabelSurface()`/`setRoadLabelQuality()`/
`startRoadCapture()`/`stopRoadCapture()`/`getRoadLabelState()`. Die 6
Untergrund-Pillen, die 4 Qualitätskreise und die Aufnahme-Taste sind über
direkte `lv_obj_add_event_cb()`-Aufrufe verdrahtet (nicht als EEZ-Actions —
11 fast identische Actions für reine Logik ohne Screen-Wechsel hätten
keinen Mehrwert gehabt), siehe `ui_RimRidgeRQInitLabelControls()`/
`ui_RimRidgeRQUpdateLabel()` in `src/ui/RimRidgeRQCustFunc.cpp`. Start/Stopp
ist ein einfacher Tap (kein Long-Press) — ein erneuter Tap auf die aktive
Pille/den aktiven Kreis setzt sie auf "keiner" zurück, ein Tap auf die
Aufnahme-Taste stoppt einen laufenden Mitschnitt. Die Anzeige liest den
Zustand dabei immer aus `getRoadLabelState()`, nie aus dem Tap selbst, weil
ein Mitschnitt nach spätestens 30 min von selbst endet und das Label einen
Neustart nicht übersteht.

## 7. Hinweise für die EEZ-Studio-Umsetzung

- **Bild-Icons:** `LV_IMG_CF_ALPHA_8BIT`, per `lv_obj_set_style_img_recolor`
  zur Laufzeit auf `RR_BRASS` (oder die jeweilige Zonenfarbe) eingefärbt —
  nicht Farbe ins Bitmap backen. Mindestgröße 32–34px, siehe §2.
- **Arc-Widgets:** beide Ringe (Speed, künftig Distanz) sind Standard-`lv_arc`
  mit den Winkeln/Werten aus §4 — kein Custom-Drawing nötig.
- **Namenskonvention** (aus dem bestehenden Mainscreen ableitbar):
  `rr_<element>` für Widgets, `rr_ic_<name>` für Icons, `rr_btn_<name>` für
  Buttons, `_val`/`_unit`-Suffix für Wert/Einheit-Paare, `_pill`/`_bg` für
  Chip-Flächen. Für die neuen Screens fortführen, z. B. `rr_settings_build`,
  `rr_btn_reset`, `rr_btn_deepsleep`.
- **Screens:** `SCREEN_ID_RIM_RIDGE`, `SCREEN_ID_RIM_RIDGE_NAV`,
  `SCREEN_ID_RIM_RIDGE_RQ`, `SCREEN_ID_RIM_RIDGE_SETTINGS`. Präfixe der
  Widgets: `rr_`, `rrnav_`, `rq_`, `rrset_`.
- **Prüfen auf dem Gerät:** Screenshot und synthetische Touches per
  `Tools/uishot.py` (siehe `doc/DEBUG.md`, „Remote UI testing").
- **Fahrzeit/Uhrzeit-Widget:** `rr_ic_time` (Icon) + `rr_time_val` (Text)
  auf `rim_ridge`. Welcher Zustand gilt, entscheidet die Firmware
  (`ui_RimRidgeUpdateTime()`).
