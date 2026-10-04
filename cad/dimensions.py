"""Maße für das schlanke TRGB-Gehäuse am Canyon CP0007 Gravel Cockpit CF.

Grundlage: cad/COCKPIT_ANALYSIS.md (Auswertung der Fotos in cad/TEMP_Fotos/).
Dort steht zu jedem Wert, ob er gemessen, geschätzt oder vorgegeben ist.

Konzept: Das Gehäuse sitzt rittlings auf dem Vorbau, hinter dem Lenkeranschluss.
Display oben, Vorbau in der Mitte (Loft-Ausschnitt), Akku unten. Abstützung
ausschliesslich am Vorbau -- der obere Lenkerbuegel ist als FLEX AREA
gekennzeichnet und darf nicht geklemmt werden.

Die Grundmasse (Vorbau, Lage von Display und Traeger) stammen aus dem vom
Nutzer konstruierten FreeCAD_SimpleModel.FCStd.
"""

# --- Koordinatenkonvention ----------------------------------------------------
# Rechtshaendig, fahrradbezogen:
#   +X = vorne (Fahrtrichtung)
#   +Y = links (aus Fahrersicht)
#   +Z = oben
#
# Nullpunkt: Schaftachse (X=0, Y=0), Z=0 auf der Oberkante des Vorbaus
# in der Schaftachse. Das ist am Rad nachmessbar.

# --- Vorbau ----------------------------------------------------------------
# Am Rad nachgemessen: 50,6 mm breit, 33,2 mm hoch nah am Steuerrohr, vorne
# nur noch 31 mm. Oben und unten ist er verrundet (ungleichmaessige Kurven,
# oben enger als unten). Die Werte hier dienen nur dem Bezugskoerper fuer
# Pruefung und Darstellung.
STEM_WIDTH = 50.6
STEM_HEIGHT = 33.2

# Hinteres Ende des Vorbaus, gemessen ab der Schaftachse. GESCHAETZT (Schaft
# 1 1/4" plus Klemmwand) -- alle X-Lagen unten beziehen sich darauf und
# wandern mit, wenn der Wert am Rad nachgemessen wird.
STEM_REAR_END_X = -22.0

# Ausschnitt im Traeger [VORGABE]: 51 breit, 35 hoch (Teller bis Akkudeckel).
# Die Luft zum Vorbau fuellt Moosgummi; die genaue Hoehe wird spaeter ueber
# den Akkudeckel eingestellt.
CAVITY_WIDTH = 51.0
CAVITY_HEIGHT = 35.0
# Klemmweg [VORGABE]: Die Wangen sind kuerzer als der Ausschnitt hoch ist. Der
# Akkudeckel steckt mit seinen Kurven zwischen den Wangen und laesst sich um
# CLAMP_TRAVEL verschieben; zwischen Wange und Deckel bleibt ein Spalt, ueber
# den die Stapelschrauben den Vorbau klemmen. CAVITY_HEIGHT ist die gezeichnete
# Stellung, CAVITY_HEIGHT_MIN die mit Deckel auf Anschlag.
CAVITY_HEIGHT_MIN = 31.0
CLAMP_TRAVEL = 10.0
# Fuehrung fuer den Klemmweg: je Seite eine Zunge auf dem Akkudeckel, die in
# einem Schlitz in der Unterseite der Wange laeuft.
GUIDE_THICKNESS = 3.0
GUIDE_LENGTH = 10.0
GUIDE_ENGAGE_MIN = 2.0          # so tief steckt die Zunge noch bei groesstem Spalt
GUIDE_GAP = 0.2                 # Luft je Seite im Schlitz
# Oben, zwischen Teller und Wangen, folgt der Ausschnitt dem Vorbau mit einem
# Radius -- das versteift den Traeger.
CAVITY_TOP_RADIUS = 5.0
# Unten sitzt die Rundung am Akkudeckel, damit sich der Traeger von oben
# aufstecken laesst: eine Kurve, deren Radius von der Wange (senkrecht) zur
# Deckelflaeche (waagerecht) linear waechst.
AKKU_DECKEL_CURVE_R_WALL = 5.0
AKKU_DECKEL_CURVE_R_FLOOR = 10.0
AKKU_DECKEL_CURVE_GAP = 0.2     # Luft zwischen Kurve und Wange

