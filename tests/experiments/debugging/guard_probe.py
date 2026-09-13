"""Exercise the restricted GDB transport against real E-05 backends."""
from __future__ import annotations
import argparse
import importlib.util
import json
import os
import subprocess
import time
from pathlib import Path
from types import SimpleNamespace
from gdb_guard import Guard

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
spec = importlib.util.spec_from_file_location("e05", HERE / "run.py")
e05 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(e05)


def run_case(args, directory, rejection=None):
    renode = e05.RenodeProcess(args, directory)
    guard = Guard(renode.debug_port)
    mi = None
    pending = None
    result = {"case": rejection or "normal", "status": "failed"}
    try:
        mi = e05.MiSession(args.gdb, args.firmware / "firmware.elf", guard.port, directory / "gdb-mi.log")
        e05.check(mi.virtual_time_ns() == 0, "Guarded attach advanced time")
        e05.check(e05.inspect_stopped(mi)["mailbox"] == [0] * 8, "Initial state changed")
        active = rejection in (None, "interrupt", "detach", "lost_connection")
        if active:
            mi.command("10-break-insert gpio_change_marker", "10^done")
            guard.arm(5000)
            mi.command("20-exec-continue", "20^running")
            pending = e05.start_control(args, renode, "run", "5000")
            stopped = e05.stop_record(mi, mi.stop(), "breakpoint-hit", "gpio_change_marker")
            e05.check(stopped["time_ns"] == 2010875, "Guard changed the GPIO stop time")
            result["before"] = stopped
        if rejection is None:
            mi.command("21-exec-step-instruction", "21^running")
            instruction = e05.stop_record(mi, mi.stop(), "end-stepping-range", "gpio_change_marker")
            e05.check(instruction["time_ns"] == 2011000 and instruction["pc"] == 0x080000fa,
                      "Guard changed the instruction step")
            result["inspection"] = e05.inspect_stopped(mi)
            result["circuit"] = e05.circuit(args, instruction["time_ns"])
            result["instruction"] = instruction
            e05.check(not guard.rejected.is_set(), "Allowed path was rejected")
        else:
            commands = {
                "continue_without_grant": "99-exec-continue",
                "monitor_start": '99-interpreter-exec console "monitor start"',
                "monitor_reset": '99-interpreter-exec console "monitor machine Reset"',
                "write_memory": "99-data-write-memory-bytes 0x20000000 01000000",
                "interrupt": "99-exec-interrupt",
                "detach": "99-target-detach",
            }
            if rejection == "lost_connection":
                mi.process.kill()
                mi.process.wait(timeout=5)
            else:
                mi.process.stdin.write(commands[rejection] + "\n")
                mi.process.stdin.flush()
            e05.check(guard.rejected.wait(5), "Unsafe action was not rejected by the transport")
            e05.check(guard.diagnostic["forwarded"] is False and guard.diagnostic["committed"] is False,
                      "Unsafe action was forwarded or committed")
            if not active:
                result["after"] = e05.control(args, renode, "time")
                mailbox = e05.control(args, renode, "read32", "0x20000000")
                e05.check(result["after"]["after_us"] == 0 and mailbox["value"] == 0,
                          "Rejected command changed the backend")
            result["rejection"] = guard.diagnostic
        result["status"] = "passed"
    except (ValueError, OSError, TimeoutError, subprocess.SubprocessError) as error:
        result["error"] = str(error)
    finally:
        guard.expected_exit = True
        try:
            renode.terminate(normal=True)
        except (ValueError, OSError, subprocess.SubprocessError) as error:
            result["status"] = "failed"
            result["cleanup_error"] = str(error)
        if pending:
            stdout, stderr = pending.communicate(timeout=10)
            result["pending_grant"] = {"exit": pending.returncode, "stdout": stdout, "stderr": stderr,
                                       "committed": False}
            if pending.returncode == 0:
                result["status"] = "failed"
                result["error"] = "Pending grant completed unexpectedly"
        guard.close()
        if mi:
            mi.close()
        result["guard"] = guard.evidence()
        result["listeners_removed"] = not e05.sn019.listeners(renode.process.pid) and not any(
            item["port"] == guard.port for item in e05.sn019.listeners(os.getpid()))
        if not result["listeners_removed"]: result["status"] = "failed"
        (directory / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--cases", nargs="+", default=["normal", "continue_without_grant", "monitor_start",
                        "monitor_reset", "write_memory", "interrupt", "detach", "lost_connection"])
    args0 = parser.parse_args()
    output = args0.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    ide = Path("C:/ST/STM32CubeIDE_2.1.1/STM32CubeIDE")
    args = SimpleNamespace(renode=ROOT / "build/deps/renode/renode_1.16.1-dotnet_portable",
        prepared=ROOT / "build/sn019/generated", firmware=ROOT / "build/sn016/firmware",
        native=ROOT / "build/sn016/native-vs/Debug",
        ngspice_dll=ROOT / "build/deps/ngspice/Spice64_dll/dll-vs/ngspice.dll",
        audio=ROOT / "build/deps/ngspice-console/Spice64/bin",
        initialization=ROOT / "tools/backend_probe/initialization",
        gdb=next((ide / "plugins").glob("com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32*/tools/bin/arm-none-eabi-gdb.exe")))
    subprocess.run([os.sys.executable, str(ROOT / "tools/check_backend_assets.py"), "--root", str(ROOT / "build/deps")], check=True)
    subprocess.run([os.sys.executable, str(HERE / "audit.py")], check=True)
    report = {"status": "passed", "runs": [], "sha256": {
        path.name: e05.sha(path) for path in (HERE / "gdb_guard.py", Path(__file__), args.gdb,
        args.firmware / "firmware.elf", args.native / "e05_control.exe", args.native / "e05_circuit.exe")}}
    for name in args0.cases:
        result = run_case(args, output / name, None if name == "normal" else name)
        report["runs"].append(result)
        print(name, result["status"], result.get("error", ""), flush=True)
        if result["status"] != "passed":
            report["status"] = "failed"
            break
        if name != "normal":
            recovery = run_case(args, output / (name + "-recovery"))
            report["runs"].append(recovery)
            print(name + "-recovery", recovery["status"], flush=True)
            if recovery["status"] != "passed":
                report["status"] = "failed"
                break
    (output / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__": raise SystemExit(main())
