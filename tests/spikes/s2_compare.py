"""Spike S2 (SN-104): check and compare ngspice reference results.

Criteria, declared before the first cross-platform comparison (2026-10-14):

- Analytical gate (rc_step, rlc_ring): maximum error of every grid sample
  below 0.5 % of the response's full scale (the E-01 gate).
- Cross-run gate, for every probed vector: RMS difference below 0.1 % and
  maximum difference below 1 % of the vector's range in the reference run.
  The maximum is looser because switching edges fall between grid samples
  differently when two engines choose different time steps.

Usage:
  s2_compare.py check RESULT.json [--reference REF.json]
  s2_compare.py run --exe S2 --library LIB --scripts DIR --output OUT.json
                    [--reference REF.json] NETLIST...
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
from pathlib import Path

ANALYTIC_GATE = 0.005
RMS_GATE = 0.001
MAX_GATE = 0.01


def rc_step(t: float) -> float:
    return 3.3 * (1.0 - math.exp(-t / 1e-3))


def rlc_ring(t: float) -> float:
    r, l, c, v = 50.0, 10e-3, 100e-9, 5.0
    alpha = r / (2 * l)
    wd = math.sqrt(1 / (l * c) - alpha * alpha)
    return v * (1 - math.exp(-alpha * t) * (math.cos(wd * t) + alpha / wd * math.sin(wd * t)))


ANALYTIC = {("rc_step", "out"): (rc_step, 3.3), ("rlc_ring", "out"): (rlc_ring, 10.0)}


def analytic_check(result: dict) -> list[str]:
    failures = []
    for (circuit, vector), (function, scale) in ANALYTIC.items():
        data = result["circuits"].get(circuit)
        if data is None:
            failures.append(f"{circuit}: missing")
            continue
        values = data["vectors"][vector]
        stop = data["stop_time"]
        n = len(values)
        worst = max(abs(values[i] - function(stop * i / (n - 1))) for i in range(n))
        status = "ok" if worst < ANALYTIC_GATE * scale else "FAIL"
        print(f"analytic {circuit}/{vector}: max error {worst:.3g} (gate {ANALYTIC_GATE * scale:.3g}) {status}")
        if status != "ok":
            failures.append(f"{circuit}/{vector} analytic error {worst:.3g}")
    return failures


def compare(result: dict, reference: dict) -> list[str]:
    failures = []
    print(f"compare ngspice-{result['ngspice_version']} ({result['platform']}) "
          f"with ngspice-{reference['ngspice_version']} ({reference['platform']})")
    for circuit, ref in reference["circuits"].items():
        data = result["circuits"].get(circuit)
        if data is None:
            failures.append(f"{circuit}: missing")
            continue
        for vector, ref_values in ref["vectors"].items():
            values = data["vectors"].get(vector, [])
            if len(values) != len(ref_values):
                failures.append(f"{circuit}/{vector}: length {len(values)} != {len(ref_values)}")
                continue
            span = (max(ref_values) - min(ref_values)) or 1.0
            diffs = [a - b for a, b in zip(values, ref_values)]
            worst = max(abs(d) for d in diffs) / span
            rms = math.sqrt(sum(d * d for d in diffs) / len(diffs)) / span
            ok = rms < RMS_GATE and worst < MAX_GATE
            print(f"  {circuit}/{vector}: rms {rms:.3e}, max {worst:.3e} of range "
                  f"(points {data['accepted_points']} vs {ref['accepted_points']}) {'ok' if ok else 'FAIL'}")
            if not ok:
                failures.append(f"{circuit}/{vector}: rms {rms:.3e}, max {worst:.3e}")
    return failures


def check(result_path: Path, reference_path: Path | None) -> int:
    result = json.loads(result_path.read_text(encoding="utf-8"))
    failures = analytic_check(result)
    if reference_path is not None:
        failures += compare(result, json.loads(reference_path.read_text(encoding="utf-8")))
    for failure in failures:
        print(f"FAIL: {failure}")
    print("S2 passed" if not failures else "S2 failed")
    return 0 if not failures else 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="mode", required=True)
    p_check = sub.add_parser("check")
    p_check.add_argument("result", type=Path)
    p_check.add_argument("--reference", type=Path)
    p_run = sub.add_parser("run")
    p_run.add_argument("--exe", required=True)
    p_run.add_argument("--library", required=True)
    p_run.add_argument("--scripts", required=True)
    p_run.add_argument("--output", type=Path, required=True)
    p_run.add_argument("--reference", type=Path)
    p_run.add_argument("netlists", nargs="+")
    args = parser.parse_args()
    if args.mode == "run":
        args.output.parent.mkdir(parents=True, exist_ok=True)
        completed = subprocess.run([str(Path(args.exe).resolve()), args.library, args.scripts, str(args.output), *args.netlists],
                                   timeout=300)
        if completed.returncode != 0:
            print(f"s2_reference exited with {completed.returncode}")
            return 1
        return check(args.output, args.reference)
    return check(args.result, args.reference)


if __name__ == "__main__":
    sys.exit(main())
