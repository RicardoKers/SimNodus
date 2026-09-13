"""Bounded real-engine persistent CPU/GPIO/RC/ADC checkpoint probe."""
from __future__ import annotations

import argparse
import json
import math
import os
import queue
import re
import struct
import subprocess
import threading
import time
from pathlib import Path
from types import SimpleNamespace
import run as e
from cooperative_debug import CooperativeGuard
from session_contract import SessionContract


class Analog:
    def __init__(self, args, directory):
        self.log = (directory / "analog.jsonl").open("w", encoding="utf-8")
        self.errors = (directory / "analog.stderr.txt").open("w", encoding="utf-8")
        self.pending = queue.Queue()
        factory = subprocess.Popen
        if getattr(args, "process_runner", None):
            from native_process import NativeProcess
            factory = lambda command, **options: NativeProcess(args.process_runner, directory / "analog-process.pid", command, **options)
        self.process = factory([str(getattr(args, "analog_runner", None) or args.native / "e05_joint_circuit.exe"),
            str(args.ngspice_dll), str(args.audio), str(args.initialization),
            "10000000", "none", "none"], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=self.errors, text=True, bufsize=1, creationflags=subprocess.CREATE_NO_WINDOW)
        self.session = None
        self.native_exchange = args.native_exchange
        self.reader = None
        if not args.native_analog_results:
            self.reader = threading.Thread(target=self.read, daemon=True)
            self.reader.start()

    def read(self):
        for line in self.process.stdout:
            self.log.write(line)
            self.log.flush()
            self.pending.put(line)
        self.pending.put(None)

    def response(self, armed=False):
        if self.session is not None:
            result = self.session.worker_response(armed=armed)
            self.log.write(self.session.last_worker_raw + "\n")
            self.log.flush()
            return result
        try:
            line = self.pending.get(timeout=2)
        except queue.Empty as error:
            raise RuntimeError("Analog acknowledgement exceeded two seconds") from error
        e.check(line is not None, "Analog process exited before acknowledgement")
        return json.loads(line)

    def command(self, text):
        if self.native_exchange and text in ("high", "inspect"):
            self.session.command(text + "-native")
            return self.response(armed=True)
        self.process.stdin.write(text + "\n")
        self.process.stdin.flush()
        return self.response()

    def close(self, normal=False):
        try:
            if normal:
                self.process.stdin.write("quit\n")
                self.process.stdin.flush()
                self.process.wait(timeout=5)
                e.check(self.process.returncode == 0, "Analog shutdown failed")
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait(timeout=5)
            if self.reader is not None:
                self.reader.join(timeout=2)
            for pipe in (self.process.stdin, self.process.stdout):
                try:
                    pipe.close()
                except OSError:
                    # A killed helper may leave an unwritable buffered stdin.
                    pass
            self.log.close()
            self.errors.close()


def wait_file(path, timeout=3):
    deadline = time.monotonic() + timeout
    while not path.exists() and time.monotonic() < deadline:
        failure = Path(str(path).removesuffix(".ready") + ".error")
        e.check(not failure.exists(), failure.read_text() if failure.exists() else "Bridge failed")
        time.sleep(0.01)
    e.check(path.exists(), "CPU acknowledgement deadline expired")
    return path.read_text()


