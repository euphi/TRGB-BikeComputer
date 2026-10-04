"""Sync Tours-status GPX files to Nextcloud via WebDAV.

Only the "sensible" ones (see exporter.py: real rides, gpx_status "ok") are
synced -- a Debug_Archive file, or one that has moved there since (an
export-logic improvement re-classified an old ride), is removed from
Nextcloud again rather than uploaded, so the remote folder only ever holds
what Tours/ holds locally.

Unlike Komoot, this is meant to just run: no manual trigger, no per-session
button, gated purely on Settings.nextcloud_enabled (URL, user and an app
password all set). Auth is HTTP Basic with that app password -- Nextcloud's
own recommended way to give a script account access without the real login
password, and independently revocable (Settings -> Security -> "Devices &
sessions").

Known gap: deleting a session (Storage.delete()) does not remove its
Nextcloud copy, only the local one -- deletion is rare enough, and manual
enough, that clearing it out by hand when it happens is an acceptable cost
for not having Storage depend on this (optional, network-calling) module.

requests is only imported once a sync is actually attempted, so the rest of
the service works without "pip install bikelog[nextcloud]" installed.
"""

from __future__ import annotations

import logging
import threading

from .storage import Session, Storage

log = logging.getLogger("bikelog.nextcloud")


def _dav_url(settings, path: str) -> str:
    return f"{settings.nextcloud_url}/remote.php/dav/files/{settings.nextcloud_user}/{path.lstrip('/')}"


def _ensure_remote_dir(http, settings) -> None:
    """MKCOL every path segment of nextcloud_dir. 405 ("already exists") is
    the normal, expected case after the first run, not an error."""
    built = ""
    for part in (p for p in settings.nextcloud_dir.strip("/").split("/") if p):
        built = f"{built}/{part}" if built else part
        resp = http.request("MKCOL", _dav_url(settings, built))
        if resp.status_code not in (201, 405):
            resp.raise_for_status()


def upload(settings, local_path, remote_name: str) -> None:
    import requests

    with requests.Session() as http:
        http.auth = (settings.nextcloud_user, settings.nextcloud_password)
        _ensure_remote_dir(http, settings)
        with open(local_path, "rb") as fh:
            resp = http.put(_dav_url(settings, f"{settings.nextcloud_dir}/{remote_name}"), data=fh)
        resp.raise_for_status()


def delete(settings, remote_name: str) -> None:
    import requests

    with requests.Session() as http:
        http.auth = (settings.nextcloud_user, settings.nextcloud_password)
        resp = http.delete(_dav_url(settings, f"{settings.nextcloud_dir}/{remote_name}"))
        if resp.status_code not in (204, 404):
            resp.raise_for_status()


def sync_session(store: Storage, session: Session) -> str:
    """(Re)uploads a Tours file, or removes a stale one -- see the module
    docstring for which is which."""
    settings = store.settings
    if session.gpx_status != "ok":
        if session.nextcloud_file:
            try:
                delete(settings, session.nextcloud_file)
            except Exception as exc:
                log.error("Nextcloud cleanup failed for session %s: %s", session.id, exc)
                return "error"
        store.set_nextcloud(session.id, None, None, None)
        return "removed"
    local_path = settings.gpx_dir / session.gpx_file
    remote_name = local_path.name
    try:
        upload(settings, local_path, remote_name)
    except Exception as exc:
        log.error("Nextcloud upload failed for session %s: %s", session.id, exc)
        store.set_nextcloud(session.id, "error", session.nextcloud_file, session.nextcloud_gpx_version)
        return "error"
    store.set_nextcloud(session.id, "ok", remote_name, session.gpx_version)
    return "ok"


#: Startup, puller and upload threads may all call in; two syncs at once would upload
#: (or delete) the same file twice.
_lock = threading.Lock()


def sync_pending(store: Storage) -> dict[str, int]:
    if not store.settings.nextcloud_enabled:
        return {}
    counts: dict[str, int] = {}
    with _lock:
        for session in store.pending_nextcloud():
            status = sync_session(store, session)
            counts[status] = counts.get(status, 0) + 1
    if counts:
        log.info("Nextcloud sync to %s: %s", store.settings.nextcloud_dir,
                 ", ".join(f"{n} {k}" for k, n in sorted(counts.items())))
    return counts
