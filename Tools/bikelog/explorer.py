"""Where have I been: visited map tiles and a heatmap of all rides, drawn as overlay tiles.

Pure part of the map in the log service (bikelogservice/maptracks.py): projection, which
tiles a track touches, the figures explorers compare (cluster, max cluster, max square),
and the 256 px overlay tiles a slippy map (BRouter-Web, the service's /map page) lays
over its base map. No Pillow, no numpy: the tiles are palette PNGs written with zlib.

Visited tiles are the OSM tiles of one zoom level a track passes through -- zoom 14
("explorer tiles", about 1.6 km at 50 N, as VeloViewer and StatsHunters count them) and
zoom 17 ("squadrats", about 200 m). A tile counts when a segment between two track
points crosses it, not only when a point lies in it; a segment longer than MAX_GAP_M is
a gap in the recording (tunnel, train, GPS lost) and fills nothing.

  cluster tile   visited, and so are its four neighbours
  max cluster    the largest 4-connected group of cluster tiles
  max square     the largest square of visited tiles

At a coarser zoom a tile is visited if one of its zoom 17 tiles is (parent_tiles()): the
layer whose grid is always the map tile on the screen needs nothing else.

The heatmap counts tracks per pixel (a track that passes a pixel twice counts once), so
a road ridden 30 times glows and a one-off stays dim. Points are in Web Mercator,
normalised to 0..1 ("world units"); a tile at zoom z is 1/2**z of that.

What keeps the heatmap fast, after the camper's (ia216onTour, heat_tiles.py: the network
prepared per zoom, and an index of the tiles that have anything at all): HeatIndex lists
per zoom the tiles with the pieces of track in them, so drawing a tile looks at those only
and an empty one at nothing. Unlike there no track is merged into another's line: the
pixels are exactly the rides' own. HeatIndex.key() names what a tile shows, so a drawn
tile stays valid until a ride in that tile changes.
"""

from __future__ import annotations

import hashlib
import math
import struct
import threading
import zlib
from array import array
from bisect import bisect_right
from collections import Counter
from dataclasses import dataclass, field

TILE_PX = 256
EXPLORER_ZOOM = 14
SQUADRAT_ZOOM = 17
#: a longer segment is a gap in the recording, not a way ridden
MAX_GAP_M = 2000.0
#: points closer than this to the previous one are dropped when a track is stored
THIN_M = 5.0
#: points per chunk; a chunk's bounding box lets a tile skip most of a long track
CHUNK = 24
#: precomputed coarser versions of each track, for drawing at these zooms and below
LEVELS = (7, 10, 13)
MAX_LAT = 85.05112878
EARTH_CIRCUMFERENCE_M = 40075016.686


def mercator(lat: float, lon: float) -> tuple[float, float]:
    lat = max(-MAX_LAT, min(MAX_LAT, lat))
    s = math.sin(math.radians(lat))
    return (lon + 180.0) / 360.0, 0.5 - math.log((1 + s) / (1 - s)) / (4 * math.pi)


def unmercator(x: float, y: float) -> tuple[float, float]:
    lon = x * 360.0 - 180.0
    lat = math.degrees(math.atan(math.sinh(math.pi * (1 - 2 * y))))
    return lat, lon


def metres_per_unit(y: float) -> float:
    """Ground metres per world unit at world y (Mercator stretches towards the poles)."""
    lat, _ = unmercator(0.0, y)
    return EARTH_CIRCUMFERENCE_M * math.cos(math.radians(lat))


def thin(points: list[tuple[float, float]], min_m: float = THIN_M) -> list[tuple[float, float]]:
    """(lat, lon) points without the ones closer than min_m to the last one kept (the last
    point always stays). Equirectangular distance: exact enough at a few metres."""
    if len(points) < 3:
        return list(points)
    out = [points[0]]
    k = 111320.0
    for lat, lon in points[1:-1]:
        plat, plon = out[-1]
        dy = (lat - plat) * k
        dx = (lon - plon) * k * math.cos(math.radians(lat))
        if dx * dx + dy * dy >= min_m * min_m:
            out.append((lat, lon))
    out.append(points[-1])
    return out


# --- tracks ---------------------------------------------------------------------------------

@dataclass
class Chunk:
    """Up to CHUNK consecutive points of one run; neighbouring chunks share their end point,
    so no segment is lost between them."""
    minx: float
    miny: float
    maxx: float
    maxy: float
    xs: array
    ys: array


@dataclass
class Track:
    """One track in world units, split at gaps into runs, ready for drawing."""
    id: str
    runs: list[tuple[array, array]]
    bbox: tuple[float, float, float, float]
    #: zoom level (from LEVELS, or 99 = full detail) -> chunks
    levels: dict[int, list[Chunk]] = field(default_factory=dict)
    #: hash of the runs: the same digest draws the same pixels
    digest: str = ""

    def chunks(self, zoom: int) -> list[Chunk]:
        for level in LEVELS:
            if zoom <= level:
                return self.levels[level]
        return self.levels[99]


