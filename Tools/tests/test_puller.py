"""The puller: what it fetches from the device, and when."""

import json

import pytest

from bikelogservice import puller as pullmod
from bikelogservice.config import Settings
from bikelogservice.puller import HttpError, Puller, Target, parse_html_listing, sync
from bikelogservice.storage import Storage

# Trimmed from a real /logfiles/ page (BCLogger::getAllFileLinks).
HTML = b"""
<h3>In progress</h3><table><tbody>
<tr><td>#0860<br><span class="badge badge-info">running</span></td><td>
<a class="badge badge-info" href="/log/CUR/L_0860.bin">DATA 1.2 kB</a></td><td></td></tr>
</tbody></table>
<h3>2026-09-27</h3><table><tbody>
<tr><td>16:45:45</td><td><a class="badge badge-info" href="/log/20260927/L_164545.bin">DATA 3 kB</a>
<a class="badge badge-debug" href="/log/20260927/D_164545.log">DEBUG 1 kB</a></td>
<td><a class="btn btn-ghost" href="#" onclick="del(this,['20260927/I_164545.txt','20260927/T_164545.txt','20260927/L_164545.bin','20260927/D_164545.log']);return false;">DEL</a></td></tr>
</tbody></table>
<h3>NO_TIME</h3><table><tbody>
<tr><td>#7</td><td><a class="badge badge-info" href="/log/NO_TIME/L0007.bin">DATA 1 kB</a></td>
<td><a href="#" onclick="del(this,['NO_TIME/L0007.bin']);return false;">DEL</a></td></tr>
</tbody></table>
"""


class FakeDevice:
    """Stands in for Http: serves a file dict, optionally with a JSON listing."""

    def __init__(self, files: dict[str, bytes], json_listing=True, fail=()):
        self.files = files
        self.json_listing = json_listing
        self.fail = set(fail)
        self.requests: list[str] = []

    def get(self, url: str) -> bytes:
        path = url.split("//", 1)[1].split("/", 1)[1]
        self.requests.append(path)
        if path == "logfiles.json":
            if not self.json_listing:
                raise HttpError(url, 404, "Not Found")
            return json.dumps({"files": [{"path": p, "size": len(b)}
                                         for p, b in self.files.items()]}).encode()
        if path == "logfiles/":
            return HTML
        if path.startswith("log/"):
            rel = path[4:]
            if rel in self.fail or rel not in self.files:
                raise HttpError(url, 404, "Not Found")
            return self.files[rel]
        raise HttpError(url, 404, "Not Found")


@pytest.fixture
def store(tmp_path):
    s = Storage(Settings(data_dir=tmp_path / "state"))
    yield s
    s.close()


FILES = {
    "20260927/L_164545.bin": b"\xff" * 128,
    "20260927/I_164545.txt": b"start=1758983412\ndist_m=1000\n",
    "20260927/D_164545.log": b"debug\n",
    "CUR/L_0860.bin": b"growing",
}


def test_html_listing_takes_badges_and_delete_arrays():
    paths = {entry.file.path for entry in parse_html_listing(HTML)}
    assert paths == {"CUR/L_0860.bin", "NO_TIME/L0007.bin", "20260927/D_164545.log",
                     "20260927/I_164545.txt", "20260927/L_164545.bin",
                     "20260927/T_164545.txt"}


def test_sync_fetches_everything_but_the_running_session(store):
    device = FakeDevice(FILES)
    result = sync(store, "trgb", "http://bc", device, pause_s=0)
    assert (result.listed, result.fetched, result.failed) == (3, 3, 0)
    assert "log/CUR/L_0860.bin" not in device.requests
    [session] = store.list()
    assert session.summary["dist_m"] == 1000
    assert len(session.files) == 3


def test_second_sync_only_lists(store):
    sync(store, "trgb", "http://bc", FakeDevice(FILES), pause_s=0)
    device = FakeDevice(FILES)
    result = sync(store, "trgb", "http://bc", device, pause_s=0)
    assert (result.fetched, result.skipped) == (0, 3)
    assert device.requests == ["logfiles.json"]


def test_size_change_is_fetched_again(store):
    sync(store, "trgb", "http://bc", FakeDevice(FILES), pause_s=0)
    changed = dict(FILES, **{"20260927/I_164545.txt": b"start=1758983412\ndist_m=12000\n"})
    result = sync(store, "trgb", "http://bc", FakeDevice(changed), pause_s=0)
    assert result.replaced == 1
    assert store.list()[0].summary["dist_m"] == 12000


