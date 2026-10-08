"""The ride in words: prompt (bikelog.narrate) and the log service's LLM worker (llm.py)."""

import datetime
import time

import pytest
from fastapi.testclient import TestClient

from bikelog import fixtures, narrate, report, training
from bikelog.record import write_records
from bikelogservice import analysis, llm
from bikelogservice.app import create_app
from bikelogservice.config import Settings

from test_report import ride

API = "/api/v1"


def climb_report():
    records = ride([(480, 15.0, 0.0), (720, 10.0, 5.0), (240, 30.0, -5.0), (240, 15.0, 0.0)], hr=150)
    return report.compute(records, report.Athlete(hr_max=185, hr_rest=50))


def test_facts_spell_out_units_and_meaning():
    text = narrate.facts(climb_report())
    assert "Höhenmeter bergauf:" in text and " hm" not in text
    assert "Kategorie 4" in text and "Steigung" in text
    assert "Zone 1 Regeneration bis 111/min" in text
    assert "Trainingsbelastung (TRIMP)" in text
    assert "- keine Warnungen" in text
    assert "Einordnung" not in text                 # no context given


def test_context_compares_with_earlier_rides_and_names_the_phase():
    rep = climb_report()
    this = training.Ride.from_report(rep, 3)
    earlier = [training.Ride.from_report(rep, i) for i in (1, 2)]
    for i, r in enumerate(earlier):
        r.start -= datetime.timedelta(days=3 * (i + 1))
        r.day -= datetime.timedelta(days=3 * (i + 1))
    goal = training.Event("Eschborn-Frankfurt", this.day + datetime.timedelta(days=60),
                          distance_km=180, ascent_m=2200)
    ctx = narrate.context(this, earlier + [this], [goal])
    assert ctx["recent_rides"] == 2
    assert ctx["load"]["ctl"] > 0
    assert ctx["goal"]["days_left"] == 60 and ctx["goal"]["phase"] == "Aufbau"
    text = narrate.facts(rep, ctx)
    assert "nächstes Ziel: Eschborn-Frankfurt in 60 Tagen" in text
    assert "Trainingsphase: Aufbau" in text


def test_unverified_numbers():
    msgs = [{"role": "user", "content": "Strecke: 42,4 km, bergauf 1234 m, Puls 148/min, Zone 4: 35 %"}]
    ok = "Gut 42 km und 1.234 m bergauf bei Puls 148, 35 % in Zone 4, zwei Anstiege (3 davon steil)."
    assert narrate.unverified_numbers(ok, msgs) == []
    assert narrate.unverified_numbers("42.4 km", msgs) == []
    # the qwen3:8b answer from the first test: 480 hm read as hectometres
    assert narrate.unverified_numbers("Gesamtdistanz von 90 km (42 km + 480 hm)", msgs) == ["90", "480"]


def test_clean():
    assert narrate.clean("<think>\nhm = hectometres?\n</think>\n\n**Gute** Fahrt.\n\n\n\nZweiter") \
        == "Gute Fahrt.\n\nZweiter"
    assert narrate.clean("reasoning</think>Text") == "Text"
    assert narrate.clean("## Auswertung\nText") == "Auswertung\nText"


def test_prompt_key_follows_prompt_and_model():
    msgs = narrate.messages(climb_report())
    assert narrate.prompt_key(msgs, "a") == narrate.prompt_key(msgs, "a")
    assert narrate.prompt_key(msgs, "a") != narrate.prompt_key(msgs, "b")


# --- service ---------------------------------------------------------------------------

@pytest.fixture
def calls(monkeypatch):
    seen = []

    def fake_chat(url, model, msgs, timeout):
        seen.append(msgs)
        return "<think>x</think>Eine ruhige Runde über 99 km.", {"eval_count": 9, "total_s": 1.0}

    monkeypatch.setattr(llm, "chat", fake_chat)
    monkeypatch.setattr(llm, "_state", {"busy": None, "error": None})
    monkeypatch.setattr(llm, "_first", [])
    return seen


@pytest.fixture
def client(tmp_path, monkeypatch, calls):
    monkeypatch.setattr(analysis, "today", lambda: datetime.date(2026, 5, 20))
    settings = Settings(data_dir=tmp_path / "state", llm_url="http://ollama:11434")
    with TestClient(create_app(settings)) as client:
        yield client


@pytest.fixture
def sid(client, tmp_path):
    path = tmp_path / "ride.bin"
    write_records(path, fixtures.synthetic(seconds=1800))
    r = client.put(f"{API}/devices/gravel/files/20260517/L_090000.bin", content=path.read_bytes())
    return r.json()["session"]["id"]


def wait_for_text(store, sid, timeout=10.0):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        have = store.narrative(f"tour:{sid}")
        if have and not llm._lock.locked():
            return have
        time.sleep(0.05)
    raise AssertionError("no text written")


def test_text_is_written_once_and_shown(client, sid, calls):
    store = client.app.state.storage
    have = wait_for_text(store, sid)
    assert have["text"] == "Eine ruhige Runde über 99 km."
    assert have["unverified"] == ["99"]
    assert llm.refresh(store) == 0                  # nothing changed: no second call
    assert len(calls) == 1
    html = client.get(f"/ride/{sid}").text
    assert "In Worten" in html and "Eine ruhige Runde" in html
    assert "nicht in den Daten stehen: 99" in html


def test_regenerate(client, sid, calls):
    store = client.app.state.storage
    wait_for_text(store, sid)
    r = client.post(f"/ui/narrative/{sid}", follow_redirects=False)
    assert r.status_code == 303 and r.headers["location"] == f"/ride/{sid}"
    wait_for_text(store, sid)
    assert len(calls) == 2


def test_ollama_down_is_shown(client, sid, monkeypatch):
    store = client.app.state.storage
    wait_for_text(store, sid)
    store.delete_narrative(f"tour:{sid}")

    def down(*args):
        raise llm.LLMError("Ollama unter http://ollama:11434 nicht erreichbar: refused")

    monkeypatch.setattr(llm, "chat", down)
    assert llm.refresh(store) == 0
    html = client.get(f"/ride/{sid}").text
    assert "Noch kein Text: Ollama unter" in html and "Nochmal versuchen" in html


def test_off_without_url(tmp_path, calls):
    settings = Settings(data_dir=tmp_path / "state")
    with TestClient(create_app(settings)) as client:
        path = tmp_path / "ride.bin"
        write_records(path, fixtures.synthetic(seconds=1800))
        sid = client.put(f"{API}/devices/gravel/files/20260517/L_090000.bin",
                         content=path.read_bytes()).json()["session"]["id"]
        assert "In Worten" not in client.get(f"/ride/{sid}").text
        assert llm.refresh(client.app.state.storage) == 0
    assert calls == []
