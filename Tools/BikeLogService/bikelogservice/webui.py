"""The HTML pages, in the bike computer's Rim & Ridge design.

Rides (list), one ride's report, training, target events, recurring climbs and
the rider data. Rendered server-side with plain string formatting and the SVG
charts of charts.py -- a few pages do not justify a template engine or a JS
build. Colours, fonts and shapes follow doc/design/rim-ridge-design-system.md:
anthracite with one accent (brass), Big Shoulders Display for numbers, IBM Plex
Sans for labels, IBM Plex Mono for units; colour beyond brass only where it
carries meaning (heart-rate zones, road classes, form).
"""

from __future__ import annotations

import datetime
from html import escape

from bikelog import testride

from . import charts
from .storage import Session

API = "/api/v1"

FONTS = ("https://fonts.googleapis.com/css2?family=Big+Shoulders+Display:wght@600;700"
         "&family=IBM+Plex+Mono:wght@400;500&family=IBM+Plex+Sans:wght@400;600&display=swap")

CSS = """
:root{color-scheme:dark;--bg:#161B1F;--panel:#1E252B;--rim:#3A362E;--track:#332F28;--brass:#CBA36B;
 --parch-b:#F3ECDF;--parch:#E7E2D6;--muted:#9BA097;--sage:#7FA08F;--tour:#282019;
 --z1:#6C90B0;--z2:#6FA98C;--z3:#D7B463;--z4:#CE8A4C;--z5:#C1604A;
 --num:'Big Shoulders Display','Arial Narrow',sans-serif;--sans:'IBM Plex Sans',system-ui,sans-serif;
 --mono:'IBM Plex Mono',ui-monospace,monospace}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--parch);font:15px/1.5 var(--sans)}
main{max-width:1040px;margin:0 auto;padding:12px 16px 40px}
header.top{display:flex;flex-wrap:wrap;align-items:center;gap:10px 18px;padding:10px 0 14px;
 border-bottom:1.5px solid var(--rim);margin-bottom:18px}
.brand{font:500 14px var(--mono);letter-spacing:.18em;color:var(--brass);text-transform:uppercase}
.brand b{display:inline-block;width:18px;height:18px;border:1.5px solid var(--brass);border-radius:50%;
 vertical-align:-4px;margin-right:8px;box-shadow:inset 0 0 0 3px var(--bg),inset 0 0 0 5px var(--rim)}
nav{display:flex;flex-wrap:wrap;gap:6px}
nav a{font:600 13px var(--sans);color:var(--parch);padding:5px 14px;border-radius:999px;
 border:1.5px solid rgba(203,163,107,.35);background:var(--panel)}
nav a.on{background:var(--brass);color:var(--bg);border-color:var(--brass)}
h1{font:600 30px/1.1 var(--num);color:var(--parch-b);letter-spacing:.02em;margin:0 0 4px}
h2{font:500 12px var(--mono);letter-spacing:.14em;text-transform:uppercase;color:var(--brass);
 margin:28px 0 10px}
a{color:var(--brass);text-decoration:none}
a:hover{text-decoration:underline}
.mut{color:var(--muted);font-size:.85rem}
.warn{color:var(--z4)}
.panel{background:var(--panel);border:1.5px solid var(--rim);border-radius:14px;padding:14px 16px;margin:10px 0}
.tiles{display:grid;grid-template-columns:repeat(auto-fill,minmax(150px,1fr));gap:10px;margin:14px 0}
.tile{background:var(--panel);border:1.5px solid var(--rim);border-radius:14px;padding:10px 14px}
.tile .l{font:500 11px var(--mono);letter-spacing:.08em;text-transform:uppercase;color:var(--muted)}
.tile .v{font:600 30px/1.1 var(--num);color:var(--parch-b);font-variant-numeric:proportional-nums}
.tile .u{font:400 12px var(--mono);color:var(--muted);margin-left:3px}
.tile .s{font-size:.78rem;color:var(--muted)}
.hero .v{font-size:52px;font-weight:700}
.tile .v.t{font-size:22px;line-height:1.5}
.chips form button{font:500 12px var(--mono);padding:2px 10px;border-radius:999px;border:1px solid rgba(203,163,107,.35);background:none;color:var(--brass);margin:0 6px 4px 0}
table{width:100%;border-collapse:collapse}
th,td{text-align:left;padding:7px 6px;border-bottom:1px solid var(--track);vertical-align:top}
th{font:500 11px var(--mono);text-transform:uppercase;letter-spacing:.06em;color:var(--muted)}
td.num,th.num{text-align:right;font-variant-numeric:tabular-nums;white-space:nowrap}
td .big{font:600 19px var(--num);color:var(--parch-b)}
.chips a,.chips span,.chip{display:inline-block;margin:0 6px 4px 0;font:500 12px var(--mono);
 padding:2px 10px;border-radius:999px;border:1px solid rgba(203,163,107,.35);color:var(--parch)}
.chip.a{background:var(--brass);color:var(--bg);border-color:var(--brass)}
.chip.tour{background:var(--tour)}
.chip.test{background:var(--z4);color:var(--bg);border-color:var(--z4)}
tr.test td{opacity:.75}
.banner.test{border-color:var(--z4)}
form.inline{display:inline;margin:0}
button,.btn{font:600 13px var(--sans);background:var(--panel);color:var(--parch);border:1.5px solid var(--brass);
 border-radius:999px;padding:5px 16px;cursor:pointer}
button.pri,.btn.pri{background:var(--brass);color:var(--bg)}
label.btn{cursor:pointer}
button:disabled{opacity:.4;cursor:not-allowed}
button.link{background:none;border:none;color:var(--brass);padding:0;font:500 12px var(--mono);
 text-decoration:underline}
input,select,textarea{font:inherit;background:var(--bg);color:var(--parch);border:1.5px solid var(--rim);
 border-radius:8px;padding:5px 8px}
input[type=number]{width:7em}
input[type=checkbox]{width:auto;padding:0;accent-color:var(--brass);vertical-align:-2px}
label.f{display:block;margin:8px 0}
label.f span{display:inline-block;min-width:13em;color:var(--muted);font-size:.9rem}
form.filter{margin:6px 0 0;font-size:.9rem}
form.filter input{width:5em}
.chart{width:100%;height:auto;display:block}
.chart .ax{font:400 11px var(--mono);fill:var(--muted)}
.chart .val{font:600 13px var(--num);fill:var(--parch)}
.legend{display:flex;flex-wrap:wrap;gap:4px 16px;font:400 12px var(--mono);color:var(--muted);margin:6px 0}
.legend i{display:inline-block;width:14px;height:4px;border-radius:2px;margin-right:6px;vertical-align:3px}
.zband{display:flex;gap:2px;height:14px;border-radius:7px;overflow:hidden;margin:8px 0 2px}
.meter{height:8px;border-radius:4px;background:var(--track);overflow:hidden;margin:4px 0}
.meter i{display:block;height:100%;background:var(--brass);border-radius:4px}
.find li{margin:4px 0}
.prose p{margin:0 0 10px;line-height:1.55;max-width:46em}
.prose p.mut{margin:6px 0 0}
.find .w::marker{content:"⚠  ";color:var(--z4)}
.find .i::marker{content:"ℹ  ";color:var(--muted)}
.tip{position:fixed;z-index:9;pointer-events:none;display:none;background:var(--panel);color:var(--parch-b);
 border:1.5px solid var(--brass);border-radius:8px;padding:4px 9px;font:400 12px var(--mono);white-space:nowrap}
details summary{cursor:pointer;color:var(--brass);font:500 12px var(--mono);margin:6px 0}
.grid2{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:10px}
.banner{border:1.5px solid var(--brass);border-radius:14px;padding:12px 16px;margin:14px 0;background:var(--tour)}
.banner h2{margin:0 0 8px}
.banner .ask{display:flex;flex-wrap:wrap;gap:8px 14px;align-items:center;padding:8px 0;border-top:1px solid var(--track)}
.banner .ask:first-of-type{border-top:none}
.banner .ask .what{flex:1 1 16em}
.banner form{display:inline;margin:0}
.busy{border-color:var(--z4);background:var(--panel)}
progress{width:100%;height:8px;accent-color:var(--brass)}
.msg{border:1.5px solid var(--rim);border-radius:14px;padding:8px 14px;margin:12px 0;background:var(--panel)}
@media (max-width:640px){.hide-s{display:none}h1{font-size:26px}.hero .v{font-size:44px}}
"""

