"""Gehaeuse fuer die FL-Variante (Forumslader) am Diamant Nhoma, erster Entwurf.

Fuenf Druckteile, von oben nach unten:

  fl_deckel         Platte mit Fenster, haelt das Modul am Glasrand nieder
  fl_ring           Wand ringsum, oben die Tasche fuer das Modul, unten das Fach
                    fuer ESP32, Wago-Klemmen, Schalter, USB-C-Buchse und Kabel
  fl_traeger        Boden mit ebener Oberseite; darunter die oberen Klemmschalen
                    am Lenker und hinten der Fuss auf dem Vorbau
  fl_klemme_links   untere Klemmschalen am Lenker
  fl_klemme_rechts

Alle Schrauben kommen von unten und enden in Heatset-Buchsen; von oben ist
keine zu sehen. Gebaut wird im Vorbau-System (siehe dimensions_fl.py), fuer
STEP und Bilder wird um den Vorbau-Winkel gedreht.

Ausfuehren:  cad/.venv/bin/python cad/fl/model_fl.py
"""

import math
import sys
from pathlib import Path

from build123d import (
    Align,
    Axis,
    Box,
    Circle,
    Color,
    Compound,
    Cone,
    Cylinder,
    Kind,
    Plane,
    Polygon,
    Pos,
    Rectangle,
    Rot,
    chamfer,
    export_step,
    extrude,
    fillet,
    offset,
)

sys.path.insert(0, str(Path(__file__).parent))
import dimensions_fl as d  # noqa: E402

HERE = Path(__file__).parent
EXPORT_DIR = HERE / "export"

MIN_ALL = (Align.MIN, Align.MIN, Align.MIN)

# --- Hoehen (lokal z) -------------------------------------------------------------
CARRIER_TOP_Z = d.CARRIER_BOTTOM_Z + d.CARRIER_THICKNESS
MODULE_BOTTOM_Z = CARRIER_TOP_Z + d.BAY_HEIGHT
LEDGE_BOTTOM_Z = MODULE_BOTTOM_Z - d.LEDGE_THICKNESS
GLASS_TOP_Z = MODULE_BOTTOM_Z + d.MODULE_DEPTH
TOP_Z = GLASS_TOP_Z + d.LIP
PARTING_Z = TOP_Z - d.DECKEL_THICKNESS

# --- Grundriss ----------------------------------------------------------------------
XC = d.MODULE_CENTER_X
MODULE_OUT = d.MODULE_R + d.MODULE_CLEARANCE                   # Tasche, radial
REAR_INNER_X = XC - d.MODULE_FLAT_BACK - d.MODULE_CLEARANCE    # Kinn liegt an der Rueckwand
REAR_X = REAR_INNER_X - d.WALL

# Vorbau: Schaftachse und Richtung "Schaft nach oben" im lokalen System
_STEM_TILT = math.radians(d.STEM_ANGLE_DEG) * (1 if d.STEM_NEGATIVE else -1)
STEERER_UP = (-math.sin(_STEM_TILT), math.cos(_STEM_TILT))
STEERER_X = -d.STEM_LENGTH
# Steigung der Vorbau-Achse gegen die Waagerechte
RISE_DEG = 90.0 - d.HEAD_ANGLE_DEG - (d.STEM_ANGLE_DEG if d.STEM_NEGATIVE else -d.STEM_ANGLE_DEG)

# ESP32: vorne im Fach, ohne Stiftleisten flach auf den Traeger geklebt. Eine
# Mulde zum Ausrichten waere kopfueber gedruckt eine Bruecke ueber die ganze Platine.
ESP_X = (-15.0, -15.0 + d.ESP_WIDTH)
ESP_BOTTOM_Z = CARRIER_TOP_Z
ESP_TOP_Z = ESP_BOTTOM_Z + d.ESP_PINS_BELOW + d.ESP_PCB + d.ESP_PARTS_ABOVE

