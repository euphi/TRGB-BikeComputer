"""Rendert das Gehaeusemodell als PNG.

Reiner CPU-Rasterizer mit Z-Buffer: braucht nur numpy, kein OpenGL und keinen
Framebuffer. Das ist hier nicht nur bequemer, sondern fuer die Pruefung auch
verlaesslicher als eine GL-Ansicht -- die Verdeckung stimmt pixelgenau, es gibt
keine Sortierartefakte bei ineinandergreifenden Teilen.

Ausfuehren:  cad/.venv/bin/python cad/render.py
Ergebnis:    cad/export/ansicht_<name>.png
"""

import struct
import zlib
from pathlib import Path

import numpy as np

import model as m

EXPORT_DIR = Path(__file__).parent / "export"

# Blickrichtungen als Vektor "von wo schaut die Kamera" in Fahrradkoordinaten
# (+X vorne, +Y links, +Z oben).
VIEWS = {
    "iso": (1.0, 1.0, 0.7),
    "seite": (0.0, 1.0, 0.0),     # von links, vorne ist im Bild links
    "oben": (0.0, 0.0, 1.0),      # vorne ist im Bild oben
    "vorne": (1.0, 0.0, 0.0),
    "hinten_unten": (-1.0, 0.8, -0.7),
    "hinten_rechts": (-1.0, -0.7, 0.5),   # Ladebuchse
    "hinten_links": (-1.0, 0.7, 0.15),    # Schalter, Abluft
    "vorne_schraeg": (1.0, 0.35, 0.25),   # Anstroemung
}

# Grundfarbe je Teil, damit sich die Teile im Bild unterscheiden lassen
PART_COLORS = {
    "traeger": (118, 152, 198),
    "display_gehaeuse": (214, 158, 92),
    "deckel": (206, 110, 104),
    "unterseite": (124, 180, 134),
    # Bezugskoerper, keine Druckteile
    "ref_vorbau": (92, 92, 96),
    "ref_lenker": (92, 92, 96),
    "ref_oberlenker": (104, 104, 110),
    "ref_uebergang": (120, 104, 96),
    "ref_steuerrohr": (150, 120, 100),
    "ref_lampenhalter": (170, 150, 90),
    "ref_display": (52, 60, 78),
    "ref_akku": (90, 150, 190),
    "ref_sensor": (120, 190, 120),
}
DEFAULT_COLOR = (170, 170, 170)

BACKGROUND = (250, 250, 248)
LIGHT_DIR = np.array([0.4, 0.75, 0.52])
LIGHT_DIR = LIGHT_DIR / np.linalg.norm(LIGHT_DIR)
AMBIENT = 0.34

WIDTH, HEIGHT = 1100, 820
TESSELLATION_TOLERANCE = 0.12


def _write_png(path, rgb):
    """Minimaler PNG-Writer (8 Bit, Truecolor) -- spart die Pillow-Abhaengigkeit."""
    height, width, _ = rgb.shape
    raw = b"".join(b"\x00" + rgb[y].tobytes() for y in range(height))

    def chunk(tag, data):
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 6))
    png += chunk(b"IEND", b"")
    Path(path).write_bytes(png)


def _camera_basis(direction):
    """Rechtshaendiges Kamerasystem: right/up spannen die Bildebene auf,
    forward zeigt von der Szene zur Kamera."""
    forward = np.array(direction, dtype=float)
    forward /= np.linalg.norm(forward)
    world_up = np.array([0.0, 0.0, 1.0])
    if abs(float(forward @ world_up)) > 0.999:   # Draufsicht: Z taugt nicht als Referenz
        world_up = np.array([1.0, 0.0, 0.0])
    right = np.cross(world_up, forward)
    right /= np.linalg.norm(right)
    up = np.cross(forward, right)
    return right, up, forward


def _tessellate(parts):
    """Alle Teile zu Dreiecken, in Weltkoordinaten, mit Farbe je Teil."""
    meshes = []
    for name, part in parts.items():
        verts, tris = part.tessellate(tolerance=TESSELLATION_TOLERANCE)
        if not tris:
            continue
        points = np.array([[v.X, v.Y, v.Z] for v in verts], dtype=float)
        faces = np.array(tris, dtype=np.int32)
        meshes.append((name, points, faces, np.array(PART_COLORS.get(name, DEFAULT_COLOR), float)))
    return meshes


