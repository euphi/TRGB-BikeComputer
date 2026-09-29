"""Play a GPX track into the simulator build of the bike computer.

The simulator build (env ``trgb-esp32-s3-sim``, ``src/SimSensors.*``) fakes a
speed sensor, a cadence sensor and a heart-rate strap; it takes its values
over the serial console (``sim <km/h> <rpm> <bpm>``) or over HTTP
(``/debug/sim/set``). This module turns a GPX track into one such value set
per second and sends it in real time -- the statistics run on the device's
clock, so there is no fast-forward.

Rules (the same as the GPX player on the /debug/sim page):

* speed from the geometry, distance difference over +-2 s (smooths GPS noise),
* heart rate and cadence from the TrackPointExtension if the file has them,
* without cadence: ``default_cad`` while moving, 0 below 3 km/h and downhill
  steeper than 4 % (coasting, to exercise that state too),
* pauses in the recording longer than ``max_gap`` s are shortened to it.

At the end it prints what it sent (distance, moving time, averages) next to
the device's change of the Trip statistics (/stat/summary), if it can reach
the device over HTTP.
"""

from __future__ import annotations

import bisect
import json
import os
import sys
import threading
import time
import urllib.error
import urllib.request
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field
from datetime import datetime

from .geo import haversine_m

SPEED_WINDOW_S = 2.0        # +- around the playback time
GRADIENT_WINDOW_S = 5.0
COAST_GRADIENT_PCT = -4.0
STOP_KMH = 3.0

# Statistics::cycle() hysteresis: moving from > 5.5 km/h, stopped below 0.3 km/h
MOVE_START_KMH = 5.5
MOVE_END_KMH = 0.3


@dataclass
class Point:
    s: float                 # playback time from the start in s (after shortening pauses)
    d: float                 # distance from the start in m
    ele: float | None
    hr: float | None
    cad: float | None


@dataclass
class Track:
    points: list[Point]
    has_cad: bool
    has_hr: bool

    @property
    def duration(self) -> float:
        return self.points[-1].s

    @property
    def distance(self) -> float:
        return self.points[-1].d


@dataclass
class Sample:
    speed: float             # km/h
    cad: int | None          # None: no cadence sensor
    hr: int | None           # None: no heart-rate strap
    dist: float              # m along the track


