"""Push inbox: sessions a bike computer uploaded to a Nextcloud folder -- against a fake
WebDAV server, so the tests never touch the network. And the automatic push straight to the
service (X-BikeLog-Auto), which must not bring deleted sessions back either."""

from __future__ import annotations

import datetime
import urllib.parse

import pytest
from fastapi.testclient import TestClient

from bikelog import fixtures
from bikelog.record import write_records
from bikelogservice import inbox
from bikelogservice.app import create_app
from bikelogservice.config import Settings
from bikelogservice.storage import Storage

API = "/api/v1"
DAV_ROOT = "/remote.php/dav/files/bike/"
NOW = datetime.datetime(2026, 10, 18, 12, 0, tzinfo=datetime.timezone.utc)
OLD = NOW - datetime.timedelta(hours=3)


class _Resp:
    def __init__(self, status_code, content=b""):
        self.status_code = status_code
        self.content = content

    def raise_for_status(self):
        if self.status_code >= 400:
            raise RuntimeError(f"HTTP {self.status_code}")


class FakeDav:
    """A WebDAV folder tree: {"BikeLog-Inbox/TRGB-FL/20261018/L_143012.bin": bytes}; folders
    are implied by the paths, plus explicitly empty ones in `dirs`."""

    def __init__(self, files: dict[str, bytes], modified=OLD, file_times: dict | None = None):
        self.files = dict(files)
        self.file_times = file_times or {}
        self.dirs: set[str] = set()
        self.modified = modified
        self.calls: list[tuple[str, str]] = []

    def _path(self, url: str) -> str:
        path = urllib.parse.unquote(urllib.parse.urlparse(url).path)
        assert path.startswith(DAV_ROOT), path
        return path[len(DAV_ROOT):].strip("/")

    def _children(self, folder: str) -> dict[str, bool]:
        prefix = folder + "/"
        out: dict[str, bool] = {}
        for path in [*self.files, *self.dirs]:
            if path.startswith(prefix):
                rest = path[len(prefix):]
                name, sep, _ = rest.partition("/")
                out[name] = out.get(name, False) or bool(sep) or path in self.dirs
        return out

    def _exists_dir(self, folder: str) -> bool:
        return folder in self.dirs or any(p.startswith(folder + "/") for p in self.files)

    def request(self, method, url, data=None, headers=None):
        folder = self._path(url)
        self.calls.append((method, folder))
        assert method == "PROPFIND" and headers["Depth"] == "1"
        if not self._exists_dir(folder):
            return _Resp(404)
        stamp = self.modified.strftime("%a, %d %b %Y %H:%M:%S GMT")
        parts = [f"<d:response><d:href>{DAV_ROOT}{folder}/</d:href><d:propstat><d:prop>"
                 "<d:resourcetype><d:collection/></d:resourcetype></d:prop></d:propstat></d:response>"]
        for name, is_dir in sorted(self._children(folder).items()):
            href = urllib.parse.quote(f"{DAV_ROOT}{folder}/{name}") + ("/" if is_dir else "")
            if is_dir:
                prop = f"<d:resourcetype><d:collection/></d:resourcetype><d:getlastmodified>{stamp}</d:getlastmodified>"
            else:
                size = len(self.files[f"{folder}/{name}"])
                when = self.file_times.get(f"{folder}/{name}", self.modified)
                prop = (f"<d:resourcetype/><d:getcontentlength>{size}</d:getcontentlength>"
                        f"<d:getlastmodified>{when.strftime('%a, %d %b %Y %H:%M:%S GMT')}</d:getlastmodified>")
            parts.append(f"<d:response><d:href>{href}</d:href><d:propstat><d:prop>{prop}"
                         "</d:prop></d:propstat></d:response>")
        body = '<?xml version="1.0"?><d:multistatus xmlns:d="DAV:">' + "".join(parts) + "</d:multistatus>"
        return _Resp(207, body.encode())

    def get(self, url):
        path = self._path(url)
        self.calls.append(("GET", path))
        return _Resp(200, self.files[path]) if path in self.files else _Resp(404)

    def delete(self, url):
        path = self._path(url)
        self.calls.append(("DELETE", path))
        if path in self.files:
            del self.files[path]
            return _Resp(204)
        if self._exists_dir(path):
            self.files = {p: b for p, b in self.files.items() if not p.startswith(path + "/")}
            self.dirs = {d for d in self.dirs if d != path and not d.startswith(path + "/")}
            return _Resp(204)
        return _Resp(404)


@pytest.fixture
def settings(tmp_path):
    return Settings(data_dir=tmp_path / "state", nextcloud_url="https://cloud.example",
                    nextcloud_user="bike", nextcloud_password="app-pw",
                    nextcloud_inbox="BikeLog-Inbox", pull_targets=["pendler=TRGB-FL"])


