"""Opt-in cooperative CPU-only debugger gate and host coordinator."""
import select
import threading
import time
from pathlib import Path
from gdb_guard import Guard, Frames, Rejected, authorize


class CooperativeGuard(Guard):
    def __init__(self, port):
        self.pause_requested = threading.Event()
        self.lock = threading.Lock()
        self.generation = 0
        self.request_wall = None
        super().__init__(port)

    def begin(self, duration_us):
        with self.lock:
            if self.armed or self.rejected.is_set() or duration_us not in (100000, 5000):
                raise Rejected("invalid cooperative authorization")
            self.duration_us = duration_us
            self.generation += 1
            self.pause_requested.clear()
            self.armed = True
            self.records.append({"generation": self.generation, "host_grant_us": duration_us})

    def complete(self, outcome):
        with self.lock:
            if (not self.armed or outcome["requested"] != self.duration_us * 1000
                or not 0 <= outcome["start"] <= outcome["end"]
                or not 0 <= outcome["unused"] <= outcome["requested"]
                or outcome["end"] - outcome["start"] + outcome["unused"] != outcome["requested"]
                or not outcome["sinks"] or any(x != outcome["end"] for x in outcome["sinks"])):
                raise Rejected("invalid cooperative completion evidence")
            self.armed = False
            self.records.append({"generation": self.generation, "acknowledged_outcome": outcome})

    def _relay(self, client, upstream):
        frames = Frames()
        while not self.closed.is_set():
            try:
                frames.expire(time.monotonic())
                readable, _, _ = select.select([client, upstream], [], [], 0.01)
                for source in readable:
                    data = source.recv(4096)
                    if not data:
                        if (self.armed or self.pause_requested.is_set()) and not self.expected_exit:
                            self.fail("connection lost during cooperative session")
                        return
                    if source is upstream:
                        client.sendall(data)
                        continue
                    for value in data:
                        if value == 3 and not frames.buffer:
                            with self.lock:
                                if not self.armed or self.pause_requested.is_set():
                                    raise Rejected("interrupt without an active cooperative grant")
                                self.request_wall = time.monotonic_ns()
                                self.pause_requested.set()
                                self.records.append({"interrupt_intercepted": True, "forwarded": False,
                                                     "generation": self.generation})
                            continue
                        for frame, payload in frames.feed(bytes([value]), time.monotonic()):
                            if payload is not None:
                                with self.lock:
                                    if self.pause_requested.is_set() and payload in (b"D", b"D;1"):
                                        raise Rejected("detach before coordinated session teardown")
                                    authorize(payload, self.armed and not self.pause_requested.is_set())
                                    self.records.append({"forwarded": True, "packet": payload.decode("ascii")})
                            upstream.sendall(frame)
            except (Rejected, UnicodeError) as error:
                self.fail(error)
                return
            except OSError as error:
                if not self.closed.is_set() and not self.expected_exit:
                    self.records.append({"transport_error": str(error), "errno": error.errno,
                                         "winerror": getattr(error, "winerror", None)})
                    # Stay inside the socket context until the host stops Renode.
                    self.fail("cooperative transport failure")
                return


class Coordinator:
    def __init__(self, renode, guard, output, bridge):
        self.renode, self.guard, self.output = renode, guard, output
        self.phase = "initial"
        self.evidence = {}
        self.send("include @" + bridge.as_posix())

    def send(self, command):
        self.renode.process.stdin.write(command + "\n")
        self.renode.process.stdin.flush()

    def publish(self, name, text="ready"):
        p = self.output / name
        p.with_suffix(p.suffix + ".tmp").write_text(text, encoding="utf-8")
        p.with_suffix(p.suffix + ".tmp").replace(p)

    def result(self, name):
        p = self.output / name
        if Path(str(p) + ".error").exists():
            raise ValueError(Path(str(p) + ".error").read_text())
        if not p.exists(): return None
        try:
            contents = p.read_text()
        except PermissionError:
            # Windows may briefly deny access during publication. The caller's
            # existing phase deadline still bounds retries; publish no outcome.
            self.evidence["result_read_retries"] = self.evidence.get("result_read_retries", 0) + 1
            return None
        v = dict(line.split("=", 1) for line in contents.splitlines())
        for key in ("start", "end", "requested", "unused"): v[key] = int(v[key])
        v["sinks"] = [int(x) for x in v["sinks"].split(",")]
        if v["end"] - v["start"] + v["unused"] != v["requested"] or not v["sinks"] or any(x != v["end"] for x in v["sinks"]):
            raise ValueError("Cooperative time agreement failed")
        return v

    def poll(self, transcript):
        if self.phase in ("cancelling", "notifying") and time.monotonic_ns() - self.guard.request_wall > 2_000_000_000:
            raise ValueError("Cooperative pause acknowledgement exceeded two seconds")
        if self.phase == "initial" and "REQUEST_COOP_START" in transcript:
            self.guard.begin(100000)
            self.send("emulation StartCancellationProbe 100000 @" + (self.output / "coop-first.txt").as_posix())
            self.phase = "starting"
        if self.phase == "starting" and (self.output / "coop-first.txt.ready").exists():
            self.publish("coop-ready")
            self.phase = "running"
        if self.phase == "running":
            if self.guard.pause_requested.is_set():
                self.send("emulation CancelCancellationProbe")
                self.phase = "cancelling"
            elif (self.output / "coop-first.txt").exists():
                raise ValueError("Initial grant completed before IDE interrupt")
        if self.phase == "cancelling":
            if time.monotonic_ns() - self.guard.request_wall > 2_000_000_000:
                raise ValueError("Cooperative cancellation exceeded two seconds")
            v = self.result("coop-first.txt")
            if v:
                if v["cancelled"] != "True" or not 0 < v["end"] < 100000000 or v["unused"] == 0:
                    raise ValueError("IDE interrupt did not cancel a pending interval")
                self.guard.complete(v)
                self.evidence["pause"] = v
                self.evidence["cancellation_wall_ns"] = time.monotonic_ns() - self.guard.request_wall
                self.send("emulation NotifyCancellationProbe " + str(v["end"]) + " @" + (self.output / "coop-notified").as_posix())
                self.phase = "notifying"
        if self.phase == "notifying" and (self.output / "coop-notified").exists():
            text = (self.output / "coop-notified").read_text()
            if text:
                if text != "notified": raise ValueError(text)
                self.publish("coop-paused", str(self.evidence["pause"]["end"]))
                self.phase = "paused"
        if self.phase == "paused" and "REQUEST_COOP_RESUME" in transcript:
            self.guard.begin(5000)
            self.send("emulation StartCancellationProbe 5000 @" + (self.output / "coop-second.txt").as_posix())
            self.phase = "resuming"
        if self.phase == "resuming" and (self.output / "coop-second.txt.ready").exists():
            self.publish("coop-resume-ready")
            self.phase = "resumed"
        if self.phase == "resumed":
            v = self.result("coop-second.txt")
            if v:
                if v["unused"] or v["cancelled"] != "False" or v["start"] != self.evidence["pause"]["end"] or v["end"] - v["start"] != 5000000:
                    raise ValueError("Fresh cooperative resume failed")
                self.guard.complete(v)
                self.evidence["resume"] = v
                self.publish("coop-complete", str(v["end"]))
                self.phase = "complete"