PANEL_Z = CARRIER_TOP_Z + d.PANEL_NUT_SPACE / 2 + 0.25       # Achse Schalter / USB-C
# Kabelverschraubung links: Dichtflaeche bleibt ueber der Fuge zum Traeger
GLAND_Z = CARRIER_TOP_Z + d.GLAND_SEAL_D / 2 + 0.5
GLAND_Y_IN = d.HALF_WIDTH - d.WALL - d.GLAND_NUT_DEPTH

USB_CHAMBER_X = (XC + d.MODULE_USB_X[0] - 1.0, XC + d.MODULE_USB_X[1] + 1.0)
USB_CHAMBER_Y_OUT = -(MODULE_OUT + d.USB_PLUG_SPACE)
USB_CHAMBER_TOP_Z = GLASS_TOP_Z - d.MODULE_USB_DEPTH[0] + 2.0

REAR_LEDGE_HALF_Y = 13.0     # reicht ueber die Buchsen der hinteren Traegerschrauben

CLAMP_Y_MID = (d.CLAMP_Y_IN + d.CLAMP_Y_OUT) / 2
CLAMP_BORE_R = d.BAR_D / 2 + d.CLAMP_RUBBER
CLAMP_TOP_Z = d.CLAMP_SPLIT_GAP / 2
CLAMP_SEAT_Z = CLAMP_TOP_Z + d.M3_INSERT_DEPTH - 0.5 - d.CLAMP_SCREW_LENGTH

LID_SEAT_Z = PARTING_Z + d.M2_ENGAGEMENT - d.LID_SCREW_LENGTH  # Kopfauflage im Ring
LID_BORE_R = d.M2_HEAD_D / 2 + d.M2_WALL
BASE_SEAT_Z = d.CARRIER_BOTTOM_Z + d.M2_HEAD_HEIGHT


# --- 2D-Umrisse -------------------------------------------------------------------
def _outline(inset=0.0):
    """Grundriss des Gehaeuses: Rechteck, vorne grosse, hinten kleine Ecken."""
    face = Rectangle(d.FRONT_X - REAR_X, 2 * d.HALF_WIDTH, align=(Align.MIN, Align.CENTER))
    face = Pos(REAR_X, 0) * face
    face = fillet(face.vertices().filter_by_position(Axis.X, d.FRONT_X - 0.1, d.FRONT_X + 0.1), d.FRONT_CORNER_R)
    face = fillet(face.vertices().filter_by_position(Axis.X, REAR_X - 0.1, REAR_X + 0.1), d.REAR_CORNER_R)
    if inset:
        face = offset(face, -inset, kind=Kind.ARC)
    return face


def _module_face(grow=0.0):
    """Umriss des Moduls (Kreis + Kinn nach hinten), um grow vergroessert."""
    ty, tx = d.MODULE_TANGENT
    trapezoid = Polygon(
        (XC + tx, ty), (XC - d.MODULE_FLAT_BACK, d.MODULE_FLAT_HALF),
        (XC - d.MODULE_FLAT_BACK, -d.MODULE_FLAT_HALF), (XC + tx, -ty),
        (XC, 0), align=None,
    )
    face = Pos(XC, 0) * Circle(d.MODULE_R) + trapezoid
    if grow:
        face = offset(face, grow, kind=Kind.ARC)
    return face


def _slab(face, z0, z1):
    return extrude(Pos(0, 0, z0) * face, z1 - z0)


def _box(x0, x1, y0, y1, z0, z1):
    return Pos(min(x0, x1), min(y0, y1), min(z0, z1)) * Box(
        abs(x1 - x0), abs(y1 - y0), abs(z1 - z0), align=MIN_ALL)


def _vcyl(x, y, r, z0, z1):
    return Pos(x, y, z0) * Cylinder(r, z1 - z0, align=(Align.CENTER, Align.CENTER, Align.MIN))


