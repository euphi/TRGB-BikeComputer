"""Pull the logs from the bike computer as soon as it shows up on the network.

The bike computer already serves its SD card (``/log/<day>/<file>``) and a list
of it, so the service fetches instead of the firmware pushing: no upload client
on the device, no credentials there, no retry logic in a task that competes with
BLE and the display for internal heap.

When to pull:

* **mDNS**: the firmware calls ``MDNS.begin("TRGB-BC")`` right after joining the
  WiFi, which announces ``TRGB-BC._http._tcp.local``. A ServiceBrowser sees that
  within a second or two and triggers a pull.
* **Polling**, because an announcement is easy to miss: the browser only reports
  a service it does not already have cached, and a device that is switched off
  sends no goodbye, so after a quick round trip the entry may still be cached.
  Every ``pull_interval_s`` the service therefore asks for the host's address
  (one multicast query, no traffic to the device if it is away). If it answers
  and was absent before -- or the last pull is older than RESYNC_S -- it pulls.

What to pull: every file in the listing that is not held yet (or whose size
changed), except the running session's working directory and sessions that were
deleted here. New sessions only appear on the device at boot (LogSessions moves
them out of CUR/), so a pull that finds nothing costs one listing request.
"""

from __future__ import annotations

import datetime
import json
import logging
import re
import socket
import threading
import time
import urllib.error
import urllib.request
from dataclasses import asdict, dataclass, field
from typing import Callable

from . import exporter, sdlayout
from .sdlayout import SdFile

log = logging.getLogger("bikelog.pull")

#: Pull again after this long even without a new appearance -- covers a reboot
#: fast enough to fall between two polls without the announcement being seen.
RESYNC_S = 1800
#: mDNS announcements come in bursts (the firmware's plus retransmissions).
TRIGGER_DEBOUNCE_S = 15
#: Pause between file downloads: every request costs the ESP32 several KB of
#: internal heap, and its web server sheds load (503) when that runs low.
DOWNLOAD_PAUSE_S = 0.2


class HttpError(Exception):
    def __init__(self, url: str, status: int | None, reason: str):
        super().__init__(f"{url}: {status or ''} {reason}".strip())
        self.status = status


class Http:
    """Minimal GET with retries -- urllib, so the service needs no HTTP client dep."""

    def __init__(self, timeout: float = 30.0, retries: int = 3, backoff_s: float = 2.0):
        self.timeout = timeout
        self.retries = retries
        self.backoff_s = backoff_s

    def get(self, url: str) -> bytes:
        attempt = 0
        while True:
            try:
                with urllib.request.urlopen(url, timeout=self.timeout) as resp:
                    body = resp.read()
                    expected = resp.headers.get("Content-Length")
                    if expected is not None and int(expected) != len(body):
                        raise HttpError(url, None, f"truncated ({len(body)}/{expected} byte)")
                    return body
            except urllib.error.HTTPError as exc:
                # 503 = the device's low-heap load shedding: worth another try.
                if exc.code != 503 or attempt >= self.retries:
                    raise HttpError(url, exc.code, exc.reason) from exc
            except (urllib.error.URLError, OSError, HttpError) as exc:
                if attempt >= self.retries:
                    if isinstance(exc, HttpError):
                        raise
                    raise HttpError(url, None, str(getattr(exc, "reason", exc))) from exc
            attempt += 1
            time.sleep(self.backoff_s * attempt)


# --- listing ------------------------------------------------------------

@dataclass(frozen=True)
class Listed:
    file: SdFile
    size: int | None        # None: the listing did not say (HTML fallback)


_DEL_ARRAY = re.compile(r"del\(this,\[([^\]]*)\]\)")
_QUOTED = re.compile(r"'([^']+)'")
_HREF = re.compile(r'href="/log/([^"]+)"')


def parse_json_listing(body: bytes) -> list[Listed]:
    """``/logfiles.json``: {"files": [{"path": "20260920/L_143012.bin", "size": 1234}]}"""
    data = json.loads(body)
    return _valid((entry["path"], entry.get("size")) for entry in data.get("files", []))


def parse_html_listing(body: bytes) -> list[Listed]:
    """``/logfiles/`` as rendered by BCLogger::getAllFileLinks(). The badges link
    only the downloadable types; the row's DEL button carries every file of the
    session (I_ and T_ included), so both are collected. Sizes are shown rounded
    there and are not used."""
    text = body.decode("utf-8", "replace")
    paths = set(_HREF.findall(text))
    for array in _DEL_ARRAY.findall(text):
        paths.update(_QUOTED.findall(array))
    return _valid((path, None) for path in sorted(paths))


