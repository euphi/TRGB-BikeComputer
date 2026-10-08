"""The prompt for a local LLM that writes a ride report as prose -- and the check of its answer.

The numbers come from bikelog.report; the model only puts them into words and weighs
them (what kind of ride, does the intensity fit the training phase, anything technical
to look at). A small model on a CPU cannot be trusted with arithmetic or with units:
qwen3:8b read "480 hm" as 480 hectometres and added them to the distance. So:

* the facts are labelled lines with spelled-out units and the usual meaning
  ("Höhenmeter bergauf: 480 m", "Form (TSB): -12 (negativ = ermüdet)"), never
  abbreviations or JSON keys;
* everything worth saying is already computed (shares, comparisons with the recent
  rides), the system prompt forbids computing;
* unverified_numbers() lists every number in the answer that is not in the facts
  (allowing for rounding) -- the page shows them next to the text.

Pure functions. The log service (bikelogservice.llm) talks to Ollama and stores the text.

    context(ride, rides, events)    where the ride stands: recent rides, load, next goal
    messages(report, context)       the chat messages for the model
    prompt_key(messages, model)     what a stored text was written from
    clean(text)                     strip a <think> block and stray Markdown
    unverified_numbers(text, msgs)  numbers in the text that are not in the facts
"""

from __future__ import annotations

import datetime
import hashlib
import json
import re
import statistics

from . import training

#: Bump when the prompt changes in a way that should rewrite the stored texts.
PROMPT_VERSION = 1

#: Recent rides the ride is compared with (days before it).
RECENT_DAYS = 42
#: At most this many climbs go into the prompt (the biggest).
MAX_CLIMBS = 5

ZONE_NAMES = ("Regeneration", "Grundlage", "Tempo", "Schwelle", "VO2max")

SYSTEM = """\
Du bist ein erfahrener Radsport-Trainer. Du schreibst die Auswertung einer Radfahrt \
(Fahrrad, kein Lauf) für den Fahrer selbst: auf Deutsch, per du, sachlich und konkret.

Regeln:
- Verwende nur Zahlen, die in den Fakten stehen, mit genau der Einheit von dort. \
Rechne nichts aus und erfinde keine Werte.
- Leistung ist aus Tempo und Steigung geschätzt, nicht gemessen; erwähne sie höchstens am Rande.
- Zwei bis drei kurze Absätze, zusammen höchstens 150 Wörter. Keine Überschriften, \
keine Aufzählungen, kein Markdown.
- Absatz 1: was für eine Fahrt das war (Länge, Gelände, Anstiege, Tempo).
- Absatz 2: Belastung und Puls einordnen, verglichen mit den letzten Fahrten; wenn ein \
Ziel mit Trainingsphase genannt ist: passt die Fahrt zu dieser Phase, und was als \
Nächstes sinnvoll wäre.
- Absatz 3 nur, wenn unter "Technik" Warnungen stehen: ein Satz dazu, was zu prüfen ist.\
"""


def _n(value, digits: int = 1) -> str:
    if value is None:
        return "–"
    text = f"{value:.{digits}f}" if digits else str(round(value))
    return text.replace(".", ",")


def _hmm(seconds) -> str:
    """h:mm -- the same way the text should write it, so the check finds the numbers."""
    minutes = round((seconds or 0) / 60)
    return f"{minutes // 60}:{minutes % 60:02d} h"


def _mmss(seconds) -> str:
    seconds = round(seconds or 0)
    if seconds >= 3600:
        return _hmm(seconds)
    return f"{seconds // 60}:{seconds % 60:02d} min"


# --- context -------------------------------------------------------------------------

def _next_goal(events: list[training.Event], day: datetime.date) -> training.Event | None:
    ahead = [e for e in events if e.date >= day]
    # the nearest A event, else the nearest of any priority
    return min(ahead, key=lambda e: (e.priority != "A", e.date), default=None)


def context(ride: training.Ride, rides: list[training.Ride],
            events: list[training.Event]) -> dict:
    """Where the ride stands: the rides of the RECENT_DAYS before it, the training load
    on its morning, and the next goal with its phase. Only rides before this one count,
    so a text written today still reads the same next month."""
    earlier = [r for r in rides if r.start < ride.start]
    since = ride.day - datetime.timedelta(days=RECENT_DAYS)
    recent = [r for r in earlier if r.day >= since]
    trimps = [r.trimp for r in recent if r.trimp is not None]
    out: dict = {
        "recent_rides": len(recent),
        "recent_median_km": round(statistics.median(r.distance_km for r in recent), 1) if recent else None,
        "recent_median_ascent_m": round(statistics.median(r.ascent_m for r in recent)) if recent else None,
        "recent_median_trimp": round(statistics.median(trimps)) if trimps else None,
        "load": None,
        "goal": None,
    }
    if earlier:
        # the morning of the ride: fitness as of the evening before, form = that CTL - ATL
        evening = training.load_series(earlier, ride.day - datetime.timedelta(days=1),
                                       ride.day - datetime.timedelta(days=1))[-1]
        out["load"] = {"ctl": round(evening.ctl), "tsb": round(evening.ctl - evening.atl)}
    goal = _next_goal(events, ride.day)
    if goal:
        days = (goal.date - ride.day).days
        name, text = training.phase(days)
        out["goal"] = {"name": goal.name, "date": goal.date.isoformat(), "days_left": days,
                       "km": goal.course_km, "ascent_m": goal.course_ascent_m,
                       "phase": name, "phase_text": text}
    return out


