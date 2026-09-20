"""Maße für das schlanke TRGB-Gehäuse am Canyon CP0007 Gravel Cockpit CF.

Grundlage: cad/COCKPIT_ANALYSIS.md (Auswertung der Fotos in cad/TEMP_Fotos/).
Dort steht zu jedem Wert, ob er gemessen, geschätzt oder vorgegeben ist.

Konzept: Das Gehäuse sitzt rittlings auf dem Vorbau, hinter dem Lenkeranschluss.
Display oben, Vorbau in der Mitte (Loft-Ausschnitt), Akku unten. Abstützung
ausschliesslich am Vorbau -- der obere Lenkerbuegel ist als FLEX AREA
gekennzeichnet und darf nicht geklemmt werden.
"""

# --- Koordinatenkonvention ----------------------------------------------------
# Rechtshaendig, fahrradbezogen:
#   +X = vorne (Fahrtrichtung)
#   +Y = links (aus Fahrersicht)
#   +Z = oben
#
# Nullpunkt: Schaftachse (X=0, Y=0), Z=0 auf der Oberkante des Vorbaus
# in der Schaftachse. Das ist am Rad nachmessbar.

# --- Vorbau-Querschnitte ("Trapez") ------------------------------------------
# Der Ausschnitt wird als Loft ueber diese Stationen gebaut. Jede Station ist
# ein Trapez in der Y-Z-Ebene:
#   (x, breite_unten, breite_oben, hoehe, z_oberkante)
# x      = Position in Fahrtrichtung, relativ zur Schaftachse
# breite = Y-Ausdehnung des Vorbaus an dieser Stelle
# hoehe  = Z-Ausdehnung des Vorbaus an dieser Stelle
# z_oberkante = Oberkante des Vorbaus relativ zum Nullpunkt (Vorbau faellt
#               nach hinten ab, deshalb pro Station veraenderlich)
#
# ACHTUNG: Diese Zahlen sind bewusst grobe Naeherungen (Trapez statt echter
# Aero-Form). Der Nutzer hat das so vorgegeben -- die Passung macht spaeter
# die Moosgummi-Zwischenlage, nicht die CAD-Genauigkeit. Zum Verfeinern
# einfach hier weitere Stationen ergaenzen oder die Werte korrigieren.
#
# WICHTIG zur X-Ausdehnung: Der Vorbau endet hinten an der Schaftklemmung.
# Hinter der Schaftachse (X < ca. -25) ist kein Vorbau mehr, sondern freier
# Raum ueber dem Oberrohr -- und dort ist auch kein Lenkerbuegel darueber.
# Genau dorthin gehoert das Display (tiefe Oberkante, nah am Auge), und dort
# koennen die hinteren Schrauben durch, ohne am Vorbau vorbei zu muessen.
STEM_SECTIONS = [
    # x     b_unten b_oben  hoehe  z_ok
    (-22.0,  44.0,  50.0,   32.0,   0.0),   # hinten, Schaftklemmung (breiteste Stelle)
    (  5.0,  42.0,  48.0,   30.0,   2.0),
    ( 30.0,  38.0,  43.0,   28.0,   4.5),
    ( 60.0,  34.0,  39.0,   26.0,   7.0),   # vorne, Richtung Lenkeranschluss
]

# Hinter dieser X-Position ist kein Vorbau mehr -- dort darf das Gehaeuse
# durchgehend Material haben (Schraubkanaele, Elektronik).
STEM_REAR_END_X = STEM_SECTIONS[0][0]

# Weiche Zwischenlage (Moosgummi/Isoliermaterial) zwischen Druckteil und
# Carbon. Der Ausschnitt wird um diesen Betrag groesser geloftet.
FOAM_LINER_THICKNESS = 3.0

# --- Bauraum (Nutzervorgaben) -------------------------------------------------
# Von oben nach unten. Das Oberteil darf 48mm hoch werden -- inklusive
# Sonnenschutz. Die Flex-Zone darueber federt durch, deshalb Reserve.
TOP_HEIGHT_MAX = 48.0
TOP_FLEX_RESERVE = 4.0
TOP_HEIGHT_TARGET = TOP_HEIGHT_MAX - TOP_FLEX_RESERVE  # 44mm Entwurfsziel

BOTTOM_HEIGHT_MAX = 18.0   # Akku + Verschraubung unter dem Vorbau

# --- Lage der Baugruppen in X -------------------------------------------------
# Der Traeger klammert sich ausschliesslich VOR der Schaftachse an den Vorbau.
# Damit bleibt er zwangslaeufig aus dem Schwenkbereich des Steuerrohrs heraus.
TRAEGER_X_MIN = 8.0
TRAEGER_X_MAX = 64.0

# Das Displaygehaeuse sitzt oben und ragt nach HINTEN ueber das Steuerrohr --
# dort ist es hoch genug, um nicht zu kollidieren (vgl. IMG_3913).
DISPLAY_CENTER_X = 11.0

