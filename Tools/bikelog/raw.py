"""Reader (and fixture writer) for the raw accelerometer files, src/RawCapture.h.

R_<session>_NN.bin   400 Hz capture on demand
S_<session>.bin      0.75 s snippets around each logged shock

File = 64-byte FileHeader, then blocks: 24-byte BlockHeader + count frames of
int16 x, y, z (raw LSB, *not* scale-corrected -- g = raw / lsb_per_g * scale).
A block that doesn't start with the sync word is skipped by scanning for the
next one, so a damaged spot costs one block, not the rest of the file.
"""

from __future__ import annotations

import datetime
import struct
from array import array
from dataclasses import dataclass, field
from typing import Iterable

MAGIC = b"BCRW"
VERSION = 1
SYNC = 0xB10C
SYNC_BYTES = struct.pack("<H", SYNC)
SPEED_UNKNOWN = 0xFFFF
SPEED_AGE_UNKNOWN = 0xFFFF

KIND_CAPTURE = 0
KIND_SNIPPETS = 1
BLOCK_CONTINUOUS = 0
BLOCK_SHOCK = 1
BF_GAP_BEFORE = 0x01
BF_SPEED_FROM_GPS = 0x02

HEADER_FMT = "<4sBBHff3ffqfBBHH14s"     # 64 byte, RawCap::FileHeader
BLOCK_FMT = "<HBBHHqIHH"                # 24 byte, RawCap::BlockHeader
HEADER_SIZE = struct.calcsize(HEADER_FMT)
BLOCK_SIZE = struct.calcsize(BLOCK_FMT)
FRAME_SIZE = 6


class NotARawFile(Exception):
    pass


@dataclass
class RawHeader:
    kind: int = KIND_CAPTURE
    version: int = VERSION
    odr_hz: int = 400
    lsb_per_g: float = 2048.0
    scale: float = 1.0
    g0: tuple[float, float, float] = (0.0, 0.0, 1.0)
    noise_g: float = 0.005
    start_epoch_ms: int = 0
    wheelbase_m: float = 1.05
    interval_s: int = 2
    range_g: int = 16
    pre_ms: int = 250
    post_ms: int = 500

    def pack(self) -> bytes:
        return struct.pack(HEADER_FMT, MAGIC, self.version, self.kind, self.odr_hz,
                           self.lsb_per_g, self.scale, *self.g0, self.noise_g,
                           self.start_epoch_ms, self.wheelbase_m, self.interval_s,
                           self.range_g, self.pre_ms, self.post_ms, bytes(14))

    @classmethod
    def unpack(cls, raw: bytes) -> "RawHeader":
        v = struct.unpack(HEADER_FMT, raw)
        if v[0] != MAGIC:
            raise NotARawFile("no BCRW magic -- not a raw capture file")
        return cls(version=v[1], kind=v[2], odr_hz=v[3], lsb_per_g=v[4], scale=v[5],
                   g0=(v[6], v[7], v[8]), noise_g=v[9], start_epoch_ms=v[10],
                   wheelbase_m=v[11], interval_s=v[12], range_g=v[13], pre_ms=v[14],
                   post_ms=v[15])

    @property
    def g_per_lsb(self) -> float:
        return self.scale / self.lsb_per_g


@dataclass
class RawBlock:
    type: int = BLOCK_CONTINUOUS
    flags: int = 0
    speed_cms: int = SPEED_UNKNOWN
    epoch_ms: int = 0                   # first frame
    ref: int = 0                        # block number, or the shock's event_seq
    speed_age_ms: int = SPEED_AGE_UNKNOWN
    frames: array = field(default_factory=lambda: array("h"))   # x, y, z, x, y, z, ...

    @property
    def count(self) -> int:
        return len(self.frames) // 3

    @property
    def speed_kmh(self) -> float | None:
        return None if self.speed_cms == SPEED_UNKNOWN else self.speed_cms * 0.036

    def pack(self) -> bytes:
        return struct.pack(BLOCK_FMT, SYNC, self.type, self.flags, self.count, self.speed_cms,
                           self.epoch_ms, self.ref, self.speed_age_ms, 0) + self.frames.tobytes()

    def g(self, header: RawHeader) -> list[tuple[float, float, float]]:
        k = header.g_per_lsb
        f = self.frames
        return [(f[i] * k, f[i + 1] * k, f[i + 2] * k) for i in range(0, len(f), 3)]


@dataclass
class RawStats:
    blocks: int = 0
    frames: int = 0
    resyncs: int = 0                    # damaged spots skipped
    trailing_bytes: int = 0
    gaps: int = 0                       # blocks flagged BF_GAP_BEFORE


def read_raw(path, stats: RawStats | None = None) -> tuple[RawHeader, list[RawBlock]]:
    with open(path, "rb") as fh:
        data = fh.read()
    return parse(data, stats)


def parse(data: bytes, stats: RawStats | None = None) -> tuple[RawHeader, list[RawBlock]]:
    stats = stats if stats is not None else RawStats()
    if len(data) < HEADER_SIZE:
        raise NotARawFile("shorter than the file header")
    header = RawHeader.unpack(data[:HEADER_SIZE])
    blocks: list[RawBlock] = []
    pos = HEADER_SIZE
    while pos < len(data):
        if data[pos:pos + 2] != SYNC_BYTES:
            nxt = data.find(SYNC_BYTES, pos + 1)
            stats.resyncs += 1
            if nxt < 0:
                stats.trailing_bytes = len(data) - pos
                break
            pos = nxt
            continue
        if pos + BLOCK_SIZE > len(data):
            stats.trailing_bytes = len(data) - pos
            break
        _, btype, flags, count, speed, epoch, ref, age, _ = struct.unpack_from(BLOCK_FMT, data, pos)
        end = pos + BLOCK_SIZE + count * FRAME_SIZE
        if end > len(data):
            stats.trailing_bytes = len(data) - pos
            break
        frames = array("h")
        frames.frombytes(data[pos + BLOCK_SIZE:end])
        blocks.append(RawBlock(btype, flags, speed, epoch, ref, age, frames))
        stats.blocks += 1
        stats.frames += count
        if flags & BF_GAP_BEFORE:
            stats.gaps += 1
        pos = end
    return header, blocks


def write_raw(path, header: RawHeader, blocks: Iterable[RawBlock]) -> int:
    """Write a raw file (for test fixtures). Returns the number of bytes."""
    payload = header.pack() + b"".join(b.pack() for b in blocks)
    with open(path, "wb") as fh:
        fh.write(payload)
    return len(payload)


def utc_ms(epoch_ms: int) -> datetime.datetime:
    return datetime.datetime.fromtimestamp(epoch_ms / 1000.0, datetime.timezone.utc)
