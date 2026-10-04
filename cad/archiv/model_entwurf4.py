"""Schlankes TRGB-Gehaeuse fuer das Canyon CP0007 Gravel Cockpit CF.

Aufbau von oben nach unten (siehe COCKPIT_ANALYSIS.md):

    deckel            -- haelt das Modul am Glasrand nieder, deckt das
                         Sensorfach ab und traegt vorne den Sonnenschutz
    display_gehaeuse  -- dickwandiger Ring um das runde 2.1"-Modul, vorne das
                         zur Tasche hin offene Sensorfach (BME280, BMI160)
    traeger           -- Teller unter dem ganzen Displaygehaeuse; vor dem
                         Steuerrohr zwei Wangen links/rechts des Vorbaus.
                         Das Vorbau-Profil laeuft voll durch. Aussen an den
                         Wangen die Taschen fuer Ladebuchse (rechts) und
                         Schalter (links), innen Lueftungskanaele.
    akku_deckel       -- Platte zwischen Wangen und Wanne
    akku_wanne        -- unterstes Teil, nach oben offen, Schraubenkoepfe
                         unten versenkt

Das Display sitzt ueber der Schaftklemmung, die Wangen erst davor. Damit es
dort nicht frei auskragt, laeuft der Teller des Traegers oberhalb des Vorbaus
unter dem ganzen Displaygehaeuse durch und liegt ueber das Moosgummi auf dem
Vorbau auf. Hinter den Wangen reicht nichts unter den Teller -- neben dem
Vorbau ist dort die Schaftklemmung bzw. das Steuerrohr im Weg.

Alle Schrauben kommen von unten und enden in Heatset-Buchsen; von oben ist
keine zu sehen. Vier lange Schrauben halten Teller, Becher und Deckel in einem
Zug zusammen.

Der Vorbau-Ausschnitt ist ein gerades Profil (dimensions.CAVITY_*), oben mit
Radius; die untere Rundung des Vorbaus bildet der Akkudeckel nach.

Ausfuehren:  cad/.venv/bin/python cad/model.py
"""

import math

from build123d import (
    Align,
    Axis,
    Box,
    CenterOf,
    Compound,
    Cone,
    Cylinder,
    Plane,
    Polygon,
    Pos,
    RectangleRounded,
    Rot,
    Sphere,
    Vector,
    export_step,
    chamfer,
    extrude,
    fillet,
    loft,
    revolve,
)

import dimensions as d

# --- Vorbau -------------------------------------------------------------------
def cavity_half_width_at(x):
    return d.CAVITY_WIDTH / 2


def max_cavity_half_width(x0, x1):
    return d.CAVITY_WIDTH / 2


# --- Displaygehaeuse: Grundmasse ----------------------------------------------
# Das Modul haengt am Glasrand: oben eine flache Stufe fuer das Deckglas,
# darunter die engere Tasche fuer den Koerper.
GLASS_RECESS_R = d.DISPLAY_GLASS_DIAMETER / 2 + d.DISPLAY_FIT_CLEARANCE
GLASS_RECESS_DEPTH = d.DISPLAY_GLASS_THICKNESS + 0.1
DISPLAY_POCKET_R = d.DISPLAY_POCKET_DIAMETER / 2
DISPLAY_WINDOW_D = d.DECKEL_WINDOW_DIAMETER

# Die vier Schrauben laufen senkrecht durch die Becherwand, so weit innen wie
# moeglich: im Becher ist es nur ein Durchgangsloch, das dicht an die Glasstufe
# ruecken darf; die Buchse sitzt erst im Deckel, der ueber dem Glas massiv ist.
# Aussen bleibt Wand um die Buchse -- keine Dome noetig, der Umriss bleibt ein
# glatter Kreis.
CUP_HOLE_WALL = 0.8      # Wand zwischen Durchgangsloch und Glasstufe
SCREW_CIRCLE_R = GLASS_RECESS_R + CUP_HOLE_WALL + d.SCREW_CLEARANCE_D / 2
DISPLAY_OUTER_R = d.DISPLAY_OUTER_DIAMETER / 2
DISPLAY_OUTER_D = 2 * DISPLAY_OUTER_R
DECKEL_INSERT_WALL_OUT = DISPLAY_OUTER_R - SCREW_CIRCLE_R - d.INSERT_HOLE_D / 2
# Kammern in der Becherwand reichen bis kurz vor die Schraubenloecher
CHAMBER_R = SCREW_CIRCLE_R - d.SCREW_CLEARANCE_D / 2 - 1.0
CUP_REAR_X = d.DISPLAY_CENTER_X - DISPLAY_OUTER_R
CUP_FRONT_X = d.DISPLAY_CENTER_X + DISPLAY_OUTER_R

# Sensorfach vorne am Becher, zur Displaytasche hin offen
BAY_INNER_X1 = CUP_FRONT_X + d.SENSOR_BAY_DEPTH
BAY_FRONT_X = BAY_INNER_X1 + d.WALL_THICKNESS
BAY_HALF_Y = d.SENSOR_BAY_WIDTH / 2 + d.WALL_THICKNESS

# --- X- und Y-Aufteilung ------------------------------------------------------
# Der Akku ist breiter als Vorbau plus Wangen, seitlich daneben ist also kein
# Platz fuer Schrauben, ohne das Gehaeuse breiter zu machen. Die Stapelschrauben
# sitzen deshalb vor und hinter dem Akku -- das bestimmt die Traegerlaenge.
SCREW_TO_POCKET = 1.2    # Wand zwischen Schraubenloch und Akkutasche
AKKU_SIZE_X = d.BATTERY_LENGTH + 2 * d.BATTERY_CLEARANCE
AKKU_SIZE_Y = d.BATTERY_WIDTH + 2 * d.BATTERY_CLEARANCE

TRAEGER_X_MIN = d.TRAEGER_X_MIN
SCREW_X_REAR = TRAEGER_X_MIN + d.INSERT_BOSS_D / 2
AKKU_POCKET_X0 = SCREW_X_REAR + d.SCREW_CLEARANCE_D / 2 + SCREW_TO_POCKET
AKKU_POCKET_X1 = AKKU_POCKET_X0 + AKKU_SIZE_X
SCREW_X_FRONT = AKKU_POCKET_X1 + SCREW_TO_POCKET + d.SCREW_CLEARANCE_D / 2
BATTERY_CENTER_X = (AKKU_POCKET_X0 + AKKU_POCKET_X1) / 2

# Die Buchsen der Stapelschrauben sitzen so nah am Ausschnitt, wie es geht.
# Die Wange ist so dick, dass sie die Buchsen ganz aufnimmt: ebene Flanken.
COLUMN_WALL_TO_CAVITY = 1.2
CORNER_R = d.INSERT_BOSS_D / 2      # senkrechte Hinterkanten


def _screw_xy():
    """Vier Stapelschrauben, senkrecht durch den Akkukasten in die Wangen."""
    positions = []
    for x in (SCREW_X_REAR, SCREW_X_FRONT):
        y = cavity_half_width_at(x) + COLUMN_WALL_TO_CAVITY + d.INSERT_HOLE_D / 2
        positions += [(x, y), (x, -y)]
    return positions


CHEEK_HALF_Y = max(
    max_cavity_half_width(TRAEGER_X_MIN, SCREW_X_FRONT) + d.CHEEK_WALL,
    max(abs(y) + d.INSERT_BOSS_D / 2 for _, y in _screw_xy()),
)


def _front_end_x():
    """Vorderkante des Traegers: so weit vor den vorderen Schrauben, dass die
    grosse Rundung der Vorderkanten die Buchsen nicht anschneidet."""
    r = d.FRONT_CORNER_R
    reach = r - d.INSERT_BOSS_D / 2             # Abstand Rundungsmitte -> Buchsenmitte, hoechstens
    dy = max(abs(y) for x, y in _screw_xy() if x == SCREW_X_FRONT) - (CHEEK_HALF_Y - r)
    if dy <= 0:
        return SCREW_X_FRONT + d.INSERT_BOSS_D / 2
    if dy > reach + 1e-6:
        raise ValueError("FRONT_CORNER_R ist zu gross fuer die Lage der vorderen Schrauben")
    return max(SCREW_X_FRONT + d.INSERT_BOSS_D / 2, SCREW_X_FRONT - math.sqrt(max(0.0, reach**2 - dy**2)) + r)


TRAEGER_X_MAX = _front_end_x()
CLAMP_LEN = TRAEGER_X_MAX - TRAEGER_X_MIN
CLAMP_MID = (TRAEGER_X_MAX + TRAEGER_X_MIN) / 2
AKKU_BOX_Y = max(AKKU_SIZE_Y + 2 * d.BATTERY_WALL, 2 * CHEEK_HALF_Y)


def traeger_width_y():
    return 2 * CHEEK_HALF_Y


# --- Z-Aufteilung -------------------------------------------------------------
# Nullpunkt: Oberkante Vorbau in der Schaftachse.
_PLATE_TOP_OF_STEM = 0.0
_CLAMP_BOTTOM = -d.STEM_HEIGHT

# Der Ausschnitt ist hoeher als der Vorbau; die Luft (Moosgummi) ist oben und
# unten gleich verteilt.
PLATE_BOTTOM_Z = (d.CAVITY_HEIGHT - d.STEM_HEIGHT) / 2
PLATE_TOP_Z = PLATE_BOTTOM_Z + d.PLATE_THICKNESS

CUP_TOP_Z = PLATE_TOP_Z + d.DISPLAY_POCKET_DEPTH       # Trennfuge zum Deckel
DISPLAY_TOP_Z = CUP_TOP_Z                              # Glas buendig mit dem Becher

# Die langen Schrauben sitzen mit dem Linsenkopf versenkt im Teller. Die
# Senkung ist so tief, dass die Schraube genug Gewinde in der Buchse des
# Deckels fasst; ihre Spitze bestimmt, wie tief die Buchse sitzt -- und damit,
# wie dick der Deckel mindestens sein muss, damit oben nichts durchscheint.
UPPER_HEAD_DEPTH = max(
    d.SCREW_HEAD_DEPTH,
    CUP_TOP_Z + d.MIN_THREAD_ENGAGEMENT - PLATE_BOTTOM_Z - d.UPPER_SCREW_LENGTH,
)
UPPER_SCREW_TIP_Z = PLATE_BOTTOM_Z + UPPER_HEAD_DEPTH + d.UPPER_SCREW_LENGTH
DECKEL_INSERT_HOLE = max(d.INSERT_DEPTH, UPPER_SCREW_TIP_Z - CUP_TOP_Z + 0.5)
DECKEL_THICKNESS = max(d.SUNSHADE_HEIGHT, DECKEL_INSERT_HOLE + d.DECKEL_SKIN)
DECKEL_TOP_Z = CUP_TOP_Z + DECKEL_THICKNESS            # Oberseite der Deckelplatte