# Der Akku haengt unter dem Vorbau, vor der Steuerrohr-Freihaltung.
BATTERY_CENTER_X = 36.0

# --- T-RGB Displaymodul (rund, 2.1") -----------------------------------------
# Aus references/T-RGB-FULL-3D-2.1-Inches.stp (Hersteller-STEP von LilyGO,
# https://github.com/Xinyuan-LilyGO/LilyGo-T-RGB), selbst ausgemessen:
#   Gesamt-Bounding-Box 61,50 x 59,50 x 18,30 mm
#   groesster Zylinder  R 29,75 -> D 59,50 mm  (Aussenring oben)
#   Koerper darunter    D 56,40 mm
#   Ueberstand auf einer Seite bis x = -31,75 (ca. 2 mm ueber den Ring hinaus)
#   Stecker unten im Bereich z -11 .. -14,3
DISPLAY_DIAMETER = 59.5
DISPLAY_BODY_DIAMETER = 56.4
DISPLAY_HEIGHT = 18.3
DISPLAY_TAB_OVERHANG = 2.3   # einseitiger Ueberstand ueber den Ringdurchmesser
DISPLAY_FIT_CLEARANCE = 0.4

# Sichtbares rundes Sichtfenster im Deckel (Glas ist kleiner als der Ring)
DISPLAY_WINDOW_DIAMETER = 52.0

# --- Steuerrohr-Freihaltung ---------------------------------------------------
# Das Gehaeuse dreht beim Lenken um die Schaftachse, das Steuerrohr steht fest.
# Alles, was sich innerhalb eines Zylinders um die Schaftachse UND unterhalb der
# Steuersatz-Oberkante befindet, kollidiert bei irgendeinem Lenkeinschlag.
# Als Zylinder um die Drehachse formuliert gilt die Bedingung automatisch fuer
# jeden Einschlagwinkel, auch fuer den vollen.
#
# GESCHAETZT -- am Rad nachzumessen (Steuersatz-Deckel-Durchmesser und wie weit
# unter der Vorbau-Unterkante er sitzt):
HEADSET_TOP_Z = -32.0        # Oberkante Steuersatz, relativ zum Nullpunkt
HEADTUBE_KEEPOUT_R = 28.0    # Radius um die Schaftachse, inkl. Sicherheitsabstand

# --- Wandstaerken -------------------------------------------------------------
WALL_THICKNESS = 2.4
FLOOR_THICKNESS = 2.0
FIT_CLEARANCE = 0.3         # Spiel um das Displaymodul
SUNSHADE_HEIGHT = 6.0       # Sonnenschutz-Kragen ueber dem Display
SUNSHADE_WALL = 2.0

# --- Breite (Y) ---------------------------------------------------------------
# Wird nicht mehr fest vorgegeben: die Breite des Klammerteils ergibt sich in
# model.py aus Vorbau-Ausschnitt + Moosgummi + Schraubkanal + Wand
# (traeger_width_y()), die des Displaygehaeuses aus dem runden Modul.
# Schmal in Y ist die Voraussetzung dafuer, in X nach hinten bauen zu duerfen.

# --- Akku ---------------------------------------------------------------------
# Alte Zelle als Ausgangspunkt (aus Case_Grail_TRGB_Round_v2.FCStd, "Akku":
# 54,0 x 51,5 x 7,0 mm). Unter dem Vorbau ist der Bauraum flach und schmal,
# deshalb liegend und laengs eingebaut.
BATTERY_LENGTH = 54.0   # in X
BATTERY_WIDTH = 51.5    # in Y
BATTERY_HEIGHT = 7.0    # in Z
BATTERY_CLEARANCE = 0.5
BATTERY_WALL = 1.8

# --- Verschraubung: M3 mit Heatset-Inserts ------------------------------------
# Gewinde nie direkt im Druckteil, sondern immer als eingeschmolzene
# Messingbuchse. Das bestimmt die Mindestwandstaerke: Bohrung + Wand ringsum.
SCREW_CLEARANCE_D = 3.4      # Durchgang M3
SCREW_HEAD_D = 6.2           # Senkkopf M3
SCREW_HEAD_DEPTH = 2.0

INSERT_HOLE_D = 4.2          # Bohrung fuer M3-Heatset (Buchse ca. D4,0)
INSERT_DEPTH = 6.0           # Einschmelztiefe
INSERT_WALL = 2.4            # Material ringsum die Buchse
INSERT_BOSS_D = INSERT_HOLE_D + 2 * INSERT_WALL   # 9,0 mm -> Mindestdicke der Wange

# --- Kabeldurchfuehrung -------------------------------------------------------
CABLE_HOLE_D = 5.0  # Akku -> Elektronik, seitlich am Vorbau vorbei
