"""Masse fuer das FL-Gehaeuse (Forumslader-Variante) am Diamant Nhoma.

Erster Entwurf. Herkunft jedes Werts steht dabei:
  [VORGABE]   vom Nutzer genannt
  [ALT-FL]    aus dem alten FreeCAD-Modell doc/FreeCAD/TRGB-Handlebar-FL.FCStd
              (Skizzen ausgelesen, das Modell selbst rechnet nicht mehr neu)
  [DATENBLATT] aus Herstellerangaben
  [GESCHAETZT] am Rad bzw. am Bauteil nachzumessen

Konzept: Das Gehaeuse sitzt oben auf Vorbau und Lenker. Vorne umgreift der
Traeger den Lenker links und rechts neben der Vorbau-Klemmung (je eine
Klemmschale, Schrauben von unten), hinten liegt ein Fuss mit Gummi auf dem
Vorbau. Das Modul liegt oben, darunter ein Fach fuer den ESP32 (Bluetooth
Classic zum Forumslader), die Wago-Klemmen, Schalter und USB-C-Buchse.

Koordinaten (lokal, im Vorbau-System):
  Ursprung = Lenkerachse in der Mitte des Vorbaus
  +x = entlang der Vorbau-Achse nach vorne, +y = links, +z = senkrecht zur
  Vorbau-Achse nach oben
Fuer den STEP wird alles um den Vorbau-Winkel gedreht, dort ist +X vorne
waagerecht und +Z senkrecht nach oben.
"""

# --- Rad: Diamant Nhoma (Gen 3) ----------------------------------------------
# Laut Haendler-Datenblatt [DATENBLATT]: Vorbau "Bontrager Comp, 31.8 mm,
# Blendr-kompatibel, 7 Grad", Laenge je nach Rahmengroesse 60..100 mm;
# Lenker "Bontrager Elite Gravel", Klemmung 31,8; Gabelschaft 1 1/8".
BAR_D = 31.8
BAR_CLAMP_ZONE_HALF = 55.0      # so weit ist der Lenker noch 31,8 dick [GESCHAETZT]
BAR_TOPS_D = 23.8               # nur fuer den Bezugskoerper

STEM_LENGTH = 70.0              # Lenkerachse -> Schaftachse [GESCHAETZT: Groesse?]
STEM_ANGLE_DEG = 7.0            # Vorbau-Winkel gegen die Senkrechte auf den Schaft
HEAD_ANGLE_DEG = 71.5           # Lenkwinkel [GESCHAETZT]
STEM_NEGATIVE = True            # Vorbau "nach unten" montiert (-7 Grad) [GESCHAETZT]

STEM_BODY_WIDTH = 36.0          # Vorbau-Koerper in Y [GESCHAETZT]
STEM_BODY_TOP = 17.0            # Oberkante Vorbau-Koerper ueber der Achse [GESCHAETZT]
STEM_BODY_BOTTOM = -17.0
BAR_CLAMP_OD = 40.8             # Klemmung um den Lenker, Aussen-D [GESCHAETZT]
FACEPLATE_WIDTH = 46.0          # Klemmung/Frontplatte in Y [GESCHAETZT, 4-Schrauben-Platte]
FACEPLATE_BOLT_Y = 15.0         # Schrauben der Frontplatte [GESCHAETZT]
FACEPLATE_BOLT_Z = 11.5
FACEPLATE_BOLT_HEAD_D = 8.5
FACEPLATE_BOLT_HEAD_PROUD = 5.5  # so weit stehen die Koepfe vor der Platte

STEERER_D = 28.6                # 1 1/8"
STEERER_CLAMP_OD = 38.0
STEERER_CLAMP_HEIGHT = 40.0
TOP_CAP_D = 34.0
TOP_CAP_HEIGHT = 6.0            # Kappe + Schraubenkopf, ohne Spacer ueber dem Vorbau [GESCHAETZT]