def runs_of(points: list[tuple[float, float]], max_gap_m: float = MAX_GAP_M) -> list[tuple[array, array]]:
    """(lat, lon) -> runs of world coordinates, split where two points are further apart than
    max_gap_m. A run of a single point is kept (it still marks its tile)."""
    runs: list[tuple[array, array]] = []
    xs, ys = array("d"), array("d")
    prev = None
    k = 111320.0
    for lat, lon in points:
        if prev is not None:
            dy = (lat - prev[0]) * k
            dx = (lon - prev[1]) * k * math.cos(math.radians(lat))
            if dx * dx + dy * dy > max_gap_m * max_gap_m:
                runs.append((xs, ys))
                xs, ys = array("d"), array("d")
        x, y = mercator(lat, lon)
        xs.append(x)
        ys.append(y)
        prev = (lat, lon)
    if len(xs):
        runs.append((xs, ys))
    return runs


def _simplify(xs: array, ys: array, tol: float) -> tuple[array, array]:
    """Drop points closer than tol (world units) to the last one kept."""
    if len(xs) < 3 or tol <= 0:
        return xs, ys
    ox, oy = array("d", [xs[0]]), array("d", [ys[0]])
    t2 = tol * tol
    for i in range(1, len(xs) - 1):
        dx, dy = xs[i] - ox[-1], ys[i] - oy[-1]
        if dx * dx + dy * dy >= t2:
            ox.append(xs[i])
            oy.append(ys[i])
    ox.append(xs[-1])
    oy.append(ys[-1])
    return ox, oy


def _chunks(runs: list[tuple[array, array]]) -> list[Chunk]:
    out = []
    for xs, ys in runs:
        n = len(xs)
        start = 0
        while True:
            end = min(n, start + CHUNK)
            cx, cy = xs[start:end], ys[start:end]
            out.append(Chunk(min(cx), min(cy), max(cx), max(cy), cx, cy))
            if end >= n:
                break
            start = end - 1
    return out


def make_track(track_id: str, points: list[tuple[float, float]]) -> Track | None:
    """A drawable track from (lat, lon) points, None if there are none."""
    runs = runs_of(points)
    if not runs:
        return None
    minx = min(min(xs) for xs, _ in runs)
    maxx = max(max(xs) for xs, _ in runs)
    miny = min(min(ys) for _, ys in runs)
    maxy = max(max(ys) for _, ys in runs)
    track = Track(track_id, runs, (minx, miny, maxx, maxy))
    track.levels[99] = _chunks(runs)
    h = hashlib.sha1()
    for xs, ys in runs:
        h.update(struct.pack("<I", len(xs)) + xs.tobytes() + ys.tobytes())
    track.digest = h.hexdigest()[:16]
    for level in LEVELS:
        # a bit under one pixel at the coarsest zoom this level is drawn at
        tol = 0.7 / (TILE_PX * 2 ** level)
        track.levels[level] = _chunks([_simplify(xs, ys, tol) for xs, ys in runs])
    return track


# --- visited tiles ---------------------------------------------------------------------------

def _cells(x0: float, y0: float, x1: float, y1: float, out: set) -> None:
    """Every grid cell (in tile units) the segment crosses (Amanatides & Woo)."""
    cx, cy = int(x0), int(y0)
    ex, ey = int(x1), int(y1)
    out.add((cx, cy))
    if (cx, cy) == (ex, ey):
        return
    dx, dy = x1 - x0, y1 - y0
    sx = 1 if dx > 0 else -1
    sy = 1 if dy > 0 else -1
    tdx = abs(1.0 / dx) if dx else math.inf
    tdy = abs(1.0 / dy) if dy else math.inf
    tmx = ((cx + 1 - x0) if dx > 0 else (x0 - cx)) * tdx if dx else math.inf
    tmy = ((cy + 1 - y0) if dy > 0 else (y0 - cy)) * tdy if dy else math.inf
    for _ in range(abs(ex - cx) + abs(ey - cy)):
        if tmx < tmy:
            cx += sx
            tmx += tdx
        else:
            cy += sy
            tmy += tdy
        out.add((cx, cy))


def tiles_of(runs: list[tuple[array, array]], zoom: int) -> set[tuple[int, int]]:
    """The (x, y) tiles at ``zoom`` the runs pass through."""
    n = 2 ** zoom
    out: set[tuple[int, int]] = set()
    for xs, ys in runs:
        px, py = xs[0] * n, ys[0] * n
        out.add((int(px), int(py)))
        for i in range(1, len(xs)):
            qx, qy = xs[i] * n, ys[i] * n
            if int(px) != int(qx) or int(py) != int(qy):
                _cells(px, py, qx, qy, out)
            px, py = qx, qy
    return out


