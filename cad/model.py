"""Schlankes TRGB-Gehaeuse fuer das Canyon CP0007 Gravel Cockpit CF.

Aufbau (siehe COCKPIT_ANALYSIS.md):

    display_gehaeuse  -- nimmt das runde 2.1"-Display auf, sitzt oben und ragt
                         nach hinten ueber das Steuerrohr
    traeger           -- zwei Wangen links/rechts des Vorbaus, oben durch eine
                         duenne Platte verbunden; das Vorbau-Trapez laeuft voll
                         durch. Klammert nur VOR der Schaftachse, damit beim
                         vollen Lenkeinschlag nichts ans Steuerrohr kommt.
    akku_wanne        -- haengt unter dem Vorbau, vor der Steuerrohr-Freihaltung
    akku_deckel       -- Bodenplatte, Schraubenkoepfe versenkt

traeger und display_gehaeuse sind bewusst EIN Druckteil (`traeger`), weil die
Verbindungsplatte ueber dem Vorbau beides traegt.

Der Vorbau-Ausschnitt entsteht als Loft ueber die Querschnitte in
dimensions.STEM_SECTIONS (zunaechst Trapeze, spaeter verfeinerbar).

Ausfuehren:  cad/.venv/bin/python cad/model.py
"""

import math

from build123d import (
    Box,
    Compound,
    Cylinder,
    Plane,
    Polygon,
    Pos,
    Vector,
    export_step,
    loft,
)

import dimensions as d

# --- Z-Aufteilung -------------------------------------------------------------
# Nullpunkt: Oberkante Vorbau in der Schaftachse.
STEM_BOTTOM_Z = min(z_ok - h for _, _, _, h, z_ok in d.STEM_SECTIONS)


def _stem_z_at(x, which):
    """Oberkante ('top') bzw. Unterkante ('bottom') des Vorbaus an der Stelle x,
    linear zwischen den Stationen interpoliert."""
    sections = sorted(d.STEM_SECTIONS, key=lambda s: s[0])

    def val(section):
        _, _, _, hoehe, z_ok = section
        return z_ok if which == "top" else z_ok - hoehe

    if x <= sections[0][0]:
        return val(sections[0])
    if x >= sections[-1][0]:
        return val(sections[-1])
    for a, b in zip(sections, sections[1:]):
        if a[0] <= x <= b[0]:
            t = (x - a[0]) / (b[0] - a[0])
            return val(a) + t * (val(b) - val(a))
    return val(sections[-1])


def _clamp_range_extremes():
    """Hoechste Oberkante und tiefste Unterkante des Vorbaus INNERHALB des
    Klammerbereichs. Entscheidend, weil der Vorbau nach vorne ansteigt -- die
    globalen Extremwerte liegen ausserhalb und wuerden den Traeger unnoetig
    tief nach unten ziehen, bis in die Steuerrohr-Freihaltung."""
    xs = [d.TRAEGER_X_MIN, d.TRAEGER_X_MAX] + [
        x for x, *_ in d.STEM_SECTIONS if d.TRAEGER_X_MIN <= x <= d.TRAEGER_X_MAX
    ]
    return (
        max(_stem_z_at(x, "top") for x in xs),
        min(_stem_z_at(x, "bottom") for x in xs),
    )


_CLAMP_TOP, _CLAMP_BOTTOM = _clamp_range_extremes()


def _cavity_top_z():
    return _CLAMP_TOP + d.FOAM_LINER_THICKNESS


PLATE_BOTTOM_Z = _cavity_top_z()                       # Unterkante Verbindungsplatte
PLATE_TOP_Z = PLATE_BOTTOM_Z + 4.0                     # Plattendicke 4mm

DISPLAY_FLOOR_Z = PLATE_TOP_Z                          # Boden der Displaytasche
DISPLAY_TOP_Z = DISPLAY_FLOOR_Z + d.DISPLAY_HEIGHT     # Oberkante Displaymodul
TRAEGER_TOP_Z = DISPLAY_TOP_Z                          # Trennfuge zum Deckel
DECKEL_TOP_Z = TRAEGER_TOP_Z + d.SUNSHADE_HEIGHT       # Oberkante Sonnenschutz

TRAEGER_BOTTOM_Z = _CLAMP_BOTTOM - d.FOAM_LINER_THICKNESS
AKKU_TOP_Z = TRAEGER_BOTTOM_Z
AKKU_BOTTOM_Z = _CLAMP_BOTTOM - d.BOTTOM_HEIGHT_MAX
AKKU_WANNE_BOTTOM_Z = AKKU_BOTTOM_Z + 5.0

