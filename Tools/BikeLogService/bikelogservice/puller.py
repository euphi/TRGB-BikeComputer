"""Pull the logs from the bike computer as soon as it shows up on the network.

The bike computer already serves its SD card (``/log/<day>/<file>``) and a list
of it, so the service fetches instead of the firmware pushing: no upload client
on the device, no credentials there, no retry logic in a task that competes with
BLE and the display for internal heap.

When to pull:

* **mDNS probes** (the main trigger): the firmware calls ``MDNS.begin("TRGB-BC")``
  after every WiFi connect, and before announcing itself the ESP32 *probes* for
  its name (RFC 6762 8.1: queries for ``TRGB-BC.local`` with the proposed records
  in the authority section). Only a device claiming its name does that, i.e. a
  boot or a reconnect -- exactly when LogSessions has just finished a session.
  ProbeListener sniffs for these on its own socket. Observed 2026-09-27: four
  probes within a second of the device joining the WiFi.
  The ServiceBrowser alone is not enough: it reports only services it does not
  have cached, and a quick reboot keeps the entry in its cache (PTR TTL 75 min).
* **Polling** as a safety net: every ``pull_interval_s`` one multicast address
  query (no traffic to the device if it is away). If it answers and was absent
  before -- or the last pull is older than RESYNC_S -- it pulls.

Triggered pulls wait BOOT_SETTLE_S: right after boot the device is busy (BLE,
LogSessions moving the last session out of CUR/). While CUR/ still holds more
than the running session, the pull is repeated every WORKDIR_RETRY_S.

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

from . import exporter, nextcloud, sdlayout
from .sdlayout import SdFile

log = logging.getLogger("bikelog.pull")

#: Pull again after this long even without a new appearance -- covers a reboot
#: fast enough to fall between two polls without the announcement being seen.
RESYNC_S = 1800
#: mDNS announcements come in bursts (the firmware's plus retransmissions).
TRIGGER_DEBOUNCE_S = 15
#: Delay between a probe/announcement and the pull.
BOOT_SETTLE_S = 10
#: Pull again this soon while CUR/ holds a finished session not moved yet, or
#: after a triggered pull failed (web server not up yet) ...
WORKDIR_RETRY_S = 60
#: ... at most this many times in a row.
MAX_RETRIES = 10
MDNS_GROUP = "224.0.0.251"
MDNS_PORT = 5353
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
    #: Sessions (stems) seen in CUR/: the running one plus any that LogSessions
    #: has not moved yet. More than one = pull again shortly.
    workdir_sessions: int = 0

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
    result.workdir_sessions = len({e.file.stem for e in listing
                                   if e.file.day == sdlayout.WORKDIR})
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


# --- mDNS probes ------------------------------------------------------------

def _dns_name(buf: bytes, off: int) -> tuple[str, int]:
    """Read a (possibly compressed) DNS name; returns (name, offset after it)."""
    labels: list[str] = []
    end = None
    for _ in range(128):                    # bounded: no pointer loops
        length = buf[off]
        if length == 0:
            off += 1
            break
        if length & 0xC0 == 0xC0:
            if end is None:
                end = off + 2
            off = ((length & 0x3F) << 8) | buf[off + 1]
            continue
        labels.append(buf[off + 1:off + 1 + length].decode("utf-8", "replace"))
        off += 1 + length
    else:
        raise ValueError("DNS name too long")
    return ".".join(labels), (end if end is not None else off)


def probed_names(packet: bytes) -> set[str]:
    """Names a packet probes for (lower case, without trailing dot): an mDNS
    query with records in the authority section (RFC 6762 8.1). Empty for
    anything else, including malformed packets."""
    try:
        if len(packet) < 12:
            return set()
        flags, qdcount, _an, nscount = (int.from_bytes(packet[i:i + 2], "big")
                                        for i in (2, 4, 6, 8))
        if flags & 0x8000 or not nscount:   # response, or plain query
            return set()
        names, off = set(), 12
        for _ in range(qdcount):
            name, off = _dns_name(packet, off)
            off += 4                        # type, class
            names.add(name.lower().rstrip("."))
        return names
    except (IndexError, ValueError):
        return set()


class ProbeListener:
    """Passive mDNS sniffer on its own socket (SO_REUSEPORT: coexists with
    zeroconf and avahi, every member socket gets the multicast datagrams)."""

    def __init__(self, on_probe: Callable[[set[str], str], None]):
        self.on_probe = on_probe
        self._stop = threading.Event()
        self._sock: socket.socket | None = None
        self._thread: threading.Thread | None = None

    def start(self) -> bool:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            if hasattr(socket, "SO_REUSEPORT"):
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
            sock.bind(("", MDNS_PORT))
            sock.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP,
                            socket.inet_aton(MDNS_GROUP) + socket.inet_aton("0.0.0.0"))
            sock.settimeout(1.0)
        except OSError as exc:
            log.warning("mDNS probe listener unavailable: %s", exc)
            return False
        self._sock = sock
        self._thread = threading.Thread(target=self._run, name="bikelog-probe", daemon=True)
        self._thread.start()
        return True

    def stop(self) -> None:
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=3)
        if self._sock:
            self._sock.close()

    def _run(self) -> None:
        while not self._stop.is_set():
            try:
                packet, (source, _port) = self._sock.recvfrom(9000)
            except socket.timeout:
                continue
            except OSError:
                return
            names = probed_names(packet)
            if names:
                try:
                    self.on_probe(names, source)
                except Exception:
                    log.exception("probe handler failed")


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
        self._probes: ProbeListener | None = None
        self._wake = threading.Event()
        self._stop = threading.Event()
        self._due: dict[str, float] = {}           # device -> monotonic time of a triggered check
        self._retries: dict[str, int] = {}
        self._lock = threading.Lock()
        self._thread: threading.Thread | None = None
        self._mono_sync_ok: dict[str, float] = {}
        self._mono_attempt: dict[str, float] = {}

    # --- lifecycle ---

    def start(self) -> None:
        if self.settings.pull_mdns or self._resolver is None:
            self._start_zeroconf()
        if self.settings.pull_mdns:
            probes = ProbeListener(self._on_probe)
            if probes.start():
                self._probes = probes
        self._thread = threading.Thread(target=self._run, name="bikelog-pull", daemon=True)
        self._thread.start()
        log.info("pulling from %s every %.0f s%s",
                 ", ".join(f"{t.device}={t.host}.local" for t in self.targets),
                 self.settings.pull_interval_s,
                 " and on mDNS probes/announcements" if self._probes else
                 " and on mDNS announcements" if self._browser else "")

    def stop(self) -> None:
        self._stop.set()
        self._wake.set()
        if self._thread:
            self._thread.join(timeout=5)
        if self._probes:
            self._probes.stop()
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
                self.trigger(target.device, BOOT_SETTLE_S)

    def _on_probe(self, names: set[str], source: str) -> None:
        for target in self.targets:
            if target.host.lower() + ".local" in names:
                if target.device not in self._due:
                    log.info("%s probing for its mDNS name from %s -- (re)started, pulling in %d s",
                             target.device, source, BOOT_SETTLE_S)
                self._retries[target.device] = 0
                self.trigger(target.device, BOOT_SETTLE_S)

    # --- triggers ---

    def trigger(self, device: str | None = None, delay_s: float = 0.0) -> None:
        """Check (and pull from) one device, or all, in delay_s seconds."""
        due = time.monotonic() + delay_s
        with self._lock:
            for dev in ([device] if device else [t.device for t in self.targets]):
                self._due[dev] = min(self._due.get(dev, due), due)
        self._wake.set()

    def _run(self) -> None:
        next_poll = 0.0
        while not self._stop.is_set():
            with self._lock:
                wake_at = min([next_poll, *self._due.values()])
            self._wake.wait(max(0.0, wake_at - time.monotonic()))
            self._wake.clear()
            if self._stop.is_set():
                return
            now = time.monotonic()
            with self._lock:
                forced = {dev for dev, due in self._due.items() if due <= now}
                for dev in forced:
                    del self._due[dev]
            polling = now >= next_poll
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
        result = self._pull(target, address)
        # Again soon if the pull failed (device still booting) or CUR/ still holds a
        # finished session LogSessions is about to move -- bounded, then polling.
        if result is None or result.failed or result.workdir_sessions > 1:
            tries = self._retries.get(target.device, 0)
            if tries < MAX_RETRIES:
                self._retries[target.device] = tries + 1
                self.trigger(target.device, WORKDIR_RETRY_S)
        else:
            self._retries[target.device] = 0
        return result

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
            nextcloud.sync_pending(self.storage)
        return result

    def as_dict(self) -> dict:
        return {"enabled": True, "mdns": self._browser is not None,
                "probes": self._probes is not None,
                "interval_s": self.settings.pull_interval_s,
                "targets": [asdict(s) for s in self.status.values()]}
