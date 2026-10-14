"""Native Windows creation, argv, tree cleanup, owner-loss and startup-failure tests."""
import argparse
import ctypes
from ctypes import wintypes
import json
import hashlib
import os
from pathlib import Path
import queue
import subprocess
import sys
import threading
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "experiments/debugging"))
from native_process import NativeProcess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    fixture = Path(__file__).with_name("process_fixture.py").resolve()
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    report = {"status": "running", "cases": [], "sha256": {
        str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.runner, fixture, Path(__file__))}}
    try:
        for name in ("argv", "tree", "forced", "owner-loss", "ignore", "exit7", "missing-executable", "stale-record", "fresh-recovery"):
            directory = (args.output/name).resolve()
            directory.mkdir()
            case = {"name": name}
            report["cases"].append(case)
            record = directory / "child.pid"
            values = ["", "with spaces", 'embedded"quote', "trailing\\", 'slash\\"quote', "café"]
            command = [sys.executable, "-X", "utf8", str(fixture), name, *values]
            if name == "missing-executable": command[0] = str(directory/"missing.exe")
            if name == "stale-record": record.write_text("stale",encoding="utf-8")
            environment = os.environ.copy()
            environment["SN_FIXTURE_MARKER"] = "owned-test-value"
            process = None
            handles = []
            with (directory/"stderr.txt").open("w",encoding="utf-8") as errors:
                try:
                    try:
                        process = NativeProcess(args.runner, record, command, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=errors, text=True, encoding="utf-8", cwd=directory, env=environment,
                            creationflags=subprocess.CREATE_NO_WINDOW)
                    except (ValueError, RuntimeError) as error:
                        if name not in ("missing-executable", "stale-record"): raise
                        case["expected_startup_failure"] = str(error)
                        continue
                    if name in ("missing-executable", "stale-record"): raise RuntimeError("Invalid startup succeeded")
                    if name == "exit7":
                        process.communicate(timeout=5)
                        if process.returncode != 7: raise RuntimeError("Child exit code changed")
                    else:
                        pending = queue.Queue()
                        reader = threading.Thread(target=lambda: pending.put(process.stdout.readline()), daemon=True)
                        reader.start()
                        info = json.loads(pending.get(timeout=5))
                        reader.join(timeout=1)
                        case["child"] = info
                        if info["pid"] != process.pid or info["args"] != values or info["marker"] != "owned-test-value" or Path(info["cwd"]) != directory:
                            raise RuntimeError("Native child argv/environment/cwd changed")
                        for pid in (process.pid, info["descendant"]):
                            if pid:
                                handle = kernel.OpenProcess(0x00100000, False, pid)
                                if not handle: raise ctypes.WinError(ctypes.get_last_error())
                                handles.append(handle)
                        if name == "owner-loss":
                            process.owner.kill()
                            process.wait(timeout=5)
                        elif name == "forced": process.kill()
                        elif name == "ignore":
                            try:
                                process.communicate("quit\n",timeout=.2)
                                raise RuntimeError("Ignored quit unexpectedly completed")
                            except subprocess.TimeoutExpired:
                                process.kill()
                        else:
                            process.communicate("quit\n",timeout=5)
                            if process.returncode != 0: raise RuntimeError("Graceful root exit changed")
                        for handle in handles:
                            if kernel.WaitForSingleObject(handle, 5000) != 0:
                                raise RuntimeError("Owned root or descendant survived teardown")
                        case["tree_stopped"] = True
                    case["lifecycle"] = process.evidence()
                    if name != "owner-loss":
                        native = case["lifecycle"].get("native_exit")
                        if not native or native["exit"] != process.returncode:
                            raise RuntimeError("Missing native terminal evidence")
                        if native["forced"] != (name in ("forced", "ignore")):
                            raise RuntimeError("Forced and graceful outcomes conflated")
                    elif "native_exit" in case["lifecycle"]:
                        raise RuntimeError("Owner loss fabricated normal completion")
                finally:
                    if process:
                        if process.poll() is None: process.kill()
                        for pipe in (process.stdin,process.stdout):
                            if pipe:
                                try: pipe.close()
                                except OSError: pass
                    for handle in handles: kernel.CloseHandle(handle)
        report["status"] = "passed"
    except (ValueError, RuntimeError, OSError, queue.Empty, subprocess.SubprocessError) as error:
        report.update(status="failed",error=str(error))
    (args.output/"summary.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(report["status"],len(report["cases"]),"cases",report.get("error",""))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
