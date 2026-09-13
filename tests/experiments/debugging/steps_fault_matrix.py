"""Run the predeclared extended IDE fault/recovery matrix in fresh workspaces."""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

import run as e


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ide", type=Path, required=True)
    parser.add_argument("--renode", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    opts = parser.parse_args()
    output = opts.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    report = {"status": "running", "complete_e05_profile": False, "runs": [],
              "protocol": "tests/experiments/debugging/JOINT_STEPS_FAULTS.md",
              "driver_sha256": e.sha(Path(__file__))}
    common = ["--profile", "joint", "--guarded", "--experimental-renode", str(opts.renode.resolve())]
    steps = common + ["--joint-steps"]
    race = ["--joint-race", "--joint-wall-pace-ms", "0", "--joint-race-delay-ms", "50",
            "--joint-race-initial-pace"]
    cases = []
    for fault in ("analog", "timeout", "backend", "disconnect"):
        for repetition in range(1, 4):
            prefix = f"race-{fault}-{repetition:02d}"
            cases.append((prefix, steps + race + ["--joint-fault", fault], "fault_verified"))
            cases.append((prefix + "-recovery", steps + race, "partial_probe_passed"))
    for fault in ("analog", "timeout", "backend", "disconnect"):
        cases.append(("paced-" + fault, steps + ["--joint-fault", fault], "fault_verified"))
        cases.append(("paced-" + fault + "-recovery", steps, "partial_probe_passed"))
    cases.extend([
        ("steps-lifecycle", steps + ["--joint-lifecycle"], "partial_probe_passed"),
        ("short-race", common + race, "partial_probe_passed"),
        ("default-lifecycle", ["--profile", "lifecycle", "--guarded"], "partial_probe_passed"),
    ])
    try:
        for name, options, expected in cases:
            directory = output / name
            command = [sys.executable, str(e.HERE / "cubeide.py"), "--ide", str(opts.ide.resolve()),
                       "--output", str(directory), *options]
            entry = {"name": name, "command": command, "expected_status": expected}
            report["runs"].append(entry)
            print("START " + name, flush=True)
            with (output / (name + ".log")).open("w", encoding="utf-8") as log:
                completed = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT,
                                           creationflags=subprocess.CREATE_NO_WINDOW)
            entry["exit_code"] = completed.returncode
            summary = directory / "summary.json"
            e.check(summary.exists(), "IDE run omitted summary: " + name)
            data = json.loads(summary.read_text(encoding="utf-8"))
            entry.update(status=data["status"], summary_sha256=e.sha(summary))
            e.check(completed.returncode == 0 and data["status"] == expected,
                    name + ": " + data.get("error", "unexpected process result"))
            e.check(data["ide_exit"] == 0 and data["workbench_close_accepted"]
                    and data["listeners_removed"] and data["guard_listener_removed"],
                    "IDE cleanup incomplete: " + name)
            if name == "default-lifecycle":
                reference = json.loads((e.ROOT / "build/sn016/resume-normal-04/summary.json").read_text(encoding="utf-8"))
                e.check(data["stops"] == reference["stops"] and data["circuits"] == reference["circuits"],
                        "Default lifecycle reference differs")
                entry["exact_reference"] = "build/sn016/resume-normal-04"
            (output / "matrix.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print("PASS " + name, flush=True)
        report["status"] = "passed"
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        report.update(status="failed", error=str(error))
        print("FAIL " + str(error), flush=True)
    finally:
        (output / "matrix.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