def _rasterize(meshes, direction):
    right, up, forward = _camera_basis(direction)

    all_points = np.vstack([p for _, p, _, _ in meshes])
    projected = np.stack(
        [all_points @ right, all_points @ up, all_points @ forward], axis=1
    )
    lo = projected[:, :2].min(axis=0)
    hi = projected[:, :2].max(axis=0)
    span = np.maximum(hi - lo, 1e-6)
    scale = 0.88 * min(WIDTH / span[0], HEIGHT / span[1])
    centre = (lo + hi) / 2

    colour = np.zeros((HEIGHT, WIDTH, 3), dtype=np.float64)
    colour[:] = BACKGROUND
    depth = np.full((HEIGHT, WIDTH), -np.inf)

    for _, points, faces, base_colour in meshes:
        cam = np.stack([points @ right, points @ up, points @ forward], axis=1)
        screen_x = (cam[:, 0] - centre[0]) * scale + WIDTH / 2
        screen_y = HEIGHT / 2 - (cam[:, 1] - centre[1]) * scale   # Bild-Y zeigt nach unten
        screen = np.stack([screen_x, screen_y, cam[:, 2]], axis=1)

        tri = screen[faces]                                  # (n, 3, 3)
        edge1 = points[faces[:, 1]] - points[faces[:, 0]]
        edge2 = points[faces[:, 2]] - points[faces[:, 0]]
        normals = np.cross(edge1, edge2)
        lengths = np.linalg.norm(normals, axis=1)
        valid = lengths > 1e-12
        normals[valid] /= lengths[valid, None]

        # beidseitig beleuchten -- so bleibt die Schattierung unabhaengig davon,
        # wie die Tesselierung die Dreiecke orientiert hat
        lambert = np.abs(normals @ LIGHT_DIR)
        shade = AMBIENT + (1.0 - AMBIENT) * lambert
        facet_colour = np.clip(base_colour[None, :] * shade[:, None], 0, 255)

        for index in range(len(faces)):
            if not valid[index]:
                continue
            _fill_triangle(colour, depth, tri[index], facet_colour[index])

    return np.clip(colour, 0, 255).astype(np.uint8)


def _fill_triangle(colour, depth, tri, rgb):
    """Ein Dreieck mit baryzentrischem Z-Test in den Puffer zeichnen."""
    min_x = max(int(np.floor(tri[:, 0].min())), 0)
    max_x = min(int(np.ceil(tri[:, 0].max())), WIDTH - 1)
    min_y = max(int(np.floor(tri[:, 1].min())), 0)
    max_y = min(int(np.ceil(tri[:, 1].max())), HEIGHT - 1)
    if min_x > max_x or min_y > max_y:
        return

    (x0, y0, z0), (x1, y1, z1), (x2, y2, z2) = tri
    area = (x1 - x0) * (y2 - y0) - (x2 - x0) * (y1 - y0)
    if abs(area) < 1e-9:
        return

    ys, xs = np.mgrid[min_y : max_y + 1, min_x : max_x + 1]
    px = xs + 0.5
    py = ys + 0.5

    w0 = ((x1 - px) * (y2 - py) - (x2 - px) * (y1 - py)) / area
    w1 = ((x2 - px) * (y0 - py) - (x0 - px) * (y2 - py)) / area
    w2 = 1.0 - w0 - w1
    inside = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
    if not inside.any():
        return

    z = w0 * z0 + w1 * z1 + w2 * z2
    window = depth[min_y : max_y + 1, min_x : max_x + 1]
    nearer = inside & (z > window)
    if not nearer.any():
        return

    window[nearer] = z[nearer]
    colour[min_y : max_y + 1, min_x : max_x + 1][nearer] = rgb


def render(parts=None, views=None, with_reference=True):
    parts = m.build_all() if parts is None else parts
    views = VIEWS if views is None else views
    EXPORT_DIR.mkdir(exist_ok=True)

    if with_reference:
        parts = dict(parts, **m.reference_solids())
    meshes = _tessellate(parts)
    triangles = sum(len(f) for _, _, f, _ in meshes)
    print(f"{len(meshes)} Teile, {triangles} Dreiecke")

    for name, direction in views.items():
        image = _rasterize(meshes, direction)
        path = EXPORT_DIR / f"ansicht_{name}.png"
        _write_png(path, image)
        print(f"  {name:8s} -> {path.name}")


if __name__ == "__main__":
    render()
