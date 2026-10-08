"""Session storage: the SD card's files on disk, a SQLite index next to them.

A session is everything one boot of the bike computer wrote -- L_ (binary log),
I_ (summary), D_ (debug log), S_/R_ (raw IMU), T_ (time hints), N_ (NMEA), all
sharing one stem. The files are kept verbatim under a tree that mirrors the SD
card (``sessions/<device>/<day>/<name>``), so they can be looked at with
nothing but a shell. GPX, CSV and the rest are derived on demand: improving an
exporter improves every past ride.

Storing a file is idempotent. The same bytes again are "unchanged", different
bytes under the same name replace the old ones (the device rewrites I_ files
when their format changes). That is what lets both the puller and a pushing
device simply send again after an interruption, without bookkeeping of their own.

Deleting a session keeps its index rows as a tombstone: the files are still on
the SD card, and without one the next pull would bring the session straight back.
"""

from __future__ import annotations

import datetime
import hashlib
import io
import json
import sqlite3
import threading
from dataclasses import asdict, dataclass, field
from pathlib import Path

from bikelog import testride
from bikelog.record import ReadStats, UnknownLogFormat, read_stream

from . import sdlayout
from .sdlayout import SdFile

SCHEMA_VERSION = 10

SCHEMA = """
CREATE TABLE IF NOT EXISTS sessions (
    id              INTEGER PRIMARY KEY,
    device          TEXT NOT NULL,
    day             TEXT NOT NULL,
    stem            TEXT NOT NULL,
    created_at      TEXT NOT NULL,
    updated_at      TEXT NOT NULL,
    deleted_at      TEXT,
    summary         TEXT,               -- I_*.txt as JSON
    -- derived from the L_ file:
    format_version  INTEGER,
    record_count    INTEGER,
    trailing_bytes  INTEGER,
    first_time      INTEGER,
    last_time       INTEGER,
    distance_m      REAL,
    gps_points      INTEGER,
    clock_unset     INTEGER NOT NULL DEFAULT 0,
    log_error       TEXT,
    -- automatic GPX export (exporter.py):
    gpx_file        TEXT,
    gpx_status      TEXT,
    gpx_version     INTEGER,
    gpx_dirty       INTEGER NOT NULL DEFAULT 1,
    -- Komoot upload (komoot.py): set on every session of the merged tour a
    -- session ended up part of, so any of them shows the same result.
    komoot_status   TEXT,
    komoot_tour_id  TEXT,
    komoot_uploaded_at TEXT,
    -- The question "upload this ride to Komoot?" (webui.py): NULL = still to be asked,
    -- 'ignored' = the rider said no. Uploading is komoot_status, not this.
    komoot_prompt   TEXT,
    -- test session (bikelog.testride: "sim" | "gps_playback", NULL = ride) as the
    -- log says, and the rider's verdict over it ("test" | "real", NULL = as detected)
    test_kind       TEXT,
    test_override   TEXT,
    -- no ride: switched on, nothing happened (bikelog.testride.idle), or no L_ file at
    -- all (yet). 1 until an L_ file says otherwise. Archived after a while (archive.py):
    idle            INTEGER NOT NULL DEFAULT 1,
    archived_at     TEXT,
    -- the bike of this session when it is not the device's (imports; later: told apart by
    -- the sensors). NULL: the device's bike on that day, bikes.json
    bike_id         TEXT,
    -- Nextcloud sync (nextcloud.py): the Tours file, kept up to date the
    -- same way the local export is -- see nextcloud_gpx_version below.
    nextcloud_status    TEXT,
    nextcloud_file      TEXT,
    nextcloud_synced_at TEXT,
    nextcloud_gpx_version INTEGER,
    UNIQUE (device, day, stem)
);
-- Session reports (bikelog.report) as computed, see analysis.py. cache_key says
-- what they were computed from; a different key means compute again.
CREATE TABLE IF NOT EXISTS reports (
    session_id      INTEGER PRIMARY KEY REFERENCES sessions(id) ON DELETE CASCADE,
    cache_key       TEXT NOT NULL,
    report          TEXT NOT NULL,
    computed_at     TEXT NOT NULL
);
-- Reports of tours made of several sessions (tours.py, analysis.py), keyed by what
-- they were computed from (the sessions' cache keys); a stale one is simply not found.
CREATE TABLE IF NOT EXISTS tour_reports (
    cache_key       TEXT PRIMARY KEY,
    report          TEXT NOT NULL,
    computed_at     TEXT NOT NULL
);
-- Ride reports as prose from a local LLM (llm.py, bikelog.narrate). subject names the ride
-- ("tour:<first session id>"), prompt_key what the text was written from.
CREATE TABLE IF NOT EXISTS narratives (
    subject         TEXT PRIMARY KEY,
    prompt_key      TEXT NOT NULL,
    model           TEXT NOT NULL,
    text            TEXT NOT NULL,
    unverified      TEXT NOT NULL,
    seconds         REAL,
    created_at      TEXT NOT NULL
);
CREATE TABLE IF NOT EXISTS files (
    session_id      INTEGER NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
    name            TEXT NOT NULL,
    size            INTEGER NOT NULL,
    sha256          TEXT NOT NULL,
    stored_at       TEXT NOT NULL,
    source          TEXT,
    PRIMARY KEY (session_id, name)
);
"""

