import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
for path in (TOOLS, TOOLS / "BikeLogService"):
    if str(path) not in sys.path:
        sys.path.insert(0, str(path))


import pytest


@pytest.fixture(autouse=True)
def no_weather_network(monkeypatch):
    """No test asks Open-Meteo: the wind lookup fails like an offline machine (tests of the
    wind code pass their own fetch function)."""
    from bikelog import weather

    def offline(*args, **kwargs):
        raise weather.WeatherError("network blocked in tests")

    monkeypatch.setattr(weather, "fetch_days", offline)
    monkeypatch.setattr(weather, "_failed_at", None)