SCRIPT = """
(()=>{const tip=document.createElement('div');tip.className='tip';document.body.appendChild(tip);
let xh=null;
function off(){tip.style.display='none';if(xh){xh.setAttribute('visibility','hidden');xh=null}}
function on(e){const t=e.target.closest&&e.target.closest('[data-tip]');if(!t){off();return}
 tip.textContent=t.dataset.tip;tip.style.display='block';
 const w=tip.offsetWidth,x=Math.min(e.clientX+14,innerWidth-w-8);tip.style.left=x+'px';
 tip.style.top=Math.max(8,e.clientY-36)+'px';
 const svg=t.ownerSVGElement,line=svg&&svg.querySelector('.xh');
 if(xh&&xh!==line)xh.setAttribute('visibility','hidden');
 if(line&&t.dataset.x){line.setAttribute('x1',t.dataset.x);line.setAttribute('x2',t.dataset.x);
  line.setAttribute('visibility','visible');xh=line}}
addEventListener('pointermove',on);addEventListener('pointerdown',on);addEventListener('scroll',off,{passive:true});
})();
function newGoal(form){const inp=form.querySelector('input[type=file]');const f=inp&&inp.files[0];
 if(!f)return true;
 (async()=>{const r=await fetch('/goals',{method:'POST',headers:{'Accept':'application/json'},
   body:new URLSearchParams(new FormData(form))});
  if(!r.ok){alert('Ziel nicht gespeichert: '+r.status);return}
  const id=(await r.json()).id;
  const u=await fetch('/api/v1/events/'+id+'/gpx',{method:'PUT',body:f});
  if(!u.ok){const j=await u.json().catch(()=>({detail:u.statusText}));alert(j.detail)}
  location.href='/goals#e'+id;location.reload()})();
 return false}
async function importGpx(){const files=document.getElementById('imp-files').files;
 const st=document.getElementById('imp-status');if(!files.length){st.textContent='Keine Datei gewählt.';return}
 const q=new URLSearchParams();const b=document.getElementById('imp-bike').value;
 const e=document.getElementById('imp-event').value;if(b)q.set('bike_id',b);if(e)q.set('event_id',e);
 const errs=[];let n=0;
 for(const f of files){st.textContent=`Importiere ${f.name} …`;
  const r=await fetch('/api/v1/import/gpx?'+q,{method:'PUT',body:f});
  if(r.ok)n++;else{const j=await r.json().catch(()=>({detail:r.statusText}));errs.push(f.name+': '+j.detail)}}
 if(errs.length)alert(errs.join('\n'));st.textContent=`${n} importiert.`;if(n)location.reload()}
async function uploadGpx(input,id){const f=input.files[0];if(!f)return;
 const r=await fetch('/api/v1/events/'+id+'/gpx',{method:'PUT',body:f});
 if(r.ok)location.reload();else{const j=await r.json().catch(()=>({detail:r.statusText}));alert(j.detail)}}
"""

NAV = (("/", "Fahrten"), ("/training", "Training"), ("/goals", "Ziele"), ("/bikes", "Räder"),
       ("/climbs", "Anstiege"), ("/athlete", "Fahrer"), ("/archive", "Archiv"))


def page(title: str, active: str, body: str, head: str = "") -> str:
    nav = "".join(f'<a href="{href}"{" class=on" if href == active else ""}>{name}</a>'
                  for href, name in NAV)
    return f"""<!doctype html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
{head}<title>{escape(title)} · BikeLog</title>
<link rel="preconnect" href="https://fonts.googleapis.com"><link rel="stylesheet" href="{FONTS}">
<style>{CSS}</style></head>
<body><main>
<header class="top"><span class="brand"><b></b>BikeLog</span><nav>{nav}</nav></header>
{body}
<p class="mut" style="margin-top:32px"><a href="/docs">API</a></p>
</main><script>{SCRIPT}</script></body></html>
"""


# --- formatting --------------------------------------------------------------------

def num(value, digits: int = 1) -> str:
    if value is None:
        return "–"
    return f"{value:,.{digits}f}".replace(",", " ").replace(".", ",")


def hm(seconds) -> str:
    if not seconds:
        return "–"
    h, rest = divmod(int(seconds), 3600)
    return f"{h}:{rest // 60:02d}"


def hms(seconds) -> str:
    if seconds is None:
        return "–"
    seconds = int(seconds)
    h, rest = divmod(seconds, 3600)
    return f"{h}:{rest // 60:02d}:{rest % 60:02d}" if h else f"{rest // 60}:{rest % 60:02d}"


def tile(label: str, value: str, unit: str = "", sub: str = "", hero: bool = False,
         text: bool = False) -> str:
    """Stat tile; ``text`` for a word instead of a number (smaller, wraps)."""
    return (f'<div class="tile{" hero" if hero else ""}"><div class="l">{escape(label)}</div>'
            f'<div class="v{" t" if text else ""}">{value}<span class="u">{escape(unit)}</span></div>'
            + (f'<div class="s">{sub}</div>' if sub else "") + "</div>")


def meter(share: float | None, text: str) -> str:
    if share is None:
        return f'<div class="mut">{text}</div>'
    return (f'<div>{text}</div><div class="meter" data-tip="{share * 100:.0f} %">'
            f'<i style="width:{min(1.0, share) * 100:.0f}%"></i></div>')


def _fmt_size(n: int) -> str:
    if n >= 1 << 20:
        return f"{n / (1 << 20):.1f} MB".replace(".", ",")
    if n >= 1 << 10:
        return f"{n / (1 << 10):.0f} kB"
    return f"{n} B"


def _when(s: Session) -> str:
    start = s.start_time
    if start:
        return datetime.datetime.fromtimestamp(start).strftime("%d.%m.%Y %H:%M")
    day = s.day
    if len(day) == 8 and day.isdigit():
        day = f"{day[6:]}.{day[4:6]}.{day[:4]}"
        stem = s.stem.lstrip("_")
        if len(stem) == 6 and stem.isdigit():
            return f"{day} {stem[:2]}:{stem[2:4]}"
    return f"{day or '(root)'} #{s.stem.lstrip('_')}"


# --- rides list ----------------------------------------------------------------------

def _links(s: Session) -> list[str]:
    base = f"{API}/sessions/{s.id}"
    links = []
    if s.file("L"):
        if s.gps_points and s.gpx_status != "no-gps":
            links.append(f'<a href="{base}.gpx">GPX</a>')
        links.append(f'<a href="{base}.csv?with_gps=true">CSV</a>')
    for f in s.files:
        links.append(f'<a href="{base}/files/{escape(f.name)}" title="{escape(f.name)}">'
                     f'{escape(f.name[0])} {_fmt_size(f.size)}</a>')
    if s.gpx_status == "ok" and s.komoot_status not in ("uploaded",):
        links.append(f'<form class="inline" method="post" action="/ui/sessions/{s.id}/komoot" '
                     'onsubmit="return confirm(\'Diese Fahrt jetzt zu Komoot hochladen?\')">'
                     '<button type="submit">Komoot</button></form>')
    return links


def _notes(s: Session) -> list[str]:
    summ = s.summary or {}
    notes = []
    if s.log_error:
        notes.append(f'<span class="warn" title="{escape(s.log_error)}">Log unlesbar</span>')
    if summ.get("time") == "repaired":
        notes.append('<span class="mut" title="Die Uhr wurde während der Fahrt per GPS/NTP umgestellt; '
                     'die Zeitstempel wurden angeglichen">Uhr-Sprung repariert</span>')
    elif summ.get("time") == "corrected":
        notes.append(f'<span class="mut">Zeit korrigiert ({escape(str(summ.get("src", "?")))})</span>')
    if s.gpx_status and s.gpx_status.startswith("error"):
        notes.append(f'<span class="warn" title="{escape(s.gpx_status)}">GPX-Export fehlgeschlagen</span>')
    if s.gpx_status == "debug":
        notes.append('<span class="mut">Debug_Archive (&lt; 1 km)</span>')
    if s.komoot_status == "uploaded":
        notes.append('<span class="mut">Komoot ✓</span>')
    elif s.komoot_status == "error":
        notes.append('<span class="warn">Komoot-Upload fehlgeschlagen</span>')
    if s.nextcloud_status == "ok":
        notes.append('<span class="mut">Nextcloud ✓</span>')
    elif s.nextcloud_status == "error":
        notes.append('<span class="warn">Nextcloud-Sync fehlgeschlagen</span>')
    if summ.get("shocks"):
        notes.append(f'<span class="mut">{summ["shocks"]} Stöße</span>')
    return notes


def test_chip(s: Session) -> str:
    """The marker of a test session: what the log says, or the rider's verdict."""
    if not s.is_test:
        return ""
    label = "Test" if s.test_override == "test" else testride.LABELS.get(s.test_kind, "Test")
    return f'<span class="chip test" title="Testfahrt, zählt nicht im Training">{escape(label)}</span>'


def _row(s: Session, tour: tuple[int, int, int] | None = None, bike: str | None = None) -> str:
    summ = s.summary or {}
    dist = summ.get("dist_m", s.distance_m)
    move = summ.get("move_s")
    vavg = summ.get("vavg_kmh")
    when = escape(_when(s))
    title = f'<a href="/ride/{s.id}">{when}</a>' if s.file("L") else when
    return (
        f'<tr id="s{s.id}"{" class=test" if s.is_test else ""}>'
        f'<td>{title} {test_chip(s)}'
        + (f' <a class="chip tour" href="/tour/{tour[2]}" title="Neustart unterwegs: eine Fahrt aus '
           f'{tour[1]} Sitzungen">Teil {tour[0]}/{tour[1]}</a>' if tour else "")
        + f'<br><span class="mut">{escape(s.device)}{" · " + escape(bike) if bike else ""}</span></td>'
        f'<td class="num"><span class="big">{num((dist or 0) / 1000)}</span> km</td>'
        f'<td class="num hide-s">{hm(move)}</td>'
        f'<td class="num hide-s">{num(vavg) + " km/h" if vavg else ""}</td>'
        f'<td class="chips">{"".join(_links(s))}<br>{" · ".join(_notes(s))}</td>'
        "</tr>")