def _valid(entries) -> list[Listed]:
    out = []
    for path, size in entries:
        try:
            out.append(Listed(sdlayout.parse_path(path), size))
        except sdlayout.BadPath:
            log.debug("ignoring listed path %r", path)
    return out


def fetch_listing(http: Http, base_url: str) -> list[Listed]:
    try:
        return parse_json_listing(http.get(base_url + "/logfiles.json"))
    except HttpError as exc:
        if exc.status != 404:
            raise
    # Firmware without the JSON endpoint: read the HTML page.
    return parse_html_listing(http.get(base_url + "/logfiles/"))


# --- one pull -----------------------------------------------------------

@dataclass
class SyncResult:
    listed: int = 0
    fetched: int = 0
    replaced: int = 0
    skipped: int = 0
    failed: int = 0
    bytes: int = 0
    errors: list[str] = field(default_factory=list)

    def summary(self) -> str:
        return (f"{self.listed} listed, {self.fetched} fetched ({self.bytes} byte), "
                f"{self.replaced} replaced, {self.failed} failed")


def sync(storage, device: str, base_url: str, http: Http | None = None,
         pause_s: float = DOWNLOAD_PAUSE_S) -> SyncResult:
    """Fetch everything from one device that is not stored yet."""
    http = http or Http()
    result = SyncResult()
    listing = fetch_listing(http, base_url)
    known = storage.known_files(device)
    for entry in listing:
        sdfile = entry.file
        if sdfile.day == sdlayout.WORKDIR:
            continue
        result.listed += 1
        if sdfile.path in known:
            have = known[sdfile.path]
            if have is None or entry.size is None or have == entry.size:
                result.skipped += 1
                continue
        try:
            payload = http.get(f"{base_url}/log/{sdfile.path}")
        except HttpError as exc:
            result.failed += 1
            result.errors.append(str(exc))
            log.warning("%s: %s", device, exc)
            continue
        if entry.size is not None and len(payload) != entry.size:
            # Listed size is from the same moment as the file -- a mismatch means
            # the file changed in between; the next pull takes it.
            result.failed += 1
            result.errors.append(f"{sdfile.path}: size {len(payload)} != listed {entry.size}")
            continue
        if not payload:
            result.skipped += 1
            continue
        put = storage.put_file(device, sdfile, payload, source=base_url)
        result.bytes += len(payload)
        if put.status == "replaced":
            result.replaced += 1
        elif put.status == "new":
            result.fetched += 1
        if pause_s:
            time.sleep(pause_s)
    return result


# --- when to pull ---------------------------------------------------------

@dataclass
class Target:
    device: str
    host: str               # mDNS host name without ".local"

    @classmethod
    def parse(cls, spec: str) -> "Target":
        device, sep, host = spec.partition("=")
        return cls(device.strip(), host.strip()) if sep else cls(spec.strip(), spec.strip())


@dataclass
class TargetStatus:
    device: str
    host: str
    online: bool = False
    address: str | None = None
    last_seen: str | None = None
    last_sync: str | None = None
    last_sync_ok: str | None = None
    last_result: dict | None = None
    last_error: str | None = None
    syncing: bool = False


def _now_iso() -> str:
    return datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds")


Resolver = Callable[[str], "str | None"]