def tile_lengths(runs: list[tuple[array, array]], zoom: int) -> dict[tuple[int, int], float]:
    """The tiles of tiles_of() at ``zoom``, each with the metres of line inside it: a way there
    and back counts twice, the parts of a segment go to the tiles it crosses."""
    n = 2 ** zoom
    per_tile = EARTH_CIRCUMFERENCE_M / n        # at the equator
    out: dict[tuple[int, int], float] = {}
    for xs, ys in runs:
        px, py = xs[0] * n, ys[0] * n
        out.setdefault((int(px), int(py)), 0.0)
        for i in range(1, len(xs)):
            qx, qy = xs[i] * n, ys[i] * n
            dx, dy = qx - px, qy - py
            # per segment: a tour may go from 50 N to 41 N
            metres = math.hypot(dx, dy) * per_tile / math.cosh(math.pi * (1 - 2 * ys[i]))
            cx, cy = int(px), int(py)
            ex, ey = int(qx), int(qy)
            done = 0.0                          # share of the segment given away
            if cx != ex or cy != ey:            # as _cells()
                sx = 1 if dx > 0 else -1
                sy = 1 if dy > 0 else -1
                tdx = abs(1.0 / dx) if dx else math.inf
                tdy = abs(1.0 / dy) if dy else math.inf
                tmx = ((cx + 1 - px) if dx > 0 else (px - cx)) * tdx if dx else math.inf
                tmy = ((cy + 1 - py) if dy > 0 else (py - cy)) * tdy if dy else math.inf
                for _ in range(abs(ex - cx) + abs(ey - cy)):
                    upto = max(done, min(tmx, tmy, 1.0))
                    out[cx, cy] = out.get((cx, cy), 0.0) + metres * (upto - done)
                    done = upto
                    if tmx < tmy:
                        cx += sx
                        tmx += tdx
                    else:
                        cy += sy
                        tmy += tdy
            out[cx, cy] = out.get((cx, cy), 0.0) + metres * (1.0 - done)
            px, py = qx, qy
    return out


def parent_tiles(tiles: set[tuple[int, int]], levels: int) -> set[tuple[int, int]]:
    """The tiles ``levels`` zooms further out that contain one of tiles."""
    return {(x >> levels, y >> levels) for x, y in tiles}


def cluster(tiles: set[tuple[int, int]]) -> set[tuple[int, int]]:
    """Visited tiles whose four neighbours are visited too."""
    return {(x, y) for x, y in tiles
            if (x - 1, y) in tiles and (x + 1, y) in tiles and (x, y - 1) in tiles and (x, y + 1) in tiles}


def max_cluster(tiles: set[tuple[int, int]]) -> set[tuple[int, int]]:
    """The largest 4-connected group of cluster tiles, of equally large ones the one furthest west."""
    todo = set(cluster(tiles))
    best: set[tuple[int, int]] = set()
    while todo:
        seed = todo.pop()
        group = {seed}
        stack = [seed]
        while stack:
            x, y = stack.pop()
            for nb in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if nb in todo:
                    todo.discard(nb)
                    group.add(nb)
                    stack.append(nb)
        if len(group) > len(best) or (len(group) == len(best) and min(group) < min(best)):
            best = group
    return best


def max_square(tiles: set[tuple[int, int]]) -> tuple[int, int, int]:
    """(x, y, size) of the largest square of visited tiles, (x, y) its top left tile;
    (0, 0, 0) without tiles. Of several equally large ones the first in row order."""
    size: dict[tuple[int, int], int] = {}
    best = (0, 0, 0)
    for x, y in sorted(tiles, key=lambda t: (t[1], t[0])):
        s = 1 + min(size.get((x - 1, y), 0), size.get((x, y - 1), 0), size.get((x - 1, y - 1), 0))
        size[(x, y)] = s
        if s > best[2]:
            best = (x - s + 1, y - s + 1, s)
    return best


@dataclass
class TileStats:
    zoom: int
    tiles: set[tuple[int, int]]
    cluster: set[tuple[int, int]]
    max_cluster: set[tuple[int, int]]
    square: tuple[int, int, int]
    #: tiles grouped by (x >> 5, y >> 5), so a map tile looks only at the tiles near it
    buckets: dict[tuple[int, int], list[tuple[int, int]]] = field(default_factory=dict)

    def as_dict(self) -> dict:
        x, y, s = self.square
        out = {"zoom": self.zoom, "tiles": len(self.tiles), "cluster": len(self.cluster),
               "max_cluster": len(self.max_cluster), "max_square": s}
        if s:
            n = 2 ** self.zoom
            north, west = unmercator(x / n, y / n)
            south, east = unmercator((x + s) / n, (y + s) / n)
            out["max_square_bounds"] = [round(south, 6), round(west, 6), round(north, 6), round(east, 6)]
        return out