# Sonnenschutz: so hoch wie beim alten Deckel, aber nicht ueber das
# Entwurfsziel hinaus (darueber federt die Flex-Zone des Lenkers).
HOOD_HEIGHT = min(
    d.HOOD_HEIGHT_MAX, _PLATE_TOP_OF_STEM + d.TOP_HEIGHT_TARGET - DECKEL_TOP_Z
)
HOOD_TOP_Z = DECKEL_TOP_Z + HOOD_HEIGHT

# Akkukasten: Deckel direkt unter den Wangen, darunter die Wanne
# Die Wangen enden ueber dem Akkudeckel; der Spalt ist der Klemmweg.
TRAEGER_BOTTOM_Z = PLATE_BOTTOM_Z - d.CAVITY_HEIGHT_MIN
AKKU_DECKEL_TOP_Z = PLATE_BOTTOM_Z - d.CAVITY_HEIGHT       # gezeichnete Stellung
CLAMP_GAP = TRAEGER_BOTTOM_Z - AKKU_DECKEL_TOP_Z
AKKU_DECKEL_BOTTOM_Z = AKKU_DECKEL_TOP_Z - d.AKKU_DECKEL_THICKNESS
AKKU_POCKET_H = d.BATTERY_HEIGHT + d.BATTERY_CLEARANCE
AKKU_BOTTOM_Z = AKKU_DECKEL_BOTTOM_Z - AKKU_POCKET_H - d.AKKU_FLOOR
STACK_SCREW_TIP_Z = AKKU_BOTTOM_Z + d.SCREW_HEAD_DEPTH + d.STACK_SCREW_LENGTH
# Gewinde in der Wange, wenn der Deckel auf Anschlag sitzt (Spalt 0); die
# Bohrung ueber der Buchse muss die Schraube so weit aufnehmen
STACK_ENGAGEMENT_MAX = STACK_SCREW_TIP_Z - AKKU_DECKEL_TOP_Z
STACK_BORE_DEPTH = STACK_ENGAGEMENT_MAX + 0.2

# Wie weit der Ausschnitt nach unten offen bleibt
_SKIRT_Z = AKKU_BOTTOM_Z - 20.0


def _deckel_chamfer():
    """Flachste Fase am Sichtfenster, die die Buchsen gerade noch laesst.

    Die Fase beginnt ueber einem kurzen Absatz am Fensterrand und steigt nach
    aussen. Am naechsten kommt sie der oberen Innenkante der Buchsenbohrung;
    dort bleibt genau DECKEL_INSERT_WALL_MIN stehen. Liefert den Winkel gegen
    die Waagerechte und den Radius an der Deckeloberseite."""
    r0 = DISPLAY_WINDOW_D / 2
    dr = SCREW_CIRCLE_R - d.INSERT_HOLE_D / 2 - r0
    dz = DECKEL_INSERT_HOLE - d.DECKEL_CHAMFER_LAND
    angle = math.atan2(dz, dr) + math.asin(d.DECKEL_INSERT_WALL_MIN / math.hypot(dr, dz))
    r_top = r0 + (DECKEL_THICKNESS - d.DECKEL_CHAMFER_LAND) / math.tan(angle)
    return math.degrees(angle), r_top


DECKEL_CHAMFER_DEG, DECKEL_CHAMFER_TOP_R = _deckel_chamfer()
HOOD_BASE_R = DECKEL_CHAMFER_TOP_R + 0.5       # Innenradius am Fuss, knapp ausserhalb der Fase

# --- Lange Schrauben ----------------------------------------------------------
# Sie gehen von unten durch den Teller. Ihre Lage ist durch das gedruckte
# Displaygehaeuse festgelegt. Die vorderen liegen damit halb in der Flanke
# der Wangen; dort fuehrt eine senkrechte Nut Schraube und Dreher nach oben.
UPPER_SCREW_ANGLE = math.degrees(math.asin(d.UPPER_SCREW_Y / SCREW_CIRCLE_R))
UPPER_ACCESS_D = d.SCREW_HEAD_D + 0.4


def _upper_screw_xy():
    """Vier Schrauben, je zwei vor und hinter der Displaymitte."""
    a = UPPER_SCREW_ANGLE
    return [
        (
            d.DISPLAY_CENTER_X + SCREW_CIRCLE_R * math.cos(math.radians(b)),
            SCREW_CIRCLE_R * math.sin(math.radians(b)),
        )
        for b in (a, 180 - a, a - 180, -a)
    ]


# --- Taschen fuer Ladebuchse und Schalter -------------------------------------
# Waagerechte Bohrungen unter dem Teller, aussen an den Wangen, Oeffnung nach
# hinten: rechts die Ladebuchse, links spiegelbildlich der Schalter. Dahinter
# gehen die Kabel durch den Teller nach oben in eine Kammer der Becherwand.
POD_X0 = TRAEGER_X_MIN
POD_BORE_X1 = POD_X0 + d.CHARGE_SOCKET_DEPTH + d.CHARGE_CABLE_SPACE
POD_X1 = POD_BORE_X1 + d.CHARGE_POD_WALL


class Pod:
    def __init__(self, side, hole_d):
        self.side = side                                   # +1 links, -1 rechts
        self.hole_d = hole_d
        # Die Stirnflaeche muss rund um die Bohrung den Kragen von Buchse bzw.
        # Schalter aufnehmen -- auch nach oben, unter dem Teller, und nach
        # innen, neben dem Vorbau.
        face = max(d.POD_FACE_D / 2 + d.POD_FACE_MARGIN, hole_d / 2 + d.CHARGE_POD_WALL)
        self.y = side * max(
            CHEEK_HALF_Y + hole_d / 2 - d.POD_INTO_CHEEK,
            d.CAVITY_WIDTH / 2 + d.POD_FACE_D / 2 + d.POD_FACE_MARGIN,
        )
        self.z = PLATE_BOTTOM_Z - face
        self.outer_y = self.y + side * face
        self.bottom_z = self.z - face


POD_SOCKET = Pod(-1, d.CHARGE_SOCKET_HOLE_D)
POD_SWITCH = Pod(+1, d.SWITCH_HOLE_D)
PODS = (POD_SOCKET, POD_SWITCH)
POD_LOWEST_Z = min(p.bottom_z for p in PODS)

# Steigbohrung hinter Buchse bzw. Schalter: so weit aussen, dass sie noch in
# der Kammer der Becherwand herauskommt
POD_RISER_X = d.CABLE_RISER_X
POD_RISER_Y = math.sqrt(
    (CHAMBER_R - d.CABLE_HOLE_D / 2 - 0.3) ** 2 - (POD_RISER_X - d.DISPLAY_CENTER_X) ** 2
)
POD_RISER_DEG = math.degrees(math.atan2(-POD_RISER_Y, POD_RISER_X - d.DISPLAY_CENTER_X)) % 360

# Kammer rechts vom USB-C des Moduls bis ueber die Steigbohrung; links dieselbe
# gespiegelt (dort kommt das geschaltete Akkukabel an)
CHAMBER_MARGIN_DEG = 8.0
CHAMBER_FROM_DEG = min(d.MODULE_USB_ANGLE, POD_RISER_DEG) - CHAMBER_MARGIN_DEG
CHAMBER_TO_DEG = max(d.MODULE_USB_ANGLE, POD_RISER_DEG) + CHAMBER_MARGIN_DEG

# Akkukabel: aus der Wanne senkrecht in der linken Wange nach oben (zum Vorbau
# hin offen, das Kabel laesst sich einlegen), auf Hoehe des Schalters quer in
# dessen Tasche, von dort durch die Steigbohrung zum Display.
CABLE_X = POD_RISER_X
CABLE_Y = cavity_half_width_at(CABLE_X)

# --- Lueftung und Ablauf ------------------------------------------------------
# dicht am Ausschnitt -- weiter aussen liegt die Nut der vorderen langen Schrauben
VENT_DUCT_WALL = 1.2
VENT_DUCT_Y = d.CAVITY_WIDTH / 2 + VENT_DUCT_WALL + d.VENT_DUCT_D / 2


def _vent_axis():
    """Gerader, nach hinten ansteigender Zuluftkanal: Einlass (x, z) an der
    gerundeten Vorderkante der Wange, Muendung (x, z) auf der Oberseite des
    Tellers. Die schraege Muendung ist eine Ellipse; sie liegt so, dass ihr
    hinteres Ende gerade neben dem Modulkoerper herauskommt."""
    r = d.FRONT_CORNER_R
    dy = VENT_DUCT_Y - (CHEEK_HALF_Y - r)
    x_in = TRAEGER_X_MAX - r + (math.sqrt(r * r - dy * dy) if dy > 0 else r) - 1.0
    z_in = _PLATE_TOP_OF_STEM + d.VENT_INLET_Z
    x_rear = d.DISPLAY_CENTER_X + math.sqrt((d.DISPLAY_BODY_R_MAX + 0.5) ** 2 - VENT_DUCT_Y**2)
    x_out = x_rear
    for _ in range(6):      # halbe Ellipsenlaenge haengt von der Steigung ab
        slope = math.atan2(PLATE_TOP_Z - z_in, x_in - x_out)
        x_out = x_rear + d.VENT_DUCT_D / 2 / math.sin(slope)
    return (x_in, z_in), (x_out, PLATE_TOP_Z)


VENT_IN, VENT_OUT = _vent_axis()
VENT_SLOPE_DEG = math.degrees(
    math.atan2(VENT_OUT[1] - VENT_IN[1], VENT_IN[0] - VENT_OUT[0])
)
DRAIN_X = TRAEGER_X_MIN - d.DRAIN_HOLE_D            # hinter den Wangen, dort ist unten frei
DRAIN_Y = CHEEK_HALF_Y + d.DRAIN_HOLE_D / 2 + 0.5


# --- Vorbau-Ausschnitt --------------------------------------------------------
def _arc(cy, cz, r, deg0, deg1, steps=8):
    return [
        (cy + r * math.cos(math.radians(deg0 + (deg1 - deg0) * i / steps)),
         cz + r * math.sin(math.radians(deg0 + (deg1 - deg0) * i / steps)))
        for i in range(steps + 1)
    ]


def _lid_curve(steps=16):
    """Kurve am Akkudeckel als Punkte (Abstand von der Wange, Hoehe ueber der
    Deckelflaeche). Sie beginnt senkrecht an der Wange mit R_WALL und laeuft
    waagerecht in die Deckelflaeche aus mit R_FLOOR; dazwischen waechst der
    Radius linear mit dem Winkel."""
    r0 = d.AKKU_DECKEL_CURVE_R_WALL
    k = (d.AKKU_DECKEL_CURVE_R_FLOOR - r0) / (math.pi / 2)
    out = lambda t: r0 * (1 - math.cos(t)) + k * (math.sin(t) - t * math.cos(t))
    drop = lambda t: r0 * math.sin(t) + k * (math.cos(t) + t * math.sin(t) - 1)
    height = drop(math.pi / 2)
    return [
        (out(t), height - drop(t))
        for t in (math.pi / 2 * i / steps for i in range(steps + 1))
    ]


