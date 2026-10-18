"""Collect sessions a bike computer pushed to a Nextcloud folder (the push inbox).

The counterpart of the firmware's log push (src/LogPush.cpp): a bike computer that is not
parked in reach of the home WiFi uploads its finished session files by WebDAV to

    <BIKELOG_NEXTCLOUD_INBOX>/<host>/<day>/<name>      e.g. BikeLog-Inbox/TRGB-FL/20261018/L_143012.bin

-- the same relative paths the pull fetches from the device. A Nextcloud upload link ("files
drop") keeps only the file name, so the firmware uploads there flat:

    <inbox>/TRGB-FL~20261018~L_143012.bin                     (host~day~name, day empty: ~~)
    <inbox>/TRGB-FL/TRGB-FL~20261018~L_143012.bin             ("file request": a folder per nickname)

A name that is already there gets " (2)" before the extension instead of being replaced; such
copies are taken oldest first, so the newest version ends up stored. Every ``inbox_interval_s`` this
module lists the inbox, files each file through the same Storage.put_file() as the pull, and
deletes it from the Nextcloud once it is stored. So a pushed file and a pulled one end up
identical (same bytes = "unchanged"), and the inbox only ever holds what has not arrived yet.

Like the pull, it does not bring back a session deleted (or archived) here: such files are
dropped from the inbox unread. ``<host>`` is the device's network name; BIKELOG_PULL_TARGETS
maps it to the device name the sessions are filed under (``pendler=TRGB-FL``), a host without
a target is filed under its own name.

Uses the Nextcloud login of the GPX sync (nextcloud.py). requests is only imported when the
inbox is actually read, like there.
"""

from __future__ import annotations

import datetime
import email.utils
import logging
import re
import threading
import urllib.parse
import xml.etree.ElementTree as ET
from dataclasses import asdict, dataclass, field

from . import analysis, exporter, nextcloud, sdlayout
from .puller import Target

log = logging.getLogger("bikelog.inbox")

_DAV = "{DAV:}"
_PROPFIND = (b'<?xml version="1.0"?><d:propfind xmlns:d="DAV:"><d:prop>'
             b'<d:resourcetype/><d:getcontentlength/><d:getlastmodified/></d:prop></d:propfind>')
_DEVICE = re.compile(r"^[A-Za-z0-9_-]{1,32}$")
#: Flat name of a files-drop upload (LogPushCore.h dropName()), with Nextcloud's " (2)" for a
#: name that was taken.
_FLAT = re.compile(r"^(?P<host>[A-Za-z0-9_-]{1,32})~(?P<day>[A-Za-z0-9_]{0,16})~"
                   r"(?P<stem>[A-Za-z][A-Za-z0-9_]{0,31})(?: \(\d+\))?(?P<ext>\.[A-Za-z0-9]{1,4})$")
#: The firmware's "Test target" on an upload link.
_PROBE = re.compile(r"^[A-Za-z0-9_-]{1,32}~push-test(?: \(\d+\))?\.txt$")
#: An emptied day folder is removed only when nothing has been written to it for this long --
#: the bike computer may be uploading into it right now.
FOLDER_IDLE_S = 3600


@dataclass(frozen=True)
class Entry:
    name: str
    is_dir: bool
    size: int | None
    modified: datetime.datetime | None


@dataclass
class InboxResult:
    files: int = 0
    stored: int = 0          # new or replaced
    unchanged: int = 0
    dropped: int = 0         # deleted here, or not a log file
    failed: int = 0
    folders_removed: int = 0
    errors: list[str] = field(default_factory=list)

    def summary(self) -> str:
        return (f"{self.files} file(s): {self.stored} stored, {self.unchanged} unchanged, "
                f"{self.dropped} dropped, {self.failed} failed")


def parse_multistatus(body: bytes) -> list[Entry]:
    """PROPFIND Depth 1 answer -> entries below the folder (the folder itself is left out:
    it is the first response)."""
    root = ET.fromstring(body)
    out = []
    for i, resp in enumerate(root.iter(_DAV + "response")):
        href = resp.findtext(_DAV + "href") or ""
        if i == 0:
            continue
        name = urllib.parse.unquote(href.rstrip("/").rsplit("/", 1)[-1])
        is_dir = resp.find(f".//{_DAV}resourcetype/{_DAV}collection") is not None
        length = resp.findtext(f".//{_DAV}getcontentlength")
        modified = resp.findtext(f".//{_DAV}getlastmodified")
        when = None
        if modified:
            try:
                when = email.utils.parsedate_to_datetime(modified)
            except (TypeError, ValueError):
                pass
        out.append(Entry(name, is_dir, int(length) if length and length.isdigit() else None, when))
    return out


