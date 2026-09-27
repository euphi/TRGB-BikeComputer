"""The SD card's naming scheme, as far as the service needs to know it.

Mirrors the firmware (src/LogSessions.cpp, BCLogger.cpp ``timeKey``)::

    /BIKECOMP/20260920/L_143012.bin      day directory, type letter, session stem
                      R_143012_01.bin    raw capture 1 of session _143012
    /BIKECOMP/NO_TIME/L_0042.bin         no time known (older firmware: L0042.bin)
    /BIKECOMP/CUR/...                    running / unfinished sessions -- never taken

All files of one session share the stem, so (device, day, stem) is a session.
"""

from __future__ import annotations

import re
from dataclasses import dataclass

#: Working directory of the running session. Its files are still growing and get
#: renamed on the next boot, so neither the puller nor the upload API takes them.
WORKDIR = "CUR"

KINDS = {
    "L": "data",       # binary log (LogRecords.h) -- GPX/CSV are derived from it
    "D": "debug",
    "N": "nmea",       # raw Forumslader NMEA
    "S": "shocks",     # raw IMU snippets around shocks (RawCapture.h)
    "R": "raw",        # raw IMU capture on demand
    "I": "summary",    # key=value summary written by LogSessions
    "T": "timehints",
}

_DAY = re.compile(r"^(|[A-Za-z0-9_]{1,16})$")
_NAME = re.compile(r"^[A-Za-z][A-Za-z0-9_]{0,31}\.[A-Za-z0-9]{1,4}$")


@dataclass(frozen=True)
class SdFile:
    day: str        # "20260920", "NO_TIME", "" for files directly in /BIKECOMP
    name: str       # "L_143012.bin"

    @property
    def kind(self) -> str:
        return self.name[0].upper()

    @property
    def stem(self) -> str:
        """Session key inside a day: "L_143012.bin" -> "_143012", "R_143012_01.bin"
        -> "_143012", legacy "L0042.bin" -> "0042" (kept distinct from "_0042")."""
        rest = self.name[1:].split(".", 1)[0]
        if rest.startswith("_"):
            return "_" + rest[1:].split("_", 1)[0]
        return rest.split("_", 1)[0]

    @property
    def path(self) -> str:
        return f"{self.day}/{self.name}" if self.day else self.name


class BadPath(ValueError):
    pass


def parse(day: str, name: str) -> SdFile:
    """Validate one (day, name) pair. Raises BadPath for anything that could escape
    the storage directory or does not follow the scheme."""
    if not _DAY.match(day or "") or not _NAME.match(name or ""):
        raise BadPath(f"not a log file path: {day!r}/{name!r}")
    if name[0] == "x":
        raise BadPath(f"hidden file: {name}")      # 'x' = hidden on the device
    return SdFile(day or "", name)


def parse_path(path: str) -> SdFile:
    """"20260920/L_143012.bin" or "L0042.bin" -> SdFile."""
    day, _, name = path.strip("/").rpartition("/")
    return parse(day, name)


def parse_summary(text: str) -> dict:
    """I_*.txt (``key=value`` lines, see SessionStats::format) -> dict, numbers
    converted. Unknown keys are kept: the firmware may add some."""
    out: dict = {}
    for line in text.splitlines():
        key, sep, value = line.partition("=")
        if not sep or not key.strip():
            continue
        value = value.strip()
        for conv in (int, float):
            try:
                out[key.strip()] = conv(value)
                break
            except ValueError:
                continue
        else:
            out[key.strip()] = value
    return out
