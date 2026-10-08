"""Server-side SVG charts in the Rim & Ridge palette -- no JS library, no build.

Every chart is a responsive <svg> (viewBox, width 100 %). Hover/touch details come
from ``data-tip`` attributes on invisible hit areas, read by the few lines of
script in webui.SCRIPT; line charts also get a crosshair (``data-x``).

Colours: the RR tokens (doc/design/rim-ridge-design-system.md, §1). They are
muted on purpose; brass against zone blue stays apart for colour-blind readers
too, every chart with two series has a legend, and the zone colours (fixed by
the design system, close for Z1/Z2) are separated by gaps and named in the legend
and the tooltips.
"""

from __future__ import annotations

from html import escape

BG = "#161B1F"
PANEL = "#1E252B"
RIM = "#3A362E"
TRACK = "#332F28"
BRASS = "#CBA36B"
PARCH_BRIGHT = "#F3ECDF"
PARCH = "#E7E2D6"
MUTED = "#9BA097"
SAGE = "#7FA08F"
ZONES = ("#6C90B0", "#6FA98C", "#D7B463", "#CE8A4C", "#C1604A")
#: diverging pair for form (TSB): fresh / tired
FRESH, TIRED = ZONES[0], ZONES[3]

W, H = 720, 220
PAD_L, PAD_R, PAD_T, PAD_B = 44, 16, 14, 26


def _nice_max(value: float) -> float:
    if value <= 0:
        return 1.0
    for step in (1, 2, 2.5, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000, 2000, 2500, 5000, 10000):
        if value / step <= 5:
            return step * int(value / step + 0.999999)
    return value


def _ticks(lo: float, hi: float, n: int = 4) -> list[float]:
    span = hi - lo
    raw = span / n if span else 1
    for step in (1, 2, 2.5, 5, 10, 20, 25, 50, 100, 200, 250, 500, 1000):
        if step >= raw:
            break
    first = step * int(lo / step + (0 if lo <= 0 else 0.999999))
    out = []
    v = first
    while v <= hi + 1e-9:
        out.append(round(v, 6))
        v += step
    return out


def _fmt(v: float) -> str:
    return f"{v:.0f}" if abs(v) >= 10 or v == int(v) else f"{v:.1f}".replace(".", ",")


def _svg(body: str, label: str, h: int = H) -> str:
    return (f'<svg class="chart" viewBox="0 0 {W} {h}" role="img" aria-label="{escape(label)}">'
            f'{body}</svg>')


def _grid(y0: float, y1: float, ys, h: int = H, unit: str = "") -> str:
    out = []
    for v in ys:
        y = _y(v, y0, y1, h)
        out.append(f'<line x1="{PAD_L}" x2="{W - PAD_R}" y1="{y:.1f}" y2="{y:.1f}" stroke="{TRACK}" '
                   f'stroke-width="1" vector-effect="non-scaling-stroke"/>')
        out.append(f'<text x="{PAD_L - 6}" y="{y + 4:.1f}" text-anchor="end" class="ax">{_fmt(v)}{unit}</text>')
    return "".join(out)


def _y(v: float, lo: float, hi: float, h: int = H) -> float:
    return PAD_T + (1 - (v - lo) / (hi - lo if hi != lo else 1)) * (h - PAD_T - PAD_B)


def _x(i: float, n: int) -> float:
    return PAD_L + (i / max(1, n - 1)) * (W - PAD_L - PAD_R)


def _bar_path(x: float, y_top: float, y_base: float, w: float, r: float = 4) -> str:
    """Column with rounded data-end, square at the baseline."""
    hgt = y_base - y_top
    if hgt <= 0:
        return ""
    r = min(r, w / 2, hgt)
    return (f"M{x:.1f},{y_base:.1f}V{y_top + r:.1f}Q{x:.1f},{y_top:.1f} {x + r:.1f},{y_top:.1f}"
            f"H{x + w - r:.1f}Q{x + w:.1f},{y_top:.1f} {x + w:.1f},{y_top + r:.1f}V{y_base:.1f}Z")


