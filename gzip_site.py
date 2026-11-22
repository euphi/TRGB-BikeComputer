# Pre-compress the text files of the web site before the LittleFS image is built.
# ESPAsyncWebServer's serveStatic() serves "<file>.gz" (gzip content) when it exists, and
# tries it first. That cuts flash and, more to the point, the internal heap a large response costs
# while it is in flight (measured per request, see WebInstrument "peak"). Fonts (.woff2) and other
# already compressed files stay as they are. The originals are not kept: every browser accepts gzip.
# The source tree is not touched; the compressed copy is staged under the build directory and
# PROJECT_DATA_DIR is pointed at it, so "pio run -t buildfs/uploadfs" pick it up.
Import("env")
import gzip
import os
import shutil

GZIP_EXT = (".css", ".js", ".html", ".svg", ".ico")

src = env.subst("$PROJECT_DATA_DIR")
dst = os.path.join(env.subst("$BUILD_DIR"), "data_gz")
if os.path.isdir(src):
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    raw = packed = 0
    for root, _dirs, files in os.walk(src):
        rel = os.path.relpath(root, src)
        out_dir = dst if rel == "." else os.path.join(dst, rel)
        os.makedirs(out_dir, exist_ok=True)
        for name in files:
            path = os.path.join(root, name)
            if name.lower().endswith(GZIP_EXT):
                with open(path, "rb") as f:
                    data = f.read()
                comp = gzip.compress(data, compresslevel=9, mtime=0)
                with open(os.path.join(out_dir, name + ".gz"), "wb") as f:
                    f.write(comp)
                raw += len(data)
                packed += len(comp)
            else:
                shutil.copy2(path, os.path.join(out_dir, name))
    print("gzip_site: %d -> %d bytes (text files), data dir: %s" % (raw, packed, dst))
    env.Replace(PROJECT_DATA_DIR=dst)
