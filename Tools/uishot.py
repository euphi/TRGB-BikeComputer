#!/usr/bin/env python3
"""Remote UI testing against the bike computer's /debug/ui/* routes (src/UiDebug.h).

    uishot.py shot out.png                 screenshot of the active screen
    uishot.py tap X Y                      tap
    uishot.py press X Y [MS]               long press (default 800 ms)
    uishot.py swipe X Y X2 Y2 [MS]         swipe (default 300 ms)
    uishot.py screen                       name of the active screen

Host: --host (default TRGB-BC.local). Every touch command waits until the touch has
finished on the device, so commands can simply be chained in a shell script.
"""

import argparse
import sys
import time
import urllib.error
import urllib.request

from PIL import Image


def get(host, path, timeout=10):
    with urllib.request.urlopen(f"http://{host}{path}", timeout=timeout) as r:
        return r.status, r.headers, r.read()


def touch(host, x, y, x2=None, y2=None, ms=100):
    q = f"/debug/ui/touch?x={x}&y={y}&ms={ms}"
    if x2 is not None:
        q += f"&x2={x2}&y2={y2}"
    for _ in range(50):
        try:
            get(host, q)
            break
        except urllib.error.HTTPError as e:
            if e.code != 409:
                raise
            time.sleep(0.1)
    time.sleep(ms / 1000 + 0.15)		# touch runs on the device's own clock


def shot(host, out):
    get(host, "/debug/ui/snap")
    for _ in range(50):
        time.sleep(0.2)
        try:
            _, headers, data = get(host, "/debug/ui/snap.raw", timeout=30)
            break
        except urllib.error.HTTPError as e:
            if e.code != 409:
                raise
    else:
        sys.exit("snapshot not ready")
    w, h = int(headers["X-Width"]), int(headers["X-Height"])
    img = Image.frombytes("RGB", (w, h), data, "raw", "BGR;16")
    img.save(out)
    print(f"{out}: {w}x{h}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="TRGB-BC.local")
    ap.add_argument("cmd", choices=["shot", "tap", "press", "swipe", "screen"])
    ap.add_argument("args", nargs="*")
    a = ap.parse_args()
    v = a.args
    if a.cmd == "shot":
        shot(a.host, v[0] if v else "uishot.png")
    elif a.cmd == "tap":
        touch(a.host, int(v[0]), int(v[1]))
    elif a.cmd == "press":
        touch(a.host, int(v[0]), int(v[1]), ms=int(v[2]) if len(v) > 2 else 800)
    elif a.cmd == "swipe":
        touch(a.host, int(v[0]), int(v[1]), int(v[2]), int(v[3]), ms=int(v[4]) if len(v) > 4 else 300)
    elif a.cmd == "screen":
        print(get(a.host, "/debug/ui/screen")[2].decode())


if __name__ == "__main__":
    main()