#: Timestamps below this mean the ESP32 clock was never set -- see
#: bikelog.gpx.MIN_PLAUSIBLE_YEAR. LogSessions corrects most of these on the
#: device, the flag catches the rest.
MIN_PLAUSIBLE_TIMESTAMP = int(
    datetime.datetime(2020, 1, 1, tzinfo=datetime.timezone.utc).timestamp())


def _now() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")


@dataclass
class StoredFile:
    name: str
    size: int
    sha256: str
    stored_at: str
    source: str | None

    @property
    def kind(self) -> str:
        return self.name[0].upper()


@dataclass
class Session:
    id: int
    device: str
    day: str
    stem: str
    created_at: str
    updated_at: str
    deleted_at: str | None
    summary: dict | None
    format_version: int | None
    record_count: int | None
    trailing_bytes: int | None
    first_time: int | None
    last_time: int | None
    distance_m: float | None
    gps_points: int | None
    clock_unset: int
    log_error: str | None
    gpx_file: str | None = None
    gpx_status: str | None = None
    gpx_version: int | None = None
    gpx_dirty: int = 1
    komoot_status: str | None = None
    komoot_tour_id: str | None = None
    komoot_uploaded_at: str | None = None
    komoot_prompt: str | None = None
    test_kind: str | None = None
    test_override: str | None = None
    idle: int = 1
    archived_at: str | None = None
    bike_id: str | None = None
    nextcloud_status: str | None = None
    nextcloud_file: str | None = None
    nextcloud_synced_at: str | None = None
    nextcloud_gpx_version: int | None = None
    files: list[StoredFile] = field(default_factory=list)

    @property
    def start_time(self) -> int | None:
        """Best known start: the device's summary, else the first log record."""
        start = (self.summary or {}).get("start")
        if isinstance(start, int) and start >= MIN_PLAUSIBLE_TIMESTAMP:
            return start
        return self.first_time if self.first_time and not self.clock_unset else None

    def file(self, kind: str) -> StoredFile | None:
        return next((f for f in self.files if f.kind == kind), None)

    @property
    def is_test(self) -> bool:
        """Simulator or GPS playback, not a ride -- unless the rider said otherwise."""
        if self.test_override:
            return self.test_override == "test"
        return bool(self.test_kind)

    def as_dict(self) -> dict:
        data = asdict(self)
        data["clock_unset"] = bool(self.clock_unset)
        data["gpx_dirty"] = bool(self.gpx_dirty)
        data["start_time"] = self.start_time
        data["is_test"] = self.is_test
        data["idle"] = bool(self.idle)
        data["duration_s"] = (self.last_time - self.first_time
                              if self.first_time and self.last_time else None)
        data["files"] = [asdict(f) for f in self.files]
        return data


#: SQL for Session.is_test
#: (COALESCE: never NULL, so NOT TEST_SQL keeps the rides -- NULL = 'test' would drop them)
TEST_SQL = ("(COALESCE(test_override, CASE WHEN test_kind IS NULL THEN 'real' ELSE 'test' END)"
            " = 'test')")