LID_CURVE_WIDTH = _lid_curve()[-1][0]       # so weit reicht die Kurve nach innen
LID_CURVE_HEIGHT = _lid_curve()[0][1]       # und so hoch an der Wange


def _prism_x(points, x0, x1):
    """Profil in der Y-Z-Ebene, von x0 bis x1 gezogen."""
    # Umlaufsinn vereinheitlichen -- sonst zieht ein gespiegeltes Profil nach hinten
    area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(points, points[1:] + points[:1]))
    if area < 0:
        points = points[::-1]
    return extrude(Plane.YZ.offset(x0) * Polygon(*points, align=None), amount=x1 - x0)


def stem_cavity():
    """Ausschnitt fuer den Vorbau samt Moosgummi, nach unten offen. Er steht
    vorne und hinten ueber -- das Profil laeuft voll durch den Traeger durch."""
    half, top, r = d.CAVITY_WIDTH / 2, PLATE_BOTTOM_Z, d.CAVITY_TOP_RADIUS
    points = (
        [(half, _SKIRT_Z)]
        + _arc(half - r, top - r, r, 0, 90)
        + _arc(-half + r, top - r, r, 90, 180)
        + [(-half, _SKIRT_Z)]
    )
    return _prism_x(points, CUP_REAR_X - 10.0, TRAEGER_X_MAX + 10.0)


def stem_solid():
    """Der Vorbau selbst, als Bezugskoerper fuer Pruefung und Darstellung:
    gemessene Breite und Hoehe, oben der Radius des Ausschnitts, unten die
    Kurve des Akkudeckels."""
    half, top, r = d.STEM_WIDTH / 2, _PLATE_TOP_OF_STEM, d.CAVITY_TOP_RADIUS
    bottom = _CLAMP_BOTTOM
    curve = _lid_curve()
    points = (
        _arc(half - r, top - r, r, 0, 90)
        + _arc(-half + r, top - r, r, 90, 180)
        + [(-half + dy, bottom + dz) for dy, dz in curve]
        + [(half - dy, bottom + dz) for dy, dz in reversed(curve)]
    )
    return _prism_x(points, d.STEM_REAR_END_X, TRAEGER_X_MAX + 25.0)


def display_solid():
    """Huelle des Displaymoduls (Deckglas plus Koerper), als Bezugskoerper."""
    cx = d.DISPLAY_CENTER_X
    glass_t = d.DISPLAY_GLASS_THICKNESS
    body_h = d.DISPLAY_HEIGHT - glass_t
    glass = Pos(cx, 0, DISPLAY_TOP_Z - glass_t / 2) * Cylinder(
        d.DISPLAY_GLASS_DIAMETER / 2, glass_t
    )
    body = glass + Pos(cx, 0, DISPLAY_TOP_Z - glass_t - body_h / 2) * Cylinder(
        d.DISPLAY_BODY_R_MAX, body_h
    )
    # Fahne der Anzeige unter dem Glas, nach hinten
    tab_r = d.DISPLAY_GLASS_DIAMETER / 2 - d.DISPLAY_TAB_EDGE_GAP
    body += Pos(cx, 0, DISPLAY_TOP_Z - glass_t) * Box(
        tab_r, d.DISPLAY_TAB_WIDTH, d.DISPLAY_TAB_DEPTH, align=(Align.MAX, Align.CENTER, Align.MAX)
    ) & Pos(cx, 0, DISPLAY_TOP_Z - glass_t - d.DISPLAY_TAB_DEPTH / 2) * Cylinder(tab_r, d.DISPLAY_TAB_DEPTH)
    # Steckbuchsen unter der Platine, so hoch wie am echten Modul
    (x0, x1), (y0, y1) = d.I2C_SOCKET_X, d.I2C_SOCKET_Y
    body += Pos(cx + x0, y0, DISPLAY_TOP_Z - d.DISPLAY_HEIGHT - d.I2C_SOCKET_EXTRA_HEIGHT) * Box(
        x1 - x0, y1 - y0, 4.0, align=(Align.MIN, Align.MIN, Align.MIN)
    )
    return body


# --- Steuerrohr-Freihaltung ---------------------------------------------------
def headtube_keepout():
    """Zylinder um die Schaftachse unterhalb der Steuersatz-Oberkante.

    Weil das Gehaeuse um genau diese Achse dreht, gilt die Bedingung fuer jeden
    Lenkeinschlag gleichermassen -- wer hier hineinragt, kollidiert irgendwann.
    """
    height = 120.0
    return Pos(0, 0, d.HEADSET_TOP_Z - height / 2) * Cylinder(
        d.HEADTUBE_KEEPOUT_R, height
    )


# --- Teile --------------------------------------------------------------------
def _polar(angle_deg, r, z):
    a = math.radians(angle_deg)
    return (d.DISPLAY_CENTER_X + r * math.cos(a), r * math.sin(a), z)


def _radial_plane(angle_deg, r, z):
    """Ebene auf dem Radius r, lokale z-Achse radial nach aussen, lokale
    x-Achse nach oben."""
    a = math.radians(angle_deg)
    return Plane(origin=_polar(angle_deg, r, z), x_dir=(0, 0, 1), z_dir=(math.cos(a), math.sin(a), 0))


def _head_recess(x, y, z_face, depth):
    """Zylindrische Senkung fuer einen Linsenkopf, der von unten kommt und
    in der Flaeche z_face verschwindet."""
    overshoot = 0.2     # sauberer Schnitt durch die Flaeche
    return Pos(x, y, z_face - overshoot) * Cylinder(
        d.SCREW_HEAD_D / 2, depth + overshoot, align=(Align.CENTER, Align.CENTER, Align.MIN)
    )


def _bore_x(x0, x1, y, z, diameter):
    """Waagerechte Bohrung in Fahrtrichtung."""
    return Pos((x0 + x1) / 2, y, z) * Rot(0, 90, 0) * Cylinder(diameter / 2, x1 - x0)


def _bore_y(x, y0, y1, z, diameter):
    """Waagerechte Bohrung quer."""
    return Pos(x, (y0 + y1) / 2, z) * Rot(90, 0, 0) * Cylinder(diameter / 2, abs(y1 - y0))


def _bore_z(x, y, z0, z1, diameter):
    return Pos(x, y, z0) * Cylinder(
        diameter / 2, z1 - z0, align=(Align.CENTER, Align.CENTER, Align.MIN)
    )


def _rounded_block(x0, x1, half_y, z0, z1, radius):
    sketch = Pos((x0 + x1) / 2, 0, z0) * RectangleRounded(x1 - x0, 2 * half_y, radius)
    return extrude(sketch, amount=z1 - z0)


def _body_outline(z0, z1, half_y):
    """Grundriss von Traeger und Akkukasten: hinten kleine Kantenradien, vorne
    die grosse Rundung fuer die Anstroemung."""
    r = d.FRONT_CORNER_R
    front = _rounded_block(TRAEGER_X_MIN, TRAEGER_X_MAX, half_y, z0, z1, r)
    keep = Pos(TRAEGER_X_MIN + r, 0, z0) * Box(
        CLAMP_LEN, 4 * half_y, z1 - z0, align=(Align.MIN, Align.CENTER, Align.MIN)
    )
    rear = _rounded_block(TRAEGER_X_MIN, TRAEGER_X_MAX - r, half_y, z0, z1, CORNER_R)
    return rear + (front & keep)


def _cup_outline(z_from, z_to):
    """Umriss des Displaygehaeuses als Vollkoerper: Kreis und das Sensorfach
    vorne. Teller, Becher und Deckel haben alle diesen Umriss und liegen damit
    buendig aufeinander."""
    cx = d.DISPLAY_CENTER_X
    height = z_to - z_from
    body = Pos(cx, 0, z_from + height / 2) * Cylinder(DISPLAY_OUTER_R, height)
    body += _rounded_block(cx, BAY_FRONT_X, BAY_HALF_Y, z_from, z_to, d.SENSOR_BAY_CORNER_R)
    return body


def _bay_interior(z_from, z_to, open_to_pocket):
    """Innenraum des Sensorfachs. Im Becher laeuft er bis in die Displaytasche
    durch (keine Wand dazwischen); im Deckel endet er an der Becherwand, damit
    der Deckel ueber Glasrand und Buchsen massiv bleibt."""
    cx = d.DISPLAY_CENTER_X
    height = z_to - z_from
    room = _rounded_block(
        cx, BAY_INNER_X1, d.SENSOR_BAY_WIDTH / 2, z_from, z_to,
        d.SENSOR_BAY_CORNER_R - d.WALL_THICKNESS,
    )
    if open_to_pocket:
        return room
    return room - Pos(cx, 0, z_from + height / 2) * Cylinder(DISPLAY_OUTER_R, height + 1)


def _vent_duct(side):
    """Zuluftkanal einer Seite samt aufgeweitetem Einlass."""
    (x_in, z_in), (x_out, z_out) = VENT_IN, VENT_OUT
    length = math.hypot(x_out - x_in, z_out - z_in)
    axis = ((x_out - x_in) / length, 0, (z_out - z_in) / length)
    plane = Plane(origin=(x_in, side * VENT_DUCT_Y, z_in), x_dir=(0, 1, 0), z_dir=axis)
    r, rf = d.VENT_DUCT_D / 2, d.VENT_INLET_RADIUS
    outside = 10.0      # so weit reicht der Schnitt vor den Einlass hinaus
    duct = plane * Pos(0, 0, -outside) * Cylinder(
        r, length + outside + 4.0, align=(Align.CENTER, Align.CENTER, Align.MIN)
    )
    # Einlasskante mit Viertelkreis ausgerundet: Rotationskoerper um die Kanalachse
    steps = 8
    arc = [
        (r + rf - rf * math.sin(math.pi / 2 * i / steps), rf - rf * math.cos(math.pi / 2 * i / steps))
        for i in range(steps + 1)
    ]
    profile = Plane.XZ * Polygon((0, -outside), (r + rf, -outside), *arc, (0, rf), align=None)
    return duct + plane * revolve(profile, axis=Axis.Z)


def _groove_x(x0, x1, y, z, width):
    """Rinne mit rundem Querschnitt und ausgerundeten Enden, laengs."""
    return (
        _bore_x(x0, x1, y, z, width)
        + Pos(x0, y, z) * Sphere(width / 2)
        + Pos(x1, y, z) * Sphere(width / 2)
    )


