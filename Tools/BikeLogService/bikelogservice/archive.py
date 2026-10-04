"""Idle sessions out of the way: archived after Settings.idle_archive_days.

An idle session is one in which the bike computer was switched on and nothing
happened (bikelog.testride.idle: the wheel stood, the position got nowhere), or
which has no binary log at all -- debug output only. The pages never list them;
after a week (by default) their files move from ``sessions/`` to ``archive/``,
same tree (``archive/<device>/<day>/<name>``), so they can still be looked at
with a shell when a debug log is needed.

The session's index row stays behind as a tombstone (deleted_at, archived_at)
for as long as the files are on the SD card -- without it the next pull would
fetch them again. The pull drops the row once they are gone from the card
(Storage.purge_gone), and ``delete_on_device()`` removes them from the card
right away while the bike computer is reachable (firmware: ``GET /del/<path>``),
then the row too. Nothing is queued: offline means "not now". An explicit upload
of one of its files revives the session, same as for a deleted one.
"""

from __future__ import annotations

import datetime
import logging
import shutil

from .sdlayout import SdFile
from .storage import Storage

log = logging.getLogger("bikelog.archive")


def move_files(store: Storage, session) -> None:
    """The session's files from sessions/ to archive/ (same tree), its GPX away."""
    target = store.settings.archive_dir / session.device / (session.day or "_")
    for f in session.files:
        src = store.path_for(session, f.name)
        if src.exists():
            target.mkdir(parents=True, exist_ok=True)
            shutil.move(str(src), str(target / f.name))
    if session.gpx_file:
        (store.settings.gpx_dir / session.gpx_file).unlink(missing_ok=True)


def delete_on_device(store: Storage, puller, sessions) -> tuple[int, list[str]]:
    """Delete these sessions' files on the bike computer now (puller.delete_files raises
    DeviceUnavailable when it is not reachable). A session whose files are all gone from
    the card is archived here (if it was not yet) and forgotten by the index.
    Returns (sessions done, error messages)."""
    done, errors = 0, []
    for session in sessions:
        paths = [SdFile(session.day, f.name).path for f in session.files]
        results = puller.delete_files(session.device, paths)
        failed = [f"{path}: {msg}" for path, ok, msg in results if not ok]
        if failed:
            errors.extend(failed)
            continue
        if session.deleted_at is None:
            try:
                move_files(store, session)
            except OSError as exc:
                errors.append(f"{session.day}/{session.stem}: archive: {exc}")
                continue
        store.remove_index(session.id)
        done += 1
    if done:
        log.info("%d session(s) deleted on the device and archived", done)
    return done, errors


def archive_idle(store: Storage, now: datetime.datetime | None = None) -> int:
    """Move idle sessions older than idle_archive_days to the archive; returns how many."""
    days = store.settings.idle_archive_days
    if not days or days <= 0:
        return 0
    now = now or datetime.datetime.now(datetime.timezone.utc)
    cutoff = (now - datetime.timedelta(days=days)).isoformat(timespec="seconds")
    done = 0
    for session in store.idle_to_archive(cutoff):
        try:
            move_files(store, session)
        except OSError as exc:
            log.warning("archiving session %d failed: %s", session.id, exc)
            continue
        store.mark_archived(session.id)
        done += 1
    if done:
        log.info("%d idle session(s) archived to %s", done, store.settings.archive_dir)
    return done