def _distance_clause(min_distance_m: float | None, max_distance_m: float | None,
                     tests: bool = True, idle: bool = True) -> tuple[str, tuple]:
    clause, params = ("" if tests else f"AND NOT {TEST_SQL} ") + ("" if idle else "AND idle = 0 "), []
    if min_distance_m is not None:
        clause += "AND distance_m >= ? "
        params.append(min_distance_m)
    if max_distance_m is not None:
        clause += "AND distance_m <= ? "
        params.append(max_distance_m)
    return clause, tuple(params)


@dataclass
class PutResult:
    session: Session
    status: str             # "new" | "unchanged" | "replaced"


class Storage:
    def __init__(self, settings):
        self.settings = settings
        settings.ensure_dirs()
        # One connection shared by the request threads and the puller; sqlite3
        # objects are not safe for concurrent use, hence the lock around every use.
        self._lock = threading.RLock()
        self._db = sqlite3.connect(settings.db_path, check_same_thread=False)
        self._db.row_factory = sqlite3.Row
        self._db.execute("PRAGMA foreign_keys = ON")
        version = self._db.execute("PRAGMA user_version").fetchone()[0]
        if version < 2:
            # v1 indexed single uploaded rides and never held real data.
            self._db.execute("DROP TABLE IF EXISTS rides")
        self._db.executescript(SCHEMA)
        if version == 2:
            for column in ("gpx_file TEXT", "gpx_status TEXT", "gpx_version INTEGER",
                           "gpx_dirty INTEGER NOT NULL DEFAULT 1"):
                self._db.execute(f"ALTER TABLE sessions ADD COLUMN {column}")
        if version == 3:
            for column in ("komoot_status TEXT", "komoot_tour_id TEXT", "komoot_uploaded_at TEXT"):
                self._db.execute(f"ALTER TABLE sessions ADD COLUMN {column}")
        if version == 4:
            for column in ("nextcloud_status TEXT", "nextcloud_file TEXT", "nextcloud_synced_at TEXT",
                           "nextcloud_gpx_version INTEGER"):
                self._db.execute(f"ALTER TABLE sessions ADD COLUMN {column}")
        # Columns added without a re-derive: just make sure they are there.
        columns = {r[1] for r in self._db.execute("PRAGMA table_info(sessions)")}
        for column, kind in (("bike_id", "TEXT"),):
            if column not in columns:
                self._db.execute(f"ALTER TABLE sessions ADD COLUMN {column} {kind}")
        self._db.execute(f"PRAGMA user_version = {SCHEMA_VERSION}")
        self._db.commit()
        if 0 < version < 7:
            columns = {r[1] for r in self._db.execute("PRAGMA table_info(sessions)")}
            if "komoot_prompt" not in columns:
                self._db.execute("ALTER TABLE sessions ADD COLUMN komoot_prompt TEXT")
                # 7: sessions that exist now are not asked about; the page asks for the new ones.
                # (Their "Komoot" button in the table stays.)
                self._db.execute("UPDATE sessions SET komoot_prompt = 'ignored'")
                self._db.commit()
        if 0 < version < 10:
            columns = {r[1] for r in self._db.execute("PRAGMA table_info(sessions)")}
            for column, kind in (("test_kind", "TEXT"), ("test_override", "TEXT"),
                                 ("idle", "INTEGER NOT NULL DEFAULT 1"), ("archived_at", "TEXT")):
                if column not in columns:
                    self._db.execute(f"ALTER TABLE sessions ADD COLUMN {column} {kind}")
            self._db.commit()
            # 6: clock steps inside a session are repaired (bikelog.timefix); 8: test
            # sessions, 9: idle sessions are recognised (bikelog.testride), 10: empty
            # logs (only zero bytes) count as idle -- every stored session gets its
            # index columns derived again.
            self.rederive_all()

    def close(self) -> None:
        with self._lock:
            self._db.close()

    # --- paths ---------------------------------------------------------

    def dir_for(self, session: Session) -> Path:
        return self.settings.sessions_dir / session.device / (session.day or "_")

    def path_for(self, session: Session, name: str) -> Path:
        return self.dir_for(session) / name

    # --- queries -------------------------------------------------------

    def _session(self, row) -> Session:
        data = dict(row)
        data["summary"] = json.loads(data["summary"]) if data["summary"] else None
        files = self._db.execute(
            "SELECT name, size, sha256, stored_at, source FROM files "
            "WHERE session_id = ? ORDER BY name", (data["id"],)).fetchall()
        return Session(**data, files=[StoredFile(**dict(f)) for f in files])

    def get(self, session_id: int, include_deleted: bool = False) -> Session | None:
        with self._lock:
            row = self._db.execute("SELECT * FROM sessions WHERE id = ?",
                                   (session_id,)).fetchone()
            if row is None or (row["deleted_at"] and not include_deleted):
                return None
            return self._session(row)

    def find(self, device: str, day: str, stem: str) -> Session | None:
        """Also returns tombstones -- callers decide what a deleted session means."""
        with self._lock:
            row = self._db.execute(
                "SELECT * FROM sessions WHERE device = ? AND day = ? AND stem = ?",
                (device, day, stem)).fetchone()
            return self._session(row) if row else None

    def list(self, limit: int = 100, offset: int = 0,
             min_distance_m: float | None = None,
             max_distance_m: float | None = None, tests: bool = True,
             idle: bool = True) -> list[Session]:
        """Sessions newest first, optionally restricted to a distance range
        (the wheel-sensor trip distance, same figure the session list shows).
        A session whose log could not be parsed (distance_m is NULL) matches
        neither bound, same as SQL's usual NULL handling. ``tests=False`` leaves
        out test sessions (simulator, GPS playback), ``idle=False`` the ones without a ride."""
        clause, params = _distance_clause(min_distance_m, max_distance_m, tests, idle)
        with self._lock:
            # Day directories sort chronologically, NO_TIME/legacy after them;
            # within a day the HHMMSS stem does the rest.
            rows = self._db.execute(
                "SELECT * FROM sessions WHERE deleted_at IS NULL " + clause +
                "ORDER BY (day GLOB '[0-9]*') DESC, day DESC, length(stem) DESC, stem DESC "
                "LIMIT ? OFFSET ?", (*params, limit, offset)).fetchall()
            return [self._session(row) for row in rows]

    def count(self, min_distance_m: float | None = None,
              max_distance_m: float | None = None, tests: bool = True, idle: bool = True) -> int:
        clause, params = _distance_clause(min_distance_m, max_distance_m, tests, idle)
        with self._lock:
            return self._db.execute(
                "SELECT COUNT(*) FROM sessions WHERE deleted_at IS NULL " + clause,
                params).fetchone()[0]

    def sessions_for_device(self, device: str) -> list[Session]:
        """Every non-deleted session of one device that has a binary log, in
        ride order -- the input to komoot.py's short-pause grouping. Test sessions
        and idle sessions are left out: a desk test or the device lying switched on
        between two rides must not join them."""
        with self._lock:
            rows = self._db.execute(
                "SELECT s.* FROM sessions s WHERE s.deleted_at IS NULL AND s.device = ? "
                f"AND NOT {TEST_SQL} AND s.idle = 0 "
                "AND EXISTS (SELECT 1 FROM files f WHERE f.session_id = s.id "
                "            AND substr(f.name, 1, 1) = 'L') "
                "ORDER BY s.day, s.stem", (device,)).fetchall()
            sessions = [self._session(row) for row in rows]
            sessions.sort(key=lambda s: s.first_time if s.first_time else 0)
            return sessions

    def known_files(self, device: str) -> dict[str, int | None]:
        """{"20260920/L_143012.bin": size} of everything held (or tombstoned) for a
        device -- what the puller compares the device's listing against. Files of
        deleted sessions report None: never fetch again, whatever the size."""
        with self._lock:
            rows = self._db.execute(
                "SELECT s.day, f.name, f.size, s.deleted_at FROM files f "
                "JOIN sessions s ON s.id = f.session_id WHERE s.device = ?",
                (device,)).fetchall()
        return {SdFile(r["day"], r["name"]).path: (None if r["deleted_at"] else r["size"])
                for r in rows}

    def records(self, session: Session, stats: ReadStats | None = None, types=(0,)):
        """Parsed records of the session's L_ file -- the input for every export."""
        log = session.file("L")
        if log is None:
            return
        # Clock steps inside the session (a wrong GPS time, see bikelog.timefix) are undone,
        # so every export sees one continuous ride.
        with open(self.path_for(session, log.name), "rb") as fh:
            yield from read_stream(fh, stats, types, repair_time=True,
                                   time_hints=self._hints(session))

    def _hints(self, session: Session) -> bytes | None:
        """The session's T_*.txt (clock steps), if it was fetched."""
        hint = session.file("T")
        if hint is None:
            return None
        try:
            return self.path_for(session, hint.name).read_bytes()
        except OSError:
            return None

    # --- mutations -----------------------------------------------------

    def put_file(self, device: str, sdfile: SdFile, payload: bytes,
                 source: str | None = None) -> PutResult:
        """Store one file of a session, creating the session on first sight.

        A tombstoned session is revived: an explicit upload means someone wants it.
        (The puller never gets here for those -- known_files() tells it to skip.)
        """
        if sdfile.day == sdlayout.WORKDIR:
            raise sdlayout.BadPath("files of the running session are not taken")
        digest = hashlib.sha256(payload).hexdigest()
        now = _now()
        with self._lock:
            session = self.find(device, sdfile.day, sdfile.stem)
            if session is None:
                cur = self._db.execute(
                    "INSERT INTO sessions (device, day, stem, created_at, updated_at) "
                    "VALUES (?, ?, ?, ?, ?)", (device, sdfile.day, sdfile.stem, now, now))
                session = self.get(cur.lastrowid)
            elif session.deleted_at:
                self._db.execute("UPDATE sessions SET deleted_at = NULL, archived_at = NULL WHERE id = ?",
                                 (session.id,))
                self._db.execute("DELETE FROM files WHERE session_id = ?", (session.id,))
                session = self.get(session.id)

            old = next((f for f in session.files if f.name == sdfile.name), None)
            path = self.path_for(session, sdfile.name)
            if old and old.sha256 == digest and path.exists():
                return PutResult(session, "unchanged")

            path.parent.mkdir(parents=True, exist_ok=True)
            # Temp name first: an interrupted write must not leave a half file
            # under a name the index claims is complete.
            tmp = path.with_name(path.name + ".part")
            tmp.write_bytes(payload)
            tmp.replace(path)

            self._db.execute(
                "INSERT OR REPLACE INTO files (session_id, name, size, sha256, stored_at, source) "
                "VALUES (?, ?, ?, ?, ?, ?)",
                (session.id, sdfile.name, len(payload), digest, now, source))
            updates = {"updated_at": now}
            if sdfile.kind in ("L", "I", "T"):
                updates["gpx_dirty"] = 1        # all three feed the GPX (track, time repair, summary)
            if sdfile.kind == "I":
                updates["summary"] = json.dumps(
                    sdlayout.parse_summary(payload.decode("utf-8", "replace")))
            self._db.execute(
                "UPDATE sessions SET %s WHERE id = ?" % ", ".join(f"{k} = ?" for k in updates),
                (*updates.values(), session.id))
            self._db.commit()
            if sdfile.kind in ("L", "I", "T"):
                self._rederive(session.id)
            return PutResult(self.get(session.id), "replaced" if old else "new")

    def _rederive(self, session_id: int) -> int:
        """Index columns (and the device summary's times) from the stored L_/T_/I_
        files. Run whenever one of them changed: the repair of clock steps needs both
        the log and the hints, in whichever order they arrived."""
        session = self.get(session_id, include_deleted=True)
        log = session.file("L") if session else None
        if log is None:
            return 0
        try:
            payload = self.path_for(session, log.name).read_bytes()
        except OSError:
            return 0
        info = _summarise_log(payload, self._hints(session))
        steps = info.pop("time_steps")
        summary = session.summary
        if summary is not None:
            # The device wrote start/end/duration before the repair; show the repaired ones.
            summary = {k: v for k, v in summary.items() if k not in ("time_steps", "time_repaired")}
            if steps and info["first_time"]:
                summary.update(start=info["first_time"], end=info["last_time"],
                               dur_s=info["last_time"] - info["first_time"],
                               time="repaired", time_steps=steps)
            info["summary"] = json.dumps(summary)
        if steps:
            # The times in the GPX change: export again, and let Nextcloud take the new file.
            info["gpx_dirty"] = 1
            info["nextcloud_gpx_version"] = None
        self._db.execute(
            "UPDATE sessions SET %s WHERE id = ?" % ", ".join(f"{k} = ?" for k in info),
            (*info.values(), session_id))
        self._db.commit()
        return steps

    def rederive_all(self) -> int:
        """Run _rederive() over every session with a log (after an upgrade of the repair).
        Returns how many had clock steps repaired -- only those are exported again; the
        others' GPX stays as it is (re-exporting everything would also re-judge old
        sessions by today's sanitizing, which not every one survives)."""
        with self._lock:
            ids = [r[0] for r in self._db.execute(
                "SELECT s.id FROM sessions s WHERE EXISTS (SELECT 1 FROM files f "
                "WHERE f.session_id = s.id AND substr(f.name, 1, 1) = 'L')").fetchall()]
            return sum(1 for sid in ids if self._rederive(sid))

    def pending_exports(self, version: int) -> list[Session]:
        """Sessions whose GPX is missing, outdated (L_/I_ changed) or was made
        by an older exporter -- so exporter improvements reach old rides too."""
        with self._lock:
            rows = self._db.execute(
                "SELECT s.* FROM sessions s WHERE s.deleted_at IS NULL "
                "AND (s.gpx_dirty = 1 OR s.gpx_version IS NOT ?) "
                "AND EXISTS (SELECT 1 FROM files f WHERE f.session_id = s.id "
                "            AND substr(f.name, 1, 1) = 'L')", (version,)).fetchall()
            return [self._session(row) for row in rows]

    def set_export(self, session_id: int, gpx_file: str | None, status: str,
                   version: int) -> None:
        with self._lock:
            self._db.execute(
                "UPDATE sessions SET gpx_file = ?, gpx_status = ?, gpx_version = ?, "
                "gpx_dirty = 0 WHERE id = ?", (gpx_file, status, version, session_id))
            self._db.commit()

    def set_komoot(self, session_ids: list[int], status: str, tour_id: str | None) -> None:
        """Marks every session of a merged tour with the same upload result,
        so looking any one of them up shows it -- see komoot.py."""
        with self._lock:
            self._db.executemany(
                "UPDATE sessions SET komoot_status = ?, komoot_tour_id = ?, komoot_uploaded_at = ? "
                "WHERE id = ?", [(status, tour_id, _now(), sid) for sid in session_ids])
            self._db.commit()

    def set_komoot_prompt(self, session_ids: list[int], value: str | None) -> None:
        """'ignored' = never ask again for these sessions; None = ask again."""
        with self._lock:
            self._db.executemany("UPDATE sessions SET komoot_prompt = ? WHERE id = ?",
                                 [(value, sid) for sid in session_ids])
            self._db.commit()

    def idle_to_archive(self, older_than: str) -> list[Session]:
        """Live idle sessions stored before ``older_than`` (ISO time) -- archive.py's work."""
        with self._lock:
            rows = self._db.execute(
                "SELECT * FROM sessions WHERE deleted_at IS NULL AND idle = 1 AND created_at < ?",
                (older_than,)).fetchall()
            return [self._session(row) for row in rows]

    def idle_sessions(self) -> list[Session]:
        """Live idle sessions, newest first -- not archived yet."""
        with self._lock:
            rows = self._db.execute(
                "SELECT * FROM sessions WHERE deleted_at IS NULL AND idle = 1 "
                "ORDER BY created_at DESC").fetchall()
            return [self._session(row) for row in rows]

    def archived_sessions(self) -> list[Session]:
        """Archived sessions whose files are still on the SD card (their tombstones), newest first."""
        with self._lock:
            rows = self._db.execute(
                "SELECT * FROM sessions WHERE archived_at IS NOT NULL ORDER BY day DESC, stem DESC"
            ).fetchall()
            return [self._session(row) for row in rows]

    def purge_gone(self, device: str, listed: set[str]) -> int:
        """Drop the index rows of deleted/archived sessions none of whose files is in the
        device's listing any more: the tombstone was only there to stop the puller from
        fetching them again. Returns how many."""
        with self._lock:
            rows = self._db.execute(
                "SELECT s.id, s.day, f.name FROM sessions s JOIN files f ON f.session_id = s.id "
                "WHERE s.device = ? AND s.deleted_at IS NOT NULL", (device,)).fetchall()
            on_card: dict[int, bool] = {}
            for r in rows:
                on_card[r["id"]] = on_card.get(r["id"], False) or SdFile(r["day"], r["name"]).path in listed
            gone = [sid for sid, there in on_card.items() if not there]
            for sid in gone:
                self._db.execute("DELETE FROM sessions WHERE id = ?", (sid,))
            self._db.commit()
            return len(gone)

    def remove_index(self, session_id: int) -> None:
        """Forget a session completely (its files are neither here nor on the SD card)."""
        with self._lock:
            self._db.execute("DELETE FROM sessions WHERE id = ?", (session_id,))
            self._db.commit()

    def mark_archived(self, session_id: int) -> None:
        """Out of every list, a tombstone for the puller (known_files: never fetch again);
        the files were moved to the archive by the caller."""
        now = _now()
        with self._lock:
            self._db.execute("UPDATE sessions SET deleted_at = ?, archived_at = ?, gpx_file = NULL "
                             "WHERE id = ?", (now, now, session_id))
            self._db.execute("DELETE FROM reports WHERE session_id = ?", (session_id,))
            self._db.commit()

    def set_bike(self, session_id: int, bike_id: str | None) -> None:
        with self._lock:
            self._db.execute("UPDATE sessions SET bike_id = ? WHERE id = ?", (bike_id, session_id))
            self._db.commit()

    def set_test_override(self, session_id: int, value: str | None) -> None:
        """The rider's verdict: "test", "real", or None (back to what the log says).
        The GPX is exported again -- a test belongs in Debug_Archive, not in Tours."""
        if value not in ("test", "real", None):
            raise ValueError(value)
        with self._lock:
            self._db.execute("UPDATE sessions SET test_override = ?, gpx_dirty = 1 WHERE id = ?",
                             (value, session_id))
            self._db.commit()

    def devices(self) -> list[str]:
        with self._lock:
            return [r[0] for r in self._db.execute(
                "SELECT DISTINCT device FROM sessions WHERE deleted_at IS NULL ORDER BY device")]

    def pending_nextcloud(self) -> list[Session]:
        """Tours-status sessions whose Nextcloud copy is missing or stale
        (gpx_version moved on since the last sync, or the last sync failed),
        plus previously-synced sessions whose export is no longer "ok" (moved
        to Debug_Archive, or now unreadable) -- their remote copy is stale
        and needs removing, not updating. See nextcloud.py."""
        with self._lock:
            rows = self._db.execute(
                "SELECT * FROM sessions WHERE deleted_at IS NULL AND ("
                "  (gpx_status = 'ok' AND (nextcloud_gpx_version IS NOT gpx_version "
                "                          OR nextcloud_status = 'error'))"
                "  OR (gpx_status IS NOT 'ok' AND nextcloud_file IS NOT NULL)"
                ")").fetchall()
            return [self._session(row) for row in rows]

    def set_nextcloud(self, session_id: int, status: str | None, file: str | None,
                      gpx_version: int | None) -> None:
        with self._lock:
            self._db.execute(
                "UPDATE sessions SET nextcloud_status = ?, nextcloud_file = ?, "
                "nextcloud_synced_at = ?, nextcloud_gpx_version = ? WHERE id = ?",
                (status, file, _now(), gpx_version, session_id))
            self._db.commit()

    def sessions_with_log(self) -> list[Session]:
        """All live sessions that have a binary log and are not idle, oldest first."""
        with self._lock:
            rows = self._db.execute(
                "SELECT s.* FROM sessions s WHERE s.deleted_at IS NULL AND s.idle = 0 "
                "AND EXISTS (SELECT 1 FROM files f WHERE f.session_id = s.id "
                "            AND substr(f.name, 1, 1) = 'L') "
                "ORDER BY s.first_time, s.id").fetchall()
            return [self._session(row) for row in rows]

    def cached_report(self, session_id: int) -> tuple[str, dict] | None:
        with self._lock:
            row = self._db.execute("SELECT cache_key, report FROM reports WHERE session_id = ?",
                                   (session_id,)).fetchone()
        return (row["cache_key"], json.loads(row["report"])) if row else None

    def cached_tour_report(self, cache_key: str) -> dict | None:
        with self._lock:
            row = self._db.execute("SELECT report FROM tour_reports WHERE cache_key = ?",
                                   (cache_key,)).fetchone()
        return json.loads(row["report"]) if row else None

    def store_tour_report(self, cache_key: str, report: dict) -> None:
        with self._lock:
            self._db.execute("INSERT OR REPLACE INTO tour_reports (cache_key, report, computed_at) "
                             "VALUES (?, ?, ?)", (cache_key, json.dumps(report, ensure_ascii=False), _now()))
            self._db.commit()

    def narrative(self, subject: str) -> dict | None:
        with self._lock:
            row = self._db.execute("SELECT * FROM narratives WHERE subject = ?", (subject,)).fetchone()
        if row is None:
            return None
        data = dict(row)
        data["unverified"] = json.loads(data["unverified"])
        return data

    def store_narrative(self, subject: str, prompt_key: str, model: str, text: str,
                        unverified: list[str], seconds: float | None) -> None:
        with self._lock:
            self._db.execute(
                "INSERT OR REPLACE INTO narratives (subject, prompt_key, model, text, unverified, "
                "seconds, created_at) VALUES (?, ?, ?, ?, ?, ?, ?)",
                (subject, prompt_key, model, text, json.dumps(unverified, ensure_ascii=False),
                 seconds, _now()))
            self._db.commit()

    def delete_narrative(self, subject: str) -> None:
        with self._lock:
            self._db.execute("DELETE FROM narratives WHERE subject = ?", (subject,))
            self._db.commit()

    def cache_keys(self) -> dict[int, str]:
        with self._lock:
            return {r["session_id"]: r["cache_key"] for r in
                    self._db.execute("SELECT session_id, cache_key FROM reports").fetchall()}

    def store_report(self, session_id: int, cache_key: str, report: dict) -> None:
        with self._lock:
            self._db.execute(
                "INSERT INTO reports (session_id, cache_key, report, computed_at) VALUES (?, ?, ?, ?) "
                "ON CONFLICT (session_id) DO UPDATE SET cache_key = excluded.cache_key, "
                "report = excluded.report, computed_at = excluded.computed_at",
                (session_id, cache_key, json.dumps(report, ensure_ascii=False), _now()))
            self._db.commit()

    def delete(self, session_id: int) -> bool:
        with self._lock:
            session = self.get(session_id)
            if session is None:
                return False
            for f in session.files:
                self.path_for(session, f.name).unlink(missing_ok=True)
            if session.gpx_file:
                (self.settings.gpx_dir / session.gpx_file).unlink(missing_ok=True)
            self._db.execute("UPDATE sessions SET deleted_at = ?, gpx_file = NULL WHERE id = ?",
                             (_now(), session_id))
            self._db.execute("DELETE FROM reports WHERE session_id = ?", (session_id,))
            self._db.commit()
            return True