def tile_stats(tiles: set[tuple[int, int]], zoom: int) -> TileStats:
    stats = TileStats(zoom, tiles, cluster(tiles), max_cluster(tiles), max_square(tiles))
    for t in tiles:
        stats.buckets.setdefault((t[0] >> 5, t[1] >> 5), []).append(t)
    return stats


# --- PNG ------------------------------------------------------------------------------------

def _chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)


def png(pixels: bytearray, palette: list[tuple[int, int, int, int]], size: int = TILE_PX) -> bytes:
    """Palette PNG (8 bit, alpha per palette entry) of size x size palette indices."""
    raw = bytearray()
    for row in range(size):
        raw.append(0)
        raw += pixels[row * size:(row + 1) * size]
    return (b"\x89PNG\r\n\x1a\n"
            + _chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 3, 0, 0, 0))
            + _chunk(b"PLTE", b"".join(bytes(c[:3]) for c in palette))
            + _chunk(b"tRNS", bytes(c[3] for c in palette))
            + _chunk(b"IDAT", zlib.compress(bytes(raw), 6))
            + _chunk(b"IEND", b""))


#: what an empty tile is: fully transparent
EMPTY_PNG = png(bytearray(TILE_PX * TILE_PX), [(0, 0, 0, 0)])


# --- overlay: visited tiles ------------------------------------------------------------------

VISITED_PALETTE = [
    (0, 0, 0, 0),           # 0 nothing
    (214, 64, 40, 84),      # 1 visited
    (205, 120, 250, 120),   # 2 cluster: light ...
    (40, 0, 105, 150),      # 3 max cluster: ... and dark violet, apart also at half the alpha
    (40, 40, 40, 110),      # 4 grid line
    (255, 204, 0, 255),     # 5 max square border
]
#: zoomed in past the tiles' own zoom the fill would tint the whole map: fainter there, but the
#: clusters less so (halved, cluster and max cluster could not be told apart)
VISITED_PALETTE_CLOSE = [(*c[:3], {1: 42, 2: 80, 3: 112}.get(i, c[3])) for i, c in enumerate(VISITED_PALETTE)]


#: the other colouring of the tiles, by the distance ridden in them: six steps of one hue, each
#: clearly apart in lightness: darker = more on a light map ...
DISTANCE_PALETTE = [(0, 0, 0, 0), (255, 165, 90, 130), (244, 120, 40, 150), (222, 84, 14, 172),
                    (186, 52, 0, 190), (140, 26, 0, 208), (92, 6, 0, 226), (40, 40, 40, 110)]
#: ... and lighter = more on a dark one (last entry of both: the grid)
DISTANCE_PALETTE_DARK = [(0, 0, 0, 0), (205, 80, 20, 110), (225, 92, 20, 148), (240, 108, 28, 185),
                         (250, 134, 48, 213), (255, 175, 95, 224), (255, 218, 165, 234), (200, 200, 200, 60)]
_ROUND_KM = tuple(m * 10.0 ** e if e >= 0 else m / 10.0 ** -e for e in range(-3, 6) for m in (1, 2, 5))


def km_steps(zoom: int) -> tuple[float, ...]:
    """The five limits (km) between the six steps at a tile zoom: a tile 200 km wide holds a
    thousand times what one of 200 m does. Round numbers from about one tile's width (one ride
    straight through; width at 50 N) up, each 2 to 2.5 times the one before. A fine tile is
    mostly crossed just once: there the first limit stays clear above a width, or a straight
    way would flicker between two steps with the latitude and the angle."""
    width = EARTH_CIRCUMFERENCE_M / 1000 * math.cos(math.radians(50)) / 2 ** zoom
    i = min(range(len(_ROUND_KM)), key=lambda k: abs(math.log(_ROUND_KM[k] / width)))
    if zoom >= EXPLORER_ZOOM and _ROUND_KM[i] < 1.2 * width:
        i += 1
    return _ROUND_KM[i:i + 5]


def tile_metres(rides, levels: int = 0) -> dict[tuple[int, int], float]:
    """{tile: metres ridden in it} from each ride's recordings (tile_lengths() each), ``levels``
    zooms further out. Of several recordings of one ride the longest in the tile counts."""
    total: dict[tuple[int, int], float] = {}
    for recordings in rides:
        longest: dict[tuple[int, int], float] = {}
        for lengths in recordings:
            here: dict[tuple[int, int], float] = {}
            for (x, y), metres in lengths.items():
                tile = x >> levels, y >> levels
                here[tile] = here.get(tile, 0.0) + metres
            for tile, metres in here.items():
                if metres >= longest.get(tile, 0.0):
                    longest[tile] = metres
        for tile, metres in longest.items():
            total[tile] = total.get(tile, 0.0) + metres
    return total