def _xcyl(y, z, r, x0, x1):
    """Zylinder entlang X."""
    return Pos(x0, y, z) * Rot(0, 90, 0) * Cylinder(r, x1 - x0, align=(Align.CENTER, Align.CENTER, Align.MIN))


def _ycyl(x, z, r, y0, y1):
    """Zylinder entlang Y."""
    return Pos(x, y0, z) * Rot(-90, 0, 0) * Cylinder(r, y1 - y0, align=(Align.CENTER, Align.CENTER, Align.MIN))


# --- Druckteile ----------------------------------------------------------------------
def build_traeger():
    part = _slab(_outline(), d.CARRIER_BOTTOM_Z, CARRIER_TOP_Z)

    # obere Klemmschalen links und rechts neben der Frontplatte, vorne aussen
    # der Ecke des Grundrisses folgend
    for s in (1, -1):
        part += _box(-d.CLAMP_HALF_X, d.CLAMP_HALF_X, s * d.CLAMP_Y_IN, s * d.CLAMP_Y_OUT,
                     CLAMP_TOP_Z, d.CARRIER_BOTTOM_Z) & _slab(_outline(), -50, 50)
    part -= _ycyl(0, 0, CLAMP_BORE_R, -d.CLAMP_Y_OUT - 1, d.CLAMP_Y_OUT + 1)
    for s in (1, -1):
        for sx in (1, -1):
            part -= _vcyl(sx * d.CLAMP_SCREW_X, s * CLAMP_Y_MID, d.M3_INSERT_HOLE_D / 2,
                          CLAMP_TOP_Z - 0.1, CLAMP_TOP_Z + d.M3_INSERT_DEPTH)

    # Fuss auf dem Vorbau (Gummi dazwischen)
    foot_y = d.STEM_BODY_WIDTH / 2 - 1.0
    part += _box(d.FOOT_X[0], d.FOOT_X[1], -foot_y, foot_y,
                 d.STEM_BODY_TOP + d.FOOT_RUBBER, d.CARRIER_BOTTOM_Z)

    # Schrauben Traeger -> Ring: Kopf von unten versenkt
    for x, y in d.BASE_SCREWS:
        part -= _vcyl(x, y, d.M2_CLEARANCE_D / 2, d.CARRIER_BOTTOM_Z - 1, CARRIER_TOP_Z + 1)
        part -= _vcyl(x, y, d.M2_HEAD_D / 2, d.CARRIER_BOTTOM_Z - 1, BASE_SEAT_Z)

    # Ablauf an der tiefsten Stelle (hinten), neben dem Vorbau
    for s in (1, -1):
        part -= _vcyl(-40.0, s * 20.0, 1.5, d.CARRIER_BOTTOM_Z - 1, CARRIER_TOP_Z + 1)
    return part


