import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent.parent
for path in (TOOLS, TOOLS / "BikeLogService"):
    if str(path) not in sys.path:
        sys.path.insert(0, str(path))
