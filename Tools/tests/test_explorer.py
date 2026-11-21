"""bikelog.explorer: visited tiles, cluster/square, overlay tiles, duplicates."""

import struct
import zlib

import pytest

from bikelog import explorer as ex


def _png_pixels(data: bytes) -> bytes:
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos, idat = 8, b""
    while pos < len(data):
        n, = struct.unpack(">I", data[pos:pos + 4])
        kind = data[pos + 4:pos + 8]
        if kind == b"IDAT":
            idat += data[pos + 8:pos + 8 + n]
        pos += 12 + n
    raw = zlib.decompress(idat)
    return b"".join(raw[r * 257 + 1:(r + 1) * 257] for r in range(256))


def test_mercator_round_trip():
    x, y = ex.mercator(50.00, 8.60)
    lat, lon = ex.unmercator(x, y)
    assert (lat, lon) == pytest.approx((50.00, 8.60), abs=1e-9)


def test_segment_fills_every_tile_it_crosses():
    # two points 3 tiles apart diagonally at zoom 14: the tiles between count too
    n = 2 ** 14
    a = ex.unmercator(100.5 / n, 200.5 / n)
    b = ex.unmercator(103.5 / n, 203.5 / n)
    tiles = ex.tiles_of(ex.runs_of([a, b], max_gap_m=1e9), 14)
    assert (100, 200) in tiles and (103, 203) in tiles
    assert len(tiles) == 7                     # a staircase: 3 steps right, 3 down


def test_gap_fills_nothing():
    runs = ex.runs_of([(49.0, 10.0), (49.0, 10.5)])          # 36 km, no point between
    assert len(runs) == 2
    assert len(ex.tiles_of(runs, 14)) == 2


def test_cluster_and_max_square():
    block = {(x, y) for x in range(10, 15) for y in range(20, 25)}        # 5x5
    tiles = block | {(30, 30), (31, 30)}
    assert ex.max_square(tiles) == (10, 20, 5)
    assert ex.cluster(tiles) == {(x, y) for x in range(11, 14) for y in range(21, 24)}
    assert len(ex.max_cluster(tiles)) == 9
    stats = ex.tile_stats(tiles, 14).as_dict()
    assert (stats["tiles"], stats["max_square"], stats["max_cluster"]) == (27, 5, 9)
    s, w, n, e = stats["max_square_bounds"]
    assert s < n and w < e


def test_visited_tile_draws_the_tile_and_the_square():
    stats = ex.tile_stats({(8000, 5000)}, 14)
    png = ex.visited_tile(stats, 14, 8000, 5000)
    px = _png_pixels(png)
    assert px[128 * 256 + 128] == 1                         # inside: visited
    assert px[0] == 5                                       # corner: the 1x1 max square border
    # one zoom out, the tile is a quarter of the map tile
    px = _png_pixels(ex.visited_tile(stats, 13, 4000, 2500))
    assert px[64 * 256 + 64] == 1 and px[192 * 256 + 192] in (0, 4)
    # far away: nothing to draw at low zoom
    assert ex.visited_tile(ex.tile_stats({(1, 1)}, 14), 6, 60, 60) is None


def test_heat_counts_tracks_not_passes():
    n = 2 ** 14
    lat0, lon0 = ex.unmercator(8000.1 / n, 5000.5 / n)
    lat1, lon1 = ex.unmercator(8000.9 / n, 5000.5 / n)
    there_and_back = ex.make_track("a", [(lat0, lon0), (lat1, lon1), (lat0, lon0)])
    other = ex.make_track("b", [(lat0, lon0), (lat1, lon1)])
    px = _png_pixels(ex.heat_tile([there_and_back], 14, 8000, 5000))
    assert px[128 * 256 + 128] == 1                         # twice along, counted once
    px = _png_pixels(ex.heat_tile([there_and_back, other], 14, 8000, 5000))
    assert px[128 * 256 + 128] == 2
    assert ex.heat_tile([other], 14, 9000, 5000) is None


def test_coarse_levels_keep_the_shape():
    pts = [(49.0 + i * 1e-4, 10.0 + (i % 2) * 1e-5) for i in range(2000)]
    track = ex.make_track("t", pts)
    full = sum(len(c.xs) for c in track.chunks(16))
    coarse = sum(len(c.xs) for c in track.chunks(7))
    assert coarse < full / 20
    assert ex.heat_tile([track], 7, *[int(v * 2 ** 7) for v in ex.mercator(49.1, 10.0)]) is not None


def test_duplicates_prefer_the_better_source():
    items = [("r1", 0, 1000.0, 2000.0), ("k1", 2, 1010.0, 1990.0), ("u1", 1, 1500.0, 2500.0),
             ("k2", 2, 5000.0, 6000.0), ("x", 1, None, None)]
    assert ex.duplicates(items) == {"k1": "r1", "u1": "r1"}


def test_read_gpx_name_times_and_route_fallback():
    gpx = (b'<?xml version="1.0"?><gpx xmlns="http://www.topografix.com/GPX/1/1"><metadata><name>Meta</name>'
           b'</metadata><trk><name>Runde</name><trkseg><trkpt lat="49.0" lon="10.0"><time>2026-05-01T10:00:00Z'
           b'</time></trkpt><trkpt lat="49.01" lon="10.0"><time>2026-05-01T10:05:00Z</time></trkpt>'
           b'</trkseg></trk></gpx>')
    t = ex.read_gpx(gpx)
    assert t.name == "Runde" and len(t.points) == 2 and t.end - t.start == 300
    rte = b'<gpx><rte><rtept lat="49" lon="10"/><rtept lat="49.1" lon="10"/></rte></gpx>'
    assert len(ex.read_gpx(rte).points) == 2
    with pytest.raises(ValueError):
        ex.read_gpx(b"<gpx></gpx>")
    with pytest.raises(ValueError):
        ex.read_gpx(b"no xml")