def _groove_y(x, y0, y1, z, width):
    return (
        _bore_y(x, y0, y1, z, width)
        + Pos(x, y0, z) * Sphere(width / 2)
        + Pos(x, y1, z) * Sphere(width / 2)
    )


def _pod_solid(pod):
    """Vollkoerper einer Tasche, auf den Umriss des Tellers gestutzt -- sie
    bleibt damit im Schatten des Displaygehaeuses."""
    # so weit in die Wange, dass deren gerundete Hinterkante ganz ueberdeckt
    # ist: Wange und Tasche bilden hinten eine ebene Stirnflaeche ohne Spalt
    inner_y = pod.side * (CHEEK_HALF_Y - CORNER_R - 1.0)
    y0, y1 = sorted((inner_y, pod.outer_y))
    block = Pos(POD_X0, y0, pod.bottom_z) * Box(
        POD_X1 - POD_X0, y1 - y0, PLATE_BOTTOM_Z + 0.5 - pod.bottom_z,
        align=(Align.MIN, Align.MIN, Align.MIN),
    )
    return block & _cup_outline(POD_LOWEST_Z - 1.0, PLATE_TOP_Z)


def _i2c_recess():
    """Mulde auf der Telleroberseite unter den Steckbuchsen des Moduls."""
    cx, m = d.DISPLAY_CENTER_X, d.I2C_RECESS_MARGIN
    (x0, x1), (y0, y1) = d.I2C_SOCKET_X, d.I2C_SOCKET_Y
    sketch = Pos(cx + (x0 + x1) / 2, (y0 + y1) / 2, PLATE_TOP_Z - d.I2C_RECESS_DEPTH) * RectangleRounded(
        x1 - x0 + 2 * m, y1 - y0 + 2 * m, m
    )
    pocket = Pos(cx, 0, PLATE_TOP_Z) * Cylinder(DISPLAY_POCKET_R, 2 * d.I2C_RECESS_DEPTH + 1)
    return extrude(sketch, amount=d.I2C_RECESS_DEPTH + 0.5) & pocket


def _tab_recess(z0, z1):
    """Platz fuer die Fahne der Anzeige hinten unter dem Glasrand: die Tasche
    ist dort ueber die Breite der Fahne bis fast an die Glasstufe aufgeweitet."""
    cx = d.DISPLAY_CENTER_X
    r = d.DISPLAY_GLASS_DIAMETER / 2 - d.DISPLAY_TAB_EDGE_GAP + d.DISPLAY_TAB_CLEARANCE
    half = d.DISPLAY_TAB_WIDTH / 2 + d.DISPLAY_TAB_CLEARANCE
    slot = Pos(cx, 0, z0) * Box(r, 2 * half, z1 - z0, align=(Align.MAX, Align.CENTER, Align.MIN))
    return slot & Pos(cx, 0, (z0 + z1) / 2) * Cylinder(r, z1 - z0)


def build_traeger():
    """Teller unter dem Displaygehaeuse plus zwei Wangen am Vorbau.

    Gedruckt wird er auf dem Kopf (Teller auf dem Druckbett) -- so braucht
    weder der Ausschnitt noch eine der Senkungen Stuetzmaterial; die
    waagerechten Bohrungen ueberbruecken sich selbst.
    """
    # Block ueber dem Klammerbereich; der Ausschnitt macht daraus zwei Wangen
    # und die Platte darueber
    body = _body_outline(TRAEGER_BOTTOM_Z, PLATE_TOP_Z, CHEEK_HALF_Y)
    # Teller unter dem ganzen Displaygehaeuse; hinter den Wangen bleibt er
    # eine blanke Platte, die auf dem Vorbau aufliegt
    body += _cup_outline(PLATE_BOTTOM_Z, PLATE_TOP_Z)
    for pod in PODS:
        body += _pod_solid(pod)

    body -= stem_cavity()

    # Ladebuchse und Schalter: Bohrung von hinten, dahinter die Kabel durch
    # den Teller nach oben
    for pod in PODS:
        body -= _bore_x(POD_X0 - 1.0, POD_BORE_X1, pod.y, pod.z, pod.hole_d)
        body -= _bore_z(POD_RISER_X, pod.side * POD_RISER_Y, pod.z, PLATE_TOP_Z + 0.5, d.CABLE_HOLE_D)

    # Akkukabel: in der linken Wange hoch, dann quer in die Schaltertasche
    body -= _bore_z(CABLE_X, CABLE_Y, TRAEGER_BOTTOM_Z - 0.5, POD_SWITCH.z, d.CABLE_HOLE_D)
    body -= _bore_y(CABLE_X, CABLE_Y, POD_SWITCH.y, POD_SWITCH.z, d.CABLE_HOLE_D)

    # Zuluft: von der Vorderseite jeder Wange gerade ansteigend in die Displaytasche
    for side in (1, -1):
        body -= _vent_duct(side)

    # Ablauf: runde Rinne auf dem Teller laengs durch Fach und Tasche, quer
    # dazu zu zwei Loechern neben dem Vorbau
    cx = d.DISPLAY_CENTER_X
    body -= _groove_x(
        cx - DISPLAY_POCKET_R + 4.0, BAY_INNER_X1 - 3.0, 0, PLATE_TOP_Z, d.DRAIN_GROOVE_WIDTH
    )
    body -= _groove_y(DRAIN_X, -DRAIN_Y, DRAIN_Y, PLATE_TOP_Z, d.DRAIN_GROOVE_WIDTH)
    for side in (1, -1):
        body -= _bore_z(DRAIN_X, side * DRAIN_Y, PLATE_BOTTOM_Z - 0.5, PLATE_TOP_Z + 0.5, d.DRAIN_HOLE_D)

    # Heatset-Buchsen fuer die Stapelschrauben, von unten eingeschmolzen
    # Schlitze fuer die Fuehrungszungen des Akkudeckels
    for side in (1, -1):
        body -= _guide(side, TRAEGER_BOTTOM_Z - 0.5, GUIDE_HEIGHT + 1.0, d.GUIDE_GAP)

    # Buchse der I2C-/Akkustecker unter der Platine: flache Mulde im Teller
    body -= _i2c_recess()

    for x, y in _screw_xy():
        body -= _bore_z(x, y, TRAEGER_BOTTOM_Z - 0.2, TRAEGER_BOTTOM_Z + d.INSERT_DEPTH, d.INSERT_HOLE_D)
        # darueber frei fuer die Schraube, wenn der Deckel weit eingeschoben ist
        body -= _bore_z(x, y, TRAEGER_BOTTOM_Z, TRAEGER_BOTTOM_Z + STACK_BORE_DEPTH, d.SCREW_CLEARANCE_D)

    # Durchgang und Senkung fuer die langen Schrauben zum Deckel
    for x, y in _upper_screw_xy():
        body -= _bore_z(x, y, PLATE_BOTTOM_Z - 0.5, PLATE_TOP_Z + 0.5, d.SCREW_CLEARANCE_D)
        body -= _head_recess(x, y, PLATE_BOTTOM_Z, UPPER_HEAD_DEPTH)
        # freier Zugang von unten, auch dort, wo eine Tasche daneben liegt;
        # an den Wangen als Nut ueber die ganze Hoehe
        z0 = TRAEGER_BOTTOM_Z - 0.5 if x > TRAEGER_X_MIN else POD_LOWEST_Z - 1.0
        body -= _bore_z(x, y, z0, PLATE_BOTTOM_Z, UPPER_ACCESS_D)

    return body


def _vent_xs():
    """Zwei Schlitze je Seitenwand, nahe der Vorderwand des Fachs."""
    return [BAY_INNER_X1 - 9.0, BAY_INNER_X1 - 13.0]


def _chamber(from_deg, to_deg):
    """Kammer in der Becherwand als Kreissektor um die Displayachse
    (align=None haelt die Achse im Ursprung, sonst wuerde die Huelle des
    Sektors zentriert)."""
    return (
        Pos(d.DISPLAY_CENTER_X, 0, PLATE_TOP_Z - 0.5)
        * Rot(0, 0, from_deg)
        * Cylinder(CHAMBER_R, d.USB_CHAMBER_HEIGHT + 0.5, arc_size=to_deg - from_deg, align=None)
    )


def _vent_out_zs():
    step = (d.USB_CHAMBER_HEIGHT - d.VENT_OUT_HEIGHT) / max(d.VENT_OUT_COUNT - 1, 1)
    return [PLATE_TOP_Z + 4.0 + i * step * 0.6 for i in range(d.VENT_OUT_COUNT)]


def build_display_gehaeuse():
    """Dickwandiger Ring um das 2.1"-Modul mit Sensorfach. Hat keinen eigenen
    Boden -- der Teller des Traegers schliesst Tasche und Fach nach unten ab.

    Gedruckt wird er aufrecht: die Tasche weitet sich nach oben zur Glasstufe,
    es gibt also keinen nennenswerten Ueberhang.
    """
    height = CUP_TOP_Z - PLATE_TOP_Z
    mid_z = PLATE_TOP_Z + height / 2
    cx = d.DISPLAY_CENTER_X
    body = _cup_outline(PLATE_TOP_Z, CUP_TOP_Z)

    # Tasche fuer den Modulkoerper und Stufe fuer das Deckglas
    body -= Pos(cx, 0, mid_z) * Cylinder(DISPLAY_POCKET_R, height + 1)
    body -= Pos(cx, 0, CUP_TOP_Z - GLASS_RECESS_DEPTH / 2) * Cylinder(
        GLASS_RECESS_R, GLASS_RECESS_DEPTH
    )

    # Fahne der Anzeige hinten
    body -= _tab_recess(PLATE_TOP_Z - 0.5, CUP_TOP_Z + 0.5)

    # Sensorfach, zur Displaytasche hin offen; Lueftungsschlitze seitlich
    body -= _bay_interior(PLATE_TOP_Z - 0.5, CUP_TOP_Z + 0.5, open_to_pocket=True)
    for x in _vent_xs():
        body -= Pos(x, 0, PLATE_TOP_Z + 3.0) * Box(
            d.SENSOR_VENT_WIDTH,
            2 * BAY_HALF_Y + 1,
            d.SENSOR_VENT_HEIGHT,
            align=(Align.CENTER, Align.CENTER, Align.MIN),
        )

    # Kammern fuer die Kabel von Ladebuchse (rechts) und Schalter (links)
    body -= _chamber(CHAMBER_FROM_DEG, CHAMBER_TO_DEG)
    body -= _chamber(360 - CHAMBER_TO_DEG, 360 - CHAMBER_FROM_DEG)

    # Abluft hinten, im Windschatten
    wall = DISPLAY_OUTER_R - DISPLAY_POCKET_R
    for z in _vent_out_zs():
        body -= _radial_plane(180, DISPLAY_POCKET_R + wall / 2, z) * Box(
            d.VENT_OUT_HEIGHT, d.VENT_OUT_WIDTH, wall + 2
        )

    # Durchgang fuer die langen Schrauben
    for x, y in _upper_screw_xy():
        body -= Pos(x, y, mid_z) * Cylinder(d.SCREW_CLEARANCE_D / 2, height + 1)
    return body


