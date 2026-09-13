"""Validate the bounded persistent ngspice pause candidate with native helpers."""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    args.output.mkdir(parents=True, exist_ok=False)
    dll = root / "build/deps/ngspice/Spice64_dll/dll-vs/ngspice.dll"
    inputs = [str(dll), str(root / "build/deps/ngspice-console/Spice64/bin"),
              str(root / "tools/backend_probe/initialization"), "5000000", "0", "none"]
    def run(name: str, label: str) -> list[dict]:
        result = subprocess.run([str(args.native.resolve() / (name + ".exe")), *inputs],
                                capture_output=True, text=True, timeout=30)
        (args.output / (label + ".stdout.txt")).write_text(result.stdout, encoding="utf-8")
        (args.output / (label + ".stderr.txt")).write_text(result.stderr, encoding="utf-8")
        if result.returncode != 0:
            raise RuntimeError(f"{label} failed: {result.returncode}; inspect retained logs")
        return [json.loads(line) for line in result.stdout.splitlines()]
    reference = run("e05_circuit", "reference")[0]
    repetitions = [run("e05_persistent_circuit", f"persistent-{i}") for i in range(1, 4)]
    expected = [2010875, 2011375, 4010750, 4027000, 5000000]
    for records in repetitions:
        if len(records) != len(expected) or not records[-1]["all_boundaries_exact"]:
            raise RuntimeError("Incomplete boundary sequence")
        for record, ns in zip(records, expected):
            if record.get("boundary_ns", record.get("target_ns")) != ns:
                raise RuntimeError("Unexpected boundary")
            if abs(record["actual_s"] - ns * 1e-9) > 1e-12:
                raise RuntimeError("Boundary error exceeds 1 ps")
            analytical = 3.3 * (1 - math.exp(-ns * 1e-9 / 0.001))
            if abs(record["output_v"] - analytical) > 1e-5:
                raise RuntimeError("Analytical voltage error exceeds 10 microvolts")
        if abs(records[-1]["output_v"] - reference["output_v"]) > 1e-5:
            raise RuntimeError("Continuous reference mismatch")
    if any(records != repetitions[0] for records in repetitions[1:]):
        raise RuntimeError("Repetitions differ")
    paths = [Path(__file__).resolve(), Path(__file__).with_name("persistent_circuit.cpp"),
             Path(__file__).with_name("circuit.cpp"), dll,
             args.native.resolve() / "e05_persistent_circuit.exe",
             args.native.resolve() / "e05_circuit.exe"]
    report = {"status": "passed", "complete_e05_profile": False,
              "scope": "Known future boundaries in one persistent foreground RC circuit",
              "time_tolerance_s": 1e-12, "voltage_tolerance_v": 1e-5,
              "pause_observation_wall_ms": 100, "reference": reference,
              "repetitions": repetitions,
              "sha256": {str(p.relative_to(root)).replace(chr(92), "/"):
                         hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}}
    (args.output / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"status": "passed", "repetitions": len(repetitions),
                      "endpoint_difference_v": repetitions[0][-1]["output_v"] - reference["output_v"]}))


if __name__ == "__main__":
    main()