def _pull_line(pull: dict | None) -> str:
    if not pull:
        return '<p class="mut">Abholen aus (BIKELOG_PULL).</p>'
    parts = []
    for t in pull["targets"]:
        state = "holt ab" if t["syncing"] else ("online" if t["online"] else "offline")
        text = f'{escape(t["device"])} ({escape(t["host"])}.local): {state}'
        if t["last_sync_ok"]:
            text += f', zuletzt {escape(t["last_sync_ok"].replace("T", " ")[:16])} UTC'
            res = t.get("last_result") or {}
            if res.get("fetched") or res.get("replaced"):
                text += f' ({res.get("fetched", 0)} neue, {res.get("replaced", 0)} geänderte Dateien)'
        if t["last_error"]:
            text += f' <span class="warn">— {escape(t["last_error"])}</span>'
        parts.append(text)
    return '<p class="mut">' + "<br>".join(parts) + "</p>"


def _mb(n) -> str:
    return f"{(n or 0) / 1e6:.1f} MB".replace(".", ",")


def _activity(t: dict) -> str:
    """What a running pull is doing, as a box with a progress bar. Empty when idle."""
    act = t.get("activity")
    if not (t.get("syncing") or act):
        return ""
    act = act or {}
    phase = act.get("phase")
    if phase == "downloading":
        total = act.get("bytes_total") or 0
        done = act.get("bytes_done") or 0
        bar = f'<progress max="{total}" value="{done}"></progress>' if total else ""
        text = (f'Lade von {escape(t["device"])}: Datei {act.get("index")}/{act.get("total")} '
                f'<span class="mut">{escape(str(act.get("name", "")))}</span>'
                + (f' · {_mb(done)} von {_mb(total)}' if total else ""))
    elif phase == "processing":
        text = f'Verarbeite die neuen Daten: {escape(str(act.get("detail", "")))} …'
        bar = "<progress></progress>"
    else:
        text = f'Verbinde mit {escape(t["device"])} und lese die Dateiliste …'
        bar = "<progress></progress>"
    return f'<div class="banner busy"><h2>Abruf läuft</h2>{text}{bar}</div>'


def _tour_label(group: list[Session]) -> tuple[str, str]:
    """(when, what) of a merge-group for the question: start, distance, duration."""
    first = group[0]
    dist = sum(((s.summary or {}).get("dist_m", s.distance_m) or 0) for s in group)
    start = min((s.first_time for s in group if s.first_time), default=None)
    end = max((s.last_time for s in group if s.last_time), default=None)
    parts = [f"{num(dist / 1000)} km"] + ([hm(end - start) + " h"] if start and end else [])
    if len(group) > 1:
        parts.append(f"{len(group)} Sitzungen zusammengefasst")
    return _when(first), " · ".join(parts)


def _prompts(groups: list[list[Session]]) -> str:
    """The standing question per finished ride: upload to Komoot? It is shown on every page
    view until answered with one of the two buttons -- leaving the page answers nothing."""
    if not groups:
        return ""
    rows = []
    for group in groups:
        when, what = _tour_label(group)
        sid = next((s.id for s in group if s.gpx_status == "ok"), group[0].id)
        rows.append(
            f'<div class="ask"><div class="what"><strong>{escape(when)}</strong><br>{what}</div>'
            f'<form method="post" action="/ui/sessions/{sid}/komoot"><button class="pri" type="submit">'
            'Zu Komoot hochladen</button></form>'
            f'<form method="post" action="/ui/sessions/{sid}/komoot-ignore"'
            ' onsubmit="return confirm(\'Diese Fahrt nicht zu Komoot hochladen und nicht mehr fragen?\')">'
            '<button type="submit">Nicht hochladen</button></form></div>')
    title = "Neue Fahrt bereit" if len(groups) == 1 else f"{len(groups)} neue Fahrten bereit"
    return f'<div class="banner"><h2>{title} – zu Komoot hochladen?</h2>{"".join(rows)}</div>'


def busy(pull: dict | None) -> bool:
    return bool(pull and any(t.get("syncing") or t.get("activity") for t in pull["targets"]))


def _filter_form(min_km: float | None, max_km: float | None, tests: bool = False,
                 hidden_tests: int = 0, idle: int = 0) -> str:
    min_v = f' value="{min_km:g}"' if min_km is not None else ""
    max_v = f' value="{max_km:g}"' if max_km is not None else ""
    reset = (' <a href="/">zurücksetzen</a>'
             if (min_km is not None or max_km is not None or tests) else "")
    hint = (f' <span class="mut">{hidden_tests} Testfahrt{"en" if hidden_tests != 1 else ""} '
            'ausgeblendet</span>' if hidden_tests else "")
    if idle:
        hint += (f' <a class="mut" href="/archive">{idle} Leerlauf-Sitzung{"en" if idle != 1 else ""} '
                 '(nur Debug-Daten) im Archiv</a>')
    return (
        '<form class="filter" method="get">'
        f'Länge von <input type="number" name="min_km" min="0" step="0.1"{min_v}> '
        f'bis <input type="number" name="max_km" min="0" step="0.1"{max_v}> km '
        f'<label><input type="checkbox" name="tests" value="true"{" checked" if tests else ""} '
        'onchange="this.form.submit()"> Testfahrten zeigen</label> '
        '<button type="submit">Filtern</button>' + reset + hint + '</form>')


def _bike_options(registry, selected: str | None = None, empty: str = "–") -> str:
    opts = [f'<option value="">{escape(empty)}</option>']
    for b in (registry.bikes if registry else []):
        opts.append(f'<option value="{escape(b.id)}"{" selected" if b.id == selected else ""}>'
                    f'{escape(b.name)}</option>')
    return "".join(opts)


def _import_form(registry, events) -> str:
    """Rides recorded elsewhere, as GPX: uploaded one by one by the page's script."""
    ev = "".join(f'<option value="{escape(e.id)}">{escape(e.name)}</option>' for e in events)
    return ('<details style="margin-top:8px"><summary>GPX-Fahrten importieren</summary><div class="panel">'
            '<p class="mut">Aufgezeichnete Fahrten von Garmin, Strava, Komoot &amp; Co. (GPX mit Zeiten, '
            'Puls und Trittfrequenz werden übernommen). Sie zählen wie Fahrten des Fahrradcomputers, '
            'werden aber nicht exportiert oder hochgeladen.</p>'
            '<label class="f"><span>Dateien</span><input type="file" id="imp-files" multiple '
            'accept=".gpx,application/gpx+xml"></label>'
            f'<label class="f"><span>Rad</span><select id="imp-bike">{_bike_options(registry)}</select></label>'
            '<label class="f"><span>Frühere Teilnahme an</span><select id="imp-event">'
            f'<option value="">–</option>{ev}</select> <span class="mut">optional, z. B. das Rennen '
            'vom letzten Jahr</span></label>'
            '<button class="pri" type="button" onclick="importGpx()">Importieren</button> '
            '<span id="imp-status" class="mut"></span></div></details>')


def index(sessions: list[Session], pull: dict | None,
          min_km: float | None = None, max_km: float | None = None,
          prompts: list[list[Session]] | None = None, message: str | None = None,
          tests: bool = False, hidden_tests: int = 0, idle: int = 0,
          tours: dict[int, tuple[int, int, int]] | None = None,
          bike_names: dict[int, str] | None = None, registry=None, events=None) -> str:
    """``tours``: session id -> (part, of parts, first session id) for rides split by a reboot;
    ``bike_names``: session id -> bike; ``registry``/``events`` for the import form."""
    tours, bike_names = tours or {}, bike_names or {}
    rows = "\n".join(_row(s, tours.get(s.id), bike_names.get(s.id)) for s in sessions) or \
        '<tr><td colspan="5" class="mut">Keine Fahrten für diesen Filter.</td></tr>'
    working = "".join(_activity(t) for t in (pull or {}).get("targets", []))
    # While something is being fetched or processed the page reloads itself, so the progress
    # (and, at the end, the question about the new ride) shows up without a click.
    refresh = '<meta http-equiv="refresh" content="3">' if busy(pull) else ""
    note = f'<div class="msg">{escape(message)}</div>' if message else ""
    body = f"""<h1>Fahrten</h1>
{note}
{working}
{_prompts(prompts or [])}
{_pull_line(pull)}
{_filter_form(min_km, max_km, tests, hidden_tests, idle)}
{_import_form(registry, events or [])}
<table style="margin-top:14px"><thead><tr><th>Sitzung</th><th class="num">Strecke</th>
<th class="num hide-s">Fahrzeit</th><th class="num hide-s">Ø</th><th>Dateien</th></tr></thead>
<tbody>
{rows}
</tbody></table>"""
    return page("Fahrten", "/", body, head=refresh)