def _fill(px: bytearray, x0: int, y0: int, x1: int, y1: int, value: int) -> None:
    x0, y0 = max(0, x0), max(0, y0)
    x1, y1 = min(TILE_PX, x1), min(TILE_PX, y1)
    if x0 >= x1 or y0 >= y1:
        return
    row = bytes([value]) * (x1 - x0)
    for y in range(y0, y1):
        px[y * TILE_PX + x0:y * TILE_PX + x1] = row


def visited_tile(stats: TileStats, z: int, tx: int, ty: int, faint: bool = False,
                 metres: dict[tuple[int, int], float] | None = None, dark: bool = False) -> bytes | None:
    """The overlay tile (z, tx, ty) showing stats' tiles: visited, cluster and max cluster
    filled, the grid once a tile is 24 px wide, the max square outlined. None = empty.
    faint: the pale fill at every zoom (a layer that covers much of the screen at any zoom).
    metres (tile_metres()): coloured by the distance ridden instead, without cluster and max square;
    dark: for a dark map."""
    scale = TILE_PX * 2.0 ** (z - stats.zoom)          # pixels per visited tile
    ox, oy = tx * TILE_PX, ty * TILE_PX
    # the visited tiles under this map tile
    lo_x, lo_y = int(ox // scale), int(oy // scale)
    hi_x, hi_y = int((ox + TILE_PX - 1e-9) // scale), int((oy + TILE_PX - 1e-9) // scale)
    candidates = []
    for bx in range(lo_x >> 5, (hi_x >> 5) + 1):
        for by in range(lo_y >> 5, (hi_y >> 5) + 1):
            candidates.extend(stats.buckets.get((bx, by), ()))
    grid = scale >= 24
    sx, sy, ss = stats.square if metres is None else (0, 0, 0)
    line = 4 if metres is None else len(DISTANCE_PALETTE) - 1
    steps = () if metres is None else km_steps(stats.zoom)
    if not candidates and not grid and not ss:
        return None
    px = bytearray(TILE_PX * TILE_PX)
    drawn = False

    def box(x: int, y: int) -> tuple[int, int, int, int]:
        x0 = int(math.floor(x * scale - ox))
        y0 = int(math.floor(y * scale - oy))
        x1 = int(math.floor((x + 1) * scale - ox))
        y1 = int(math.floor((y + 1) * scale - oy))
        return x0, y0, max(x1, x0 + 1), max(y1, y0 + 1)

    shown = [(bisect_right(steps, metres.get(t, 0.0) / 1000) + 1 if metres is not None
              else 3 if t in stats.max_cluster else 2 if t in stats.cluster else 1, t)
             for t in candidates if lo_x <= t[0] <= hi_x and lo_y <= t[1] <= hi_y]
    if scale < 1:                   # tiles share pixels: the better one is drawn last
        shown.sort()
    for value, t in shown:
        _fill(px, *box(*t), value)
        drawn = True
    if grid:
        step = scale
        first = math.ceil(ox / step) * step
        x = first
        while x < ox + TILE_PX:
            _fill(px, int(x - ox), 0, int(x - ox) + 1, TILE_PX, line)
            x += step
        y = math.ceil(oy / step) * step
        while y < oy + TILE_PX:
            _fill(px, 0, int(y - oy), TILE_PX, int(y - oy) + 1, line)
            y += step
        drawn = True
    if ss:
        w = 3 if z >= stats.zoom - 4 else 2
        x0 = int(math.floor(sx * scale - ox))
        y0 = int(math.floor(sy * scale - oy))
        x1 = int(math.floor((sx + ss) * scale - ox))
        y1 = int(math.floor((sy + ss) * scale - oy))
        if x1 >= 0 and y1 >= 0 and x0 < TILE_PX and y0 < TILE_PX:
            _fill(px, x0, y0, x1, y0 + w, 5)
            _fill(px, x0, y1 - w, x1, y1, 5)
            _fill(px, x0, y0, x0 + w, y1, 5)
            _fill(px, x1 - w, y0, x1, y1, 5)
            drawn = True
    if not drawn:
        return None
    if metres is None:
        return png(px, VISITED_PALETTE_CLOSE if faint or z > stats.zoom else VISITED_PALETTE)
    palette = DISTANCE_PALETTE_DARK if dark else DISTANCE_PALETTE
    # the map's names must show through: thinner close in and in a layer that covers much
    share = 0.5 if z > stats.zoom and not faint else 0.65 if faint else 1.0
    return png(px, [(r, g, b, a if i == line else round(a * share)) for i, (r, g, b, a) in enumerate(palette)])


# --- overlay: heatmap -------------------------------------------------------------------------

#: count -> colour, the cyan of the camper's heatmap: light for a single track, deeper and
#: more opaque the more often a way was ridden (the base map is light, so more = darker)
HEAT_STOPS = ((1, (0, 200, 235, 170)), (3, (0, 160, 225, 205)), (8, (0, 105, 200, 235)),
              (20, (20, 45, 160, 255)))
HEAT_LEVELS = 20
#: palette index of the glow around the lines
HEAT_HALO = HEAT_LEVELS + 1
HALO_COLOUR = (0, 220, 255, 60)


def _heat_palette() -> list[tuple[int, int, int, int]]:
    pal = [(0, 0, 0, 0)]
    for n in range(1, HEAT_LEVELS + 1):
        for (n0, c0), (n1, c1) in zip(HEAT_STOPS, HEAT_STOPS[1:]):
            if n0 <= n <= n1:
                f = (math.log(n) - math.log(n0)) / (math.log(n1) - math.log(n0))
                pal.append(tuple(round(a + (b - a) * f) for a, b in zip(c0, c1)))
                break
    pal.append(HALO_COLOUR)
    return pal


HEAT_PALETTE = _heat_palette()


def _brush(z: int) -> tuple[tuple[int, int], ...]:
    if z < 11:
        return ((0, 0),)
    if z < 15:
        return ((0, 0), (1, 0), (0, 1), (1, 1))
    return ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1))


