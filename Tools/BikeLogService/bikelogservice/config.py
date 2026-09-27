"""Service configuration, read from the environment.

Everything has a working default so the service starts with no setup at all
(``uvicorn bikelogservice.app:app``); the systemd unit overrides what it
needs. Environment variables rather than a config file, because that is what
a systemd unit passes naturally and it keeps secrets out of the repo.
"""

from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path


def _default_data_dir() -> Path:
    # XDG-ish, so a user-level systemd unit needs no extra directory setup.
    base = os.environ.get("STATE_DIRECTORY") or os.environ.get("XDG_STATE_HOME")
    if base:
        return Path(base) / "bikelog"
    return Path.home() / ".local" / "state" / "bikelog"


def _flag(value: str) -> bool:
    return value.strip().lower() in ("1", "true", "yes", "on")


def _split(value: str | None) -> list[str]:
    return [item.strip() for item in (value or "").split(",") if item.strip()]


@dataclass
class Settings:
    data_dir: Path = field(default_factory=_default_data_dir)

    #: Auth is off by default -- home network, one device. Everything needed
    #: to switch it on is already wired (see auth.py): set BIKELOG_REQUIRE_AUTH=1
    #: and BIKELOG_TOKENS, no route changes and no client changes beyond
    #: sending the header the firmware already sends.
    require_auth: bool = False
    #: token[:name] entries, comma separated. The name only shows up in logs.
    tokens: list[str] = field(default_factory=list)

    max_upload_bytes: int = 64 * 1024 * 1024

    #: Pull the logs from the bike computer when it shows up on the network
    #: (see puller.py). Off by default so tests and ad-hoc runs stay passive.
    pull: bool = False
    #: device=mdns-host entries, comma separated. The device name is what the
    #: sessions are filed under; the host is the name the firmware passes to
    #: MDNS.begin(), without ".local".
    pull_targets: list[str] = field(default_factory=lambda: ["trgb=TRGB-BC"])
    #: Fallback poll interval for when an mDNS announcement was missed.
    pull_interval_s: float = 120.0
    #: Listen for mDNS announcements (the trigger that makes pulling immediate).
    pull_mdns: bool = True

    @classmethod
    def from_env(cls, env=None) -> "Settings":
        env = env if env is not None else os.environ
        data_dir = env.get("BIKELOG_DATA_DIR")
        return cls(
            data_dir=Path(data_dir) if data_dir else _default_data_dir(),
            require_auth=_flag(env.get("BIKELOG_REQUIRE_AUTH", "0")),
            tokens=_split(env.get("BIKELOG_TOKENS")),
            max_upload_bytes=int(env.get("BIKELOG_MAX_UPLOAD_BYTES", 64 * 1024 * 1024)),
            pull=_flag(env.get("BIKELOG_PULL", "0")),
            pull_targets=_split(env.get("BIKELOG_PULL_TARGETS")) or ["trgb=TRGB-BC"],
            pull_interval_s=float(env.get("BIKELOG_PULL_INTERVAL_S", 120)),
            pull_mdns=_flag(env.get("BIKELOG_PULL_MDNS", "1")),
        )

    @property
    def sessions_dir(self) -> Path:
        return self.data_dir / "sessions"

    @property
    def db_path(self) -> Path:
        return self.data_dir / "index.sqlite3"

    def ensure_dirs(self) -> None:
        self.sessions_dir.mkdir(parents=True, exist_ok=True)