# --- Schnittstelle zum Displaygehaeuse [GEDRUCKT] -------------------------------
# Das Displaygehaeuse ist gedruckt. Diese beiden Lagen waren frueher aus der
# Wangenbreite abgeleitet und sind jetzt fest; der Traeger richtet sich danach.
UPPER_SCREW_Y = 34.783          # lange Schrauben: Abstand von der Mittelebene
CABLE_RISER_X = 30.0            # Steigbohrungen von Buchse und Schalter

# --- Bauraum (Nutzervorgaben) -------------------------------------------------
# Von oben nach unten. Das Oberteil darf 48mm hoch werden -- inklusive
# Sonnenschutz. Die Flex-Zone darueber federt durch, deshalb Reserve.
TOP_HEIGHT_MAX = 48.0
TOP_FLEX_RESERVE = 4.0
TOP_HEIGHT_TARGET = TOP_HEIGHT_MAX - TOP_FLEX_RESERVE  # 44mm Entwurfsziel

BOTTOM_HEIGHT_MAX = 18.0   # Akku + Verschraubung unter dem Vorbau

# --- Lage der Baugruppen in X -------------------------------------------------
# Aus FreeCAD_SimpleModel.FCStd uebernommen, jeweils als Abstand vom hinteren
# Vorbau-Ende nach vorne.
#
# Die Wangen des Traegers (und alles, was unter den Vorbau reicht) beginnen
# erst vor dem Steuerrohr. Wie lang der Traeger wird, ergibt sich in model.py
# aus dem Akku plus den Schrauben davor und dahinter.
TRAEGER_X_MIN = STEM_REAR_END_X + 35.5      # +13,5

# Das Display sitzt ueber der Schaftklemmung, also weiter hinten als die
# Wangen. Getragen wird es dort vom Teller des Traegers, der oberhalb des
# Vorbaus nach hinten durchlaeuft -- ueber dem Vorbau stoert er das Steuerrohr
# nicht (vgl. IMG_3913).
DISPLAY_CENTER_X = STEM_REAR_END_X + 40.5   # +18,5

# --- T-RGB Displaymodul (rund, 2.1") -----------------------------------------
# Aus der Baugruppe "T-RGB-FULL-3D_ASM" in FreeCAD_SimpleModel.FCStd. Das
# Deckglas ist im Hersteller-STEP zu gross; der Nutzer hat es dort auf das
# echte Mass skaliert (Faktor 0,78 -> D 74,77 mm).
#
#   Deckglas      D 74,8 x 1,0 mm [VORGABE; im STEP 1,9], steht ringsum ueber
#                 den Koerper hinaus
#   Koerper       D 56,4 mm; der FPC-Block hinten reicht bis R 28,75
#   Gesamthoehe   17,4 mm ab Glasoberseite: 16,4 mm Koerper bis zu den hoechsten
#                 Bauteilen der Platine (aus dem STEP) plus 1,0 mm Glas
#
# Das Modul haengt also am Glasrand: das Glas liegt in einer flachen Stufe
# oben im Gehaeuse, der Koerper ragt frei in die Tasche darunter.
# (Die frueheren D 59,5 stammten aus der zweiten, falschen Baugruppe im STEP.)
DISPLAY_GLASS_DIAMETER = 74.8
DISPLAY_GLASS_THICKNESS = 1.0
DISPLAY_BODY_DIAMETER = 56.4
DISPLAY_BODY_R_MAX = 28.75
DISPLAY_BODY_DEPTH = 16.4             # unter dem Glas
DISPLAY_HEIGHT = DISPLAY_GLASS_THICKNESS + DISPLAY_BODY_DEPTH
DISPLAY_FIT_CLEARANCE = 0.4     # radial; mit 0,2 presste das Glas sehr knapp [VORGABE: +0,4 im Durchmesser]

