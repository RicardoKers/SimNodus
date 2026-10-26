"""Runs every example through simnodus-cli: circuit check, a run (interactive
for a few simulated seconds at maximum speed, or the project's transient run),
the speed relative to real time and any messages. Part of the classroom
robustness pass (2026-10-25); run it before a release to catch regressions.

Run from the repository root:
  python tools/examples_pass.py --build-dir build/release [--seconds 3] [--json out.json]
"""
import argparse
import glob
import json
import os
import subprocess
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
parser.add_argument("--build-dir", default="build/release")
parser.add_argument("--seconds", default="3", help="simulated seconds of an interactive run")
parser.add_argument("--json", help="write the results to this file")
options = parser.parse_args()
CLI = os.path.join(ROOT, options.build_dir, "apps", "simnodus", "simnodus-cli")
SECONDS = options.seconds


class Serve:
    def __init__(self):
        self.p = subprocess.Popen([CLI, "--library", os.path.join(ROOT, "resources", "library"), "serve"], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8")

    def call(self, command, **args):
        self.p.stdin.write(json.dumps({"command": command, "args": args}) + "\n")
        self.p.stdin.flush()
        line = self.p.stdout.readline()
        if not line:
            return {"ok": False, "error": {"code": "crash", "message": "no reply (exit %s)" % self.p.poll()}}
        return json.loads(line)

    def close(self):
        self.p.stdin.close()
        err = self.p.stderr.read()
        self.p.wait(timeout=60)
        return err


rows = []
for path in sorted(glob.glob(os.path.join(ROOT, "examples", "*.snx"))):
    name = os.path.basename(path)[:-4]
    s = Serve()
    row = {"example": name}
    r = s.call("project.open", path=path)
    if not r["ok"]:
        row["open"] = r["error"]
    else:
        row["open_warnings"] = r["result"].get("warnings")
    r = s.call("circuit.check")
    if r["ok"]:
        row["check"] = [d["severity"] + ": " + d["message"] for d in r["result"].get("diagnostics", [])]
    else:
        row["check"] = r["error"]
    settings = s.call("settings.get")["result"]
    row["mode"] = settings["mode"]
    args = {} if settings["mode"] == "transient" else {"duration": SECONDS}
    start = time.time()
    r = s.call("sim.run", **args)
    row["wall"] = round(time.time() - start, 2)
    if r["ok"]:
        st = r["result"]["status"]
        row["sim_time"] = st["time"]
        row["rtf"] = round(st.get("real_time_factor") or 0, 2)
        row["messages"] = r["result"].get("messages", [])
    else:
        row["run_error"] = r["error"]
    row["stderr"] = [l for l in s.close().splitlines() if l.strip()][:8]
    rows.append(row)
    problems = row.get("check") or row.get("messages") or row.get("run_error") or row.get("open")
    speed = f"{row['rtf']:6.2f}x" if row.get("rtf") else "   fast"  # too short a run to measure
    print(f"{name:45} {row['mode']:11} {speed}  {'PROBLEM ' + str(problems) if problems else ''}", flush=True)

if options.json:
    with open(options.json, "w", encoding="utf-8") as fh:
        json.dump(rows, fh, indent=1, ensure_ascii=False)
