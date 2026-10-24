"""Hourly wind and weather for a ride, from Open-Meteo -- the input of the wind model in
bikelog.report. This module is the only one that touches the network; the report itself
takes a ready ``Weather``.

    cell(lat, lon)                       the 0.1 degree cell a position is rounded to
    load(cache_dir, lat, lon, t0, t1)    -> Loaded: weather (or None) + why not + when to retry
    Weather.at(t)                        interpolated Sample at epoch second t

Privacy: only the cell (0.1 degree, ~10 km) and the dates leave the machine, never the
track. Wind does not vary on a smaller scale than the model's grid anyway.

Source: the archive API (ERA5 reanalysis, final after a few days); for the last days, for
which it has gaps, the "historical forecast" API (model analyses, available without delay).
Days that came from there, or are less than PROVISIONAL_DAYS old, are marked provisional and
fetched again once the archive is final -- the report then carries a ``retry_after`` (see
analysis.py). Everything fetched is kept per day and cell in ``cache_dir``
(``2026-10-24_49.5_10.9.json``), so a report is reproducible and recomputing it needs no
network. Any failure -- no network, a timeout, an answer without data -- is not an error for
the caller: ``Loaded.weather`` is None and ``error`` says why.
"""

from __future__ import annotations

import datetime
import json
import math
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path

ARCHIVE_URL = "https://archive-api.open-meteo.com/v1/archive"
FORECAST_URL = "https://historical-forecast-api.open-meteo.com/v1/forecast"
SOURCES = (("archive", ARCHIVE_URL), ("historical-forecast", FORECAST_URL))

#: The cell positions are rounded to before they are sent.
CELL_DEG = 0.1
HOURLY = ("wind_speed_10m", "wind_direction_10m", "wind_gusts_10m", "temperature_2m",
          "surface_pressure")
TIMEOUT_S = 8.0
#: Days younger than this are not final in the archive (ERA5 lags by ~5 days).
PROVISIONAL_DAYS = 7
#: A failed fetch is tried again after this long (seconds).
RETRY_S = 3600
#: After a network failure, no further attempt for this long in the same process -- a
#: refresh over many rides must not wait for a timeout on every one of them.
BACKOFF_S = 300

_failed_at: float | None = None         # time.monotonic() of the last network failure


class WeatherError(Exception):
    """The weather service could not be reached or answered with nonsense."""


def cell(lat: float, lon: float) -> tuple[float, float]:
    """Position rounded to the 0.1 degree cell (half up, no float surprises)."""
    def snap(x: float) -> float:
        return round(math.floor(round(x / CELL_DEG, 6) + 0.5) * CELL_DEG, 1)
    return snap(lat), snap(lon)


@dataclass
class Sample:
    """Weather at one moment; wind speed in m/s, direction in degrees *from which* it blows."""

    speed_ms: float
    dir_deg: float
    gust_ms: float | None = None
    temp_c: float | None = None
    pressure_hpa: float | None = None


@dataclass
class Weather:
    """Hourly series for the cell of a ride, hours as epoch seconds (UTC, start of the hour)."""

    source: str
    lat: float
    lon: float
    times: list[int] = field(default_factory=list)
    speed_ms: list = field(default_factory=list)        # None where the service had no value
    dir_deg: list = field(default_factory=list)
    gust_ms: list = field(default_factory=list)
    temp_c: list = field(default_factory=list)
    pressure_hpa: list = field(default_factory=list)

    def at(self, t: float) -> Sample | None:
        """Interpolated weather at epoch second ``t``; None outside the series (more than
        an hour beyond its ends) or where the wind is unknown. Wind is interpolated as a
        vector, so 350 to 10 degrees does not swing through south."""
        if not self.times or t < self.times[0] - 3600 or t > self.times[-1] + 3600:
            return None
        i = _bisect(self.times, t)
        lo, hi = max(i, 0), min(i + 1, len(self.times) - 1)
        f = 0.0
        if hi != lo:
            if self.times[hi] - self.times[lo] <= 3600:
                f = min(1.0, max(0.0, (t - self.times[lo]) / (self.times[hi] - self.times[lo])))
            else:
                hi = lo                                  # hole in the series: nearest hour only

        def pick(series, a, b):
            va, vb = series[a], series[b]
            if va is None or vb is None:
                return va if vb is None else vb
            return va + f * (vb - va)

        wind = []
        for k in (lo, hi):
            s, d = self.speed_ms[k], self.dir_deg[k]
            wind.append(None if s is None or d is None else
                        (-s * math.sin(math.radians(d)), -s * math.cos(math.radians(d))))
        if wind[0] is None and wind[1] is None:
            return None
        if wind[0] is None or wind[1] is None:
            e, n = wind[0] or wind[1]
        else:
            e = wind[0][0] + f * (wind[1][0] - wind[0][0])
            n = wind[0][1] + f * (wind[1][1] - wind[0][1])
        speed = math.hypot(e, n)
        return Sample(speed_ms=speed, dir_deg=math.degrees(math.atan2(-e, -n)) % 360 if speed else 0.0,
                      gust_ms=pick(self.gust_ms, lo, hi), temp_c=pick(self.temp_c, lo, hi),
                      pressure_hpa=pick(self.pressure_hpa, lo, hi))