@pytest.fixture
def ride_bytes(tmp_path):
    path = tmp_path / "ride.bin"
    write_records(path, fixtures.synthetic())
    return path.read_bytes()


def _collect(store, dav, now=NOW):
    return inbox.collect(store, inbox.Inbox(store.settings, dav), now=now)


def test_parse_multistatus_skips_the_folder_itself():
    dav = FakeDav({"In/TRGB-FL/20261018/L_143012.bin": b"x" * 5, "In/TRGB-FL/L0042.bin": b"y"})
    body = dav.request("PROPFIND", "https://c" + DAV_ROOT + "In/TRGB-FL/", headers={"Depth": "1"}).content
    entries = inbox.parse_multistatus(body)
    assert [(e.name, e.is_dir, e.size) for e in entries] == [("20261018", True, None), ("L0042.bin", False, 1)]
    assert entries[0].modified == OLD


def test_collect_files_pushed_sessions_like_pulled_ones(settings, ride_bytes):
    store = Storage(settings)
    dav = FakeDav({
        "BikeLog-Inbox/TRGB-FL/20261018/L_143012.bin": ride_bytes,
        "BikeLog-Inbox/TRGB-FL/20261018/D_143012.log": b"debug\n",
        "BikeLog-Inbox/TRGB-FL/L0042.bin": ride_bytes,           # old layout, directly in /BIKECOMP
        "BikeLog-Inbox/TRGB-BC/20261017/D_090000.log": b"other\n",
    })
    result = _collect(store, dav)
    assert (result.files, result.stored, result.failed) == (4, 4, 0)
    known = store.known_files("pendler")                        # TRGB-FL -> pendler (pull target)
    assert set(known) == {"20261018/L_143012.bin", "20261018/D_143012.log", "L0042.bin"}
    assert set(store.known_files("TRGB-BC")) == {"20261017/D_090000.log"}   # no target: own name
    # Everything stored is gone from the inbox, the emptied (and quiet) day folders too.
    assert dav.files == {}
    assert result.folders_removed == 2
    assert store.find("pendler", "20261018", "_143012").files[0].source.startswith("inbox:TRGB-FL/")


def test_same_bytes_as_pulled_are_unchanged(settings, ride_bytes):
    store = Storage(settings)
    from bikelogservice.sdlayout import parse_path
    store.put_file("pendler", parse_path("20261018/L_143012.bin"), ride_bytes, source="http://pull")
    dav = FakeDav({"BikeLog-Inbox/TRGB-FL/20261018/L_143012.bin": ride_bytes})
    result = _collect(store, dav)
    assert (result.stored, result.unchanged) == (0, 1)
    assert dav.files == {}


def test_deleted_sessions_stay_deleted(settings, ride_bytes):
    store = Storage(settings)
    from bikelogservice.sdlayout import parse_path
    put = store.put_file("pendler", parse_path("20261018/L_143012.bin"), ride_bytes)
    store.delete(put.session.id)
    dav = FakeDav({"BikeLog-Inbox/TRGB-FL/20261018/L_143012.bin": ride_bytes})
    result = _collect(store, dav)
    assert (result.dropped, result.stored) == (1, 0)
    assert dav.files == {}
    assert store.find("pendler", "20261018", "_143012").deleted_at


def test_a_busy_day_folder_is_kept(settings, ride_bytes):
    """Written to a moment ago: the bike computer may be uploading into it right now."""
    store = Storage(settings)
    dav = FakeDav({"BikeLog-Inbox/TRGB-FL/20261018/L_143012.bin": ride_bytes}, modified=NOW)
    result = _collect(store, dav)
    assert result.stored == 1 and result.folders_removed == 0
    assert ("DELETE", "BikeLog-Inbox/TRGB-FL/20261018") not in dav.calls


def test_junk_and_running_sessions_are_dropped(settings):
    store = Storage(settings)
    dav = FakeDav({"BikeLog-Inbox/TRGB-FL/CUR/L_0042.bin": b"x", "BikeLog-Inbox/TRGB-FL/20261018/README": b"x",
                   "BikeLog-Inbox/bad name!/20261018/L_1.bin": b"x"})
    result = _collect(store, dav)
    assert (result.files, result.dropped, result.stored) == (2, 2, 0)
    assert list(dav.files) == ["BikeLog-Inbox/bad name!/20261018/L_1.bin"]      # not a device: left alone