def build_ring():
    part = _slab(_outline(), CARRIER_TOP_Z, PARTING_Z)
    # Fach unter dem Modul: volle Innenkontur
    part -= _slab(_outline(d.WALL), CARRIER_TOP_Z - 1, MODULE_BOTTOM_Z)
    # Tasche fuer das Modul
    part -= _slab(_module_face(d.MODULE_CLEARANCE), MODULE_BOTTOM_Z - 1, PARTING_Z + 1)

    # Auflage vorne: Block zwischen Vorderwand und Modul, nur in der Mitte (ESP32 daneben)
    front = _box(XC, d.FRONT_X, -12, 12, CARRIER_TOP_Z, MODULE_BOTTOM_Z)
    front -= _slab(_module_face(d.MODULE_CLEARANCE - d.LEDGE_WIDTH), CARRIER_TOP_Z - 1, MODULE_BOTTOM_Z + 1)
    part += front & _slab(_outline(), CARRIER_TOP_Z, MODULE_BOTTOM_Z)
    # Auflage hinten: Leiste an der Rueckwand, darunter 45 Grad (ueber dem Kabel)
    rear = Polygon(
        (REAR_INNER_X - 0.5, MODULE_BOTTOM_Z - d.LEDGE_WIDTH - d.LEDGE_THICKNESS - 0.5),
        (REAR_INNER_X + d.LEDGE_WIDTH, LEDGE_BOTTOM_Z),
        (REAR_INNER_X + d.LEDGE_WIDTH, MODULE_BOTTOM_Z),
        (REAR_INNER_X - 0.5, MODULE_BOTTOM_Z), align=None,
    )
    part += _rear_ledge(rear)

    # Saeulen fuer die Deckelschrauben, Buchsen fuer die Traegerschrauben
    for x, y in d.LID_SCREWS:
        part += _vcyl(x, y, LID_BORE_R, CARRIER_TOP_Z, MODULE_BOTTOM_Z) & _slab(_outline(), CARRIER_TOP_Z, MODULE_BOTTOM_Z)
    # bis unter die Modulauflage, damit sie kopfueber gedruckt nicht frei beginnen
    for x, y in d.BASE_SCREWS:
        boss = _vcyl(x, y, d.M2_INSERT_HOLE_D / 2 + d.M2_WALL, CARRIER_TOP_Z, MODULE_BOTTOM_Z)
        part += boss & _slab(_outline(), CARRIER_TOP_Z, MODULE_BOTTOM_Z)
    for x, y in d.LID_SCREWS:
        part -= _vcyl(x, y, d.M2_HEAD_D / 2, CARRIER_TOP_Z - 1, LID_SEAT_Z)
        part -= _vcyl(x, y, d.M2_CLEARANCE_D / 2, CARRIER_TOP_Z - 1, PARTING_Z + 1)
    for x, y in d.BASE_SCREWS:
        part -= _vcyl(x, y, d.M2_INSERT_HOLE_D / 2, CARRIER_TOP_Z - 1, CARRIER_TOP_Z + d.M2_INSERT_DEPTH + 1.0)

    # Kammer fuer den Winkelstecker am USB-C des Moduls (rechts), unten ins Fach offen
    part -= _box(USB_CHAMBER_X[0], USB_CHAMBER_X[1], -MODULE_OUT + 2.0, USB_CHAMBER_Y_OUT,
                 MODULE_BOTTOM_Z - 1, USB_CHAMBER_TOP_Z)

    # Rueckwand: Schalter (links), USB-C-Buchse (rechts)
    part -= _xcyl(d.PANEL_Y, PANEL_Z, d.SWITCH_HOLE_D / 2, REAR_X - 1, REAR_INNER_X + 1)
    part -= _xcyl(-d.PANEL_Y, PANEL_Z, d.USBC_HOLE_D / 2, REAR_X - 1, REAR_INNER_X + 1)
    # Kabelverschraubung in der linken Seitenwand; die Mutter dreht sich in einer
    # Tasche im Ring, die nach unten ins Fach offen ist
    inner_y = d.HALF_WIDTH - d.WALL
    part -= _ycyl(d.GLAND_X, GLAND_Z, d.GLAND_NUT_SPACE / 2, GLAND_Y_IN, inner_y + 0.01)
    part -= _box(d.GLAND_X - d.GLAND_NUT_SPACE / 2, d.GLAND_X + d.GLAND_NUT_SPACE / 2,
                 GLAND_Y_IN, inner_y + 0.01, CARRIER_TOP_Z - 1, GLAND_Z)
    part -= _ycyl(d.GLAND_X, GLAND_Z, d.GLAND_HOLE_D / 2, inner_y - 1, d.HALF_WIDTH + 1)
    return part


def _rear_ledge(profile):
    # Profil liegt in XZ; als Prisma ueber die Breite der Leiste
    h = REAR_LEDGE_HALF_Y
    return Pos(0, h, 0) * extrude(Plane.XZ * profile, 2 * h)