# --- Displaymodul T-RGB 2.1" "Half Circle" (das ovale) [ALT-FL] ----------------
# Umriss aus "Ausschnitt T-RGB" (Sketch001): Kreis R 30,5, nach hinten ein
# trapezfoermiges Kinn mit 35 mm breiter Abflachung, 32,5 mm hinter der
# Kreismitte. Das Glas deckt den ganzen Umriss. Auf dem Foto
# (doc/IMG20230406075242-cropped.jpg) zeigt das Kinn zum Fahrer.
# Der Akku sitzt im Modul zwischen Anzeige und Platine [VORGABE].
MODULE_R = 30.5
MODULE_FLAT_HALF = 17.5
MODULE_FLAT_BACK = 32.5          # Abflachung hinter der Kreismitte
MODULE_TANGENT = (27.07, -14.04)  # Uebergang Kreis -> Kinn (y quer, x laengs, ab Kreismitte)
MODULE_DEPTH = 19.8              # Glas bis Rueckseite ("DatumPlane - Height T-RGB") [ALT-FL]
MODULE_CLEARANCE = 0.4
WINDOW_D = 55.0                  # Fenster ("Fenster Display", R 27,5) [ALT-FL]
# USB-C des Moduls: rechte Seite (-Y), 19 mm hinter bis 3,5 mm vor der
# Kreismitte, 10..20 mm unter der Glasoberseite ("Pocket - USB-C") [ALT-FL].
MODULE_USB_X = (-19.5, 3.5)
MODULE_USB_DEPTH = (10.0, 20.0)
USB_PLUG_SPACE = 7.5             # Platz neben dem Modul fuer einen flachen Winkelstecker

# --- ESP32 (Bluetooth Classic) -----------------------------------------------
# "Wemos ESP32 38-pin" [VORGABE]. Haendlerangaben fuer 38-polige DevKits
# streuen zwischen 48 x 26 und 55 x 28 mm; gezeichnet ist das groesste.
# Ohne Stiftleisten [VORGABE], flach auf den Boden des Traegers geklebt
# (bisher auf eine Plexiglasplatte quer unter dem T-RGB [VORGABE]).
ESP_LENGTH = 55.0                # quer (Y)
ESP_WIDTH = 28.5                 # laengs (X)
ESP_PCB = 1.6
ESP_PINS_BELOW = 0.0             # ohne Stiftleisten [VORGABE]
ESP_PARTS_ABOVE = 3.5            # WROOM-Abschirmung, Micro-USB
ESP_CLEARANCE = 0.4

# --- Einbauteile -------------------------------------------------------------
# USB-C-Einbaubuchse, kleine Bauform [VORGABE: "12 mm Bohrung?"]: M12-Gewinde,
# Bohrung wie beim Schalter 0,3 mm groesser, weil gedruckte Loecher schrumpfen.
USBC_HOLE_D = 12.3
SWITCH_HOLE_D = 12.3             # wie im runden Gehaeuse (D 12) [VORGABE dort]
PANEL_NUT_SPACE = 16.0           # Mutter muss sich drehen lassen (D 16)
PANEL_BODY_DEPTH = 14.0          # Platz hinter der Wand
PANEL_Y = 21.5                   # Abstand von der Mitte
# Kabel vom Forumslader: 3 x 1,5 mm^2 (5 V, 12 V Akku, GND) [VORGABE].
# H05VV-F 3G1,5 ist ca. 8,5 mm dick [GESCHAETZT]. Wasserdicht ueber eine
# Kabelverschraubung M16 x 1,5 (Klemmbereich ca. 4,5..10 mm) [VORGABE].
# In der Rueckwand ist dafuer kein Platz: die Mutter (SW 19, ueber Eck 22)
# reicht dort bis unter das Kinn des Moduls. Sie sitzt deshalb hinten in der
# linken Seitenwand, wo der Ring oberhalb des Fachs massiv ist.
CABLE_D = 8.5
GLAND_HOLE_D = 16.5              # M16 + 0,5 (gedruckte Loecher schrumpfen)
GLAND_NUT_SPACE = 23.0           # Mutter muss sich drehen lassen (ueber Eck 22)
GLAND_NUT_DEPTH = 7.0            # Platz innen hinter der Wand (Mutter ca. 5)
GLAND_SEAL_D = 24.0              # ebene Dichtflaeche aussen
GLAND_OUTSIDE_LENGTH = 22.0      # Hutmutter samt Koerper aussen [GESCHAETZT]
GLAND_X = -27.0
# Wago 221 fuer die Verteilung [ALT-FL "Pocket Kabelfuehrung / Wago 221"]
WAGO_ZONE = (13.0, 22.0, 9.0)    # x, y, z

