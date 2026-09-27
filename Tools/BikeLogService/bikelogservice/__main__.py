"""Command line: run the service, or fill its storage by hand.

    python -m bikelogservice serve [--host 0.0.0.0] [--port 8080]
    python -m bikelogservice pull  [--device trgb] [TRGB-BC.local | 192.168.0.171]
    python -m bikelogservice import /media/sdcard/BIKECOMP [--device trgb]

``pull`` and ``import`` work on the same storage as the service
(BIKELOG_DATA_DIR) and may run while it does.
"""

from __future__ import annotations

import argparse
import logging
import sys
from pathlib import Path

from . import sdlayout
from .config import Settings
from .puller import Http, sync
from .storage import Storage


def _serve(args) -> int:
    import uvicorn

    uvicorn.run("bikelogservice.app:app", host=args.host, port=args.port,
                proxy_headers=True)
    return 0


def _pull(args) -> int:
    store = Storage(Settings.from_env())
    base = args.host if args.host.startswith("http") else "http://" + args.host
    result = sync(store, args.device, base.rstrip("/"), Http())
    print(result.summary())
    for err in result.errors:
        print("  " + err, file=sys.stderr)
    return 1 if result.failed else 0


def _import(args) -> int:
    """Copy a BIKECOMP tree (card reader, backup) into the storage."""
    store = Storage(Settings.from_env())
    root = Path(args.dir)
    counts: dict[str, int] = {}
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.stat().st_size == 0:
            continue
        rel = path.relative_to(root).as_posix()
        try:
            sdfile = sdlayout.parse_path(rel)
        except sdlayout.BadPath:
            counts["ignored"] = counts.get("ignored", 0) + 1
            continue
        if sdfile.day == sdlayout.WORKDIR:
            counts["running"] = counts.get("running", 0) + 1
            continue
        status = store.put_file(args.device, sdfile, path.read_bytes(),
                                source="import:" + str(root)).status
        counts[status] = counts.get(status, 0) + 1
    print(", ".join(f"{n} {k}" for k, n in sorted(counts.items())) or "nothing found")
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(prog="bikelogservice")
    sub = parser.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("serve", help="run the web service")
    p.add_argument("--host", default="0.0.0.0")
    p.add_argument("--port", type=int, default=8080)
    p.set_defaults(func=_serve)

    p = sub.add_parser("pull", help="fetch new files from a device once")
    p.add_argument("host", nargs="?", default="TRGB-BC.local")
    p.add_argument("--device", default="trgb")
    p.set_defaults(func=_pull)

    p = sub.add_parser("import", help="import a BIKECOMP directory tree")
    p.add_argument("dir")
    p.add_argument("--device", default="trgb")
    p.set_defaults(func=_import)

    args = parser.parse_args(argv)
    logging.basicConfig(level=logging.INFO, format="%(levelname)s %(name)s: %(message)s")
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
