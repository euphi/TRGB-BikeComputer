"""Ride reports as prose, written by a local LLM (Ollama), in the background.

bikelog.narrate builds the prompt from a ride's report and checks the answer; this
module talks to Ollama and keeps the texts in the ``narratives`` table, one per ride
(subject "tour:<first session id>" -- a ride of several sessions after a reboot is
one text). Off unless BIKELOG_LLM_URL is set.

refresh() runs after the reports (analysis.refresh): newest ride first, one answer
at a time. On the Orange Pi's CPU an 8B model writes ~3 tokens/s, so a text takes a
few minutes -- that is what the background is for. A ride without a text always gets
one; a text whose facts changed (a new goal, another bike) is rewritten only for rides
of the last llm_refresh_days, older ones keep theirs. Ollama not reachable: the pass
stops and the page says why; the next pull tries again.

The request switches the model's thinking off ("think": false): qwen3 otherwise
reasons in English for several hundred tokens before answering -- minutes on a CPU,
and in the test it talked itself into reading "hm" as hectometres. narrate.clean()
still strips a <think> block, for an Ollama too old to know the switch.
"""

from __future__ import annotations

import datetime
import json
import logging
import sqlite3
import threading
import time
import urllib.error
import urllib.request

from bikelog import narrate, training

from . import analysis
from .storage import Session, Storage

log = logging.getLogger("bikelog.llm")

#: generation options: little creativity, room for the prompt and ~150 words
OPTIONS = {"temperature": 0.3, "num_ctx": 4096, "num_predict": 700}

_lock = threading.Lock()            # one pass at a time
_again = threading.Event()          # something changed while a pass ran: run another
_state: dict = {"busy": None, "error": None}
_first: list[str] = []              # subjects asked for on the page: before the others


class LLMError(RuntimeError):
    """Ollama did not answer usefully."""


def subject(group: list[Session]) -> str:
    return f"tour:{group[0].id}"


def chat(url: str, model: str, msgs: list[dict], timeout: float) -> tuple[str, dict]:
    """One answer from Ollama's /api/chat: (content, timing figures)."""
    body = json.dumps({"model": model, "messages": msgs, "stream": False, "think": False,
                       "options": OPTIONS}).encode()
    req = urllib.request.Request(url + "/api/chat", data=body,
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            data = json.loads(resp.read())
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode(errors="replace")[:300]
        raise LLMError(f"Ollama: HTTP {exc.code} {detail}") from exc
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        raise LLMError(f"Ollama unter {url} nicht erreichbar: {getattr(exc, 'reason', exc)}") from exc
    content = (data.get("message") or {}).get("content") or ""
    if not content.strip():
        raise LLMError("Ollama: leere Antwort")
    ns = 1e9
    stats = {"eval_count": data.get("eval_count"),
             "eval_s": (data.get("eval_duration") or 0) / ns,
             "prompt_eval_count": data.get("prompt_eval_count"),
             "total_s": (data.get("total_duration") or 0) / ns}
    return content, stats


def status() -> dict:
    return dict(_state)


def for_page(store: Storage, group: list[Session], rep: dict) -> dict | None:
    """What the ride page shows: the stored text, or whether one is being written.
    None when texts are off or the ride gets none (test, too short)."""
    if not store.settings.llm_enabled:
        return None
    real = any(s.test_override == "real" for s in group)
    if any(s.test_override == "test" for s in group) or not training.Ride.from_report(rep, allow_test=real):
        return None
    subj = subject(group)
    return {"text": store.narrative(subj), "busy": _state["busy"] == subj,
            "working": _state["busy"] is not None, "error": _state["error"],
            "model": store.settings.llm_model}


def regenerate(store: Storage, group: list[Session]) -> None:
    """Forget the ride's text and write it again, before any other."""
    subj = subject(group)
    store.delete_narrative(subj)
    if subj not in _first:
        _first.append(subj)
    start(store)


def start(store: Storage) -> None:
    if store.settings.llm_enabled:
        threading.Thread(target=refresh, args=(store,), name="bikelog-llm", daemon=True).start()


def refresh(store: Storage) -> int:
    """Write every missing or stale text; returns how many. A call while a pass runs
    makes that pass run once more instead."""
    if not store.settings.llm_enabled:
        return 0
    if not _lock.acquire(blocking=False):
        _again.set()
        return 0
    done = 0
    try:
        while True:
            _again.clear()
            done += _pass(store)
            if not _again.is_set():
                return done
    except sqlite3.ProgrammingError:        # storage closed under us (shutdown)
        return done
    finally:
        _state["busy"] = None
        _lock.release()


def _todo(store: Storage) -> list[tuple[str, list[dict], datetime.date]]:
    """(subject, messages, day) of every ride that counts as training, newest first,
    the ones asked for on the page in front."""
    rides = analysis.rides(store)
    by_first = {r.session_ids[0]: r for r in rides if r.session_ids}
    events = analysis.events(store)
    out = []
    for group in reversed(analysis.tours_of(store)):
        ride = by_first.get(group[0].id)
        if ride is None:                # test, too short, no report
            continue
        rep = analysis.tour_report_for(store, group)
        out.append((subject(group), narrate.messages(rep, narrate.context(ride, rides, events)), ride.day))
    out.sort(key=lambda item: item[0] not in _first)
    return out


def _pass(store: Storage) -> int:
    settings = store.settings
    model = settings.llm_model
    oldest = analysis.today() - datetime.timedelta(days=settings.llm_refresh_days)
    done = 0
    for subj, msgs, day in _todo(store):
        key = narrate.prompt_key(msgs, model)
        have = store.narrative(subj)
        if have and (have["prompt_key"] == key or day < oldest):
            continue
        _state["busy"] = subj
        t0 = time.monotonic()
        try:
            content, stats = chat(settings.llm_url, model, msgs, settings.llm_timeout_s)
        except LLMError as exc:
            log.warning("%s", exc)
            _state["error"] = str(exc)
            return done
        text = narrate.clean(content)
        store.store_narrative(subj, key, model, text, narrate.unverified_numbers(text, msgs),
                              round(time.monotonic() - t0, 1))
        _state["error"] = None
        if subj in _first:
            _first.remove(subj)
        done += 1
        log.info("text for %s: %s tokens in %.0f s", subj, stats["eval_count"], stats["total_s"])
    return done