def _summarise_log(payload: bytes, hints: bytes | None = None) -> dict:
    """Parse an L_ file once, when it is stored, to fill the index. A file that
    does not parse is kept anyway (it is the device's data, not ours to drop) and
    the reason recorded."""
    blank = dict(format_version=None, record_count=None, trailing_bytes=None,
                 first_time=None, last_time=None, distance_m=None, gps_points=None,
                 clock_unset=0, time_steps=0, test_kind=None, idle=1)
    if not payload.strip(b"\0"):
        # Nothing but zero bytes (preallocated, or the device died before the first
        # write): no ride, no error worth anyone's attention -- idle, archived later.
        return {**blank, "log_error": "empty log (%d zero byte)" % len(payload)}
    stats = ReadStats()
    try:
        records = list(read_stream(io.BytesIO(payload), stats, repair_time=True, time_hints=hints))
    except UnknownLogFormat as exc:
        # visible, not idle: someone has to look at it
        return {**blank, "idle": 0, "log_error": str(exc)}
    if not records:
        return {**blank, "format_version": stats.version or None,
                "log_error": "no complete data record (%d byte)" % len(payload)}
    first, last = records[0], records[-1]
    return {
        "format_version": stats.version,
        "record_count": len(records),
        "trailing_bytes": stats.trailing_bytes,
        "first_time": first.timestamp,
        "last_time": last.timestamp,
        "distance_m": last.distance - first.distance,
        "gps_points": sum(1 for rec in records if rec.gps_valid),
        "clock_unset": int(first.timestamp < MIN_PLAUSIBLE_TIMESTAMP),
        "log_error": None,
        "time_steps": stats.time_steps,
        "test_kind": testride.classify(records).kind,
        "idle": int(testride.idle(records)),
    }