def test_deleted_session_is_not_fetched_again(store):
    sync(store, "trgb", "http://bc", FakeDevice(FILES), pause_s=0)
    store.delete(store.list()[0].id)
    device = FakeDevice(FILES)
    result = sync(store, "trgb", "http://bc", device, pause_s=0)
    assert result.fetched == 0
    assert store.count() == 0


def test_failed_download_is_retried_on_the_next_sync(store):
    first = sync(store, "trgb", "http://bc",
                 FakeDevice(FILES, fail={"20260927/D_164545.log"}), pause_s=0)
    assert (first.fetched, first.failed) == (2, 1)
    second = sync(store, "trgb", "http://bc", FakeDevice(FILES), pause_s=0)
    assert second.fetched == 1


def test_html_fallback_when_there_is_no_json_listing(store):
    files = {"20260927/L_164545.bin": b"\xff" * 128, "20260927/D_164545.log": b"d\n",
             "20260927/I_164545.txt": b"start=1\n", "20260927/T_164545.txt": b"start 0\n",
             "NO_TIME/L0007.bin": b"\xff" * 64}
    device = FakeDevice(files, json_listing=False)
    result = sync(store, "trgb", "http://bc", device, pause_s=0)
    assert (result.fetched, result.failed) == (5, 0)
    # Without sizes, a known file is not fetched again.
    device = FakeDevice(files, json_listing=False)
    assert sync(store, "trgb", "http://bc", device, pause_s=0).fetched == 0
    assert device.requests == ["logfiles.json", "logfiles/"]


# --- when to pull ---

class FakeResolver:
    def __init__(self):
        self.address = None

    def __call__(self, host):
        return self.address


@pytest.fixture
def bc(store, monkeypatch):
    resolver = FakeResolver()
    settings = Settings(pull=True, pull_mdns=False, pull_targets=["trgb=TRGB-BC"])
    p = Puller(store, settings, http=FakeDevice(FILES), resolver=resolver)
    monkeypatch.setattr(pullmod, "DOWNLOAD_PAUSE_S", 0)
    return p, resolver, Target("trgb", "TRGB-BC")


def test_target_spec():
    assert Target.parse("gravel=TRGB-BC") == Target("gravel", "TRGB-BC")
    assert Target.parse("TRGB-BC") == Target("TRGB-BC", "TRGB-BC")


def test_offline_device_is_not_contacted(bc):
    p, resolver, target = bc
    assert p.check(target) is None
    assert p.http.requests == []
    assert p.status["trgb"].online is False


def test_appearing_device_is_pulled_once(bc):
    p, resolver, target = bc
    resolver.address = "192.168.0.171"
    assert p.check(target).fetched == 3
    st = p.status["trgb"]
    assert st.online and st.address == "192.168.0.171" and st.last_sync_ok
    # Still there at the next poll: nothing to do until it goes away and comes back.
    assert p.check(target) is None
    resolver.address = None
    p.check(target)
    resolver.address = "192.168.0.171"
    assert p.check(target) is not None


def test_mdns_trigger_pulls_even_if_already_online_but_debounced(bc, monkeypatch):
    p, resolver, target = bc
    resolver.address = "192.168.0.171"
    p.check(target)
    # Announcement burst right after the pull: ignored.
    assert p.check(target, forced=True) is None
    p._mono_attempt["trgb"] -= pullmod.TRIGGER_DEBOUNCE_S + 1
    assert p.check(target, forced=True) is not None


def test_mdns_callback_matches_the_instance_name(bc):
    p, resolver, target = bc

    class State:
        name = "Added"
    p._on_service(None, "_http._tcp.local.", "Shelly._http._tcp.local.", State)
    assert not p._due
    p._on_service(None, "_http._tcp.local.", "trgb-bc._http._tcp.local.", State)
    assert set(p._due) == {"trgb"}


# --- probes: the reboot signal ---

