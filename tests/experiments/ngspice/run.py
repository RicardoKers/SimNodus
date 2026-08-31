"""Execute isolated E-01 processes and compare real samples to analytical references."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
FIXTURES = Path(__file__).resolve().parent
VOLTAGE_TOLERANCE = 0.0165
TIME_TOLERANCE = 1e-12


def rows(path: Path) -> list[dict[str, float]]:
    with path.open(encoding="utf-8", newline="") as stream:
        return [{key: float(value) for key, value in row.items()} for row in csv.DictReader(stream)]


def check(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def response(time: float, external: bool) -> float:
    def step(delay: float) -> float:
        return -math.expm1(-(time - delay) / 0.001) if time >= delay else 0.0
    return 3.3 * (step(0.001) - step(0.003)) if external else 3.3 * step(0.0)


def analyze(path: Path, external: bool, final: float | None) -> dict:
    data = rows(path)
    check(len(data) > 100, f"Insufficient samples: {path.name}")
    check(all(math.isfinite(v) for row in data for v in row.values()), "Nonfinite vector data")
    check(all(b["time_s"] > a["time_s"] for a, b in zip(data, data[1:])), "Nonmonotonic final vector")
    if final is not None:
        check(abs(data[-1]["time_s"] - final) <= TIME_TOLERANCE, "Incorrect final time")
    error = [(abs(row["output_v"] - response(row["time_s"], external)), row["time_s"]) for row in data]
    gated = [value for value, time in error if not external or all(abs(time-edge) > 2e-6 for edge in (0.001, 0.003))]
    check(bool(gated) and max(gated) <= VOLTAGE_TOLERANCE, "RC voltage error exceeds predeclared tolerance")
    return {"samples": len(data), "first_time_s": data[0]["time_s"], "final_time_s": data[-1]["time_s"],
            "max_error_v": max(gated), "max_error_including_edges_v": max(v for v, _ in error)}


def compare(a: Path, b: Path) -> None:
    left, right = rows(a), rows(b)
    check(len(left) == len(right), "Reset/repeat sample count changed")
    for x, y in zip(left, right):
        check(abs(x["time_s"] - y["time_s"]) <= TIME_TOLERANCE, "Reset/repeat timing changed")
        check(abs(x["output_v"] - y["output_v"]) <= 1e-9, "Reset/repeat voltage changed")
        check(abs(x["input_v"] - y["input_v"]) <= 1e-9, "Reset/repeat input voltage changed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, default=ROOT / "build/e01/Debug/e01.exe")
    parser.add_argument("--deps", type=Path, default=ROOT / "build/deps")
    parser.add_argument("--output", type=Path, default=ROOT / "build/e01-results")
    parser.add_argument("--compare-with", type=Path, help="Check complete final vectors against an earlier run")
    cases = ["rc", "external", "breakpoint", "pause", "reset", "invalid", "retry", "background"]
    parser.add_argument("--cases", nargs="+", choices=cases, default=cases)
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    executable, deps = args.executable.resolve(), args.deps.resolve()
    subprocess.run([sys.executable, str(ROOT / "tools/check_backend_assets.py"), "--root", str(deps), "--ngspice-only"], check=True)
    report = {"voltage_tolerance_v": VOLTAGE_TOLERANCE, "time_tolerance_s": TIME_TOLERANCE, "cases": {}}
    failed = False
    for mode in args.cases:
        directory = output / mode
        directory.mkdir(exist_ok=True)
        argv = [str(executable), str(deps / "ngspice/Spice64_dll/dll-vs/ngspice.dll"),
                str(deps / "ngspice-console/Spice64/bin"), str(ROOT / "tools/backend_probe/initialization"),
                str(FIXTURES), mode, str(directory)]
        result = {}
        try:
            with (directory / "engine.log").open("w", encoding="utf-8") as log:
                process = subprocess.run(argv, cwd=directory, stdout=log, stderr=subprocess.STDOUT, timeout=30)
            result["process_exit"] = process.returncode
            check(process.returncode == 0, f"Native process failed with {process.returncode}; inspect engine.log")
            result["metrics"] = json.loads((directory / "metrics.json").read_text(encoding="utf-8"))
            metrics = result["metrics"]
            check(metrics["idle_before_quit"] == 1 and metrics["quit_requested"] == 1 and metrics["exit_status"] == 0,
                  "Normal idle shutdown was not confirmed")
            external = mode in ("external", "retry")
            result["first"] = analyze(directory / "first.csv", external, None if mode in ("pause", "background") else (0.006 if external else 0.005))
            if mode == "pause":
                reached = result["first"]["final_time_s"]
                check(0.002 < reached <= 0.002001 + TIME_TOLERANCE, "Pause overshoot exceeds declared bound")
                result["resumed"] = analyze(directory / "resumed.csv", False, 0.005)
            if mode == "background":
                result["resumed"] = analyze(directory / "resumed.csv", False, 0.05)
                m = result["metrics"]
                check(m["background_flags"] == [0, 1, 0, 1], "Unexpected background callback lifecycle")
                check(m["data_on_main_thread"] == 0, "Background samples did not use the worker")
                check(m["pause_request_observed_time_s"] <= m["actual_pause_time_s"] < 0.05,
                      "Pause was not observed before completion")
            if mode == "reset":
                for name in ("circuit-reset.csv", "full-reset.csv"):
                    result[name] = analyze(directory / name, False, 0.005)
                    compare(directory / "first.csv", directory / name)
            trials = rows(directory / "source.csv")
            sync = rows(directory / "sync.csv")
            result["source_queries"] = len(trials)
            result["source_time_reversals"] = sum(b["time_s"] < a["time_s"] for a, b in zip(trials, trials[1:]))
            result["sync_calls"] = len(sync)
            result["sync_redo_calls"] = sum(row["redo"] != 0 for row in sync)
            result["requested_retries"] = sum(row["request_retry"] for row in sync)
            result["sync_locations"] = sorted({int(row["location"]) for row in sync})
            if external:
                check(len(trials) > 0 and all(row["value_v"] in (0, 3.3) for row in trials), "External source was not exercised")
            if mode == "retry":
                check(result["requested_retries"] == 1, "Single solver retry was not exercised")
                trial_time = next(row["time_s"] for row in sync if row["request_retry"] == 1)
                check(not any(row["time_s"] == trial_time for row in rows(directory / "first.csv")),
                      "Rejected trial appeared in the accepted output")
                result["rejected_trial_time_s"] = trial_time
            if mode == "invalid":
                metrics = result["metrics"]
                check(metrics["invalid_load"] != 0 or metrics["invalid_exit_callback"] or metrics["invalid_diagnostics"] > 0,
                      "Invalid netlist was not diagnosed")
                check(metrics["invalid_vector_present"] == 0 and metrics["invalid_callback_samples"] == 0,
                      "Invalid netlist generated transient samples")
            callbacks = rows(directory / "callbacks.csv")
            expected = rows(directory / ("resumed.csv" if mode in ("pause", "background") else "first.csv"))
            if mode == "reset":
                expected += rows(directory / "circuit-reset.csv") + rows(directory / "full-reset.csv")
            check(len(callbacks) == len(expected), "Missing or duplicated callback samples")
            check(all(all(a[key] == b[key] for key in ("time_s", "input_v", "output_v"))
                      for a, b in zip(callbacks, expected)), "Callback data differ from accepted vectors")
            result["callback_samples"] = len(callbacks)
            result["callbacks_match_vectors"] = True
            check(result["metrics"]["callback_fault"] == 0, "Callback failed")
            if mode != "background":
                check(result["metrics"]["data_on_main_thread"] == 1, "Unexpected callback execution context")
            if mode == "breakpoint":
                result["nearest_breakpoint_sample_s"] = min(abs(row["time_s"] - 0.002) for row in expected)
                check(result["nearest_breakpoint_sample_s"] <= TIME_TOLERANCE, "Integration boundary was missed")
            if args.compare_with:
                final_file = "resumed.csv" if mode in ("pause", "background") else "first.csv"
                # An asynchronous pause can change the sampling grid; its analytical gate still applies.
                if mode != "background":
                    compare(directory / final_file, args.compare_with / mode / final_file)
                    result["matches_previous_run"] = True
            result["status"] = "passed"
        except (ValueError, OSError, subprocess.TimeoutExpired) as error:
            result["status"] = "failed"
            result["error"] = str(error)
            failed = True
        report["cases"][mode] = result
        print(mode, result["status"], result.get("error", result.get("first", "")))
    paths = [executable, *FIXTURES.glob("*.cir"), FIXTURES / "e01.cpp", FIXTURES / "CMakeLists.txt", Path(__file__),
             ROOT / "tools/backend_probe/ngspice_host.hpp", ROOT / "tools/backend_probe/initialization/spinit",
             ROOT / "tools/backend-baseline.json"]
    report["sha256"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    report["status"] = "failed" if failed else "passed"
    (output / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
