"""Run the spike S1 scenario matrix, one process per scenario.

usage: run_s1.py --exe S1 --library LIB --scripts DIR --output DIR [--quick]

Writes one JSON file per scenario and summary.json into the output folder and
prints a compact table. Raw results stay in the build tree; the report in
docs/experiments/S1-continuous-run.md quotes them.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

SCENARIOS = [
    ("growth-1us-all", "growth", "maxstep=1e-6 sim=4 save=all"),
    ("growth-1us-probes", "growth", "maxstep=1e-6 sim=4 save=probes"),
    ("growth-10us-probes", "growth", "maxstep=1e-5 sim=10 save=probes"),
    ("growth-100us-probes", "growth", "maxstep=1e-4 sim=20 save=probes"),
    ("growth-100us-interp", "growth", "maxstep=1e-4 sim=20 save=probes interp=1"),
    ("growth-100us-stop3600", "growth", "maxstep=1e-4 sim=10 save=probes stop=3600"),
    ("pacing-100us-1x", "pacing", "maxstep=1e-4 speed=1 wall=10"),
    ("pacing-10us-1x", "pacing", "maxstep=1e-5 speed=1 wall=10"),
    ("pacing-1us-0.1x", "pacing", "maxstep=1e-6 speed=0.1 wall=10"),
    ("pacing-100us-1x-hires", "pacing", "maxstep=1e-4 speed=1 wall=10 hires=1"),
    ("segments-100us-0.5s", "segments", "maxstep=1e-4 sim=6 segment=0.5"),
    ("segments-100us-1s", "segments", "maxstep=1e-4 sim=6 segment=1"),
    ("segments-100us-2s", "segments", "maxstep=1e-4 sim=6 segment=2"),
    ("segments-10us-1s", "segments", "maxstep=1e-5 sim=4 segment=1"),
    ("soak-100us-1s", "soak", "maxstep=1e-4 sim=300 segment=1"),
    ("soak-10us-1s", "soak", "maxstep=1e-5 sim=60 segment=1"),
]

QUICK = {"growth-100us-probes", "pacing-100us-1x", "segments-100us-1s", "soak-10us-1s"}

KEYS = {
    "growth": ["points_per_sim_second", "speed_unpaced", "private_mb_per_sim_second", "bytes_per_point"],
    "pacing": ["achieved_ratio", "max_lag_seconds", "max_oversleep_seconds", "sleep_fraction", "latency_wall",
               "private_mb_per_sim_second"],
    "segments": ["segments", "restart_overhead_median", "restart_overhead_max", "max_error_volts",
                 "max_error_after_restart", "private_mb_per_sim_second"],
    "soak": ["segments", "restart_seconds_median", "restart_seconds_max", "private_mb_first", "private_mb_last",
             "private_mb_peak", "private_mb_per_sim_second_late"],
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", required=True)
    parser.add_argument("--library", required=True)
    parser.add_argument("--scripts", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--quick", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    summary = {}
    failures = 0
    for name, mode, extra in SCENARIOS:
        if args.quick and name not in QUICK:
            continue
        target = args.output / f"{name}.json"
        command = [str(Path(args.exe).resolve()), args.library, args.scripts, str(target), mode, *extra.split()]
        try:
            completed = subprocess.run(command, capture_output=True, text=True, timeout=600)
        except subprocess.TimeoutExpired:
            print(f"{name}: TIMEOUT")
            failures += 1
            continue
        if completed.returncode != 0:
            print(f"{name}: FAILED ({completed.returncode})\n{completed.stderr}")
            failures += 1
            continue
        result = json.loads(target.read_text(encoding="utf-8"))
        summary[name] = {k: v for k, v in result.items() if k != "memory"}
        values = ", ".join(f"{k}={result.get(k)}" for k in KEYS[mode])
        print(f"{name}: {values}")
    (args.output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