def build_deckel():
    plate = _slab(_outline(), PARTING_Z, TOP_Z)
    plate = chamfer(plate.edges().group_by(Axis.Z)[-1], d.TOP_CHAMFER)
    part = plate - _slab(_module_face(d.MODULE_CLEARANCE), PARTING_Z - 1, GLASS_TOP_Z)
    window = Pos(XC, 0, GLASS_TOP_Z - 0.01) * Cone(d.WINDOW_D / 2, d.WINDOW_D / 2 + d.LIP + 0.02, d.LIP + 0.02,
                                                     align=(Align.CENTER, Align.CENTER, Align.MIN))
    part -= window
    for x, y in d.LID_SCREWS:
        part -= _vcyl(x, y, d.M2_INSERT_HOLE_D / 2, PARTING_Z - 0.1, PARTING_Z + d.M2_INSERT_DEPTH)
    return part


def build_klemme(side):
    s = 1 if side == "links" else -1
    part = _box(-d.CLAMP_HALF_X, d.CLAMP_HALF_X, s * d.CLAMP_Y_IN, s * d.CLAMP_Y_OUT,
                -d.CLAMP_BELOW, -CLAMP_TOP_Z)
    part = chamfer(part.edges().filter_by(Axis.Y).group_by(Axis.Z)[0], 4.0)
    part &= _slab(_outline(), -50, 50)
    part -= _ycyl(0, 0, CLAMP_BORE_R, -d.CLAMP_Y_OUT - 1, d.CLAMP_Y_OUT + 1)
    for sx in (1, -1):
        part -= _vcyl(sx * d.CLAMP_SCREW_X, s * CLAMP_Y_MID, d.M3_CLEARANCE_D / 2, -d.CLAMP_BELOW - 1, 1)
        part -= _vcyl(sx * d.CLAMP_SCREW_X, s * CLAMP_Y_MID, d.M3_HEAD_D / 2, -d.CLAMP_BELOW - 1, CLAMP_SEAT_Z)
    return part


def build_all():
    parts = {
        "fl_traeger": build_traeger(),
        "fl_ring": build_ring(),
        "fl_deckel": build_deckel(),
        "fl_klemme_links": build_klemme("links"),
        "fl_klemme_rechts": build_klemme("rechts"),
    }
    for name, part in parts.items():
        part.label = name
    return parts


# --- Bezugskoerper ---------------------------------------------------------------------
FRAME_REFS = ("ref_lenker", "ref_vorbau", "ref_klemmung", "ref_frontschrauben", "ref_schaftklemmung",
              "ref_ahead_kappe", "ref_gabelschaft")
ELECTRONICS_REFS = ("ref_modul", "ref_esp32", "ref_schalter", "ref_usbc", "ref_verschraubung", "ref_kabel",
                    "ref_wago", "ref_winkelstecker")
# das Kabel steckt in der Verschraubung
ELECTRONICS_ALLOWED = {("ref_verschraubung", "ref_kabel")}


def _along(axis_xy, origin_xy, r, s0, s1):
    """Zylinder entlang einer Richtung in der XZ-Ebene."""
    ax, az = axis_xy
    angle = math.degrees(math.atan2(ax, az))      # Drehung um Y, die +Z auf die Achse legt
    ox, oz = origin_xy
    return Pos(ox + ax * s0, 0, oz + az * s0) * Rot(0, angle, 0) * Cylinder(
        r, s1 - s0, align=(Align.CENTER, Align.CENTER, Align.MIN))