# Displaygehaeuse: aussen rund um das Modul
DISPLAY_OUTER_D = d.DISPLAY_DIAMETER + 2 * (d.DISPLAY_FIT_CLEARANCE + d.WALL_THICKNESS)
DISPLAY_POCKET_D = d.DISPLAY_DIAMETER + 2 * d.DISPLAY_FIT_CLEARANCE

# Wie weit der Ausschnitt nach unten offen bleibt
_SKIRT_Z = AKKU_BOTTOM_Z - 20.0


# --- Vorbau-Ausschnitt --------------------------------------------------------
def _section_points(section, liner):
    """Trapez-Querschnitt des Vorbaus plus nach unten offener Schacht."""
    _, b_unten, b_oben, hoehe, z_ok = section
    half_o = b_oben / 2 + liner
    half_u = b_unten / 2 + liner
    z_top = z_ok + liner
    z_bot = z_ok - hoehe - liner
    return [
        (half_o, z_top),
        (-half_o, z_top),
        (-half_u, z_bot),
        (-half_u, _SKIRT_Z),
        (half_u, _SKIRT_Z),
        (half_u, z_bot),
    ]


def _through_sections():
    """Stationen, vorne und hinten ueberstehend -- das Trapez laeuft voll durch
    den Traeger durch, es bleibt also kein Material davor oder dahinter."""
    sections = sorted(d.STEM_SECTIONS, key=lambda s: s[0])
    rear = (d.TRAEGER_X_MIN - 40.0,) + tuple(sections[0][1:])
    front = (d.TRAEGER_X_MAX + 40.0,) + tuple(sections[-1][1:])
    return [rear] + sections + [front]


def stem_cavity(liner=None):
    liner = d.FOAM_LINER_THICKNESS if liner is None else liner
    profiles = [
        Plane.YZ.offset(s[0]) * Polygon(*_section_points(s, liner), align=None)
        for s in _through_sections()
    ]
    return loft(profiles, ruled=True)


def cavity_half_width_at(x, liner=None):
    liner = d.FOAM_LINER_THICKNESS if liner is None else liner
    sections = sorted(d.STEM_SECTIONS, key=lambda s: s[0])

    def half(section):
        return max(section[1], section[2]) / 2 + liner

    if x <= sections[0][0]:
        return half(sections[0])
    if x >= sections[-1][0]:
        return half(sections[-1])
    for a, b in zip(sections, sections[1:]):
        if a[0] <= x <= b[0]:
            t = (x - a[0]) / (b[0] - a[0])
            return half(a) + t * (half(b) - half(a))
    return half(sections[-1])


def max_cavity_half_width(liner=None):
    liner = d.FOAM_LINER_THICKNESS if liner is None else liner
    return max(max(s[1], s[2]) / 2 + liner for s in d.STEM_SECTIONS)


# --- Breite und Schrauben -----------------------------------------------------
# Die Stapelschrauben enden in Heatset-Buchsen in den Wangen. Massgeblich ist
# daher nicht der Schraubendurchmesser, sondern der Buchsendurchmesser plus
# Wand ringsum -- das bestimmt direkt, wie breit der Traeger werden muss.
SCREW_X_REAR = d.TRAEGER_X_MIN + 10.0
SCREW_X_FRONT = d.TRAEGER_X_MAX - 10.0


def _screw_xy():
    """Vier Stapelschrauben, senkrecht durch Akkukasten in die Wangen."""
    positions = []
    for x in (SCREW_X_REAR, SCREW_X_FRONT):
        y = cavity_half_width_at(x) + d.INSERT_BOSS_D / 2
        positions += [(x, y), (x, -y)]
    return positions


def traeger_width_y():
    """Aussenbreite des Klammerteils: Ausschnitt + Buchsendom + Wand."""
    return 2 * max(abs(y) + d.INSERT_BOSS_D / 2 for _, y in _screw_xy())


