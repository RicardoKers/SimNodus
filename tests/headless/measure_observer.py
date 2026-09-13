"""Paired real-engine memory-observer control; see OBSERVER_CONTROL.md."""
import argparse
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys
import time
import winreg

from measure_baseline import ROOT, describe, sampler, sha, write


def verify(hashes):
    changed = [p for p, h in hashes.items() if not Path(p).is_file() or sha(p) != h]
    if changed:
        raise RuntimeError("Changed or missing inputs: " + repr(changed))


def session_metrics(report, reference):
    sys.path.insert(0, str(ROOT / "tests/experiments/debugging"))
    from joint_persistent import compare_fixed
    if report["status"] != "passed" or len(report["runs"]) != 3:
        raise RuntimeError("Incomplete or failed engine batch")
    rows = []
    for run in report["runs"]:
        for session in (run, run["recreated_session"]):
            compare_fixed(session["checkpoints"], reference["runs"][0]["checkpoints"], True)
            points = session["checkpoints"] + [session["async_pause"]]
            voltage_error = max(abs(p["analog"]["output_v"] - (3.3 * (1 - math.exp(
                -(p["joint_time_ns"] - 2011375) * 1e-9 / .001))
                if p["joint_time_ns"] > 2011375 else 0)) for p in points)
            endpoint_error = max(abs(p["analog"]["actual_s"] - p["joint_time_ns"] * 1e-9) for p in points)
            pause = session["async_pause"]["acknowledgement_wall_ns"]
            final = session["checkpoints"][-1]
            if (voltage_error > 1e-5 or endpoint_error > 1e-12 or pause > 2e9
                    or final["mailbox"][4] != 3541 or final["joint_time_ns"] != 4027000):
                raise RuntimeError("Unchanged numeric acceptance failed")
            rows.append(dict(pause_wall_ns=pause, pause_virtual_ns=session["async_pause"]["joint_time_ns"],
                final_virtual_ns=final["joint_time_ns"], adc_code=final["mailbox"][4],
                max_rc_error_v=voltage_error, max_endpoint_error_s=endpoint_error, fixed_checkpoints=9))
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    baseline_path = ROOT / "build/sn018/baseline-01/manifest.json"
    old = json.loads(baseline_path.read_text())
    frozen = dict(old["sha256"])
    historical = json.loads((ROOT / "docs/experiments/evidence/SN-018-baseline-summary.json").read_text())
    preserved = dict(historical["raw_files"])
    preserved.update({str(p): sha(p) for p in (ROOT / "docs/experiments/evidence").glob("*.json")})
    verify(frozen)
    verify(preserved)
    for p in (Path(__file__), Path(__file__).with_name("OBSERVER_CONTROL.md"), baseline_path):
        frozen[str(p)] = sha(p)
    modes = ["U", "S", "S", "U", "U", "S"]
    command = old["commands"][0]
    if command[-2] != "--output":
        raise RuntimeError("Unexpected baseline command layout")
    commands = [command[:-1] + [str(out / f"batch-{i:02}-{mode}")]
                for i, mode in enumerate(modes, 1)]
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"HARDWARE\DESCRIPTION\System\CentralProcessor\0") as key:
        cpu = winreg.QueryValueEx(key, "ProcessorNameString")[0]
    manifest = dict(task="SN-018", utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        commands=commands, modes=modes, sha256=frozen, preserved_sha256=preserved,
        versions=old["versions"], python=sys.version, os=platform.platform(), cpu=cpu,
        logical_cpus=os.cpu_count(), power_scheme=subprocess.check_output(["powercfg", "/getactivescheme"], text=True),
        contract="Three adjacent pairs U,S / S,U / U,S; six fresh sessions per batch; no retries")
    write(out / "manifest.json", manifest)
    reference = json.loads((ROOT / "build/sn017/application-session-recovery-01/summary.json").read_text())
    sample = sampler()
    result = dict(task="SN-018", status="passed", complete_SN018=False, manifest=manifest, batches=[])
    for index, (mode, cmd) in enumerate(zip(modes, commands), 1):
        batch = dict(index=index, mode=mode, sessions=[], memory=None)
        result["batches"].append(batch)
        samples, sample_durations = [], []
        try:
            with (out / f"batch-{index:02}.log").open("w", encoding="utf-8") as log:
                started = time.perf_counter_ns()
                process = subprocess.Popen(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                           creationflags=subprocess.CREATE_NO_WINDOW)
                while process.poll() is None:
                    if mode == "S":
                        sample_started = time.perf_counter_ns()
                        row = sample(process.pid)
                        sample_durations.append(time.perf_counter_ns() - sample_started)
                        row["elapsed_ns"] = time.perf_counter_ns() - started
                        samples.append(row)
                    time.sleep(.1)
                batch["wall_ns"] = time.perf_counter_ns() - started
                batch["exit_code"] = process.returncode
            if mode == "S":
                write(out / f"memory-{index:02}.json", samples)
                batch["memory"] = dict(samples=len(samples), query_errors=sum(len(s["errors"]) for s in samples),
                    sampler_call_wall_ns=describe(sample_durations),
                    sampler_total_wall_ns=sum(sample_durations),
                    peak_sampled_working_set_bytes=max(sum(p["working_set"] for p in s["processes"]) for s in samples),
                    peak_sampled_private_bytes=max(sum(p["private_bytes"] for p in s["processes"]) for s in samples))
            if process.returncode:
                raise RuntimeError("Engine harness failed; no retry")
            report_path = Path(cmd[-1]) / "summary.json"
            batch["report_sha256"] = sha(report_path)
            batch["sessions"] = session_metrics(json.loads(report_path.read_text()), reference)
        except (OSError, ValueError, RuntimeError, KeyError) as error:
            batch["error"] = str(error)
            result["status"] = "failed"
        write(out / "summary.json", result)
        print(f"Batch {index} {mode}: {result['status']} ({batch.get('wall_ns', 0) / 1e9:.3f} s)", flush=True)
        if result["status"] != "passed":
            break
    try:
        verify(frozen)
        verify(preserved)
        result["inputs_and_prior_evidence_unchanged"] = True
    except RuntimeError as error:
        result.update(status="failed", preservation_error=str(error))
    if result["status"] == "passed":
        result["conditions"] = {}
        for mode in ("U", "S"):
            batches = [b for b in result["batches"] if b["mode"] == mode]
            result["conditions"][mode] = dict(batch_wall_ns=describe([b["wall_ns"] for b in batches]),
                pause_wall_ns=describe([s["pause_wall_ns"] for b in batches for s in b["sessions"]]))
        result["pairs"] = []
        for pair in range(3):
            batches = {b["mode"]: b for b in result["batches"][pair*2:pair*2+2]}
            u, s = batches["U"], batches["S"]
            result["pairs"].append(dict(pair=pair+1, sampled_minus_unsampled_wall_ns=s["wall_ns"]-u["wall_ns"],
                relative_wall_percent=100*(s["wall_ns"]/u["wall_ns"]-1),
                median_pause_difference_ns=statistics.median(x["pause_wall_ns"] for x in s["sessions"])
                    - statistics.median(x["pause_wall_ns"] for x in u["sessions"])))
    result["raw_files"] = {str(p): sha(p) for p in out.rglob("*") if p.is_file() and p != out / "summary.json"}
    write(out / "summary.json", result)
    return 0 if result["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
