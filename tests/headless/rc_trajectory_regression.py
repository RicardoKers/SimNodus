"""Exercise native RC acceptance and fail-closed ordering over actual Windows pipes."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import sys
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
    names = ("valid", "positive-error", "negative-error", "missing-high", "pre-edge-error",
             "duplicate-enable", "late-enable", "enable-argument", "reference")
    try:
        for name in names:
            directory = (args.output / name).resolve()
            directory.mkdir()
            rr, rw = os.pipe()
            ar, aw = os.pipe()
            out_read, out_write = os.pipe()
            with os.fdopen(rw, "wb", buffering=0) as renode, os.fdopen(aw, "wb", buffering=0) as analog, os.fdopen(out_read, "rb", buffering=0) as output:
                client = SessionContract(args.runner, renode, analog, output)
            case = {"name": name}
            report["cases"].append(case)
            def command(text):
                try:
                    return client.command(text)
                except RuntimeError:
                    value = client.records[-1]["snapshot"]
                    if not value["error"]:
                        raise
                    return value
            def publish(target, voltage, samples):
                os.write(out_write, (json.dumps({"time_ns": target, "actual_s": target * 1e-9,
                    "output_v": voltage, "samples": samples}, separators=(",", ":")) + "\n").encode("ascii"))
            def advance(start, target, voltage, samples):
                path = directory / f"grant-{target}.txt"
                command(f'grant-native 5000000 0 "{path.as_posix()}"')
                os.read(rr, 4096)
                Path(str(path) + ".ready").write_text("active", encoding="utf-8")
                command("poll-ready")
                command(f"stop-native {target}")
                os.read(rr, 4096)
                path.write_text(f"start={start}\nrequested=5000000\nend={target}\nreason=cancelled\nunused={5000000-target+start}\nsinks={target}\ncancelled=True", encoding="utf-8")
                assert not command("poll-result")["error"]
                assert not command("advance-native")["error"]
                assert os.read(ar, 4096) == f"advance {target}\n".encode()
                publish(target, voltage, samples)
                return command("poll-worker")
            try:
                if name not in ("reference", "late-enable"):
                    final = command("enable-rc 1" if name == "enable-argument" else "enable-rc")
                if name == "duplicate-enable":
                    final = command("enable-rc")
                if name not in ("duplicate-enable", "enable-argument"):
                    publish(0, 0, 0)
                    command("read-worker")
                    assert not command("poll-worker")["error"]
                    if name == "late-enable":
                        final = command("enable-rc")
                    else:
                        final = advance(0, 2011375, .001 if name == "pre-edge-error" else 0, 2)
                        if not final["error"]:
                            if name != "missing-high":
                                assert not command("high-native")["error"]
                                assert os.read(ar, 4096) == b"high\n"
                                publish(2011375, 0, 2)
                                assert not command("poll-worker")["error"]
                            assert not command("commit")["error"]
                            voltage = 3.3 * (1 - math.exp(-1))
                            if name in ("positive-error", "reference"):
                                voltage += .00002
                            elif name == "negative-error":
                                voltage -= .00002
                            final = advance(2011375, 3011375, voltage, 3)
                success = name in ("valid", "reference")
                assert bool(final["error"]) != success, "Unexpected trajectory acceptance"
                if success:
                    assert final["phase"] == 4 and final["rc_checks"] == (2 if name == "valid" else 0)
                else:
                    prior = final["commits"]
                    assert command("commit")["commits"] == prior, "Rejected trajectory committed"
                case["final"] = final
            finally:
                command("abort")
                client.close()
                case["records"] = client.records
                os.close(rr)
                os.close(ar)
                os.close(out_write)
        report["status"] = "passed"
    except (AssertionError, RuntimeError, OSError, ValueError) as error:
        report.update(status="failed", error=str(error))
    (args.output / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(report["status"], len(report["cases"]), "cases", report.get("error", ""))
    return int(report["status"] != "passed")


if __name__ == "__main__":
    raise SystemExit(main())