# Tasche fuer den Koerper, wie im FreeCAD-Modell: rundum Luft fuer Kabel
DISPLAY_POCKET_DIAMETER = 70.0
DISPLAY_POCKET_DEPTH = 18.0          # ab Glasoberseite; 0,6 mm Luft unter dem Modul

# Aussendurchmesser des Displaygehaeuses [VORGABE]: so klein, wie es die
# Schrauben neben der Glasstufe noch zulassen.
DISPLAY_OUTER_DIAMETER = 88.8

# Die Anzeige selbst hat nur D 55, nur das Glas ist groesser [VORGABE]. Der
# Deckel laesst unten D 58 frei -- genug zum Bedienen -- und haelt das Modul
# am breiten Glasrand nieder. Nach oben oeffnet er sich als flache Fase.
DISPLAY_ACTIVE_DIAMETER = 55.0
DECKEL_WINDOW_DIAMETER = 58.0
DECKEL_CHAMFER_LAND = 0.8     # senkrechter Absatz am Glas, statt Messerkante
DECKEL_INSERT_WALL_MIN = 1.2  # was die Fase an der Buchse mindestens stehen laesst

# --- Sensorfach (BME280 + BMI160) --------------------------------------------
# Vorne am Displaygehaeuse, auf der Platte des Traegers; im FreeCAD-Modell der
# Quader vor dem Becher. Innenmasse fuer zwei Breakouts von ca. 17 x 12 mm
# [VORGABE], nebeneinander mit der langen Seite quer, plus I2C-Verdrahtung.
SENSOR_BAY_WIDTH = 40.0       # innen, in Y: 2 x 17 + Luft
SENSOR_BAY_DEPTH = 15.0       # innen, in X, in der Mitte vor der Becherwand
SENSOR_BAY_CORNER_R = 10.0    # aussen, vordere Ecken (Anstroemung)
# Zwischen Fach und Displaytasche steht keine Wand [VORGABE] -- mehr Freiheit
# bei der Montage; die Becherwand ist dort ueber die Fachbreite offen.
SENSOR_VENT_WIDTH = 1.5       # Lueftungsschlitze seitlich, fuer den BME280
SENSOR_VENT_HEIGHT = 8.0

# --- Ladebuchse ---------------------------------------------------------------
# Wie im alten Gehaeuse: innen ein kleiner Winkelstecker im USB-C des Moduls,
# zweipolig (nur 5 V) auf eine Einbaubuchse an spritzwassergeschuetzter
# Stelle [VORGABE].
#
# Die Buchse ist D 10,5 und 15 mm tief, die Kabel gehen axial hinten heraus
# [VORGABE]. Radial im Displaygehaeuse ist dafuer kein Platz (dort sitzt der
# Modulkoerper). Sie liegt deshalb waagerecht in einer Tasche am Traeger:
# unter dem Teller, aussen an der rechten Wange, Oeffnung nach hinten zum
# Fahrer. Der Teller ist das Dach, die Wange schirmt nach vorne ab.
CHARGE_SOCKET_HOLE_D = 10.8
CHARGE_SOCKET_DEPTH = 15.0
CHARGE_CABLE_SPACE = 5.0      # hinter der Buchse, bevor die Kabel nach oben gehen
CHARGE_POD_WALL = 3.2
# Buchse und Schalter werden von aussen aufgesteckt und brauchen an der
# Stirnflaeche D 15 [VORGABE] -- das ist zugleich der Platz fuer die Finger.
POD_FACE_D = 15.0
POD_FACE_MARGIN = 0.5
POD_INTO_CHEEK = 6.0          # so weit greift die Bohrung in die Wange (spart Breite)

# Schalter: spiegelbildlich an der linken Wange, gleiche Bauart, D 12
# [VORGABE]. Er schaltet den Akku -- das Akkukabel laeuft deshalb durch seine
# Tasche und erst von dort nach oben zum Display.
SWITCH_HOLE_D = 12.3

