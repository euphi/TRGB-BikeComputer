"""The bike log service.

Keeps every session the bike computer writes (all of its files, verbatim) and
derives GPX/CSV from them on demand. Sessions arrive by pull (puller.py: the
service fetches from the device when it shows up on the network) or by push
(``PUT /api/v1/devices/{device}/files/{day}/{name}``); both end up in the same
idempotent Storage.put_file(). Publishing to Nextcloud/Komoot/Strava and the
heatmap come later and hang off the same storage.

Run it:  uvicorn bikelogservice.app:app
API doc: http://<host>:8080/docs (FastAPI generates it from the routes)
"""

from __future__ import annotations

import asyncio
import datetime
import io
import re
import threading
from contextlib import asynccontextmanager
from urllib.parse import parse_qs, urlencode

from fastapi import Depends, FastAPI, HTTPException, Path, Query, Request, Response, status
from fastapi.responses import HTMLResponse, PlainTextResponse, RedirectResponse
from fastapi.concurrency import run_in_threadpool

from bikelog import bikes, csvexport, gpximport, report, training
from bikelog.record import ReadStats

from . import analysis, archive, exporter, importer, llm, tours, komoot, nextcloud, sdlayout, webui
from .auth import AuthDep, Principal
from .config import Settings
from .puller import DeviceUnavailable, Puller
from .storage import Session, Storage

API = "/api/v1"
DEVICE_RE = r"^[A-Za-z0-9_-]{1,32}$"


