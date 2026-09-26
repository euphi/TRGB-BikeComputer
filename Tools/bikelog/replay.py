"""Replay raw recordings through the firmware's own road-quality algorithm.

Instead of a Python re-implementation that would drift from the firmware, this
builds Tools/rqreplay/rq_replay.cpp together with src/RoadQuality.cpp -- the
exact code running on the bike computer -- with the host's g++, and parses
its output. The binary is cached in Tools/.build/ and rebuilt whenever one of
its sources is newer.
"""

from __future__ import annotations

import math
import shutil
import subprocess
from dataclasses import dataclass
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
REPO = TOOLS.parent
SOURCES = [
    TOOLS / "rqreplay" / "rq_replay.cpp",
    REPO / "src" / "RoadQuality.cpp",
    REPO / "src" / "RoadQuality.h",
    REPO / "src" / "RawCapture.h",
]
BINARY = TOOLS / ".build" / "rq_replay"

#: Parameters rq_replay understands (key=value), with the firmware default for the help text
PARAMS = {
    "interval": "Intervall in s (2)",
    "shock": "absolute Stoßschwelle in g (3.0)",
    "rel": "relative Stoßschwelle, Faktor auf RMS der letzten Sekunde (6)",
    "off": "Hysterese: Ende unter off * Schwelle (0.5)",
    "hp": "Hochpass in Hz (2)",
    "lp": "Tiefpass in Hz (80)",
    "baseline": "Baseline in g bei vref (0.15 = Standard)",
    "exp": "Geschwindigkeitsexponent (0.8)",
    "vref": "Referenzgeschwindigkeit km/h (20)",
    "vmin": "Mindestgeschwindigkeit km/h (6)",
    "wheelbase": "Radstand in m (aus der Datei)",
    "gtau": "Zeitkonstante Schwerkraft in s (2)",
    "maxevents": "Stöße pro Minute höchstens (20)",
    "t1": "Zählschwelle 1 in g (1)",
    "t2": "Zählschwelle 2 in g (2)",
    "sev2": "Schwere 2 ab g (5)",
    "sev3": "Schwere 3 ab g (8)",
    "latency": "Speed-Latenz der Steigungsschätzung in ms (500)",
}


class ReplayUnavailable(Exception):
    pass


def build(force: bool = False) -> Path:
    """Compile rq_replay if needed; returns the binary's path."""
    if not force and BINARY.exists():
        built = BINARY.stat().st_mtime
        if all(src.stat().st_mtime <= built for src in SOURCES):
            return BINARY
    compiler = shutil.which("g++") or shutil.which("clang++")
    if not compiler:
        raise ReplayUnavailable("kein C++-Compiler (g++/clang++) gefunden -- für 'raw replay' nötig")
    BINARY.parent.mkdir(exist_ok=True)
    cmd = [compiler, "-std=c++17", "-O2", "-I", str(REPO / "src"),
           str(REPO / "src" / "RoadQuality.cpp"), str(TOOLS / "rqreplay" / "rq_replay.cpp"),
           "-o", str(BINARY)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        raise ReplayUnavailable("rq_replay ließ sich nicht bauen:\n" + result.stderr)
    return BINARY


def _f(text: str) -> float | None:
    value = float(text)
    return None if math.isnan(value) else value


@dataclass
class ReplayInterval:
    t_ms: float
    road_class: int
    roughness: float | None
    rms_vert_g: float
    rms_horiz_g: float
    peak_vert_max_g: float
    peak_vert_min_g: float
    peak_total_g: float
    vdv: float
    speed_kmh: float | None
    distance_m: float
    over_1g: int
    over_2g: int
    flags: int
    grad_raw: float | None


@dataclass
class ReplayShock:
    t_ms: float
    ref: int
    peak_g: float
    vert_max_g: float
    vert_min_g: float
    horiz_g: float
    duration_ms: float
    second_g: float
    second_delay_ms: float
    severity: int
    flags: int
    threshold_g: float
    pre_rms_g: float


def run(path, params: dict[str, float] | None = None) -> tuple[list[ReplayInterval], list[ReplayShock], str]:
    """Replay one R_/S_ file. Returns (intervals, shocks, the tool's stderr summary)."""
    binary = build()
    args = [str(binary), str(path)] + ["%s=%s" % (k, v) for k, v in (params or {}).items()]
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode != 0:
        raise ReplayUnavailable(result.stderr.strip() or "rq_replay failed")
    intervals: list[ReplayInterval] = []
    shocks: list[ReplayShock] = []
    for line in result.stdout.splitlines():
        p = line.split(",")
        if p[0] == "I":
            intervals.append(ReplayInterval(
                float(p[1]), int(p[2]), _f(p[3]), float(p[4]), float(p[5]), float(p[6]),
                float(p[7]), float(p[8]), float(p[9]), _f(p[10]), float(p[11]), int(p[12]),
                int(p[13]), int(p[14]), _f(p[15])))
        elif p[0] == "S":
            shocks.append(ReplayShock(
                float(p[1]), int(p[2]), float(p[3]), float(p[4]), float(p[5]), float(p[6]),
                float(p[7]), float(p[8]), float(p[9]), int(p[10]), int(p[11]), float(p[12]),
                float(p[13])))
    return intervals, shocks, result.stderr.strip()