# --- one ride ----------------------------------------------------------------------------

ROAD_COLOURS = {"glatt": charts.ZONES[0], "gut": charts.ZONES[1], "mäßig": charts.ZONES[2],
                "rau": charts.ZONES[3], "sehr rau": charts.ZONES[4]}


def _climb_rows(climbs: list[dict]) -> str:
    rows = []
    for c in climbs:
        rows.append(
            f'<tr><td><span class="chip{" a" if c["category"] in ("1", "2", "HC") else ""}">'
            f'{escape(c["category"] or "–")}</span></td>'
            f'<td class="num">{num(c["start_km"])}</td><td class="num">{num(c["length_m"] / 1000, 2)} km</td>'
            f'<td class="num">{c["gain_m"]} m</td>'
            f'<td class="num">{num(c["avg_grade_pct"])} / {num(c["max_grade_100m_pct"])} %</td>'
            f'<td class="num">{hms(c["duration_s"])}</td><td class="num hide-s">{c["vam_m_h"] or "–"}</td>'
            f'<td class="num hide-s">{c["avg_hr"] or "–"}</td>'
            f'<td class="num hide-s">{c["est_avg_power_w"] or "–"} W</td></tr>')
    return ("<table><thead><tr><th>Kat.</th><th class=num>ab km</th><th class=num>Länge</th>"
            "<th class=num>Höhe</th><th class=num>Ø / max.</th><th class=num>Zeit</th>"
            "<th class='num hide-s'>VAM</th><th class='num hide-s'>Puls</th>"
            "<th class='num hide-s'>Leistung*</th></tr></thead><tbody>" + "".join(rows) + "</tbody></table>")


def _test_button(s: Session, mark: str, label: str, primary: bool = False) -> str:
    return (f'<form method="post" action="/ui/sessions/{s.id}/test"><input type="hidden" name="mark" '
            f'value="{mark}"><button{" class=pri" if primary else ""} type="submit">{label}</button></form>')


def _test_banner(s: Session, rep: dict) -> str:
    """Why this session counts as a test (or not), with the buttons to overrule it."""
    detected = (rep.get("meta") or {}).get("test") or {}
    reason = escape(detected.get("reason") or "")
    if s.test_override == "test":
        return ('<div class="banner test"><h2>Als Testfahrt markiert</h2>'
                '<p>Von dir markiert — zählt nicht im Training, kein Upload.</p>'
                '<div class="ask">' + _test_button(s, "auto", "Markierung aufheben") + "</div></div>")
    if s.test_override == "real":
        return ('<p class="mut">Von dir als echte Fahrt bestätigt'
                + (f" (erkannt war: {reason})" if reason else "") + ". "
                + _test_button(s, "auto", "Bestätigung aufheben").replace("<button", '<button class="link"')
                + "</p>")
    if s.is_test:
        return ('<div class="banner test"><h2>Testfahrt</h2>'
                f'<p>{reason or "Laut Log keine echte Fahrt"}. Zählt nicht im Training, landet in '
                'Debug_Archive und wird nicht hochgeladen.</p>'
                '<div class="ask">' + _test_button(s, "real", "Doch eine echte Fahrt") + "</div></div>")
    return ('<p class="mut">' + _test_button(s, "test", "Als Testfahrt markieren")
            .replace("<button", '<button class="link"') + "</p>")


def _tour_banner(s: Session, group: list[Session]) -> str:
    """On a session page: this is part n of a ride the bike computer rebooted in."""
    n = next(i for i, x in enumerate(group) if x.id == s.id) + 1
    return (f'<div class="banner"><h2>Teil {n} von {len(group)} einer Fahrt</h2>'
            '<p>Der Fahrradcomputer ist unterwegs neu gestartet; die Sitzungen liegen nur kurz '
            'auseinander und zählen im Training als eine Fahrt.</p>'
            f'<div class="ask"><a class="btn" href="/tour/{group[0].id}">Bericht der ganzen Fahrt</a></div></div>')


def _bike_form(s: Session, registry, bike) -> str:
    """The bike of this ride: the device's (Räder page) unless chosen here."""
    if registry is None or not registry.bikes:
        return ('<p class="mut">Kein Rad zugeordnet — unter <a href="/bikes">Räder</a> anlegen und dem '
                'Fahrradcomputer zuordnen (Gewicht und Aerodynamik für die Leistungsschätzung).</p>')
    own = " (nur diese Fahrt)" if s.bike_id else ""
    return (f'<form class="inline" method="post" action="/ui/sessions/{s.id}/bike"><span class="mut">Rad: '
            f'</span><b>{escape(bike.name) if bike else "–"}</b>{own} '
            f'<select name="bike_id" onchange="this.form.submit()">'
            f'{_bike_options(registry, s.bike_id, "wie das Gerät")}</select></form>')


def _narrative(s: Session, n: dict | None) -> str:
    """The ride in words from the local LLM (llm.py) -- or why there is none yet."""
    if n is None:
        return ""
    again = (f'<form class="inline" method="post" action="/ui/narrative/{s.id}">'
             '<button class="link">{}</button></form>')
    stored = n["text"]
    if stored is None:
        if n["busy"]:
            note = f'{escape(n["model"])} schreibt gerade den Text zu dieser Fahrt (dauert einige Minuten).'
        elif n["error"]:
            note = f'Noch kein Text: {escape(n["error"])}. ' + again.format("Nochmal versuchen")
        elif n["working"]:
            note = f'Text kommt — {escape(n["model"])} schreibt gerade einen anderen. ' + again.format("Diesen zuerst")
        else:
            note = "Noch kein Text. " + again.format("Jetzt schreiben")
        return f'<h2>In Worten</h2><div class="panel"><p class="mut">{note}</p></div>'
    paras = "".join(f"<p>{escape(p.strip())}</p>" for p in stored["text"].split("\n\n") if p.strip())
    when = stored["created_at"][:16].replace("T", " ")
    meta = (f'Geschrieben von {escape(stored["model"])} aus den Zahlen dieser Seite ({escape(when)}, '
            f'{round(stored["seconds"] or 0)} s). ')
    if stored["unverified"]:
        meta += ('<span class="warn">Zahlen, die nicht in den Daten stehen: '
                 + ", ".join(escape(x) for x in stored["unverified"]) + "</span>. ")
    meta += ("Wird neu geschrieben…" if n["busy"] else again.format("Neu schreiben"))
    return f'<h2>In Worten</h2><div class="panel prose">{paras}<p class="mut">{meta}</p></div>'