def host_pause(mi, analog, send, directory, previous_ns, rise_ns, guard=None, session=None, native_results=False, native_commands=False, native_debug_results=False, native_grants=False, native_analog=False, native_analog_results=False):
    """Cancel a running paced grant; notify GDB only after analog agreement."""
    path = directory / "async-grant.txt"
    if session and not native_grants:
        session.command("begin 100000000 1")
    if guard:
        guard.begin(100000)
    if native_commands:
        session.start_native(path, 100000000 if native_grants else None)
    else:
        send("emulation StartCancellationProbe 100000 @" + path.as_posix() + " 0 1")
        wait_file(Path(str(path) + ".ready"))
    mi.command("60-exec-continue", "60^running")
    deadline = time.monotonic() + 2
    observations = []
    while True:
        e.check(time.monotonic() < deadline, "Asynchronous progress deadline expired")
        progress = directory / f"async-progress-{len(observations)}.txt"
        if native_debug_results:
            observed = session.debug_reply(progress)["observed_ns"]
        elif native_commands:
            session.command(f'sample-native "{progress.as_posix()}"')
        else:
            send("emulation SampleCancellationProbe @" + progress.as_posix())
        if not native_debug_results:
            observed = int(wait_file(progress, timeout=2))
        observations.append(observed)
        if session and not native_debug_results:
            session.command(f"observe {observed}")
        e.check(observed < 4010750, "ADC breakpoint preceded host pause request")
        if observed > previous_ns:
            break
    started = time.monotonic_ns()
    if guard:
        mi.command("61-exec-interrupt", "61^done", timeout=2)
        e.check(guard.pause_requested.wait(2) and not guard.rejected.is_set(),
                "Direct MI interruption was not retained by the relay")
    if native_commands:
        session.cancel_native(observed if native_grants else None)
    else:
        if native_results:
            session.expect_result(path)
        send("emulation CancelCancellationProbe")
    if native_results:
        values = session.read_result()
    else:
        values = dict(line.split("=", 1) for line in wait_file(path, timeout=2).splitlines())
        for key in ("start", "end", "requested", "unused"):
            values[key] = int(values[key])
        values["sinks"] = [int(x) for x in values["sinks"].split(",")]
    end = values["end"]
    e.check(values["start"] == previous_ns and previous_ns < end < 4010750
            and values["requested"] == 100000000 and values["unused"] > 0
            and end - previous_ns + values["unused"] == values["requested"]
            and values["cancelled"] == "True" and values["reason"] == "cancelled"
            and values["sinks"] == [end], "Asynchronous cancellation accounting failed")
    if session and not native_results:
        session.acknowledge(values)
    if guard:
        guard.complete(values)
    if native_analog:
        session.command("advance-native")
        sample = analog.response()
    else:
        sample = analog.command(f"advance {end}")
    e.check(sample["time_ns"] == end and abs(sample["actual_s"] - end * 1e-9) <= 1e-12,
            "Asynchronous joint time disagreement")
    analytical = 3.3 * (1 - math.exp(-(end - rise_ns) * 1e-9 / 0.001))
    e.check(0 < sample["output_v"] and abs(sample["output_v"] - analytical) <= 1e-5,
            "Asynchronous RC trajectory exceeded tolerance")
    if session and not native_analog_results:
        session.analog(sample)
    notification = directory / "async-notified.txt"
    if native_debug_results:
        session.debug_reply(notification, notification=True)
    elif native_commands:
        session.command(f'notify-native "{notification.as_posix()}"')
    else:
        send(f"emulation NotifyCancellationProbe {end} @" + notification.as_posix())
    if not native_debug_results:
        e.check(wait_file(notification, timeout=2) == "notified", "Joint notification failed")
    if native_commands and not native_debug_results:
        session.command("notified")
    line = mi.stop(timeout=2)
    stop = e.stop_record(mi, line, "signal-received")
    elapsed = time.monotonic_ns() - started
    e.check('signal-name="SIGINT"' in line and stop["time_ns"] == end
            and elapsed <= 2_000_000_000, "Joint GDB acknowledgement failed")
    before = e.inspect_stopped(mi)
    registers = mi.command("93-data-list-register-values x 0 13 14", "93^done")
    time.sleep(0.1)
    e.check(e.inspect_stopped(mi) == before and before["after_ns"] == end
            and mi.command("93-data-list-register-values x 0 13 14", "93^done") == registers
            and analog.command("inspect") == sample, "Joint pause inspection changed")
    if session:
        session.command("commit")
    return {"joint_time_ns": end, "cpu": stop, "grant": values, "analog": sample,
            "progress_observations_ns": observations, "acknowledgement_wall_ns": elapsed,
            "inspection_stable": True, "registers": registers, "mailbox": before["mailbox"],
            "notification": "after-analog-agreement",
            "request": "guarded-mi-interrupt" if guard else "host-cancellation"}


def compare_fixed(actual, reference, variable_pause=False):
    if not variable_pause:
        e.check(actual == reference, "Deterministic checkpoint sequence differs")
        return
    e.check(len(actual) == len(reference) == 9, "Fixed checkpoint count differs")
    for point, baseline in zip(actual, reference):
        for key in ("joint_time_ns", "cpu", "gpio_odr", "mailbox", "registers"):
            e.check(point[key] == baseline[key], "Fixed CPU checkpoint differs: " + key)
        e.check(abs(point["analog"]["output_v"] - baseline["analog"]["output_v"]) <= 1e-5,
                "Fixed analog checkpoint differs")
        e.check(point["exchange"].get("expected_adc_code") == baseline["exchange"].get("expected_adc_code"),
                "Fixed ADC code differs")


