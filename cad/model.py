"""Parametrisches Gehaeuse fuer den TRGB-BikeComputer am Canyon CP0007.

Sechster Entwurf. Vier Druckteile, von oben nach unten:

  deckel            Deckel mit Fase und Sonnenschutz, haelt das Modul am Glasrand
  display_gehaeuse  Ring um Modul und Akku, vorne das Sensorfach; unten offen
  traeger           Keilplatte mit ebener Oberseite, darunter zwei Wangen am Vorbau
                    und die Taschen fuer Ladebuchse und Schalter
  unterseite        Klemmplatte unter dem Vorbau, steckt mit Kurven und Zungen in den Wangen

Der Einbauraum ist klein: unter dem Vorbau sind zwischen dem Uebergang zum
Steuerrohr und dem Lampenhalter 50 mm frei und nur 18 mm tief; auch oben soll
nichts weiter als diese 50 mm nach vorne reichen. Der Akku liegt deshalb oben,
im Displaygehaeuse unter dem Modul.

Traeger und Displaygehaeuse sind getrennte Teile mit ebener Fuge. Die Fuge ist
gegen den Vorbau geneigt (Display vorne hoeher). Alles oberhalb der Fuge ist
im Koordinatensystem des Displays gebaut (Ursprung Mitte der Fuge, z =
Displayachse, +x vorne) und wird mit LOC an seinen Platz gesetzt.

Alle Schrauben kommen von unten und enden in Heatset-Buchsen; von oben ist
keine zu sehen.

Ausfuehren:  cad/.venv/bin/python cad/model.py
"""

import math

from build123d import (
    Align,
    Axis,
    Box,
    Compound,
    Cone,
    Cylinder,
    Plane,
    Polygon,
    Pos,
    Rectangle,
    RectangleRounded,
    Rot,
    Vector,
    chamfer,
    export_step,
    extrude,
    fillet,
    loft,
    revolve,
)

import dimensions as d

MIN_Z = (Align.CENTER, Align.CENTER, Align.MIN)
MAX_Z = (Align.CENTER, Align.CENTER, Align.MAX)
MIN_ALL = (Align.MIN, Align.MIN, Align.MIN)

# --- Vorbau und Wangen --------------------------------------------------------
CAVITY_HALF = d.CAVITY_WIDTH / 2
STEM_TOP_Z = 0.0
STEM_BOTTOM_Z = -d.STEM_HEIGHT

# Der Ausschnitt ist hoeher als der Vorbau; die Luft (Moosgummi) ist oben und
# unten gleich verteilt.
PLATE_BOTTOM_Z = (d.CAVITY_HEIGHT - d.STEM_HEIGHT) / 2
CHEEK_TOP_Z = PLATE_BOTTOM_Z + 3.0                         # Wangenblock, wo er vor der Keilplatte liegt
# Die Wangen enden ueber der Unterseite; der Spalt ist der Klemmweg.
TRAEGER_BOTTOM_Z = PLATE_BOTTOM_Z - d.CAVITY_HEIGHT_MIN
UNTERSEITE_TOP_Z = PLATE_BOTTOM_Z - d.CAVITY_HEIGHT        # gezeichnete Stellung
UNTERSEITE_BOTTOM_Z = UNTERSEITE_TOP_Z - d.UNTERSEITE_THICKNESS
CLAMP_GAP = TRAEGER_BOTTOM_Z - UNTERSEITE_TOP_Z
_SKIRT_Z = UNTERSEITE_BOTTOM_Z - 20.0

TRAEGER_X_MIN = d.TRAEGER_X_MIN
TRAEGER_X_MAX = d.FRONT_LIMIT_X
FREE_END_X = TRAEGER_X_MIN + d.FREE_ZONE_LENGTH              # dort beginnen Lampenhalter und Lenkeruebergang
CLAMP_MID = (TRAEGER_X_MIN + TRAEGER_X_MAX) / 2

# Die Buchsen der Stapelschrauben sitzen so nah am Ausschnitt, wie es geht.
COLUMN_WALL_TO_CAVITY = 1.2
CORNER_R = d.INSERT_BOSS_D / 2      # senkrechte Hinterkanten
STACK_Y = CAVITY_HALF + COLUMN_WALL_TO_CAVITY + d.INSERT_HOLE_D / 2
CHEEK_HALF_Y = max(CAVITY_HALF + d.CHEEK_WALL, STACK_Y + d.INSERT_BOSS_D / 2)
STACK_X_REAR = TRAEGER_X_MIN + d.INSERT_BOSS_D / 2
# vorne so weit zurueck, dass die Rundung der Vorderkante die Buchse nicht anschneidet
STACK_X_FRONT = TRAEGER_X_MAX - max(d.FRONT_CORNER_R, d.INSERT_BOSS_D / 2)


def _screw_xy():
    """Vier Stapelschrauben (M3), senkrecht durch die Unterseite in die Wangen."""
    return [(x, s * STACK_Y) for x in (STACK_X_REAR, STACK_X_FRONT) for s in (1, -1)]


STACK_HEAD_DEPTH = d.UNTERSEITE_THICKNESS - d.SCREW_SEAT_MIN
# Gewinde in der Wange bei Spalt 0; die Bohrung ueber der Buchse nimmt die Schraube auf
STACK_ENGAGEMENT_MAX = d.STACK_SCREW_LENGTH - d.SCREW_SEAT_MIN
STACK_BORE_DEPTH = STACK_ENGAGEMENT_MAX + 0.2

# --- Displaygehaeuse (lokale Koordinaten) -------------------------------------
GLASS_RECESS_R = d.DISPLAY_GLASS_DIAMETER / 2 + d.DISPLAY_FIT_CLEARANCE
GLASS_RECESS_DEPTH = d.DISPLAY_GLASS_THICKNESS + 0.1
# Umriss: Rechteck mit grossen Eckradien. Vorne/hinten und an den Seiten ist
# die Wand neben dem Glas so duenn wie moeglich; in den vier Ecken ist Platz
# fuer die Schrauben, ohne dass aussen etwas zu sehen ist.
R_OUT = GLASS_RECESS_R + d.CUP_WALL_AT_GLASS               # halbe Laenge (vorne und hinten)
FLAT_Y = GLASS_RECESS_R + d.CUP_WALL_AT_FLAT               # halbe Breite
DISPLAY_POCKET_R = d.DISPLAY_POCKET_DIAMETER / 2
DISPLAY_WINDOW_D = d.DECKEL_WINDOW_DIAMETER

# Sensorfach: die Platine liegt dicht vor dem Modul, zum Teil in der Wand des
# Gehaeuses; nur der Rest steht vorne ueber. Stifte und Kabel stehen an der
# Vorderkante nach oben, die Kabel laufen in einer Mulde des Traegers zurueck.
BAY_IN_Y = d.SENSOR_SIZE_Y / 2 + d.SENSOR_CLEARANCE
BAY_SENSOR_LX0 = d.DISPLAY_BODY_R_MAX + 0.75
BAY_IN_LX1 = BAY_SENSOR_LX0 + d.SENSOR_SIZE_X + d.SENSOR_CLEARANCE
BAY_FRONT_LX = BAY_IN_LX1 + d.SENSOR_BAY_WALL
# volle Hoehe ueberall: Platine plus Stifte und Kabel darueber, 18 mm in Z
BAY_IN_H = d.SENSOR_HEIGHT + d.SENSOR_PIN_SPACE + d.SENSOR_CLEARANCE
BAY_TOP_LZ = BAY_IN_H + d.SENSOR_BAY_WALL
BAY_OUT_Y = BAY_IN_Y + d.SENSOR_BAY_WALL
BAY_SLOT_LX = (R_OUT + 1.8, R_OUT + 4.3)


# --- Lage des Displays --------------------------------------------------------
# Neigung: die Keilplatte ist hinten PLATE_MIN_THICKNESS und vorne (an der
# Vorderkante des Sensorfachs) PLATE_FRONT_THICKNESS dick
ALPHA = math.asin((d.PLATE_FRONT_THICKNESS - d.PLATE_MIN_THICKNESS) / (R_OUT + BAY_FRONT_LX))
TILT_DEG = math.degrees(ALPHA)


def _solve_frame():
    """Ursprung des geneigten Systems: die Keilplatte ist hinten gerade
    PLATE_MIN_THICKNESS dick, und vorne endet der Traeger bei FRONT_LIMIT_X."""
    sin, cos = math.sin(ALPHA), math.cos(ALPHA)
    oz = PLATE_BOTTOM_Z + d.PLATE_MIN_THICKNESS + R_OUT * sin
    t = (PLATE_BOTTOM_Z - oz - BAY_FRONT_LX * sin) / cos     # vorderste Kante, unten
    ox = d.FRONT_LIMIT_X - BAY_FRONT_LX * cos + t * sin
    return ox, oz


ORIGIN_X, ORIGIN_Z = _solve_frame()
LOC = Pos(ORIGIN_X, 0, ORIGIN_Z) * Rot(0, -TILT_DEG, 0)


def to_world(lx, ly, lz):
    sin, cos = math.sin(ALPHA), math.cos(ALPHA)
    return (ORIGIN_X + lx * cos - lz * sin, ly, ORIGIN_Z + lx * sin + lz * cos)