def _hood_wall(h):
    """Innenradius und Wandstaerke des Sonnenschutzes in der Hoehe h."""
    t = d.HOOD_WALL_BASE + (d.HOOD_WALL_TOP - d.HOOD_WALL_BASE) * h / HOOD_HEIGHT
    return HOOD_BASE_R - d.HOOD_LEAN * h, t


def _sunshade():
    """Duenne, nach innen geneigte Wand um die Vorderseite des Sichtfensters,
    an den Seiten nach hinten gezogen und hinten zum Fahrer offen."""
    sink = 0.5      # greift in die Deckelplatte
    r0, t0 = _hood_wall(0)
    r1, t1 = _hood_wall(HOOD_HEIGHT)
    profile = Plane.XZ * Polygon(
        (r0, -sink), (r0 + t0, -sink), (r1 + t1, HOOD_HEIGHT), (r1, HOOD_HEIGHT), align=None
    )
    wall = revolve(profile, axis=Axis.Z, revolution_arc=2 * d.HOOD_HALF_ANGLE)
    return Pos(d.DISPLAY_CENTER_X, 0, DECKEL_TOP_Z) * Rot(0, 0, -d.HOOD_HALF_ANGLE) * wall


def build_deckel():
    """Deckel mit Sonnenschutz. Haelt das Modul am Glasrand nieder und nimmt
    unten die Buchsen fuer die langen Schrauben auf -- oben ist er geschlossen.

    Gedruckt wird er mit der Unterseite auf dem Bett: Fase und Sonnenschutz
    stehen dann ohne Stuetzmaterial, die Decke ueber dem Sensorfach ist eine
    kurze Bruecke."""
    height = DECKEL_TOP_Z - CUP_TOP_Z
    mid_z = CUP_TOP_Z + height / 2
    body = _cup_outline(CUP_TOP_Z, DECKEL_TOP_Z)
    body = fillet(body.edges().group_by(Axis.Z)[-1], d.EDGE_FILLET)

    body -= Pos(d.DISPLAY_CENTER_X, 0, mid_z) * Cylinder(DISPLAY_WINDOW_D / 2, height + 2)
    # flache Fase nach innen, so flach wie die Buchsen es zulassen
    body -= Pos(d.DISPLAY_CENTER_X, 0, CUP_TOP_Z + d.DECKEL_CHAMFER_LAND) * Cone(
        DISPLAY_WINDOW_D / 2,
        DECKEL_CHAMFER_TOP_R,
        height - d.DECKEL_CHAMFER_LAND,
        align=(Align.CENTER, Align.CENTER, Align.MIN),
    )
    body += _sunshade()
    # ueber dem Sensorfach ausgehoehlt: mehr Hoehe fuer Stecker und Kabel
    body -= _bay_interior(CUP_TOP_Z - 0.5, DECKEL_TOP_Z - d.DECKEL_SKIN, open_to_pocket=False)
    for x, y in _upper_screw_xy():
        body -= _bore_z(x, y, CUP_TOP_Z - 0.2, CUP_TOP_Z + DECKEL_INSERT_HOLE, d.INSERT_HOLE_D)
    return body


# Fuehrungszungen: mittig in der Wange, zwischen der Nut der vorderen langen
# Schraube und der vorderen Stapelschraube -- dort ist die Wange unten massiv.
GUIDE_HEIGHT = d.CLAMP_TRAVEL + d.GUIDE_ENGAGE_MIN
GUIDE_Y = (d.CAVITY_WIDTH / 2 + CHEEK_HALF_Y) / 2
GUIDE_X = (
    max(x for x, _ in _upper_screw_xy()) + UPPER_ACCESS_D / 2
    + SCREW_X_FRONT - d.INSERT_BOSS_D / 2
) / 2


def _guide(side, z0, height, gap):
    """Zunge (gap=0) bzw. ihr Schlitz in der Wange (gap>0)."""
    return Pos(GUIDE_X, side * GUIDE_Y, z0) * Box(
        d.GUIDE_LENGTH + 2 * gap, d.GUIDE_THICKNESS + 2 * gap, height,
        align=(Align.CENTER, Align.CENTER, Align.MIN),
    )


def _upper_access(z0, z1):
    """Nut fuer die vorderen langen Schrauben in der Flanke des Akkukastens --
    so bleiben sie auch bei montiertem Akku erreichbar."""
    cut = None
    for x, y in _upper_screw_xy():
        if x > TRAEGER_X_MIN:
            bore = _bore_z(x, y, z0, z1, UPPER_ACCESS_D)
            cut = bore if cut is None else cut + bore
    return cut


def build_akku_deckel():
    """Platte zwischen Wangen und Wanne. Oben drueckt sie ueber das Moosgummi
    gegen den Vorbau; die Unterseite ist eben und liegt beim Drucken auf dem
    Bett. In der Lage halten sie die vier Stapelschrauben."""
    body = _body_outline(AKKU_DECKEL_BOTTOM_Z, AKKU_DECKEL_TOP_Z, AKKU_BOX_Y / 2)
    # Kurven an beiden Wangen: sie bilden die untere Rundung des Vorbaus nach
    wall_y = d.CAVITY_WIDTH / 2 - d.AKKU_DECKEL_CURVE_GAP
    top_z = AKKU_DECKEL_TOP_Z + LID_CURVE_HEIGHT
    outline = _body_outline(AKKU_DECKEL_BOTTOM_Z, top_z, AKKU_BOX_Y / 2)
    for side in (1, -1):
        points = [(side * (wall_y - dy), AKKU_DECKEL_TOP_Z + dz) for dy, dz in _lid_curve()]
        points += [
            (side * (wall_y - LID_CURVE_WIDTH), AKKU_DECKEL_TOP_Z - 0.2),
            (side * wall_y, AKKU_DECKEL_TOP_Z - 0.2),
        ]
        body += _prism_x(points, TRAEGER_X_MIN, TRAEGER_X_MAX) & outline
    # Fuehrungszungen, oben angefast zum Einfaedeln
    for side in (1, -1):
        tongue = _guide(side, AKKU_DECKEL_TOP_Z - 0.2, GUIDE_HEIGHT + 0.2, 0.0)
        body += chamfer(tongue.edges().group_by(Axis.Z)[-1], 0.8)
    lip_z = AKKU_DECKEL_BOTTOM_Z      # Unterseite eben: druckt ohne Stuetzen
    for x, y in _screw_xy():
        body -= _bore_z(x, y, lip_z - 0.5, AKKU_DECKEL_TOP_Z + 0.5, d.SCREW_CLEARANCE_D)
    # Akkukabel nach oben in den Kanal der linken Wange
    body -= _bore_z(CABLE_X, CABLE_Y, lip_z - 0.5, top_z + 0.5, d.CABLE_HOLE_D)
    return body - _upper_access(AKKU_DECKEL_BOTTOM_Z - 0.5, AKKU_DECKEL_TOP_Z + 0.5)


def build_akku_wanne():
    """Unterstes Teil: Wanne fuer den Akku, nach oben offen. Die Schrauben
    gehen von unten durch ihre massiven Enden; die Koepfe sind versenkt."""
    body = _body_outline(AKKU_BOTTOM_Z, AKKU_DECKEL_BOTTOM_Z, AKKU_BOX_Y / 2)
    body = fillet(body.edges().group_by(Axis.Z)[0], d.AKKU_EDGE_FILLET)
    body -= Pos(BATTERY_CENTER_X, 0, AKKU_BOTTOM_Z + d.AKKU_FLOOR) * Box(
        AKKU_SIZE_X, AKKU_SIZE_Y, AKKU_POCKET_H + 0.5,
        align=(Align.CENTER, Align.CENTER, Align.MIN),
    )
    for x, y in _screw_xy():
        body -= _bore_z(x, y, AKKU_BOTTOM_Z - 0.5, AKKU_DECKEL_BOTTOM_Z + 0.5, d.SCREW_CLEARANCE_D)
        body -= _head_recess(x, y, AKKU_BOTTOM_Z, d.SCREW_HEAD_DEPTH)
    return body - _upper_access(AKKU_BOTTOM_Z - 0.5, AKKU_DECKEL_BOTTOM_Z + 0.5)


# --- Pruefungen ---------------------------------------------------------------
def _volume(shape):
    return 0.0 if shape is None else sum(s.volume for s in shape.solids())


def _overlap(a, b):
    """Gemeinsames Volumen zweier Koerper in mm3."""
    return _volume(a.intersect(b))


def display_support(parts):
    """Wie gut das Displaygehaeuse getragen wird.

    Liefert den Anteil seiner Grundflaeche, unter dem der Teller liegt, und den
    Abstand des Schwerpunkts (Becher + Deckel) vom naechsten Rand der
    Schraubenbasis in X -- positiv heisst: der Schwerpunkt liegt zwischen den
    Schrauben, es haengt also nichts einseitig an ihnen.
    """
    slab = _cup_outline(PLATE_BOTTOM_Z, PLATE_BOTTOM_Z + 0.5)
    carried = _overlap(parts["traeger"], slab) / slab.volume

    upper = [parts["display_gehaeuse"], parts["deckel"]]
    mass = sum(p.volume for p in upper)
    com_x = sum(p.center(CenterOf.MASS).X * p.volume for p in upper) / mass
    xs = [x for x, _ in _upper_screw_xy()]
    inside = min(com_x - min(xs), max(xs) - com_x)
    return carried, com_x, inside