def run_case(args, directory, result, reconnect=False):
    renode = e.RenodeProcess(args, directory)
    mi = analog = guard = session = None
    completed = False
    def send(command):
        renode.process.stdin.write(command + "\n")
        renode.process.stdin.flush()
    try:
        analog = Analog(args, directory)
        if getattr(args, "session_runner", None):
            session = SessionContract(args.session_runner, renode.process.stdin if args.native_commands else None,
                                      analog.process.stdin if args.native_analog else None,
                                      analog.process.stdout if args.native_analog_results else None,
                                      args.native / "e05_control.exe" if args.native_adc_process else None,
                                      renode.control_port if args.native_adc_process else None)
            if args.native_readback:
                session.command("enable-inspection-time" if args.native_inspection_time else "enable-inspection-interval" if args.native_inspection_interval else "enable-inspection-request" if args.native_inspection_request else "enable-inspection" if args.native_inspection else "enable-readback-time" if args.native_readback_time else "enable-readback-request" if args.native_readback_request else "enable-readback-mi" if args.native_readback_mi else "enable-readback")
            if args.native_rc:
                session.command("enable-rc")
            if args.native_analog_results:
                analog.session = session
        e.check(analog.response() == {"time_ns": 0, "actual_s": 0, "output_v": 0, "samples": 0},
                "Analog initial state differs")
        send("include @" + (e.HERE / ("cancel_joint_bridge.cs" if args.pause else "cancel_notify_bridge.cs")).as_posix())
        if args.guarded:
            guard = CooperativeGuard(renode.debug_port)
        mi = e.MiSession(args.gdb, args.firmware / "firmware.elf", guard.port if guard else renode.debug_port, directory / "gdb-mi.log")
        e.check(mi.virtual_time_ns() == 0, "Attach advanced time")
        if reconnect:
            initial = e.inspect_stopped(mi)
            zero = {"time_ns": 0, "actual_s": 0, "output_v": 0, "samples": 0}
            e.check(initial["mailbox"] == [0] * 8 and analog.command("inspect") == zero,
                    "Recreated session retained prior digital or analog state")
            gdb_pid = mi.process.pid
            mi.command("3-target-detach", "3^done")
            time.sleep(0.1)
            detached = e.control(args, renode, "time")
            e.check(detached == {"command": "time", "before_us": 0, "after_us": 0},
                    "Detached CPU advanced without a grant")
            e.check(analog.command("inspect") == zero, "Detached circuit advanced")
            mi.command(f"4-target-select remote 127.0.0.1:{guard.port if guard else renode.debug_port}",
                       "4^connected", timeout=20)
            attached = e.inspect_stopped(mi)
            e.check(attached == initial and mi.process.pid == gdb_pid,
                    "Same-GDB reconnect changed the initial CPU state")
            time.sleep(0.1)
            e.check(e.inspect_stopped(mi) == attached and analog.command("inspect") == zero,
                    "Reconnected inspection advanced a recreated engine")
            result["reconnect"] = {"same_gdb_process": True, "initial_cpu": initial,
                "detached_time": detached, "initial_analog": zero,
                "inspection_stable": True}
        for token, location in enumerate(("gpio_change_marker", "*0x08000102", "adc_read_marker", "*0x080001b8"), 10):
            mi.command(f"{token}-break-insert {location}", f"{token}^done")
        if args.steps:
            mi.command("14-break-insert step_target", "14^done")
        boundaries = ((2010875, 2011000, 2011375, 2012125, 2012250,
                       2012500, 2012875, 4010750, 4027000) if args.steps else
                      (2010875, 2011375, 4010750, 4027000))
        edge_index, adc_index = (2, 7) if args.steps else (1, 2)
        expected_pcs = (0x80000f8, 0x80000fa, 0x8000102, 0x8000120,
                        0x8000122, 0x8000126, 0x800012a, 0x8000134, 0x80001b8)
        source_line = 0
        previous_ns = 0
        rise_ns = None
        adc_code = None
        for index, expected in enumerate(boundaries):
            if args.pause and index == adc_index:
                result["async_pause"] = host_pause(mi, analog, send, directory, previous_ns, rise_ns, guard, session, args.native_results, args.native_commands, args.native_debug_results, args.native_grants, args.native_analog, args.native_analog_results)
                previous_ns = result["async_pause"]["joint_time_ns"]
            path = directory / f"grant-{index}.txt"
            if session and not args.native_grants:
                session.command("begin 5000000 0")
            if guard:
                guard.begin(5000)
            if args.native_commands:
                session.start_native(path, 5000000 if args.native_grants else None)
            else:
                send("emulation StartCancellationProbe 5000 @" + path.as_posix())
                wait_file(Path(str(path) + ".ready"))
            token = 20 + index
            action = ("step-instruction" if index == 1 else
                      "next" if index in (4, 5, 6) else "continue") if args.steps else "continue"
            mi.command(f"{token}-exec-{action}", f"{token}^running")
            stop_line = mi.stop()
            stop = e.stop_record(mi, stop_line,
                "breakpoint-hit" if action == "continue" else "end-stepping-range")
            if args.steps:
                e.check(stop["pc"] == expected_pcs[index], "Combined step PC changed")
                if index in (3, 4, 5, 6):
                    line = re.search(r'line="([0-9]+)"', stop_line)
                    e.check(stop["function"] == "step_target" and "firmware.c" in stop_line
                            and line is not None and int(line.group(1)) > source_line,
                            "Step-over source progression changed")
                    source_line = int(line.group(1))
                    stop["source_line"] = source_line
            e.check(stop["time_ns"] == expected, "Firmware breakpoint time changed")
            if session and not args.native_grants:
                session.command(f"observe {stop['time_ns']}")
            if args.fault in ("cpu", "cpu-owner") and index == adc_index:
                result["fault_injected"] = "cpu-exit-before-third-acknowledgement"
                if args.fault == "cpu-owner":
                    from native_process import lose_supervisor_for_test
                    result["owner_loss"] = lose_supervisor_for_test(renode.process)
                result["injected_exit"] = renode.terminate(normal=False)
                if not args.native_commands:
                    raise RuntimeError("Injected CPU loss before cancellation acknowledgement")
            wall = time.monotonic_ns()
            drop_result = args.fault == "result-timeout" and index == adc_index
            if args.native_commands:
                session.cancel_native(stop["time_ns"] if args.native_grants else None)
            else:
                if args.native_results:
                    ingress_path = directory / "withheld-result.txt" if drop_result else path
                    session.expect_result(ingress_path)
                send("emulation CancelCancellationProbe")
            if drop_result:
                # The actual backend output remains untouched as failure evidence.
                result["fault_injected"] = "withheld-third-cancellation-result"
            if args.native_results:
                try:
                    values = session.read_result()
                except RuntimeError:
                    if drop_result:
                        result["result_timeout_wall_ns"] = time.monotonic_ns() - wall
                        result["backend_result_retained"] = path.exists()
                    raise
            else:
                values = dict(line.split("=", 1) for line in wait_file(path).splitlines())
                for key in ("start", "end", "requested", "unused"):
                    values[key] = int(values[key])
                values["sinks"] = [int(x) for x in values["sinks"].split(",")]
            e.check(values["start"] == previous_ns and values["end"] == expected
                    and values["requested"] == 5000000 and values["unused"] > 0
                    and expected - previous_ns + values["unused"] == 5000000
                    and values["cancelled"] == "True" and values["sinks"] == [expected],
                    "CPU cancellation accounting failed")
            e.check(time.monotonic_ns() - wall <= 2_000_000_000, "CPU cancellation exceeded two seconds")
            if session and not args.native_results:
                session.acknowledge(values)
            if guard:
                guard.complete(values)
            if args.fault in ("analog", "analog-owner") and index == adc_index:
                if args.fault == "analog-owner":
                    from native_process import lose_supervisor_for_test
                    result["owner_loss"] = lose_supervisor_for_test(analog.process)
                else:
                    analog.process.kill()
                analog.process.wait(timeout=5)
                result["fault_injected"] = "analog-exit-before-third-boundary"
                result["injected_exit"] = analog.process.returncode
            if args.native_analog:
                session.command("advance-native")
                sample = analog.response()
            else:
                sample = analog.command(f"advance {expected}")
            e.check(sample["time_ns"] == expected and abs(sample["actual_s"] - expected * 1e-9) <= 1e-12,
                    "Joint time agreement failed")
            analytical = 0 if rise_ns is None else 3.3 * (1 - math.exp(-(expected - rise_ns) * 1e-9 / 0.001))
            e.check(abs(sample["output_v"] - analytical) <= 1e-5, "Joint analog trajectory exceeded 10 microvolts")
            if session and not args.native_analog_results:
                session.analog(sample)
            odr = struct.unpack("<I", mi.read_memory(e.GPIO_ODR_ADDRESS, 4))[0]
            e.check(bool(odr & 1) == (index >= edge_index), "Unexpected GPIO state")
            exchange = {}
            if index == edge_index:
                rise_ns = expected
                e.check(analog.command("high") == sample, "GPIO exchange advanced the circuit")
                exchange = {"gpio_high_at_ns": expected}
            if index == adc_index:
                if args.native_adc:
                    deadline = time.monotonic() + 2
                    prepared = session.command("prepare-adc")["adc_request"]
                    microvolts, adc_code = prepared["microvolts"], prepared["expected_code"]
                    remaining = deadline - time.monotonic()
                    e.check(remaining > 0, "ADC preparation deadline expired")
                    if args.native_adc_process:
                        session.command("apply-adc", timeout=remaining)
                        while True:
                            remaining = deadline - time.monotonic()
                            e.check(remaining > 0, "ADC application deadline expired")
                            response = session.command("poll-adc", timeout=remaining)
                            if response["adc_process_status"] == "ready":
                                applied = response["adc_control"]
                                break
                            time.sleep(min(.001, max(0, deadline-time.monotonic())))
                    else:
                        applied = e.control(args, renode, "adc", "0", str(microvolts), timeout=remaining)
                        e.check(applied.get("command") == "adc", "Unexpected ADC control response")
                        remaining = deadline - time.monotonic()
                        e.check(remaining > 0, "ADC application deadline expired")
                        session.command(f"adc-applied 0 {microvolts} {applied['before_us']} {applied['after_us']}", timeout=remaining)
                else:
                    microvolts = int(math.floor(sample["output_v"] * 1e6 + 0.5))
                    adc_code = min(4095, microvolts * 4096 // 3300000)
                    applied = e.control(args, renode, "adc", "0", str(microvolts))
                exchange = {"adc_microvolts": microvolts, "expected_adc_code": adc_code, "control": applied}
            before = e.inspect_stopped(mi)
            registers_before = mi.command("93-data-list-register-values x 0 13 14", "93^done")
            time.sleep(0.1)
            registers_after = mi.command("93-data-list-register-values x 0 13 14", "93^done")
            e.check(registers_before == registers_after, "Joint r0/sp/lr inspection changed")
            after = e.inspect_stopped(mi)
            e.check(before == after and after["after_ns"] == expected, "CPU changed during joint inspection")
            e.check(analog.command("inspect") == sample, "Analog changed during joint inspection")
            if index == len(boundaries) - 1:
                mailbox = after["mailbox"]
                e.check(mailbox == [0x534E3035, 4, 1, 1, adc_code, 1, 0, 0], "Firmware did not commit analog ADC code")
                e.check(adc_code == 3541, "Persistent fixture ADC code changed")
                if args.native_readback:
                    if args.native_readback_request:
                        deadline = time.monotonic() + 1.9
                        request = session.command("arm-readback")["readback_request"]
                        def remaining_readback():
                            remaining = deadline - time.monotonic()
                            e.check(remaining > 0, "Mailbox request deadline expired")
                            return remaining
                        lines = mi.command(request["command"], f"{request['token']}^done", timeout=remaining_readback())
                        replies = [line for line in lines if line.startswith(f"{request['token']}^done")]
                        e.check(len(replies) == 1, "Correlated mailbox reply is missing or ambiguous")
                        if args.native_readback_time:
                            time_lines = mi.command(request["time_command"], f"{request['time_token']}^done", timeout=remaining_readback())
                            streams = [line for line in time_lines if line.startswith('@"[Domain = Elapsed Virtual Time: ')]
                            completions = [line for line in time_lines if line.startswith(f"{request['time_token']}^")]
                            e.check(len(streams) == 1 and len(completions) == 1, "Raw time reply is missing or ambiguous")
                            e.check(e.parse_virtual_time(time_lines) == after["after_ns"], "Correlated mailbox read advanced time")
                            session.command("readback-timed " + " ".join(json.dumps(line) for line in (replies[0], streams[0], completions[0])), timeout=remaining_readback())
                        else:
                            observed_time = mi.virtual_time_ns(timeout=remaining_readback())
                            e.check(observed_time == after["after_ns"], "Correlated mailbox read advanced time")
                            session.command(f"readback-mi {observed_time} " + json.dumps(replies[0]), timeout=remaining_readback())
                    elif args.native_readback_mi:
                        address, count, replies = mi.last_memory_read
                        e.check(address == e.MAILBOX_ADDRESS and count == 32 and len(replies) == 1,
                                "Final raw mailbox reply is missing or ambiguous")
                        session.command(f"readback-mi {after['after_ns']} " + json.dumps(replies[0]))
                    else:
                        session.command("readback " + " ".join(map(str, [after["after_ns"], *mailbox])))
            if args.native_inspection and index == len(boundaries) - 1:
                if args.native_inspection_request:
                    request = session.command("arm-inspection", timeout=remaining_readback())["inspection_request"]
                    for reading in range(2):
                        if reading:
                            if args.native_inspection_interval:
                                while request is None:
                                    released = session.command("release-inspection", timeout=remaining_readback())
                                    request = released.get("inspection_request")
                                    if request is None:
                                        time.sleep(min(released["inspection_wait_ms"] / 1000, remaining_readback()))
                            else:
                                e.check(remaining_readback() > .1, "Insufficient budget for register observation")
                                time.sleep(.1)
                        lines = mi.command(request["command"], f"{request['token']}^done", timeout=remaining_readback())
                        replies = [line for line in lines if line.startswith(f"{request['token']}^")]
                        e.check(len(replies) == 1, "Correlated register reply is missing or ambiguous")
                        if reading and not args.native_inspection_time:
                            e.check(mi.virtual_time_ns(timeout=remaining_readback()) == after["after_ns"], "Register pair advanced time")
                        verified = session.command("inspection-result " + json.dumps(replies[0]), timeout=remaining_readback())
                        request = verified.get("inspection_request")
                    if args.native_inspection_time:
                        time_request = verified["inspection_time_request"]
                        time_lines = mi.command(time_request["command"], f"{time_request['token']}^done", timeout=remaining_readback())
                        streams = [line for line in time_lines if line.startswith('@"[Domain = Elapsed Virtual Time: ')]
                        completions = [line for line in time_lines if line.startswith(f"{time_request['token']}^")]
                        e.check(len(streams) == 1 and len(completions) == 1, "Post-pair time reply is missing or ambiguous")
                        e.check(e.parse_virtual_time(time_lines) == after["after_ns"], "Register pair advanced time")
                        session.command("inspection-time-result " + json.dumps(streams[0]) + " " + json.dumps(completions[0]), timeout=remaining_readback())
                else:
                    first = [line for line in registers_before if line.startswith("93^done")]
                    second = [line for line in registers_after if line.startswith("93^done")]
                    e.check(len(first) == 1 and len(second) == 1, "Final register reply is missing or ambiguous")
                    session.command("verify-inspection " + json.dumps(first[0]) + " " + json.dumps(second[0]), timeout=remaining_readback())
            if session:
                session.command("commit")
            result["checkpoints"].append({"joint_time_ns": expected, "cpu": stop, "grant": values,
                "analog": sample, "gpio_odr": odr, "mailbox": after["mailbox"], "exchange": exchange,
                "registers": registers_after, "inspection_stable": True})
            previous_ns = expected
        analog.close(normal=True)
        if args.process_runner:
            result["analog_process"] = analog.process.evidence()
        result["analog_exit"] = analog.process.returncode
        analog = None
        if guard:
            e.check(not guard.rejected.is_set() and not guard.armed, "Direct guard rejected or retained a grant")
            interrupts = [p for p in guard.records if p.get("interrupt_intercepted")]
            e.check(len(interrupts) == 1 and not interrupts[0]["forwarded"], "Expected one retained MI interruption")
            guard.expected_exit = True
        renode.terminate(normal=True)
        result["renode_exit"] = renode.process.returncode
        completed = True
    finally:
        if guard:
            guard.expected_exit = True
        if analog:
            analog.close()
            result["analog_exit"] = analog.process.returncode
            if args.process_runner:
                result["analog_process"] = analog.process.evidence()
        if renode.process.poll() is None:
            renode.terminate(normal=False)
        if guard:
            guard.close()
            result["guard"] = guard.evidence()
            result["guard_listener_removed"] = not any(p["port"] == guard.port for p in e.sn019.listeners(os.getpid()))
            e.check(result["guard_listener_removed"], "Direct guard listener survived shutdown")
        if mi:
            mi.close()
            result["gdb_exit"] = mi.process.returncode
        result["listeners_removed"] = not e.sn019.listeners(renode.process.pid)
        if args.process_runner:
            result["renode_process"] = renode.process.evidence()
        if session:
            try:
                if not completed:
                    session.command("abort")
            finally:
                session.close()
                result["session_contract"] = session.records
                result["session_contract_exit"] = session.process.returncode
            e.check(session.process.returncode == 0, "C++ session process shutdown failed")
            final = session.records[-1]["snapshot"]
            expected_commits = len(result["checkpoints"]) + int("async_pause" in result)
            e.check(final["commits"] == expected_commits, "C++ joint commit count differs")
            e.check(final["phase"] == (0 if completed else 5), "C++ terminal phase differs")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--renode", type=Path, required=True)
    parser.add_argument("--ide", type=Path, required=True)
    parser.add_argument("--native", type=Path, required=True)
    parser.add_argument("--process-runner", type=Path, help="Opt-in native fixture process supervisor")
    parser.add_argument("--native-adc-process", action="store_true", help="Launch and validate the bounded ADC helper in the native runner")
    parser.add_argument("--native-adc", action="store_true", help="Prepare and confirm the bounded ADC input through native state")
    parser.add_argument("--native-exchange", action="store_true", help="Emit bounded GPIO high and analog inspection through the native runner")
    parser.add_argument("--native-analog-results", action="store_true", help="Read worker stdout exclusively through the native runner")
    parser.add_argument("--native-analog", action="store_true", help="Emit analog catch-up directly from acknowledged native CPU state")
    parser.add_argument("--native-grants", action="store_true", help="Use composite native grant/open and observe/cancel transitions")
    parser.add_argument("--native-debug-results", action="store_true", help="Read complete progress/notification replies in the native helper")
    parser.add_argument("--native-commands", action="store_true", help="Use native start/cancel/progress/notification commands and ready handshake")
    parser.add_argument("--native-results", action="store_true", help="Use C++ cancellation-result ingress")
    parser.add_argument("--session-runner", type=Path, help="Opt-in C++ joint session state gate")
    parser.add_argument("--analog-runner", type=Path, help="Explicit extracted RC worker")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--fault", choices=("analog", "cpu", "result-timeout", "cpu-owner", "analog-owner"))
    parser.add_argument("--steps", action="store_true",
                        help="Include instruction step and three source step-over stops")
    parser.add_argument("--lifecycle", action="store_true",
                        help="Recreate both engines, detach/reconnect, then repeat the step sequence")
    parser.add_argument("--pause", action="store_true",
                        help="Include paced asynchronous host cancellation and joint GDB notification")
    parser.add_argument("--guarded", action="store_true",
                        help="Route all GDB traffic through the cooperative relay and use actual MI interrupt")
    parser.add_argument("--native-rc", action="store_true", help="Validate the bounded RC trajectory before native analog acceptance")
    parser.add_argument("--native-readback", action="store_true", help="Require native final mailbox validation before final commit")
    parser.add_argument("--native-readback-mi", action="store_true", help="Decode the exact raw MI mailbox result natively")
    parser.add_argument("--native-readback-request", action="store_true", help="Require a native mailbox request token and deadline")
    parser.add_argument("--native-readback-time", action="store_true", help="Validate the raw time stream and completion in the native runner")
    parser.add_argument("--native-inspection", action="store_true", help="Validate final register stability before native final commit")
    parser.add_argument("--native-inspection-request", action="store_true", help="Correlate the final register pair with native requests")
    parser.add_argument("--native-inspection-interval", action="store_true", help="Gate second register request on the native 100 ms interval")
    parser.add_argument("--native-inspection-time", action="store_true", help="Require native raw time confirmation after the register pair")
    opts = parser.parse_args()
    if opts.native_inspection_time and not opts.native_inspection_interval:
        parser.error("Native post-pair time requires --native-inspection-interval")
    if opts.native_inspection_interval and not opts.native_inspection_request:
        parser.error("Native observation interval requires --native-inspection-request")
    if opts.native_inspection_request and not opts.native_inspection:
        parser.error("Native register requests require --native-inspection")
    if opts.native_inspection and not opts.native_readback_time:
        parser.error("Native inspection requires --native-readback-time")
    if opts.native_readback_time and not opts.native_readback_request:
        parser.error("Native raw time requires --native-readback-request")
    if opts.native_readback_request and not opts.native_readback_mi:
        parser.error("Native mailbox request requires --native-readback-mi")
    if opts.native_readback_mi and not opts.native_readback:
        parser.error("Native raw readback requires --native-readback")
    if opts.native_readback and not opts.native_adc:
        parser.error("Native readback requires --native-adc")
    if opts.native_rc and not opts.native_exchange:
        parser.error("Native RC validation requires --native-exchange")
    if opts.fault in ("cpu-owner", "analog-owner") and not opts.process_runner:
        parser.error("Supervisor-loss injection requires --process-runner")
    if opts.native_adc_process and not opts.native_adc:
        parser.error("Native ADC process requires --native-adc")
    if opts.native_adc and not opts.native_exchange:
        parser.error("Native ADC coordination requires --native-exchange")
    if opts.native_exchange and not opts.native_analog_results:
        parser.error("Native exchange requires --native-analog-results")
    if opts.native_analog_results and not opts.native_analog:
        parser.error("Native analog replies require --native-analog")
    if opts.native_analog and not opts.native_grants:
        parser.error("Native analog commands require --native-grants")
    if opts.native_grants and not opts.native_debug_results:
        parser.error("Native grants require --native-debug-results")
    if opts.native_debug_results and not opts.native_commands:
        parser.error("Native debug results require --native-commands")
    if opts.native_commands and (not opts.native_results or opts.fault == "result-timeout"):
        parser.error("Native commands require --native-results and exclude the legacy withheld-path injection")
    if opts.fault == "result-timeout" and not opts.native_results:
        parser.error("Result timeout injection requires --native-results")
    if opts.native_results and not opts.session_runner:
        parser.error("--native-results requires --session-runner")
    if opts.lifecycle and (not opts.steps or opts.fault):
        parser.error("Lifecycle requires --steps and excludes fault injection")
    if opts.steps and opts.fault:
        parser.error("Combined step faults require a separate integration profile")
    if opts.pause and (not opts.steps or opts.fault):
        parser.error("Host pause requires --steps and excludes fault injection")
    if opts.guarded and not opts.pause:
        parser.error("Guarded direct GDB requires the extended pause profile")
    root = e.ROOT
    out = opts.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    args = SimpleNamespace(guarded=opts.guarded, pause=opts.pause, steps=opts.steps, fault=opts.fault, renode=opts.renode.resolve(), prepared=root / "build/sn019/generated",
        firmware=root / "build/sn016/firmware", native=opts.native.resolve(),
        analog_runner=opts.analog_runner.resolve() if opts.analog_runner else None,
        session_runner=opts.session_runner.resolve() if opts.session_runner else None,
        native_results=opts.native_results, native_commands=opts.native_commands, native_debug_results=opts.native_debug_results, native_grants=opts.native_grants, native_analog=opts.native_analog, native_analog_results=opts.native_analog_results, native_exchange=opts.native_exchange, native_adc=opts.native_adc, native_adc_process=opts.native_adc_process, native_rc=opts.native_rc, native_readback=opts.native_readback, native_readback_mi=opts.native_readback_mi, native_readback_request=opts.native_readback_request, native_readback_time=opts.native_readback_time, native_inspection=opts.native_inspection, native_inspection_request=opts.native_inspection_request, native_inspection_interval=opts.native_inspection_interval, native_inspection_time=opts.native_inspection_time,
        process_runner=opts.process_runner.resolve() if opts.process_runner else None,
        ngspice_dll=root / "build/deps/ngspice/Spice64_dll/dll-vs/ngspice.dll",
        audio=root / "build/deps/ngspice-console/Spice64/bin", initialization=root / "tools/backend_probe/initialization",
        gdb=next((opts.ide / "plugins").glob("com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32*/tools/bin/arm-none-eabi-gdb.exe")))
    report = {"status": "passed", "complete_e05_profile": False,
        "scope": "Plain GDB, held firmware boundaries, persistent RC and boundary-sampled ADC",
        "steps": opts.steps, "lifecycle": opts.lifecycle, "pause": opts.pause,
        "native_inspection_time": opts.native_inspection_time, "native_inspection_interval": opts.native_inspection_interval, "native_inspection_request": opts.native_inspection_request, "native_inspection": opts.native_inspection, "native_readback_time": opts.native_readback_time, "native_readback_request": opts.native_readback_request, "native_readback_mi": opts.native_readback_mi, "native_readback": opts.native_readback, "native_rc": opts.native_rc, "native_adc_process": opts.native_adc_process, "native_adc": opts.native_adc, "native_exchange": opts.native_exchange, "native_analog_results": opts.native_analog_results, "native_analog": opts.native_analog, "native_grants": opts.native_grants, "native_debug_results": opts.native_debug_results, "guarded": opts.guarded, "native_results": opts.native_results, "native_commands": opts.native_commands,
        "pause_request": "guarded-mi-interrupt" if opts.guarded else "host-cancellation" if opts.pause else None,
        "wall_pace_ms": 1 if opts.pause else 0, "fault": opts.fault, "runs": [],
        "sha256": {str(p): e.sha(p) for p in (Path(__file__), e.HERE / "joint_circuit.cpp",
            e.HERE / "run.py", e.HERE / "gdb_guard.py", e.HERE / "cooperative_debug.py", e.HERE / "cancel_notify_bridge.cs", e.HERE / "cancel_joint_bridge.cs", e.HERE.parent / "adc/stm32f103_adc.cs",
            args.renode / "Renode.dll", args.renode / "Infrastructure.dll", args.ngspice_dll,
            args.analog_runner or args.native / "e05_joint_circuit.exe", args.native / "e05_control.exe", args.gdb,
            args.firmware / "firmware.elf")}}
    if args.session_runner:
        report["session_runner"] = str(args.session_runner)
        for p in (args.session_runner, e.HERE / "session_contract.py", root / "src/core/joint_session.hpp",
                  root / "apps/cli/session_contract_main.cpp", root / "src/application/fixture_inspection.hpp", root / "src/application/fixture_adc.hpp", root / "src/application/fixture_analog.hpp", root / "src/application/fixture_execution.hpp", root / "src/application/fixture_session.hpp", root / "src/core/adc_fixture.hpp", root / "src/core/fixture_readback.hpp", root / "src/adapters/gdb/fixture_memory.hpp", root / "src/adapters/gdb/fixture_time.hpp", root / "src/adapters/gdb/fixture_registers.hpp",
                  root / "src/adapters/renode/adc_process.cpp", root / "src/adapters/renode/adc_process.hpp",
                  root / "src/adapters/ngspice/worker_reply.cpp", root / "src/adapters/ngspice/worker_reply.hpp",
                  root / "src/adapters/ngspice/worker_channel.cpp", root / "src/adapters/ngspice/worker_channel.hpp",
                  root / "src/adapters/renode/cancellation_result.cpp",
                  root / "src/adapters/renode/cancellation_result.hpp",
                  root / "src/adapters/renode/control_channel.cpp", root / "src/adapters/renode/control_channel.hpp"):
            report["sha256"][str(p)] = e.sha(p)
    if args.process_runner:
        report["process_runner"] = str(args.process_runner)
        for p in (args.process_runner, e.HERE / "native_process.py", root / "src/platform/windows/fixture_process.cpp"):
            report["sha256"][str(p)] = e.sha(p)
    for repetition in range(1 if opts.fault else 3):
        result = {"checkpoints": []}
        report["runs"].append(result)
        try:
            run_case(args, out / str(repetition), result)
            if opts.lifecycle:
                e.check(result["listeners_removed"] and result["renode_exit"] == 0
                        and result["analog_exit"] == 0 and result["gdb_exit"] == 0,
                        "Prior session resources survived recreation")
                second = {"checkpoints": []}
                result["recreated_session"] = second
                run_case(args, out / str(repetition) / "recreated", second, reconnect=True)
                e.check(second["listeners_removed"] and second["gdb_exit"] == 0,
                        "Recreated session resources survived shutdown")
                compare_fixed(second["checkpoints"], result["checkpoints"], opts.pause)
            e.check(not opts.fault, "Expected analog failure was not observed")
            e.check(result["listeners_removed"], "Renode listeners remain after shutdown")
            compare_fixed(result["checkpoints"], report["runs"][0]["checkpoints"], opts.pause)
        except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
            result["error"] = str(error)
            expected_failure = (opts.fault in ("analog", "analog-owner")
                and result.get("fault_injected") == "analog-exit-before-third-boundary"
                and result.get("injected_exit", 0) != 0 and result.get("analog_exit", 0) != 0
                and result.get("listeners_removed") and len(result["checkpoints"]) == 2)
            expected_failure = expected_failure or (opts.fault in ("cpu", "cpu-owner")
                and result.get("fault_injected") == "cpu-exit-before-third-acknowledgement"
                and result.get("injected_exit", 0) != 0 and result.get("listeners_removed")
                and len(result["checkpoints"]) == 2)
            expected_failure = expected_failure or (opts.fault == "result-timeout"
                and result.get("fault_injected") == "withheld-third-cancellation-result"
                and result.get("backend_result_retained") and result.get("listeners_removed")
                and 1_900_000_000 <= result.get("result_timeout_wall_ns", 0) <= 2_000_000_000
                and len(result["checkpoints"]) == 2)
            if expected_failure and opts.fault in ("cpu-owner", "analog-owner"):
                expected_failure = bool(result.get("owner_loss", {}).get("child_exit_observed"))
            if expected_failure and args.session_runner:
                final = result["session_contract"][-1]["snapshot"]
                expected_failure = (final["phase"] == 5 and final["commits"] == 2
                    and final["acknowledgements"] == (3 if opts.fault in ("analog", "analog-owner") else 2)
                    and result["session_contract_exit"] == 0)
            result["expected_failure_observed"] = bool(expected_failure)
            if not expected_failure:
                report["status"] = "failed"
        (out / "summary.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(repetition, report["status"], result.get("error", ""), flush=True)
        if report["status"] != "passed":
            break
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