def _halo(brush) -> tuple[tuple[int, int], ...]:
    """The brush grown by two pixels: the glow around a line."""
    grown = {(x + dx, y + dy) for x, y in brush for dx in range(-2, 3) for dy in range(-2, 3)
             if abs(dx) + abs(dy) <= 3}
    return tuple(sorted(grown - set(brush)))


#: raise it when the pixels of a heat tile change: stored tiles are drawn again
HEAT_VERSION = 1
#: HeatIndex lists the tiles down to this zoom; closer in, the parent's list is filtered
INDEX_ZOOM = 15
#: a tile is drawn on a grid this much wider on every side, so a brush needs no clipping
PAD = 4
GRID = TILE_PX + 2 * PAD
#: the tile's own bytes of that grid, as an int: spreading a shape is a shift and an OR
_WINDOW = int.from_bytes(
    b"".join(bytes(PAD) + b"\xff" * TILE_PX + bytes(PAD) if PAD <= row < PAD + TILE_PX else bytes(GRID)
             for row in range(GRID)), "little")
_IS_SET = bytes(1 if i else 0 for i in range(256))


def _offsets(shape) -> tuple[int, ...]:
    return tuple(y * GRID + x for x, y in shape)


def _centres(chunk: Chunk, scale: float, ox: float, oy: float, out: set[int]) -> None:
    """The grid pixels the chunk's lines step on, as far as a brush there reaches the tile:
    a line is stepped a pixel at a time along its longer axis."""
    lo, hi = -2.0, TILE_PX + 2.0
    add = out.add
    floor = math.floor              # not int(x + PAD): that rounds up just below an edge
    xs, ys = chunk.xs, chunk.ys
    px, py = xs[0] * scale - ox, ys[0] * scale - oy
    if len(xs) == 1:
        if lo <= px < hi and lo <= py < hi:
            add((floor(py) + PAD) * GRID + floor(px) + PAD)
        return
    for qx, qy in zip(xs[1:], ys[1:]):
        qx = qx * scale - ox
        qy = qy * scale - oy
        dx, dy = qx - px, qy - py
        if -1 < dx < 1 and -1 < dy < 1:             # under a pixel: just both ends
            if lo <= px < hi and lo <= py < hi:
                add((floor(py) + PAD) * GRID + floor(px) + PAD)
            x, y = px + dx, py + dy
            if lo <= x < hi and lo <= y < hi:
                add((floor(y) + PAD) * GRID + floor(x) + PAD)
        elif not ((px < lo and qx < lo) or (px > hi and qx > hi)
                  or (py < lo and qy < lo) or (py > hi and qy > hi)):
            steps = int(max(abs(dx), abs(dy))) + 1
            dx /= steps
            dy /= steps
            x, y = px, py
            for _ in range(steps + 1):
                if lo <= x < hi and lo <= y < hi:
                    add((floor(y) + PAD) * GRID + floor(x) + PAD)
                x += dx
                y += dy
        px, py = qx, qy