def _screw_holes(solid, diameter, z_from, z_to):
    height = z_to - z_from
    for x, y in _screw_xy():
        solid -= Pos(x, y, z_from + height / 2) * Cylinder(diameter / 2, height)
    return solid


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
def build_traeger():
    """Zwei Wangen am Vorbau, oben durch eine duenne Platte verbunden.

    Das Displaygehaeuse ist bewusst NICHT mehr Teil davon -- getrennt gedruckt
    laesst sich beides ohne Stuetzmaterial und in der jeweils guenstigsten Lage
    drucken.
    """
    width = traeger_width_y()
    clamp_len = d.TRAEGER_X_MAX - d.TRAEGER_X_MIN
    clamp_mid = (d.TRAEGER_X_MAX + d.TRAEGER_X_MIN) / 2

    clamp_h = PLATE_BOTTOM_Z - TRAEGER_BOTTOM_Z
    body = Pos(clamp_mid, 0, TRAEGER_BOTTOM_Z + clamp_h / 2) * Box(
        clamp_len, width, clamp_h
    )

    plate_h = PLATE_TOP_Z - PLATE_BOTTOM_Z
    body += Pos(clamp_mid, 0, PLATE_BOTTOM_Z + plate_h / 2) * Box(
        clamp_len, width, plate_h
    )

    body -= stem_cavity()

    cable_y = max_cavity_half_width() + d.CABLE_HOLE_D / 2 + 1.0
    body -= Pos(clamp_mid, cable_y, (PLATE_TOP_Z + TRAEGER_BOTTOM_Z) / 2) * Cylinder(
        d.CABLE_HOLE_D / 2, PLATE_TOP_Z - TRAEGER_BOTTOM_Z
    )

    # Heatset-Buchsen fuer die Stapelschrauben, von unten eingeschmolzen
    for x, y in _screw_xy():
        body -= Pos(x, y, TRAEGER_BOTTOM_Z + d.INSERT_DEPTH / 2) * Cylinder(
            d.INSERT_HOLE_D / 2, d.INSERT_DEPTH
        )

    # Durchgang fuer die Schrauben, die das Displaygehaeuse von unten halten
    for x, y in _housing_screw_xy():
        body -= Pos(x, y, (PLATE_BOTTOM_Z + PLATE_TOP_Z) / 2) * Cylinder(
            d.SCREW_CLEARANCE_D / 2, plate_h + 1
        )
        body -= Pos(x, y, PLATE_BOTTOM_Z + d.SCREW_HEAD_DEPTH / 2) * Cylinder(
            d.SCREW_HEAD_D / 2, d.SCREW_HEAD_DEPTH
        )

    return body


HOUSING_FOOT_R = DISPLAY_OUTER_D / 2 + d.INSERT_BOSS_D / 2 - 1.5
HOUSING_FOOT_H = 8.0    # Hoehe der Fuesse, damit die Buchse Platz hat


def _housing_screw_xy():
    """Zwei Fuesse, mit denen der Becher von unten an der Traegerplatte haengt.

    Bei +-45 Grad nach vorne gelegt: dort liegen sie sicher ueber der Platte
    (der Becher kragt nach hinten darueber hinaus) und ausserhalb der
    Displaytasche, gehen also am Modul vorbei.
    """
    out = []
    for a in (45.0, -45.0):
        out.append(
            (
                d.DISPLAY_CENTER_X + HOUSING_FOOT_R * math.cos(math.radians(a)),
                HOUSING_FOOT_R * math.sin(math.radians(a)),
            )
        )
    return out


def build_display_gehaeuse():
    """Runder Becher fuer das 2.1"-Modul. Sitzt auf der Traegerplatte, von
    unten verschraubt; oben nimmt er den Deckel auf."""
    height = TRAEGER_TOP_Z - PLATE_TOP_Z
    mid_z = PLATE_TOP_Z + height / 2
    body = Pos(d.DISPLAY_CENTER_X, 0, mid_z) * Cylinder(DISPLAY_OUTER_D / 2, height)

    # Augen fuer die Deckelschrauben, aussen angesetzt statt die ganze Wand zu
    # verdicken -- sonst wuerde der Becher unnoetig gross
    for x, y in _deckel_screw_xy():
        body += Pos(x, y, mid_z) * Cylinder(d.INSERT_BOSS_D / 2, height)

    # Displaytasche
    body -= Pos(d.DISPLAY_CENTER_X, 0, PLATE_TOP_Z + d.DISPLAY_HEIGHT / 2) * Cylinder(
        DISPLAY_POCKET_D / 2, d.DISPLAY_HEIGHT + 0.5
    )
    # seitlicher Ueberstand des Moduls
    body -= Pos(
        d.DISPLAY_CENTER_X - DISPLAY_POCKET_D / 2, 0, PLATE_TOP_Z + d.DISPLAY_HEIGHT / 2
    ) * Box(2 * d.DISPLAY_TAB_OVERHANG, 22.0, d.DISPLAY_HEIGHT + 0.5)

    # Heatset-Buchsen fuer den Deckel, von oben eingeschmolzen
    for x, y in _deckel_screw_xy():
        body -= Pos(x, y, TRAEGER_TOP_Z - d.INSERT_DEPTH / 2) * Cylinder(
            d.INSERT_HOLE_D / 2, d.INSERT_DEPTH
        )

    # Befestigungsfuesse zur Traegerplatte, mit Buchse von unten
    for x, y in _housing_screw_xy():
        body += Pos(x, y, PLATE_TOP_Z + HOUSING_FOOT_H / 2) * Cylinder(
            d.INSERT_BOSS_D / 2, HOUSING_FOOT_H
        )
    for x, y in _housing_screw_xy():
        body -= Pos(x, y, PLATE_TOP_Z + d.INSERT_DEPTH / 2) * Cylinder(
            d.INSERT_HOLE_D / 2, d.INSERT_DEPTH
        )

    # Kabeldurchlass nach unten zur Elektronik
    body -= Pos(d.DISPLAY_CENTER_X, 0, PLATE_TOP_Z + 1.5) * Cylinder(
        d.CABLE_HOLE_D / 2, 6.0
    )
    return body