# USB-C des Moduls (aus dem STEP): bei 251,5 Grad um die Displaymitte, rechts
# hinten. Von dort bis ueber die Buchsentasche laeuft innen eine Kammer in der
# Becherwand fuer Winkelstecker und Kabel.
MODULE_USB_ANGLE = 251.5
USB_CHAMBER_HEIGHT = 12.0     # ab Teller, bleibt unter der Glasstufe

# --- Lueftung und Ablauf [VORGABE] ---------------------------------------------
# Zuluft: je ein Kanal links und rechts von der Vorderseite des Traegers durch
# die Wange in die Displaytasche. Der Einlass liegt tief, der Kanal steigt
# gerade nach hinten an -- Wasser laeuft zum Einlass zurueck, und es gibt
# keinen engen Knick. Der Einlass ist mit einem Radius aufgeweitet.
VENT_DUCT_D = 4.0
VENT_INLET_Z = -12.0          # Hoehe des Einlasses, bezogen auf Vorbau-Oberkante
VENT_INLET_RADIUS = 1.5       # Rundung der Einlasskante (mehr laesst die Wange nicht zu)
# Abluft: Schlitze hinten in der Becherwand, im Windschatten.
VENT_OUT_WIDTH = 14.0
VENT_OUT_HEIGHT = 2.5
VENT_OUT_COUNT = 2
# Ablauf: Rinne auf der Oberseite des Tellers, die Feuchtigkeit aus Fach und
# Tasche zu zwei Loechern seitlich neben dem Vorbau fuehrt. Querschnitt rund
# (Halbkreis), Enden ausgerundet.
DRAIN_GROOVE_WIDTH = 3.0
DRAIN_HOLE_D = 3.0

# --- Sonnenschutz auf dem Deckel [VORGABE] -----------------------------------
# Wie der "Deggl" des alten Gehaeuses (Case_Grail_TRGB_Round_v2): duenne, nach
# innen geneigte Wand vorne, an den Seiten nach hinten gezogen, hinten zum
# Fahrer offen. Dort: 11 mm hoch, Wand 2,0 -> 0,8 mm, Neigung 5,3 mm auf 11 mm,
# seitlich bis ca. 35 mm hinter die Displaymitte.
HOOD_HEIGHT_MAX = 11.0      # beim alten Deggl
HOOD_HEIGHT = 15.0          # jetzt hoeher: der Oberlenker ist nicht mehr im Weg [VORGABE]
HOOD_WALL_BASE = 2.0
HOOD_WALL_TOP = 0.8
HOOD_LEAN = 5.3 / 11.0        # Einwaertsneigung je mm Hoehe
HOOD_HALF_ANGLE = 130.0       # Grad ab der Fahrtrichtung, je Seite

# --- Rundungen (Aerodynamik) [VORGABE] ----------------------------------------
FRONT_CORNER_R = 5.0          # senkrechte Vorderkanten von Traeger und Akkukasten
EDGE_FILLET = 2.5             # Oberkante Deckel
AKKU_EDGE_FILLET = 2.0        # Unterkante Akkuwanne

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
# Lieber dicke Waende und wenig Infill [VORGABE]. Die Schraubbuchsen sitzen
# deshalb in der Wand statt in angesetzten Domen -- glatte Aussenflaechen.
WALL_THICKNESS = 3.2
PLATE_THICKNESS = 5.0       # Teller des Traegers, zugleich Boden der Displaytasche
CHEEK_WALL = 4.0            # Mindestdicke der Wangen; an den Buchsen wird es mehr
FLOOR_THICKNESS = 2.0
FIT_CLEARANCE = 0.3         # Spiel um das Displaymodul
SUNSHADE_HEIGHT = 6.0       # Sonnenschutz-Kragen ueber dem Display (Mindestmass)
DECKEL_SKIN = 2.4           # Deckelstaerke ueber dem Sensorfach und ueber den Buchsen
SUNSHADE_WALL = 2.0

# --- Breite (Y) ---------------------------------------------------------------
# Wird nicht fest vorgegeben: die Breite des Klammerteils ergibt sich in
# model.py aus Vorbau-Ausschnitt + Moosgummi + Wange (bzw. Schraubdom an den
# vier Ecken), die des Displaygehaeuses aus dem runden Modul.
# Schmal in Y ist die Voraussetzung dafuer, in X nach hinten bauen zu duerfen.

