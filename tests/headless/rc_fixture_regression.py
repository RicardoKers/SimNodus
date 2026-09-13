"""Compare extracted and reference workers using real ngspice in fresh processes."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--runner", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    original = ROOT / "build/sn016/persistent-native/Debug/e05_joint_circuit.exe"
    dll = ROOT / "build/deps/ngspice/Spice64_dll/dll-vs/ngspice.dll"
    tail = [dll, ROOT / "build/deps/ngspice-console/Spice64/bin",
            ROOT / "tools/backend_probe/initialization", "10000000", "none", "none"]
    report = {"status": "running", "cases": [], "commands": [],
              "sha256": {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in (args.runner, original, dll, Path(__file__),
                                   ROOT / "src/adapters/ngspice/rc_fixture.cpp",
                                   ROOT / "src/core/backend_contracts.hpp")}}
    def run(binary, text):
        command = list(map(str, [binary, *tail]))
        if command not in report["commands"]:
            report["commands"].append(command)
        return subprocess.run(command, input=text, text=True, capture_output=True, timeout=10)
    try:
        boundaries = sorted(set(range(2012000, 4010000, 31250)) |
                            {3139875, 3139999, 3140000, 3140001, 3140125, 4010749})
        boundaries += [3140000] * 2
        for boundary in boundaries:
            text = (f"advance 2010875\ninspect\nadvance 2011375\nhigh\ninspect\n"
                    f"advance {boundary}\ninspect\nadvance 4010750\ninspect\nadvance 4027000\ninspect\nquit\n")
            old, new = run(original, text), run(args.runner, text)
            case = {"boundary_ns": boundary, "input": text, "original_exit": old.returncode,
                    "exit": new.returncode, "stderr": new.stderr, "stdout": new.stdout}
            report["cases"].append(case)
            if old.returncode or new.returncode or old.stdout != new.stdout:
                raise RuntimeError("Extracted worker differs from reference")
            snapshots = [json.loads(line) for line in new.stdout.splitlines()]
            expected = [0, 2010875, 2010875, 2011375, 2011375, 2011375, boundary, boundary,
                        4010750, 4010750, 4027000, 4027000]
            if [p["time_ns"] for p in snapshots] != expected:
                raise RuntimeError("Unexpected boundaries")
            for point in snapshots:
                t = point["time_ns"]
                voltage = 0 if t <= 2011375 else 3.3 * (1-math.exp(-(t-2011375)*1e-9/.001))
                if abs(point["actual_s"]-t*1e-9) > 1e-12 or abs(point["output_v"]-voltage) > 1e-5:
                    raise RuntimeError("Independent time/RC tolerance exceeded")
        report["adversarial"] = []
        for text in ("advance 0\n", "advance 10000000\n", "advance -1\n", "advance 1 extra\n",
                     "high\n", "low\n", "unknown\n", "advance 2011375\nadvance 2011375\n", ""):
            result = run(args.runner, text)
            report["adversarial"].append({"input": text, "exit": result.returncode,
                                           "stdout": result.stdout, "stderr": result.stderr})
            if result.returncode == 0:
                raise RuntimeError("Invalid request was accepted")
        report["status"] = "passed"
    except (RuntimeError, ValueError, subprocess.SubprocessError) as error:
        report.update(status="failed", error=str(error))
    (args.output / "summary.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(report["status"], len(report["cases"]), "boundaries", report.get("error", ""))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