def _bisect(times: list[int], t: float) -> int:
    """Index of the last hour that starts at or before t (-1 if none)."""
    lo, hi = 0, len(times)
    while lo < hi:
        mid = (lo + hi) // 2
        if times[mid] <= t:
            lo = mid + 1
        else:
            hi = mid
    return lo - 1


@dataclass
class Loaded:
    """What load() found: the weather, or None with the reason; ``retry_after`` (epoch s) is
    when asking again would be worth it (network was down; data still provisional)."""

    weather: Weather | None
    error: str | None = None
    retry_after: int | None = None


# --- Open-Meteo ---------------------------------------------------------------------

def _epoch(day: datetime.date) -> int:
    return int(datetime.datetime(day.year, day.month, day.day, tzinfo=datetime.timezone.utc).timestamp())


def fetch_days(url: str, lat: float, lon: float, first: datetime.date, last: datetime.date,
               timeout: float = TIMEOUT_S, opener=urllib.request.urlopen) -> dict[datetime.date, dict]:
    """The hourly series of the days first..last (UTC) from one Open-Meteo endpoint:
    {day: {"time": [epoch], "wind_speed_10m": [m/s | None], ...}}. WeatherError on any failure."""
    query = urllib.parse.urlencode({
        "latitude": "%.1f" % lat, "longitude": "%.1f" % lon,
        "start_date": first.isoformat(), "end_date": last.isoformat(),
        "hourly": ",".join(HOURLY), "wind_speed_unit": "ms", "timezone": "GMT"})
    req = urllib.request.Request(url + "?" + query, headers={"User-Agent": "bikelog"})
    try:
        with opener(req, timeout=timeout) as resp:
            data = json.loads(resp.read().decode("utf-8"))
        hourly = data["hourly"]
        stamps = [int(datetime.datetime.strptime(s, "%Y-%m-%dT%H:%M")
                      .replace(tzinfo=datetime.timezone.utc).timestamp()) for s in hourly["time"]]
        series = {name: hourly.get(name) or [None] * len(stamps) for name in HOURLY}
        if any(len(v) != len(stamps) for v in series.values()):
            raise ValueError("series of different length")
    except (urllib.error.URLError, OSError, ValueError, KeyError, TypeError) as exc:
        # URLError and timeouts are OSErrors; a bad answer is ValueError/KeyError/TypeError
        raise WeatherError("%s: %s" % (type(exc).__name__, exc)) from exc
    out: dict[datetime.date, dict] = {}
    for i, ts in enumerate(stamps):
        day = datetime.datetime.fromtimestamp(ts, datetime.timezone.utc).date()
        d = out.setdefault(day, {"time": [], **{name: [] for name in HOURLY}})
        d["time"].append(ts)
        for name in HOURLY:
            v = series[name][i]
            d[name].append(float(v) if isinstance(v, (int, float)) else None)
    return out


def _non_null(day: dict) -> int:
    return sum(1 for s, d in zip(day["wind_speed_10m"], day["wind_direction_10m"])
               if s is not None and d is not None)


def _complete(day: dict) -> bool:
    return _non_null(day) >= 24 and len(day["time"]) >= 24


# --- cache ------------------------------------------------------------------------------

def _path(cache_dir: Path, day: datetime.date, lat: float, lon: float) -> Path:
    return Path(cache_dir) / ("%s_%.1f_%.1f.json" % (day.isoformat(), lat, lon))