class Inbox:
    """WebDAV access to the inbox folder. ``http`` is a requests.Session (or a fake in tests)."""

    def __init__(self, settings, http):
        self.settings = settings
        self.http = http
        self.root = settings.nextcloud_inbox.strip("/")

    def url(self, *parts: str) -> str:
        path = "/".join([self.root, *(urllib.parse.quote(p) for p in parts)])
        return nextcloud._dav_url(self.settings, path)

    def list(self, *parts: str) -> list[Entry]:
        resp = self.http.request("PROPFIND", self.url(*parts) + "/", data=_PROPFIND,
                                 headers={"Depth": "1", "Content-Type": "application/xml"})
        if resp.status_code == 404:
            return []
        resp.raise_for_status()
        return parse_multistatus(resp.content)

    def get(self, *parts: str) -> bytes:
        resp = self.http.get(self.url(*parts))
        resp.raise_for_status()
        return resp.content

    def delete(self, *parts: str) -> None:
        resp = self.http.delete(self.url(*parts))
        if resp.status_code not in (200, 204, 404):
            resp.raise_for_status()


def device_for(settings, host: str) -> str | None:
    """The device a host folder is filed under: the pull target with this host name, else the
    name itself (if it is a valid device name)."""
    for spec in settings.pull_targets:
        target = Target.parse(spec)
        if target.host.lower() == host.lower():
            return target.device
    return host if _DEVICE.match(host) else None


def _old_enough(entry: Entry, now: datetime.datetime) -> bool:
    return entry.modified is None or (now - entry.modified).total_seconds() > FOLDER_IDLE_S


def _by_age(entries: list[Entry]) -> list[Entry]:
    oldest = datetime.datetime.min.replace(tzinfo=datetime.timezone.utc)
    return sorted(entries, key=lambda e: (e.modified or oldest, e.name))


def _take_flat(store, inbox: Inbox, folder: tuple[str, ...], entries: list[Entry],
               result: InboxResult) -> list[Entry]:
    """Files-drop uploads among `entries` (files in `folder`); returns the other entries."""
    rest = []
    known_by_device: dict[str, dict] = {}
    for entry in _by_age(entries):
        if entry.is_dir:
            rest.append(entry)
            continue
        if _PROBE.match(entry.name):
            log.info("inbox: test upload %s - deleted", entry.name)
            _delete(inbox, (*folder, entry.name), result)
            continue
        m = _FLAT.match(entry.name)
        if not m:
            rest.append(entry)
            continue
        device = device_for(store.settings, m["host"])
        if device not in known_by_device:
            known_by_device[device] = store.known_files(device)
        _take(store, inbox, device, (*folder, entry.name), m["day"], m["stem"] + m["ext"], entry,
              known_by_device[device], result)
    return rest


def collect(store, inbox: Inbox, now: datetime.datetime | None = None) -> InboxResult:
    """One pass over the inbox: store every file, delete what is stored (or not wanted)."""
    now = now or datetime.datetime.now(datetime.timezone.utc)
    result = InboxResult()
    for host in _take_flat(store, inbox, (), inbox.list(), result):
        if not host.is_dir:
            log.warning("inbox: %s is no upload of a bike computer - left alone", host.name)
            continue
        device = device_for(store.settings, host.name)
        if device is None:
            log.warning("inbox: folder %r is not a device name - left alone", host.name)
            continue
        known = store.known_files(device)
        for entry in _take_flat(store, inbox, (host.name,), inbox.list(host.name), result):
            if entry.is_dir:
                files = [e for e in inbox.list(host.name, entry.name) if not e.is_dir]
                for f in files:
                    _take(store, inbox, device, (host.name, entry.name, f.name), entry.name, f.name, f,
                          known, result)
                # Empty now and quiet for a while: remove the day folder (checked again right
                # before, the device may have added something in the meantime).
                if _old_enough(entry, now) and entry.name != sdlayout.WORKDIR:
                    try:
                        if not inbox.list(host.name, entry.name):
                            inbox.delete(host.name, entry.name)
                            result.folders_removed += 1
                    except Exception as exc:
                        log.warning("inbox: %s/%s not removed: %s", host.name, entry.name, exc)
            else:
                _take(store, inbox, device, (host.name, entry.name), "", entry.name, entry, known, result)
    return result


