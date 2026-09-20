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

    @classmethod
    def from_env(cls, env=None) -> "Settings":
        env = env if env is not None else os.environ
        data_dir = env.get("BIKELOG_DATA_DIR")
        return cls(
            data_dir=Path(data_dir) if data_dir else _default_data_dir(),
            require_auth=env.get("BIKELOG_REQUIRE_AUTH", "0").lower() in ("1", "true", "yes"),
            tokens=_split(env.get("BIKELOG_TOKENS")),
            max_upload_bytes=int(env.get("BIKELOG_MAX_UPLOAD_BYTES", 64 * 1024 * 1024)),
        )

    @property
    def rides_dir(self) -> Path:
        return self.data_dir / "rides"

    @property
    def db_path(self) -> Path:
        return self.data_dir / "index.sqlite3"

    def ensure_dirs(self) -> None:
        self.rides_dir.mkdir(parents=True, exist_ok=True)