def _read(path: Path) -> dict | None:
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        if data["day"]["time"] and isinstance(data["source"], str):
            return data
    except (OSError, ValueError, KeyError, TypeError):
        pass
    return None


def _write(path: Path, entry: dict) -> None:
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp = path.with_name(path.name + ".part")
        tmp.write_text(json.dumps(entry), encoding="utf-8")
        tmp.replace(path)
    except OSError:
        pass                                    # no cache is a slower report, not a failed one


def _final_after(day: datetime.date) -> int:
    """Epoch second from which the archive is expected to have the day for good."""
    return _epoch(day) + 86400 + PROVISIONAL_DAYS * 86400


def load(cache_dir: Path | None, lat: float, lon: float, start_ts: float, end_ts: float, *,
         now: float | None = None, fetch=None) -> Loaded:
    """Weather for a ride around (lat, lon) between two epoch seconds. Reads the day files
    from ``cache_dir`` and fetches what is missing or still provisional (archive first,
    historical forecast for what the archive does not have yet)."""
    global _failed_at
    now = time.time() if now is None else now
    fetch = fetch or fetch_days            # looked up now, so that tests can replace it
    clat, clon = cell(lat, lon)
    utc = datetime.timezone.utc
    first = datetime.datetime.fromtimestamp(start_ts - 3600, utc).date()
    last = datetime.datetime.fromtimestamp(end_ts + 3600, utc).date()
    days = [first + datetime.timedelta(days=n) for n in range((last - first).days + 1)]

    entries: dict[datetime.date, dict] = {}
    todo: list[datetime.date] = []
    for day in days:
        entry = _read(_path(cache_dir, day, clat, clon)) if cache_dir else None
        if entry is not None:
            entries[day] = entry
        if entry is None or (entry.get("provisional") and now >= _final_after(day)):
            todo.append(day)

    error = None
    if todo:
        if _failed_at is not None and time.monotonic() - _failed_at < BACKOFF_S:
            error = "network"
        else:
            try:
                for day, entry in _fetch_missing(fetch, clat, clon, todo, now).items():
                    entries[day] = entry
                    if cache_dir:
                        _write(_path(cache_dir, day, clat, clon), entry)
                _failed_at = None
            except WeatherError:
                _failed_at = time.monotonic()
                error = "network"

    usable = [entries[d] for d in days if d in entries and _non_null(entries[d]["day"])]
    # When asking again is worth it: the network was down; a day is not final yet (empty
    # ones soon, others when the archive has settled)
    again = [int(now) + RETRY_S] if error else []
    for d in days:
        if d in entries and entries[d].get("provisional"):
            again.append(_final_after(d) if _non_null(entries[d]["day"]) else int(now) + RETRY_S)
    retry = min(again) if again else None
    if not usable:
        return Loaded(None, error or "no_data", retry)
    sources = sorted({e["source"] for e in usable})
    w = Weather(source="+".join(sources), lat=clat, lon=clon)
    for e in usable:
        d = e["day"]
        w.times += d["time"]
        w.speed_ms += d["wind_speed_10m"]
        w.dir_deg += d["wind_direction_10m"]
        w.gust_ms += d["wind_gusts_10m"]
        w.temp_c += d["temperature_2m"]
        w.pressure_hpa += d["surface_pressure"]
    return Loaded(w, error, retry)


def _fetch_missing(fetch, lat: float, lon: float, days: list[datetime.date], now: float) -> dict:
    """Fresh cache entries for ``days``: the archive, and for days it has not (fully) got the
    historical forecast; whichever has more data is kept. WeatherError only if the *first*
    request fails -- a failing fallback just leaves the archive's (maybe empty) answer."""
    best: dict[datetime.date, dict] = {}
    for n, (name, url) in enumerate(SOURCES):
        want = [d for d in days if d not in best or not _complete(best[d]["day"])]
        if not want:
            break
        try:
            got = fetch(url, lat, lon, min(want), max(want))
        except WeatherError:
            if n == 0:
                raise
            break
        for day in want:
            if day in got and (day not in best or _non_null(got[day]) > _non_null(best[day]["day"])):
                best[day] = {"day": got[day], "source": name}
    for day, entry in best.items():
        entry["provisional"] = now < _final_after(day)
        entry["fetched"] = int(now)
        entry["lat"], entry["lon"] = lat, lon
    return best