def test_upload_link_files_arrive_flat(settings, ride_bytes):
    """A Nextcloud files drop keeps only the name: host~day~name, at the root of the shared folder
    or in a folder per nickname (file request)."""
    store = Storage(settings)
    dav = FakeDav({
        "BikeLog-Inbox/TRGB-FL~20261018~L_143012.bin": ride_bytes,
        "BikeLog-Inbox/TRGB-FL~~L0042.bin": ride_bytes,
        "BikeLog-Inbox/TRGB-BC/TRGB-BC~20261017~D_090000.log": b"debug\n",
        "BikeLog-Inbox/TRGB-BC~push-test.txt": b"TRGB-BC log push test\n",
        "BikeLog-Inbox/TRGB-BC~push-test (2).txt": b"TRGB-BC log push test\n",
        "BikeLog-Inbox/holiday.jpg": b"not ours",
    })
    result = _collect(store, dav)
    assert (result.stored, result.failed) == (3, 0)
    assert set(store.known_files("pendler")) == {"20261018/L_143012.bin", "L0042.bin"}
    assert set(store.known_files("TRGB-BC")) == {"20261017/D_090000.log"}
    assert set(dav.files) == {"BikeLog-Inbox/holiday.jpg"}         # someone else's file stays


def test_renamed_copies_newest_wins(settings):
    """The I_ summary uploaded again before the service collected the first one: Nextcloud
    stores the second as "... (2).txt"."""
    store = Storage(settings)
    old_summary, new_summary = b"v=1\ndist_m=1\n", b"v=2\ndist_m=2\n"
    dav = FakeDav({"BikeLog-Inbox/TRGB-FL~20261018~I_143012 (2).txt": new_summary,
                   "BikeLog-Inbox/TRGB-FL~20261018~I_143012.txt": old_summary},
                  file_times={"BikeLog-Inbox/TRGB-FL~20261018~I_143012 (2).txt": NOW,
                              "BikeLog-Inbox/TRGB-FL~20261018~I_143012.txt": OLD})
    result = _collect(store, dav)
    assert result.stored == 2 and dav.files == {}
    session = store.find("pendler", "20261018", "_143012")
    assert store.path_for(session, "I_143012.txt").read_bytes() == new_summary


def test_missing_inbox_is_empty(settings):
    result = _collect(Storage(settings), FakeDav({}))
    assert result.files == 0


def test_inbox_is_off_without_folder_or_login(tmp_path):
    assert not Settings(data_dir=tmp_path, nextcloud_inbox="In").inbox_enabled
    assert not Settings(data_dir=tmp_path, nextcloud_url="https://c", nextcloud_user="u",
                        nextcloud_password="p").inbox_enabled
    env = {"BIKELOG_NEXTCLOUD_URL": "https://c/", "BIKELOG_NEXTCLOUD_USER": "u",
           "BIKELOG_NEXTCLOUD_PASSWORD": "p", "BIKELOG_NEXTCLOUD_INBOX": "/BikeLog-Inbox/"}
    s = Settings.from_env(env)
    assert s.inbox_enabled and s.nextcloud_inbox == "BikeLog-Inbox" and s.inbox_interval_s == 300


def test_run_once_exports_what_arrived(settings, ride_bytes, monkeypatch):
    calls = []
    monkeypatch.setattr(inbox.exporter, "export_pending", lambda store: calls.append("export"))
    monkeypatch.setattr(inbox.nextcloud, "sync_pending", lambda store: calls.append("sync"))
    monkeypatch.setattr(inbox.analysis, "refresh", lambda store: calls.append("analyse"))
    store = Storage(settings)
    dav = FakeDav({"BikeLog-Inbox/TRGB-FL/20261018/L_143012.bin": ride_bytes})
    assert inbox.run_once(store, http=dav).stored == 1
    assert calls == ["export", "sync", "analyse"]
    calls.clear()
    assert inbox.run_once(store, http=dav).files == 0
    assert calls == []


# --- automatic push straight to the service ---------------------------------------------

def test_auto_push_does_not_revive_a_deleted_session(tmp_path, ride_bytes):
    with TestClient(create_app(Settings(data_dir=tmp_path / "state"))) as client:
        url = f"{API}/devices/gravel/files/20260920/L_143012.bin"
        sid = client.put(url, content=ride_bytes).json()["session"]["id"]
        client.delete(f"{API}/sessions/{sid}")
        r = client.put(url, content=ride_bytes, headers={"X-BikeLog-Auto": "1"})
        assert r.status_code == 200 and r.json()["status"] == "deleted"
        assert client.get(f"{API}/sessions/{sid}").status_code == 404
        # without the header (a person uploading on purpose) it comes back, as before
        assert client.put(url, content=ride_bytes).json()["status"] == "new"


def test_inbox_api_without_inbox(tmp_path):
    with TestClient(create_app(Settings(data_dir=tmp_path / "state"))) as client:
        assert client.get(f"{API}/inbox").json() == {"enabled": False}
        assert client.post(f"{API}/inbox").status_code == 409
        assert client.get(f"{API}/health").json()["inbox"] is False