# --- Gehaeuse ----------------------------------------------------------------
HALF_WIDTH = 40.0
FRONT_X = 28.0                   # Vorderkante (lokal)
WALL = 3.2
FRONT_CORNER_R = 18.0
REAR_CORNER_R = 8.0
MODULE_CENTER_X = -10.5          # Kreismitte des Moduls
REAR_GAP_TO_TOPCAP = 3.0         # Luft zwischen Rueckwand und Ahead-Kappe

CARRIER_BOTTOM_Z = 22.0          # 1,6 mm ueber der Lenkerklemmung (R 20,4) [GESCHAETZT]
CARRIER_THICKNESS = 4.0
BAY_HEIGHT = 16.5                # Fach unter dem Modul: Mutter D 16 + Luft
LEDGE_WIDTH = 2.5                # Auflage fuer das Modul
LEDGE_THICKNESS = 1.5
LIP = 1.6                        # Deckel ueber dem Glas
DECKEL_THICKNESS = 4.8           # M2x3-Buchse in 4 mm Bohrung + 0,8 Deckschicht [VORGABE CP0007]
TOP_CHAMFER = 2.0

# --- Klemmung am Lenker -------------------------------------------------------
CLAMP_Y_IN = 24.0                # neben der Frontplatte (46 breit) + 1 mm
CLAMP_Y_OUT = 35.0
CLAMP_HALF_X = 23.5
CLAMP_RUBBER = 0.5               # Gummiband im Sitz
CLAMP_SPLIT_GAP = 1.0            # Klemmspalt
CLAMP_BELOW = 21.0               # Unterschale reicht so tief unter die Lenkerachse
CLAMP_SCREW_X = 19.6
CLAMP_SCREW_LENGTH = 16.0

# Fuss hinten auf dem Vorbau
FOOT_X = (-41.0, -29.0)
FOOT_RUBBER = 1.0

# --- Verschraubung (wie CP0007) [VORGABE] ---------------------------------------
M2_CLEARANCE_D = 2.6
M2_HEAD_D = 4.4
M2_HEAD_HEIGHT = 2.0
M2_INSERT_HOLE_D = 3.2
M2_INSERT_DEPTH = 4.0
M2_ENGAGEMENT = 3.8
M2_WALL = 1.0
M3_CLEARANCE_D = 3.8
M3_HEAD_D = 6.2
M3_INSERT_HOLE_D = 4.2
M3_INSERT_DEPTH = 6.0
LID_SCREW_LENGTH = 20.0          # Ring -> Deckel, Kopf tief im Ring versenkt
BASE_SCREW_LENGTH = 6.0          # Traeger -> Ring

# Schraubenlagen (lokal x, y)
LID_SCREWS = ((24.0, 0.0), (-42.0, 33.0), (-42.0, -33.0))
BASE_SCREWS = ((24.0, 13.0), (24.0, -13.0), (-45.0, 10.0), (-45.0, -10.0))

# --- Farben im STEP ---------------------------------------------------------------
COLOR_FRAME = (0.25, 0.25, 0.27)
COLOR_ELECTRONICS = (0.85, 0.10, 0.10)
PART_COLORS = {
    "fl_traeger": (0.15, 0.45, 0.90),     # blau
    "fl_ring": (1.00, 0.80, 0.10),        # gelb
    "fl_deckel": (0.20, 0.75, 0.35),      # gruen
    "fl_klemme_links": (0.10, 0.80, 0.85),  # tuerkis
    "fl_klemme_rechts": (0.10, 0.80, 0.85),
}
