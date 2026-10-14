"""Exercise native cancellation ingress with real files and Windows sharing locks."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "experiments/debugging"))
from session_contract import SessionContract


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    reference = args.reference.read_text(encoding="utf-8")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                                   ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.CloseHandle.restype = wintypes.BOOL
    report = {"status": "running", "cases": [], "sha256": {
        str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.runner, args.reference, Path(__file__))}}
    cases = ("atomic-publication", "locked-then-released", "lock-past-deadline", "missing-timeout",
             "late-publication", "malformed", "duplicate-field", "unknown-field", "oversize",
             "backend-error", "stale-result", "stale-accounting", "duplicate-consumption",
             "rearm-deadline", "unicode path")
    try:
        for name in cases:
            directory = (args.output / name).resolve()
            directory.mkdir()
            path = directory / ("résultat.txt" if name == "unicode path" else "grant.txt")
            client = SessionContract(args.runner)
            handle = None
            case = {"name": name}
            report["cases"].append(case)
            def command(text):
                try:
                    return client.command(text)
                except RuntimeError:
                    if not client.records or not client.records[-1]["snapshot"]["error"]:
                        raise
                    return client.records[-1]["snapshot"]
            def poll():
                return command("poll-result")
            def publish(text=reference):
                path.write_text(text, encoding="utf-8")
            try:
                command("begin 5000000 0")
                command("observe 2010875")
                if name == "stale-result": publish()
                budget = 80 if name in ("lock-past-deadline", "missing-timeout", "late-publication") else 1900
                armed = command(f'expect-result "{path.as_posix()}" {budget}')
                if name == "stale-result":
                    final = armed
                    if final["result_status"] != "stale": raise RuntimeError("Stale result accepted")
                elif name in ("locked-then-released", "lock-past-deadline"):
                    publish()
                    handle = kernel.CreateFileW(str(path), 0x80000000, 0, None, 3, 0x80, None)
                    if handle == ctypes.c_void_p(-1).value: raise ctypes.WinError(ctypes.get_last_error())
                    first = poll()
                    if first["result_status"] != "pending" or first["read_retries"] < 1 or first["acknowledgements"]:
                        raise RuntimeError("Denied read acknowledged or did not retry")
                    if name == "lock-past-deadline": time.sleep(.12)
                    kernel.CloseHandle(handle)
                    handle = None
                    final = poll()
                    if final["result_status"] != ("ready" if name == "locked-then-released" else "timeout"):
                        raise RuntimeError("Lock release reset the deadline")
                elif name in ("missing-timeout", "late-publication"):
                    first = poll()
                    if first["acknowledgements"] or first["result_status"] != "pending":
                        raise RuntimeError("Missing result acknowledged")
                    time.sleep(.12)
                    if name == "late-publication": publish()
                    final = poll()
                    if final["result_status"] != "timeout": raise RuntimeError("Deadline was extended")
                elif name == "atomic-publication":
                    temporary = Path(str(path)+".tmp")
                    temporary.write_text(reference, encoding="utf-8")
                    first = poll()
                    if first["result_status"] != "pending" or first["acknowledgements"]:
                        raise RuntimeError("Temporary file acknowledged")
                    temporary.replace(path)
                    final = poll()
                elif name == "rearm-deadline":
                    final = command(f'expect-result "{path.as_posix()}" 1900')
                    if not final["error"]: raise RuntimeError("Active deadline was rearmed")
                else:
                    text = reference
                    if name == "malformed": text = "start=not-a-number"
                    if name == "duplicate-field": text += "\nstart=0"
                    if name == "unknown-field": text += "\nunknown=0"
                    if name == "oversize": text = "a" * 4097
                    if name == "stale-accounting": text = text.replace("start=0", "start=1")
                    publish(text)
                    if name == "backend-error": Path(str(path)+".error").write_text("backend failed", encoding="utf-8")
                    final = poll()
                    if name == "duplicate-consumption":
                        if final["acknowledgements"] != 1: raise RuntimeError("Valid result not acknowledged")
                        final = poll()
                success = name in ("atomic-publication", "locked-then-released", "unicode path")
                if success:
                    if final["error"] or final["acknowledgements"] != 1 or final["commits"]:
                        raise RuntimeError("Valid ingress contract failed")
                elif not final["error"] or final["acknowledgements"] != int(name == "duplicate-consumption") or final["commits"]:
                    raise RuntimeError("Invalid result changed accounting or committed")
                case["final"] = final
            finally:
                if handle is not None: kernel.CloseHandle(handle)
                try:
                    command("abort")
                finally:
                    client.close()
                    case["records"] = client.records
                    case["exit"] = client.process.returncode
            if case["exit"] != 0: raise RuntimeError("Ingress helper survived or failed teardown")
        report["status"] = "passed"
    except (ValueError, RuntimeError, OSError) as error:
        report.update(status="failed", error=str(error))
    (args.output/"summary.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(report["status"],len(report["cases"]),"cases",report.get("error",""))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