def reference_solids():
    refs = {}
    half = d.BAR_CLAMP_ZONE_HALF
    bar = _ycyl(0, 0, d.BAR_D / 2, -half, half)
    for s in (1, -1):
        y0, y1 = sorted((s * half, s * (half + 15)))
        bar += Pos(0, s * half, 0) * Rot(-90 * s, 0, 0) * Cone(
            d.BAR_D / 2, d.BAR_TOPS_D / 2, 15, align=(Align.CENTER, Align.CENTER, Align.MIN))
        y0, y1 = sorted((s * (half + 15), s * 200))
        bar += _ycyl(0, 0, d.BAR_TOPS_D / 2, y0, y1)
    refs["ref_lenker"] = bar
    w = d.STEM_BODY_WIDTH / 2
    refs["ref_vorbau"] = _box(STEERER_X, -d.BAR_CLAMP_OD / 2 + 4, -w, w, d.STEM_BODY_BOTTOM, d.STEM_BODY_TOP)
    refs["ref_klemmung"] = _ycyl(0, 0, d.BAR_CLAMP_OD / 2, -d.FACEPLATE_WIDTH / 2, d.FACEPLATE_WIDTH / 2) - _ycyl(
        0, 0, d.BAR_D / 2, -d.FACEPLATE_WIDTH, d.FACEPLATE_WIDTH)
    bolt_x = math.sqrt((d.BAR_CLAMP_OD / 2) ** 2 - d.FACEPLATE_BOLT_Z ** 2)
    bolts = None
    for y in (d.FACEPLATE_BOLT_Y, -d.FACEPLATE_BOLT_Y):
        for z in (d.FACEPLATE_BOLT_Z, -d.FACEPLATE_BOLT_Z):
            b = _xcyl(y, z, d.FACEPLATE_BOLT_HEAD_D / 2, bolt_x - 2, bolt_x + d.FACEPLATE_BOLT_HEAD_PROUD)
            bolts = b if bolts is None else bolts + b
    refs["ref_frontschrauben"] = bolts
    h = d.STEERER_CLAMP_HEIGHT / 2
    refs["ref_schaftklemmung"] = _along(STEERER_UP, (STEERER_X, 0), d.STEERER_CLAMP_OD / 2, -h, h)
    refs["ref_ahead_kappe"] = _along(STEERER_UP, (STEERER_X, 0), d.TOP_CAP_D / 2, h, h + d.TOP_CAP_HEIGHT)
    refs["ref_gabelschaft"] = _along(STEERER_UP, (STEERER_X, 0), d.STEERER_D / 2, -h - 120, -h)

    refs["ref_modul"] = _slab(_module_face(), MODULE_BOTTOM_Z, GLASS_TOP_Z)
    refs["ref_esp32"] = _box(ESP_X[0], ESP_X[1], -d.ESP_LENGTH / 2, d.ESP_LENGTH / 2, ESP_BOTTOM_Z, ESP_TOP_Z)
    body = d.PANEL_BODY_DEPTH
    refs["ref_schalter"] = _xcyl(d.PANEL_Y, PANEL_Z, 6.0, REAR_INNER_X, REAR_INNER_X + body) + _xcyl(
        d.PANEL_Y, PANEL_Z, 7.5, REAR_X - 4, REAR_X)
    refs["ref_usbc"] = _xcyl(-d.PANEL_Y, PANEL_Z, 6.0, REAR_INNER_X, REAR_INNER_X + body) + _xcyl(
        -d.PANEL_Y, PANEL_Z, 7.5, REAR_X - 2.5, REAR_X)
    refs["ref_verschraubung"] = _ycyl(d.GLAND_X, GLAND_Z, 11.0, d.HALF_WIDTH, d.HALF_WIDTH + d.GLAND_OUTSIDE_LENGTH) + _ycyl(
        d.GLAND_X, GLAND_Z, 11.0, GLAND_Y_IN + 1.0, GLAND_Y_IN + 6.0)
    refs["ref_kabel"] = _ycyl(d.GLAND_X, GLAND_Z, d.CABLE_D / 2, GLAND_Y_IN, d.HALF_WIDTH + d.GLAND_OUTSIDE_LENGTH + 25)
    wx, wy, wz = d.WAGO_ZONE
    refs["ref_wago"] = _box(ESP_X[0] - 1.5 - wx, ESP_X[0] - 1.5, -wy / 2, wy / 2, CARRIER_TOP_Z, CARRIER_TOP_Z + wz)
    plug_x = (XC + d.MODULE_USB_X[0] + 4, XC + d.MODULE_USB_X[1] - 4)
    refs["ref_winkelstecker"] = _box(plug_x[0], plug_x[1], -d.MODULE_R, -(d.MODULE_R + d.USB_PLUG_SPACE - 0.6),
                                     GLASS_TOP_Z - d.MODULE_USB_DEPTH[1] + 2, GLASS_TOP_Z - d.MODULE_USB_DEPTH[0])
    for name, ref in refs.items():
        ref.label = name
    return refs