def _probe_packet(host="TRGB-BC", response=False, authority=1):
    """What the ESP32 sent on boot (2026-09-27): questions for the service
    instance and the host, ANY/IN, with the claimed records as authority."""
    import struct

    def name(*labels):
        return b"".join(bytes([len(l)]) + l.encode() for l in labels) + b"\0"
    flags = 0x8400 if response else 0
    q1 = name(host, "_http", "_tcp", "local") + struct.pack("!HH", 255, 1)
    host_off = 12 + len(q1)
    q2 = name(host, "local") + struct.pack("!HH", 255, 1)
    q3 = struct.pack("!H", 0xC000 | host_off) + struct.pack("!HH", 255, 1)   # compressed
    head = struct.pack("!6H", 0, flags, 3, 0, authority, 0)
    return head + q1 + q2 + q3 + b"\xc0\x0c" + b"\x00" * 10


def test_probe_is_recognised():
    assert pullmod.probed_names(_probe_packet()) == {"trgb-bc._http._tcp.local", "trgb-bc.local"}


def test_ordinary_queries_and_responses_are_not_probes():
    assert pullmod.probed_names(_probe_packet(authority=0)) == set()
    assert pullmod.probed_names(_probe_packet(response=True)) == set()
    assert pullmod.probed_names(b"\x00" * 5) == set()
    assert pullmod.probed_names(_probe_packet()[:20]) == set()      # truncated


def test_probe_schedules_a_delayed_pull(bc):
    p, resolver, target = bc
    p._on_probe({"shelly.local"}, "192.168.0.9")
    assert not p._due
    p._on_probe({"trgb-bc.local", "trgb-bc._http._tcp.local"}, "192.168.0.171")
    import time
    assert p._due["trgb"] - time.monotonic() > pullmod.BOOT_SETTLE_S - 1


def test_unmoved_session_in_workdir_means_pull_again(bc, store):
    p, resolver, target = bc
    resolver.address = "192.168.0.171"
    p.http = FakeDevice(dict(FILES, **{"CUR/L_0859.bin": b"old", "CUR/D_0859.log": b"d"}))
    result = p.check(target)
    assert result.workdir_sessions == 2          # running 0860 + finished 0859
    assert "trgb" in p._due                      # retry scheduled
    p._due.clear()
    p._mono_attempt["trgb"] -= 100
    p.http = FakeDevice(FILES)                   # LogSessions has moved it
    assert p.check(target, forced=True).workdir_sessions == 1
    assert not p._due


def test_failed_pull_is_retried(bc):
    p, resolver, target = bc
    resolver.address = "192.168.0.171"

    class Down:
        def get(self, url):
            raise HttpError(url, None, "connection refused")
    p.http = Down()
    assert p.check(target) is None
    assert "trgb" in p._due


def test_sync_reports_what_it_is_doing(store):
    seen = []
    sync(store, "trgb", "http://bc", FakeDevice(FILES), pause_s=0, progress=lambda **kw: seen.append(kw))
    assert seen[0]["phase"] == "listing"
    downloads = [s for s in seen if s["phase"] == "downloading"]
    assert [d["index"] for d in downloads] == [1, 2, 3]
    assert all(d["total"] == 3 for d in downloads)
    assert downloads[-1]["bytes_total"] == sum(len(b) for p, b in FILES.items() if not p.startswith("CUR/"))


def test_a_second_sync_downloads_nothing_to_report(store):
    sync(store, "trgb", "http://bc", FakeDevice(FILES), pause_s=0)
    seen = []
    sync(store, "trgb", "http://bc", FakeDevice(FILES), pause_s=0, progress=lambda **kw: seen.append(kw))
    assert [s["phase"] for s in seen] == ["listing"]


def test_the_page_shows_a_running_pull_and_refreshes_itself():
    from bikelogservice import webui
    pull = {"targets": [{"device": "trgb", "host": "TRGB-BC", "online": True, "syncing": True,
                         "last_sync_ok": None, "last_error": None, "last_result": None,
                         "activity": {"phase": "downloading", "name": "20261004/L_122743.bin",
                                      "index": 2, "total": 5, "bytes_done": 1_000_000, "bytes_total": 4_000_000}}]}
    html = webui.index([], pull)
    assert "Abruf l" in html and "Datei 2/5" in html and "L_122743.bin" in html
    assert 'http-equiv="refresh"' in html
    idle = {"targets": [{**pull["targets"][0], "syncing": False, "activity": None}]}
    assert 'http-equiv="refresh"' not in webui.index([], idle)
