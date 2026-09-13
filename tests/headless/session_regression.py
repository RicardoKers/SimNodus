"""Replay real session commands and reject adversarial variations in the C++ gate."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    reference = json.loads(args.reference.read_text())
    records = reference["runs"][0]["session_contract"]
    commands = [r["command"] for r in records[1:]]
    command = [str(args.runner), "--cooperative-fixture"]
    report = {"status": "running", "command": command, "cases": [],
              "sha256": {str(p): hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in (args.runner, args.reference, Path(__file__))}}
    def run(name, lines, expected_commits=None, eof=False):
        result = subprocess.run(command, input="\n".join(lines)+("\n" if eof else "\nquit\n"),
                                text=True, capture_output=True, timeout=5)
        snapshots = [json.loads(line) for line in result.stdout.splitlines()]
        report["cases"].append({"name": name, "input": lines, "exit": result.returncode,
                                "snapshots": snapshots, "stderr": result.stderr})
        if eof:
            if result.returncode == 0:
                raise RuntimeError("Uncoordinated EOF accepted")
        elif result.returncode != 0:
            raise RuntimeError("Coordinated contract process did not close")
        elif expected_commits is not None:
            final = snapshots[-1]
            if final["phase"] != 5 or not final["error"] or final["commits"] != expected_commits:
                raise RuntimeError("Invalid transition published state or recovered")
        return snapshots
    try:
        snapshots = run("real-command-replay", commands)
        # Historical replay predates opt-in RC/readback diagnostics. Require them
        # to remain disabled/zero; preserve exact comparison of every old field.
        expected = [{"native_rc": 0, "rc_checks": 0, "native_readback": 0,
                     "readback_verified": 0, **r["snapshot"]} for r in records]
        if snapshots != expected:
            raise RuntimeError("Real command replay changed snapshots")
        ack_index = next(i for i,c in enumerate(commands) if c.startswith("ack "))
        ack = commands[ack_index].split()
        for name, field, value in (("unused-mismatch", 4, "1"), ("foreign-start", 1, "1"),
                                   ("wrong-sink", 6, "1"), ("unsigned-overflow", 3, str(2**64)),
                                   ("negative-time", 2, "-1"), ("not-cancelled", 5, "0")):
            changed = ack.copy()
            changed[field] = value
            run(name, commands[:ack_index]+[" ".join(changed), "begin 5000000 0", "commit"], 0)
        run("premature-commit", commands[:ack_index]+["commit"], 0)
        run("duplicate-ack", commands[:ack_index+1]+[commands[ack_index]], 0)
        run("no-observation", [commands[0], commands[ack_index]], 0)
        run("unpaced-pause", ["begin 100000000 0"], 0)
        run("reset-rejected", ["reset"], 0)
        run("trailing-token", ["begin 5000000 0 extra"], 0)
        run("lost-host", commands[:ack_index], eof=True)
        first_commit = commands.index("commit")
        run("failure-preserves-commit", commands[:first_commit+1]+["abort", "begin 5000000 0", "commit"], 1)
        report["status"] = "passed"
    except (ValueError, RuntimeError, subprocess.SubprocessError) as error:
        report.update(status="failed", error=str(error))
    (args.output/"summary.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(report["status"], len(report["cases"]), "cases", report.get("error", ""))
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