def _draw_heat(pieces: list[tuple[str, Chunk]], z: int, tx: int, ty: int) -> bytes | None:
    """The heat tile from its (ride, chunk) pieces. None = empty."""
    scale = TILE_PX * 2 ** z
    ox, oy = tx * TILE_PX, ty * TILE_PX
    centres: dict[str, set[int]] = {}
    for ride, chunk in pieces:
        out = centres.get(ride)
        if out is None:
            out = centres[ride] = set()
        _centres(chunk, scale, ox, oy, out)
    brush = _brush(z)
    offsets = _offsets(brush)
    counts: Counter = Counter()
    for centre in centres.values():
        if len(offsets) == 1:
            counts.update(centre)
        elif centre:
            pix: set[int] = set()
            for o in offsets:
                pix.update(map(o.__add__, centre))      # per ride: a pixel counts once
            counts.update(pix)
    grid = bytearray(GRID * GRID)
    drawn = False
    end = PAD + TILE_PX
    for i, n in counts.items():
        if PAD <= i % GRID < end and PAD <= i // GRID < end:
            grid[i] = n if n < HEAT_LEVELS else HEAT_LEVELS
            drawn = True
    if not drawn:
        return None
    if z >= 9:
        core = int.from_bytes(grid.translate(_IS_SET), "little")
        glow = 0
        for o in _offsets(_halo(brush)):
            glow |= core << 8 * o if o >= 0 else core >> -8 * o
        glow &= _WINDOW & ~(core * 255)
        grid = (int.from_bytes(grid, "little") | glow * HEAT_HALO).to_bytes(len(grid), "little")
    px = b"".join(grid[r * GRID + PAD:r * GRID + end] for r in range(PAD, end))
    return png(px, HEAT_PALETTE)


class HeatIndex:
    """Which pieces of which tracks each heat tile shows, prepared per zoom on first use."""

    def __init__(self, tracks: list[Track], same_ride: dict[str, str] | None = None):
        same = same_ride or {}
        self.tracks = tracks
        self._rides = [same.get(t.id, t.id) for t in tracks]
        self._marks = [f"{t.digest}:{ride}".encode() for t, ride in zip(tracks, self._rides)]
        self._zooms: dict[int, dict[tuple[int, int], list[tuple[int, Chunk]]]] = {}
        self._building = threading.Lock()

    def tiles(self, z: int) -> dict[tuple[int, int], list[tuple[int, Chunk]]]:
        """{(x, y): [(track number, chunk)]} of every tile at z (up to INDEX_ZOOM) a piece
        of track reaches, two pixels around it included."""
        have = self._zooms.get(z)
        if have is None:
            with self._building:        # a view asks for 40 tiles at once: built for all
                have = self._zooms.get(z)
                if have is None:
                    have = self._zooms[z] = self._build(z)
        return have

    def _build(self, z: int) -> dict[tuple[int, int], list[tuple[int, Chunk]]]:
        n = 2 ** z
        margin = 2.0 / TILE_PX
        out: dict[tuple[int, int], list[tuple[int, Chunk]]] = {}
        for no, track in enumerate(self.tracks):
            for c in track.chunks(z):
                item = (no, c)
                for x in range(int(c.minx * n - margin), int(c.maxx * n + margin) + 1):
                    for y in range(int(c.miny * n - margin), int(c.maxy * n + margin) + 1):
                        out.setdefault((x, y), []).append(item)
        return out

    def pieces(self, z: int, x: int, y: int) -> list[tuple[int, Chunk]]:
        """The (track number, chunk) pieces in tile (z, x, y); none = the tile is empty."""
        if z <= INDEX_ZOOM:
            return self.tiles(z).get((x, y), [])
        up = z - INDEX_ZOOM
        near = self.tiles(INDEX_ZOOM).get((x >> up, y >> up), [])
        n = 2 ** z
        margin = 2.0 / (TILE_PX * n)
        x0, y0 = x / n - margin, y / n - margin
        x1, y1 = (x + 1) / n + margin, (y + 1) / n + margin
        return [p for p in near
                if not (p[1].maxx < x0 or p[1].minx > x1 or p[1].maxy < y0 or p[1].miny > y1)]

    def key(self, pieces: list[tuple[int, Chunk]]) -> str:
        """What a tile with these pieces shows: changes only if a track in the tile does."""
        h = hashlib.sha1(b"%d|" % HEAT_VERSION)
        for mark in sorted(self._marks[no] for no in {no for no, _ in pieces}):
            h.update(mark + b"|")
        return h.hexdigest()[:20]

    def draw(self, pieces: list[tuple[int, Chunk]], z: int, x: int, y: int) -> bytes | None:
        return _draw_heat([(self._rides[no], c) for no, c in pieces], z, x, y)