def ride_page(s: Session, rep: dict, group: list[Session] | None = None, tour: bool = False,
              registry=None, bike=None, narrative: dict | None = None) -> str:
    """One session's report -- or, with ``tour``, the report of the whole ride ``group``."""
    group = group or [s]
    if tour:
        parts = "".join(f'<a href="/ride/{x.id}">Teil {i + 1}: {escape(_when(x))}</a>'
                        for i, x in enumerate(group))
        head = (f'<h1>Fahrt {escape(_when(s))}</h1><p class="mut">{len(group)} Sitzungen, '
                'zusammengefasst (Neustart unterwegs)</p>' f'<div class="chips">{parts}</div>')
    else:
        base = f"{API}/sessions/{s.id}"
        links = _links(s) + [f'<a href="{base}/report.md">Bericht (Markdown)</a>',
                             f'<a href="{base}/report.json">JSON</a>']
        head = (f'<h1>{escape(_when(s))} {test_chip(s)}</h1><div class="chips">{"".join(links)}</div>'
                + _bike_form(s, registry, bike)
                + _test_banner(s, rep) + (_tour_banner(s, group) if len(group) > 1 else ""))
    if rep.get("empty"):
        return page("Fahrt", "/", head + '<p class="mut">Leeres Log.</p>')
    ride, heart, power, road, health = (rep[k] for k in ("ride", "heart", "power", "road", "health"))
    out = [head]
    tiles = [tile("Strecke", num(ride["distance_km"]), "km", hero=True),
             tile("Fahrzeit", hm(ride["moving_s"]), "h", f'gesamt {hm(ride["duration_s"])}'),
             tile("Ø", num(ride["avg_moving_kmh"]), "km/h", f'max. {num(ride["max_kmh"])}'),
             tile("Höhenmeter", str(ride["ascent_m"]), "m", f'{ride["min_ele_m"]}–{ride["max_ele_m"]} m')]
    if heart["avg"]:
        tiles.append(tile("Puls", str(heart["avg"]), "bpm", f'max. {heart["max"]}'))
    if heart["trimp"] is not None:
        tiles.append(tile("TRIMP", str(heart["trimp"]), "", escape(heart["trimp_method"])))
    if power.get("normalized_w"):
        tiles.append(tile("Leistung*", str(power["normalized_w"]), "W", f'Ø {power["avg_moving_w"]} W, NP'))
    if ride["stops"]["count"]:
        tiles.append(tile("Stopps", str(ride["stops"]["count"]), "", hm(ride["stops"]["total_s"]) + " h"))
    out.append('<div class="tiles">' + "".join(tiles) + "</div>")
    out.append(_narrative(s, narrative))

    if rep.get("profile"):
        out.append("<h2>Höhenprofil</h2>")
        out.append('<div class="panel">' + charts.profile(rep["profile"], "Höhenprofil", rep["climbs"])
                   + (charts.legend([("Höhe (Baro)", charts.SAGE), ("Anstieg", charts.BRASS)])
                      if rep["climbs"] else "") + "</div>")
    if rep["climbs"]:
        out.append("<h2>Anstiege</h2>" + '<div class="panel">' + _climb_rows(rep["climbs"]) + "</div>")

    out.append("<h2>Puls und Belastung</h2>")
    parts = []
    if heart["zones_s"]:
        parts.append(f'<div class="panel"><div class="mut">Zeit in den Pulszonen (Grenzen '
                     f'{" / ".join(str(b) for b in heart.get("zones_bpm", []))} bpm)</div>'
                     + charts.zone_band(heart["zones_s"]) + "</div>")
    elif heart["avg"]:
        parts.append('<div class="panel mut">Für Pulszonen und TRIMP die maximale Herzfrequenz '
                     'unter <a href="/athlete">Fahrer</a> eintragen.</div>')
    facts = []
    if heart["avg"]:
        facts.append(f'Puls-Abdeckung: {num((heart["coverage_moving"] or 0) * 100, 0)} % der Fahrzeit')
    if heart["decoupling_pct"] is not None:
        facts.append(f'Aerobe Entkopplung: {num(heart["decoupling_pct"])} % '
                     '<span class="mut">(Leistung/Puls, 1. gegen 2. Hälfte; unter 5 % gilt als gute Grundlage)</span>')
    if heart["efficiency_w_per_bpm"]:
        facts.append(f'Effizienz: {num(heart["efficiency_w_per_bpm"], 2)} W/bpm')
    if power.get("best_w"):
        b, wkg = power["best_w"], power.get("best_w_per_kg") or {}
        facts.append("Bestwerte*: " + ", ".join(
            f'{k} {v} W' + (f' ({num(wkg.get(k), 2)} W/kg)' if wkg.get(k) else "")
            for k, v in b.items() if v))
        facts.append(f'Arbeit*: {power["work_kj"]} kJ')
    if facts:
        parts.append('<div class="panel"><ul class="find">' + "".join(f"<li>{f}</li>" for f in facts)
                     + "</ul></div>")
    out.append("".join(parts) or '<p class="mut">Kein Puls aufgezeichnet.</p>')
    if power.get("avg_moving_w") is not None:
        m = power["model"]
        out.append(f'<p class="mut">* Leistung geschätzt aus Tempo, Steigung und {num(m["mass_kg"], 0)} kg '
                   f'Systemgewicht (CdA {num(m["cda"], 2)}, Crr {num(m["crr"], 3)}), ohne Wind — für '
                   'Vergleiche zwischen Fahrten, kein Messwert.</p>')

    if road["intervals"]:
        out.append("<h2>Wege</h2><div class='panel'>")
        share = road["class_share_rated"]
        if share:
            cells = "".join(f'<span style="flex:{share[k]:.4f};background:{c}" '
                            f'data-tip="{escape(k)}: {share[k] * 100:.0f} %"></span>'
                            for k, c in ROAD_COLOURS.items() if share.get(k))
            out.append('<div class="mut">Wegeklassen (Beschleunigungssensor)'
                       + (" — ohne Referenzfahrt" if road["uncalibrated"] else "") + "</div>"
                       f'<div class="zband">{cells}</div>'
                       + charts.legend([(f"{k} {share[k] * 100:.0f} %", colour)
                                        for k, colour in ROAD_COLOURS.items() if share.get(k)]))
        if road["labels"]:
            out.append("<table><thead><tr><th>Label</th><th class=num>km</th><th class=num>Rauheit</th>"
                       "<th class=num>Klassen 1–5</th></tr></thead><tbody>" + "".join(
                           f'<tr><td>{escape(lab["surface"] or "–")} Q{lab["quality"] or "–"}</td>'
                           f'<td class=num>{num(lab["km"], 2)}</td><td class=num>{num(lab["roughness_median"], 2)}</td>'
                           f'<td class=num>{"/".join(str(n) for n in lab["classes_1_5"])}</td></tr>'
                           for lab in road["labels"]) + "</tbody></table>")
        sh = road["shocks"]
        out.append(f'<p>Stöße: {sh["count"]} (Schwere 1/2/3: '
                   + "/".join(str(sh["by_severity"][k]) for k in ("1", "2", "3")) + ")"
                   + (f', {sh["suppressed"]} wegen Ratenlimit nicht geloggt' if sh["suppressed"] else "")
                   + "</p>")
        if sh["top"]:
            out.append("<table><thead><tr><th class=num>km</th><th class=num>g</th><th class=num>km/h</th>"
                       "<th>Ort</th></tr></thead><tbody>" + "".join(
                           f'<tr><td class=num>{num(x["km"])}</td><td class=num>{num(x["g"])}</td>'
                           f'<td class=num>{num(x["kmh"])}</td><td>'
                           + (f'<a href="https://www.openstreetmap.org/?mlat={x["lat"]}&mlon={x["lon"]}#map=18/'
                              f'{x["lat"]}/{x["lon"]}">Karte</a>' if x["lat"] is not None else "–")
                           + (" · beide Räder" if x["both_wheels"] else "") + "</td></tr>"
                           for x in sh["top"]) + "</tbody></table>")
        out.append("</div>")

    out.append("<h2>Technik</h2><div class='panel'><ul class='find'>")
    for f in health["findings"]:
        out.append(f'<li class="{"w" if f["level"] == "warn" else "i"}">{escape(f["text"])}</li>')
    if not health["findings"]:
        out.append('<li class="i">Keine Auffälligkeiten.</li>')
    gps = health["gps"]
    out.append(f'<li class="i">GPS: {num(gps["fresh_share"] * 100, 0)} % der Datensätze mit frischem Fix, '
               f'Genauigkeit Median {num(gps["accuracy_m_median"])} m</li>')
    if health.get("baro_vs_gps"):
        b = health["baro_vs_gps"]
        out.append(f'<li class="i">Baro gegen GPS-Höhe: Versatz {b["offset_m"]:+d} m, Drift {b["drift_m"]:+d} m</li>')
    out.append("</ul></div>")
    return page(("Fahrt " if tour else "Sitzung ") + _when(s), "/", "\n".join(out))


# --- training ----------------------------------------------------------------------------

MONTHS = ("Jan", "Feb", "Mär", "Apr", "Mai", "Jun", "Jul", "Aug", "Sep", "Okt", "Nov", "Dez")


def _date_ticks(days: list[datetime.date]) -> dict[int, str]:
    """A tick on the first of every month (every second one for long spans)."""
    ticks = {i: MONTHS[d.month - 1] for i, d in enumerate(days) if d.day == 1}
    if len(ticks) > 8:
        ticks = dict(list(ticks.items())[::2])
    return ticks