# --- facts ---------------------------------------------------------------------------

def facts(rep: dict, ctx: dict | None = None) -> str:
    """The report as labelled lines -- what the model may use, and all it may use."""
    meta, ride, heart, power, road, health = (rep[k] for k in ("meta", "ride", "heart", "power",
                                                              "road", "health"))
    start = datetime.datetime.fromisoformat(meta["start_utc"]).astimezone()
    out = ["Fahrt:",
           f"- Datum: {start.day}.{start.month}.{start.year}, Start {start.hour}:{start.minute:02d} Uhr"]
    bike = meta.get("bike")
    if bike:
        out.append(f"- Rad: {bike['name']} ({bike['type']})")
    out += [f"- Strecke: {_n(ride['distance_km'])} km",
            f"- Fahrzeit: {_hmm(ride['moving_s'])}, Gesamtzeit mit Pausen: {_hmm(ride['duration_s'])}",
            f"- Durchschnittsgeschwindigkeit in Bewegung: {_n(ride['avg_moving_kmh'])} km/h, "
            f"Höchstgeschwindigkeit: {_n(ride['max_kmh'])} km/h",
            f"- Höhenmeter bergauf: {ride['ascent_m']} m, bergab: {ride['descent_m']} m "
            f"(Höhe zwischen {ride['min_ele_m']} m und {ride['max_ele_m']} m über NN)"]
    stops = ride["stops"]
    if stops["count"]:
        out.append(f"- Pausen: {stops['count']}, zusammen {_mmss(stops['total_s'])}, "
                   f"längste {_mmss(stops['longest_s'])}")
    if ride.get("temp_c"):
        out.append(f"- Temperatur: {_n(ride['temp_c']['min'], 0)} bis {_n(ride['temp_c']['max'], 0)} °C")
    if ride.get("avg_cadence"):
        out.append(f"- Trittfrequenz: im Schnitt {ride['avg_cadence']} Umdrehungen pro Minute, "
                   f"getreten in {_n((ride['pedalling_share'] or 0) * 100, 0)} % der Fahrzeit")

    every = rep.get("climbs") or []
    climbs = sorted(every, key=lambda c: c["gain_m"], reverse=True)[:MAX_CLIMBS]
    if climbs:
        biggest = f"; die {len(climbs)} größten von {len(every)}" if len(every) > len(climbs) else ""
        out += ["", f"Anstiege (Kategorie 6 = kleinster, 1 und HC = größte{biggest}):"]
        for c in sorted(climbs, key=lambda c: c["start_km"]):
            line = (f"- ab km {_n(c['start_km'])}: Kategorie {c['category'] or '–'}, "
                    f"{_n(c['length_m'] / 1000)} km lang, {c['gain_m']} m hoch, "
                    f"im Schnitt {_n(c['avg_grade_pct'])} % Steigung, gefahren in {_mmss(c['duration_s'])}")
            if c.get("avg_hr"):
                line += f", Puls im Schnitt {c['avg_hr']}/min"
            out.append(line)
    else:
        out += ["", "Anstiege: keine nennenswerten."]

    out += ["", "Puls und Belastung:"]
    if heart.get("avg"):
        out.append(f"- Puls im Schnitt {heart['avg']}/min, höchster {heart['max']}/min")
        zones = heart.get("zones_s")
        if zones:
            total = sum(zones.values()) or 1
            bounds = heart.get("zones_bpm") or []
            parts = []
            for i, (z, s) in enumerate(zones.items()):
                name = ZONE_NAMES[i] if i < len(ZONE_NAMES) else z
                limit = f" bis {bounds[i]}/min" if i < len(bounds) else " darüber"
                parts.append(f"Zone {i + 1} {name}{limit}: {_n(s / total * 100, 0)} %")
            out.append("- Zeit in den Pulszonen: " + "; ".join(parts))
        if heart.get("trimp") is not None:
            out.append(f"- Trainingsbelastung (TRIMP): {heart['trimp']}")
        if heart.get("decoupling_pct") is not None:
            out.append(f"- Aerobe Entkopplung zwischen erster und zweiter Hälfte: "
                       f"{_n(heart['decoupling_pct'])} % (unter 5 % = gute Grundlagenausdauer)")
    else:
        out.append("- kein Puls aufgezeichnet")
    if power.get("normalized_w"):
        out.append(f"- geschätzte Leistung: im Schnitt {power['avg_moving_w']} W, normalisiert "
                   f"{power['normalized_w']} W")

    if ctx:
        out += ["", "Einordnung:"]
        if ctx["recent_rides"]:
            line = (f"- Fahrten in den {RECENT_DAYS} Tagen davor: {ctx['recent_rides']}, typisch "
                    f"{_n(ctx['recent_median_km'])} km und {ctx['recent_median_ascent_m']} m bergauf")
            if ctx.get("recent_median_trimp") is not None:
                line += f", typische Trainingsbelastung (TRIMP) {ctx['recent_median_trimp']}"
            out.append(line)
        else:
            out.append(f"- keine Fahrten in den {RECENT_DAYS} Tagen davor aufgezeichnet")
        if ctx.get("load"):
            out.append(f"- Fitness (CTL) vor der Fahrt: {ctx['load']['ctl']}, Form (TSB): "
                       f"{ctx['load']['tsb']} (negativ = ermüdet, positiv = erholt)")
        goal = ctx.get("goal")
        if goal:
            course = ""
            if goal.get("km"):
                course = f", Strecke {_n(goal['km'], 0)} km"
                if goal.get("ascent_m"):
                    course += f" mit {goal['ascent_m']} m bergauf"
            out.append(f"- nächstes Ziel: {goal['name']} in {goal['days_left']} Tagen{course}")
            out.append(f"- Trainingsphase: {goal['phase']} -- {goal['phase_text']}")

    shares = road.get("class_share_rated") or {}
    if shares:
        out += ["", "Untergrund (vom Beschleunigungssensor bewertet):",
                "- " + ", ".join(f"{k} {_n(v * 100, 0)} %" for k, v in shares.items())
                + f"; Stöße: {road['shocks']['count']}"]

    warnings = [f["text"] for f in health.get("findings", []) if f["level"] == "warn"]
    out += ["", "Technik:"]
    out += [f"- Warnung: {w}" for w in warnings] or ["- keine Warnungen"]
    return "\n".join(out)