def selfcheck(parts):
    stem_mid_z = (_PLATE_TOP_OF_STEM + _CLAMP_BOTTOM) / 2
    plate_mid_z = (PLATE_BOTTOM_Z + PLATE_TOP_Z) / 2 - 0.5      # unter der Ablaufrinne
    cup_mid_z = (PLATE_TOP_Z + CUP_TOP_Z) / 2
    cx = d.DISPLAY_CENTER_X
    cheek_x = CLAMP_MID + 8.0
    cheek_y = cavity_half_width_at(cheek_x) + 1.0
    ledge_r = (DISPLAY_POCKET_R + GLASS_RECESS_R) / 2
    chamber_mid_deg = (CHAMBER_FROM_DEG + CHAMBER_TO_DEG) / 2
    chamber_r = (DISPLAY_POCKET_R + CHAMBER_R) / 2
    bay_x = (CUP_FRONT_X + BAY_INNER_X1) / 2
    hood_h = HOOD_HEIGHT / 2
    hood_r, hood_t = _hood_wall(hood_h)
    hood_point = lambda deg: _polar(deg, hood_r + hood_t / 2, DECKEL_TOP_Z + hood_h)
    pocket_z = AKKU_BOTTOM_Z + d.AKKU_FLOOR + 2.0
    vent_mid = ((VENT_IN[0] + VENT_OUT[0]) / 2, (VENT_IN[1] + VENT_OUT[1]) / 2)

    cases = [
        ("Vorbau-Ausschnitt ist frei", (CLAMP_MID, 0, stem_mid_z), False, "traeger"),
        # dicht am Ausschnitt, zwischen den Schrauben: dort laeuft kein Kanal
        ("Wange links ist Material", (cheek_x, cheek_y, stem_mid_z), True, "traeger"),
        ("Wange rechts ist Material", (cheek_x, -cheek_y, stem_mid_z), True, "traeger"),
        ("Platte ueber dem Vorbau", (TRAEGER_X_MAX - 15.0, 8.0, plate_mid_z), True, "traeger"),
        ("Profil laeuft hinten durch", (TRAEGER_X_MIN + 1, 0, stem_mid_z), False, "traeger"),
        ("Profil laeuft vorne durch", (TRAEGER_X_MAX - 1, 0, stem_mid_z), False, "traeger"),
        ("Teller unter der Displaymitte", (cx, 0, plate_mid_z), True, "traeger"),
        ("Teller reicht bis unter die hintere Becherwand",
         (CUP_REAR_X + 1.0, 0, plate_mid_z), True, "traeger"),
        ("Vorbau ist unter dem Teller frei", (CUP_REAR_X + 5.0, 0, -5.0), False, "traeger"),
        ("Teller unter dem Sensorfach", (bay_x, 8.0, plate_mid_z), True, "traeger"),
        ("Ablaufrinne auf dem Teller", (cx, 0, PLATE_TOP_Z - 0.5), False, "traeger"),
        ("Ablaufrinne im Sensorfach", (bay_x - 2.0, 0, PLATE_TOP_Z - 0.5), False, "traeger"),
        ("Ablaufloch links", (DRAIN_X, DRAIN_Y, plate_mid_z), False, "traeger"),
        ("Ablaufloch rechts", (DRAIN_X, -DRAIN_Y, plate_mid_z), False, "traeger"),
        ("Zuluftkanal links", (vent_mid[0], VENT_DUCT_Y, vent_mid[1]), False, "traeger"),
        ("Zuluftkanal rechts", (vent_mid[0], -VENT_DUCT_Y, vent_mid[1]), False, "traeger"),
        ("Zuluft muendet in der Tasche (links)",
         (VENT_OUT[0], VENT_DUCT_Y, VENT_OUT[1] - 0.3), False, "traeger"),
        ("Zuluft muendet in der Tasche (rechts)",
         (VENT_OUT[0], -VENT_DUCT_Y, VENT_OUT[1] - 0.3), False, "traeger"),
        ("Wange unter dem Zuluftkanal",
         (vent_mid[0], VENT_DUCT_Y, vent_mid[1] - d.VENT_DUCT_D), True, "traeger"),
        ("Akkukabel: Kanal in der linken Wange",
         (CABLE_X, CABLE_Y + 1.0, (TRAEGER_BOTTOM_Z + POD_SWITCH.z) / 2), False, "traeger"),
        ("Akkukabel: quer in die Schaltertasche",
         (CABLE_X, (CABLE_Y + POD_SWITCH.y) / 2, POD_SWITCH.z), False, "traeger"),
        ("Akkukabel: Loch im Akkudeckel",
         (CABLE_X, CABLE_Y, (AKKU_DECKEL_TOP_Z + AKKU_DECKEL_BOTTOM_Z) / 2), False, "akku_deckel"),
        ("Displaytasche ist frei", (cx, 0, cup_mid_z), False, "display_gehaeuse"),
        # Proben bei -90 Grad und oberhalb der Kammer: dort ist die Wand voll
        ("Becherwand ist Material",
         (cx, -(DISPLAY_OUTER_R - d.WALL_THICKNESS / 2), cup_mid_z), True, "display_gehaeuse"),
        ("Kammer rechts ist frei",
         _polar(chamber_mid_deg, chamber_r, PLATE_TOP_Z + 3.0), False, "display_gehaeuse"),
        ("Kammer links ist frei",
         _polar(360 - chamber_mid_deg, chamber_r, PLATE_TOP_Z + 3.0), False, "display_gehaeuse"),
        ("Wand ueber der Kammer",
         _polar(chamber_mid_deg, chamber_r, PLATE_TOP_Z + d.USB_CHAMBER_HEIGHT + 2.0),
         True, "display_gehaeuse"),
        ("Abluft hinten",
         _polar(180, DISPLAY_OUTER_R - 2.0, _vent_out_zs()[0]), False, "display_gehaeuse"),
        ("Stufe traegt den Glasrand",
         (cx, -ledge_r, CUP_TOP_Z - GLASS_RECESS_DEPTH - 1.0), True, "display_gehaeuse"),
        ("Glasstufe ist frei",
         (cx, -ledge_r, CUP_TOP_Z - GLASS_RECESS_DEPTH / 2), False, "display_gehaeuse"),
        ("Sensorfach ist frei", (bay_x, 0, cup_mid_z), False, "display_gehaeuse"),
        ("Sensorfach hat eine Vorderwand",
         (BAY_FRONT_X - d.WALL_THICKNESS / 2, 0, cup_mid_z), True, "display_gehaeuse"),
        ("Keine Wand zwischen Fach und Tasche (unten)",
         (CUP_FRONT_X - 2.0, 0, PLATE_TOP_Z + 2.0), False, "display_gehaeuse"),
        ("Keine Wand zwischen Fach und Tasche (oben)",
         (CUP_FRONT_X - 2.0, 0, CUP_TOP_Z - 4.0), False, "display_gehaeuse"),
        ("Deckel hat Sichtfenster", (cx, 0, DECKEL_TOP_Z - 1.0), False, "deckel"),
        ("Fase am Sichtfenster",
         (cx, -(DISPLAY_WINDOW_D / 2 + DECKEL_CHAMFER_TOP_R) / 2, DECKEL_TOP_Z - 0.5), False, "deckel"),
        ("Absatz am Fensterrand",
         (cx, -(DISPLAY_WINDOW_D / 2 + 1.0), CUP_TOP_Z + d.DECKEL_CHAMFER_LAND / 2), True, "deckel"),
        ("Deckel haelt den Glasrand",
         (cx, -(d.DISPLAY_GLASS_DIAMETER / 2 - 1.0), CUP_TOP_Z + 1.0), True, "deckel"),
        ("Deckel schliesst das Sensorfach",
         (bay_x, 0, DECKEL_TOP_Z - d.DECKEL_SKIN / 2), True, "deckel"),
        ("Sonnenschutz vorne", hood_point(0), True, "deckel"),
        ("Sonnenschutz an der Seite", hood_point(d.HOOD_HALF_ANGLE - 10), True, "deckel"),
        ("Sonnenschutz ist hinten offen", hood_point(180), False, "deckel"),
        ("Akkutasche ist frei", (BATTERY_CENTER_X, 0, pocket_z), False, "akku_wanne"),
        ("Akkuwanne hat einen Boden",
         (BATTERY_CENTER_X, 0, AKKU_BOTTOM_Z + d.AKKU_FLOOR / 2), True, "akku_wanne"),
        ("Akkudeckel ist geschlossen",
         (BATTERY_CENTER_X, 0, (AKKU_DECKEL_TOP_Z + AKKU_DECKEL_BOTTOM_Z) / 2), True, "akku_deckel"),
        ("Akkudeckel ist unten eben",
         (BATTERY_CENTER_X, 0, AKKU_DECKEL_BOTTOM_Z - 0.3), False, "akku_deckel"),
        ("Kurve am Akkudeckel links",
         (CLAMP_MID, d.CAVITY_WIDTH / 2 - 1.0, AKKU_DECKEL_TOP_Z + 1.0), True, "akku_deckel"),
        ("Kurve am Akkudeckel rechts",
         (CLAMP_MID, -d.CAVITY_WIDTH / 2 + 1.0, AKKU_DECKEL_TOP_Z + 1.0), True, "akku_deckel"),
    ]
    for pod, name in ((POD_SOCKET, "Ladebuchse"), (POD_SWITCH, "Schalter")):
        x_mid = POD_X0 + d.CHARGE_SOCKET_DEPTH / 2
        cases += [
            (f"Bohrung {name}", (x_mid, pod.y, pod.z), False, "traeger"),
            (f"Wand unter {name}", (x_mid, pod.y, pod.bottom_z + d.CHARGE_POD_WALL / 2), True, "traeger"),
            (f"Tasche {name} ist vorne geschlossen",
             (POD_X1 - d.CHARGE_POD_WALL / 2, pod.side * (CHEEK_HALF_Y + 0.5), pod.z), True, "traeger"),
            (f"Kabel {name} geht durch den Teller",
             (POD_RISER_X, pod.side * POD_RISER_Y, plate_mid_z), False, "traeger"),
            (f"Kammer liegt ueber dem Kabel {name}",
             (POD_RISER_X, pod.side * POD_RISER_Y, PLATE_TOP_Z + 3.0), False, "display_gehaeuse"),
        ]
    i2c_x = cx + sum(d.I2C_SOCKET_X) / 2
    i2c_y = sum(d.I2C_SOCKET_Y) / 2
    tab_r = d.DISPLAY_GLASS_DIAMETER / 2 - d.DISPLAY_TAB_EDGE_GAP
    cases += [
        ("Mulde fuer die I2C-Buchse im Teller",
         (i2c_x, i2c_y, PLATE_TOP_Z - d.I2C_RECESS_DEPTH + 0.3), False, "traeger"),
        ("Teller bleibt unter der Mulde geschlossen",
         (i2c_x, i2c_y, PLATE_TOP_Z - d.I2C_RECESS_DEPTH - 0.5), True, "traeger"),
        ("Aussparung fuer die Displayfahne hinten",
         (cx - tab_r, 0, CUP_TOP_Z - GLASS_RECESS_DEPTH - 1.0), False, "display_gehaeuse"),
        ("Becherwand hinter der Displayfahne",
         (cx - GLASS_RECESS_R - 2.0, 0, CUP_TOP_Z - 0.5), True, "display_gehaeuse"),
    ]
    for side in (1, -1):
        cases += [
            ("Fuehrungszunge am Akkudeckel",
             (GUIDE_X, side * GUIDE_Y, AKKU_DECKEL_TOP_Z + GUIDE_HEIGHT - 1.5), True, "akku_deckel"),
            ("Schlitz der Fuehrung in der Wange",
             (GUIDE_X, side * GUIDE_Y, TRAEGER_BOTTOM_Z + GUIDE_HEIGHT), False, "traeger"),
            ("Wange aussen neben dem Schlitz",
             (GUIDE_X, side * (CHEEK_HALF_Y - 1.0), TRAEGER_BOTTOM_Z + 3.0), True, "traeger"),
            ("Wange innen neben dem Schlitz",
             (GUIDE_X, side * (d.CAVITY_WIDTH / 2 + 1.0), TRAEGER_BOTTOM_Z + 3.0), True, "traeger"),
        ]
    failures = []

    def inside(part_name, point):
        return parts[part_name].solids()[0].is_inside(Vector(*point))

    for name, point, expected, part_name in cases:
        if inside(part_name, point) != expected:
            failures.append(f"{name} (Punkt {tuple(round(c, 1) for c in point)}, Teil {part_name})")

    for name, part in parts.items():
        if len(part.solids()) != 1:
            failures.append(f"{name} besteht aus {len(part.solids())} Koerpern statt einem")

    riser_r = math.hypot(POD_RISER_X - cx, POD_RISER_Y)
    if (riser_r - d.CABLE_HOLE_D / 2 < d.DISPLAY_BODY_R_MAX
            or riser_r + d.CABLE_HOLE_D / 2 > CHAMBER_R):
        failures.append("Kabel von Buchse/Schalter muenden nicht zwischen Modulkoerper und Kammerwand")
    vent_r = math.hypot(VENT_OUT[0] - cx, VENT_DUCT_Y)
    if not d.DISPLAY_BODY_R_MAX < vent_r < DISPLAY_POCKET_R:
        failures.append("Zuluft muendet nicht im Ringspalt neben dem Modulkoerper")
    if VENT_SLOPE_DEG < 5.0:
        failures.append("Zuluftkanal steigt nicht nach hinten an")

    # Buchse und Schalter: an der Stirnflaeche muss der Kragen D POD_FACE_D
    # satt aufliegen (ebene Flaeche ohne Spalt) und darf nicht am Teller anstossen
    for pod, name in ((POD_SOCKET, "Ladebuchse"), (POD_SWITCH, "Schalter")):
        ring = d.POD_FACE_D / 2 - 0.2
        for a in range(0, 360, 20):
            y = pod.y + ring * math.cos(math.radians(a))
            z = pod.z + ring * math.sin(math.radians(a))
            if not inside("traeger", (POD_X0 + 0.3, y, z)):
                failures.append(f"{name}: Stirnflaeche traegt den Kragen nicht ringsum (bei {a} Grad)")
                break
            if inside("traeger", (POD_X0 - 1.0, y, z)):
                failures.append(f"{name}: vor der Stirnflaeche ist der Kragen nicht frei (bei {a} Grad)")
                break
    if BAY_FRONT_X > TRAEGER_X_MAX:
        failures.append("Sensorfach ragt vorne ueber die Traegerplatte hinaus")
    if HOOD_HEIGHT < 4.0:
        failures.append(f"Fuer den Sonnenschutz bleiben nur {HOOD_HEIGHT:.1f} mm Hoehe")
    if HOOD_BASE_R + d.HOOD_WALL_BASE > DISPLAY_OUTER_R - d.EDGE_FILLET + 0.01:
        failures.append("Sonnenschutz steht in der Kantenrundung des Deckels")

    # Hinter den Wangen darf nichts unter den Teller reichen (Schaftklemmung)
    behind = Pos(TRAEGER_X_MIN - 0.2, 0, PLATE_BOTTOM_Z - 0.2) * Box(
        100, 200, 100, align=(Align.MAX, Align.CENTER, Align.MAX)
    )
    below = _overlap(parts["traeger"], behind)
    if below > 1.0:
        failures.append(f"Traeger reicht hinter den Wangen unter den Teller ({below:.0f} mm3)")

    # Verschraubung: jede Schraube muss durch alle Teile frei durchgehen und
    # in einer Buchse enden, die ringsum im Material sitzt
    wall_probe = d.INSERT_HOLE_D / 2 + d.INSERT_WALL / 2

    def check_screw(label, x, y, free, insert, outward):
        for part_name, z in free + [insert]:
            if inside(part_name, (x, y, z)):
                failures.append(f"{label} ({x:.0f},{y:.0f}) blockiert in {part_name}")
        part_name, z = insert
        ox, oy = outward
        if not inside(part_name, (x + ox * wall_probe, y + oy * wall_probe, z)):
            failures.append(f"{label} ({x:.0f},{y:.0f}): Buchse in {part_name} hat keine Wand")

    for x, y in _screw_xy():
        check_screw(
            "Stapelschraube", x, y,
            free=[("akku_wanne", (AKKU_DECKEL_BOTTOM_Z + AKKU_BOTTOM_Z) / 2),
                  ("akku_deckel", (AKKU_DECKEL_TOP_Z + AKKU_DECKEL_BOTTOM_Z) / 2),
                  ("traeger", TRAEGER_BOTTOM_Z + STACK_BORE_DEPTH - 0.3)],
            insert=("traeger", TRAEGER_BOTTOM_Z + d.INSERT_DEPTH / 2),
            outward=(0.0, math.copysign(1.0, y)),
        )
        # die Buchse muss auch zur gerundeten Vorderkante hin Wand haben
        if not inside("traeger", (x + math.copysign(wall_probe, x - CLAMP_MID), y,
                                  TRAEGER_BOTTOM_Z + d.INSERT_DEPTH / 2)):
            failures.append(f"Stapelschraube ({x:.0f},{y:.0f}): keine Wand zur Stirnseite")
        if AKKU_POCKET_X0 - SCREW_TO_POCKET < x < AKKU_POCKET_X1 + SCREW_TO_POCKET:
            failures.append(f"Stapelschraube ({x:.0f},{y:.0f}) laeuft durch die Akkutasche")
    for x, y in _upper_screw_xy():
        r = math.hypot(x - cx, y)
        check_screw(
            "Lange Schraube", x, y,
            free=[("traeger", plate_mid_z), ("display_gehaeuse", cup_mid_z)],
            insert=("deckel", CUP_TOP_Z + d.INSERT_DEPTH / 2),
            outward=((x - cx) / r, y / r),
        )
        # der ganze Kopfquerschnitt muss von unten frei sein, bis unter die Taschen
        head_r = d.SCREW_HEAD_D / 2
        blocked = any(
            inside("traeger", (x + head_r * math.cos(math.radians(a)),
                               y + head_r * math.sin(math.radians(a)), z))
            for a in range(0, 360, 30)
            for z in (PLATE_BOTTOM_Z - 1.0, POD_SWITCH.z, POD_LOWEST_Z + 0.5)
        )
        if blocked or inside("traeger", (x, y, PLATE_BOTTOM_Z - 3.0)):
            failures.append(f"Lange Schraube ({x:.0f},{y:.0f}) ist von unten nicht zugaenglich")
        if not inside("deckel", (x, y, DECKEL_TOP_Z - 0.5)):
            failures.append(f"Lange Schraube ({x:.0f},{y:.0f}) ist von oben sichtbar")

    if DECKEL_INSERT_WALL_OUT < 2.0:
        failures.append(f"Buchsen im Deckel haben aussen nur {DECKEL_INSERT_WALL_OUT:.1f} mm Wand")
    if DECKEL_CHAMFER_TOP_R > DISPLAY_OUTER_R - d.WALL_THICKNESS:
        failures.append("Fase laesst oben am Deckel keinen Rand stehen")
    if DISPLAY_WINDOW_D < d.DISPLAY_ACTIVE_DIAMETER:
        failures.append("Sichtfenster ist kleiner als die Anzeige")

    # Unter jedem Kopf muss genug Material bleiben
    seat = d.PLATE_THICKNESS - UPPER_HEAD_DEPTH
    if seat < d.SCREW_SEAT_MIN - 1e-9:
        failures.append(f"Lange Schraube: nur {seat:.1f} mm Material unter dem Kopf")

    # Schraubenlaengen: genug Gewinde in der Buchse, Spitze im Bohrloch
    for label, tip, insert_from, hole in (
        ("Stapelschraube", STACK_SCREW_TIP_Z, TRAEGER_BOTTOM_Z, STACK_BORE_DEPTH + CLAMP_GAP),
        ("Stapelschraube bei groesstem Spalt",
         STACK_SCREW_TIP_Z - (d.CLAMP_TRAVEL - CLAMP_GAP), TRAEGER_BOTTOM_Z, STACK_BORE_DEPTH + d.CLAMP_TRAVEL),
        ("Lange Schraube", UPPER_SCREW_TIP_Z, CUP_TOP_Z, DECKEL_INSERT_HOLE),
    ):
        engagement = tip - insert_from
        if engagement < d.MIN_THREAD_ENGAGEMENT - 1e-9:
            failures.append(f"{label}: nur {engagement:.1f} mm Gewinde in der Buchse")
        if tip > insert_from + hole - 0.3:
            failures.append(f"{label}: Spitze steht {tip - insert_from - hole:.1f} mm ueber das Bohrloch")

    # Kein Teil darf in ein anderes, in den Vorbau oder ins Displaymodul ragen
    solids = dict(parts, vorbau=stem_solid(), display=display_solid())
    names = list(solids)
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            vol = _overlap(solids[a], solids[b])
            if vol > 1.0:
                failures.append(f"{a} und {b} durchdringen sich ({vol:.0f} mm3)")

    # Der Akkudeckel muss sich bis auf Anschlag einschieben lassen
    pushed = Pos(0, 0, CLAMP_GAP) * parts["akku_deckel"]
    vol = _overlap(pushed, parts["traeger"])
    if vol > 1.0:
        failures.append(f"Akkudeckel stoesst beim Einschieben an den Traeger ({vol:.0f} mm3)")

    # Ueber den Stapelschrauben muss die Wange geschlossen bleiben (Taschen!)
    for x, y in _screw_xy():
        if not inside("traeger", (x, y, TRAEGER_BOTTOM_Z + STACK_BORE_DEPTH + 0.5)):
            failures.append(f"Bohrung der Stapelschraube ({x:.0f},{y:.0f}) bricht oben durch")
        for dy in (-1, 1):
            if not inside("traeger", (x, y + dy * d.SCREW_CLEARANCE_D / 2, TRAEGER_BOTTOM_Z + STACK_BORE_DEPTH + 0.5)):
                failures.append(f"Bohrung der Stapelschraube ({x:.0f},{y:.0f}) bricht seitlich oben durch")

    # Auflage des Displays -- der Grund fuer den Teller
    carried, com_x, com_inside = display_support(parts)
    if carried < 0.9:
        failures.append(f"Displaygehaeuse liegt nur zu {carried:.0%} auf dem Teller auf")
    if com_inside <= 0:
        failures.append("Schwerpunkt des Displaygehaeuses liegt ausserhalb der Schraubenbasis")

    # Steuerrohr: nichts darf in den Schwenkbereich ragen
    keepout = headtube_keepout()
    keepout_hits = [
        (name, vol) for name, part in parts.items() if (vol := _overlap(part, keepout)) > 1.0
    ]

    screws = len(_screw_xy()) + len(_upper_screw_xy())
    if failures:
        print("!! Selbsttest fehlgeschlagen:")
        for f in failures:
            print(f"   - {f}")
    else:
        print(f"Selbsttest: {len(cases)} Stichpunkte, {screws} Verschraubungen, "
              f"Durchdringung und Auflage in Ordnung")

    if keepout_hits:
        print()
        print("Hinweis Steuerrohr-Freihaltung (Annahme: Oberkante Steuersatz "
              f"{d.HEADSET_TOP_Z:+.0f} mm, Radius {d.HEADTUBE_KEEPOUT_R:.0f} mm):")
        for name, vol in keepout_hits:
            print(f"   - {name} liegt mit {vol:.0f} mm3 innerhalb")
        print("   Beide Annahmen sind geschaetzt. Zu messen: Durchmesser des")
        print("   Steuersatz-Deckels und sein Abstand unter der Vorbau-Unterkante.")
        print("   Stimmen sie, muesste der Akku weiter nach vorne.")
    return not failures


