"""Opt-in actual-IDE joint pause coordinator for the bounded E-05 fixture."""
import math
import time
from types import SimpleNamespace
from cooperative_debug import Coordinator
from joint_persistent import Analog
import run as e


class JointCoordinator(Coordinator):
    def __init__(self, renode, guard, output, bridge, fault="none", wall_pace_ms=1, breakpoint_first=False, race=False, race_delay_ms=0, steps=False, race_initial_pace=False):
        super().__init__(renode, guard, output, bridge.with_name("cancel_joint_bridge.cs"))
        e.check(wall_pace_ms in (0, 1), "Unsupported wall pacing")
        self.steps = steps
        self.pause_index = 7 if steps else 2
        self.adc_index = 8 if steps else 3
        self.edge_index = 2 if steps else 1
        self.end_count = 10 if steps else 5
        self.boundaries = ({0: 2010875, 1: 2011000, 2: 2011375, 3: 2012125,
                            4: 2012250, 5: 2012500, 6: 2012875, 8: 4010750, 9: 4027000}
                           if steps else {0: 2010875, 1: 2011375, 3: 4010750, 4: 4027000})
        self.delay_progress_pending = None
        self.delay_progress_index = 0
        self.race_delay_ms = race_delay_ms
        self.race = race
        self.race_initial_pace = race_initial_pace
        self.breakpoint_collision = False
        self.breakpoint_first = breakpoint_first
        self.wall_pace_ms = wall_pace_ms
        self.fault = fault
        self.args = SimpleNamespace(native=e.ROOT / "build/sn016/persistent-native/Debug",
            ngspice_dll=e.ROOT / "build/deps/ngspice/Spice64_dll/dll-vs/ngspice.dll",
            audio=e.ROOT / "build/deps/ngspice-console/Spice64/bin",
            initialization=e.ROOT / "tools/backend_probe/initialization")
        self.analog = Analog(self.args, output)
        self.evidence = {"checkpoints": [], "scope": "Bounded actual IDE joint pause after GPIO high",
            "race_initial_pace": race_initial_pace,
            "steps": steps, "wall_pace_ms": wall_pace_ms, "breakpoint_first": breakpoint_first, "race": race, "race_delay_ms": race_delay_ms, "async_observations": []}
        initial = self.analog.response()
        e.check(initial == {"time_ns": 0, "actual_s": 0, "output_v": 0, "samples": 0}, "Analog initial state changed")
        self.evidence["initial_analog"] = initial
        self.index = 0
        self.previous = 0
        self.progress_index = 0
        self.progress_pending = None
        self.phase = "await_start"

    def close(self):
        self.analog.close(normal=self.phase == "complete")
        self.evidence["analog_exit"] = self.analog.process.returncode

    def poll(self, transcript):
        n = self.index
        path = self.output / f"joint-{n}.txt"
        if self.phase == "await_start" and f"REQUEST_JOINT_START_{n}" in transcript:
            if n == self.pause_index:
                self.async_started_wall = time.monotonic_ns()
            self.guard.begin(100000 if n == self.pause_index else 5000)
            pace_ms = 1 if self.race_initial_pace else self.wall_pace_ms
            self.send(f"emulation StartCancellationProbe {100000 if n == self.pause_index else 5000} @" + path.as_posix() + (f" 0 {pace_ms}" if n == self.pause_index else " 0 0"))
            self.phase = "starting"
        if self.phase == "starting" and self.output.joinpath(f"joint-{n}.txt.ready").exists():
            self.publish(f"joint-ready-{n}")
            self.phase = "running"
        if self.phase == "running" and n == self.pause_index and not self.breakpoint_first and not (self.output / "joint-active").exists():
            if self.progress_pending is None:
                self.progress_pending = self.output / f"joint-progress-{self.progress_index}"
                self.send("emulation SampleCancellationProbe @" + self.progress_pending.as_posix())
            elif self.progress_pending.exists():
                observed = int(self.progress_pending.read_text(encoding="utf-8"))
                self.evidence["async_observations"].append({"cpu_time_ns": observed,
                    "host_elapsed_ns": time.monotonic_ns() - self.async_started_wall})
                e.check(observed < 4010750, "CPU reached ADC before asynchronous injection")
                if observed > self.previous:
                    self.publish("joint-active", str(observed))
                self.progress_index += 1
                self.progress_pending = None
        if self.phase in ("running", "backend_ready") and n == self.pause_index and self.fault == "backend" and "REQUEST_JOINT_BACKEND_LOSS" in transcript:
            e.check(self.guard.armed and self.guard.pause_requested.is_set() == self.race
                    and not self.guard.rejected.is_set() and self.renode.process.poll() is None,
                    "CPU loss injection requires an active healthy grant")
            if self.race:
                e.check(self.phase == "backend_ready" and "JOINT_RACE_BREAKPOINT_CONFIRMED" in transcript,
                        "Race CPU loss preceded IDE breakpoint confirmation")
            else:
                self.requested_wall = time.monotonic_ns()
            backend_exit = self.renode.terminate(normal=False)
            e.check(backend_exit != 0, "Injected CPU loss exited normally")
            self.evidence["failure"] = {"kind": "backend", "renode_exit": backend_exit,
                "grant_unacknowledged": True, "grant_start_ns": self.previous,
                "requested_us": 100000, "joint_pause_confirmed": False,
                "observed_cpu_before_loss_ns": 4010750 if self.race else None}
            self.phase = "backend_lost"
        if self.phase == "backend_lost":
            e.check(time.monotonic_ns() - self.requested_wall <= 2_000_000_000,
                    "CPU loss diagnosis exceeded two seconds")
            if self.guard.rejected.is_set():
                e.check(self.guard.armed and self.guard.diagnostic["payload"] is None
                        and self.guard.diagnostic["reason"] in ("connection lost during cooperative session", "cooperative transport failure")
                        and not path.exists(), "CPU loss unexpectedly acknowledged its grant")
                unchanged = self.analog.command("inspect")
                e.check(unchanged == self.evidence["checkpoints"][-1]["analog"],
                        "Analog advanced beyond the last acknowledged CPU boundary")
                failure = self.evidence["failure"]
                failure.update(analog_after_loss=unchanged, diagnostic=dict(self.guard.diagnostic))
                self.analog.process.kill()
                self.analog.process.wait(timeout=5)
                failure["analog_exit"] = self.analog.process.returncode
                self.guard.expected_exit = True
                self.guard.close()
                failure["wall_ns"] = time.monotonic_ns() - self.requested_wall
                self.publish("joint-failed", "Session ended: CPU backend lost during active grant. Start a new session to recover.")
                self.phase = "failed"
            return
        if self.phase in ("running", "disconnect_ready") and n == self.pause_index and self.fault == "disconnect" and self.guard.rejected.is_set():
            e.check(self.guard.armed and self.guard.pause_requested.is_set() == self.race
                    and self.renode.process.poll() is None
                    and self.guard.diagnostic["payload"] is None
                    and self.guard.diagnostic["reason"] in ("connection lost during cooperative session", "cooperative transport failure"),
                    "Unexpected active disconnect evidence")
            if self.race:
                e.check(self.phase == "disconnect_ready" and "JOINT_RACE_BREAKPOINT_CONFIRMED" in transcript,
                        "Debugger loss preceded the observed arbitration breakpoint")
            else:
                self.requested_wall = time.monotonic_ns()
            self.evidence["disconnect_detected"] = {"cpu_backend_alive": True, "grant_armed": True,
                "interrupt_pending": self.guard.pause_requested.is_set()}
            self.send("emulation CancelCancellationProbe")
            self.phase = "cancelling"
        if self.phase == "running":
            requested = ("REQUEST_JOINT_BREAKPOINT_PAUSE" in transcript if self.breakpoint_first else self.guard.pause_requested.is_set()) if n == self.pause_index else f"REQUEST_JOINT_STOP_{n}" in transcript
            if requested:
                self.requested_wall = self.guard.request_wall if n == self.pause_index and not self.breakpoint_first else time.monotonic_ns()
                if n == self.pause_index:
                    self.evidence["interrupt_host_elapsed_ns"] = self.requested_wall - self.async_started_wall
                if n == self.pause_index and self.race_delay_ms:
                    self.send("emulation SampleCancellationProbe @" + (self.output / "race-delay-observed").as_posix())
                    self.phase = "race_delay"
                else:
                    self.send("emulation CancelCancellationProbe")
                    self.phase = "cancelling"
            elif path.exists():
                raise ValueError("Joint grant completed before the requested stop")
        if self.phase == "race_delay":
            e.check(time.monotonic_ns() - self.requested_wall <= 2_000_000_000, "Race delay exceeded deadline")
            observed_path = self.output / "race-delay-observed"
            if observed_path.exists():
                observed = int(observed_path.read_text(encoding="utf-8"))
                e.check(self.previous < observed < 4010750, "Injected delay did not observe CPU before ADC after interrupt")
                self.evidence["cpu_after_retained_interrupt_ns"] = observed
                if self.race_initial_pace:
                    released = self.output / "race-pacing-released"
                    if not self.evidence.get("pacing_release_requested"):
                        self.send("emulation ReleaseCancellationProbePacing @" + released.as_posix())
                        self.evidence["pacing_release_requested"] = True
                    if not released.exists():
                        return
                    e.check(released.read_text(encoding="utf-8") == "released", "Race pacing release failed")
                    self.evidence["pacing_released_after_retained_interrupt"] = True
                if time.monotonic_ns() - self.requested_wall >= self.race_delay_ms * 1_000_000:
                    if self.delay_progress_pending is None:
                        self.delay_progress_pending = self.output / f"race-delay-progress-{self.delay_progress_index}"
                        self.send("emulation SampleCancellationProbe @" + self.delay_progress_pending.as_posix())
                    elif self.delay_progress_pending.exists():
                        reached = int(self.delay_progress_pending.read_text(encoding="utf-8"))
                        e.check(observed <= reached <= 4010750, "Delayed race escaped its breakpoint")
                        self.evidence.setdefault("delay_progress_ns", []).append(reached)
                        if reached == 4010750:
                            self.evidence["injected_delay_wall_ns"] = time.monotonic_ns() - self.requested_wall
                            if self.fault == "backend":
                                self.publish("joint-race-backend-ready", str(reached))
                                self.phase = "backend_ready"
                            elif self.fault == "disconnect":
                                self.publish("joint-race-disconnect-ready", str(reached))
                                self.phase = "disconnect_ready"
                            else:
                                self.send("emulation CancelCancellationProbe")
                                self.phase = "cancelling"
                        self.delay_progress_index += 1
                        self.delay_progress_pending = None
        if self.phase in ("cancelling", "notifying", "backend_ready", "disconnect_ready"):
            e.check(time.monotonic_ns() - self.requested_wall <= 2_000_000_000,
                    "Joint acknowledgement exceeded two seconds")
        if self.phase == "cancelling":
            v = self.result(path.name)
            if v:
                if n == self.pause_index:
                    self.evidence["async_candidate_outcome"] = v
                    self.evidence["cancel_host_elapsed_ns"] = time.monotonic_ns() - self.async_started_wall
                expected = self.boundaries.get(n)
                if n == self.pause_index and self.breakpoint_first:
                    expected = 4010750
                    e.check(not self.guard.pause_requested.is_set(), "Breakpoint-first must not intercept an interrupt")
                if n == self.pause_index and self.race and v["end"] == 4010750:
                    e.check(self.guard.pause_requested.is_set(), "Race requires a retained real interrupt")
                    if not (self.output / "joint-race-candidate").exists():
                        self.publish("joint-race-candidate", str(v["end"]))
                    if "JOINT_RACE_BREAKPOINT_CONFIRMED" not in transcript:
                        return
                    self.breakpoint_collision = True
                    self.evidence["breakpoint_collision"] = True
                    expected = 4010750
                e.check(v["start"] == self.previous and v["cancelled"] == "True" and v["unused"] > 0,
                        "Joint cancellation accounting failed")
                e.check(v["end"] == expected if expected else self.previous < v["end"] < 4010750,
                        "Joint stop escaped the bounded firmware interval")
                self.guard.complete(v)
                if self.fault == "disconnect" and n == self.pause_index:
                    unchanged = self.analog.command("inspect")
                    e.check(unchanged == self.evidence["checkpoints"][-1]["analog"],
                            "Analog advanced during debugger loss")
                    self.evidence["failure"] = {"kind": "disconnect", "cancelled_cpu_grant": v,
                        "diagnostic": dict(self.guard.diagnostic), "joint_pause_confirmed": False,
                        "analog_after_loss": unchanged}
                    self.guard.expected_exit = True
                    self.evidence["failure"]["renode_exit"] = self.renode.terminate(normal=True)
                    self.analog.process.kill()
                    self.analog.process.wait(timeout=5)
                    self.evidence["failure"]["analog_exit"] = self.analog.process.returncode
                    self.guard.close()
                    self.evidence["failure"]["wall_ns"] = time.monotonic_ns() - self.requested_wall
                    self.publish("joint-failed", "Session ended: debugger connection lost during active grant. Start a new session to recover.")
                    self.phase = "failed"
                    return
                if self.fault != "none" and n == self.pause_index:
                    analog_before_loss = None
                    if self.race:
                        e.check(self.breakpoint_collision, "Race fault requires the confirmed breakpoint")
                        analog_before_loss = self.analog.command("inspect")
                        e.check(analog_before_loss == self.evidence["checkpoints"][-1]["analog"],
                                "Analog advanced before race fault injection")
                    if self.fault == "analog":
                        self.analog.process.kill()
                        self.analog.process.wait(timeout=5)
                        e.check(self.analog.process.returncode != 0, "Injected analog loss exited normally")
                    wait_started = time.monotonic_ns()
                    try:
                        self.analog.command("stall" if self.fault == "timeout" else f"advance {v['end']}")
                    except (OSError, ValueError, RuntimeError) as error:
                        waited = time.monotonic_ns() - wait_started
                        alive = self.analog.process.poll() is None
                        if self.fault == "timeout":
                            e.check(alive and str(error) == "Analog acknowledgement exceeded two seconds"
                                    and waited >= 2_000_000_000, "Expected live-worker timeout was not observed")
                            self.analog.process.kill()
                            self.analog.process.wait(timeout=5)
                        self.evidence["failure"] = {"kind": self.fault, "analog_exit": self.analog.process.returncode,
                            "worker_alive_at_failure": alive, "wait_wall_ns": waited,
                            "analog_before_loss": analog_before_loss,
                            "cancelled_cpu_grant": v, "diagnostic": str(error), "joint_pause_confirmed": False}
                    else:
                        raise ValueError("Faulted analog process acknowledged a boundary")
                    self.guard.expected_exit = True
                    self.evidence["failure"]["renode_exit"] = self.renode.terminate(normal=True)
                    self.guard.close()
                    self.evidence["failure"]["wall_ns"] = time.monotonic_ns() - self.requested_wall
                    reason = "analog acknowledgement deadline expired" if self.fault == "timeout" else "analog backend lost before joint pause acknowledgement"
                    self.publish("joint-failed", "Session ended: " + reason + ". Start a new session to recover.")
                    self.phase = "failed"
                    return
                sample = self.analog.command(f"advance {v['end']}")
                e.check(sample["time_ns"] == v["end"] and abs(sample["actual_s"] - v["end"] * 1e-9) <= 1e-12,
                        "Joint clocks disagree")
                analytical = 0 if n <= self.edge_index else 3.3 * (1 - math.exp(-(v["end"] - 2011375) * 1e-9 / 0.001))
                e.check(abs(sample["output_v"] - analytical) <= 1e-5, "Joint RC trajectory exceeds tolerance")
                odr = e.control(self.args, self.renode, "read32", str(e.GPIO_ODR_ADDRESS))["value"]
                e.check(bool(odr & 1) == (n >= self.edge_index), "Unexpected joint GPIO state")
                point = {"time_ns": v["end"], "grant": v, "analog": sample, "gpio_odr": odr}
                if n == self.edge_index:
                    e.check(self.analog.command("high") == sample, "GPIO update advanced analog state")
                if n == self.adc_index or (n == self.pause_index and (self.breakpoint_first or self.breakpoint_collision)):
                    microvolts = int(math.floor(sample["output_v"] * 1e6 + 0.5))
                    point["adc_microvolts"] = microvolts
                    point["adc_code"] = min(4095, microvolts * 4096 // 3300000)
                    e.control(self.args, self.renode, "adc", "0", str(microvolts))
                self.evidence["checkpoints"].append(point)
                self.previous = v["end"]
                if n == self.pause_index and not (self.breakpoint_first or self.breakpoint_collision):
                    self.send(f"emulation NotifyCancellationProbe {v['end']} @" + (self.output / "joint-notified").as_posix())
                    self.phase = "notifying"
                else:
                    if n == self.pause_index:
                        point["stop_kind"] = "breakpoint_collision" if self.breakpoint_collision else "breakpoint_first"
                        self.evidence["pause_wall_ns"] = time.monotonic_ns() - self.requested_wall
                    self.publish(f"joint-stopped-{n}", str(v["end"]))
                    self.phase = "inspecting"
        if self.phase == "notifying" and (self.output / "joint-notified").exists():
            value = (self.output / "joint-notified").read_text(encoding="utf-8")
            if value:
                e.check(value == "notified", value)
                self.evidence["pause_wall_ns"] = time.monotonic_ns() - self.requested_wall
                self.publish(f"joint-stopped-{n}", str(self.previous))
                self.phase = "inspecting"
        if self.phase == "inspecting" and f"JOINT_INSPECTED_{n}" in transcript:
            point = self.evidence["checkpoints"][-1]
            e.check(self.analog.command("inspect") == point["analog"], "Analog moved during IDE inspection")
            point["inspection_stable"] = True
            self.publish(f"joint-inspected-{n}")
            self.index += 2 if n == self.pause_index and (self.breakpoint_first or self.breakpoint_collision) else 1
            self.phase = "complete" if self.index == self.end_count else "await_start"
