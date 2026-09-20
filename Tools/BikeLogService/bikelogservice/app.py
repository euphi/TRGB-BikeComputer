"""The bike log upload service.

Scope of this step: take raw .bin uploads from the bike computer, index
them, and hand them back out as GPX or CSV. Publishing to Nextcloud/Komoot/
Strava and the heatmap come later and hang off the same storage -- which is
why the raw uploads are kept and everything else is derived.

Run it:  uvicorn bikelogservice.app:app
API doc: http://<host>:8000/docs (FastAPI generates it from the routes)
"""

from __future__ import annotations

import io

from fastapi import Depends, FastAPI, HTTPException, Query, Request, Response, status
from fastapi.responses import JSONResponse, PlainTextResponse

from bikelog import csvexport, gpx
from bikelog.record import ReadStats

from .auth import AuthDep, Principal
from .config import Settings
from .storage import DuplicateRide, InvalidLog, Storage

API = "/api/v1"


def create_app(settings: Settings | None = None) -> FastAPI:
    settings = settings or Settings.from_env()
    app = FastAPI(
        title="BikeLog Service",
        version="1.0.0",
        description="Rohlogs des Fahrradcomputers annehmen, indizieren und "
                    "als GPX/CSV ausliefern.",
    )
    app.state.settings = settings
    app.state.storage = Storage(settings)

    def storage(request: Request) -> Storage:
        return request.app.state.storage

    # ------------------------------------------------------------------
    # Every route below takes the auth dependency, even while it returns an
    # anonymous principal -- see auth.py. That is what makes switching auth
    # on a config change instead of a sweep through the routes.
    # ------------------------------------------------------------------

    @app.get(API + "/health", tags=["service"])
    def health(principal: Principal = AuthDep, store: Storage = Depends(storage)):
        """Cheap liveness probe -- what the ESP32 checks before uploading."""
        return {
            "status": "ok",
            "rides": store.count(),
            "auth_required": settings.require_auth,
            "principal": principal.name,
        }

    @app.post(API + "/rides", status_code=status.HTTP_201_CREATED, tags=["rides"])
    async def upload_ride(
        request: Request,
        filename: str = Query(default="upload.bin",
                              description="Original name on the SD card"),
        device: str | None = Query(default=None, description="Device identifier"),
        principal: Principal = AuthDep,
        store: Storage = Depends(storage),
    ):
        """Upload one raw binary log (request body = the file, unchanged).

        Idempotent: uploading the same content again returns 200 with the
        same ride id instead of creating a duplicate. The firmware may
        therefore retry blindly after a dropped connection.
        """
        payload = await request.body()
        if not payload:
            raise HTTPException(status.HTTP_400_BAD_REQUEST, "empty body")
        if len(payload) > settings.max_upload_bytes:
            raise HTTPException(413,
                                "upload exceeds %d bytes" % settings.max_upload_bytes)
        try:
            ride = store.add(payload, filename=filename, device=device,
                             uploaded_by=principal.name)
        except DuplicateRide as dup:
            return JSONResponse(status_code=status.HTTP_200_OK,
                                content={"duplicate": True, **dup.ride.as_dict()})
        except InvalidLog as exc:
            raise HTTPException(422, str(exc))
        return {"duplicate": False, **ride.as_dict()}

    @app.get(API + "/rides", tags=["rides"])
    def list_rides(limit: int = Query(default=100, ge=1, le=1000),
                   offset: int = Query(default=0, ge=0),
                   principal: Principal = AuthDep,
                   store: Storage = Depends(storage)):
        return {"total": store.count(),
                "rides": [ride.as_dict() for ride in store.list(limit, offset)]}

    def _require(store: Storage, ride_id: str):
        ride = store.get(ride_id)
        if ride is None:
            raise HTTPException(status.HTTP_404_NOT_FOUND, "unknown ride")
        return ride

    @app.get(API + "/rides/{ride_id}.bin", tags=["export"])
    def download_raw(ride_id: str, principal: Principal = AuthDep,
                     store: Storage = Depends(storage)):
        ride = _require(store, ride_id)
        return Response(
            content=store.path_for(ride_id).read_bytes(),
            media_type="application/octet-stream",
            headers={"Content-Disposition": 'attachment; filename="%s"' % ride.filename})

    @app.get(API + "/rides/{ride_id}.gpx", tags=["export"])
    def download_gpx(
        ride_id: str,
        max_fix_age_ms: int = Query(default=5000, ge=0),
        segment_gap_s: float = Query(default=60.0, ge=0),
        ele: str = Query(default="auto", pattern="^(auto|baro|gps)$"),
        max_accuracy_m: float = Query(default=0.0, ge=0),
        principal: Principal = AuthDep,
        store: Storage = Depends(storage),
    ):
        ride = _require(store, ride_id)
        options = gpx.GpxOptions(max_fix_age_ms=max_fix_age_ms,
                                 segment_gap_s=segment_gap_s,
                                 ele_source=ele,
                                 max_accuracy_m=max_accuracy_m)
        xml, stats = gpx.to_string(store.records(ride_id), options)
        if stats.written == 0:
            # Better a clear 409 than a valid but empty GPX that every
            # importer accepts and then shows as a track with no points.
            raise HTTPException(status.HTTP_409_CONFLICT,
                                "no usable GPS fix in this ride (%s)" % stats.summary())
        return Response(content=xml, media_type="application/gpx+xml", headers={
            "Content-Disposition": 'attachment; filename="%s.gpx"' % ride_id,
            "X-Bikelog-Points": str(stats.written),
            "X-Bikelog-Segments": str(stats.segments),
        })

    @app.get(API + "/rides/{ride_id}.csv", tags=["export"])
    def download_csv(ride_id: str, with_gps: bool = Query(default=False),
                     principal: Principal = AuthDep,
                     store: Storage = Depends(storage)):
        _require(store, ride_id)
        buffer = io.StringIO()
        csvexport.write_stream(buffer, store.records(ride_id, ReadStats()),
                               with_gps=with_gps)
        return PlainTextResponse(
            buffer.getvalue(), media_type="text/csv",
            headers={"Content-Disposition": 'attachment; filename="%s.csv"' % ride_id})

    # Declared after the ".gpx"/".csv"/".bin" routes on purpose: this
    # pattern would otherwise match those suffixes as part of the id.
    @app.get(API + "/rides/{ride_id}", tags=["rides"])
    def get_ride(ride_id: str, principal: Principal = AuthDep,
                 store: Storage = Depends(storage)):
        return _require(store, ride_id).as_dict()

    @app.delete(API + "/rides/{ride_id}", status_code=status.HTTP_204_NO_CONTENT,
                tags=["rides"])
    def delete_ride(ride_id: str, principal: Principal = AuthDep,
                    store: Storage = Depends(storage)):
        if not store.delete(ride_id):
            raise HTTPException(status.HTTP_404_NOT_FOUND, "unknown ride")
        return Response(status_code=status.HTTP_204_NO_CONTENT)

    return app


app = create_app()