def training_page(load: list, weeks: list, rides_total: int, has_hr_max: bool,
                  without_hr: int) -> str:
    out = ["<h1>Training</h1>"]
    if not has_hr_max:
        out.append('<p class="warn">Ohne maximale Herzfrequenz gibt es keinen TRIMP und damit keine '
                   'Trainingslast — unter <a href="/athlete">Fahrer</a> eintragen.</p>')
    if not rides_total:
        out.append('<p class="mut">Noch keine Fahrten (ab 1 km, ohne Simulator).</p>')
        return page("Training", "/training", "\n".join(out))
    now = load[-1]
    cur, last4 = weeks[-1], weeks[-4:]
    out.append('<div class="tiles">'
               + tile("Fitness", num(now.ctl, 0), "CTL", "42-Tage-Mittel des TRIMP", hero=True)
               + tile("Ermüdung", num(now.atl, 0), "ATL", "7-Tage-Mittel")
               + tile("Form", f"{now.tsb:+.0f}", "TSB", "positiv = erholt")
               + tile("Diese Woche", num(cur.distance_km, 0), "km", f"{hm(cur.moving_s)} h · {cur.ascent_m:.0f} m")
               + tile("4 Wochen", num(sum(w.distance_km for w in last4), 0), "km",
                      f"{hm(sum(w.moving_s for w in last4))} h · {sum(w.ascent_m for w in last4):.0f} m")
               + "</div>")
    if without_hr:
        out.append(f'<p class="mut">{without_hr} Fahrt(en) ohne Puls zählen in der Trainingslast als 0.</p>')

    days = [p.day for p in load]
    tips = [f"{p.day:%d.%m.}  Fitness {p.ctl:.0f} · Ermüdung {p.atl:.0f} · Form {p.tsb:+.0f}"
            + (f" · TRIMP {p.trimp:.0f}" if p.trimp else "") for p in load]
    out.append("<h2>Fitness und Ermüdung</h2><div class='panel'>"
               + charts.legend([("Fitness (CTL)", charts.BRASS), ("Ermüdung (ATL)", charts.ZONES[0])])
               + charts.line_chart([str(d) for d in days],
                                   [("Fitness", charts.BRASS, [p.ctl for p in load]),
                                    ("Ermüdung", charts.ZONES[0], [p.atl for p in load])],
                                   tips, "Fitness und Ermüdung", _date_ticks(days))
               + "<div class='mut' style='margin-top:10px'>Form (TSB) = Fitness − Ermüdung des Vortags</div>"
               + charts.diverging_bars([p.tsb for p in load],
                                       [f"{p.day:%d.%m.}  Form {p.tsb:+.0f}" for p in load], "Form",
                                       _date_ticks(days))
               + charts.legend([("erholt", charts.FRESH), ("ermüdet", charts.TIRED)])
               + "</div>")

    # every second week labelled -- 16 dates do not fit side by side on a phone
    labels = [f"{w.monday:%d.%m.}" if (len(weeks) - 1 - i) % 2 == 0 else "" for i, w in enumerate(weeks)]
    wtips = [f"Woche ab {w.monday:%d.%m.}: {w.distance_km:.0f} km, {hm(w.moving_s)} h, "
             f"{w.ascent_m:.0f} m, TRIMP {w.trimp:.0f}" for w in weeks]
    kms = [round(w.distance_km) for w in weeks]
    mark = {len(kms) - 1, max(range(len(kms)), key=lambda i: kms[i])}
    out.append("<h2>Wochen</h2><div class='panel'><div class='mut'>Kilometer je Woche</div>"
               + charts.columns(labels, kms, wtips, "Kilometer je Woche", mark=mark) + "</div>")
    if any(w.zones_s for w in weeks):
        names = ["Z1", "Z2", "Z3", "Z4", "Z5"]
        zones = [[w.zones_s.get(z, 0) for z in names] for w in weeks]
        ztips = [f"Woche ab {w.monday:%d.%m.}: " + ", ".join(
            f"{z} {w.zones_s.get(z, 0) / 3600:.1f} h".replace(".", ",") for z in names) for w in weeks]
        out.append("<div class='panel'><div class='mut'>Stunden je Pulszone</div>"
                   + charts.stacked_zones(labels, zones, ztips, "Stunden je Pulszone")
                   + charts.legend(list(zip(names, charts.ZONES))) + "</div>")
    rows = "".join(
        f'<tr><td>{w.monday:%d.%m.%Y}</td><td class=num>{w.rides}</td><td class=num>{num(w.distance_km, 0)}</td>'
        f'<td class=num>{hm(w.moving_s)}</td><td class=num>{w.ascent_m:.0f}</td>'
        f'<td class=num>{w.trimp:.0f}</td><td class="num hide-s">{num(w.longest_km, 0)}</td>'
        f'<td class="num hide-s">{w.best_20min_w or "–"}</td></tr>' for w in reversed(weeks))
    out.append("<details><summary>Tabelle</summary><table><thead><tr><th>Woche ab</th>"
               "<th class=num>Fahrten</th><th class=num>km</th><th class=num>h</th><th class=num>Hm</th>"
               "<th class=num>TRIMP</th><th class='num hide-s'>Längste</th><th class='num hide-s'>20 min*</th>"
               f"</tr></thead><tbody>{rows}</tbody></table>"
               "<p class='mut'>* geschätzte Leistung, beste 20 Minuten der Woche</p></details>")
    return page("Training", "/training", "\n".join(out))


# --- goals ------------------------------------------------------------------------------

def _event_form(e=None) -> str:
    """Create (e None) or edit an event. The new-event form takes the GPX too: the script
    creates the event, then uploads the file to it."""
    v = (lambda attr, default="": escape(str(getattr(e, attr) if e and getattr(e, attr) is not None
                                             else default)))
    prio = e.priority if e else "A"
    opts = "".join(f'<option value="{p}"{" selected" if p == prio else ""}>{p}</option>' for p in "ABC")
    submit = "" if e else ' onsubmit="return newGoal(this)"'
    return (f'<form method="post" action="/goals"{submit}>'
            + (f'<input type="hidden" name="id" value="{v("id")}">' if e else "")
            + f'<label class="f"><span>Name</span><input name="name" required value="{v("name")}"></label>'
            f'<label class="f"><span>Datum</span><input type="date" name="date" required value="{v("date")}"></label>'
            f'<label class="f"><span>Priorität</span><select name="priority">{opts}</select> '
            '<span class="mut">A = Saisonziel, B = wichtig, C = Trainingsrennen</span></label>'
            f'<label class="f"><span>Distanz (km)</span><input type="number" step="0.1" name="distance_km" '
            f'value="{v("distance_km")}"> <span class="mut">leer = aus der GPX</span></label>'
            f'<label class="f"><span>Höhenmeter</span><input type="number" step="1" name="ascent_m" '
            f'value="{v("ascent_m")}"></label>'
            f'<label class="f"><span>Notiz</span><input name="notes" style="width:min(30em,100%)" '
            f'value="{v("notes")}"></label>'
            + ('' if e else '<label class="f"><span>Strecke als GPX</span><input type="file" '
               'accept=".gpx,application/gpx+xml"> <span class="mut">optional, geht auch später</span></label>')
            + '<button class="pri" type="submit">Speichern</button></form>')


def _participations(e, parts) -> str:
    rows = []
    for s, rep in sorted(parts, key=lambda p: p[0].first_time or 0):
        if rep.get("empty"):
            continue
        ride, heart, power = rep["ride"], rep["heart"], rep["power"]
        rows.append(
            f'<tr><td><a href="/ride/{s.id}">{escape(_when(s)[:10])}</a></td>'
            f'<td class=num>{num(ride["distance_km"], 1)} km</td>'
            f'<td class=num><span class="big">{hms(ride["duration_s"])}</span></td>'
            f'<td class="num hide-s">{hms(ride["moving_s"])}</td>'
            f'<td class=num>{num(ride["avg_moving_kmh"])}</td>'
            f'<td class="num hide-s">{ride["ascent_m"]} m</td>'
            f'<td class=num>{heart["avg"] or "–"}</td>'
            f'<td class="num hide-s">{power.get("normalized_w") or "–"}</td>'
            f'<td class="num hide-s">{heart["trimp"] if heart["trimp"] is not None else "–"}</td>'
            f'<td><form class="inline" method="post" action="/goals/{escape(e.id)}/participations/{s.id}/delete">'
            '<button class="link" type="submit" title="nur die Verknüpfung, die Fahrt bleibt">entfernen</button>'
            '</form></td></tr>')
    return ("<table><thead><tr><th>Datum</th><th class=num>Strecke</th><th class=num>Gesamtzeit</th>"
            "<th class='num hide-s'>Fahrzeit</th><th class=num>Ø km/h</th><th class='num hide-s'>Hm</th>"
            "<th class=num>Puls Ø</th><th class='num hide-s'>NP*</th><th class='num hide-s'>TRIMP</th><th></th>"
            "</tr></thead><tbody>" + "".join(rows) + "</tbody></table>")


def _gpx_button(event_id: str, label: str, primary: bool = False) -> str:
    """A button that opens the file dialog and uploads the GPX straight away."""
    return (f'<label class="btn{" pri" if primary else ""}" style="display:inline-block">{escape(label)}'
            f'<input type="file" accept=".gpx,application/gpx+xml" style="display:none" '
            f'onchange="uploadGpx(this,\'{escape(event_id)}\')"></label>')


