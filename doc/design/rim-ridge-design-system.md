# Rim & Ridge — Design System

Visuelle Identität des TRGB-BikeComputers: ein rundes 480×480-Display
(GC9A01-Klasse-TFT), LVGL-UI. Zurückhaltend, dünne Linien, ein Akzentton
(Messing) auf Anthrazit. Named nach dem Bootlogo-Motiv — der Bildschirmrand
selbst ist die Felge.

**Status:**

| Screen | Datei | Zustand |
|---|---|---|
| Mainscreen | [`mainscreen.svg`](mainscreen.svg) | **implementiert**, `EEZStudio/TRGB-BikeComputer.eez-project`, Screen `rim_ridge` |
| Navigationsscreen | [`navscreen.svg`](navscreen.svg) | **implementiert**, Screen `rim_ridge_nav` (`SCREEN_ID_RIM_RIDGE_NAV`) |
| Einstellungen | [`settings.svg`](settings.svg) | Spezifikation, noch nicht in EEZ Studio angelegt |

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
exportieren, nicht als `LV_IMG_CF_INDEXED_1BIT` wie die älteren,
ungenutzten Assets in `src/ui/img/state-icons.c`.

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
| Neustart (Settings) | noch nicht exportiert | identisch zum Cadence-Icon (Kreisbogen + Pfeilspitze) — Wiederverwendung als „rotierend/zurücksetzen" |
| Tiefschlaf (Settings) | noch nicht exportiert | Vollkreis + versetzter Kreis in Flächenfarbe („ausgestanzte" Mondsichel) |
| Stoppuhr (Fahrzeit) | `img_rr_icon_stopwatch` | Kreis + kleine Krone/Taste oben + zwei Zeiger — siehe §4 „Fahrzeit / Uhrzeit" |
| Uhr (Uhrzeit) | `img_rr_icon_clock` | wie Stoppuhr, aber ohne Krone — teilt sich denselben Widget-Slot mit dem Stoppuhr-Icon |

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
Fußzeile (Pause-Taste, Fahrzustand-Icon 34px, Distanz, Modus-Chip,
Einstellungen-Taste) → außen der Speed-Arc mit Ø-Marker → oben
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

Erster Ausbauschritt, bewusst minimal: Zahnrad + Screen-Titel oben, zwei
Info-Zeilen (Build, IP-Adresse) als Label/Wert-Paar, zwei Aktions-Pillen
(Neustart, Tiefschlaf) mit Icon + Beschriftung.

| Element | x | y | Größe | Font/Farbe |
|---|---|---|---|---|
| Zahnrad | 240 | 88 | r=9·1.5 (Zähne bis r=18·1.5) | `BRASS` |
| Titel „EINSTELLUNGEN" | 240 | 135 | 15px, letter-spacing 4 | Mono, `BRASS` |
| Label „BUILD" | 240 | 180 | 12px | Mono, `MUTED` |
| Wert Build-String | 240 | 210 | 24px | Mono, `PARCHMENT` |
| Label „IP-ADRESSE" | 240 | 252 | 12px | Mono, `MUTED` |
| Wert IP-Adresse | 240 | 282 | 24px | Mono, `PARCHMENT` |
| Pille „Neustart" | 135–345 | 320–360 | h=40, rx=20 | `PANEL_BG`/`BRASS`-Rahmen |
| Pille „Tiefschlaf" | 135–345 | 375–415 | h=40, rx=20 | `PANEL_BG`/`BRASS`-Rahmen |

**Für spätere Erweiterung:** weitere Info-Zeilen lassen sich zwischen
IP-Adresse (y=282) und der ersten Pille (y=320) einschieben — bei y≈300
ist noch komfortabel Platz für ein bis zwei weitere Label/Wert-Paare, bevor
es an der Kreisrundung eng wird (§3-Formel vorher gegenrechnen). Wird die
Liste länger, auf ein scrollbares `lv_obj` mit vertikalem Flow wechseln
statt weiter manuell zu positionieren.

**Offene Punkte vor der Umsetzung** (bewusst nicht Teil dieser Spezifikation,
aber vor dem Verdrahten in EEZ Studio zu klären):

- Neustart und Tiefschlaf sind folgenreich (Reboot bzw. Power-Down) — vor
  dem Verdrahten der echten Aktion überlegen, ob ein Long-Press oder ein
  Bestätigungsschritt nötig ist, statt eines einfachen Tap. Betrifft dieselbe
  offene Frage wie bei der bestehenden „Pause"-Taste auf dem Mainscreen.
- Build-String-Format (Version + Git-Hash? nur Datum? `build_versioning.py`
  im Repo-Root generiert vermutlich schon etwas Passendes — dessen Format
  übernehmen statt ein neues zu erfinden).
- IP-Anzeige setzt voraus, dass WLAN/STA-Modus überhaupt aktiv ist (wofür
  genau — OTA? Web-Interface? `WifiWebserver.cpp` existiert im Repo) — Screen
  sollte einen Leerzustand („nicht verbunden") vorsehen, nicht nur die
  Erfolgs-Anzeige.

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
- **Neuer Screen `SCREEN_ID_SETTINGS`:** fehlt noch als EEZ-Screen
  (vorhanden: `SCREEN_ID_RIM_RIDGE`, `SCREEN_ID_RIM_RIDGE_NAV`).
- **Fahrzeit/Uhrzeit-Widget:** `rr_ic_time` (Icon) + `rr_time_val` (Text)
  auf `rim_ridge`. Welcher Zustand gilt, entscheidet die Firmware
  (`ui_RimRidgeUpdateTime()`).
