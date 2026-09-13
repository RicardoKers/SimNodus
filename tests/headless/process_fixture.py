"""Owned process-lifecycle test child; no simulation capability is implied."""
import json
import os
from pathlib import Path
import subprocess
import sys
import threading

mode = sys.argv[1]
if mode == "leaf":
    threading.Event().wait()
if mode == "exit7":
    raise SystemExit(7)
child = None
if mode in ("tree", "forced", "owner-loss"):
    child = subprocess.Popen([sys.executable, "-X", "utf8", str(Path(__file__).resolve()), "leaf"],
        stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        creationflags=subprocess.CREATE_NO_WINDOW)
print(json.dumps({"pid": os.getpid(), "args": sys.argv[2:], "cwd": os.getcwd(),
                  "marker": os.environ.get("SN_FIXTURE_MARKER"), "descendant": child.pid if child else None}), flush=True)
for line in sys.stdin:
    if line.strip() == "quit" and mode != "ignore":
        raise SystemExit(0)
if mode == "ignore":
    threading.Event().wait()