def line_chart(labels: list[str], series: list[tuple[str, str, list[float]]],
               tips: list[str], label: str, x_ticks: dict[int, str] | None = None) -> str:
    """Several series on one y-scale (same unit!). series = (name, colour, values)."""
    n = len(labels)
    if not n:
        return ""
    hi = _nice_max(max((max(v) for _, _, v in series if v), default=1))
    body = [_grid(0, hi, _ticks(0, hi))]
    for i, text in (x_ticks or {}).items():
        body.append(f'<text x="{_x(i, n):.1f}" y="{H - 8}" text-anchor="middle" class="ax">{escape(text)}</text>')
    ends = []
    for name, colour, values in series:
        pts = " ".join(f"{_x(i, n):.1f},{_y(v, 0, hi):.1f}" for i, v in enumerate(values))
        body.append(f'<polyline points="{pts}" fill="none" stroke="{colour}" stroke-width="2" '
                    'stroke-linejoin="round" stroke-linecap="round" vector-effect="non-scaling-stroke"/>')
        ends.append((name, colour, values[-1]))
    for name, colour, v in ends:
        body.append(f'<circle cx="{_x(n - 1, n):.1f}" cy="{_y(v, 0, hi):.1f}" r="4" fill="{colour}" '
                    f'stroke="{BG}" stroke-width="2"/>')
    body.append(f'<line class="xh" x1="0" x2="0" y1="{PAD_T}" y2="{H - PAD_B}" stroke="{MUTED}" '
                'stroke-width="1" vector-effect="non-scaling-stroke" visibility="hidden"/>')
    step = (W - PAD_L - PAD_R) / max(1, n - 1)
    for i in range(n):
        x = _x(i, n)
        body.append(f'<rect class="hit" x="{x - step / 2:.1f}" y="{PAD_T}" width="{step:.1f}" '
                    f'height="{H - PAD_T - PAD_B}" fill="transparent" data-x="{x:.1f}" '
                    f'data-tip="{escape(tips[i])}"/>')
    return _svg("".join(body), label)


def diverging_bars(values: list[float], tips: list[str], label: str,
                   x_ticks: dict[int, str] | None = None, h: int = 140) -> str:
    """One bar per value around zero: positive FRESH, negative TIRED."""
    n = len(values)
    if not n:
        return ""
    m = _nice_max(max((abs(v) for v in values), default=1)) or 1
    body = [_grid(-m, m, [-m, 0, m], h)]
    slot = (W - PAD_L - PAD_R) / n
    bw = max(1.0, min(24.0, slot - 1))
    y0 = _y(0, -m, m, h)
    for i, v in enumerate(values):
        x = PAD_L + i * slot + (slot - bw) / 2
        y = _y(v, -m, m, h)
        top, bottom = (y, y0) if v >= 0 else (y0, y)
        if bottom - top >= 0.5:
            body.append(f'<rect x="{x:.1f}" y="{top:.1f}" width="{bw:.1f}" height="{bottom - top:.1f}" '
                        f'fill="{FRESH if v >= 0 else TIRED}"/>')
        body.append(f'<rect class="hit" x="{PAD_L + i * slot:.1f}" y="{PAD_T}" width="{slot:.1f}" '
                    f'height="{h - PAD_T - PAD_B}" fill="transparent" data-tip="{escape(tips[i])}"/>')
    for i, text in (x_ticks or {}).items():
        body.append(f'<text x="{PAD_L + (i + 0.5) * slot:.1f}" y="{h - 8}" text-anchor="middle" '
                    f'class="ax">{escape(text)}</text>')
    return _svg("".join(body), label, h)


def columns(labels: list[str], values: list[float], tips: list[str], label: str,
            colour: str = BRASS, unit: str = "", mark: set[int] | None = None) -> str:
    """Single-series columns; values on the caps only for the indices in ``mark``."""
    n = len(values)
    if not n:
        return ""
    hi = _nice_max(max(values, default=0) * 1.12)
    body = [_grid(0, hi, _ticks(0, hi))]
    slot = (W - PAD_L - PAD_R) / n
    bw = min(24.0, slot * 0.6)
    base = _y(0, 0, hi)
    for i, v in enumerate(values):
        x = PAD_L + i * slot + (slot - bw) / 2
        top = _y(v, 0, hi)
        if v > 0:
            body.append(f'<path d="{_bar_path(x, top, base, bw)}" fill="{colour}"/>')
        if mark and i in mark and v > 0:
            body.append(f'<text x="{x + bw / 2:.1f}" y="{top - 5:.1f}" text-anchor="middle" '
                        f'class="val">{_fmt(v)}{unit}</text>')
        body.append(f'<text x="{x + bw / 2:.1f}" y="{H - 8}" text-anchor="middle" class="ax">'
                    f'{escape(labels[i])}</text>')
        body.append(f'<rect class="hit" x="{PAD_L + i * slot:.1f}" y="{PAD_T}" width="{slot:.1f}" '
                    f'height="{H - PAD_T - PAD_B}" fill="transparent" data-tip="{escape(tips[i])}"/>')
    return _svg("".join(body), label)


