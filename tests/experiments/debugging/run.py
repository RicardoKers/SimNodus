"""Run and analyze the predeclared plain-GDB portion of E-05."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import queue
import re
import socket
import struct
import subprocess
import tempfile
import threading
import time
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
REPETITIONS = 3
MAILBOX_ADDRESS = 0x20000000
GPIO_ODR_ADDRESS = 0x4001080C
GPIO_AFTER_WRITE_ADDRESS = 0x08000102
EXPECTED_ADC_CODE = 2048

spec = importlib.util.spec_from_file_location("sn019", HERE.parent / "renode-client/run.py")
sn019 = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sn019)
check = sn019.check


def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def reserve_port() -> int:
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        return reservation.getsockname()[1]


def reserve_pair() -> tuple[int, int]:
    control, debug = reserve_port(), reserve_port()
    while debug == control:
        debug = reserve_port()
    return control, debug


def parse_virtual_time(lines: list[str]) -> int:
    text = "\n".join(lines)
    match = re.search(r"Elapsed Virtual Time: (\d+):(\d+):(\d+)\.(\d{9})", text)
    check(match is not None, "GDB monitor response omitted elapsed virtual time")
    hours, minutes, seconds, fraction = (int(value) for value in match.groups())
    return ((hours * 60 + minutes) * 60 + seconds) * 1_000_000_000 + fraction


class MiSession:
    def __init__(self, executable: Path, elf: Path, port: int, log_path: Path):
        self.lines: list[str] = []
        self.log_path = log_path
        self.pending: queue.Queue[str] = queue.Queue()
        self.process = subprocess.Popen(
            [str(executable), "--interpreter=mi2", "--nx", "--quiet", str(elf)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        threading.Thread(target=self._reader, daemon=True).start()
        self.command("0-gdb-set mi-async on", "0^done")
        self.command(f"1-target-select remote 127.0.0.1:{port}", "1^connected", timeout=20)
        self.command(f"2-gdb-set substitute-path /simnodus/e05 {HERE.as_posix()}", "2^done")

    def _reader(self) -> None:
        for raw in self.process.stdout:
            line = raw.rstrip("\r\n")
            self.lines.append(line)
            self.pending.put(line)

    def wait(self, predicate, timeout: float = 10.0) -> str:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                line = self.pending.get(timeout=max(0.01, deadline - time.monotonic()))
            except queue.Empty as error:
                raise TimeoutError("GDB response deadline expired; tail: " + " | ".join(self.lines[-30:])) from error
            if predicate(line):
                return line
        raise TimeoutError("GDB response deadline expired; tail: " + " | ".join(self.lines[-30:]))

    def command(self, text: str, expected: str, timeout: float = 10.0) -> list[str]:
        check(self.process.poll() is None, "GDB exited before a command")
        start = len(self.lines)
        self.process.stdin.write(text + "\n")
        self.process.stdin.flush()
        self.wait(lambda line: line.startswith(expected), timeout)
        return self.lines[start:]

    def stop(self, timeout: float = 10.0) -> str:
        return self.wait(lambda line: line.startswith("*stopped"), timeout)

    def interrupt(self, timeout: float = 2.0) -> str:
        self.command("93-exec-interrupt", "93^done", timeout=timeout)
        return self.stop(timeout)

    def virtual_time_ns(self, timeout: float = 10.0) -> int:
        return parse_virtual_time(
            self.command('90-interpreter-exec console "monitor machine ElapsedVirtualTime"', "90^done", timeout=timeout)
        )

    def read_memory(self, address: int, count: int) -> bytes:
        lines = self.command(f"91-data-read-memory-bytes 0x{address:x} {count}", "91^done")
        self.last_memory_read = (address, count, [line for line in lines if line.startswith("91^done")])
        match = re.search(r'contents="([0-9a-fA-F]+)"', "\n".join(lines))
        check(match is not None, "GDB memory response omitted contents")
        result = bytes.fromhex(match.group(1))
        check(len(result) == count, "GDB memory response length changed")
        return result

    def close(self, detach: bool = False) -> None:
        if self.process.poll() is not None:
            self.log_path.write_text("\n".join(self.lines) + "\n", encoding="utf-8")
            return
        if detach:
            try:
                self.command("97-target-detach", "97^done", timeout=5)
            except (ValueError, OSError, TimeoutError):
                pass
        try:
            self.process.stdin.write("98-gdb-exit\n")
            self.process.stdin.flush()
            self.process.wait(timeout=5)
        except (OSError, subprocess.TimeoutExpired):
            self.process.kill()
            self.process.wait(timeout=5)
        self.log_path.write_text("\n".join(self.lines) + "\n", encoding="utf-8")


class RenodeProcess:
    def __init__(self, args, directory: Path):
        directory.mkdir(parents=True, exist_ok=True)
        self.directory = directory
        self.control_port, self.debug_port = reserve_pair()
        self.log_path = directory / "renode.log"
        config = directory / "renode.config"
        config.write_text(
            "[general]\ncompiler-cache-enabled = False\nhistory-path = "
            + (directory / "history").as_posix()
            + "\n[monitor]\nconsume-exceptions-from-command = True\n"
              "break-script-on-exception = True\n[plugins]\nenabled-plugins =\n",
            encoding="utf-8",
        )
        paths = [
            args.prepared / "LoopbackControl.cs",
            HERE / "loopback_gdb.cs",
            HERE / "stm32f103c8-debug.repl",
            args.firmware / "firmware.elf",
        ]
        check(not any(character.isspace() for path in paths for character in str(path)),
              "E-05 Renode paths must not contain whitespace")
        commands = [
            f"include @{paths[0].as_posix()}",
            f"include @{paths[1].as_posix()}",
            'mach create "sn016"',
            f"machine LoadPlatformDescription @{paths[2].as_posix()}",
            f"sysbus LoadELF @{paths[3].as_posix()}",
            "sysbus.cpu VectorTableOffset 0x08000000",
            'emulation SetGlobalQuantum "0.000001"',
            f'emulation CreateLoopbackControlServer "sn016-control" {self.control_port}',
            f"machine StartLoopbackGdbServer {self.debug_port}",
        ]
        environment = os.environ.copy()
        environment["TEMP"] = environment["TMP"] = tempfile.mkdtemp(prefix="runtime-", dir=directory)
        self.log = self.log_path.open("w", encoding="utf-8")
        factory = subprocess.Popen
        if getattr(args, "process_runner", None):
            from native_process import NativeProcess
            factory = lambda command, **options: NativeProcess(args.process_runner, directory / "renode-process.pid", command, **options)
        self.process = factory(
            [str(args.renode / "Renode.exe"), "--disable-xwt", "--console", "--plain",
             "--config", str(config), "--execute", "; ".join(commands)],
            stdin=subprocess.PIPE,
            stdout=self.log,
            stderr=subprocess.STDOUT,
            text=True,
            env=environment,
            cwd=str(args.renode),
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        try:
            deadline = time.monotonic() + 30
            while time.monotonic() < deadline:
                check(self.process.poll() is None, "Renode exited during E-05 startup")
                endpoints = sn019.listeners(self.process.pid)
                if len(endpoints) == 2:
                    expected = [
                        {"address": "127.0.0.1", "port": self.control_port, "owner_matches": True},
                        {"address": "127.0.0.1", "port": self.debug_port, "owner_matches": True},
                    ]
                    check(sorted(endpoints, key=lambda item: item["port"]) == sorted(expected, key=lambda item: item["port"]),
                          "Unexpected E-05 Renode listener")
                    self.listeners = endpoints
                    return
                contents = self.log_path.read_text(encoding="utf-8", errors="replace")
                check("error executing command" not in contents and "Fatal error" not in contents,
                      "Renode E-05 startup failed")
                time.sleep(0.05)
            raise ValueError("Renode E-05 listener startup timed out")
        except BaseException:
            self.terminate(normal=False)
            raise

    def terminate(self, normal: bool = True) -> int:
        if self.process.poll() is None:
            if normal:
                try:
                    self.process.communicate("quit\n", timeout=10)
                except (OSError, subprocess.TimeoutExpired):
                    self.process.kill()
                    self.process.communicate(timeout=5)
            else:
                self.process.terminate()
                try:
                    self.process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.process.kill()
                    self.process.wait(timeout=5)
        self.log.close()
        check(not sn019.listeners(self.process.pid), "E-05 Renode listener survived shutdown")
        if normal:
            check(self.process.returncode == 0, "Renode E-05 shutdown was not normal")
        return self.process.returncode


def control(args, renode: RenodeProcess, *command: str, timeout: float = 35.0) -> dict:
    result = subprocess.run(
        [str(args.native / "e05_control.exe"), str(renode.control_port), *command],
        capture_output=True,
        text=True,
        timeout=timeout,
        creationflags=subprocess.CREATE_NO_WINDOW,
    )
    check(result.returncode == 0, result.stderr.strip() or "E-05 control command failed")
    return json.loads(result.stdout)


def start_control(args, renode: RenodeProcess, *command: str) -> subprocess.Popen:
    return subprocess.Popen(
        [str(args.native / "e05_control.exe"), str(renode.control_port), *command],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        creationflags=subprocess.CREATE_NO_WINDOW,
    )


def finish_control(process: subprocess.Popen, timeout: float = 35.0) -> dict:
    stdout, stderr = process.communicate(timeout=timeout)
    check(process.returncode == 0, stderr.strip() or "Pending E-05 control command failed")
    return json.loads(stdout)


def circuit(args, target_ns: int, rise_ns: int | None = None, fall_ns: int | None = None) -> dict:
    result = subprocess.run(
        [str(args.native / "e05_circuit.exe"), str(args.ngspice_dll), str(args.audio),
         str(args.initialization), str(target_ns), str(rise_ns) if rise_ns is not None else "none",
         str(fall_ns) if fall_ns is not None else "none"],
        capture_output=True,
        text=True,
        timeout=30,
        creationflags=subprocess.CREATE_NO_WINDOW,
    )
    check(result.returncode == 0, result.stderr.strip() or "E-05 circuit command failed")
    data = json.loads(result.stdout)
    check(abs(data["actual_s"] - target_ns * 1e-9) <= 1e-12,
          "ngspice checkpoint overshot the debugger stop")
    return data


def stop_record(mi: MiSession, line: str, expected_reason: str, expected_function: str | None = None) -> dict:
    reason = re.search(r'reason="([^"]+)"', line)
    address = re.search(r'frame=\{addr="0x([0-9a-fA-F]+)"', line)
    function = re.search(r'func="([^"]+)"', line)
    check(reason is not None and reason.group(1) == expected_reason, "Unexpected GDB stop reason")
    check(address is not None, "GDB stop omitted the program counter")
    if expected_function is not None:
        check(function is not None and function.group(1) == expected_function, "Unexpected GDB stop function")
    return {
        "reason": reason.group(1),
        "pc": int(address.group(1), 16),
        "function": function.group(1) if function else None,
        "time_ns": mi.virtual_time_ns(),
    }


def inspect_stopped(mi: MiSession) -> dict:
    before = mi.virtual_time_ns()
    registers = mi.command("92-data-list-register-values x 13 14 15", "92^done")
    mailbox = struct.unpack("<8I", mi.read_memory(MAILBOX_ADDRESS, 32))
    after = mi.virtual_time_ns()
    check(before == after, "GDB inspection advanced virtual time")
    return {"before_ns": before, "after_ns": after, "register_response": "\n".join(registers),
            "mailbox": list(mailbox)}


def normal_case(args, directory: Path) -> dict:
    renode = RenodeProcess(args, directory)
    mi = None
    pending = None
    try:
        check(control(args, renode, "time") == {"command": "time", "before_us": 0, "after_us": 0},
              "Fresh Renode time changed")
        mi = MiSession(args.gdb, args.firmware / "firmware.elf", renode.debug_port, directory / "gdb-mi.log")
        attach_time = mi.virtual_time_ns()
        check(attach_time == 0, "GDB attach advanced virtual time")
        control(args, renode, "adc", "0", "1650000")
        breaks = [
            ("10-break-insert gpio_change_marker", "10^done"),
            (f"11-break-insert *0x{GPIO_AFTER_WRITE_ADDRESS:x}", "11^done"),
            ("12-break-insert step_target", "12^done"),
            ("13-break-insert adc_read_marker", "13^done"),
        ]
        for command_text, expected in breaks:
            mi.command(command_text, expected)

        mi.command("20-exec-continue", "20^running")
        pending = start_control(args, renode, "run", "5000")
        gpio = stop_record(mi, mi.stop(), "breakpoint-hit", "gpio_change_marker")
        gpio["circuit"] = circuit(args, gpio["time_ns"])
        gpio["inspection"] = inspect_stopped(mi)

        mi.command("21-exec-step-instruction", "21^running")
        instruction = stop_record(mi, mi.stop(), "end-stepping-range", "gpio_change_marker")
        check(instruction["pc"] != gpio["pc"] and instruction["time_ns"] >= gpio["time_ns"],
              "Instruction step did not change PC or regressed time")
        instruction["circuit"] = circuit(args, instruction["time_ns"])

        mi.command("22-exec-continue", "22^running")
        edge = stop_record(mi, mi.stop(), "breakpoint-hit")
        check(edge["pc"] == GPIO_AFTER_WRITE_ADDRESS, "GPIO after-write breakpoint moved")
        odr = struct.unpack("<I", mi.read_memory(GPIO_ODR_ADDRESS, 4))[0]
        check(odr & 1, "GPIO output was not high after the write")
        edge["gpio_odr"] = odr
        edge["circuit"] = circuit(args, edge["time_ns"], edge["time_ns"])

        mi.command("23-exec-continue", "23^running")
        step_target = stop_record(mi, mi.stop(), "breakpoint-hit", "step_target")
        step_target["circuit"] = circuit(args, step_target["time_ns"], edge["time_ns"])
        next_stops = []
        for token in (24, 25, 26):
            mi.command(f"{token}-exec-next", f"{token}^running")
            item = stop_record(mi, mi.stop(), "end-stepping-range", "step_target")
            check(item["pc"] != args.build["symbols"]["step_helper"], "Source next entered the helper")
            next_stops.append(item)
        check(next_stops[-1]["time_ns"] >= step_target["time_ns"], "Source next regressed time")
        next_stops[-1]["circuit"] = circuit(args, next_stops[-1]["time_ns"], edge["time_ns"])

        mi.command("27-exec-continue", "27^running")
        adc = stop_record(mi, mi.stop(), "breakpoint-hit", "adc_read_marker")
        adc["circuit"] = circuit(args, adc["time_ns"], edge["time_ns"])
        mi.command("28-exec-finish", "28^running")
        finish_line = mi.stop()
        finish_reason = re.search(r'reason="([^"]+)"', finish_line)
        check(finish_reason is not None and finish_reason.group(1) in ("function-finished", "end-stepping-range"),
              "ADC finish returned an unexpected stop")
        finish = stop_record(mi, finish_line, finish_reason.group(1))
        finish["circuit"] = circuit(args, finish["time_ns"], edge["time_ns"])
        adc_assignment_steps = []
        final_inspection = inspect_stopped(mi)
        for token in range(60, 68):
            if final_inspection["mailbox"][4] == EXPECTED_ADC_CODE:
                break
            mi.command(f"{token}-exec-step-instruction", f"{token}^running")
            step = stop_record(mi, mi.stop(), "end-stepping-range")
            adc_assignment_steps.append(step)
            final_inspection = inspect_stopped(mi)
        check(final_inspection["mailbox"][4] == EXPECTED_ADC_CODE,
              "ADC result was not committed to firmware memory while stopped")
        committed_time = mi.virtual_time_ns()
        final_circuit = circuit(args, committed_time, edge["time_ns"])
        magic, ticks, changes, adc_reads, adc_result, step_count, fault, _ = final_inspection["mailbox"]
        check(magic == 0x534E3035 and ticks >= 4 and changes == 1 and adc_reads == 1,
              "Firmware marker observations changed")
        check(adc_result == EXPECTED_ADC_CODE and step_count == 1 and fault == 0,
              "Firmware ADC or step-over result changed")
        renode.terminate(normal=True)
        pending_stdout, pending_stderr = pending.communicate(timeout=10)
        check(pending.returncode != 0, "Pending grant unexpectedly committed after coordinated session stop")
        pending_exit = pending.returncode
        pending = None
        mi.close()
        mi = None

        return {
            "attach_time_ns": attach_time,
            "gpio_stop": gpio,
            "instruction_step": instruction,
            "gpio_edge_stop": edge,
            "step_target_stop": step_target,
            "next_stops": next_stops,
            "adc_stop": adc,
            "adc_finish_stop": finish,
            "adc_assignment_steps": adc_assignment_steps,
            "final_circuit": final_circuit,
            "final_mailbox": final_inspection["mailbox"],
            "coordinated_stop_time_ns": committed_time,
            "pending_grant_after_session_stop": {"exit": pending_exit, "stdout": pending_stdout,
                                                  "diagnostic": pending_stderr.strip(), "committed": False},
            "listener_addresses": sorted(item["address"] for item in renode.listeners),
        }
    finally:
        if pending and pending.poll() is None:
            pending.kill()
            pending.communicate(timeout=5)
        if mi:
            mi.close()
        renode.terminate(normal=True)


def pause_case(args, directory: Path) -> dict:
    renode = RenodeProcess(args, directory)
    mi = None
    pending = None
    try:
        mi = MiSession(args.gdb, args.firmware / "firmware.elf", renode.debug_port, directory / "gdb-mi.log")
        mi.command("30-exec-continue", "30^running")
        pending = start_control(args, renode, "run", "100000")
        time.sleep(0.01)
        started = time.monotonic()
        stop = stop_record(mi, mi.interrupt(timeout=2), "signal-received")
        wall = time.monotonic() - started
        check(0 <= stop["time_ns"] <= 100_000_000 and wall <= 2.0,
              "GDB interrupt missed its wall deadline")
        advance = finish_control(pending)
        pending = None
        effective_time = mi.virtual_time_ns()
        check(advance["after_us"] == 100000 and effective_time == 100_000_000,
              "Interrupted global domain did not expose its actual host boundary")
        inspection = inspect_stopped(mi)
        renode.terminate(normal=True)
        mi.close()
        mi = None
        return {"stop_reply": stop, "interrupt_wall_s": wall, "effective_global_time_ns": effective_time,
                "inspection_after_global_stabilization": inspection, "supported": False,
                "diagnostic": "GDB stopped the CPU, but global virtual time continued to the active grant boundary",
                "committed_at_interrupt": False}
    finally:
        if pending and pending.poll() is None:
            pending.kill()
            pending.communicate(timeout=5)
        if mi:
            mi.close()
        renode.terminate(normal=True)


def reset_case(args, directory: Path) -> dict:
    renode = RenodeProcess(args, directory)
    mi = None
    try:
        mi = MiSession(args.gdb, args.firmware / "firmware.elf", renode.debug_port, directory / "gdb-mi.log")
        time_ns = mi.virtual_time_ns()
        mailbox = list(struct.unpack("<8I", mi.read_memory(MAILBOX_ADDRESS, 32)))
        analog = circuit(args, 0)
        check(time_ns == 0 and mailbox == [0] * 8 and analog["actual_s"] == 0,
              "Coordinated fresh-session reset did not restore initial state")
        mi.close(detach=True)
        mi = None
        after_detach = control(args, renode, "time")
        check(after_detach["after_us"] == 0, "Stopped disconnect without a grant advanced virtual time")
        reconnected = MiSession(args.gdb, args.firmware / "firmware.elf", renode.debug_port,
                                directory / "gdb-reconnect-mi.log")
        reconnect_time = reconnected.virtual_time_ns()
        reconnected.close(detach=True)
        check(reconnect_time == 0, "Reconnect without a grant advanced virtual time")
        return {"renode_time_ns": time_ns, "mailbox": mailbox, "circuit": analog,
                "disconnect": {"before_ns": time_ns, "after_ns": reconnect_time, "hidden_advance": False},
                "debugger_reset": {"status": "rejected", "diagnostic": "debugger-only reset is unsupported"}}
    finally:
        if mi:
            mi.close(detach=True)
        renode.terminate(normal=True)


def backend_failure_case(args, directory: Path) -> dict:
    renode = RenodeProcess(args, directory)
    mi = None
    pending = None
    try:
        mi = MiSession(args.gdb, args.firmware / "firmware.elf", renode.debug_port, directory / "gdb-mi.log")
        mi.command("40-exec-continue", "40^running")
        pending = start_control(args, renode, "run", "1000000")
        time.sleep(0.02)
        renode.terminate(normal=False)
        stdout, stderr = pending.communicate(timeout=10)
        check(pending.returncode != 0, "Control command succeeded after Renode termination")
        return {"renode_exit": renode.process.returncode, "control_exit": pending.returncode,
                "control_stdout": stdout, "control_diagnostic": stderr.strip(), "committed": False,
                "listeners_removed": True}
    finally:
        if pending and pending.poll() is None:
            pending.kill()
            pending.communicate(timeout=5)
        if mi:
            mi.close()
        if renode.process.poll() is None:
            renode.terminate(normal=False)


def unreachable_timeout_case(args, directory: Path) -> dict:
    renode = RenodeProcess(args, directory)
    mi = None
    try:
        mi = MiSession(args.gdb, args.firmware / "firmware.elf", renode.debug_port, directory / "gdb-mi.log")
        mi.command("50-break-insert *0x0800fffe", "50^done")
        mi.command("51-exec-continue", "51^running")
        advance = control(args, renode, "run", "1000")
        timed_out = False
        try:
            mi.stop(timeout=1)
        except TimeoutError:
            timed_out = True
        check(timed_out and advance["after_us"] == 1000,
              "Unreachable breakpoint did not produce the declared stop timeout")
        return {"deadline_s": 1, "run_boundary_us": advance["after_us"], "timed_out": True,
                "committed": False}
    finally:
        if mi:
            mi.close()
        renode.terminate(normal=True)


def stable_normal_signature(case: dict) -> dict:
    def compact_stop(item: dict) -> dict:
        return {key: item[key] for key in ("reason", "pc", "function", "time_ns")}
    return {
        "attach_time_ns": case["attach_time_ns"],
        "gpio": compact_stop(case["gpio_stop"]),
        "instruction": compact_stop(case["instruction_step"]),
        "edge": compact_stop(case["gpio_edge_stop"]),
        "step_target": compact_stop(case["step_target_stop"]),
        "next": [compact_stop(item) for item in case["next_stops"]],
        "adc": compact_stop(case["adc_stop"]),
        "adc_finish": compact_stop(case["adc_finish_stop"]),
        "adc_assignment": [compact_stop(item) for item in case["adc_assignment_steps"]],
        "mailbox": case["final_mailbox"],
        "coordinated_stop_time_ns": case["coordinated_stop_time_ns"],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renode", type=Path, default=ROOT / "build/deps/renode/renode_1.16.1-dotnet_portable")
    parser.add_argument("--prepared", type=Path, default=ROOT / "build/sn019/generated")
    parser.add_argument("--firmware", type=Path, default=ROOT / "build/sn016/firmware")
    parser.add_argument("--native", type=Path, default=ROOT / "build/sn016/native-vs/Debug")
    parser.add_argument("--audit", type=Path, default=ROOT / "build/sn016/audit")
    parser.add_argument("--deps", type=Path, default=ROOT / "build/deps")
    parser.add_argument("--gdb", type=Path, default=Path(r"C:\ST\STM32CubeIDE_2.1.1\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin\arm-none-eabi-gdb.exe"))
    parser.add_argument("--output", type=Path, default=ROOT / "build/sn016/results")
    args = parser.parse_args()
    for name in ("renode", "prepared", "firmware", "native", "audit", "deps", "gdb", "output"):
        setattr(args, name, getattr(args, name).resolve())
    args.output.mkdir(parents=True, exist_ok=True)
    args.ngspice_dll = args.deps / "ngspice/Spice64_dll/dll-vs/ngspice.dll"
    args.audio = args.deps / "ngspice-console/Spice64/bin"
    args.initialization = ROOT / "tools/backend_probe/initialization"

    subprocess.run([os.sys.executable, str(ROOT / "tools/check_backend_assets.py"), "--root", str(args.deps)], check=True)
    subprocess.run([os.sys.executable, str(HERE / "audit.py"), "--root", str(args.audit)], check=True)
    args.build = json.loads((args.firmware / "build.json").read_text(encoding="utf-8"))
    check(sha(args.firmware / "firmware.elf") == args.build["elf_sha256"], "E-05 firmware ELF changed")
    check(args.build["identical_rebuild"] and args.build["debug_information"] == "DWARF-4",
          "E-05 deterministic debug firmware gate failed")
    gdb_version = subprocess.check_output([str(args.gdb), "--version"], text=True, timeout=10).splitlines()[0]
    report = {
        "status": "passed",
        "profile": {"repetitions": REPETITIONS, "time_authority": "experiment_host",
                    "gdb_autostart": False, "reverse_execution": False,
                    "external_control_deadline_ms": 30000},
        "gdb": {"version": gdb_version, "sha256": sha(args.gdb)},
        "firmware": args.build,
        "audit": json.loads((args.audit / "audit.json").read_text(encoding="utf-8")),
        "runs": [],
    }
    try:
        for repetition in range(1, REPETITIONS + 1):
            base = args.output / f"repetition-{repetition}"
            started = datetime.now(timezone.utc).isoformat()
            run = {
                "normal": normal_case(args, base / "normal"),
                "pause": pause_case(args, base / "pause"),
                "reset": reset_case(args, base / "reset"),
                "backend_failure": backend_failure_case(args, base / "backend-failure"),
                "backend_recovery": reset_case(args, base / "backend-recovery"),
                "unreachable_timeout": unreachable_timeout_case(args, base / "unreachable-timeout"),
                "timeout_recovery": reset_case(args, base / "timeout-recovery"),
                "started_utc": started,
                "completed_utc": datetime.now(timezone.utc).isoformat(),
            }
            report["runs"].append(run)
            print(f"plain GDB repetition {repetition}: passed", flush=True)
        signatures = [stable_normal_signature(run["normal"]) for run in report["runs"]]
        check(all(item == signatures[0] for item in signatures[1:]),
              "Repeated plain-GDB committed stops or firmware observations changed")
        report["repeated_normal_signature"] = signatures[0]
    except (ValueError, OSError, TimeoutError, subprocess.TimeoutExpired) as error:
        report["status"] = "failed"
        report["error"] = str(error)
    paths = [HERE / name for name in ("loopback_gdb.cs", "control.cpp", "circuit.cpp", "firmware.c",
             "firmware.ld", "mailbox.h", "stm32f103c8-debug.repl", "run.py", "CMakeLists.txt")]
    paths += [args.native / "e05_control.exe", args.native / "e05_circuit.exe", args.gdb,
              args.ngspice_dll, args.renode / "Renode.exe"]
    report["sha256"] = {path.name: sha(path) for path in paths}
    (args.output / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