def _deckel_screw_xy():
    """Drei Schrauben auf dem Displaykreis, aussen an Augen gefuehrt."""
    r = DISPLAY_OUTER_D / 2 + d.INSERT_BOSS_D / 2 - 2.0
    return [
        (
            d.DISPLAY_CENTER_X + r * math.cos(math.radians(a)),
            r * math.sin(math.radians(a)),
        )
        for a in (90, 210, 330)
    ]


def build_deckel():
    """Runder Deckel mit Sonnenschutz-Kragen, von oben in die Buchsen des
    Displaygehaeuses geschraubt (Senkkopf, buendig)."""
    height = DECKEL_TOP_Z - TRAEGER_TOP_Z
    mid_z = TRAEGER_TOP_Z + height / 2
    body = Pos(d.DISPLAY_CENTER_X, 0, mid_z) * Cylinder(DISPLAY_OUTER_D / 2, height)
    for x, y in _deckel_screw_xy():
        body += Pos(x, y, mid_z) * Cylinder(d.INSERT_BOSS_D / 2, height)

    body -= Pos(d.DISPLAY_CENTER_X, 0, mid_z) * Cylinder(
        d.DISPLAY_WINDOW_DIAMETER / 2, height + 2
    )
    for x, y in _deckel_screw_xy():
        body -= Pos(x, y, mid_z) * Cylinder(d.SCREW_CLEARANCE_D / 2, height + 2)
        body -= Pos(x, y, DECKEL_TOP_Z - d.SCREW_HEAD_DEPTH / 2) * Cylinder(
            d.SCREW_HEAD_D / 2, d.SCREW_HEAD_DEPTH
        )
    return body


def build_akku_wanne():
    height = AKKU_TOP_Z - AKKU_WANNE_BOTTOM_Z
    length = d.BATTERY_WIDTH + 2 * (d.BATTERY_WALL + d.BATTERY_CLEARANCE)
    width = d.BATTERY_LENGTH + 2 * (d.BATTERY_WALL + d.BATTERY_CLEARANCE)
    width = max(width, traeger_width_y())
    body = Pos(d.BATTERY_CENTER_X, 0, AKKU_WANNE_BOTTOM_Z + height / 2) * Box(
        length, width, height
    )
    pocket_h = d.BATTERY_HEIGHT + d.BATTERY_CLEARANCE
    body -= Pos(d.BATTERY_CENTER_X, 0, AKKU_WANNE_BOTTOM_Z + pocket_h / 2) * Box(
        d.BATTERY_WIDTH + 2 * d.BATTERY_CLEARANCE,
        d.BATTERY_LENGTH + 2 * d.BATTERY_CLEARANCE,
        pocket_h,
    )
    body = _screw_holes(body, d.SCREW_CLEARANCE_D, AKKU_WANNE_BOTTOM_Z, AKKU_TOP_Z)
    return body


