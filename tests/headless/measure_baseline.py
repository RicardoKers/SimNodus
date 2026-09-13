"""Measure the unchanged bounded real-engine composition; see BASELINE.md."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
import math
import platform
from pathlib import Path
import statistics
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


class Entry(c.Structure):
    _fields_ = [("size", w.DWORD), ("usage", w.DWORD), ("pid", w.DWORD),
                ("heap", c.c_size_t), ("module", w.DWORD), ("threads", w.DWORD),
                ("parent", w.DWORD), ("priority", w.LONG), ("flags", w.DWORD),
                ("name", w.WCHAR * 260)]


class Memory(c.Structure):
    _fields_ = [("cb", w.DWORD), ("faults", w.DWORD)] + [
        (name, c.c_size_t) for name in ("peak_ws", "ws", "peak_paged", "paged",
                                      "peak_nonpaged", "nonpaged", "pagefile",
                                      "peak_pagefile", "private")]


def sampler():
    kernel = c.WinDLL("kernel32", use_last_error=True)
    psapi = c.WinDLL("psapi", use_last_error=True)
    kernel.CreateToolhelp32Snapshot.argtypes = [w.DWORD, w.DWORD]
    kernel.CreateToolhelp32Snapshot.restype = w.HANDLE
    kernel.Process32FirstW.argtypes = kernel.Process32NextW.argtypes = [w.HANDLE, c.POINTER(Entry)]
    kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
    kernel.OpenProcess.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    psapi.GetProcessMemoryInfo.argtypes = [w.HANDLE, c.POINTER(Memory), w.DWORD]

    def sample(root_pid):
        snapshot = kernel.CreateToolhelp32Snapshot(2, 0)
        if snapshot == c.c_void_p(-1).value:
            raise c.WinError(c.get_last_error())
        entries = []
        entry = Entry(size=c.sizeof(Entry))
        try:
            more = kernel.Process32FirstW(snapshot, c.byref(entry))
            while more:
                entries.append((entry.pid, entry.parent, entry.name))
                more = kernel.Process32NextW(snapshot, c.byref(entry))
        finally:
            kernel.CloseHandle(snapshot)
        owned = {root_pid}
        while True:
            expanded = owned | {pid for pid, parent, _ in entries if parent in owned}
            if expanded == owned:
                break
            owned = expanded
        rows, errors = [], []
        for pid, _, name in entries:
            if pid not in owned:
                continue
            handle = kernel.OpenProcess(0x410, False, pid)
            if not handle:
                errors.append([pid, c.get_last_error()])
                continue
            try:
                memory = Memory(cb=c.sizeof(Memory))
                if psapi.GetProcessMemoryInfo(handle, c.byref(memory), memory.cb):
                    rows.append(dict(pid=pid, name=name, working_set=memory.ws,
                                     private_bytes=memory.private))
                else:
                    errors.append([pid, c.get_last_error()])
            finally:
                kernel.CloseHandle(handle)
        return dict(processes=rows, errors=errors)
    return sample


def describe(values):
    return dict(values=values, minimum=min(values), median=statistics.median(values),
                maximum=max(values), sample_stdev=statistics.stdev(values) if len(values) > 1 else None)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    opts = parser.parse_args()
    out = opts.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    reference_path = ROOT / "build/sn017/application-session-recovery-01/summary.json"
    reference = json.loads(reference_path.read_text())
    frozen = dict(reference["sha256"])
    for path, digest in frozen.items():
        if sha(path) != digest:
            raise RuntimeError("Reference input changed: " + path)
    for folder in ("tests/experiments/debugging", "tests/experiments/adc",
                   "build/sn019/generated", "build/sn016/firmware",
                   "tools/backend_probe/initialization"):
        for path in (ROOT / folder).rglob("*"):
            if path.is_file() and "__pycache__" not in path.parts:
                frozen[str(path)] = sha(path)
    for path in (Path(__file__), ROOT / "tests/headless/BASELINE.md", reference_path):
        frozen[str(path)] = sha(path)
    protected = {str(p): sha(p) for p in (ROOT / "docs/experiments/evidence").glob("*.json")}
    command = [sys.executable, "tests/experiments/debugging/joint_persistent.py",
        "--renode", "build/sn016/source-notification", "--ide", "C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE",
        "--native", "build/sn016/persistent-native/Debug",
        "--analog-runner", "build/sn017/native/Debug/simnodus_rc_fixture.exe",
        "--session-runner", "build/sn017/native/Debug/simnodus_session_contract.exe",
        "--process-runner", "build/sn017/native/Debug/simnodus_fixture_process.exe"]
    command += ["--" + flag for flag in ("native-results native-commands native-debug-results native-grants "
        "native-analog native-analog-results native-exchange native-adc native-adc-process native-rc "
        "native-readback native-readback-mi native-readback-request native-readback-time native-inspection "
        "native-inspection-request native-inspection-interval native-inspection-time steps lifecycle pause guarded").split()]
    commands = [command + ["--output", str(out / f"batch-{i:02}")] for i in range(1, 4)]
    manifest = dict(schema=1, utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        python=sys.version, os=platform.platform(), processor=platform.processor(),
        host_details=subprocess.check_output(["powershell", "-NoProfile", "-Command",
            "Get-CimInstance Win32_Processor | Select-Object Name,NumberOfLogicalProcessors | ConvertTo-Json; powercfg /getactivescheme"], text=True),
        commands=commands, sha256=frozen, preserved_evidence_sha256=protected,
        repetitions="3 batches, 6 fresh sessions each; no warm-up", sample_interval_s=0.1,
        versions="Patched Renode 1.16.1; ngspice 47; GDB 15.2.90.20241229; MSVC 19.51.36246.0 Debug")
    write(out / "manifest.json", manifest)
    sample = sampler()
    results = dict(status="passed", complete_SN018=False, batches=[])
    sys.path.insert(0, str(ROOT / "tests/experiments/debugging"))
    from joint_persistent import compare_fixed
    for index, cmd in enumerate(commands, 1):
        with (out / f"batch-{index:02}.log").open("w", encoding="utf-8") as log:
            started = time.perf_counter_ns()
            process = subprocess.Popen(cmd, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT,
                                       creationflags=subprocess.CREATE_NO_WINDOW)
            samples = []
            while process.poll() is None:
                row = sample(process.pid)
                row["elapsed_ns"] = time.perf_counter_ns() - started
                samples.append(row)
                time.sleep(0.1)
            elapsed = time.perf_counter_ns() - started
        write(out / f"memory-{index:02}.json", samples)
        batch = dict(index=index, exit_code=process.returncode, wall_ns=elapsed,
            peak_sampled_working_set_bytes=max(sum(p["working_set"] for p in s["processes"]) for s in samples),
            peak_sampled_private_bytes=max(sum(p["private_bytes"] for p in s["processes"]) for s in samples),
            sample_count=len(samples), query_errors=sum(len(s["errors"]) for s in samples), sessions=[])
        results["batches"].append(batch)
        report_path = out / f"batch-{index:02}/summary.json"
        batch["report_sha256"] = sha(report_path) if report_path.exists() else None
        if process.returncode != 0:
            results["status"] = "failed"
            write(out / "summary.json", results)
            break
        report = json.loads(report_path.read_text())
        for run in report["runs"]:
            for session in (run, run["recreated_session"]):
                compare_fixed(session["checkpoints"], reference["runs"][0]["checkpoints"], True)
                points = session["checkpoints"] + [session["async_pause"]]
                errors = [abs(p["analog"]["output_v"] - (3.3 * (1 - math.exp(
                    -(p["joint_time_ns"] - 2011375) * 1e-9 / 0.001)) if p["joint_time_ns"] > 2011375 else 0)) for p in points]
                time_errors = [abs(p["analog"]["actual_s"] - p["joint_time_ns"] * 1e-9) for p in points]
                assert max(errors) <= 1e-5 and max(time_errors) <= 1e-12
                assert session["checkpoints"][-1]["mailbox"][4] == 3541
                batch["sessions"].append(dict(pause_wall_ns=session["async_pause"]["acknowledgement_wall_ns"],
                    pause_virtual_ns=session["async_pause"]["joint_time_ns"],
                    final_virtual_ns=4027000, adc_code=3541, max_rc_error_v=max(errors),
                    max_endpoint_error_s=max(time_errors), fixed_checkpoints=9))
        write(out / "summary.json", results)
        print(f"Batch {index}: passed, {elapsed / 1e9:.3f} wall seconds", flush=True)
    results["inputs_unchanged"] = all(sha(p) == h for p, h in frozen.items())
    results["prior_evidence_unchanged"] = all(sha(p) == h for p, h in protected.items())
    sessions = [s for b in results["batches"] for s in b["sessions"]]
    if sessions:
        results["pause_wall_ns"] = describe([s["pause_wall_ns"] for s in sessions])
        results["batch_wall_ns"] = describe([b["wall_ns"] for b in results["batches"]])
    if not results["inputs_unchanged"] or not results["prior_evidence_unchanged"]:
        results["status"] = "failed"
    write(out / "summary.json", results)
    return 0 if results["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