def messages(rep: dict, ctx: dict | None = None) -> list[dict]:
    return [{"role": "system", "content": SYSTEM},
            {"role": "user", "content": "Schreibe die Auswertung dieser Radfahrt.\n\n" + facts(rep, ctx)}]


def prompt_key(msgs: list[dict], model: str) -> str:
    raw = json.dumps([PROMPT_VERSION, model, msgs], ensure_ascii=False, sort_keys=True)
    return hashlib.sha256(raw.encode()).hexdigest()[:20]


# --- the answer ----------------------------------------------------------------------

_THINK = re.compile(r"<think>.*?</think>", re.S)


def clean(text: str) -> str:
    """The answer without a reasoning block, Markdown emphasis and headings."""
    text = _THINK.sub("", text)
    if "</think>" in text:                       # opening tag eaten by the template
        text = text.split("</think>", 1)[1]
    text = re.sub(r"^#+\s*", "", text, flags=re.M)
    text = text.replace("**", "").replace("__", "")
    text = re.sub(r"\n{3,}", "\n\n", text)
    return text.strip()


# 1.234 (thousands) | 42,5 / 42.5 | 42
_NUMBER = re.compile(r"(?<![\d.,])(\d{1,3}(?:\.\d{3})+(?![\d,])|\d+(?:[.,]\d+)?)")


def _numbers(text: str) -> list[tuple[str, float, int]]:
    """(as written, value, decimals) of every number in ``text``; a minus is ignored."""
    out = []
    for m in _NUMBER.finditer(text):
        raw = m.group(1)
        if re.fullmatch(r"\d{1,3}(?:\.\d{3})+", raw):
            out.append((raw, float(raw.replace(".", "")), 0))
            continue
        norm = raw.replace(",", ".")
        decimals = len(norm.split(".")[1]) if "." in norm else 0
        out.append((raw, float(norm), decimals))
    return out


def unverified_numbers(text: str, msgs: list[dict]) -> list[str]:
    """Numbers in the model's text that are not in the prompt -- also not rounded to the
    precision the text writes them with. Small counts (up to 12: "zwei Anstiege", "Zone 4",
    "Absatz 3") are always fine. A non-empty list does not prove a mistake (a model may
    round 42,4 km to "gut 40 km"), it is a reason to look."""
    known = [v for _, v, _ in _numbers("\n".join(m["content"] for m in msgs))]
    out = []
    for raw, value, decimals in _numbers(text):
        if decimals == 0 and value <= 12:
            continue
        step = 10 ** -decimals
        if any(abs(round(k, decimals) - value) < step / 2 or abs(k - value) <= step / 2
               for k in known):
            continue
        if raw not in out:
            out.append(raw)
    return out
