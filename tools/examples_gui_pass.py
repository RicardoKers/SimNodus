"""Drives every example in the real SimNodus window like a student would.

Starts simnodus --control (offscreen, own settings, own control name), then
for each example: open, run interactively at the project's speed, operate every
switch, button, potentiometer and adjustable source, pause, resume, stop, save
a copy, reopen the copy and check it. Prints errors and messages per example.
Part of the classroom robustness pass (2026-10-25).

Run from the repository root:
  python tools/examples_gui_pass.py --build-dir build/release [--qt-bin C:/Qt/6.11.1/msvc2022_64/bin]
"""
import argparse
import glob
import json
import os
import subprocess
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
parser.add_argument("--build-dir", default="build/release")
parser.add_argument("--qt-bin", default="", help="folder with the Qt DLLs, if they are not on PATH")
options = parser.parse_args()
BIN = os.path.join(ROOT, options.build_dir, "apps", "simnodus")
OUT = tempfile.mkdtemp(prefix="simnodus_gui_pass_")

kinds = {}
for path in glob.glob(os.path.join(ROOT, "resources", "library", "**", "*.json"), recursive=True):
    try:
        d = json.load(open(path, encoding="utf-8"))
    except Exception:
        continue
    if isinstance(d, dict) and d.get("type") and isinstance(d.get("interactive"), dict):
        kinds[d["type"]] = d["interactive"].get("kind")

env = dict(os.environ, QT_QPA_PLATFORM="offscreen", SIMNODUS_CONTROL_NAME="simnodus-robust-pass",
           PATH=(options.qt_bin + os.pathsep if options.qt_bin else "") + os.environ["PATH"])
settings = os.path.join(OUT, "settings.ini")
app = subprocess.Popen([os.path.join(BIN, "simnodus"), "--control", "--settings", settings], env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
time.sleep(4)


class Connect:
    def __init__(self):
        self.p = subprocess.Popen([os.path.join(BIN, "simnodus-cli"), "connect"], env=env, stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8")

    def call(self, command, **args):
        if app.poll() is not None:
            return {"ok": False, "error": {"code": "crash", "message": "window exited with %s" % app.returncode}}
        self.p.stdin.write(json.dumps({"command": command, "args": args}) + "\n")
        self.p.stdin.flush()
        line = self.p.stdout.readline()
        if not line:
            return {"ok": False, "error": {"code": "no-reply", "message": self.p.stderr.read()[:300]}}
        return json.loads(line)


c = Connect()
hello = c.call("hello", directory=OUT)
if not hello.get("ok"):
    raise SystemExit(f"no window: {hello}")
rows = []
for path in sorted(glob.glob(os.path.join(ROOT, "examples", "*.snx"))):
    name = os.path.basename(path)[:-4]
    row = {"example": name, "errors": []}

    def step(command, **args):
        t = time.time()
        r = c.call(command, **args)
        if not r.get("ok"):
            row["errors"].append("%s: %s" % (command, r.get("error")))
        row.setdefault("timings", {})[command] = round(time.time() - t, 2)
        return r.get("result", {}) if r.get("ok") else None

    step("project.open", path=path, discard=True)
    info = step("circuit.inspect") or {}
    controls = [(x["designator"], kinds[x["type"]]) for x in info.get("components", []) if x["type"] in kinds]
    row["controls"] = len(controls)
    mode = (info.get("settings") or {}).get("mode", "interactive")
    if mode == "transient":
        step("sim.run")
    else:
        step("sim.start")
        step("sim.advance", by="0.5")
        for designator, kind in controls:
            if kind in ("toggle",):
                step("sim.control", component=designator, value="1")
            elif kind == "button":
                step("sim.control", component=designator, value="1")
            elif kind == "potentiometer":
                step("sim.control", component=designator, value="0.8")
            else:
                continue
            step("sim.advance", by="0.2")
            step("sim.control", component=designator, value="0" if kind != "potentiometer" else "0.2")
            step("sim.advance", by="0.2")
        step("sim.pause")
        step("sim.advance", by="0.2")  # resumes
        status = step("sim.status") or {}
        row["messages"] = status.get("messages", [])
        step("sim.stop")
    copy = os.path.join(OUT, name + "_copy.snx")
    step("project.save", path=copy)
    step("project.open", path=copy, discard=True)
    check = step("circuit.check") or {}
    row["check_after_save"] = [d["message"] for d in check.get("diagnostics", [])]
    rows.append(row)
    problems = row["errors"] + row.get("messages", []) + row["check_after_save"]
    print(f"{name:45} {row['controls']} control(s)  {'PROBLEM ' + str(problems) if problems else 'ok'}", flush=True)

c.p.stdin.close()
app.terminate()
try:
    log = app.communicate(timeout=20)[0]
except subprocess.TimeoutExpired:
    app.kill()
    log = app.communicate()[0]
if log.strip():
    print("window output:", log[-1500:])
raise SystemExit(1 if any(r["errors"] or r.get("messages") or r["check_after_save"] for r in rows) else 0)
