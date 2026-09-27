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
from contextlib import asynccontextmanager

from fastapi import Depends, FastAPI, HTTPException, Path, Query, Request, Response, status
from fastapi.responses import HTMLResponse, PlainTextResponse

from bikelog import csvexport, gpx
from bikelog.record import ReadStats, split

from . import sdlayout, webui
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

    @asynccontextmanager
    async def lifespan(app: FastAPI):
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
    def index(principal: Principal = AuthDep, store: Storage = Depends(storage)):
        return webui.index(store.list(limit=500), puller.as_dict() if puller else None)

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
                      principal: Principal = AuthDep,
                      store: Storage = Depends(storage)):
        return {"total": store.count(),
                "sessions": [s.as_dict() for s in store.list(limit, offset)]}

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
        principal: Principal = AuthDep,
        store: Storage = Depends(storage),
    ):
        session = _require_log(store, session_id)
        options = gpx.GpxOptions(max_fix_age_ms=max_fix_age_ms,
                                 segment_gap_s=segment_gap_s,
                                 ele_source=ele,
                                 max_accuracy_m=max_accuracy_m)
        records, _, shock_events = split(store.records(session, types=None))
        xml, stats = gpx.to_string(records, options, shock_events if shocks else None)
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
