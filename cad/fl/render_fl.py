"""Bilder des FL-Gehaeuses, mit dem Rasterizer aus cad/render.py.

Ausfuehren:  cad/.venv/bin/python cad/fl/render_fl.py
Ergebnis:    cad/fl/export/fl_<ansicht>.png (Baugruppe am Rad) und
             cad/fl/export/teil_<teil>.png (jedes Druckteil allein)
"""

import sys
from pathlib import Path

HERE = Path(__file__).parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))

import model_fl as m  # noqa: E402
import render as r    # noqa: E402

COLORS = {
    "fl_traeger": (118, 152, 198),
    "fl_ring": (214, 158, 92),
    "fl_deckel": (206, 110, 104),
    "fl_klemme_links": (124, 180, 134),
    "fl_klemme_rechts": (124, 180, 134),
    "ref_modul": (52, 60, 78),
    "ref_esp32": (40, 120, 60),
    "ref_schalter": (200, 60, 60),
    "ref_usbc": (90, 90, 200),
    "ref_kabel": (40, 40, 40),
    "ref_verschraubung": (60, 60, 60),
    "ref_wago": (230, 140, 40),
    "ref_winkelstecker": (90, 90, 200),
}
for name in m.FRAME_REFS:
    COLORS.setdefault(name, (92, 92, 96))

VIEWS = {
    "iso": (1.0, 1.0, 0.7),
    "seite": (0.0, 1.0, 0.0),
    "oben": (0.0, 0.0, 1.0),
    "hinten": (-1.0, 0.0, 0.35),
    "hinten_unten": (-1.0, 0.8, -0.7),
    "vorne_unten": (1.0, -0.6, -0.6),
}


def _draw(parts, views, prefix):
    r.PART_COLORS.update(COLORS)
    meshes = r._tessellate(parts)
    for view, direction in views.items():
        path = m.EXPORT_DIR / f"{prefix}_{view}.png"
        r._write_png(path, r._rasterize(meshes, direction))
        print("  ", path.name)


def main(only=None):
    m.EXPORT_DIR.mkdir(exist_ok=True)
    parts = m.build_all()
    refs = m.reference_solids()
    # Lenker nur kurz zeichnen, sonst wird das Gehaeuse winzig
    refs["ref_lenker"] = refs["ref_lenker"] & m._box(-40, 40, -70, 70, -40, 40)
    refs["ref_gabelschaft"] = refs["ref_gabelschaft"] & m._box(-200, 200, -50, 50, -60, 80)
    world = {k: m.to_world(v) for k, v in {**parts, **refs}.items()}

    if only in (None, "baugruppe"):
        _draw(world, VIEWS, "fl")
        # ohne Deckel und Ring: Blick ins Fach
        inner = {k: v for k, v in world.items() if k not in ("fl_deckel", "fl_ring", "ref_modul")}
        _draw(inner, {"fach_iso": (0.6, 0.8, 1.0)}, "fl")
        # Ring ohne Deckel und Modul
        ring = {k: v for k, v in world.items() if k not in ("fl_deckel", "ref_modul")}
        _draw(ring, {"ring_iso": (-0.7, 0.8, 1.0)}, "fl")
    if only in (None, "teile"):
        for name, part in parts.items():
            if name == "fl_klemme_rechts":
                continue
            _draw({name: part}, {"oben": (0.5, 0.6, 1.0), "unten": (0.5, -0.6, -1.0)}, f"teil_{name}")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else None)
