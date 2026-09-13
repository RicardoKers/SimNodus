"""Exploratory single-CPU cancellation/resume probe; no joint-pause approval."""
import argparse
import json
import time
from pathlib import Path
from types import SimpleNamespace
import run as e


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renode", type=Path, required=True)
    parser.add_argument("--ide", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repetitions", type=int, default=3)
    opts = parser.parse_args()
    root = e.ROOT
    out = opts.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    a = SimpleNamespace(renode=opts.renode.resolve(), prepared=root / "build/sn019/generated",
        firmware=root / "build/sn016/firmware", native=root / "build/sn016/native-vs/Debug",
        ngspice_dll=root / "build/deps/ngspice/Spice64_dll/dll-vs/ngspice.dll",
        audio=root / "build/deps/ngspice-console/Spice64/bin", initialization=root / "tools/backend_probe/initialization",
        gdb=next((opts.ide / "plugins").glob("com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32*/tools/bin/arm-none-eabi-gdb.exe")))
    a.build = json.loads((a.firmware / "build.json").read_text(encoding="utf-8"))
    report = {"status": "passed", "joint_pause_supported": False, "runs": [], "sha256": {
        p.name: e.sha(p) for p in (Path(__file__), e.HERE / "cancel_bridge.cs", e.HERE / "renode-cancellation.patch",
            a.renode / "Renode.dll", a.renode / "Infrastructure.dll", a.gdb, a.firmware / "firmware.elf")}}
    for repetition in range(opts.repetitions):
        directory = out / str(repetition)
        renode = e.RenodeProcess(a, directory)
        mi = None
        result = {"stops": [], "outcomes": []}
        def send(command):
            renode.process.stdin.write(command + "\n")
            renode.process.stdin.flush()
        def start(path, command, duration=5000, cancel_after=0):
            send("emulation StartCancellationProbe " + str(duration) + " @" + path.as_posix() + " " + str(cancel_after))
            ready = Path(str(path) + ".ready")
            deadline = time.monotonic() + 3
            while not ready.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            e.check(ready.exists(), "New grant did not become active before debugger continue")
            if command is not None:
                mi.command(command + "-exec-continue", command + "^running")
        def outcome(path):
            deadline = time.monotonic() + 5
            while not path.exists() and time.monotonic() < deadline:
                error = path.with_suffix(path.suffix + ".error")
                e.check(not error.exists(), error.read_text() if error.exists() else "Bridge failed")
                time.sleep(0.01)
            e.check(path.exists(), "Cancellation outcome deadline expired")
            values = dict(line.split("=", 1) for line in path.read_text().splitlines())
            for name in ("start", "end", "requested", "unused"): values[name] = int(values[name])
            values["sinks"] = [int(v) for v in values["sinks"].split(",")]
            e.check(values["end"] - values["start"] + values["unused"] == values["requested"], "Time accounting mismatch")
            e.check(values["sinks"] and all(v == values["end"] for v in values["sinks"]), "Sink time mismatch")
            return values
        try:
            send("include @" + (e.HERE / "cancel_bridge.cs").as_posix())
            mi = e.MiSession(a.gdb, a.firmware / "firmware.elf", renode.debug_port, directory / "gdb-mi.log")
            mi.command("10-break-insert gpio_change_marker", "10^done")
            mi.command("11-break-insert *0x08000102", "11^done")
            for step, expected in enumerate((2010875, 2011375)):
                path = (directory / (str(step) + ".txt")).resolve()
                start(path, str(20 + step))
                stopped = e.stop_record(mi, mi.stop(), "breakpoint-hit")
                e.check(stopped["time_ns"] == expected, "Stop differs from binary baseline")
                result["stops"].append(stopped)
                started = time.monotonic_ns()
                send("emulation CancelCancellationProbe")
                values = outcome(path)
                values["wall_ns"] = time.monotonic_ns() - started
                e.check(values["wall_ns"] <= 2_000_000_000 and values["cancelled"] == "True", "Cancellation failed")
                e.check(values["end"] == expected and values["unused"] > 0, "Cancellation advanced past held stop")
                result["outcomes"].append(values)
                inspection = e.inspect_stopped(mi)
                e.check(inspection["after_ns"] == expected, "Inspection advanced after cancellation")
                send("emulation CancelCancellationProbe")
                time.sleep(0.1)
                e.check(mi.virtual_time_ns() == expected, "Unacknowledged time after cancellation")
            # A new full interval in the same backend proves the abandoned remainder is not reused.
            mi.command("30-break-delete", "30^done")
            path = (directory / "complete.txt").resolve()
            start(path, "31")
            completed = outcome(path)
            e.check(completed["unused"] == 0 and completed["cancelled"] == "False", "New interval did not complete")
            e.check(completed["end"] == 7011375, "Old grant affected new endpoint")
            result["completed"] = completed
            path = (directory / "running.txt").resolve()
            start(path, None, cancel_after=1000)
            running = outcome(path)
            e.check(running["end"] == 8011375 and running["unused"] == 4000000
                and running["cancelled"] == "True" and running["reason"] == "cancelled", "Running cancellation mismatch")
            result["running"] = running
            time.sleep(0.1)
            sample = (directory / "stable.txt").resolve()
            send("emulation SampleCancellationProbe @" + sample.as_posix())
            deadline = time.monotonic() + 2
            while not sample.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            e.check(sample.exists() and int(sample.read_text()) == running["end"], "Running cancellation did not stabilize")
            result["running_stable_ns"] = int(sample.read_text())
            result["gdb_thread_info"] = "\n".join(mi.command("51-thread-info", "51^"))
            result["gdb_running_after_host_pause"] = ('state="running"' in result["gdb_thread_info"]
                or "while the target is running" in result["gdb_thread_info"])
            e.check(result["gdb_running_after_host_pause"], "Unexpected GDB state observation")
            path = (directory / "endpoint.txt").resolve()
            start(path, None, duration=1000, cancel_after=1000)
            endpoint = outcome(path)
            e.check(endpoint["end"] == 9011375 and endpoint["unused"] == 0
                and endpoint["reason"] == "completed", "Endpoint completion precedence mismatch")
            result["endpoint"] = endpoint
        except (OSError, ValueError, RuntimeError) as error:
            result["error"] = str(error)
            report["status"] = "failed"
        finally:
            renode.terminate(normal=False)
            if mi: mi.close()
            result["listeners_removed"] = not e.sn019.listeners(renode.process.pid)
        report["runs"].append(result)
        (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(repetition, report["status"], result.get("error", ""), flush=True)
        if report["status"] != "passed": break
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
