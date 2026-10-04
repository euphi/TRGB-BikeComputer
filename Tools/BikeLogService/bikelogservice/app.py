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
import io
import re
import threading
from contextlib import asynccontextmanager
from urllib.parse import urlencode

from fastapi import Depends, FastAPI, HTTPException, Path, Query, Request, Response, status
from fastapi.responses import HTMLResponse, PlainTextResponse, RedirectResponse

from bikelog import csvexport
from bikelog.record import ReadStats

from . import exporter, komoot, nextcloud, sdlayout, webui
from .auth import AuthDep, Principal
from .config import Settings
from .puller import Puller
from .storage import Session, Storage

API = "/api/v1"
DEVICE_RE = r"^[A-Za-z0-9_-]{1,32}$"


def create_app(settings: Settings | None = None, puller: Puller | None = None) -> FastAPI:
    settings = settings or Settings.from_env()
    store = Storage(settings)
    if puller is None and settings.pull:
        puller = Puller(store, settings)

    def _export_and_sync(store: Storage) -> None:
        exporter.export_pending(store)
        nextcloud.sync_pending(store)

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
    def index(min_km: float | None = Query(default=None, ge=0),
             max_km: float | None = Query(default=None, ge=0),
             msg: str | None = Query(default=None, max_length=300),
             principal: Principal = AuthDep, store: Storage = Depends(storage)):
        min_m = min_km * 1000 if min_km is not None else None
        max_m = max_km * 1000 if max_km is not None else None
        sessions = store.list(limit=500, min_distance_m=min_m, max_distance_m=max_m)
        return webui.index(sessions, puller.as_dict() if puller else None,
                           min_km=min_km, max_km=max_km,
                           prompts=komoot.pending_prompts(store), message=msg)

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

    @app.post("/ui/sessions/{session_id}/komoot-ignore", include_in_schema=False)
    def ui_komoot_ignore(session_id: int, principal: Principal = AuthDep,
                         store: Storage = Depends(storage)):
        session = _require(store, session_id)
        store.set_komoot_prompt([s.id for s in komoot.group_for(store, session)], "ignored")
        return _back(session_id, "Gut, diese Fahrt wird nicht zu Komoot hochgeladen.")

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
                      principal: Principal = AuthDep,
                      store: Storage = Depends(storage)):
        min_m = min_km * 1000 if min_km is not None else None
        max_m = max_km * 1000 if max_km is not None else None
        return {"total": store.count(min_m, max_m),
                "sessions": [s.as_dict() for s in store.list(limit, offset, min_m, max_m)]}

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
        # Nextcloud is a network call (WebDAV) -- don't hold up the device's
        # upload on it; the session's gpx_status is already current above.
        threading.Thread(target=nextcloud.sync_pending, args=(store,),
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