def goals_page(items: list[tuple], today: datetime.date) -> str:
    out = ["<h1>Ziele</h1>"]
    if not items:
        out.append('<p class="mut">Noch kein Ziel eingetragen.</p>')
    for e, r, parts in items:
        past = r["days_left"] < 0
        out.append(f'<div class="panel" id="e{escape(e.id)}">')
        out.append(f'<div style="display:flex;flex-wrap:wrap;gap:6px 14px;align-items:baseline">'
                   f'<h1 style="margin:0">{escape(e.name)}</h1><span class="chip{" a" if e.priority == "A" else " tour"}">'
                   f'{escape(e.priority)}</span><span class="mut">{e.date:%d.%m.%Y}</span></div>')
        if e.notes:
            out.append(f'<p class="mut">{escape(e.notes)}</p>')
        tiles = [tile("Noch", str(max(0, r["days_left"])), "Tage", f'{num(r["weeks_left"])} Wochen', hero=True),
                 tile("Phase", escape(r["phase"]), "", text=True)]
        if e.course_km:
            tiles.append(tile("Strecke", num(e.course_km, 0), "km"))
        if e.course_ascent_m:
            tiles.append(tile("Höhenmeter", f"{e.course_ascent_m:.0f}", "m"))
        tiles.append(tile("Fitness", num(r["ctl"], 0), "CTL", f'Form {r["tsb"]:+.0f}'))
        out.append('<div class="tiles">' + "".join(tiles) + "</div>")
        if not past:
            out.append(f'<p>{escape(r["phase_text"])}</p>')
            out.append('<div class="grid2"><div>' + meter(
                r["longest_share"],
                f'Längste Fahrt der letzten 6 Wochen: <b>{num(r["longest_km"], 0)} km</b>'
                + (f' = {r["longest_share"] * 100:.0f} % der Renndistanz' if r["longest_share"] is not None else ""))
                + "</div><div>" + meter(
                r["week_ascent_share"],
                f'Meiste Höhenmeter in einer Woche: <b>{r["max_week_ascent_m"]} m</b>'
                + (f' = {r["week_ascent_share"] * 100:.0f} % der Rennhöhenmeter'
                   if r["week_ascent_share"] is not None else "")) + "</div></div>")
            if r["rides_recent_without_hr"]:
                out.append(f'<p class="mut">{r["rides_recent_without_hr"]} der {r["rides_recent"]} Fahrten der '
                           'letzten 6 Wochen ohne Puls.</p>')
        prof = e.profile or {}
        if prof.get("profile"):
            out.append("<h2>Strecke</h2>" + charts.profile(prof["profile"], "Höhenprofil " + e.name,
                                                             prof.get("climbs"))
                       + f'<p>{_gpx_button(e.id, "Andere Strecke (GPX) hochladen")}</p>')
        else:
            out.append('<div class="banner"><h2>Noch keine Strecke</h2><p>Mit der Strecke als GPX '
                       '(mit Höhen) kommen Distanz, Höhenmeter, Höhenprofil und die Anstiege mit deiner '
                       'geschätzten Zeit dazu.</p><div class="ask">'
                       + _gpx_button(e.id, "Strecke (GPX) hochladen", primary=True) + "</div></div>")
        if r["climbs"]:
            out.append("<table><thead><tr><th>Kat.</th><th class=num>ab km</th><th class=num>Länge</th>"
                       "<th class=num>Höhe</th><th class=num>Ø %</th><th class=num>deine Zeit*</th></tr></thead><tbody>"
                       + "".join(f'<tr><td><span class="chip">{escape(c["category"])}</span></td>'
                                 f'<td class=num>{num(c["start_km"])}</td><td class=num>{num(c["length_m"] / 1000, 1)} km</td>'
                                 f'<td class=num>{c["gain_m"]} m</td><td class=num>{num(c["avg_grade_pct"])}</td>'
                                 f'<td class=num>{hms(c["est_duration_s"])}</td></tr>' for c in r["climbs"])
                       + "</tbody></table><p class='mut'>* aus deiner besten VAM auf vergleichbaren Anstiegen "
                       "(mindestens halbe Höhe) der letzten 90 Tage — frisch gefahren, nicht nach 100 km.</p>")
        if parts:
            out.append("<h2>Deine bisherigen Teilnahmen</h2>" + _participations(e, parts))
        out.append(f'<details><summary>Bearbeiten</summary>{_event_form(e)}'
                   f'<form method="post" action="/goals/{escape(e.id)}/delete" '
                   'onsubmit="return confirm(\'Ziel löschen?\')"><button type="submit">Löschen</button></form>'
                   "</details></div>")
    out.append("<h2>Neues Ziel</h2><div class='panel'>" + _event_form() + "</div>")
    return page("Ziele", "/goals", "\n".join(out))


# --- recurring climbs ---------------------------------------------------------------------

def climbs_page(groups: list[dict]) -> str:
    out = ["<h1>Anstiege</h1>",
           '<p class="mut">Anstiege, die du mehrmals gefahren bist (Fuß und Gipfel höchstens 150 m '
           'auseinander), schnellste Fahrt zuerst.</p>']
    if not groups:
        out.append('<p class="mut">Noch keiner — dafür braucht es GPS (TrailBridge) und mindestens zwei '
                   'Fahrten über denselben Anstieg.</p>')
    for g in groups:
        best = g["efforts"][0]["duration_s"]
        f, s = g["foot"], g["summit"]
        out.append(f'<div class="panel"><div style="display:flex;gap:10px;align-items:baseline;flex-wrap:wrap">'
                   f'<span class="chip a">{escape(g["category"] or "–")}</span>'
                   f'<span class="big" style="font:600 22px var(--num);color:var(--parch-b)">'
                   f'{num(g["length_m"] / 1000, 1)} km · {g["gain_m"]} m · {num(g["avg_grade_pct"])} %</span>'
                   f'<a class="mut" href="https://www.openstreetmap.org/directions?route={f[0]},{f[1]};{s[0]},{s[1]}">'
                   'Karte</a></div>'
                   "<table><thead><tr><th>Datum</th><th class=num>Zeit</th><th class=num>Abstand</th>"
                   "<th class=num>VAM</th><th class='num hide-s'>Puls</th><th class='num hide-s'>Leistung*</th>"
                   "</tr></thead><tbody>")
        latest = max(g["efforts"], key=lambda e: e["day"])
        for i, e in enumerate(g["efforts"]):
            if i == 5:
                out.append(f'</tbody></table><details><summary>alle {len(g["efforts"])} Fahrten</summary>'
                           '<table><tbody>')
            day = datetime.date.fromisoformat(e["day"])
            mark = ' <span class="chip">zuletzt</span>' if e is latest else ""
            out.append(f'<tr><td><a href="/ride/{e["session_id"]}">{day:%d.%m.%Y}</a>{mark}</td>'
                       f'<td class=num><span class="big">{hms(e["duration_s"])}</span></td>'
                       f'<td class=num>{"+" + hms(e["duration_s"] - best) if e["duration_s"] > best else "–"}</td>'
                       f'<td class=num>{e["vam_m_h"] or "–"}</td><td class="num hide-s">{e["avg_hr"] or "–"}</td>'
                       f'<td class="num hide-s">{e["est_avg_power_w"] or "–"} W</td></tr>')
        out.append("</tbody></table>" + ("</details>" if len(g["efforts"]) > 5 else "") + "</div>")
    return page("Anstiege", "/climbs", "\n".join(out))


# --- rider ---------------------------------------------------------------------------------

ATHLETE_FIELDS = (
    ("hr_max", "Maximale Herzfrequenz", "bpm", "1", "gemessen (Test, Rennen), nicht per Formel"),
    ("hr_rest", "Ruhepuls", "bpm", "1", "morgens im Liegen; für TRIMP nach Banister"),
    ("rider_kg", "Körpergewicht", "kg", "0.1", "für W/kg und, mit dem Radgewicht, die Leistungsschätzung"),
    ("mass_kg", "Systemgewicht ohne Rad", "kg", "0.1", "Fahrer + Rad, nur für Fahrten ohne zugeordnetes Rad"),
    ("cda", "Luftwiderstand CdA ohne Rad", "m²", "0.01", "nur für Fahrten ohne zugeordnetes Rad"),
    ("crr", "Rollwiderstand Crr ohne Rad", "", "0.001", "nur für Fahrten ohne zugeordnetes Rad"),
)


def athlete_page(athlete, saved: bool = False, error: str | None = None) -> str:
    rows = []
    for name, label, unit, step, hint in ATHLETE_FIELDS:
        value = getattr(athlete, name)
        rows.append(f'<label class="f"><span>{label}</span><input type="number" step="{step}" '
                    f'name="{name}" value="{"" if value is None else value}"> {unit} '
                    f'<span class="mut">{hint}</span></label>')
    zones = ", ".join(f"{z:g}" for z in athlete.zones_pct)
    rows.append(f'<label class="f"><span>Zonengrenzen (% HFmax)</span><input name="zones_pct" '
                f'value="{zones}"> <span class="mut">Obergrenzen Z1–Z4</span></label>')
    msg = ""
    if saved:
        msg = '<p class="mut">Gespeichert — die Berichte werden neu berechnet.</p>'
    if error:
        msg = f'<p class="warn">{escape(error)}</p>'
    body = (f"<h1>Fahrer</h1>{msg}<div class='panel'><form method='post' action='/athlete'>"
            + "".join(rows) + "<button class='pri' type='submit'>Speichern</button></form></div>"
            "<p class='mut'>Gespeichert in athlete.json im Datenverzeichnis des Dienstes. Pulszonen, "
            "TRIMP und W/kg gibt es nur mit diesen Angaben. Gewicht und Aerodynamik der Räder stehen "
            "unter <a href='/bikes'>Räder</a>.</p>")
    return page("Fahrer", "/athlete", body)


# --- archive -----------------------------------------------------------------------------

def _delete_button(session_id: int | None, label: str, online: bool, primary: bool = False) -> str:
    hidden = f'<input type="hidden" name="id" value="{session_id}">' if session_id is not None else ""
    ask = ("Alle diese Sitzungen auf dem Fahrradcomputer löschen?" if session_id is None
           else "Diese Sitzung auf dem Fahrradcomputer löschen?")
    return (f'<form class="inline" method="post" action="/ui/archive/device-delete" '
            f'onsubmit="return confirm(\'{ask}\')">{hidden}'
            f'<button{" class=pri" if primary else ""} type="submit"{"" if online else " disabled"}>'
            f'{label}</button></form>')