# --- Selbsttest ------------------------------------------------------------------------
def _overlap(a, b):
    try:
        common = a & b
        return common.volume if common is not None else 0.0
    except Exception:
        return 0.0


def selfcheck(parts):
    refs = reference_solids()
    failures = []
    names = list(parts)
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            v = _overlap(parts[a], parts[b])
            if v > 0.01:
                failures.append(f"{a} durchdringt {b} ({v:.2f} mm3)")
        for r, ref in refs.items():
            v = _overlap(parts[a], ref)
            if v > 0.01:
                failures.append(f"{a} durchdringt {r} ({v:.2f} mm3)")
    elec = list(ELECTRONICS_REFS)
    for i, a in enumerate(elec):
        for b in elec[i + 1:]:
            if (a, b) in ELECTRONICS_ALLOWED:
                continue
            v = _overlap(refs[a], refs[b])
            if v > 0.01:
                failures.append(f"{a} stoesst an {b} ({v:.2f} mm3)")

    # Schrauben: genug Gewinde, Koepfe versenkt
    lid_engagement = d.LID_SCREW_LENGTH - (PARTING_Z - LID_SEAT_Z)
    if lid_engagement < d.M2_ENGAGEMENT - 0.05:
        failures.append(f"Deckelschraube: nur {lid_engagement:.1f} mm in der Buchse")
    base_engagement = d.BASE_SCREW_LENGTH - (CARRIER_TOP_Z - BASE_SEAT_Z)
    if base_engagement < 3.0:
        failures.append(f"Traegerschraube: nur {base_engagement:.1f} mm in der Buchse")
    clamp_engagement = CLAMP_SEAT_Z + d.CLAMP_SCREW_LENGTH - CLAMP_TOP_Z
    if clamp_engagement < 5.0:
        failures.append(f"Klemmschraube: nur {clamp_engagement:.1f} mm in der Buchse")

    # Rueckwand vor der Ahead-Kappe
    cap_front = STEERER_X + STEERER_UP[0] * (d.STEERER_CLAMP_HEIGHT / 2) + d.TOP_CAP_D / 2
    if REAR_X - cap_front < d.REAR_GAP_TO_TOPCAP:
        failures.append(f"Rueckwand nur {REAR_X - cap_front:.1f} mm vor der Ahead-Kappe")

    failures += _overhang_failures(parts)

    print(f"Selbsttest: {len(failures)} Fehler")
    for f in failures:
        print("  FEHLER", f)
    return not failures


# Drucklage: kopfueber heisst Oberseite (lokal +z) aufs Bett
PRINT_UPSIDE_DOWN = {"fl_traeger": True, "fl_ring": True, "fl_deckel": True,
                     "fl_klemme_links": False, "fl_klemme_rechts": False}
OVERHANG_AREA_MAX = 100.0   # mm2 je Flaeche; die Auflagen fuer das Modul (2,5 mm breit) liegen darunter