def build_akku_deckel():
    height = AKKU_WANNE_BOTTOM_Z - AKKU_BOTTOM_Z
    length = d.BATTERY_WIDTH + 2 * (d.BATTERY_WALL + d.BATTERY_CLEARANCE)
    width = max(
        d.BATTERY_LENGTH + 2 * (d.BATTERY_WALL + d.BATTERY_CLEARANCE), traeger_width_y()
    )
    body = Pos(d.BATTERY_CENTER_X, 0, AKKU_BOTTOM_Z + height / 2) * Box(
        length, width, height
    )
    body = _screw_holes(body, d.SCREW_CLEARANCE_D, AKKU_BOTTOM_Z, AKKU_WANNE_BOTTOM_Z)
    for x, y in _screw_xy():
        body -= Pos(x, y, AKKU_BOTTOM_Z + d.SCREW_HEAD_DEPTH / 2) * Cylinder(
            d.SCREW_HEAD_D / 2, d.SCREW_HEAD_DEPTH
        )
    return body


# --- Pruefungen ---------------------------------------------------------------
def selfcheck(parts):
    stem_mid_z = (_CLAMP_TOP + _CLAMP_BOTTOM) / 2
    side_y = max_cavity_half_width() + 2.0
    clamp_mid = (d.TRAEGER_X_MAX + d.TRAEGER_X_MIN) / 2
    probe_x = d.TRAEGER_X_MIN + 5.0   # abseits von Kabelloch und Schrauben
    traeger = parts["traeger"]

    cases = [
        ("Vorbau-Ausschnitt ist frei", (clamp_mid, 0, stem_mid_z), False, "traeger"),
        ("Wange links ist Material", (probe_x, side_y, stem_mid_z), True, "traeger"),
        ("Wange rechts ist Material", (probe_x, -side_y, stem_mid_z), True, "traeger"),
        ("Verbindungsplatte ueber dem Vorbau", (clamp_mid, 0, PLATE_BOTTOM_Z + 2.0), True, "traeger"),
        ("Trapez laeuft hinten durch", (d.TRAEGER_X_MIN + 1, 0, stem_mid_z), False, "traeger"),
        ("Trapez laeuft vorne durch", (d.TRAEGER_X_MAX - 1, 0, stem_mid_z), False, "traeger"),
        ("Displaytasche ist frei", (d.DISPLAY_CENTER_X, 0, DISPLAY_FLOOR_Z + 5), False, "display_gehaeuse"),
        # Probe bei -90 Grad: dort sitzt weder die Modul-Lasche (-X) noch ein Auge
        ("Becherwand ist Material",
         (d.DISPLAY_CENTER_X, -(DISPLAY_OUTER_D / 2 - 1.2), DISPLAY_FLOOR_Z + 5), True, "display_gehaeuse"),
        ("Deckel hat Sichtfenster", (d.DISPLAY_CENTER_X, 0, DECKEL_TOP_Z - 1.0), False, "deckel"),
    ]
    failures = []
    for name, point, expected, part_name in cases:
        solid = parts[part_name].solids()[0]
        if solid.is_inside(Vector(*point)) != expected:
            failures.append(f"{name} (Punkt {point}, Teil {part_name})")

    # Steuerrohr: nichts darf in den Schwenkbereich ragen
    keepout = headtube_keepout()
    keepout_hits = []
    for name, part in parts.items():
        overlap = part.intersect(keepout)
        solids = overlap.solids() if overlap is not None else []
        vol = sum(s.volume for s in solids)
        if vol > 1.0:
            keepout_hits.append((name, vol))

    # Verschraubung: jede Schraube muss durch alle Teile darueber/darunter
    # passen und in genau einer Buchse enden
    def air(part_name, point):
        return not parts[part_name].solids()[0].is_inside(Vector(*point))

    for x, y in _screw_xy():
        for part_name, z in (
            ("akku_deckel", (AKKU_WANNE_BOTTOM_Z + AKKU_BOTTOM_Z) / 2),
            ("akku_wanne", (AKKU_TOP_Z + AKKU_WANNE_BOTTOM_Z) / 2),
            ("traeger", TRAEGER_BOTTOM_Z + 2.0),
        ):
            if not air(part_name, (x, y, z)):
                failures.append(f"Stapelschraube ({x:.0f},{y:.0f}) blockiert in {part_name}")
    for x, y in _housing_screw_xy():
        if not air("traeger", (x, y, (PLATE_BOTTOM_Z + PLATE_TOP_Z) / 2)):
            failures.append(f"Becherschraube ({x:.0f},{y:.0f}) blockiert in der Platte")
        if not air("display_gehaeuse", (x, y, PLATE_TOP_Z + 2.0)):
            failures.append(f"Becherschraube ({x:.0f},{y:.0f}) hat keine Buchse im Becher")
    for x, y in _deckel_screw_xy():
        if not air("deckel", (x, y, (TRAEGER_TOP_Z + DECKEL_TOP_Z) / 2)):
            failures.append(f"Deckelschraube ({x:.0f},{y:.0f}) blockiert im Deckel")
        if not air("display_gehaeuse", (x, y, TRAEGER_TOP_Z - 2.0)):
            failures.append(f"Deckelschraube ({x:.0f},{y:.0f}) hat keine Buchse im Becher")

    if failures:
        print("!! Selbsttest fehlgeschlagen:")
        for f in failures:
            print(f"   - {f}")
    else:
        print(f"Selbsttest: {len(cases)} Stichpunkte + "
              f"{len(_screw_xy()) + len(_housing_screw_xy()) + len(_deckel_screw_xy())} Verschraubungen in Ordnung")

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
    print(f"  Sonnenschutz oben        {DECKEL_TOP_Z:+7.1f} mm")
    print(f"  Displayglas oben         {DISPLAY_TOP_Z:+7.1f} mm")
    print(f"  Displayboden             {DISPLAY_FLOOR_Z:+7.1f} mm")
    print(f"  Verbindungsplatte        {PLATE_BOTTOM_Z:+7.1f} .. {PLATE_TOP_Z:+.1f} mm")
    print(f"  Vorbau Oberkante            0.0 mm")
    print(f"  Vorbau Unterkante hinten {STEM_BOTTOM_Z:+7.1f} mm  (ausserhalb des Klammerbereichs)")
    print(f"  Vorbau im Klammerbereich {_CLAMP_TOP:+7.1f} .. {_CLAMP_BOTTOM:+.1f} mm")
    print(f"  Steuersatz Oberkante     {d.HEADSET_TOP_Z:+7.1f} mm  (Freihaltung R{d.HEADTUBE_KEEPOUT_R:.0f})")
    print(f"  Unterkante Akku          {AKKU_BOTTOM_Z:+7.1f} mm")
    print()
    print(f"Oberteil ueber Vorbau      {DECKEL_TOP_Z:7.1f} mm  (Vorgabe max {d.TOP_HEIGHT_MAX:.0f})")
    print(f"Akku unter Vorbau          {_CLAMP_BOTTOM - AKKU_BOTTOM_Z:7.1f} mm  (Vorgabe max {d.BOTTOM_HEIGHT_MAX:.0f})")
    print(f"Traeger klammert bei X     {d.TRAEGER_X_MIN:+.0f} .. {d.TRAEGER_X_MAX:+.0f} mm")
    widest = 2 * max_cavity_half_width()
    print(f"Traegerbreite Y            {traeger_width_y():7.1f} mm"
          f"  = Ausschnitt {widest:.1f} + 2x Buchsendom {d.INSERT_BOSS_D:.1f}")
    print(f"Display D                  {DISPLAY_OUTER_D:7.1f} mm  (Modul D{d.DISPLAY_DIAMETER})")
    print()
    for name, part in parts.items():
        bb = part.bounding_box()
        print(
            f"  {name:18s} vol={part.volume:9.1f}mm3  "
            f"X{bb.min.X:+7.1f}..{bb.max.X:+6.1f}  "
            f"Y{bb.size.Y:6.1f}  Z{bb.min.Z:+7.1f}..{bb.max.Z:+6.1f}"
        )
    print()
    if DECKEL_TOP_Z > d.TOP_HEIGHT_MAX:
        print(f"!! Oberkante {DECKEL_TOP_Z:.1f} mm ueberschreitet die Vorgabe von {d.TOP_HEIGHT_MAX:.0f} mm")


def build_all():
    parts = {
        "traeger": build_traeger(),
        "display_gehaeuse": build_display_gehaeuse(),
        "deckel": build_deckel(),
        "akku_wanne": build_akku_wanne(),
        "akku_deckel": build_akku_deckel(),
    }
    for name, part in parts.items():
        part.label = name
    return parts


if __name__ == "__main__":
    parts = build_all()
    report(parts)
    print("-" * 74)
    selfcheck(parts)
    print("-" * 74)

    assembly = Compound(children=list(parts.values()))
    assembly.label = "TRGB_Gehaeuse_CP0007"
    out = "export/TRGB_Gehaeuse.step"
    export_step(assembly, out)
    print(f"Ein STEP mit {len(parts)} benannten Teilen geschrieben: {out}")