def archive_page(idle: list[Session], archived: list[Session], days: float,
                 online: dict[str, bool], pull_enabled: bool, message: str | None = None) -> str:
    out = ["<h1>Archiv</h1>"]
    if message:
        out.append(f'<div class="msg">{escape(message)}</div>')
    out.append(f'<p class="mut">Leerlauf-Sitzungen: der Fahrradcomputer war an, ohne dass gefahren wurde '
               f'(Rad und Position standen still) — nur Debug-Daten. Sie erscheinen nicht in der Fahrtenliste, '
               f'bekommen kein GPX und wandern {num(days, 0)} Tage nach dem Abholen nach '
               '<code>archive/</code> im Datenverzeichnis. Aus dem Index verschwinden sie erst, wenn sie '
               'auch auf der SD-Karte nicht mehr liegen — sonst holt der nächste Abruf sie wieder.</p>')
    any_online = any(online.values())
    if not pull_enabled:
        out.append('<p class="warn">Abholen ist aus (BIKELOG_PULL) — Löschen auf dem BC geht nur mit '
                   'bekanntem Gerät.</p>')
    elif not any_online:
        out.append('<p class="mut">Der Fahrradcomputer ist gerade nicht erreichbar — Löschen auf dem BC '
                   'geht nur, solange er im WLAN ist. Es wird nichts vorgemerkt.</p>')
    if idle or archived:
        out.append("<p>" + _delete_button(None, f"Alle {len(idle) + len(archived)} auf dem BC löschen",
                                          any_online, primary=True) + "</p>")

    def rows(sessions: list[Session], archived_view: bool) -> str:
        body = []
        for s in sessions:
            base = f"{API}/sessions/{s.id}"
            files = "".join(
                (f'<span>{escape(f.name)} {_fmt_size(f.size)}</span>' if archived_view else
                 f'<a href="{base}/files/{escape(f.name)}">{escape(f.name)} {_fmt_size(f.size)}</a>')
                for f in s.files)
            when = (f'archiviert {escape((s.archived_at or "")[:10])}' if archived_view
                    else f'abgeholt {escape(s.created_at[:10])}')
            body.append(f'<tr><td>{escape(_when(s))}<br><span class="mut">{escape(s.device)} · {when}</span></td>'
                        f'<td class="chips">{files}</td>'
                        f'<td class="num">{_delete_button(s.id, "Auf dem BC löschen", online.get(s.device, False))}'
                        "</td></tr>")
        return ("<table><thead><tr><th>Sitzung</th><th>Dateien</th><th></th></tr></thead><tbody>"
                + "".join(body) + "</tbody></table>")

    out.append(f"<h2>Leerlauf, noch nicht archiviert ({len(idle)})</h2>")
    out.append(rows(idle, False) if idle else '<p class="mut">Keine.</p>')
    out.append(f"<h2>Archiviert, noch auf der SD-Karte ({len(archived)})</h2>")
    out.append(rows(archived, True) if archived else '<p class="mut">Keine.</p>')
    return page("Archiv", "/archive", "\n".join(out))


# --- bikes -----------------------------------------------------------------------------------

def _bike_form_full(b=None) -> str:
    from bikelog import bikes as bikes_mod
    v = (lambda attr: "" if b is None or getattr(b, attr) is None else escape(str(getattr(b, attr))))
    types = "".join(f'<option{" selected" if b and b.type == t else ""}>{escape(t)}</option>'
                    for t in bikes_mod.TYPES)
    return ('<form method="post" action="/bikes">'
            + (f'<input type="hidden" name="id" value="{escape(b.id)}">' if b else "")
            + f'<label class="f"><span>Name</span><input name="name" required value="{v("name")}"></label>'
            f'<label class="f"><span>Typ</span><select name="type">{types}</select></label>'
            f'<label class="f"><span>Gewicht fahrbereit</span><input type="number" step="0.1" name="mass_kg" '
            f'value="{v("mass_kg")}"> kg <span class="mut">mit Flaschen, Taschen, Licht</span></label>'
            f'<label class="f"><span>Luftwiderstand CdA</span><input type="number" step="0.01" name="cda" '
            f'value="{v("cda")}"> m² <span class="mut">Rennrad Unterlenker ≈ 0,32, Gravel Oberlenker ≈ 0,40, '
            'Pendler aufrecht mit Taschen ≈ 0,55</span></label>'
            f'<label class="f"><span>Rollwiderstand Crr</span><input type="number" step="0.001" name="crr" '
            f'value="{v("crr")}"> <span class="mut">Rennreifen Asphalt ≈ 0,004, Gravel ≈ 0,006, '
            'Pendler-Reifen ≈ 0,008</span></label>'
            f'<label class="f"><span>Notiz</span><input name="notes" value="{v("notes")}"></label>'
            '<button class="pri" type="submit">Speichern</button></form>')


def bikes_page(registry, devices: list[str], per_bike: dict, today: datetime.date,
               message: str | None = None) -> str:
    out = ["<h1>Räder</h1>"]
    if message:
        out.append(f'<div class="msg">{escape(message)}</div>')
    out.append('<p class="mut">Ein Rad trägt Gewicht und Aerodynamik für die Leistungsschätzung. Welcher '
               'Fahrradcomputer ab wann an welchem Rad fährt, steht unten; eine Fahrt bekommt das Rad, das '
               'ihrem Gerät am Fahrtag zugeordnet war (auf der Fahrtseite auch einzeln änderbar).</p>')
    year = today.year
    for b in registry.bikes:
        rides = per_bike.get(b.name, [])
        km_year = sum(r.distance_km for r in rides if r.day.year == year)
        km_all = sum(r.distance_km for r in rides)
        devs = sorted({a.device for a in registry.assignments if a.bike_id == b.id})
        out.append(f'<div class="panel"><div style="display:flex;gap:10px;align-items:baseline;flex-wrap:wrap">'
                   f'<h1 style="margin:0">{escape(b.name)}</h1><span class="chip tour">{escape(b.type)}</span>'
                   + "".join(f'<span class="chip">{escape(d)}</span>' for d in devs) + "</div>"
                   '<div class="tiles">'
                   + tile(f"Strecke {year}", num(km_year, 0), "km", f"{sum(1 for r in rides if r.day.year == year)} Fahrten")
                   + tile("Gesamt", num(km_all, 0), "km", f"{len(rides)} Fahrten")
                   + tile("Gewicht", num(b.mass_kg, 1) if b.mass_kg else "–", "kg")
                   + tile("CdA / Crr", (num(b.cda, 2) if b.cda else "–") + " / " + (num(b.crr, 3) if b.crr else "–"), "", text=True)
                   + "</div>"
                   f'<details><summary>Bearbeiten</summary>{_bike_form_full(b)}'
                   f'<form method="post" action="/bikes/{escape(b.id)}/delete" '
                   'onsubmit="return confirm(\'Rad löschen? Die Fahrten bleiben, verlieren aber die Zuordnung.\')">'
                   '<button type="submit">Löschen</button></form></details></div>')
    out.append("<h2>Neues Rad</h2><div class='panel'>" + _bike_form_full() + "</div>")

    out.append("<h2>Fahrradcomputer → Rad</h2>")
    rows = []
    for a in sorted(registry.assignments, key=lambda a: (a.device, a.since)):
        bike = registry.bike(a.bike_id)
        rows.append(f'<tr><td>{escape(a.device)}</td><td>{a.since:%d.%m.%Y}</td>'
                    f'<td>{escape(bike.name) if bike else "?"}</td><td>'
                    '<form class="inline" method="post" action="/bikes/assign/delete">'
                    f'<input type="hidden" name="device" value="{escape(a.device)}">'
                    f'<input type="hidden" name="since" value="{a.since.isoformat()}">'
                    '<button class="link" type="submit">entfernen</button></form></td></tr>')
    if rows:
        out.append("<table><thead><tr><th>Gerät</th><th>ab</th><th>Rad</th><th></th></tr></thead><tbody>"
                   + "".join(rows) + "</tbody></table>")
    else:
        out.append('<p class="mut">Noch keine Zuordnung.</p>')
    if registry.bikes:
        dev_opts = "".join(f"<option>{escape(d)}</option>" for d in devices)
        out.append('<div class="panel"><form method="post" action="/bikes/assign">'
                   f'<label class="f"><span>Gerät</span><select name="device">{dev_opts}</select> '
                   '<span class="mut">Name des Abhol-Ziels (BIKELOG_PULL_TARGETS)</span></label>'
                   f'<label class="f"><span>Rad</span><select name="bike_id">'
                   + "".join(f'<option value="{escape(b.id)}">{escape(b.name)}</option>' for b in registry.bikes)
                   + '</select></label>'
                   f'<label class="f"><span>ab</span><input type="date" name="since" required value="2020-01-01"> '
                   '<span class="mut">erste Fahrt mit dieser Zuordnung; beim Umbau ein neues Datum</span></label>'
                   '<button class="pri" type="submit">Zuordnen</button></form></div>')
    return page("Räder", "/bikes", "\n".join(out))