# --- Akku ---------------------------------------------------------------------
# Alte Zelle als Ausgangspunkt (aus Case_Grail_TRGB_Round_v2.FCStd, "Akku":
# 54,0 x 51,5 x 7,0 mm). Liegend eingebaut, die lange Seite in Fahrtrichtung
# -- so liegt sie im FreeCAD-Modell, und so bleibt der Kasten schmal.
BATTERY_LENGTH = 54.0   # in X
BATTERY_WIDTH = 51.5    # in Y
BATTERY_HEIGHT = 7.0    # in Z
BATTERY_CLEARANCE = 0.5
BATTERY_WALL = 1.8
# Die Wanne ist nach oben offen und das unterste Teil; der Deckel steckt
# zwischen Wanne und Wangen [VORGABE] und traegt das Moosgummi zum Vorbau.
AKKU_DECKEL_THICKNESS = 2.4
AKKU_FLOOR = 3.0            # 1 mm mehr, seit der Deckel unten eben ist: Schraubenweg bleibt gleich

# --- Verschraubung: M3 mit Heatset-Inserts ------------------------------------
# Gewinde nie direkt im Druckteil, sondern immer als eingeschmolzene
# Messingbuchse [VORGABE]. Alle Schrauben kommen von unten, von oben ist
# keine zu sehen [VORGABE].
#
# Linsenkopf ISO 7380 [VORGABE]: Laenge ohne Kopf, Kopf D 5,7 x 1,65. Der
# Kopf sitzt in einer zylindrischen Senkung und steht nicht ueber.
SCREW_CLEARANCE_D = 3.8      # Durchgang M3 -- 3,4 war im Druck zu stramm [VORGABE]
SCREW_HEAD_D = 6.2           # Senkung fuer den Kopf
SCREW_HEAD_DEPTH = 2.0       # Mindesttiefe der Senkung
SCREW_SEAT_MIN = 2.0         # Material, das unter dem Kopf mindestens bleibt
UPPER_SCREW_LENGTH = 30.0    # Traeger -> Becher -> Buchse im Deckel
STACK_SCREW_LENGTH = 16.0    # Unterseite -> Buchse in der Wange
MIN_THREAD_ENGAGEMENT = 4.0

INSERT_HOLE_D = 4.2          # Bohrung fuer M3-Heatset (Buchse ca. D4,0 x 5,7)
INSERT_DEPTH = 6.0           # Einschmelztiefe
INSERT_WALL = 2.4            # Material ringsum die Buchse
INSERT_BOSS_D = INSERT_HOLE_D + 2 * INSERT_WALL   # 9,0 mm

# --- Kabeldurchfuehrung -------------------------------------------------------
# Akku -> Elektronik, seitlich am Vorbau vorbei. 6 mm, damit der Stecker
# (JST 1,25 mm, ca. 4,5 x 3,4 mm) durchpasst.
CABLE_HOLE_D = 6.0

# --- Aussparungen fuer die Displayelektronik [VORGABE] ---------------------------
# Lagen im Koordinatensystem des Moduls (Ursprung Displaymitte, +X vorne,
# +Y links), abgelesen aus T-RGB-FULL-3D_ASM im FreeCAD-Modell.
#
# Fahne der Anzeige (FPC) direkt unter dem Deckglas, nach hinten: reicht am
# echten Modul bis 0,8 mm an den Glasrand. Breite aus dem STEP (Fahne 31,6,
# an den Schultern 35,4).
DISPLAY_TAB_WIDTH = 35.4
DISPLAY_TAB_EDGE_GAP = 0.8      # Abstand Fahne -> Glasrand
DISPLAY_TAB_DEPTH = 6.0         # GESCHAETZT, nur fuer den Bezugskoerper
DISPLAY_TAB_CLEARANCE = 0.4     # Luft ringsum in der Aussparung
# Die beiden Steckbuchsen (I2C, Akku) unter der Platine sind die hoechsten
# Bauteile; mit Stecker fehlt ca. 1 mm. Aussparung auf der Telleroberseite.
I2C_SOCKET_X = (4.3, 18.6)
I2C_SOCKET_Y = (14.4, 27.0)
I2C_SOCKET_EXTRA_HEIGHT = 1.0   # so viel baut die Buchse hoeher als im STEP
I2C_RECESS_MARGIN = 1.5
I2C_RECESS_DEPTH = 2.0

