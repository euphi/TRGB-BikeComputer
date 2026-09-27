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
    assert not p._forced
    p._on_service(None, "_http._tcp.local.", "trgb-bc._http._tcp.local.", State)
    assert p._forced == {"trgb"}
