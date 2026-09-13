"""Measure public Renode pause APIs during an accepted RunFor; never approve joint pause."""
from __future__ import annotations
import argparse
import json
import time
from pathlib import Path
from types import SimpleNamespace
import run as e05


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ide", type=Path, required=True)
    options = parser.parse_args()
    output = options.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    root = e05.ROOT
    args = SimpleNamespace(renode=root / "build/deps/renode/renode_1.16.1-dotnet_portable",
        prepared=root / "build/sn019/generated", firmware=root / "build/sn016/firmware",
        native=root / "build/sn016/native-vs/Debug",
        ngspice_dll=root / "build/deps/ngspice/Spice64_dll/dll-vs/ngspice.dll",
        audio=root / "build/deps/ngspice-console/Spice64/bin",
        initialization=root / "tools/backend_probe/initialization",
        gdb=next((options.ide / "plugins").glob("com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32*/tools/bin/arm-none-eabi-gdb.exe")))
    args.build = json.loads((args.firmware / "build.json").read_text(encoding="utf-8"))
    report = {"joint_pause_supported": False, "runs": [], "inputs_sha256": {
        p.name: e05.sha(p) for p in (Path(__file__), e05.HERE / "run.py", args.gdb,
        args.firmware / "firmware.elf", args.renode / "Renode.exe", args.prepared / "LoopbackControl.cs")}}
    for name, command in [("machine_request", "machine PauseAndRequestEmulationPause false"),
                          ("emulation_pause", "emulation PauseAll")]:
        directory = output / name
        renode = e05.RenodeProcess(args, directory)
        mi = pending = None
        result = {"case": name, "monitor_command": command, "joint_commit": False}
        try:
            mi = e05.MiSession(args.gdb, args.firmware / "firmware.elf", renode.debug_port, directory / "gdb-mi.log")
            mi.command("10-break-insert gpio_change_marker", "10^done")
            mi.command("20-exec-continue", "20^running")
            pending = e05.start_control(args, renode, "run", "5000")
            result["before"] = e05.stop_record(mi, mi.stop(), "breakpoint-hit", "gpio_change_marker")
            e05.check(pending.poll() is None, "Grant was not pending before pause")
            marker = "SIMNODUS_PAUSE_COMMAND_RETURNED"
            offset = renode.log_path.stat().st_size
            started = time.monotonic_ns()
            renode.process.stdin.write(command + '\necho "' + marker + '"\n')
            renode.process.stdin.flush()
            deadline = time.monotonic() + 3
            tail = ""
            while time.monotonic() < deadline:
                with renode.log_path.open("rb") as log:
                    log.seek(offset)
                    tail = log.read().decode("utf-8", errors="replace")
                if marker in [line.strip() for line in tail.splitlines()]:
                    break
                time.sleep(0.02)
            result["monitor_tail"] = tail
            result["monitor_returned"] = marker in [line.strip() for line in tail.splitlines()]
            result["monitor_wall_ns"] = time.monotonic_ns() - started
            e05.check(result["monitor_returned"], "Host monitor command did not acknowledge return")
            # A successful monitor return is not a cancellation acknowledgement.
            result["observations"] = []
            for _ in range(3):
                result["observations"].append({"time_ns": mi.virtual_time_ns(), "grant_exit": pending.poll()})
                time.sleep(0.1)
            result["inspection"] = e05.inspect_stopped(mi)
            result["classification"] = "candidate_requires_further_validation" if pending.poll() is not None else "no_grant_cancellation"
        except (OSError, ValueError, RuntimeError) as error:
            result["classification"] = "probe_failed"
            result["error"] = str(error)
        finally:
            renode.terminate(normal=False)
            if pending:
                stdout, stderr = pending.communicate(timeout=10)
                result["grant_after_cleanup"] = {"exit": pending.returncode, "stdout": stdout, "stderr": stderr}
            if mi: mi.close()
            result["listeners_removed"] = not e05.sn019.listeners(renode.process.pid)
            (directory / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        try:
            result["fresh_recovery"] = e05.normal_case(args, output / (name + "-recovery"))
        except (OSError, ValueError, RuntimeError) as error:
            result["classification"] = "probe_failed"
            result["recovery_error"] = str(error)
        report["runs"].append(result)
        (output / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(name, result["classification"], flush=True)
    return 1 if any(r["classification"] == "probe_failed" for r in report["runs"]) else 0


if __name__ == "__main__":
    raise SystemExit(main())
