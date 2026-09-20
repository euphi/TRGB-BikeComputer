#!/usr/bin/python3
"""Convert a binary datalog from the bike computer to CSV.

Kept as a thin wrapper around ``python3 -m bikelog csv`` so the old
invocation and its options keep working. The actual code lives in the
bikelog package next to this file, where the GPX exporter and the upload
service share it.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from bikelog.cli import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main(["csv"] + sys.argv[1:]))
