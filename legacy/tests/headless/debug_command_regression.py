"""Exercise native debug command ordering using real Windows pipes, without engines."""
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
    names = ("valid", "sample-before-ready", "duplicate-sample", "cancel-pending-sample",
             "invalid-sample-path", "stale-sample", "sample-error-file", "broken-sample",
             "notify-before-ack", "notify-before-analog", "duplicate-notify", "invalid-notify-path",
             "stale-notify", "notify-error-file", "broken-notify", "commit-pending-notify",
             "unsolicited-confirmation", "duplicate-confirmation", "late-notify", "late-confirmation",
             "sample-after-ack")
    try:
        for name in names:
            directory = (args.output/name).resolve()
            directory.mkdir()
            grant, progress, notification = (directory/file for file in ("grant.txt", "progress.txt", "notification.txt"))
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
                    if not client.records[-1]["snapshot"]["error"]:
                        raise
                    return client.records[-1]["snapshot"]
            def wire(expected):
                actual = os.read(read_fd, 4096).decode("ascii")
                case["bytes"].append(actual)
                if actual != expected:
                    raise RuntimeError("Unexpected monitor command")
            try:
                command("begin 100000000 1")
                command(f'start-native "{grant.as_posix()}"')
                wire(f"emulation StartCancellationProbe 100000 @{grant.as_posix()} 0 1\n")
                if name == "sample-before-ready":
                    final = command(f'sample-native "{progress.as_posix()}"')
                else:
                    Path(str(grant)+".ready").write_text("active", encoding="utf-8")
                    command("poll-ready")
                    if name == "stale-sample": progress.write_text("1", encoding="utf-8")
                    if name == "sample-error-file": Path(str(progress)+".error").write_text("error", encoding="utf-8")
                    if name == "broken-sample": os.close(read_fd); read_fd = None
                    sample_path = progress.as_posix() + (";quit" if name == "invalid-sample-path" else "")
                    final = command(f'sample-native "{sample_path}"')
                    if final["sample_writes"]:
                        wire(f"emulation SampleCancellationProbe @{progress.as_posix()}\n")
                    if not final["error"]:
                        if name == "duplicate-sample": final = command(f'sample-native "{progress.as_posix()}"')
                        elif name == "cancel-pending-sample": final = command("cancel-native")
                        else:
                            command("observe 2010875")
                            if name == "notify-before-ack":
                                final = command(f'notify-native "{notification.as_posix()}"')
                            else:
                                command("cancel-native")
                                wire("emulation CancelCancellationProbe\n")
                                grant.write_text("start=0\nrequested=100000000\nend=2010875\nreason=cancelled\nunused=97989125\nsinks=2010875\ncancelled=True", encoding="utf-8")
                                ack = command("poll-result")
                                if ack["error"] or ack["acknowledgements"] != 1 or ack["commits"]:
                                    raise RuntimeError("Cancellation setup failed")
                                if name == "notify-before-analog": final = command(f'notify-native "{notification.as_posix()}"')
                                elif name == "sample-after-ack": final = command(f'sample-native "{progress.as_posix()}"')
                                else:
                                    command("analog 2010875 0.002010875 0")
                                    if name == "unsolicited-confirmation": final = command("notified")
                                    else:
                                        if name == "stale-notify": notification.write_text("notified", encoding="utf-8")
                                        if name == "notify-error-file": Path(str(notification)+".error").write_text("error", encoding="utf-8")
                                        if name == "broken-notify": os.close(read_fd); read_fd = None
                                        if name == "late-notify": time.sleep(2.01)
                                        notify_path = notification.as_posix() + (";quit" if name == "invalid-notify-path" else "")
                                        final = command(f'notify-native "{notify_path}"')
                                        if final["notify_writes"]:
                                            wire(f"emulation NotifyCancellationProbe 2010875 @{notification.as_posix()}\n")
                                        if not final["error"]:
                                            if name == "duplicate-notify": final = command(f'notify-native "{notification.as_posix()}"')
                                            elif name == "commit-pending-notify": final = command("commit")
                                            else:
                                                if name == "late-confirmation": time.sleep(2.01)
                                                final = command("notified")
                                                if name == "duplicate-confirmation": final = command("notified")
                                                elif not final["error"]: final = command("commit")
                if name == "valid":
                    if final["error"] or final["commits"] != 1 or final["notify_writes"] != 1:
                        raise RuntimeError("Valid sequence rejected")
                elif not final["error"] or final["commits"]:
                    raise RuntimeError("Invalid sequence accepted or committed")
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
                            case["pipe_eof"] = os.read(read_fd, 4096) == b""
                        finally:
                            os.close(read_fd)
            if case["exit"] != 0 or case.get("pipe_eof") is False:
                raise RuntimeError("Helper leaked commands or failed cleanup")
        report["status"] = "passed"
    except (RuntimeError, ValueError, OSError) as error:
        report.update(status="failed", error=str(error))
    (args.output/"summary.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(report["status"], len(report["cases"]), "cases", report.get("error", ""))
    return int(report["status"] != "passed")


if __name__ == "__main__":
    raise SystemExit(main())