# =============================================================================
# Fuenfter Entwurf: Akku oben, Display geneigt, unten nur eine Klemmplatte
# =============================================================================
# Einbauraum [VORGABE], nach Anprobe am Rad:
# - Unter dem Vorbau liegt hinten der Uebergang zum Steuerrohr (gehoert zum
#   Vorbau, dreht mit), vorne ein Lampenhalter. Dazwischen sind 50 mm frei.
# - Unter dem Vorbau duerfen 18 mm genutzt werden; tiefer steht das Steuerrohr
#   (fest am Rahmen, laeuft schraeg nach vorne unten).
# - Auch oben hoechstens 50 mm vor das Steuerrohr: weiter vorne verdecken
#   Oberlenker und Leitungen die Sicht.
# Als "Steuerrohr" gilt hier die Stelle, an der bisher die Wangen begannen
# (TRAEGER_X_MIN) -- ANNAHME, am Rad zu pruefen.
CLAMP_ZONE_LENGTH = 45.0        # Laenge des Traegers am Vorbau [VORGABE: 40 .. 45]
FREE_ZONE_LENGTH = 50.0         # so viel ist am Rad frei
FRONT_LIMIT_X = TRAEGER_X_MIN + CLAMP_ZONE_LENGTH
BOTTOM_DEPTH_MAX = 18.0

# Display vorne hoeher als hinten [VORGABE: "leicht angewinkelt"]. Der Winkel
# ist frei gewaehlt.

# Der Akku liegt im Displaygehaeuse unter dem Modul, auf der ebenen Oberseite
# des Traegers [VORGABE: Traeger und Displaygehaeuse getrennt, beide mit
# ebener Auflageflaeche druckbar].
PLATE_MIN_THICKNESS = 6.0   # Keilplatte des Traegers hinten [VORGABE]
PLATE_FRONT_THICKNESS = 10.0  # ... und vorne [VORGABE]; daraus ergibt sich die Neigung
TRAY_FLOOR = 1.4            # Boden unter dem Akku [VORGABE]; der Akku liegt waagerecht in der Keilplatte
POD_OPENING_WALL = 1.0      # Wand des Displaygehaeuses ueber den Taschen [VORGABE]
DISPLAY_UNDER_GAP = 2.0     # Luft zwischen Akku und Modul: Stecker unter der Platine

# Displaygehaeuse so klein wie moeglich [VORGABE]; links und rechts abgeflacht.
CUP_WALL_AT_GLASS = 1.6     # Wand neben der Glasstufe vorne und hinten
CUP_WALL_AT_FLAT = 0.8      # ... und an den abgeflachten Seiten

# Verschraubung oben: M2 Innensechskant, M2x8 und M2x10 [VORGABE], Heatset
# M2 x 3 mit D 3,6 aussen, Bohrung 1 mm tiefer als die Buchse.
M2_CLEARANCE_D = 2.6
M2_HEAD_D = 4.4             # Senkung fuer den Zylinderkopf (D 3,8)
M2_HEAD_HEIGHT = 2.0
M2_INSERT_HOLE_D = 3.2
M2_INSERT_DEPTH = 4.0
M2_ENGAGEMENT = 3.8
M2_WALL = 1.0               # Material um Buchse bzw. Senkung
LID_SCREW_LENGTH = 10.0
BASE_SCREW_LENGTH_FRONT = 10.0
BASE_SCREW_LENGTH_REAR = 8.0
DECKEL_SKIN_M2 = 0.8        # Deckschicht ueber den Buchsen: Deckel 4,8 mm [VORGABE]