def stacked_zones(labels: list[str], zones: list[list[float]], tips: list[str], label: str,
                  scale: float = 3600, unit: str = " h") -> str:
    """Columns stacked by heart-rate zone (Z1 at the bottom), 2 px surface gaps."""
    n = len(zones)
    if not n:
        return ""
    totals = [sum(z) / scale for z in zones]
    hi = _nice_max(max(totals, default=0) or 1)
    body = [_grid(0, hi, _ticks(0, hi), unit="")]
    slot = (W - PAD_L - PAD_R) / n
    bw = min(24.0, slot * 0.6)
    base = _y(0, 0, hi)
    for i, zs in enumerate(zones):
        x = PAD_L + i * slot + (slot - bw) / 2
        y = base
        filled = [(k, v) for k, v in enumerate(zs) if v > 0]
        for j, (k, v) in enumerate(filled):
            hgt = (v / scale) / hi * (H - PAD_T - PAD_B)
            top = y - hgt
            gap = 2 if j else 0
            if j == len(filled) - 1:
                body.append(f'<path d="{_bar_path(x, top, y - gap, bw)}" fill="{ZONES[k]}"/>')
            elif hgt - gap > 0.3:
                body.append(f'<rect x="{x:.1f}" y="{top:.1f}" width="{bw:.1f}" height="{hgt - gap:.1f}" '
                            f'fill="{ZONES[k]}"/>')
            y = top
        body.append(f'<text x="{x + bw / 2:.1f}" y="{H - 8}" text-anchor="middle" class="ax">'
                    f'{escape(labels[i])}</text>')
        body.append(f'<rect class="hit" x="{PAD_L + i * slot:.1f}" y="{PAD_T}" width="{slot:.1f}" '
                    f'height="{H - PAD_T - PAD_B}" fill="transparent" data-tip="{escape(tips[i])}"/>')
    return _svg("".join(body), label)


def profile(points: list[list[float]], label: str, climbs: list[dict] | None = None,
            h: int = 200) -> str:
    """Elevation over distance: sage line with a 10 % wash, climbs as brass bands."""
    if len(points) < 2:
        return ""
    kms = [p[0] for p in points]
    hs = [p[1] for p in points]
    lo, hi = min(hs), max(hs)
    span = max(20.0, hi - lo)
    lo, hi = lo - span * 0.08, hi + span * 0.12
    ys = _ticks(lo, hi, 3)
    lo, hi = min(lo, ys[0]), max(hi, ys[-1])
    km_max = kms[-1] or 1

    def xk(km: float) -> float:
        return PAD_L + km / km_max * (W - PAD_L - PAD_R)

    body = [_grid(lo, hi, ys, h, " m")]
    for c in climbs or []:
        x1 = xk(c["start_km"])
        x2 = xk(c["start_km"] + c["length_m"] / 1000)
        body.append(f'<rect x="{x1:.1f}" y="{PAD_T}" width="{max(1.0, x2 - x1):.1f}" '
                    f'height="{h - PAD_T - PAD_B}" fill="{BRASS}" fill-opacity="0.14"/>')
        if c.get("category"):
            body.append(f'<text x="{(x1 + x2) / 2:.1f}" y="{PAD_T + 11}" text-anchor="middle" '
                        f'class="val">{escape(c["category"])}</text>')
    pts = " ".join(f"{xk(k):.1f},{_y(v, lo, hi, h):.1f}" for k, v in points)
    base = _y(lo, lo, hi, h)
    body.append(f'<polygon points="{xk(kms[0]):.1f},{base:.1f} {pts} {xk(kms[-1]):.1f},{base:.1f}" '
                f'fill="{SAGE}" fill-opacity="0.10"/>')
    body.append(f'<polyline points="{pts}" fill="none" stroke="{SAGE}" stroke-width="2" '
                'stroke-linejoin="round" vector-effect="non-scaling-stroke"/>')
    for t in _ticks(0, km_max, 6):
        body.append(f'<text x="{xk(t):.1f}" y="{h - 8}" text-anchor="middle" class="ax">{_fmt(t)} km</text>')
    body.append(f'<line class="xh" x1="0" x2="0" y1="{PAD_T}" y2="{h - PAD_B}" stroke="{MUTED}" '
                'stroke-width="1" vector-effect="non-scaling-stroke" visibility="hidden"/>')
    n = len(points)
    step = (W - PAD_L - PAD_R) / max(1, n - 1)
    for k, v in points:
        x = xk(k)
        body.append(f'<rect class="hit" x="{x - step / 2:.1f}" y="{PAD_T}" width="{step:.1f}" '
                    f'height="{h - PAD_T - PAD_B}" fill="transparent" data-x="{x:.1f}" '
                    f'data-tip="km {_fmt(k)} · {v:.0f} m"/>')
    return _svg("".join(body), label, h)


def zone_band(zones_s: dict) -> str:
    """The RR heart-rate zone band as a proportional bar (HTML, with labels)."""
    total = sum(zones_s.values()) or 1
    cells = []
    for i, (name, s) in enumerate(zones_s.items()):
        share = s / total
        if share <= 0:
            continue
        cells.append(f'<span style="flex:{share:.4f};background:{ZONES[i]}" '
                     f'data-tip="{escape(name)}: {share * 100:.0f} % ({int(s // 60)} min)"></span>')
    legend = "".join(f'<span><i style="background:{ZONES[i]}"></i>{escape(name)} '
                     f'{s / total * 100:.0f} %</span>' for i, (name, s) in enumerate(zones_s.items()))
    return f'<div class="zband">{"".join(cells)}</div><div class="legend">{legend}</div>'


def legend(items: list[tuple[str, str]]) -> str:
    return '<div class="legend">' + "".join(
        f'<span><i style="background:{c}"></i>{escape(n)}</span>' for n, c in items) + "</div>"