def create_app(settings: Settings | None = None, puller: Puller | None = None) -> FastAPI:
    settings = settings or Settings.from_env()
    store = Storage(settings)
    if puller is None and settings.pull:
        puller = Puller(store, settings)

    def _analyse(store: Storage) -> None:
        # reports first (the pages need them), then the texts (minutes each)
        analysis.refresh(store)
        llm.refresh(store)

    def _export_and_sync(store: Storage) -> None:
        exporter.export_pending(store)
        nextcloud.sync_pending(store)
        _analyse(store)

    def _sync_and_analyse(store: Storage) -> None:
        nextcloud.sync_pending(store)
        _analyse(store)

    @asynccontextmanager
    async def lifespan(app: FastAPI):
        # Catch up on GPX files (new exporter version, sessions stored while the
        # export was off) and Nextcloud sync, without holding up the start.
        threading.Thread(target=_export_and_sync, args=(store,),
                         name="bikelog-export", daemon=True).start()
        if puller:
            puller.start()
        yield
        if puller:
            # zeroconf refuses to close (blocking I/O) on the event loop thread.
            await asyncio.to_thread(puller.stop)
        store.close()

    app = FastAPI(
        title="BikeLog Service",
        version="2.0.0",
        description="Sitzungen des Fahrradcomputers abholen/annehmen, "
                    "aufbewahren und als GPX/CSV ausliefern.",
        lifespan=lifespan,
    )
    app.state.settings = settings
    app.state.storage = store
    app.state.puller = puller

    def storage(request: Request) -> Storage:
        return request.app.state.storage

    # ------------------------------------------------------------------
    # Every route below takes the auth dependency, even while it returns an
    # anonymous principal -- see auth.py. That is what makes switching auth
    # on a config change instead of a sweep through the routes.
    # ------------------------------------------------------------------

    @app.get("/", response_class=HTMLResponse, include_in_schema=False)
    def index(min_km: str | None = Query(default=None, max_length=20),
             max_km: str | None = Query(default=None, max_length=20),
             msg: str | None = Query(default=None, max_length=300),
             tests: bool = Query(default=False),
             principal: Principal = AuthDep, store: Storage = Depends(storage)):
        # The filter form sends empty fields as "min_km=" -- that is "no bound", not an error.
        min_km_v, max_km_v = _opt_km(min_km), _opt_km(max_km)
        min_m = min_km_v * 1000 if min_km_v is not None else None
        max_m = max_km_v * 1000 if max_km_v is not None else None
        sessions = store.list(limit=500, min_distance_m=min_m, max_distance_m=max_m,
                              tests=tests, idle=False)
        hidden = 0 if tests else (store.count(min_m, max_m, idle=False)
                                  - store.count(min_m, max_m, tests=False, idle=False))
        return webui.index(sessions, puller.as_dict() if puller else None,
                           min_km=min_km_v, max_km=max_km_v,
                           prompts=komoot.pending_prompts(store), message=msg,
                           tests=tests, hidden_tests=hidden, idle=len(store.idle_sessions()),
                           tours={s.id: (i + 1, len(g), g[0].id) for g in analysis.tours_of(store)
                                  if len(g) > 1 for i, s in enumerate(g)},
                           bike_names=_bike_names(store, sessions),
                           registry=analysis.registry(store), events=analysis.events(store))

    def _bike_names(store: Storage, sessions: list[Session]) -> dict[int, str]:
        reg = analysis.registry(store)
        out = {}
        for s in sessions:
            bike = analysis.bike_for(store, s, reg)
            if bike:
                out[s.id] = bike.name
        return out

    def _opt_km(value: str | None) -> float | None:
        if value is None or not value.strip():
            return None
        try:
            km = float(value.replace(",", "."))
        except ValueError:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, f"not a distance: {value!r}")
        if km < 0:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "distance must not be negative")
        return km

    # --- archive: idle sessions, deleting on the device ---

    def _device_online() -> dict[str, bool]:
        return {t["device"]: bool(t["online"]) for t in (puller.as_dict()["targets"] if puller else [])}

    @app.get("/archive", response_class=HTMLResponse, include_in_schema=False)
    def archive_page(msg: str | None = Query(default=None, max_length=500),
                     principal: Principal = AuthDep, store: Storage = Depends(storage)):
        return webui.archive_page(store.idle_sessions(), store.archived_sessions(),
                                  settings.idle_archive_days, _device_online(),
                                  pull_enabled=puller is not None, message=msg)

    def _delete_on_device(store: Storage, sessions: list[Session]) -> tuple[int, list[str]]:
        if puller is None:
            raise DeviceUnavailable("pulling is disabled (BIKELOG_PULL) -- the device is unknown")
        return archive.delete_on_device(store, puller, sessions)

    def _deletable(store: Storage, session_id: int | None) -> list[Session]:
        """Idle or archived sessions (only those may be deleted on the device from here)."""
        candidates = store.idle_sessions() + store.archived_sessions()
        if session_id is None:
            return candidates
        chosen = [s for s in candidates if s.id == session_id]
        if not chosen:
            raise HTTPException(status.HTTP_404_NOT_FOUND, "no idle or archived session with this id")
        return chosen

    @app.post("/ui/archive/device-delete", include_in_schema=False)
    async def ui_device_delete(request: Request, principal: Principal = AuthDep,
                               store: Storage = Depends(storage)):
        form = await _form(request)
        sid = int(form["id"]) if form.get("id", "").isdigit() else None
        try:
            # blocking mDNS lookup + HTTP to the device: not on the event loop
            done, errors = await run_in_threadpool(
                lambda: _delete_on_device(store, _deletable(store, sid)))
            text = f"{done} Sitzung(en) auf dem BC gelöscht und archiviert."
            if errors:
                text += " Nicht gelöscht: " + "; ".join(errors[:3]) + (" …" if len(errors) > 3 else "")
        except DeviceUnavailable as exc:
            text = f"Nichts gelöscht: {exc}"
        return RedirectResponse("/archive?" + urlencode({"msg": text}),
                                status_code=status.HTTP_303_SEE_OTHER)

    @app.post(API + "/archive/device-delete", tags=["sessions"])
    def device_delete(session_id: int | None = Query(default=None,
                                                     description="one idle/archived session; all if omitted"),
                      principal: Principal = AuthDep, store: Storage = Depends(storage)):
        """Delete idle and archived sessions on the bike computer now -- only while it is
        reachable, nothing is queued. Sessions whose files are all gone from the card are
        archived and dropped from the index."""
        try:
            done, errors = _delete_on_device(store, _deletable(store, session_id))
        except DeviceUnavailable as exc:
            raise HTTPException(status.HTTP_409_CONFLICT, str(exc))
        return {"deleted_sessions": done, "errors": errors}

    # The page's own buttons: same actions as the API routes below, but they answer with a
    # redirect back to the page (a form post must not end on a bare JSON document).
    def _back(session_id: int, text: str) -> RedirectResponse:
        return RedirectResponse("/?" + urlencode({"msg": text}) + f"#s{session_id}",
                                status_code=status.HTTP_303_SEE_OTHER)

    @app.post("/ui/sessions/{session_id}/komoot", include_in_schema=False)
    def ui_komoot(session_id: int, principal: Principal = AuthDep, store: Storage = Depends(storage)):
        session = _require_log(store, session_id)
        group = komoot.group_for(store, session)
        if any(s.komoot_status == "uploaded" for s in group):
            return _back(session_id, "Diese Fahrt ist schon bei Komoot.")
        result = komoot.upload_group(store, group)
        if result.status == "uploaded":
            store.set_komoot_prompt([s.id for s in group], "ignored")
            return _back(session_id, "Zu Komoot hochgeladen.")
        return _back(session_id, "Komoot-Upload nicht erfolgt: %s" % (result.message or result.status))

    @app.post("/ui/sessions/{session_id}/test", include_in_schema=False)
    async def ui_test(session_id: int, request: Request, principal: Principal = AuthDep,
                      store: Storage = Depends(storage)):
        _require(store, session_id)
        mark = (await _form(request)).get("mark", "")
        _set_test(store, session_id, mark)
        return RedirectResponse(f"/ride/{session_id}", status_code=status.HTTP_303_SEE_OTHER)

    def _set_test(store: Storage, session_id: int, mark: str) -> None:
        if mark not in ("test", "real", "auto"):
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "mark must be test, real or auto")
        store.set_test_override(session_id, None if mark == "auto" else mark)
        exporter.export_pending(store)       # into Tours or Debug_Archive
        threading.Thread(target=nextcloud.sync_pending, args=(store,),
                         name="bikelog-nextcloud", daemon=True).start()

    @app.post(API + "/sessions/{session_id}/test", tags=["sessions"])
    def mark_test(session_id: int, mark: str = Query(pattern="^(test|real|auto)$",
                                                     description="test, real, or auto (as the log says)"),
                  principal: Principal = AuthDep, store: Storage = Depends(storage)):
        """Overrule the automatic test detection (simulator flag, GPS playback) of a session."""
        _require(store, session_id)
        _set_test(store, session_id, mark)
        return store.get(session_id).as_dict()

    @app.post("/ui/sessions/{session_id}/komoot-ignore", include_in_schema=False)
    def ui_komoot_ignore(session_id: int, principal: Principal = AuthDep,
                         store: Storage = Depends(storage)):
        session = _require(store, session_id)
        store.set_komoot_prompt([s.id for s in komoot.group_for(store, session)], "ignored")
        return _back(session_id, "Gut, diese Fahrt wird nicht zu Komoot hochgeladen.")

    # --- pages (Rim & Ridge) ---

    def _athlete_or_default(store: Storage) -> report.Athlete:
        try:
            return analysis.athlete(store)
        except (OSError, ValueError, TypeError):
            return report.Athlete()

    @app.get("/ride/{session_id}", response_class=HTMLResponse, include_in_schema=False)
    def ride_page(session_id: int, principal: Principal = AuthDep, store: Storage = Depends(storage)):
        session = _require_log(store, session_id)
        group = [session] if (session.is_test or session.idle) else tours.group_for(store, session)
        reg = analysis.registry(store)
        rep = _report(store, session)
        return webui.ride_page(session, rep, group=group, registry=reg,
                               bike=analysis.bike_for(store, session, reg),
                               narrative=llm.for_page(store, group, rep) if len(group) == 1 else None)

    @app.get("/tour/{session_id}", response_class=HTMLResponse, include_in_schema=False)
    def tour_page(session_id: int, principal: Principal = AuthDep, store: Storage = Depends(storage)):
        """The whole ride a session belongs to (several sessions after a reboot on the way)."""
        session = _require_log(store, session_id)
        group = tours.group_for(store, session)
        try:
            rep = analysis.tour_report_for(store, group)
        except (OSError, ValueError, TypeError) as exc:
            raise HTTPException(status.HTTP_500_INTERNAL_SERVER_ERROR, str(exc))
        return webui.ride_page(group[0], rep, group=group, tour=len(group) > 1,
                               narrative=llm.for_page(store, group, rep))

    @app.post("/ui/narrative/{session_id}", include_in_schema=False)
    def narrative_again(session_id: int, principal: Principal = AuthDep,
                        store: Storage = Depends(storage)):
        """Write the ride's text again (prompt or model changed, or it was just bad)."""
        session = _require_log(store, session_id)
        group = tours.group_for(store, session)
        llm.regenerate(store, group)
        target = f"/tour/{group[0].id}" if len(group) > 1 else f"/ride/{session.id}"
        return RedirectResponse(target, status_code=status.HTTP_303_SEE_OTHER)

    @app.get("/training", response_class=HTMLResponse, include_in_schema=False)
    def training_page(days: int = Query(default=180, ge=28, le=1100),
                      principal: Principal = AuthDep, store: Storage = Depends(storage)):
        rides = analysis.rides(store)
        today = analysis.today()
        # the chart starts with the first ride if that is later -- no months of zeros
        start = today - datetime.timedelta(days=days - 1)
        if rides:
            start = max(start, min(today - datetime.timedelta(days=27), rides[0].day))
        load = training.load_series(rides, start, today)
        return webui.training_page(load, training.weeks(rides, today, 16), len(rides),
                                   bool(_athlete_or_default(store).hr_max),
                                   sum(1 for r in rides if r.trimp is None))

    @app.get("/goals", response_class=HTMLResponse, include_in_schema=False)
    def goals_page(principal: Principal = AuthDep, store: Storage = Depends(storage)):
        rides = analysis.rides(store)
        today = analysis.today()
        items = []
        for e in analysis.events(store):
            parts = []
            for sid in e.participations:
                session = store.get(sid)
                if session is not None and session.file("L"):
                    try:
                        parts.append((session, analysis.report_for(store, session)))
                    except Exception:
                        continue
            items.append((e, training.readiness(e, rides, today), parts))
        # upcoming first (nearest on top), past ones at the end
        items.sort(key=lambda er: (er[1]["days_left"] < 0, abs(er[1]["days_left"])))
        return webui.goals_page(items, today)

    async def _form(request: Request) -> dict[str, str]:
        body = (await request.body()).decode("utf-8", "replace")
        return {k: v[0].strip() for k, v in parse_qs(body, keep_blank_values=True).items()}

    def _num(value: str, kind=float):
        return kind(value.replace(",", ".")) if value else None

    @app.post("/goals", include_in_schema=False)
    async def save_goal(request: Request, principal: Principal = AuthDep,
                        store: Storage = Depends(storage)):
        form = await _form(request)
        try:
            when = datetime.date.fromisoformat(form.get("date", ""))
            distance, ascent = _num(form.get("distance_km", "")), _num(form.get("ascent_m", ""))
        except ValueError as exc:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, f"invalid value: {exc}")
        if not form.get("name"):
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "name missing")
        old = next((e for e in analysis.events(store) if e.id == form.get("id")), None)
        event = old or training.Event(name=form["name"], date=when)
        event.name, event.date = form["name"], when
        event.priority = form.get("priority", "A") if form.get("priority") in ("A", "B", "C") else "A"
        event.distance_km, event.ascent_m, event.notes = distance, ascent, form.get("notes", "")
        analysis.save_event(store, event)
        if "application/json" in request.headers.get("accept", ""):
            return {"id": event.id}             # the page's script uploads the GPX next
        return RedirectResponse(f"/goals#e{event.id}", status_code=status.HTTP_303_SEE_OTHER)

    @app.post("/goals/{event_id}/delete", include_in_schema=False)
    def delete_goal(event_id: str, principal: Principal = AuthDep, store: Storage = Depends(storage)):
        if not analysis.delete_event(store, event_id):
            raise HTTPException(status.HTTP_404_NOT_FOUND, "unknown event")
        return RedirectResponse("/goals", status_code=status.HTTP_303_SEE_OTHER)

    @app.get("/climbs", response_class=HTMLResponse, include_in_schema=False)
    def climbs_page(principal: Principal = AuthDep, store: Storage = Depends(storage)):
        return webui.climbs_page(training.recurring_climbs(analysis.rides(store)))

    @app.get("/athlete", response_class=HTMLResponse, include_in_schema=False)
    def athlete_page(saved: bool = False, principal: Principal = AuthDep,
                     store: Storage = Depends(storage)):
        try:
            return webui.athlete_page(analysis.athlete(store), saved=saved)
        except (OSError, ValueError, TypeError) as exc:
            return webui.athlete_page(report.Athlete(), error=f"athlete.json unbrauchbar: {exc}")

    @app.post("/athlete", include_in_schema=False)
    async def save_athlete(request: Request, principal: Principal = AuthDep,
                           store: Storage = Depends(storage)):
        form = await _form(request)
        rider = _athlete_or_default(store)
        try:
            for name, kind in (("hr_max", int), ("hr_rest", int), ("rider_kg", float)):
                if name in form:
                    setattr(rider, name, _num(form[name], kind))
            for name in ("mass_kg", "cda", "crr"):
                if form.get(name):
                    setattr(rider, name, _num(form[name]))
            if form.get("zones_pct"):
                zones = tuple(float(z.replace(",", ".")) for z in form["zones_pct"].replace(";", " ").split())
                if len(zones) != 4 or list(zones) != sorted(zones):
                    raise ValueError("vier aufsteigende Zonengrenzen, z. B. 60 70 80 90")
                rider.zones_pct = zones
        except ValueError as exc:
            return HTMLResponse(webui.athlete_page(rider, error=str(exc)), status_code=400)
        analysis.save_athlete(store, rider)
        threading.Thread(target=_analyse, args=(store,), name="bikelog-analysis",
                         daemon=True).start()
        return RedirectResponse("/athlete?saved=1", status_code=status.HTTP_303_SEE_OTHER)

    # --- bikes ---

    def _reanalyse() -> None:
        threading.Thread(target=_analyse, args=(app.state.storage,), name="bikelog-analysis",
                         daemon=True).start()

    def _devices(store: Storage) -> list[str]:
        known = set(store.devices()) | {t.partition("=")[0].strip() for t in settings.pull_targets}
        return sorted(known - {importer.IMPORT_DEVICE})

    @app.get("/bikes", response_class=HTMLResponse, include_in_schema=False)
    def bikes_page(msg: str | None = Query(default=None, max_length=300),
                   principal: Principal = AuthDep, store: Storage = Depends(storage)):
        rides = analysis.rides(store)
        per_bike: dict[str, list] = {}
        for r in rides:
            per_bike.setdefault(r.bike or "", []).append(r)
        return webui.bikes_page(analysis.registry(store), _devices(store), per_bike,
                                analysis.today(), message=msg)

    def _back_bikes(text: str) -> RedirectResponse:
        return RedirectResponse("/bikes?" + urlencode({"msg": text}), status_code=status.HTTP_303_SEE_OTHER)

    @app.post("/bikes", include_in_schema=False)
    async def save_bike(request: Request, principal: Principal = AuthDep, store: Storage = Depends(storage)):
        form = await _form(request)
        if not form.get("name"):
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "name missing")
        reg = analysis.registry(store)
        bike = reg.bike(form.get("id")) or bikes.Bike(name=form["name"])
        if bike not in reg.bikes:
            reg.bikes.append(bike)
        try:
            bike.name = form["name"]
            bike.type = form.get("type") if form.get("type") in bikes.TYPES else bike.type
            bike.mass_kg, bike.cda, bike.crr = (_num(form.get(k, "")) for k in ("mass_kg", "cda", "crr"))
        except ValueError as exc:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, f"invalid value: {exc}")
        bike.notes = form.get("notes", "")
        reg.save(analysis.bikes_path(store))
        _reanalyse()
        return _back_bikes(f"{bike.name} gespeichert.")

    @app.post("/bikes/assign", include_in_schema=False)
    async def assign_bike(request: Request, principal: Principal = AuthDep, store: Storage = Depends(storage)):
        form = await _form(request)
        reg = analysis.registry(store)
        try:
            since = datetime.date.fromisoformat(form.get("since", ""))
        except ValueError:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "invalid date")
        if not form.get("device") or reg.bike(form.get("bike_id")) is None:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "device or bike missing")
        reg.assignments = [a for a in reg.assignments
                           if not (a.device == form["device"] and a.since == since)]
        reg.assignments.append(bikes.Assignment(form["device"], form["bike_id"], since))
        reg.save(analysis.bikes_path(store))
        _reanalyse()
        return _back_bikes(f"{form['device']} fährt ab {since:%d.%m.%Y} am {reg.bike(form['bike_id']).name}.")

    @app.post("/bikes/assign/delete", include_in_schema=False)
    async def unassign_bike(request: Request, principal: Principal = AuthDep, store: Storage = Depends(storage)):
        form = await _form(request)
        reg = analysis.registry(store)
        reg.assignments = [a for a in reg.assignments
                           if not (a.device == form.get("device") and a.since.isoformat() == form.get("since"))]
        reg.save(analysis.bikes_path(store))
        _reanalyse()
        return _back_bikes("Zuordnung entfernt.")

    # after the fixed /bikes/assign/... paths: {bike_id} would swallow "assign"
    @app.post("/bikes/{bike_id}/delete", include_in_schema=False)
    def delete_bike(bike_id: str, principal: Principal = AuthDep, store: Storage = Depends(storage)):
        reg = analysis.registry(store)
        bike = reg.bike(bike_id)
        if bike is None:
            raise HTTPException(status.HTTP_404_NOT_FOUND, "unknown bike")
        reg.bikes.remove(bike)
        reg.assignments = [a for a in reg.assignments if a.bike_id != bike_id]
        reg.save(analysis.bikes_path(store))
        _reanalyse()
        return _back_bikes(f"{bike.name} gelöscht.")

    @app.post("/ui/sessions/{session_id}/bike", include_in_schema=False)
    async def ui_session_bike(session_id: int, request: Request, principal: Principal = AuthDep,
                              store: Storage = Depends(storage)):
        _require(store, session_id)
        bike_id = (await _form(request)).get("bike_id") or None
        if bike_id and analysis.registry(store).bike(bike_id) is None:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "unknown bike")
        store.set_bike(session_id, bike_id)
        return RedirectResponse(f"/ride/{session_id}", status_code=status.HTTP_303_SEE_OTHER)

    # --- import ---

    @app.put(API + "/import/gpx", tags=["sessions"])
    async def import_gpx(request: Request,
                         bike_id: str | None = Query(default=None, description="bike of this ride (bikes.json)"),
                         event_id: str | None = Query(default=None,
                                                      description="an earlier edition of this goal"),
                         principal: Principal = AuthDep, store: Storage = Depends(storage)):
        """A ride recorded elsewhere (request body = the GPX file, with times): becomes a
        session of the device "import" and counts like any other ride."""
        payload = await request.body()
        if not payload or len(payload) > settings.max_upload_bytes:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "empty or too large")
        if bike_id and analysis.registry(store).bike(bike_id) is None:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "unknown bike")
        event = next((e for e in analysis.events(store) if e.id == event_id), None) if event_id else None
        if event_id and event is None:
            raise HTTPException(status.HTTP_404_NOT_FOUND, "unknown event")
        try:
            session = importer.import_gpx(store, payload, bike_id)
        except gpximport.NotARide as exc:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, str(exc))
        except Exception as exc:                    # ET.ParseError and friends
            raise HTTPException(status.HTTP_400_BAD_REQUEST, f"GPX unbrauchbar: {exc}")
        if event and session.id not in event.participations:
            event.participations.append(session.id)
            analysis.save_event(store, event)
        exporter.export_pending(store)
        _reanalyse()
        return session.as_dict()

    @app.post("/goals/{event_id}/participations/{session_id}/delete", include_in_schema=False)
    def drop_participation(event_id: str, session_id: int, principal: Principal = AuthDep,
                           store: Storage = Depends(storage)):
        event = next((e for e in analysis.events(store) if e.id == event_id), None)
        if event is None:
            raise HTTPException(status.HTTP_404_NOT_FOUND, "unknown event")
        event.participations = [s for s in event.participations if s != session_id]
        analysis.save_event(store, event)
        return RedirectResponse(f"/goals#e{event_id}", status_code=status.HTTP_303_SEE_OTHER)

    # --- training API ---

    @app.get(API + "/training", tags=["training"])
    def training_data(days: int = Query(default=180, ge=1, le=1100), weeks: int = Query(default=16, ge=1, le=156),
                      principal: Principal = AuthDep, store: Storage = Depends(storage)):
        """Daily load (TRIMP, fitness/fatigue/form) and weekly totals -- the
        figures behind /training, e.g. for an LLM."""
        rides = analysis.rides(store)
        today = analysis.today()
        load = training.load_series(rides, today - datetime.timedelta(days=days - 1), today)
        return {
            "today": today.isoformat(),
            "load": [{**vars(p), "day": p.day.isoformat()} for p in load],
            "weeks": [{**vars(w), "monday": w.monday.isoformat()} for w in training.weeks(rides, today, weeks)],
            "recurring_climbs": training.recurring_climbs(rides),
        }

    @app.get(API + "/events", tags=["training"])
    def list_events(principal: Principal = AuthDep, store: Storage = Depends(storage)):
        """Target events with the rider's readiness for each."""
        rides = analysis.rides(store)
        today = analysis.today()
        return [{**e.as_dict(), "readiness": training.readiness(e, rides, today)}
                for e in analysis.events(store)]

    @app.put(API + "/events/{event_id}/gpx", tags=["training"])
    async def put_event_gpx(event_id: str, request: Request, principal: Principal = AuthDep,
                            store: Storage = Depends(storage)):
        """Course GPX of an event (request body = the file): distance, ascent, climbs."""
        payload = await request.body()
        if not payload or len(payload) > settings.max_upload_bytes:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "empty or too large")
        try:
            event = analysis.set_event_gpx(store, event_id, payload)
        except KeyError:
            raise HTTPException(status.HTTP_404_NOT_FOUND, "unknown event")
        except Exception as exc:                    # ET.ParseError, no elevations, ...
            raise HTTPException(status.HTTP_400_BAD_REQUEST, f"GPX unbrauchbar: {exc}")
        return event.as_dict()

    @app.get(API + "/health", tags=["service"])
    def health(principal: Principal = AuthDep, store: Storage = Depends(storage)):
        return {
            "status": "ok",
            "sessions": store.count(),
            "auth_required": settings.require_auth,
            "principal": principal.name,
            "pull": puller is not None,
        }

    # --- sessions ---

    @app.get(API + "/sessions", tags=["sessions"])
    def list_sessions(limit: int = Query(default=100, ge=1, le=1000),
                      offset: int = Query(default=0, ge=0),
                      min_km: float | None = Query(default=None, ge=0,
                                                    description="only sessions with at least this much distance"),
                      max_km: float | None = Query(default=None, ge=0,
                                                    description="only sessions with at most this much distance"),
                      tests: bool = Query(default=False,
                                          description="include test sessions (simulator, GPS playback)"),
                      idle: bool = Query(default=False,
                                         description="include idle sessions (switched on, no ride)"),
                      principal: Principal = AuthDep,
                      store: Storage = Depends(storage)):
        min_m = min_km * 1000 if min_km is not None else None
        max_m = max_km * 1000 if max_km is not None else None
        return {"total": store.count(min_m, max_m, tests, idle),
                "sessions": [s.as_dict() for s in store.list(limit, offset, min_m, max_m, tests, idle)]}

    def _require(store: Storage, session_id: int) -> Session:
        session = store.get(session_id)
        if session is None:
            raise HTTPException(status.HTTP_404_NOT_FOUND, "unknown session")
        return session

    def _require_log(store: Storage, session_id: int) -> Session:
        session = _require(store, session_id)
        if session.file("L") is None:
            raise HTTPException(status.HTTP_404_NOT_FOUND, "session has no binary log")
        return session

    @app.get(API + "/sessions/{session_id}.gpx", tags=["export"])
    def download_gpx(
        session_id: int,
        max_fix_age_ms: int = Query(default=5000, ge=0),
        segment_gap_s: float = Query(default=60.0, ge=0),
        ele: str = Query(default="auto", pattern="^(auto|baro|gps)$"),
        max_accuracy_m: float = Query(default=0.0, ge=0),
        shocks: bool = Query(default=True),
        labels: bool = Query(default=True),
        rich: bool = Query(default=True, description="own extensions (gradient, road quality, labels, summary)"),
        sanitize: bool = Query(default=True, description="trim GPS jitter at stops (see bikelog.sanitize)"),
        principal: Principal = AuthDep,
        store: Storage = Depends(storage),
    ):
        session = _require_log(store, session_id)
        options = exporter.gpx_options(session, settings)
        options.max_fix_age_ms = max_fix_age_ms
        options.segment_gap_s = segment_gap_s
        options.ele_source = ele
        options.max_accuracy_m = max_accuracy_m
        options.shocks = shocks
        options.labels = labels
        options.rich = rich
        options.sanitize = sanitize
        xml, stats = exporter.render(store, session, options)
        if stats.written == 0:
            # Better a clear 409 than a valid but empty GPX that every
            # importer accepts and then shows as a track with no points.
            raise HTTPException(status.HTTP_409_CONFLICT,
                                "no usable GPS fix in this session (%s)" % stats.summary())
        return Response(content=xml, media_type="application/gpx+xml", headers={
            "Content-Disposition": 'attachment; filename="%s.gpx"' % _export_name(session),
            "X-Bikelog-Points": str(stats.written),
            "X-Bikelog-Segments": str(stats.segments),
        })

    @app.get(API + "/sessions/{session_id}.csv", tags=["export"])
    def download_csv(session_id: int, with_gps: bool = Query(default=False),
                     principal: Principal = AuthDep,
                     store: Storage = Depends(storage)):
        session = _require_log(store, session_id)
        buffer = io.StringIO()
        csvexport.write_stream(buffer, store.records(session, ReadStats()), with_gps=with_gps)
        return PlainTextResponse(buffer.getvalue(), media_type="text/csv", headers={
            "Content-Disposition": 'attachment; filename="%s.csv"' % _export_name(session)})

    def _report(store: Storage, session: Session) -> dict:
        try:
            return analysis.report_for(store, session)
        except (OSError, ValueError, TypeError) as exc:
            raise HTTPException(status.HTTP_500_INTERNAL_SERVER_ERROR,
                                f"{settings.athlete_path}: {exc}")

    @app.get(API + "/sessions/{session_id}/report.json", tags=["export"])
    def get_report(session_id: int, principal: Principal = AuthDep,
                   store: Storage = Depends(storage)):
        """All key figures of the session (bikelog.report) -- the input for a
        report generator."""
        return _report(store, _require_log(store, session_id))

    @app.get(API + "/sessions/{session_id}/report.md", tags=["export"])
    def get_report_md(session_id: int, principal: Principal = AuthDep,
                      store: Storage = Depends(storage)):
        """The same figures as a German Markdown report, without an LLM."""
        session = _require_log(store, session_id)
        return PlainTextResponse(report.to_markdown(_report(store, session)),
                                 media_type="text/markdown; charset=utf-8")

    @app.get(API + "/sessions/{session_id}/files/{name}", tags=["export"])
    def download_file(session_id: int, name: str, principal: Principal = AuthDep,
                      store: Storage = Depends(storage)):
        session = _require(store, session_id)
        if not any(f.name == name for f in session.files):
            raise HTTPException(status.HTTP_404_NOT_FOUND, "no such file in this session")
        text = name.endswith((".txt", ".log"))
        return Response(
            content=store.path_for(session, name).read_bytes(),
            media_type="text/plain; charset=utf-8" if text else "application/octet-stream",
            headers={} if text else {"Content-Disposition": 'attachment; filename="%s"' % name})

    # Declared after the ".gpx"/".csv" routes on purpose (those are int-typed
    # here, but keep the more specific routes first all the same).
    @app.get(API + "/sessions/{session_id}", tags=["sessions"])
    def get_session(session_id: int, principal: Principal = AuthDep,
                    store: Storage = Depends(storage)):
        return _require(store, session_id).as_dict()

    @app.delete(API + "/sessions/{session_id}", status_code=status.HTTP_204_NO_CONTENT,
                tags=["sessions"])
    def delete_session(session_id: int, principal: Principal = AuthDep,
                       store: Storage = Depends(storage)):
        """Removes the files; the index keeps a tombstone so a pull does not bring
        the session back from the SD card. An explicit PUT revives it."""
        if not store.delete(session_id):
            raise HTTPException(status.HTTP_404_NOT_FOUND, "unknown session")
        return Response(status_code=status.HTTP_204_NO_CONTENT)

    # --- Komoot upload ---

    @app.post(API + "/sessions/{session_id}/komoot", tags=["export"])
    def upload_to_komoot(
        session_id: int,
        force: bool = Query(default=False, description="upload again even if already done"),
        principal: Principal = AuthDep,
        store: Storage = Depends(storage),
    ):
        """Uploads the merged tour (see komoot.py) that this session is part
        of. Every session in that tour is marked with the result, so calling
        this on any one of them shows the same status afterwards."""
        session = _require_log(store, session_id)
        group = komoot.group_for(store, session)
        if not force and any(s.komoot_status == "uploaded" for s in group):
            done = next(s for s in group if s.komoot_status == "uploaded")
            raise HTTPException(status.HTTP_409_CONFLICT,
                                "already uploaded (uploaded_at=%s); retry with ?force=true"
                                % done.komoot_uploaded_at)
        result = komoot.upload_group(store, group)
        if result.status in ("not-configured", "too-short", "no-gps"):
            raise HTTPException(status.HTTP_409_CONFLICT, result.message or result.status)
        if result.status == "error":
            raise HTTPException(status.HTTP_502_BAD_GATEWAY, result.message or "upload failed")
        return {"status": result.status, "session_ids": result.session_ids}

    @app.post(API + "/sessions/{session_id}/komoot/ignore", tags=["export"])
    def komoot_ignore(session_id: int, ask_again: bool = Query(default=False),
                      principal: Principal = AuthDep, store: Storage = Depends(storage)):
        """Answers the page's question "upload to Komoot?" with no (or, with ask_again,
        puts the question back). Applies to the whole merged tour."""
        session = _require_log(store, session_id)
        group = komoot.group_for(store, session)
        store.set_komoot_prompt([s.id for s in group], None if ask_again else "ignored")
        return {"status": "asking" if ask_again else "ignored", "session_ids": [s.id for s in group]}

    # --- push ---

    @app.put(API + "/devices/{device}/files/{path:path}", tags=["sessions"])
    async def put_file(
        request: Request,
        device: str = Path(pattern=DEVICE_RE),
        path: str = Path(description="Path below /BIKECOMP/, e.g. 20260920/L_143012.bin"),
        principal: Principal = AuthDep,
        store: Storage = Depends(storage),
    ):
        """Store one file of a session (request body = the file, unchanged).

        Idempotent: the same bytes again are ``unchanged``, different bytes
        replace the stored file. A device or script may retry blindly.
        """
        try:
            sdfile = sdlayout.parse_path(path)
        except sdlayout.BadPath as exc:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, str(exc))
        if sdfile.day == sdlayout.WORKDIR:
            raise HTTPException(status.HTTP_409_CONFLICT,
                                "files of the running session are not taken")
        payload = await request.body()
        if not payload:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "empty body")
        if len(payload) > settings.max_upload_bytes:
            raise HTTPException(413, "upload exceeds %d bytes" % settings.max_upload_bytes)
        result = store.put_file(device, sdfile, payload, source="push:" + principal.name)
        exporter.export_pending(store)
        # Nextcloud is a network call (WebDAV) and the report takes a while --
        # don't hold up the device's upload on either; the session's
        # gpx_status is already current above.
        threading.Thread(target=_sync_and_analyse, args=(store,),
                         name="bikelog-nextcloud", daemon=True).start()
        return {"status": result.status, "session": result.session.as_dict()}

    # --- pull ---

    @app.get(API + "/pull", tags=["service"])
    def pull_status(principal: Principal = AuthDep):
        return puller.as_dict() if puller else {"enabled": False}

    @app.post(API + "/pull", status_code=status.HTTP_202_ACCEPTED, tags=["service"])
    def pull_now(device: str | None = Query(default=None), principal: Principal = AuthDep):
        """Check the device(s) now instead of waiting for mDNS or the next poll."""
        if not puller:
            raise HTTPException(status.HTTP_409_CONFLICT, "pulling is disabled (BIKELOG_PULL)")
        puller.trigger(device)
        return {"triggered": device or "all"}

    return app


def _export_name(session: Session) -> str:
    return re.sub(r"[^A-Za-z0-9_-]", "", f"{session.device}_{session.day}{session.stem}")


app = create_app()