def _take(store, inbox: Inbox, device: str, parts: tuple[str, ...], day: str, name: str,
          entry: Entry, known: dict, result: InboxResult) -> None:
    """Store the inbox file at `parts` as <day>/<name> of `device`, then delete it there."""
    result.files += 1
    label = "/".join(parts)
    try:
        sdfile = sdlayout.parse(day, name)
        if sdfile.day == sdlayout.WORKDIR:
            raise sdlayout.BadPath("running session")
    except sdlayout.BadPath as exc:
        log.warning("inbox: %s is not a log file (%s) - deleted", label, exc)
        _delete(inbox, parts, result)
        result.dropped += 1
        return
    if sdfile.path in known and known[sdfile.path] is None:
        log.info("inbox: %s belongs to a session deleted here - dropped", label)
        _delete(inbox, parts, result)
        result.dropped += 1
        return
    try:
        payload = inbox.get(*parts)
    except Exception as exc:
        result.failed += 1
        result.errors.append(f"{label}: {exc}")
        log.warning("inbox: %s not read: %s", label, exc)
        return
    if entry.size is not None and len(payload) != entry.size:
        result.failed += 1          # changed while we read it: next pass
        result.errors.append(f"{label}: size {len(payload)} != listed {entry.size}")
        return
    if not payload:
        _delete(inbox, parts, result)
        result.dropped += 1
        return
    put = store.put_file(device, sdfile, payload, source="inbox:" + label)
    if put.status == "unchanged":
        result.unchanged += 1
    else:
        result.stored += 1
        log.info("inbox: %s %s (%d byte) -> %s", put.status, label, len(payload), device)
    _delete(inbox, parts, result)


def _delete(inbox: Inbox, parts, result: InboxResult) -> None:
    try:
        inbox.delete(*parts)
    except Exception as exc:
        # Stored anyway; the next pass finds it "unchanged" and tries again.
        result.errors.append(f"{'/'.join(parts)}: not deleted: {exc}")
        log.warning("inbox: %s not deleted: %s", "/".join(parts), exc)


#: The poller thread and POST /api/v1/inbox may both call in.
_lock = threading.Lock()


def run_once(store, http=None) -> InboxResult | None:
    """Collect once, then export/sync/analyse like after a pull. None if the inbox is off."""
    settings = store.settings
    if not settings.inbox_enabled:
        return None
    with _lock:
        if http is None:
            import requests

            with requests.Session() as session:
                session.auth = (settings.nextcloud_user, settings.nextcloud_password)
                result = collect(store, Inbox(settings, session))
        else:
            result = collect(store, Inbox(settings, http))
    if result.files:
        log.info("inbox %s: %s", settings.nextcloud_inbox, result.summary())
    if result.stored:
        exporter.export_pending(store)
        nextcloud.sync_pending(store)
        analysis.refresh(store)
    return result


class InboxPoller:
    """Background thread: run_once() every inbox_interval_s, or right away on trigger()."""

    def __init__(self, store):
        self.store = store
        self._wake = threading.Event()
        self._stop = threading.Event()
        self._thread: threading.Thread | None = None
        self.last_run: str | None = None
        self.last_result: dict | None = None
        self.last_error: str | None = None

    def start(self) -> None:
        self._thread = threading.Thread(target=self._run, name="bikelog-inbox", daemon=True)
        self._thread.start()
        log.info("collecting pushed sessions from Nextcloud folder %s every %.0f s",
                 self.store.settings.nextcloud_inbox, self.store.settings.inbox_interval_s)

    def stop(self) -> None:
        self._stop.set()
        self._wake.set()
        if self._thread:
            self._thread.join(timeout=5)

    def trigger(self) -> None:
        self._wake.set()

    def _run(self) -> None:
        while not self._stop.is_set():
            try:
                result = run_once(self.store)
                self.last_result = asdict(result) if result else None
                self.last_error = ("; ".join(result.errors[:3]) or None) if result else None
            except Exception as exc:          # network down, wrong password, ...
                self.last_error = str(exc)
                log.warning("inbox: %s", exc)
            self.last_run = datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")
            self._wake.wait(self.store.settings.inbox_interval_s)
            self._wake.clear()

    def as_dict(self) -> dict:
        return {"enabled": True, "folder": self.store.settings.nextcloud_inbox,
                "interval_s": self.store.settings.inbox_interval_s, "last_run": self.last_run,
                "last_result": self.last_result, "last_error": self.last_error}