def to_local(x, y, z):
    sin, cos = math.sin(ALPHA), math.cos(ALPHA)
    dx, dz = x - ORIGIN_X, z - ORIGIN_Z
    return (dx * cos + dz * sin, y, -dx * sin + dz * cos)


def plate_thickness(lx):
    """Dicke der Keilplatte, senkrecht zur Fuge gemessen."""
    return (to_world(lx, 0, 0)[2] - PLATE_BOTTOM_Z) / math.cos(ALPHA)


# Akku: waagerecht in der Keilplatte, TRAY_FLOOR ueber deren Unterseite,
# mittig unter dem Modul. Sein oberer Teil steht (hinten am meisten) ins
# Gehaeuse; die Aussparung dort reicht bis AKKU_NOTCH_LZ.
AKKU_SIZE_X = d.BATTERY_LENGTH + 2 * d.BATTERY_CLEARANCE
AKKU_SIZE_Y = d.BATTERY_WIDTH + 2 * d.BATTERY_CLEARANCE
AKKU_CX = to_world(0, 0, 0)[0]
AKKU_FLOOR_Z = PLATE_BOTTOM_Z + d.TRAY_FLOOR
AKKU_SPACE_TOP_Z = AKKU_FLOOR_Z + d.BATTERY_HEIGHT + d.BATTERY_CLEARANCE
AKKU_TOP_LZ = to_local(AKKU_CX - AKKU_SIZE_X / 2, 0, AKKU_SPACE_TOP_Z)[2]
AKKU_NOTCH_LZ = AKKU_TOP_LZ + 0.3
# Hoehe des Gehaeuses: ueber dem Akku Luft fuer die Stecker, dann das Modul bis zum Glas
CUP_H = AKKU_TOP_LZ + d.DISPLAY_UNDER_GAP + d.DISPLAY_HEIGHT
LEDGE_LZ = CUP_H - GLASS_RECESS_DEPTH                      # Auflage des Glases

# Schrauben: alle in den vier Ecken des Umrisses, von aussen unsichtbar. Je
# Ecke eine Deckelschraube (von unten durch das Gehaeuse) und eine Buchse fuer
# die Schraube aus dem Traeger.
SCREW_R = GLASS_RECESS_R + 0.8 + d.M2_HEAD_D / 2
EAR_R = d.M2_HEAD_D / 2 + d.M2_WALL                        # Platzbedarf einer Schraube samt Wand
LID_EAR_DEG = 57.0
BASE_EAR_DEG = 33.0
DECKEL_THICKNESS = d.M2_INSERT_DEPTH + d.DECKEL_SKIN_M2
DECKEL_TOP_LZ = CUP_H + DECKEL_THICKNESS
LID_SEAT_LZ = CUP_H + d.M2_ENGAGEMENT - d.LID_SCREW_LENGTH  # Kopfauflage im Gehaeuse
HOOD_HEIGHT = d.HOOD_HEIGHT


def _polar(deg, r):
    return (r * math.cos(math.radians(deg)), r * math.sin(math.radians(deg)))


def _four(deg):
    return [_polar(a, SCREW_R) for a in (deg, 180 - deg, deg - 180, -deg)]


def _lid_ears():
    return _four(LID_EAR_DEG)


def _outline_radius(deg, corner_r):
    """Abstand des Umrisses von der Mitte in Richtung deg (erster Quadrant)."""
    c, s_ = math.cos(math.radians(deg)), math.sin(math.radians(deg))
    cx, cy = R_OUT - corner_r, FLAT_Y - corner_r
    t = min(R_OUT / c if c > 1e-9 else 1e9, FLAT_Y / s_ if s_ > 1e-9 else 1e9)
    if t * c > cx and t * s_ > cy:        # Strahl trifft die Eckrundung
        b = c * cx + s_ * cy
        t = b + math.sqrt(b * b - (cx * cx + cy * cy - corner_r * corner_r))
    return t


def _corner_radius():
    """Groesster Eckradius, bei dem die Schrauben noch ringsum Wand haben."""
    r = FLAT_Y
    while r > 1.0:
        if all(_outline_radius(a, r) >= SCREW_R + EAR_R for a in (LID_EAR_DEG, BASE_EAR_DEG)):
            return r
        r -= 0.25
    raise ValueError("Schrauben passen nicht in die Ecken des Umrisses")


CORNER_OUT_R = _corner_radius()


def _base_screws():
    """Schrauben Traeger -> Displaygehaeuse: (lx, ly, Laenge)."""
    return [
        (lx, ly, d.BASE_SCREW_LENGTH_FRONT if lx > 0 else d.BASE_SCREW_LENGTH_REAR)
        for lx, ly in _four(BASE_EAR_DEG)
    ]


# --- Deckel: Fase und Sonnenschutz --------------------------------------------
def _deckel_chamfer():
    """Flachste Fase am Sichtfenster: sie muss an den Buchsen Wand lassen und
    oben Platz fuer den Fuss des Sonnenschutzes. Liefert den Winkel gegen die
    Deckelflaeche und den Radius an der Deckeloberseite."""
    r0 = DISPLAY_WINDOW_D / 2
    rise = DECKEL_THICKNESS - d.DECKEL_CHAMFER_LAND
    dr = SCREW_R - d.M2_INSERT_HOLE_D / 2 - r0
    dz = d.M2_INSERT_DEPTH - d.DECKEL_CHAMFER_LAND
    by_insert = math.atan2(dz, dr) + math.asin(d.DECKEL_INSERT_WALL_MIN / math.hypot(dr, dz))
    r_top_max = FLAT_Y - d.HOOD_WALL_BASE - 0.5 - 0.3
    by_hood = math.atan2(rise, r_top_max - r0)
    angle = max(by_insert, by_hood)
    return math.degrees(angle), r0 + rise / math.tan(angle)


DECKEL_CHAMFER_DEG, DECKEL_CHAMFER_TOP_R = _deckel_chamfer()
HOOD_BASE_R = DECKEL_CHAMFER_TOP_R + 0.5       # Innenradius am Fuss, knapp ausserhalb der Fase


# --- Taschen fuer Ladebuchse und Schalter -------------------------------------
# Kaesten unter der Keilplatte aussen an den Wangen. Die Stirnwand hinten
# traegt die Bohrung; dahinter ist der Kasten frei (die Mutter laesst sich
# drehen) und nach oben ins Displaygehaeuse offen.
POD_X0 = TRAEGER_X_MIN
POD_IN_X0 = POD_X0 + d.CHARGE_POD_WALL
POD_IN_X1 = POD_X0 + d.CHARGE_SOCKET_DEPTH + d.CHARGE_CABLE_SPACE
POD_X1 = POD_IN_X1 + d.CHARGE_POD_WALL
POD_Y = CAVITY_HALF + d.POD_SIDE_WALL + d.POD_NUT_SPACE / 2      # Achse der Bohrung
POD_Z = PLATE_BOTTOM_Z - d.POD_NUT_SPACE / 2                    # Kragen bleibt unter der Platte frei
POD_OUT_Y = POD_Y + d.POD_NUT_SPACE / 2 + d.POD_SIDE_WALL
POD_BOTTOM_Z = POD_Z - d.POD_NUT_SPACE / 2 - d.POD_SIDE_WALL
PODS = ((-1, d.CHARGE_SOCKET_HOLE_D, "Ladebuchse"), (1, d.SWITCH_HOLE_D, "Schalter"))

# Ueber den Taschen ist die Wand des Displaygehaeuses innen bis auf
# POD_OPENING_WALL ausgenommen -- so weit sind die Taschen nach oben offen.
RELIEF_LX0 = to_local(POD_IN_X0, 0, PLATE_BOTTOM_Z + d.PLATE_MIN_THICKNESS)[0]
RELIEF_LX1 = min(
    to_local(POD_IN_X1, 0, PLATE_BOTTOM_Z + d.PLATE_MIN_THICKNESS)[0],
    _polar(LID_EAR_DEG, SCREW_R)[0] - EAR_R - 0.5,          # davor steht die Deckelschraube
)
RELIEF_Y0 = POD_Y - d.POD_NUT_SPACE / 2
RELIEF_Y1 = FLAT_Y - d.POD_OPENING_WALL
RELIEF_LZ = 8.0


def _relief(side, lz0, lz1):
    return _box(RELIEF_LX0, RELIEF_LX1, side * RELIEF_Y0, side * RELIEF_Y1, lz0, lz1)


# --- Lueftung -----------------------------------------------------------------
VENT_DUCT_WALL = 1.2
VENT_DUCT_Y = CAVITY_HALF + VENT_DUCT_WALL + d.VENT_DUCT_D / 2
VENT_OUT_LZ = (AKKU_TOP_LZ + 4.0, AKKU_TOP_LZ + 9.5)


def _vent_axis():
    """Gerader, nach hinten ansteigender Zuluftkanal: Einlass (x, z) an der
    gerundeten Vorderkante der Wange, Muendung (x, z) in der Vorderwand der Tasche."""
    r = d.FRONT_CORNER_R
    dy = VENT_DUCT_Y - (CHEEK_HALF_Y - r)
    x_in = TRAEGER_X_MAX - r + (math.sqrt(r * r - dy * dy) if dy > 0 else r) - 1.0
    return (x_in, STEM_TOP_Z + d.VENT_INLET_Z), (POD_IN_X1, POD_Z + 2.0)


