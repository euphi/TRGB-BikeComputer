"""The one HTML page: sessions, newest first, with their files and exports.

Rendered server-side with plain string formatting -- a table and a status line
do not justify a template engine or a JS build.
"""

from __future__ import annotations

import datetime
from html import escape

from .storage import Session

API = "/api/v1"

CSS = """
:root{color-scheme:light dark;--bg:#f6f6f4;--fg:#1d1d1b;--mut:#6b6b66;--line:#ddd;--acc:#1f6feb;--warn:#b35900}
@media (prefers-color-scheme:dark){:root{--bg:#161615;--fg:#e8e8e4;--mut:#9a9a93;--line:#333;--acc:#58a6ff;--warn:#f0a050}}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 system-ui,sans-serif}
main{max-width:1000px;margin:0 auto;padding:20px 16px}
h1{font-size:1.4rem;margin:0 0 4px}
.mut{color:var(--mut);font-size:.85rem}
table{width:100%;border-collapse:collapse;margin-top:16px}
th,td{text-align:left;padding:7px 6px;border-bottom:1px solid var(--line);vertical-align:top}
th{font-size:.75rem;text-transform:uppercase;letter-spacing:.04em;color:var(--mut)}
td.num{text-align:right;font-variant-numeric:tabular-nums;white-space:nowrap}
a{color:var(--acc);text-decoration:none}
.f a{display:inline-block;margin:0 6px 2px 0;font-size:.8rem}
.warn{color:var(--warn)}
@media (max-width:640px){.hide-s{display:none}}
"""


def _fmt_size(n: int) -> str:
    if n >= 1 << 20:
        return f"{n / (1 << 20):.1f} MB"
    if n >= 1 << 10:
        return f"{n / (1 << 10):.0f} kB"
    return f"{n} B"


def _fmt_dur(seconds) -> str:
    if not seconds:
        return ""
    h, rest = divmod(int(seconds), 3600)
    return f"{h}:{rest // 60:02d} h"


def _when(s: Session) -> str:
    start = s.start_time
    if start:
        return datetime.datetime.fromtimestamp(start).strftime("%Y-%m-%d %H:%M")
    day = s.day
    if len(day) == 8 and day.isdigit():
        day = f"{day[:4]}-{day[4:6]}-{day[6:]}"
        stem = s.stem.lstrip("_")
        if len(stem) == 6 and stem.isdigit():
            return f"{day} {stem[:2]}:{stem[2:4]}"
    return f"{day or '(root)'} #{s.stem.lstrip('_')}"


def _row(s: Session) -> str:
    summ = s.summary or {}
    dist = summ.get("dist_m", s.distance_m)
    move = summ.get("move_s")
    vavg = summ.get("vavg_kmh")
    base = f"{API}/sessions/{s.id}"
    links = []
    if s.file("L"):
        if s.gps_points and s.gpx_status != "no-gps":
            links.append(f'<a href="{base}.gpx">GPX</a>')
        links.append(f'<a href="{base}.csv?with_gps=true">CSV</a>')
    for f in s.files:
        links.append(f'<a href="{base}/files/{escape(f.name)}" title="{escape(f.name)}">'
                     f'{escape(f.name[0])} {_fmt_size(f.size)}</a>')
    notes = []
    if s.log_error:
        notes.append(f'<span class="warn" title="{escape(s.log_error)}">log unreadable</span>')
    if summ.get("time") == "corrected":
        notes.append(f'<span class="mut">time corrected ({escape(str(summ.get("src", "?")))})</span>')
    if s.gpx_status and s.gpx_status.startswith("error"):
        notes.append(f'<span class="warn" title="{escape(s.gpx_status)}">GPX export failed</span>')
    if summ.get("shocks"):
        notes.append(f'<span class="mut">{summ["shocks"]} shocks</span>')
    return (
        f'<tr id="s{s.id}">'
        f"<td>{escape(_when(s))}<br><span class=\"mut\">{escape(s.device)}</span></td>"
        f"<td class=\"num\">{(dist or 0) / 1000:.1f} km</td>"
        f"<td class=\"num hide-s\">{_fmt_dur(move)}</td>"
        f"<td class=\"num hide-s\">{f'{vavg:.1f} km/h' if vavg else ''}</td>"
        f"<td class=\"f\">{''.join(links)}<br>{' · '.join(notes)}</td>"
        "</tr>")


def _pull_line(pull: dict | None) -> str:
    if not pull:
        return '<p class="mut">Pull disabled.</p>'
    parts = []
    for t in pull["targets"]:
        state = "syncing" if t["syncing"] else ("online" if t["online"] else "offline")
        text = f'{escape(t["device"])} ({escape(t["host"])}.local): {state}'
        if t["last_sync_ok"]:
            text += f', last pull {escape(t["last_sync_ok"].replace("T", " ")[:16])} UTC'
        if t["last_error"]:
            text += f' <span class="warn">— {escape(t["last_error"])}</span>'
        parts.append(text)
    return '<p class="mut">' + "<br>".join(parts) + "</p>"


def index(sessions: list[Session], pull: dict | None) -> str:
    rows = "\n".join(_row(s) for s in sessions) or \
        '<tr><td colspan="5" class="mut">No sessions yet.</td></tr>'
    return f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>BikeLog</title><style>{CSS}</style></head>
<body><main>
<h1>BikeLog</h1>
{_pull_line(pull)}
<table><thead><tr><th>Session</th><th>Distance</th><th class="hide-s">Moving</th>
<th class="hide-s">Ø</th><th>Files</th></tr></thead>
<tbody>
{rows}
</tbody></table>
<p class="mut"><a href="/docs">API</a></p>
</main></body></html>
"""
