"""Validate native command bytes, ready ordering and inherited pipe cleanup on Windows."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "experiments/debugging"))
from session_contract import SessionContract


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    report = {"status": "running", "cases": [], "sha256": {
        str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.runner, Path(__file__))}}
    names = ("fixed", "paced", "partial-ready", "missing-ready", "late-ready", "backend-error",
             "stale-ready", "stale-result", "malformed-ready", "duplicate-start", "observe-before-ready",
             "cancel-before-ready", "invalid-path", "unknown-command", "broken-start", "broken-cancel",
             "duplicate-cancel")
    try:
        for name in names:
            directory = (args.output / name).resolve()
            directory.mkdir()
            path = directory / "grant.txt"
            ready = Path(str(path)+".ready")
            read_fd, write_fd = os.pipe()
            writer = os.fdopen(write_fd, "wb", buffering=0)
            try:
                client = SessionContract(args.runner, writer)
            finally:
                writer.close()
            case = {"name": name, "bytes": []}
            report["cases"].append(case)
            def command(text):
                try:
                    return client.command(text)
                except RuntimeError:
                    if not client.records[-1]["snapshot"]["error"]: raise
                    return client.records[-1]["snapshot"]
            def wire(expected):
                received = os.read(read_fd, 4096).decode("ascii")
                case["bytes"].append(received)
                if received != expected: raise RuntimeError("Native wire command differs")
            try:
                duration = 100000000 if name == "paced" else 5000000
                command(f"begin {duration} {int(name == 'paced')}")
                if name == "stale-ready": ready.write_text("active", encoding="utf-8")
                if name == "stale-result": path.write_text("old", encoding="utf-8")
                if name == "broken-start":
                    os.close(read_fd); read_fd = None
                requested = str(path.as_posix()) + (";quit" if name == "invalid-path" else "")
                started = command(f'start-native "{requested}"')
                if started["start_writes"]:
                    expected = f"emulation StartCancellationProbe {duration//1000} @{path.as_posix()}"
                    if name == "paced": expected += " 0 1"
                    wire(expected+"\n")
                if started["error"]:
                    final = started
                elif name == "duplicate-start": final = command(f'start-native "{requested}"')
                elif name == "observe-before-ready": final = command("observe 2010875")
                elif name == "cancel-before-ready": final = command("cancel-native")
                elif name == "unknown-command": final = command("emulation Start")
                elif name in ("missing-ready", "late-ready"):
                    pending = command("poll-ready")
                    if pending["error"] or pending["ready_status"] != "pending": raise RuntimeError("Missing ready accepted")
                    time.sleep(2.01)
                    if name == "late-ready": ready.write_text("active", encoding="utf-8")
                    final = command("poll-ready")
                    if final["ready_status"] != "timeout": raise RuntimeError("Ready deadline was renewed")
                elif name in ("backend-error", "malformed-ready"):
                    ready.write_text("wrong" if name == "malformed-ready" else "active", encoding="utf-8")
                    if name == "backend-error": Path(str(path)+".error").write_text("failed", encoding="utf-8")
                    final = command("poll-ready")
                else:
                    if name == "partial-ready":
                        for partial in ("", "act"):
                            ready.write_text(partial, encoding="utf-8")
                            pending = command("poll-ready")
                            if pending["error"] or pending["ready_status"] != "pending":
                                raise RuntimeError("Incomplete ready accepted")
                    ready.write_text("active", encoding="utf-8")
                    available = command("poll-ready")
                    if available["ready_status"] != "ready" or available["acknowledgements"]:
                        raise RuntimeError("Ready acknowledged CPU or was rejected")
                    command("observe 2010875")
                    if name == "broken-cancel":
                        os.close(read_fd); read_fd = None
                    final = command("cancel-native")
                    if final["cancel_writes"]: wire("emulation CancelCancellationProbe\n")
                    if name == "duplicate-cancel": final = command("cancel-native")
                    elif not final["error"]:
                        path.write_text(f"start=0\nrequested={duration}\nend=2010875\nreason=cancelled\nunused={duration-2010875}\nsinks=2010875\ncancelled=True", encoding="utf-8")
                        ack = command("poll-result")
                        if ack["acknowledgements"] != 1 or ack["commits"]: raise RuntimeError("CPU acknowledgement differs")
                        command("analog 2010875 0.002010875 0")
                        final = command("commit")
                success = name in ("fixed", "paced", "partial-ready")
                if success:
                    if final["error"] or final["commits"] != 1 or final["start_writes"] != 1 or final["cancel_writes"] != 1:
                        raise RuntimeError("Valid command sequence failed")
                elif not final["error"] or final["commits"] or final["acknowledgements"]:
                    raise RuntimeError("Invalid command sequence acknowledged or committed")
                case["final"] = final
            finally:
                try:
                    command("abort")
                finally:
                    client.close()
                    case["records"] = client.records
                    case["exit"] = client.process.returncode
                    if read_fd is not None:
                        try:
                            remaining = os.read(read_fd, 4096)
                            case["pipe_eof"] = remaining == b""
                        finally:
                            os.close(read_fd)
            if case["exit"] != 0 or case.get("pipe_eof") is False:
                raise RuntimeError("Native pipe teardown failed or extra bytes were sent")
        report["status"] = "passed"
    except (ValueError, RuntimeError, OSError) as error:
        report.update(status="failed", error=str(error))
    (args.output/"summary.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(report["status"],len(report["cases"]),"cases",report.get("error",""))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
