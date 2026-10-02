# Tools

Offline tools for the bike computer's logs, and helpers for development.

| | |
|---|---|
| `bikelog/` | library and CLI for the binary logs: info, CSV, GPX, raw accelerometer data, simulator feed |
| `BikeLogService/` | service that fetches the sessions from the device and exports them |
| `gpxenrich/` | turns a GPX track into a navigable route for TrailBridge |
| `rqreplay/` | replays raw accelerometer captures through the firmware's road-quality code |
| `uishot.py` | screenshots and touch input on the device over HTTP |
| `eez_export_headless.sh` | EEZ Studio export without the GUI |
| `tests/` | pytest suite |

Documentation: [doc/TOOLS.md](../doc/TOOLS.md) (log format, files on the SD card, CLI) and
[doc/LOGSERVICE.md](../doc/LOGSERVICE.md) (the service), both also in German (`*.de.md`)
and on <https://euphi.github.io/TRGB-BikeComputer/>.