def report(parts):
    print("Stapel in Z (Nullpunkt = Oberkante Vorbau an der Schaftachse):")
    print(f"  Sonnenschutz oben        {HOOD_TOP_Z:+7.1f} mm  ({HOOD_HEIGHT:.1f} hoch)")
    print(f"  Deckelplatte oben        {DECKEL_TOP_Z:+7.1f} mm  (Deckel {DECKEL_THICKNESS:.1f} dick)")
    print(f"  Displayglas oben         {DISPLAY_TOP_Z:+7.1f} mm")
    print(f"  Modul-Unterkante         {DISPLAY_TOP_Z - d.DISPLAY_HEIGHT:+7.1f} mm")
    print(f"  Teller                   {PLATE_BOTTOM_Z:+7.1f} .. {PLATE_TOP_Z:+.1f} mm")
    print(f"  Vorbau Oberkante         {_PLATE_TOP_OF_STEM:+7.1f} mm")
    print(f"  Vorbau Unterkante        {_CLAMP_BOTTOM:+7.1f} mm")
    print(f"  Steuersatz Oberkante     {d.HEADSET_TOP_Z:+7.1f} mm  (Freihaltung R{d.HEADTUBE_KEEPOUT_R:.0f})")
    print(f"  Wangen unten             {TRAEGER_BOTTOM_Z:+7.1f} mm  (Spalt zum Akkudeckel {CLAMP_GAP:.1f}, "
          f"Ausschnitt {d.CAVITY_HEIGHT_MIN:.0f} .. {d.CAVITY_HEIGHT_MIN + d.CLAMP_TRAVEL:.0f} hoch)")
    print(f"  Akkudeckel / Wanne       {AKKU_DECKEL_BOTTOM_Z:+7.1f} / {AKKU_BOTTOM_Z:+.1f} mm")
    print()
    top = HOOD_TOP_Z - _PLATE_TOP_OF_STEM
    print(f"Oberteil ueber Vorbau      {top:7.1f} mm  (Ziel {d.TOP_HEIGHT_TARGET:.0f}, Vorgabe max {d.TOP_HEIGHT_MAX:.0f})")
    print(f"Akku unter Vorbau          {_CLAMP_BOTTOM - AKKU_BOTTOM_Z:7.1f} mm  (Vorgabe max {d.BOTTOM_HEIGHT_MAX:.0f})")
    print(f"Wangen bei X               {TRAEGER_X_MIN:+.1f} .. {TRAEGER_X_MAX:+.1f} mm  ({CLAMP_LEN:.1f} lang,"
          f" Vorderkanten R{d.FRONT_CORNER_R:.0f})")
    print(f"Teller bei X               {CUP_REAR_X:+.1f} .. {CUP_FRONT_X:+.1f} mm, Sensorfach bis {BAY_FRONT_X:+.1f}")
    cheek = CHEEK_HALF_Y - max_cavity_half_width(TRAEGER_X_MIN, TRAEGER_X_MAX)
    print(f"Breite Wangen / Akkukasten {traeger_width_y():7.1f} / {AKKU_BOX_Y:.1f} mm"
          f"  (Wange mind. {cheek:.1f} dick, Akku {d.BATTERY_WIDTH} quer)")
    print(f"Displaygehaeuse D          {DISPLAY_OUTER_D:7.1f} mm  (Deckglas D{d.DISPLAY_GLASS_DIAMETER}),"
          f" Sichtfenster D{DISPLAY_WINDOW_D:.0f}")
    print(f"Fase am Deckel             {DECKEL_CHAMFER_DEG:7.1f} Grad gegen die Waagerechte,"
          f" oben D{2 * DECKEL_CHAMFER_TOP_R:.1f}")
    r_top, t_top = _hood_wall(HOOD_HEIGHT)
    print(f"Sonnenschutz               innen D{2 * HOOD_BASE_R:.1f} am Fuss -> D{2 * r_top:.1f} oben,"
          f" Wand {d.HOOD_WALL_BASE:.1f} -> {t_top:.1f}, +-{d.HOOD_HALF_ANGLE:.0f} Grad um die Front")
    print(f"Sensorfach innen           {d.SENSOR_BAY_WIDTH:.0f} x {d.SENSOR_BAY_DEPTH:.0f} mm,"
          f" {CUP_TOP_Z - PLATE_TOP_Z + DECKEL_THICKNESS - d.DECKEL_SKIN:.0f} hoch, zur Displaytasche offen")
    for pod, name in ((POD_SOCKET, "Ladebuchse rechts"), (POD_SWITCH, "Schalter links   ")):
        print(f"{name}          D{pod.hole_d} x {d.CHARGE_SOCKET_DEPTH:.0f} tief"
              f" + {d.CHARGE_CABLE_SPACE:.0f} Kabelraum, Oeffnung nach hinten bei"
              f" X {POD_X0:+.1f}, Y {pod.y:+.1f}, Z {pod.z:+.1f}")
    print(f"Zuluft                     2x D{d.VENT_DUCT_D:.0f}, Einlass vorn bei Y +-{VENT_DUCT_Y:.1f}, Z {VENT_IN[1]:+.1f}"
          f" (Kante R{d.VENT_INLET_RADIUS}), steigt {VENT_SLOPE_DEG:.0f} Grad bis X {VENT_OUT[0]:+.1f} in der Tasche")
    print(f"Abluft                     {d.VENT_OUT_COUNT}x {d.VENT_OUT_WIDTH:.0f} x {d.VENT_OUT_HEIGHT} mm hinten im Becher")
    print(f"Ablauf                     runde Rinne R{d.DRAIN_GROOVE_WIDTH / 2} auf dem Teller,"
          f" 2x D{d.DRAIN_HOLE_D:.0f} bei X {DRAIN_X:+.1f}, Y +-{DRAIN_Y:.1f}")
    print()
    print("Schrauben (alle M3 Linsenkopf ISO 7380 von unten, versenkt, in Heatset-Buchsen):")
    print(f"  4x M3x{d.UPPER_SCREW_LENGTH:.0f}  Teller -> Becher -> Deckel"
          f"   ({UPPER_SCREW_TIP_Z - CUP_TOP_Z:.1f} mm in der Buchse,"
          f" Senkung {UPPER_HEAD_DEPTH:.1f}, darunter {d.PLATE_THICKNESS - UPPER_HEAD_DEPTH:.1f} mm)")
    print(f"  4x M3x{d.STACK_SCREW_LENGTH:.0f}  Wanne -> Akkudeckel -> Wange"
          f"  ({STACK_ENGAGEMENT_MAX - d.CLAMP_TRAVEL:.1f} .. {STACK_ENGAGEMENT_MAX:.1f} mm in der Wange"
          f" je nach Spalt, Senkung {d.SCREW_HEAD_DEPTH:.1f})")
    print()
    carried, com_x, com_inside = display_support(parts)
    xs = sorted({round(x, 1) for x, _ in _upper_screw_xy()})
    print("Auflage des Displays:")
    print(f"  Teller unter der Grundflaeche   {carried:6.0%}")
    print(f"  Schwerpunkt Becher + Deckel     X {com_x:+.1f} mm")
    print(f"  Schrauben bei                   X {xs[0]:+.1f} und {xs[-1]:+.1f} mm"
          f"  -> Schwerpunkt {com_inside:.1f} mm innerhalb")
    print(f"  Teller hinter den Wangen        {TRAEGER_X_MIN - CUP_REAR_X:6.1f} mm"
          f"  (liegt dort auf dem Vorbau auf, nichts darunter)")
    print()
    if _CLAMP_BOTTOM - AKKU_BOTTOM_Z > d.BOTTOM_HEIGHT_MAX:
        print(f"!! Akkukasten ueberschreitet die Vorgabe von {d.BOTTOM_HEIGHT_MAX:.0f} mm unter dem Vorbau")
    if top > d.TOP_HEIGHT_MAX:
        print(f"!! Oberkante {top:.1f} mm ueberschreitet die Vorgabe von {d.TOP_HEIGHT_MAX:.0f} mm")
    for name, part in parts.items():
        bb = part.bounding_box()
        print(
            f"  {name:18s} vol={part.volume / 1000:6.1f}cm3  "
            f"X{bb.min.X:+7.1f}..{bb.max.X:+6.1f}  "
            f"Y{bb.size.Y:6.1f}  Z{bb.min.Z:+7.1f}..{bb.max.Z:+6.1f}"
        )
    print()


def build_all():
    parts = {
        "traeger": build_traeger(),
        "display_gehaeuse": build_display_gehaeuse(),
        "deckel": build_deckel(),
        "akku_deckel": build_akku_deckel(),
        "akku_wanne": build_akku_wanne(),
    }
    for name, part in parts.items():
        part.label = name
    return parts


if __name__ == "__main__":
    parts = build_all()
    report(parts)
    print("-" * 74)
    ok = selfcheck(parts)
    print("-" * 74)

    assembly = Compound(children=list(parts.values()))
    assembly.label = "TRGB_Gehaeuse_CP0007"
    out = "export/TRGB_Gehaeuse.step"
    export_step(assembly, out)
    print(f"Ein STEP mit {len(parts)} benannten Teilen geschrieben: {out}")
    raise SystemExit(0 if ok else 1)