class Puller:
    """Background worker: one thread doing the pulls, zeroconf threads feeding it."""

    def __init__(self, storage, settings, http: Http | None = None,
                 resolver: Resolver | None = None):
        self.storage = storage
        self.settings = settings
        self.http = http or Http()
        self.targets = [Target.parse(spec) for spec in settings.pull_targets]
        self.status = {t.device: TargetStatus(t.device, t.host) for t in self.targets}
        self._resolver = resolver
        self._zc = None
        self._browser = None
        self._wake = threading.Event()
        self._stop = threading.Event()
        self._forced: set[str] = set()
        self._lock = threading.Lock()
        self._thread: threading.Thread | None = None
        self._mono_sync_ok: dict[str, float] = {}
        self._mono_attempt: dict[str, float] = {}

    # --- lifecycle ---

    def start(self) -> None:
        if self.settings.pull_mdns or self._resolver is None:
            self._start_zeroconf()
        self._thread = threading.Thread(target=self._run, name="bikelog-pull", daemon=True)
        self._thread.start()
        log.info("pulling from %s every %.0f s%s",
                 ", ".join(f"{t.device}={t.host}.local" for t in self.targets),
                 self.settings.pull_interval_s,
                 " and on mDNS announcements" if self._browser else "")

    def stop(self) -> None:
        self._stop.set()
        self._wake.set()
        if self._thread:
            self._thread.join(timeout=5)
        if self._zc:
            self._zc.close()

    def _start_zeroconf(self) -> None:
        try:
            from zeroconf import ServiceBrowser, Zeroconf
        except ImportError:
            log.warning("zeroconf not installed -- falling back to the system resolver, "
                        "no mDNS trigger")
            return
        self._zc = Zeroconf()
        if self.settings.pull_mdns:
            self._browser = ServiceBrowser(self._zc, "_http._tcp.local.", handlers=[self._on_service])

    def _on_service(self, zeroconf, service_type, name, state_change) -> None:
        # Runs on zeroconf's event loop: only note it and hand over, never block here.
        instance = name.split("." + service_type, 1)[0] if service_type in name else name
        for target in self.targets:
            if instance.lower() == target.host.lower() and state_change.name in ("Added", "Updated"):
                log.info("%s announced via mDNS (%s)", target.device, state_change.name)
                self.trigger(target.device)

    # --- triggers ---

    def trigger(self, device: str | None = None) -> None:
        """Check (and pull from) one device, or all, right away."""
        with self._lock:
            self._forced.update([device] if device else [t.device for t in self.targets])
        self._wake.set()

    def _run(self) -> None:
        next_poll = 0.0
        while not self._stop.is_set():
            self._wake.wait(max(0.0, next_poll - time.monotonic()))
            self._wake.clear()
            if self._stop.is_set():
                return
            with self._lock:
                forced, self._forced = self._forced, set()
            polling = time.monotonic() >= next_poll
            if polling:
                next_poll = time.monotonic() + self.settings.pull_interval_s
            for target in self.targets:
                if polling or target.device in forced:
                    try:
                        self.check(target, forced=target.device in forced)
                    except Exception:           # never let one bad pull kill the thread
                        log.exception("%s: check failed", target.device)

    # --- one check ---

    def resolve(self, host: str) -> str | None:
        if self._resolver:
            return self._resolver(host)
        if self._zc is not None:
            from zeroconf import AddressResolver
            resolver = AddressResolver(host + ".local.")
            if resolver.request(self._zc, 3000):
                addresses = resolver.parsed_addresses()
                ipv4 = [a for a in addresses if ":" not in a]
                return (ipv4 or addresses or [None])[0]
            return None
        try:
            return socket.gethostbyname(host + ".local")
        except OSError:
            return None

    def check(self, target: Target, forced: bool = False) -> SyncResult | None:
        st = self.status[target.device]
        now = time.monotonic()
        if forced and now - self._mono_attempt.get(target.device, -1e9) < TRIGGER_DEBOUNCE_S:
            return None
        address = self.resolve(target.host)
        if address is None:
            if st.online:
                log.info("%s went offline", target.device)
            st.online = False
            return None
        appeared = not st.online
        st.online, st.address, st.last_seen = True, address, _now_iso()
        stale = now - self._mono_sync_ok.get(target.device, -1e9) > RESYNC_S
        if not (forced or appeared or stale):
            return None
        return self._pull(target, address)

    def _pull(self, target: Target, address: str) -> SyncResult | None:
        st = self.status[target.device]
        self._mono_attempt[target.device] = time.monotonic()
        st.syncing, st.last_sync = True, _now_iso()
        base = f"http://{address}"
        log.info("%s: pulling from %s", target.device, base)
        try:
            result = sync(self.storage, target.device, base, self.http)
        except Exception as exc:
            st.last_error = str(exc)
            log.warning("%s: pull failed: %s", target.device, exc)
            return None
        finally:
            st.syncing = False
        st.last_result = asdict(result)
        st.last_error = "; ".join(result.errors[:3]) or None
        if not result.failed:
            st.last_sync_ok = st.last_sync
            self._mono_sync_ok[target.device] = time.monotonic()
        log.info("%s: %s", target.device, result.summary())
        if result.fetched or result.replaced:
            exporter.export_pending(self.storage)
        return result

    def as_dict(self) -> dict:
        return {"enabled": True, "mdns": self._browser is not None,
                "interval_s": self.settings.pull_interval_s,
                "targets": [asdict(s) for s in self.status.values()]}