def _local(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def _child_value(el: ET.Element, name: str) -> float | None:
    """First descendant with this local name (any namespace, like getElementsByTagNameNS('*'))."""
    for sub in el.iter():
        if sub is not el and _local(sub.tag) == name and sub.text and sub.text.strip():
            try:
                return float(sub.text)
            except ValueError:
                return None
    return None


def _child_text(el: ET.Element, name: str) -> str | None:
    for sub in el:
        if _local(sub.tag) == name and sub.text:
            return sub.text.strip()
    return None


def load(path, max_gap: float = 0.0) -> Track:
    """Reads the timed track points of a GPX file (1.0 or 1.1, any extension namespace)."""
    raw = []
    prev = None
    dist = 0.0
    for el in ET.parse(path).getroot().iter():
        if _local(el.tag) != "trkpt":
            continue
        when = _child_text(el, "time")
        if not when:
            continue
        t = datetime.fromisoformat(when.replace("Z", "+00:00")).timestamp()
        if prev is not None and t <= prev[0]:
            continue
        lat, lon = float(el.get("lat")), float(el.get("lon"))
        if prev is not None:
            dist += haversine_m(prev[1], prev[2], lat, lon)
        raw.append((t, dist, _child_value(el, "ele"), _child_value(el, "hr"), _child_value(el, "cad")))
        prev = (t, lat, lon)
    if len(raw) < 2:
        raise ValueError("%s: fewer than two track points with <time>" % path)

    points = []
    s = 0.0
    for i, (t, d, ele, hr, cad) in enumerate(raw):
        if i:
            dt = t - raw[i - 1][0]
            s += min(dt, max_gap) if max_gap > 0 else dt
        points.append(Point(s, d, ele, hr, cad))
    return Track(points,
                 has_cad=any(p.cad is not None for p in points),
                 has_hr=any(p.hr is not None for p in points))


class _Sampler:
    def __init__(self, track: Track, default_cad: int):
        self.track = track
        self.default_cad = default_cad
        self.times = [p.s for p in track.points]

    def _index(self, s: float) -> int:
        return max(0, bisect.bisect_right(self.times, s) - 1)

    def _at(self, s: float, attr: str) -> float | None:
        pts = self.track.points
        s = max(0.0, min(self.track.duration, s))
        i = self._index(s)
        a, b = pts[i], pts[min(i + 1, len(pts) - 1)]
        va, vb = getattr(a, attr), getattr(b, attr)
        if va is None or vb is None or b.s == a.s:
            return va
        return va + (vb - va) * (s - a.s) / (b.s - a.s)

    def sample(self, s: float) -> Sample:
        T = self.track.duration
        a, b = max(0.0, s - SPEED_WINDOW_S), min(T, s + SPEED_WINDOW_S)
        speed = (self._at(b, "d") - self._at(a, "d")) / (b - a) * 3.6 if b > a else 0.0
        p = self.track.points[self._index(s)]
        if self.track.has_cad:
            cad = None if p.cad is None else int(round(p.cad))
        else:
            c, e = max(0.0, s - GRADIENT_WINDOW_S), min(T, s + GRADIENT_WINDOW_S)
            dd = self._at(e, "d") - self._at(c, "d")
            e1, e2 = self._at(c, "ele"), self._at(e, "ele")
            grad = (e2 - e1) / dd * 100 if dd > 20 and e1 is not None and e2 is not None else 0.0
            cad = 0 if speed < STOP_KMH or grad < COAST_GRADIENT_PCT else self.default_cad
        hr = int(round(p.hr)) if self.track.has_hr and p.hr is not None else None
        return Sample(speed, cad, hr, self._at(s, "d"))


def sampler(track: Track, default_cad: int = 80):
    """Returns f(s) -> Sample for playback time s."""
    return _Sampler(track, default_cad).sample


# --- links to the device ---------------------------------------------------------------

def _opt(v) -> str:
    return "-" if v is None else str(v)


class SerialLink:
    """The serial console. Uses pyserial if installed, plain termios otherwise (Linux/macOS).

    The device's output is always read (a USB CDC port nobody reads can stall the
    device's log output) and printed only with echo=True.
    """

    def __init__(self, port: str, echo: bool = False):
        self.echo = echo
        self._stop = False
        try:
            import serial  # type: ignore
            self._ser = serial.Serial(port, 115200, timeout=0.2)
            self._read = lambda: self._ser.read(256)
            self._write = self._ser.write
            self._close = self._ser.close
        except ImportError:
            import termios
            import tty
            fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
            tty.setraw(fd)
            attrs = termios.tcgetattr(fd)
            attrs[6][termios.VMIN] = 0
            attrs[6][termios.VTIME] = 2
            termios.tcsetattr(fd, termios.TCSANOW, attrs)
            self._read = lambda: os.read(fd, 256)
            self._write = lambda b: os.write(fd, b)
            self._close = lambda: os.close(fd)
        self._reader = threading.Thread(target=self._drain, daemon=True)
        self._reader.start()

    def _drain(self):
        while not self._stop:
            try:
                data = self._read()
            except OSError:
                return
            if data and self.echo:
                sys.stdout.write(data.decode("utf-8", "replace"))
                sys.stdout.flush()

    def _line(self, line: str) -> bool:
        self._write((line + "\r").encode())
        return True

    def send(self, s: Sample) -> bool:
        return self._line("sim %.2f %s %s" % (s.speed, _opt(s.cad), _opt(s.hr)))

    def stop(self) -> None:
        self._line("sim stop")

    def close(self) -> None:
        time.sleep(0.3)
        self._stop = True
        self._reader.join(timeout=1)
        self._close()


class HttpLink:
    def __init__(self, host: str):
        self.base = host if host.startswith("http") else "http://" + host

    def _get(self, path: str) -> bool:
        try:
            with urllib.request.urlopen(self.base + path, timeout=3) as r:
                return r.status == 200
        except (urllib.error.URLError, OSError):
            return False

    def send(self, s: Sample) -> bool:
        return self._get("/debug/sim/set?speed=%.2f&cad=%s&hr=%s" % (s.speed, _opt(s.cad), _opt(s.hr)))

    def stop(self) -> None:
        self._get("/debug/sim/stop")

    def close(self) -> None:
        pass


def fetch_summary(host: str | None) -> dict | None:
    if not host:
        return None
    base = host if host.startswith("http") else "http://" + host
    try:
        with urllib.request.urlopen(base + "/stat/summary", timeout=5) as r:
            return json.load(r)
    except (urllib.error.URLError, OSError, ValueError):
        return None


# --- playback --------------------------------------------------------------------------

@dataclass
class Sent:
    """What was sent, integrated over the real time between the updates."""
    seconds: float = 0.0
    dist: float = 0.0
    moving_s: float = 0.0
    max_speed: float = 0.0
    cad_revs: float = 0.0
    cad_s: float = 0.0
    updates: int = 0
    errors: int = 0
    _moving: bool = field(default=False, repr=False)

    def add(self, s: Sample, dt: float) -> None:
        """s was in effect for dt seconds."""
        self.seconds += dt
        self.dist += s.speed / 3.6 * dt
        if not self._moving and s.speed > MOVE_START_KMH:
            self._moving = True
        elif self._moving and s.speed < MOVE_END_KMH:
            self._moving = False
        if self._moving:
            self.moving_s += dt
            if s.cad:
                self.cad_revs += s.cad * dt / 60
                self.cad_s += dt
        self.max_speed = max(self.max_speed, s.speed)


def play(track: Track, link, start_s: float = 0.0, duration_s: float | None = None,
         default_cad: int = 80, progress=None) -> Sent:
    """Sends one sample per second in real time until the end (or duration_s); Ctrl-C stops."""
    sample = sampler(track, default_cad)
    end = track.duration if duration_s is None else min(track.duration, start_s + duration_s)
    sent = Sent()
    t0 = time.monotonic()
    last = None     # (sample, monotonic time sent)
    k = 0
    try:
        while True:
            s = start_s + k
            if s > end:
                break
            wait = t0 + k - time.monotonic()
            if wait > 0:
                time.sleep(wait)
            x = sample(s)
            now = time.monotonic()
            if last is not None:
                sent.add(last[0], now - last[1])
            ok = link.send(x)
            sent.updates += 1
            sent.errors += 0 if ok else 1
            last = (x, now)
            if progress:
                progress(s, end, x, sent)
            k += 1
        if last is not None:
            sent.add(last[0], time.monotonic() - last[1])
    except KeyboardInterrupt:
        if last is not None:
            sent.add(last[0], time.monotonic() - last[1])
        print("\nabgebrochen")
    finally:
        link.stop()
        link.close()
    return sent


def compare(sent: Sent, before: dict | None, after: dict | None) -> str:
    """Table: sent vs. the device's Trip change during the playback."""
    rows = []

    def avg(d, t):
        return d / t * 3.6 if t else float("nan")

    dev = None
    if before and after:
        b, a = before["TRIP"], after["TRIP"]
        dd = a["dist"]["net"] - b["dist"]["net"]
        dm = a["time"]["moving"] - b["time"]["moving"]
        dt = a["time"]["total"] - b["time"]["total"]
        dev = {"dist": dd, "moving": dm, "total": dt, "avg": avg(dd, dm),
               "max": a.get("maxSpeed"), "cad": a.get("cadence")}
    fmt = "%-26s %12s %12s"
    rows.append(fmt % ("", "gesendet", "Gerät (Trip)"))

    def v(x, f):
        return "-" if x is None else (f % x)

    rows.append(fmt % ("Strecke (m)", "%.0f" % sent.dist, v(dev and dev["dist"], "%.0f")))
    rows.append(fmt % ("Fahrzeit (s)", "%.0f" % sent.moving_s, v(dev and dev["moving"], "%.0f")))
    rows.append(fmt % ("Zeit gesamt (s)", "%.0f" % sent.seconds, v(dev and dev["total"], "%.0f")))
    rows.append(fmt % ("Ø fahrend (km/h)", "%.2f" % avg(sent.dist, sent.moving_s), v(dev and dev["avg"], "%.2f")))
    rows.append(fmt % ("max (km/h)", "%.1f" % sent.max_speed, v(dev and dev["max"], "%.1f")))
    cad = sent.cad_revs / sent.cad_s * 60 if sent.cad_s else None
    rows.append(fmt % ("Ø Trittfrequenz (rpm)", v(cad, "%.1f"), v(dev and dev["cad"], "%.1f")))
    rows.append("Updates %d, Fehler %d" % (sent.updates, sent.errors))
    if dev:
        rows.append("Gerät: Trip-Änderung während der Wiedergabe; max und Ø Trittfrequenz sind "
                    "Trip-Gesamtwerte (vorher Trip zurücksetzen).")
    return "\n".join(rows)