def heat_tile(tracks: list[Track], z: int, tx: int, ty: int,
              same_ride: dict[str, str] | None = None) -> bytes | None:
    """The heatmap tile (z, tx, ty): per pixel the number of rides through it. None = empty.
    same_ride (duplicates()): recordings of one ride are drawn all, and counted once.
    One tile on its own -- for many of the same tracks keep a HeatIndex."""
    index = HeatIndex(tracks, same_ride)
    pieces = index.pieces(z, tx, ty)
    return index.draw(pieces, z, tx, ty) if pieces else None


# --- duplicates -----------------------------------------------------------------------------

def overlap_share(a: tuple[float, float], b: tuple[float, float]) -> float:
    """Shared time of two (start, end) intervals as a share of the shorter one."""
    shared = min(a[1], b[1]) - max(a[0], b[0])
    shorter = min(a[1] - a[0], b[1] - b[0])
    if shared <= 0:
        return 0.0
    return 1.0 if shorter <= 0 else shared / shorter


def duplicates(items: list[tuple[str, int, float | None, float | None]],
               min_share: float = 0.5) -> dict[str, str]:
    """{id: id of the track standing for the ride} for items (id, priority, start, end):
    tracks recorded at the same time are one ride (also a long tour and the fragments of
    it another device recorded), the best (lowest) priority stands for it. It is the same ride
    again (a BikeLog ride uploaded to Komoot and imported back, a file uploaded twice).
    Items without times are never duplicates here."""
    timed = sorted((i for i in items if i[2] is not None and i[3] is not None), key=lambda i: i[2])
    root = {i[0]: i[0] for i in timed}

    def find(key: str) -> str:
        while root[key] != key:
            root[key] = key = root[root[key]]
        return key

    rank = {i[0]: (i[1], i[2]) for i in timed}
    for k, a in enumerate(timed):
        for b in timed[k + 1:]:
            if b[2] > a[3]:
                break
            if overlap_share((a[2], a[3]), (b[2], b[3])) >= min_share:
                winner, loser = sorted((find(a[0]), find(b[0])), key=rank.__getitem__)
                root[loser] = winner
    return {key: find(key) for key in root if find(key) != key}


# --- GPX ------------------------------------------------------------------------------------

@dataclass
class GpxTrack:
    points: list[tuple[float, float]]
    start: float | None             # epoch seconds of the first/last point with a time
    end: float | None
    name: str | None


def _local(tag: str) -> str:
    return tag.rpartition("}")[2]


def _epoch(text: str | None) -> float | None:
    import datetime
    if not text:
        return None
    text = text.strip()
    try:
        dt = datetime.datetime.fromisoformat(text.replace("Z", "+00:00"))
    except ValueError:
        return None
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=datetime.timezone.utc)
    return dt.timestamp()


def read_gpx(data: bytes) -> GpxTrack:
    """Track points of a GPX (trkpt; rtept if it has no track), with the first and last time
    and the track's (or file's) name. Raises ValueError if it is no GPX or holds no point."""
    import io
    import xml.etree.ElementTree as ET
    trk: list[tuple[float, float]] = []
    rte: list[tuple[float, float]] = []
    start = end = None
    name = None
    depth_name = None
    try:
        for event, el in ET.iterparse(io.BytesIO(data), events=("end",)):
            tag = _local(el.tag)
            if tag in ("trkpt", "rtept"):
                try:
                    lat, lon = float(el.get("lat")), float(el.get("lon"))
                except (TypeError, ValueError):
                    el.clear()
                    continue
                if not (-90 <= lat <= 90 and -180 <= lon <= 180) or (lat == 0 and lon == 0):
                    el.clear()
                    continue
                (trk if tag == "trkpt" else rte).append((lat, lon))
                if tag == "trkpt":
                    for child in el:
                        if _local(child.tag) == "time":
                            t = _epoch(child.text)
                            if t is not None:
                                start = t if start is None else min(start, t)
                                end = t if end is None else max(end, t)
                el.clear()
            elif tag == "name" and el.text and el.text.strip():
                # the first <name> of trk wins over metadata's; both over none
                depth_name = depth_name or el.text.strip()
            elif tag == "trk" and name is None:
                for child in el:
                    if _local(child.tag) == "name" and child.text and child.text.strip():
                        name = child.text.strip()
    except ET.ParseError as exc:
        raise ValueError(f"kein GPX: {exc}") from exc
    points = trk or rte
    if not points:
        raise ValueError("GPX ohne Track-Punkte")
    return GpxTrack(points, start, end, name or depth_name)


def distance_m(points: list[tuple[float, float]]) -> float:
    """Length of the (lat, lon) points without the gaps longer than MAX_GAP_M."""
    total = 0.0
    k = 111320.0
    for (la, lo), (lb, lob) in zip(points, points[1:]):
        dy = (lb - la) * k
        dx = (lob - lo) * k * math.cos(math.radians(lb))
        d = math.hypot(dx, dy)
        if d <= MAX_GAP_M:
            total += d
    return total