VENT_IN, VENT_OUT = _vent_axis()
VENT_SLOPE_DEG = math.degrees(math.atan2(VENT_OUT[1] - VENT_IN[1], VENT_IN[0] - VENT_OUT[0]))

# --- Fuehrung der Unterseite --------------------------------------------------
GUIDE_HEIGHT = d.CLAMP_TRAVEL + d.GUIDE_ENGAGE_MIN
GUIDE_Y = (CAVITY_HALF + CHEEK_HALF_Y) / 2
# zwischen den Taschen und den vorderen Stapelschrauben: dort ist die Wange unten massiv
GUIDE_X = (POD_X1 + STACK_X_FRONT - d.INSERT_BOSS_D / 2) / 2


# --- Hilfsfunktionen ----------------------------------------------------------
def _arc(cy, cz, r, deg0, deg1, steps=8):
    return [
        (cy + r * math.cos(math.radians(deg0 + (deg1 - deg0) * i / steps)),
         cz + r * math.sin(math.radians(deg0 + (deg1 - deg0) * i / steps)))
        for i in range(steps + 1)
    ]


def _lid_curve(steps=16):
    """Kurve an der Unterseite als Punkte (Abstand von der Wange, Hoehe ueber
    der Plattenflaeche). Sie beginnt senkrecht an der Wange mit R_WALL und
    laeuft waagerecht in die Platte aus mit R_FLOOR; dazwischen waechst der
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


LID_CURVE_WIDTH = _lid_curve()[-1][0]
LID_CURVE_HEIGHT = _lid_curve()[0][1]


def _prism_x(points, x0, x1):
    """Profil in der Y-Z-Ebene, von x0 bis x1 gezogen."""
    # Umlaufsinn vereinheitlichen -- sonst zieht ein gespiegeltes Profil nach hinten
    area = sum(a[0] * b[1] - b[0] * a[1] for a, b in zip(points, points[1:] + points[:1]))
    if area < 0:
        points = points[::-1]
    return extrude(Plane.YZ.offset(x0) * Polygon(*points, align=None), amount=x1 - x0)


def _bore_x(x0, x1, y, z, diameter):
    return Pos((x0 + x1) / 2, y, z) * Rot(0, 90, 0) * Cylinder(diameter / 2, x1 - x0)


def _bore_z(x, y, z0, z1, diameter):
    return Pos(x, y, z0) * Cylinder(diameter / 2, z1 - z0, align=MIN_Z)


def _lcyl(lx, ly, lz0, lz1, diameter):
    """Zylinder entlang der Displayachse, lokal."""
    return Pos(lx, ly, lz0) * Cylinder(diameter / 2, lz1 - lz0, align=MIN_Z)


def _box(x0, x1, y0, y1, z0, z1):
    y0, y1 = sorted((y0, y1))
    return Pos(x0, y0, z0) * Box(x1 - x0, y1 - y0, z1 - z0, align=MIN_ALL)


def _rounded_block(x0, x1, half_y, z0, z1, radius):
    sketch = Pos((x0 + x1) / 2, 0, z0) * RectangleRounded(x1 - x0, 2 * half_y, radius)
    return extrude(sketch, amount=z1 - z0)


def _body_outline(z0, z1):
    """Grundriss von Wangen und Unterseite: hinten kleine Kantenradien, vorne
    die groessere Rundung fuer die Anstroemung."""
    r = d.FRONT_CORNER_R
    front = _rounded_block(TRAEGER_X_MIN, TRAEGER_X_MAX, CHEEK_HALF_Y, z0, z1, r)
    keep = Pos(TRAEGER_X_MIN + r, 0, z0) * Box(
        TRAEGER_X_MAX - TRAEGER_X_MIN, 4 * CHEEK_HALF_Y, z1 - z0, align=(Align.MIN, Align.CENTER, Align.MIN)
    )
    rear = _rounded_block(TRAEGER_X_MIN, TRAEGER_X_MAX - r, CHEEK_HALF_Y, z0, z1, CORNER_R)
    return rear + (front & keep)


def _above(z):
    return Pos(0, 0, z) * Box(500, 500, 250, align=MIN_Z)


def _footprint(lz0, lz1, ears, bay_top=None):
    """Umriss des Displaygehaeuses als Vollkoerper (lokal): Rechteck mit
    grossen Eckradien, dazu -- bis bay_top -- der Block des Sensorfachs."""
    body = extrude(Pos(0, 0, lz0) * RectangleRounded(2 * R_OUT, 2 * FLAT_Y, CORNER_OUT_R), amount=lz1 - lz0)
    if bay_top is not None:
        body += _rounded_block(0, BAY_FRONT_LX, BAY_OUT_Y, lz0, min(lz1, bay_top), 3.0)
    return body


def _pocket(lz0, lz1):
    """Innenraum im Grundriss, als Prisma: runde Tasche plus Rechteck des Akkus."""
    h = lz1 - lz0
    return Pos(0, 0, lz0) * (
        Cylinder(DISPLAY_POCKET_R, h, align=MIN_Z) + Box(AKKU_SIZE_X, AKKU_SIZE_Y, h, align=MIN_Z)
    )


def _battery_notch():
    """Aussparung fuer den oberen Teil des Akkus im Gehaeuse. Er wird von unten
    eingesteckt; ueber ihm laeuft die Aussparung unter 45 Grad in die runde
    Tasche aus, damit beim Drucken nichts ueberhaengt."""
    straight = _box(-AKKU_SIZE_X / 2, AKKU_SIZE_X / 2, -AKKU_SIZE_Y / 2, AKKU_SIZE_Y / 2, -0.5, AKKU_NOTCH_LZ)
    roof = extrude(Pos(0, 0, AKKU_NOTCH_LZ) * Rectangle(AKKU_SIZE_X, AKKU_SIZE_Y), amount=4.0, taper=45)
    return straight + roof


# --- Bezugskoerper: Cockpit, Display, Akku ------------------------------------
def stem_cavity():
    """Ausschnitt fuer den Vorbau samt Moosgummi, nach unten offen."""
    half, top, r = CAVITY_HALF, PLATE_BOTTOM_Z, d.CAVITY_TOP_RADIUS
    points = (
        [(half, _SKIRT_Z)]
        + _arc(half - r, top - r, r, 0, 90)
        + _arc(-half + r, top - r, r, 90, 180)
        + [(-half, _SKIRT_Z)]
    )
    return _prism_x(points, -90.0, TRAEGER_X_MAX + 10.0)


def stem_solid():
    """Vorbau: gemessene Breite und Hoehe, oben der Radius des Ausschnitts,
    unten die Kurve der Unterseite. Bis zum Beginn des Uebergangs in den Lenker."""
    half, top, r = d.STEM_WIDTH / 2, STEM_TOP_Z, d.CAVITY_TOP_RADIUS
    curve = _lid_curve()
    points = (
        _arc(half - r, top - r, r, 0, 90)
        + _arc(-half + r, top - r, r, 90, 180)
        + [(-half + dy, STEM_BOTTOM_Z + dz) for dy, dz in curve]
        + [(half - dy, STEM_BOTTOM_Z + dz) for dy, dz in reversed(curve)]
    )
    return _prism_x(points, d.STEM_REAR_END_X, FREE_END_X)


# Der Lenker liegt auf gleicher Hoehe wie der Vorbau [VORGABE].
BAR_TOP_Z = STEM_TOP_Z


def bar_solid():
    """Uebergang des Vorbaus in den Lenker (steigt an und wird breiter) und
    der Lenker selbst, als Stummel gezeichnet."""
    h = d.STEM_HEIGHT_FRONT
    wide = d.STEM_WIDTH + 2 * d.BAR_FLARE_R
    rear = Plane.YZ.offset(FREE_END_X) * Pos(0, -h / 2) * Rectangle(d.STEM_WIDTH, h)
    front = Plane.YZ.offset(d.STEM_TO_BAR_X) * Pos(0, BAR_TOP_Z - h / 2) * Rectangle(wide, h)
    bar = Pos(d.STEM_TO_BAR_X, 0, BAR_TOP_Z) * Box(
        d.BAR_DEPTH, 2 * d.REF_BAR_HALF_WIDTH, h, align=(Align.MIN, Align.CENTER, Align.MAX)
    )
    return loft([rear, front]) + bar


def tops_solid():
    """Oberlenker ("Tops"), schwebt vorne ueber dem Lenker."""
    return Pos(d.STEM_TO_BAR_X, 0, BAR_TOP_Z + d.TOPS_GAP) * Box(
        d.TOPS_DEPTH, 2 * d.REF_BAR_HALF_WIDTH, d.TOPS_HEIGHT, align=(Align.MIN, Align.CENTER, Align.MIN)
    )


def transition_solid():
    """Uebergang vom Vorbau zum Steuerrohr unter dem Vorbau; dreht mit."""
    return Pos(d.STEM_REAR_END_X, 0, STEM_BOTTOM_Z) * Box(
        TRAEGER_X_MIN - d.STEM_REAR_END_X, d.STEM_WIDTH, d.BOTTOM_DEPTH_MAX,
        align=(Align.MIN, Align.CENTER, Align.MAX),
    )


def headtube_solid():
    """Steuerrohr, fest am Rahmen: beginnt BOTTOM_DEPTH_MAX unter dem Vorbau
    und laeuft schraeg nach vorne unten."""
    tube = Rot(0, -d.STEERER_TILT_DEG, 0) * Pos(0, 0, STEM_BOTTOM_Z) * Cylinder(d.HEADTUBE_R, 110, align=MAX_Z)
    return tube - _above(STEM_BOTTOM_Z - d.BOTTOM_DEPTH_MAX)


def lamp_holder_solid():
    """Halter der Frontlampe unter dem Vorbau, vor der Klemmzone."""
    return Pos(FREE_END_X, 0, -d.STEM_HEIGHT_FRONT) * Box(
        d.LAMP_HOLDER_LENGTH, d.LAMP_HOLDER_WIDTH, d.BOTTOM_DEPTH_MAX,
        align=(Align.MIN, Align.CENTER, Align.MAX),
    )


def display_solid():
    """Huelle des Displaymoduls: Deckglas, Koerper, Fahne nach hinten und die
    Steckbuchsen unter der Platine (so hoch wie am echten Modul)."""
    glass_t = d.DISPLAY_GLASS_THICKNESS
    body_h = d.DISPLAY_HEIGHT - glass_t
    body = Pos(0, 0, CUP_H) * Cylinder(d.DISPLAY_GLASS_DIAMETER / 2, glass_t, align=MAX_Z)
    body += Pos(0, 0, CUP_H - glass_t) * Cylinder(d.DISPLAY_BODY_R_MAX, body_h, align=MAX_Z)
    tab_r = d.DISPLAY_GLASS_DIAMETER / 2 - d.DISPLAY_TAB_EDGE_GAP
    body += Pos(0, 0, CUP_H - glass_t) * Box(
        tab_r, d.DISPLAY_TAB_WIDTH, d.DISPLAY_TAB_DEPTH, align=(Align.MAX, Align.CENTER, Align.MAX)
    ) & Pos(0, 0, CUP_H - glass_t) * Cylinder(tab_r, d.DISPLAY_TAB_DEPTH, align=MAX_Z)
    (x0, x1), (y0, y1) = d.I2C_SOCKET_X, d.I2C_SOCKET_Y
    body += Pos(x0, y0, CUP_H - d.DISPLAY_HEIGHT - d.I2C_SOCKET_EXTRA_HEIGHT) * Box(
        x1 - x0, y1 - y0, 4.0, align=MIN_ALL
    )
    return LOC * body


def battery_solid():
    return Pos(AKKU_CX, 0, AKKU_FLOOR_Z) * Box(d.BATTERY_LENGTH, d.BATTERY_WIDTH, d.BATTERY_HEIGHT, align=MIN_Z)


def sensor_solid():
    """Sensorplatine im Fach, liegend; an ihrer Vorderkante stehen Stifte und
    Kabel nach oben (zusammen 18 mm in Z)."""
    x1 = BAY_SENSOR_LX0 + d.SENSOR_SIZE_X
    half = d.SENSOR_SIZE_Y / 2
    board = _box(BAY_SENSOR_LX0, x1, -half, half, 0.3, 0.3 + d.SENSOR_HEIGHT)
    pins = _box(x1 - 4.0, x1, -half, half, 0.3, 0.3 + d.SENSOR_HEIGHT + d.SENSOR_PIN_SPACE)
    return LOC * (board + pins)


def reference_solids():
    """Alles, was kein Druckteil ist: zum Pruefen und Zeichnen."""
    return {
        "ref_vorbau": stem_solid(),
        "ref_lenker": bar_solid(),
        "ref_oberlenker": tops_solid(),
        "ref_uebergang": transition_solid(),
        "ref_steuerrohr": headtube_solid(),
        "ref_lampenhalter": lamp_holder_solid(),
        "ref_display": display_solid(),
        "ref_akku": battery_solid(),
        "ref_sensor": sensor_solid(),
    }


# --- Traeger ------------------------------------------------------------------
def _vent_duct(side):
    """Zuluftkanal einer Seite samt aufgeweitetem Einlass."""
    (x_in, z_in), (x_out, z_out) = VENT_IN, VENT_OUT
    length = math.hypot(x_out - x_in, z_out - z_in)
    axis = ((x_out - x_in) / length, 0, (z_out - z_in) / length)
    plane = Plane(origin=(x_in, side * VENT_DUCT_Y, z_in), x_dir=(0, 1, 0), z_dir=axis)
    r, rf = d.VENT_DUCT_D / 2, d.VENT_INLET_RADIUS
    outside = 10.0      # so weit reicht der Schnitt vor den Einlass hinaus
    duct = plane * Pos(0, 0, -outside) * Cylinder(r, length + outside + 1.0, align=MIN_Z)
    # Einlasskante mit Viertelkreis ausgerundet: Rotationskoerper um die Kanalachse
    steps = 8
    arc = [
        (r + rf - rf * math.sin(math.pi / 2 * i / steps), rf - rf * math.cos(math.pi / 2 * i / steps))
        for i in range(steps + 1)
    ]
    profile = Plane.XZ * Polygon((0, -outside), (r + rf, -outside), *arc, (0, rf), align=None)
    return duct + plane * revolve(profile, axis=Axis.Z)


def _guide(side, z0, height, gap):
    """Zunge (gap=0) bzw. ihr Schlitz in der Wange (gap>0)."""
    return Pos(GUIDE_X, side * GUIDE_Y, z0) * Box(
        d.GUIDE_LENGTH + 2 * gap, d.GUIDE_THICKNESS + 2 * gap, height, align=MIN_Z
    )


def _pod_interior(side):
    half = d.POD_NUT_SPACE / 2
    return _box(POD_IN_X0, POD_IN_X1, side * (POD_Y - half), side * (POD_Y + half), POD_Z - half, PLATE_BOTTOM_Z)


def build_traeger():
    """Keilplatte unter dem Displaygehaeuse plus zwei Wangen am Vorbau.

    Die Oberseite ist eine Ebene (die Fuge zum Displaygehaeuse), gegen den
    Vorbau leicht geneigt, mit einer Mulde fuer den Akku. Gedruckt wird der
    Traeger auf dem Kopf, mit dieser Ebene auf dem Bett (Stuetzen nur in der
    Mulde). Hinter den Wangen reicht nichts unter die Platte."""
    body = _body_outline(TRAEGER_BOTTOM_Z, CHEEK_TOP_Z)
    # Keilplatte mit dem Umriss des Displaygehaeuses, oben durch die Fuge begrenzt
    body += LOC * _footprint(-200.0, 0.0, None, bay_top=0.0) & _above(PLATE_BOTTOM_Z)
    for side, _, _ in PODS:
        body += _box(POD_X0, POD_X1, side * (CHEEK_HALF_Y - CORNER_R - 1.0), side * POD_OUT_Y,
                     POD_BOTTOM_Z, CHEEK_TOP_Z)
    body -= stem_cavity()

    # Taschen: Bohrung in der Stirnwand, dahinter frei; nach oben offen, soweit
    # das Displaygehaeuse darueber innen hohl ist
    hollow = LOC * (_pocket(-200.0, 1.0) + _relief(1, -200.0, 1.0) + _relief(-1, -200.0, 1.0))
    for side, hole_d, _ in PODS:
        body -= _bore_x(POD_X0 - 1.0, POD_IN_X0 + 0.5, side * POD_Y, POD_Z, hole_d)
        body -= _pod_interior(side)
        half = d.POD_NUT_SPACE / 2
        shaft = _box(POD_IN_X0, POD_IN_X1, side * (POD_Y - half), side * (POD_Y + half), PLATE_BOTTOM_Z - 0.5, 80.0)
        body -= shaft & hollow
        body -= _vent_duct(side)

    # Mulde fuer den Akku, waagerecht, mit duennem Boden; davor eine Mulde
    # fuer die Kabel des Sensors
    half_x, half_y = AKKU_SIZE_X / 2, AKKU_SIZE_Y / 2
    body -= _box(AKKU_CX - half_x, AKKU_CX + half_x, -half_y, half_y, AKKU_FLOOR_Z, 60.0)
    body -= _box(AKKU_CX + half_x - 0.5, to_world(BAY_IN_LX1, 0, 0)[0], -BAY_IN_Y, BAY_IN_Y, AKKU_FLOOR_Z, 60.0)

    # Schrauben ins Displaygehaeuse: Durchgang und Senkung von unten
    for lx, ly, length in _base_screws():
        seat = -(length - d.M2_ENGAGEMENT)
        body -= LOC * _lcyl(lx, ly, seat - 0.1, 1.0, d.M2_CLEARANCE_D)
        body -= LOC * _lcyl(lx, ly, seat - 100.0, seat, d.M2_HEAD_D)

    # Schlitze fuer die Fuehrungszungen der Unterseite
    for side in (1, -1):
        body -= _guide(side, TRAEGER_BOTTOM_Z - 0.5, GUIDE_HEIGHT + 1.0, d.GUIDE_GAP)

    # Heatset-Buchsen fuer die Stapelschrauben, von unten eingeschmolzen;
    # darueber frei fuer die Schraube, wenn die Unterseite weit eingeschoben ist
    for x, y in _screw_xy():
        body -= _bore_z(x, y, TRAEGER_BOTTOM_Z - 0.2, TRAEGER_BOTTOM_Z + d.INSERT_DEPTH, d.INSERT_HOLE_D)
        body -= _bore_z(x, y, TRAEGER_BOTTOM_Z, TRAEGER_BOTTOM_Z + STACK_BORE_DEPTH, d.SCREW_CLEARANCE_D)
    return body


# --- Displaygehaeuse ----------------------------------------------------------
def _radial_plane(angle_deg, r, lz):
    """Ebene auf dem Radius r (lokal), z-Achse radial nach aussen, x-Achse entlang der Displayachse."""
    a = math.radians(angle_deg)
    return Plane(
        origin=(r * math.cos(a), r * math.sin(a), lz), x_dir=(0, 0, 1), z_dir=(math.cos(a), math.sin(a), 0)
    )


def _tab_recess(lz0, lz1):
    """Platz fuer die Fahne der Anzeige hinten unter dem Glasrand."""
    r = d.DISPLAY_GLASS_DIAMETER / 2 - d.DISPLAY_TAB_EDGE_GAP + d.DISPLAY_TAB_CLEARANCE
    half = d.DISPLAY_TAB_WIDTH / 2 + d.DISPLAY_TAB_CLEARANCE
    slot = Pos(0, 0, lz0) * Box(r, 2 * half, lz1 - lz0, align=(Align.MAX, Align.CENTER, Align.MIN))
    return slot & Pos(0, 0, lz0) * Cylinder(r, lz1 - lz0, align=MIN_Z)


def build_display_gehaeuse():
    """Ring um das Modul, vorne das Sensorfach. Unten offen: der Traeger
    schliesst ihn ab. Der Akku liegt in der Mulde des Traegers und steht nur
    wenig von unten ins Gehaeuse.

    Gedruckt wird er aufrecht, mit der Fuge auf dem Bett, ohne Stuetzen (das
    Dach des Sensorfachs und die Schlitze sind kurze Bruecken)."""
    body = _footprint(0.0, CUP_H, None, bay_top=BAY_TOP_LZ)
    body -= Pos(0, 0, -0.5) * Cylinder(DISPLAY_POCKET_R, LEDGE_LZ + 0.5, align=MIN_Z)
    body -= _battery_notch()
    # ueber den Taschen: Wand bis auf 1 mm ausgenommen, oben unter 45 Grad geschlossen
    for side in (1, -1):
        body -= _relief(side, -0.5, RELIEF_LZ)
        mid_y = side * (RELIEF_Y0 + RELIEF_Y1) / 2
        body -= extrude(
            Pos((RELIEF_LX0 + RELIEF_LX1) / 2, mid_y, RELIEF_LZ) * Rectangle(RELIEF_LX1 - RELIEF_LX0, RELIEF_Y1 - RELIEF_Y0),
            amount=4.0, taper=45,
        )
    body -= Pos(0, 0, CUP_H) * Cylinder(GLASS_RECESS_R, GLASS_RECESS_DEPTH, align=MAX_Z)
    body -= _tab_recess(AKKU_TOP_LZ, CUP_H + 0.5)

    # Sensorfach: unten offen, zur Tasche hin durchgebrochen, Schlitze seitlich
    body -= _box(DISPLAY_POCKET_R - 2.0, BAY_IN_LX1, -BAY_IN_Y, BAY_IN_Y, -0.5, BAY_IN_H)
    for lx in BAY_SLOT_LX:
        body -= Pos(lx, 0, 2.0) * Box(d.SENSOR_SLOT_WIDTH, 2 * BAY_OUT_Y + 2, d.SENSOR_SLOT_HEIGHT, align=MIN_Z)

    # Abluft hinten, im Windschatten; Ablauf: Kerbe unten an der tiefsten Stelle
    wall = R_OUT - DISPLAY_POCKET_R
    for lz in VENT_OUT_LZ:
        body -= _radial_plane(180, DISPLAY_POCKET_R + wall / 2, lz) * Box(
            d.VENT_OUT_HEIGHT, d.VENT_OUT_WIDTH, wall + 6
        )
    body -= _box(-R_OUT - 1.0, -DISPLAY_POCKET_R + 1.0, -1.5, 1.5, -0.5, 1.5)

    # Deckelschrauben: von unten durch die Ohren, Kopf tief versenkt
    for lx, ly in _lid_ears():
        body -= _lcyl(lx, ly, LID_SEAT_LZ - 0.1, CUP_H + 0.5, d.M2_CLEARANCE_D)
        body -= _lcyl(lx, ly, -0.5, LID_SEAT_LZ, d.M2_HEAD_D)
    # Buchsen fuer die Schrauben aus dem Traeger
    for lx, ly, _ in _base_screws():
        body -= _lcyl(lx, ly, -0.2, d.M2_INSERT_DEPTH, d.M2_INSERT_HOLE_D)
    return LOC * body


# --- Deckel -------------------------------------------------------------------
# Neigung wie beim alten Deggl, aber nur so weit, dass der Sonnenschutz oben
# noch 1 mm ausserhalb des Sichtfensters bleibt
HOOD_LEAN = min(d.HOOD_LEAN, (HOOD_BASE_R - DISPLAY_WINDOW_D / 2 - 1.0) / HOOD_HEIGHT)


def _hood_wall(h):
    """Innenradius und Wandstaerke des Sonnenschutzes in der Hoehe h."""
    t = d.HOOD_WALL_BASE + (d.HOOD_WALL_TOP - d.HOOD_WALL_BASE) * h / HOOD_HEIGHT
    return HOOD_BASE_R - HOOD_LEAN * h, t


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
    return Pos(0, 0, DECKEL_TOP_LZ) * Rot(0, 0, -d.HOOD_HALF_ANGLE) * wall


def build_deckel():
    """Deckel mit Sonnenschutz. Haelt das Modul am Glasrand nieder und nimmt
    unten die Buchsen fuer die Deckelschrauben auf -- oben ist er geschlossen.

    Gedruckt wird er mit der Unterseite auf dem Bett."""
    body = _footprint(CUP_H, DECKEL_TOP_LZ, None)
    body -= Pos(0, 0, CUP_H - 1.0) * Cylinder(DISPLAY_WINDOW_D / 2, DECKEL_THICKNESS + 2.0, align=MIN_Z)
    body -= Pos(0, 0, CUP_H + d.DECKEL_CHAMFER_LAND) * Cone(
        DISPLAY_WINDOW_D / 2, DECKEL_CHAMFER_TOP_R, DECKEL_THICKNESS - d.DECKEL_CHAMFER_LAND, align=MIN_Z
    )
    body += _sunshade()
    for lx, ly in _lid_ears():
        body -= _lcyl(lx, ly, CUP_H - 0.2, CUP_H + d.M2_INSERT_DEPTH, d.M2_INSERT_HOLE_D)
    return LOC * body


# --- Unterseite ---------------------------------------------------------------
def build_unterseite():
    """Klemmplatte unter dem Vorbau. Ihre beiden Kurven bilden die untere
    Rundung des Vorbaus nach und stecken zwischen den Wangen, zwei Zungen
    fuehren sie in Schlitzen der Wangen. Vier Schrauben ziehen sie gegen den
    Vorbau; zwischen Platte und Wange bleibt der Klemmspalt.

    Die Unterseite ist eben: gedruckt wird sie so, wie sie eingebaut ist."""
    top = UNTERSEITE_TOP_Z
    body = _body_outline(UNTERSEITE_BOTTOM_Z, top)
    body = fillet(body.edges().group_by(Axis.Z)[0], d.AKKU_EDGE_FILLET)

    wall_y = CAVITY_HALF - d.AKKU_DECKEL_CURVE_GAP
    outline = _body_outline(UNTERSEITE_BOTTOM_Z, top + LID_CURVE_HEIGHT)
    for side in (1, -1):
        points = [(side * (wall_y - dy), top + dz) for dy, dz in _lid_curve()]
        points += [(side * (wall_y - LID_CURVE_WIDTH), top - 0.2), (side * wall_y, top - 0.2)]
        body += _prism_x(points, TRAEGER_X_MIN, TRAEGER_X_MAX) & outline
        tongue = _guide(side, top - 0.2, GUIDE_HEIGHT + 0.2, 0.0)
        body += chamfer(tongue.edges().group_by(Axis.Z)[-1], 0.8)

    for x, y in _screw_xy():
        body -= _bore_z(x, y, UNTERSEITE_BOTTOM_Z - 0.5, top + 0.5, d.SCREW_CLEARANCE_D)
        body -= _bore_z(x, y, UNTERSEITE_BOTTOM_Z - 0.2, UNTERSEITE_BOTTOM_Z + STACK_HEAD_DEPTH, d.SCREW_HEAD_D)
    return body


# --- Pruefungen ---------------------------------------------------------------
def _volume(shape):
    return 0.0 if shape is None else sum(s.volume for s in shape.solids())


def _overlap(a, b):
    """Gemeinsames Volumen zweier Koerper in mm3."""
    return _volume(a.intersect(b))


def selfcheck(parts):
    refs = reference_solids()
    failures = []

    def inside(part_name, point):
        return parts[part_name].solids()[0].is_inside(Vector(*point))

    stem_mid_z = (STEM_TOP_Z + STEM_BOTTOM_Z) / 2
    ledge_r = (DISPLAY_POCKET_R + GLASS_RECESS_R) / 2
    tab_r = d.DISPLAY_GLASS_DIAMETER / 2 - d.DISPLAY_TAB_EDGE_GAP
    hood_h = HOOD_HEIGHT / 2
    hood_r, hood_t = _hood_wall(hood_h)
    hood_point = lambda deg: to_world(*_polar(deg, hood_r + hood_t / 2), DECKEL_TOP_LZ + hood_h)
    vent_mid = ((VENT_IN[0] + VENT_OUT[0]) / 2, (VENT_IN[1] + VENT_OUT[1]) / 2)
    bay_mid_lx = (R_OUT + BAY_IN_LX1) / 2
    plate_mid = lambda lx, ly: to_world(lx, ly, -plate_thickness(lx) / 2)

    cases = [
        ("Vorbau-Ausschnitt ist frei", (CLAMP_MID, 0, stem_mid_z), False, "traeger"),
        ("Wange links", (CLAMP_MID + 6, CAVITY_HALF + 1.0, stem_mid_z), True, "traeger"),
        ("Wange rechts", (CLAMP_MID + 6, -CAVITY_HALF - 1.0, stem_mid_z), True, "traeger"),
        ("Radius oben im Ausschnitt", (CLAMP_MID, CAVITY_HALF - 0.8, PLATE_BOTTOM_Z - 0.8), True, "traeger"),
        ("Boden unter dem Akku", (AKKU_CX, 0, PLATE_BOTTOM_Z + d.TRAY_FLOOR / 2), True, "traeger"),
        ("Mulde fuer den Akku", (AKKU_CX + AKKU_SIZE_X / 2 - 1.0, AKKU_SIZE_Y / 2 - 1.0, AKKU_FLOOR_Z + 0.5), False, "traeger"),
        ("Mulde fuer die Sensorkabel", (to_world(BAY_IN_LX1 - 2.0, 0, 0)[0], 0, AKKU_FLOOR_Z + 0.5), False, "traeger"),
        ("Boden unter der Kabelmulde", (to_world(BAY_IN_LX1 - 2.0, 0, 0)[0], 0, PLATE_BOTTOM_Z + d.TRAY_FLOOR / 2), True, "traeger"),
        ("Keilplatte hinten", plate_mid(-R_OUT + 2.0, 0), True, "traeger"),
        ("Keilplatte unter der Wand des Sensorfachs", plate_mid(bay_mid_lx, BAY_OUT_Y - 0.5), True, "traeger"),
        ("Oberseite des Traegers ist eben", to_world(-R_OUT + 3.0, 0, 0.3), False, "traeger"),
        ("Fuge neben der Mulde", to_world(AKKU_SIZE_X / 2 + 2.0, BAY_IN_Y + 3.0, -0.5), True, "traeger"),
        ("Zuluftkanal links", (vent_mid[0], VENT_DUCT_Y, vent_mid[1]), False, "traeger"),
        ("Zuluftkanal rechts", (vent_mid[0], -VENT_DUCT_Y, vent_mid[1]), False, "traeger"),
        ("Gehaeuse ist unten offen", to_world(0, 0, 0.5), False, "display_gehaeuse"),
        ("Platz fuer den Akku bis in die Ecken",
         to_world(AKKU_SIZE_X / 2 - 0.5, AKKU_SIZE_Y / 2 - 0.5, AKKU_TOP_LZ / 2), False, "display_gehaeuse"),
        ("Aussparung der Akkuecken endet unter dem Modul",
         to_world(AKKU_SIZE_X / 2 - 0.5, AKKU_SIZE_Y / 2 - 0.5, AKKU_NOTCH_LZ + 5.0), True, "display_gehaeuse"),
        ("Wand an der Akkuecke",
         to_world(AKKU_SIZE_X / 2 + 1.0, AKKU_SIZE_Y / 2 + 1.0, AKKU_TOP_LZ / 2), True, "display_gehaeuse"),
        ("Displaytasche ist frei", to_world(0, 0, CUP_H / 2), False, "display_gehaeuse"),
        ("Glasauflage", to_world(0, ledge_r, LEDGE_LZ - 1.0), True, "display_gehaeuse"),
        ("Glasstufe ist frei", to_world(0, ledge_r, LEDGE_LZ + 0.5), False, "display_gehaeuse"),
        ("Wand an der abgeflachten Seite", to_world(0, FLAT_Y - 0.4, CUP_H - 0.5), True, "display_gehaeuse"),
        ("Aussparung fuer die Displayfahne", to_world(-tab_r, 0, LEDGE_LZ - 1.0), False, "display_gehaeuse"),
        ("Wand hinter der Displayfahne", to_world(-R_OUT + 0.5, 0, CUP_H - 3.0), True, "display_gehaeuse"),
        ("Abluftschlitz hinten", to_world(-(DISPLAY_POCKET_R + R_OUT) / 2, 0, VENT_OUT_LZ[0]), False, "display_gehaeuse"),
        ("Ablaufkerbe hinten", to_world(-R_OUT + 1.0, 0, 0.5), False, "display_gehaeuse"),
        ("Sensorfach ist frei", to_world(bay_mid_lx, 0, BAY_IN_H / 2), False, "display_gehaeuse"),
        ("Sensorfach ist zur Tasche offen", to_world(DISPLAY_POCKET_R + 1.5, 0, BAY_IN_H / 2), False, "display_gehaeuse"),
        ("Sensorfach hat ein Dach", to_world(bay_mid_lx, 0, BAY_IN_H + 1.0), True, "display_gehaeuse"),
        ("Sensorfach hat eine Vorderwand", to_world(BAY_FRONT_LX - 1.0, 0, BAY_IN_H / 2), True, "display_gehaeuse"),
        ("Lueftungsschlitz links im Sensorfach", to_world(BAY_SLOT_LX[0], BAY_IN_Y + 1.0, 4.0), False, "display_gehaeuse"),
        ("Lueftungsschlitz rechts im Sensorfach", to_world(BAY_SLOT_LX[1], -BAY_IN_Y - 1.0, 4.0), False, "display_gehaeuse"),
        ("Wand ueber dem Sensorfach", to_world(R_OUT - 1.0, 0, BAY_IN_H + 2.0), True, "display_gehaeuse"),
        ("Sichtfenster ist frei", to_world(0, 0, CUP_H + DECKEL_THICKNESS / 2), False, "deckel"),
        ("Deckel haelt den Glasrand", to_world(0, DISPLAY_WINDOW_D / 2 + 1.0, CUP_H + 0.4), True, "deckel"),
        ("Sonnenschutz vorne", hood_point(0), True, "deckel"),
        ("Sonnenschutz seitlich", hood_point(90), True, "deckel"),
        ("Sonnenschutz hinten offen", hood_point(180), False, "deckel"),
        ("Unterseite ist geschlossen", (CLAMP_MID, 0, (UNTERSEITE_TOP_Z + UNTERSEITE_BOTTOM_Z) / 2), True, "unterseite"),
        ("Unterseite ist unten eben", (CLAMP_MID, 0, UNTERSEITE_BOTTOM_Z - 0.3), False, "unterseite"),
    ]
    for side, name in ((1, "links"), (-1, "rechts")):
        cases += [
            (f"Kurve an der Unterseite {name}",
             (CLAMP_MID, side * (CAVITY_HALF - 1.0), UNTERSEITE_TOP_Z + 1.0), True, "unterseite"),
            (f"Fuehrungszunge {name}",
             (GUIDE_X, side * GUIDE_Y, UNTERSEITE_TOP_Z + GUIDE_HEIGHT - 1.5), True, "unterseite"),
            (f"Schlitz der Fuehrung {name}",
             (GUIDE_X, side * GUIDE_Y, TRAEGER_BOTTOM_Z + GUIDE_HEIGHT), False, "traeger"),
            (f"Wange aussen neben dem Schlitz {name}",
             (GUIDE_X, side * (CHEEK_HALF_Y - 1.0), TRAEGER_BOTTOM_Z + 3.0), True, "traeger"),
            (f"Wange innen neben dem Schlitz {name}",
             (GUIDE_X, side * (CAVITY_HALF + 1.0), TRAEGER_BOTTOM_Z + 3.0), True, "traeger"),
            (f"Wange ueber dem Schlitz {name}",
             (GUIDE_X, side * GUIDE_Y, TRAEGER_BOTTOM_Z + GUIDE_HEIGHT + 1.5), True, "traeger"),
        ]
    x_mid = (POD_IN_X0 + POD_IN_X1) / 2
    half = d.POD_NUT_SPACE / 2
    for side, hole_d, name in PODS:
        cases += [
            (f"Bohrung {name}", (POD_X0 + d.CHARGE_POD_WALL / 2, side * POD_Y, POD_Z), False, "traeger"),
            (f"Stirnwand {name}", (POD_X0 + d.CHARGE_POD_WALL / 2, side * POD_Y, POD_Z - hole_d / 2 - 0.8), True, "traeger"),
            (f"Tasche {name} ist innen frei", (x_mid, side * (POD_Y + half - 0.5), POD_Z - half + 0.5), False, "traeger"),
            (f"Boden der Tasche {name}", (x_mid, side * POD_Y, POD_BOTTOM_Z + d.POD_SIDE_WALL / 2), True, "traeger"),
            (f"Wand zwischen {name} und Vorbau", (x_mid, side * (CAVITY_HALF + d.POD_SIDE_WALL / 2), POD_Z), True, "traeger"),
            (f"Tasche {name} ist vorne geschlossen", (POD_X1 - 0.6, side * (POD_Y + 4.0), POD_Z - 4.0), True, "traeger"),
            (f"Tasche {name} ist nach oben offen",
             (POD_IN_X0 + 2.0, side * (POD_Y - half + 1.5), PLATE_BOTTOM_Z + 2.0), False, "traeger"),
            (f"Tasche {name} ist bis an die Gehaeusewand offen",
             ((POD_IN_X0 + to_world(RELIEF_LX1, 0, 0)[0]) / 2, side * (RELIEF_Y1 - 1.0), PLATE_BOTTOM_Z + 2.0), False, "traeger"),
            (f"Gehaeuse ueber der Tasche {name} ausgenommen",
             to_world((RELIEF_LX0 + RELIEF_LX1) / 2, side * (RELIEF_Y1 - 1.0), 3.0), False, "display_gehaeuse"),
            (f"Gehaeusewand ueber der Tasche {name}",
             to_world((RELIEF_LX0 + RELIEF_LX1) / 2, side * (FLAT_Y - 0.4), 3.0), True, "display_gehaeuse"),
            (f"Glasauflage ueber der Tasche {name}",
             to_world((RELIEF_LX0 + RELIEF_LX1) / 2, side * (RELIEF_Y1 - 1.0), LEDGE_LZ - 1.0), True, "display_gehaeuse"),
            (f"Tasche {name} hat aussen ein Dach",
             (x_mid, side * (POD_OUT_Y - 2.0), PLATE_BOTTOM_Z + 1.0), True, "traeger"),
            (f"Zuluft muendet in der Tasche {name}",
             (POD_IN_X1 + d.CHARGE_POD_WALL / 2, side * VENT_DUCT_Y, VENT_OUT[1] - 0.3), False, "traeger"),
        ]

    for name, point, expected, part_name in cases:
        if inside(part_name, point) != expected:
            failures.append(f"{name} (Punkt {tuple(round(c, 1) for c in point)}, Teil {part_name})")

    for name, part in parts.items():
        if len(part.solids()) != 1:
            failures.append(f"{name} besteht aus {len(part.solids())} Koerpern statt einem")

    # Stirnflaechen von Buchse und Schalter: Kragen D15 ringsum getragen und frei
    for side, _, name in PODS:
        ring = d.POD_FACE_D / 2 - 0.2
        for a in range(0, 360, 20):
            y = side * POD_Y + ring * math.cos(math.radians(a))
            z = POD_Z + ring * math.sin(math.radians(a))
            if not inside("traeger", (POD_X0 + 0.3, y, z)):
                failures.append(f"{name}: Stirnflaeche traegt den Kragen nicht ringsum (bei {a} Grad)")
                break
            if inside("traeger", (POD_X0 - 1.0, y, z)):
                failures.append(f"{name}: vor der Stirnflaeche ist der Kragen nicht frei (bei {a} Grad)")
                break

    # --- Einbauraum ---
    for name, part in parts.items():
        bb = part.bounding_box()
        if bb.max.X > d.FRONT_LIMIT_X + 0.01:
            failures.append(f"{name} reicht bis X {bb.max.X:+.1f}, erlaubt ist {d.FRONT_LIMIT_X:+.1f}")
        if bb.min.Z < STEM_BOTTOM_Z - d.BOTTOM_DEPTH_MAX - 0.01:
            failures.append(f"{name} reicht {STEM_BOTTOM_Z - bb.min.Z:.1f} mm unter den Vorbau,"
                            f" erlaubt sind {d.BOTTOM_DEPTH_MAX:.0f}")
    # Hinter den Wangen darf nichts unter die Platte reichen (Uebergang zum Steuerrohr)
    behind = Pos(TRAEGER_X_MIN - 0.2, 0, PLATE_BOTTOM_Z - 0.2) * Box(
        150, 200, 150, align=(Align.MAX, Align.CENTER, Align.MAX)
    )
    for name, part in parts.items():
        below = _overlap(part, behind)
        if below > 1.0:
            failures.append(f"{name} reicht hinter den Wangen unter die Platte ({below:.0f} mm3)")

    # --- Stapelschrauben (M3) ---
    wall_probe = d.INSERT_HOLE_D / 2 + d.INSERT_WALL / 2
    for x, y in _screw_xy():
        label = f"Stapelschraube ({x:.0f},{y:.0f})"
        if inside("unterseite", (x, y, UNTERSEITE_TOP_Z - 0.5)):
            failures.append(f"{label} blockiert in der Unterseite")
        for z in (TRAEGER_BOTTOM_Z + d.INSERT_DEPTH / 2, TRAEGER_BOTTOM_Z + STACK_BORE_DEPTH - 0.3):
            if inside("traeger", (x, y, z)):
                failures.append(f"{label} blockiert in der Wange (Z {z:+.1f})")
        z = TRAEGER_BOTTOM_Z + d.INSERT_DEPTH / 2
        for dx, dy in ((0, math.copysign(wall_probe, y)), (math.copysign(wall_probe, x - CLAMP_MID), 0)):
            if not inside("traeger", (x + dx, y + dy, z)):
                failures.append(f"{label}: Buchse hat keine Wand")
        # ueber der Bohrung muss die Wange geschlossen bleiben (Taschen!)
        for dy in (-1, 0, 1):
            if not inside("traeger", (x, y + dy * d.SCREW_CLEARANCE_D / 2, TRAEGER_BOTTOM_Z + STACK_BORE_DEPTH + 0.4)):
                failures.append(f"{label}: Bohrung bricht oben durch")
                break
    for gap, label in ((0.0, "Unterseite auf Anschlag"), (d.CLAMP_TRAVEL, "groesster Spalt")):
        engagement = STACK_ENGAGEMENT_MAX - gap
        if engagement < d.MIN_THREAD_ENGAGEMENT - 1e-9:
            failures.append(f"Stapelschraube, {label}: nur {engagement:.1f} mm Gewinde in der Wange")

    # --- M2-Schrauben ---
    def ring_ok(part_name, lx, ly, lz, r, want):
        return all(
            inside(part_name, to_world(lx + r * math.cos(math.radians(a)), ly + r * math.sin(math.radians(a)), lz)) == want
            for a in range(0, 360, 45)
        )

    insert_ring = d.M2_INSERT_HOLE_D / 2 + 0.4
    seat_ring = (d.M2_CLEARANCE_D + d.M2_HEAD_D) / 4
    for lx, ly in _lid_ears():
        label = f"Deckelschraube (lokal {lx:.0f},{ly:.0f})"
        for part_name, lz in (("display_gehaeuse", (LID_SEAT_LZ + CUP_H) / 2), ("display_gehaeuse", LID_SEAT_LZ / 2),
                              ("deckel", CUP_H + d.M2_INSERT_DEPTH / 2)):
            if inside(part_name, to_world(lx, ly, lz)):
                failures.append(f"{label} blockiert in {part_name}")
        if not ring_ok("deckel", lx, ly, CUP_H + d.M2_INSERT_DEPTH / 2, insert_ring, True):
            failures.append(f"{label}: Buchse im Deckel hat nicht ringsum Wand")
        if not inside("deckel", to_world(lx, ly, DECKEL_TOP_LZ - 0.4)):
            failures.append(f"{label} ist von oben sichtbar")
        if not ring_ok("display_gehaeuse", lx, ly, LID_SEAT_LZ + 1.0, seat_ring, True):
            failures.append(f"{label}: Kopf liegt nicht ringsum auf")
        if not ring_ok("display_gehaeuse", lx, ly, LID_SEAT_LZ / 2, d.M2_HEAD_D / 2 + 0.4, True):
            failures.append(f"{label}: Senkung hat nicht ringsum Wand")
    for lx, ly, length in _base_screws():
        label = f"Traegerschraube (lokal {lx:.0f},{ly:.0f})"
        seat = -(length - d.M2_ENGAGEMENT)
        recess = plate_thickness(lx) + seat
        if recess < d.M2_HEAD_HEIGHT - 0.01:
            failures.append(f"{label}: Kopf steht {d.M2_HEAD_HEIGHT - recess:.1f} mm unter der Platte vor")
        for part_name, lz in (("traeger", seat / 2), ("traeger", seat - min(recess, 2.0) / 2),
                              ("display_gehaeuse", d.M2_INSERT_DEPTH / 2)):
            if inside(part_name, to_world(lx, ly, lz)):
                failures.append(f"{label} blockiert in {part_name}")
        if not ring_ok("display_gehaeuse", lx, ly, d.M2_INSERT_DEPTH / 2, insert_ring, True):
            failures.append(f"{label}: Buchse im Displaygehaeuse hat nicht ringsum Wand")
        if not ring_ok("traeger", lx, ly, seat + 1.0, seat_ring, True):
            failures.append(f"{label}: Kopf liegt nicht ringsum auf")

    if DISPLAY_WINDOW_D < d.DISPLAY_ACTIVE_DIAMETER:
        failures.append("Sichtfenster ist kleiner als die Anzeige")
    if _hood_wall(HOOD_HEIGHT)[0] < DISPLAY_WINDOW_D / 2:
        failures.append("Sonnenschutz verdeckt oben das Sichtfenster")
    if BAY_IN_H > LEDGE_LZ - 1.0:
        failures.append("Sensorfach reicht bis in die Glasauflage")
    if HOOD_BASE_R + d.HOOD_WALL_BASE > FLAT_Y:
        failures.append("Sonnenschutz steht seitlich ueber den Deckel hinaus")

    # --- Durchdringung: Teile untereinander und mit Cockpit, Display, Akku, Sensor ---
    solids = dict(parts, **refs)
    names = list(solids)
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            if a.startswith("ref_") and b.startswith("ref_"):
                continue
            vol = _overlap(solids[a], solids[b])
            if vol > 1.0:
                failures.append(f"{a} und {b} durchdringen sich ({vol:.0f} mm3)")

    # Die Unterseite muss sich bis auf Anschlag einschieben lassen
    vol = _overlap(Pos(0, 0, CLAMP_GAP) * parts["unterseite"], parts["traeger"])
    if vol > 1.0:
        failures.append(f"Unterseite stoesst beim Einschieben an den Traeger ({vol:.0f} mm3)")

    screws = len(_screw_xy()) + len(_lid_ears()) + len(_base_screws())
    if failures:
        print("!! Selbsttest fehlgeschlagen:")
        for f in failures:
            print(f"   - {f}")
    else:
        print(f"Selbsttest: {len(cases)} Stichpunkte, {screws} Verschraubungen, Einbauraum,"
              f" Durchdringung mit Cockpit, Display, Akku und Sensor in Ordnung")
    return not failures


def report(parts):
    top = max(p.bounding_box().max.Z for p in parts.values())
    rear = min(p.bounding_box().min.X for p in parts.values())
    width = max(p.bounding_box().size.Y for n, p in parts.items() if n in ("display_gehaeuse", "deckel"))
    glass_c = to_world(0, 0, CUP_H)
    print("Lage (Nullpunkt = Oberkante Vorbau an der Schaftachse, +X vorne):")
    print(f"  Oberkante Sonnenschutz   {top:+7.1f} mm  ({HOOD_HEIGHT:.0f} hoch)")
    print(f"  Displaymitte (Glas)      X {glass_c[0]:+.1f}, Z {glass_c[2]:+.1f}, Neigung {TILT_DEG:.1f} Grad"
          f" (vorne hoeher)")
    print(f"  Glas hinten / vorne      Z {to_world(-R_OUT, 0, CUP_H)[2]:+.1f} / {to_world(R_OUT, 0, CUP_H)[2]:+.1f}")
    print(f"  Keilplatte               {plate_thickness(-R_OUT):.1f} mm hinten, {plate_thickness(R_OUT):.1f} mm vorne am Kreis,"
          f" {plate_thickness(BAY_FRONT_LX):.1f} mm am Sensorfach")
    print(f"  Vorbau                   {STEM_TOP_Z:+7.1f} .. {STEM_BOTTOM_Z:+.1f} mm")
    print(f"  Wangen unten             {TRAEGER_BOTTOM_Z:+7.1f} mm  (Spalt {CLAMP_GAP:.1f},"
          f" Ausschnitt {d.CAVITY_HEIGHT_MIN:.0f} .. {d.CAVITY_HEIGHT_MIN + d.CLAMP_TRAVEL:.0f} hoch)")
    print(f"  Unterseite               {UNTERSEITE_TOP_Z:+7.1f} .. {UNTERSEITE_BOTTOM_Z:+.1f} mm"
          f"  ({STEM_BOTTOM_Z - UNTERSEITE_BOTTOM_Z:.1f} unter dem Vorbau, erlaubt {d.BOTTOM_DEPTH_MAX:.0f})")
    print()
    print(f"Laenge                     X {rear:+.1f} .. {d.FRONT_LIMIT_X:+.1f}  (Klemmzone {TRAEGER_X_MIN:+.1f} .. {TRAEGER_X_MAX:+.1f})")
    print(f"Displaygehaeuse            {2 * R_OUT:.1f} lang, {width:.1f} breit, Ecken R{CORNER_OUT_R:.1f},"
          f" {CUP_H:.1f} hoch; Glasstufe D{2 * GLASS_RECESS_R:.1f}")
    print(f"Akku                       waagerecht in der Keilplatte, Boden {d.TRAY_FLOOR} mm; steht hinten"
          f" {AKKU_TOP_LZ:.1f} mm ins Gehaeuse, X {AKKU_CX - d.BATTERY_LENGTH / 2:+.1f} .. {AKKU_CX + d.BATTERY_LENGTH / 2:+.1f}")
    print(f"Oeffnung der Taschen       {to_world(RELIEF_LX1, 0, 0)[0] - POD_IN_X0:.1f} x {RELIEF_Y1 - RELIEF_Y0:.1f} mm nach oben")
    print(f"Sensorfach                 innen {BAY_IN_LX1 - R_OUT:.1f} vor dem Gehaeuse, {2 * BAY_IN_Y:.0f} breit, {BAY_IN_H:.0f} hoch;"
          f" aussen {BAY_FRONT_LX - R_OUT:.1f} x {2 * BAY_OUT_Y:.1f}")
    print(f"Deckel                     {DECKEL_THICKNESS:.1f} dick, Fase {DECKEL_CHAMFER_DEG:.1f} Grad, oben D{2 * DECKEL_CHAMFER_TOP_R:.1f}")
    r_top, _ = _hood_wall(HOOD_HEIGHT)
    print(f"Sonnenschutz               innen D{2 * HOOD_BASE_R:.1f} am Fuss -> D{2 * r_top:.1f} oben,"
          f" +-{d.HOOD_HALF_ANGLE:.0f} Grad um die Front")
    print(f"Breite Wangen / Taschen    {2 * CHEEK_HALF_Y:.1f} / {2 * POD_OUT_Y:.1f} mm")
    for side, hole_d, name in PODS:
        print(f"{name:26s} D{hole_d} in {d.CHARGE_POD_WALL} Wand, Oeffnung nach hinten bei X {POD_X0:+.1f},"
              f" Y {side * POD_Y:+.1f}, Z {POD_Z:+.1f}; dahinter {d.POD_NUT_SPACE:.0f} x {d.POD_NUT_SPACE:.0f} x {POD_IN_X1 - POD_IN_X0:.1f}")
    print(f"Zuluft                     2x D{d.VENT_DUCT_D:.0f}, Einlass X {VENT_IN[0]:+.1f}, Y +-{VENT_DUCT_Y:.1f},"
          f" Z {VENT_IN[1]:+.1f}, steigt {VENT_SLOPE_DEG:.0f} Grad in die Taschen")
    print()
    print("Schrauben (alle von unten, in Heatset-Buchsen):")
    print(f"  4x M2x{d.LID_SCREW_LENGTH:.0f}  Displaygehaeuse -> Deckel, Kopf {LID_SEAT_LZ:.1f} mm tief im Gehaeuse versenkt")
    for lx, ly, length in sorted(set((round(lx), abs(round(ly, 3)), length) for lx, ly, length in _base_screws())):
        where = "hinten" if lx < 0 else "vorne"
        print(f"  2x M2x{length:.0f}  Traeger -> Displaygehaeuse {where}"
              f" (Platte dort {plate_thickness(lx):.1f}, Senkung {plate_thickness(lx) - (length - d.M2_ENGAGEMENT):.1f})")
    print(f"  4x M3x{d.STACK_SCREW_LENGTH:.0f}  Unterseite -> Wange  ({STACK_ENGAGEMENT_MAX - d.CLAMP_TRAVEL:.1f} .."
          f" {STACK_ENGAGEMENT_MAX:.1f} mm in der Wange je nach Spalt)")
    print()
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
        "unterseite": build_unterseite(),
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

    refs = reference_solids()
    for name, ref in refs.items():
        ref.label = name
    assembly = Compound(children=list(parts.values()) + list(refs.values()))
    assembly.label = "TRGB_Gehaeuse_CP0007"
    out = "export/TRGB_Gehaeuse.step"
    export_step(assembly, out)
    print(f"Ein STEP geschrieben: {out}  ({len(parts)} Druckteile, {len(refs)} Bezugskoerper 'ref_*')")
    raise SystemExit(0 if ok else 1)