def _overhang_failures(parts):
    """Ebene Flaechen, die in Drucklage nach unten zeigen und nicht auf dem Bett liegen."""
    failures = []
    for name, part in parts.items():
        p = Rot(180, 0, 0) * part if PRINT_UPSIDE_DOWN[name] else part
        bed = p.bounding_box().min.Z
        for face in p.faces():
            if face.normal_at(face.center()).Z < -0.72 and face.center().Z > bed + 0.2 \
                    and face.area > OVERHANG_AREA_MAX:
                failures.append(f"{name}: Ueberhang {face.area:.0f} mm2, {face.center().Z - bed:.1f} mm ueber dem Bett")
    return failures


def report(parts):
    print(f"Vorbau steigt {RISE_DEG:.1f} Grad, Display um denselben Winkel zum Fahrer geneigt")
    print(f"Hoehen ueber der Lenkerachse (senkrecht zum Vorbau): Traeger {d.CARRIER_BOTTOM_Z:.1f}, "
          f"Fach {CARRIER_TOP_Z:.1f}..{MODULE_BOTTOM_Z:.1f}, Glas {GLASS_TOP_Z:.1f}, Oberkante {TOP_Z:.1f}")
    print(f"Grundriss lokal X {REAR_X:+.1f} .. {d.FRONT_X:+.1f} ({d.FRONT_X - REAR_X:.1f} lang), "
          f"Breite {2 * d.HALF_WIDTH:.1f}")
    print(f"ESP32 oben bei {ESP_TOP_Z:.1f}, Modul unten bei {MODULE_BOTTOM_Z:.1f} "
          f"(Luft {MODULE_BOTTOM_Z - ESP_TOP_Z:.1f})")
    print(f"Schrauben: {len(d.LID_SCREWS)}x M2x{d.LID_SCREW_LENGTH:.0f} Ring->Deckel "
          f"(Kopf {LID_SEAT_Z - CARRIER_TOP_Z:.1f} mm tief), {len(d.BASE_SCREWS)}x M2x{d.BASE_SCREW_LENGTH:.0f} "
          f"Traeger->Ring, 4x M3x{d.CLAMP_SCREW_LENGTH:.0f} Klemmen")
    for name, part in parts.items():
        bb = part.bounding_box()
        print(f"  {name:18s} vol={part.volume / 1000:6.1f}cm3  X{bb.min.X:+7.1f}..{bb.max.X:+6.1f}  "
              f"Y{bb.min.Y:+6.1f}..{bb.max.Y:+6.1f}  Z{bb.min.Z:+6.1f}..{bb.max.Z:+6.1f}")


def to_world(shape):
    """Lokales Vorbau-System -> Rad: um die Lenkerachse auf den Vorbau-Winkel drehen."""
    return Rot(0, -RISE_DEG, 0) * shape


if __name__ == "__main__":
    parts = build_all()
    report(parts)
    print("-" * 74)
    ok = selfcheck(parts)
    print("-" * 74)

    EXPORT_DIR.mkdir(exist_ok=True)
    refs = reference_solids()
    world_parts = {}
    for name, part in parts.items():
        w = to_world(part)
        w.label, w.color = name, Color(*d.PART_COLORS[name])
        world_parts[name] = w
    groups = []
    for label, rgb, names in (("Rahmen", d.COLOR_FRAME, FRAME_REFS), ("Elektronik", d.COLOR_ELECTRONICS, ELECTRONICS_REFS)):
        members = []
        for n in names:
            m_ = to_world(refs[n])
            m_.label, m_.color = n, Color(*rgb)
            members.append(m_)
        group = Compound(children=members)
        group.label, group.color = label, Color(*rgb)
        groups.append(group)
    assembly = Compound(children=list(world_parts.values()) + groups)
    assembly.label = "TRGB_Gehaeuse_FL_Nhoma"
    out = EXPORT_DIR / "TRGB_Gehaeuse_FL.step"
    export_step(assembly, str(out))
    print(f"STEP geschrieben: {out.relative_to(HERE.parent.parent)}")
    raise SystemExit(0 if ok else 1)
