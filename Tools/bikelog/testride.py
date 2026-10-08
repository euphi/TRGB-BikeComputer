"""Is this session a test rather than a ride? Simulator and GPS playback.

Two kinds of test sessions end up on the SD card next to the real rides:

``sim``           the log says so: LOG_SIMULATED in the data records (or
                  RSF_SIMULATED in a ride-state record). The firmware sets it for
                  the sensor simulator (``sim`` on the serial console, simulator
                  build, since 2026-09-29) and for every frame of a TrailBridge
                  test ride (GPS_TAG_SIM_FLAGS, since 2026-10-02).
``gps_playback``  older TrailBridge test rides carry no flag. Their signature: the
                  phone plays a GPX file, so the position travels kilometres while
                  the bike computer lies on the desk and its wheel sensor counts
                  (next to) nothing. A real ride with a dead wheel sensor looks the
                  same -- which is why this kind is only "suspected" and the log
                  service lets the rider overrule it.

And a third case that is neither ride nor test: ``idle()`` -- the bike computer was
switched on and nothing happened (wheel and GPS stand still, only debug output). The
log service archives those.

Pure functions over the records; used by the report (meta.test) and by the log
service's index (sessions.test_kind, sessions.idle), so all agree.
"""

from __future__ import annotations

from dataclasses import dataclass

from .geo import haversine_m
from .record import Record, RideStateRecord

#: GPS travelled at least this far ...
PLAYBACK_MIN_GPS_M = 500.0
#: ... while the wheel sensor counted less than this share of it.
PLAYBACK_MAX_WHEEL_SHARE = 0.15
#: Idle: the wheel counted less than this, and all fixes lie within IDLE_MAX_EXTENT_M
#: (a phone lying next to the bike computer adds up kilometres of GPS jitter as a
#: path, but not as an extent -- hence the extent, not the path).
IDLE_MAX_WHEEL_M = 50.0
IDLE_MAX_EXTENT_M = 300.0
#: Only fresh, plausible fixes count (stale heartbeat repeats and jumps do not).
FRESH_FIX_MS = 5000
MAX_STEP_M = 500.0

KIND_SIM = "sim"
KIND_GPS_PLAYBACK = "gps_playback"

LABELS = {
    KIND_SIM: "Simuliert",
    KIND_GPS_PLAYBACK: "GPS-Wiedergabe?",
}


@dataclass
class TestInfo:
    kind: str | None = None             # None = a real ride as far as the log can tell
    reason: str = ""
    simulated_share: float = 0.0        # share of data records with LOG_SIMULATED
    gps_m: float = 0.0
    wheel_m: float = 0.0
    extent_m: float = 0.0               # diagonal of the fixes' bounding box

    def as_dict(self) -> dict:
        return {"kind": self.kind, "reason": self.reason,
                "simulated_share": round(self.simulated_share, 3),
                "gps_km": round(self.gps_m / 1000, 2), "wheel_km": round(self.wheel_m / 1000, 2),
                "gps_extent_km": round(self.extent_m / 1000, 2)}


def _km(m: float) -> str:
    return f"{m / 1000:.1f}".replace(".", ",")


def _fresh(records: list[Record]):
    return [r for r in records
            if r.gps_valid and r.gps_fix_age_ms <= FRESH_FIX_MS and (r.gps_lat_e7 or r.gps_lon_e7)]


def gps_extent_m(records: list[Record]) -> float:
    """Diagonal of the bounding box of the fresh fixes: how far the position really got."""
    fixes = _fresh(records)
    if len(fixes) < 2:
        return 0.0
    lats = [r.latitude for r in fixes]
    lons = [r.longitude for r in fixes]
    return haversine_m(min(lats), min(lons), max(lats), max(lons))


def gps_distance_m(records: list[Record]) -> float:
    """Path length of the fresh fixes, ignoring jumps (> MAX_STEP_M between two)."""
    dist = 0.0
    prev = None
    for rec in _fresh(records):
        if prev is not None:
            step = haversine_m(prev.latitude, prev.longitude, rec.latitude, rec.longitude)
            if step <= MAX_STEP_M:
                dist += step
        prev = rec
    return dist


def classify(everything) -> TestInfo:
    """Test or ride, from all records of a session (or only its data records)."""
    data = [r for r in everything if isinstance(r, Record)]
    states = [r for r in everything if isinstance(r, RideStateRecord)]
    info = TestInfo()
    if not data:
        return info
    sim = sum(1 for r in data if r.simulated)
    info.simulated_share = sim / len(data)
    info.wheel_m = max(0.0, data[-1].distance - data[0].distance)
    info.gps_m = gps_distance_m(data)
    info.extent_m = gps_extent_m(data)
    if sim or any(s.simulated for s in states):
        info.kind = KIND_SIM
        info.reason = ("Simulierte Daten laut Log (sim auf dem Fahrradcomputer oder "
                       "TrailBridge-Testfahrt)"
                       + (f", {info.simulated_share:.0%} der Datensätze" if sim and sim < len(data) else ""))
    elif (info.gps_m >= PLAYBACK_MIN_GPS_M and info.extent_m >= PLAYBACK_MIN_GPS_M
          and info.wheel_m < PLAYBACK_MAX_WHEEL_SHARE * info.gps_m):
        info.kind = KIND_GPS_PLAYBACK
        info.reason = (f"GPS legt {_km(info.gps_m)} km zurück, der Radsensor nur "
                       f"{_km(info.wheel_m)} km -- vermutlich eine TrailBridge-Testfahrt "
                       "(oder der Radsensor ist ausgefallen)")
    return info


def idle(records) -> bool:
    """Switched on, nothing happened: no data at all, or the wheel stood (< IDLE_MAX_WHEEL_M)
    and the position did not get anywhere (< IDLE_MAX_EXTENT_M) -- simulator or not."""
    data = [r for r in records if isinstance(r, Record)]
    if not data:
        return True
    wheel = max(0.0, data[-1].distance - data[0].distance)
    return wheel < IDLE_MAX_WHEEL_M and gps_extent_m(data) < IDLE_MAX_EXTENT_M
