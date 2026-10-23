"""Authentication -- deliberately a no-op today, and the only place that
has to change when it stops being one.

The point of this module is that *every* route already depends on
``require_principal``. Turning auth on is a config flag, not a refactor:
no route signature changes, no client changes beyond a header the firmware
sends from the start (empty while auth is off).

Bearer tokens rather than mTLS or request signing, because the ESP32 side of
this has to stay two lines in an HTTPClient call -- no certificate store, no
clock dependency, no HMAC over a streamed body it cannot buffer.
"""

from __future__ import annotations

from dataclasses import dataclass

from fastapi import Depends, Header, HTTPException, Request, status

from .config import Settings


@dataclass
class Principal:
    """Who is making the request. ``anonymous`` while auth is off."""

    name: str
    anonymous: bool = False


ANONYMOUS = Principal(name="anonymous", anonymous=True)


def _lookup(settings: Settings, token: str) -> Principal | None:
    for entry in settings.tokens:
        value, _, name = entry.partition(":")
        if _constant_time_eq(value, token):
            return Principal(name=name or "device")
    return None


def _constant_time_eq(a: str, b: str) -> bool:
    import hmac

    return hmac.compare_digest(a.encode(), b.encode())


def require_principal(
    request: Request,
    authorization: str | None = Header(default=None),
) -> Principal:
    settings: Settings = request.app.state.settings
    if not settings.require_auth:
        return ANONYMOUS
    if not authorization or not authorization.lower().startswith("bearer "):
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Bearer token required",
                            headers={"WWW-Authenticate": "Bearer"})
    principal = _lookup(settings, authorization[7:].strip())
    if principal is None:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "Unknown token",
                            headers={"WWW-Authenticate": "Bearer"})
    return principal


def require_trailbridge(
    request: Request,
    authorization: str | None = Header(default=None),
) -> Principal:
    """The route feed for TrailBridge: its own token (BIKELOG_TRAILBRIDGE_TOKEN) if one is
    set -- the service's tokens count too while auth is on --, else the same as every route."""
    settings: Settings = request.app.state.settings
    if not settings.trailbridge_token:
        return require_principal(request, authorization)
    token = authorization[7:].strip() if authorization and authorization.lower().startswith("bearer ") else ""
    if token and _constant_time_eq(settings.trailbridge_token, token):
        return Principal(name="trailbridge")
    principal = _lookup(settings, token) if token and settings.require_auth else None
    if principal is None:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "TrailBridge token required",
                            headers={"WWW-Authenticate": "Bearer"})
    return principal


#: Convenience alias so routes read as ``principal: Principal = AuthDep``.
AuthDep = Depends(require_principal)
TrailBridgeDep = Depends(require_trailbridge)