# Sensorfach vor dem Display [VORGABE]: Platine 13 x 18 mm, 8 hoch, davor
# 10 mm fuer Stifte und Kabel. Die 10 mm liegen zum Display hin (dort ist
# ohnehin Luft vor dem Modul), das Fach selbst steht vor dem Gehaeuse.
SENSOR_SIZE_X = 13.0
SENSOR_SIZE_Y = 18.0
SENSOR_HEIGHT = 8.0
SENSOR_PIN_SPACE = 10.0     # ueber der Platine, an ihrer Vorderkante: zusammen 18 mm in Z [VORGABE]
SENSOR_CLEARANCE = 1.0
SENSOR_BAY_WALL = 2.0
SENSOR_SLOT_WIDTH = 1.5
SENSOR_SLOT_HEIGHT = 8.0

# Taschen fuer Buchse und Schalter [VORGABE]: nur die Stirnwand traegt die
# Bohrung, dahinter ein Quader, in dem sich die Mutter drehen laesst (D 16),
# nach oben ins Displaygehaeuse offen.
POD_NUT_SPACE = 16.0
POD_SIDE_WALL = 1.6

UNTERSEITE_THICKNESS = 5.0  # Klemmplatte unter dem Vorbau

# --- Cockpit als Bezugskoerper -------------------------------------------------
# Nur fuer Pruefung und Darstellung. Alles GESCHAETZT, soweit nicht vermerkt.
STEM_HEIGHT_FRONT = 31.0        # gemessen, vorne
STEM_TO_BAR_X = 88.0            # Schaftachse -> Hinterkante Lenker (Zollstock: ca. 90)
BAR_FLARE_R = 30.0              # Uebergang Vorbau -> Lenker in der Draufsicht
BAR_DEPTH = 32.0
TOPS_GAP = 33.56                # gemessen, vorne: Lenker-Oberkante -> Unterkante Oberlenker
TOPS_GAP_REAR = 57.0             # gemessen, hinten: Vorbau-Oberkante -> Unterkante Oberlenker
TOPS_HEIGHT = 16.0              # gemessen
TOPS_DEPTH = 33.0
REF_BAR_HALF_WIDTH = 70.0       # so breit werden die Lenker nur gezeichnet
STEERER_TILT_DEG = 6.0          # Schaftachse gegen die Senkrechte auf den Vorbau
# Steuerrohr und Uebergang haben dieselbe Kontur und liegen in einer Linie
# [VORGABE]: 50 mm breit mit geraden Flanken, vorne ein Halbkreis R 25. Nach
# hinten folgt der Rahmen; gezeichnet sind die Flanken 50 mm lang und gerade
# abgeschlossen (dort wird nichts angebaut).
HEADTUBE_WIDTH = 50.0
HEADTUBE_STRAIGHT = 50.0
# Mitte des vorderen Halbkreises auf Hoehe der Vorbau-Unterkante. GESCHAETZT
# ("etwas weiter hinten" als im Stand davor, dort lag der Scheitel bei X +31,6).
HEADTUBE_CENTER_X = 0.0
# Ausschnitt in der Unterseite um den Uebergang, obere Kante verrundet
UNTERSEITE_CUT_CLEARANCE = 1.0
UNTERSEITE_CUT_FILLET = 3.0

# --- Farben im STEP [VORGABE] ---------------------------------------------------
# Einmal gewaehlt, bleiben ueber alle Versionen gleich. Rahmenteile dunkelgrau,
# Elektronik rot, Druckteile kontrastreich dazu.
COLOR_FRAME = (0.25, 0.25, 0.27)
COLOR_ELECTRONICS = (0.85, 0.10, 0.10)
PART_COLORS = {
    "traeger": (0.15, 0.45, 0.90),           # blau
    "display_gehaeuse": (1.00, 0.80, 0.10),  # gelb
    "deckel": (0.20, 0.75, 0.35),            # gruen
    "unterseite": (0.10, 0.80, 0.85),        # tuerkis
}
LAMP_HOLDER_LENGTH = 30.0
LAMP_HOLDER_WIDTH = 30.0
