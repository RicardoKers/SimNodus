"""Experiment-only, fail-closed RSP gate. It is not a local-process sandbox."""
from __future__ import annotations

import re
import select
import socket
import threading
import time
from collections import deque

MAX_FRAME = 16 * 1024
FRAME_DEADLINE = 2.0


class Rejected(ValueError):
    pass


class Frames:
    """Keep packets private until their entire checksum has been validated."""
    def __init__(self):
        self.buffer = bytearray()
        self.started = None

    def feed(self, data: bytes, now: float):
        for value in data:
            if not self.buffer:
                if value in (43, 45):
                    yield bytes([value]), None
                    continue
                if value == 3:
                    raise Rejected("interrupt cannot establish joint pause")
                if value != 36:
                    raise Rejected("unexpected RSP framing byte")
                self.started = now
            elif value in (36, 125, 42):
                raise Rejected("nested, escaped or run-length encoded request is unsupported")
            self.buffer.append(value)
            if len(self.buffer) > MAX_FRAME:
                raise Rejected("RSP frame exceeds 16 KiB")
            marker = self.buffer.find(b"#")
            if marker >= 0 and len(self.buffer) == marker + 3:
                frame = bytes(self.buffer)
                try:
                    if not re.fullmatch(b"[0-9a-fA-F]{2}", frame[-2:]):
                        raise ValueError("not two hex digits")
                    checksum = int(frame[-2:], 16)
                except ValueError as error:
                    raise Rejected("invalid checksum digits") from error
                payload = frame[1:marker]
                if sum(payload) % 256 != checksum:
                    raise Rejected("RSP checksum mismatch")
                self.buffer.clear()
                self.started = None
                yield frame, payload

    def expire(self, now: float):
        if self.started is not None and now - self.started > FRAME_DEADLINE:
            raise Rejected("incomplete RSP frame deadline expired")


def authorize(payload: bytes, armed: bool):
    text = payload.decode("ascii", errors="strict")
    if text in ("c", "s", "vCont;s:1;c") or re.fullmatch(r"vCont;[cs](?::(?:-1|0|1|p1\.(?:-1|0|1)))?", text):
        if not armed:
            raise Rejected("execution requires the host's pending grant")
        return
    if text in ("D", "D;1"):
        if armed:
            raise Rejected("detach during a pending grant is unsupported")
        return
    if text.startswith("qRcmd,"):
        if text == "qRcmd," + b"machine ElapsedVirtualTime".hex():
            return
        raise Rejected("monitor command is outside the read-only allowlist")
    if text in ("?", "g", "qC", "qAttached", "qAttached:1", "qOffsets", "qTStatus",
                "qfThreadInfo", "qsThreadInfo", "qSymbol::", "vCont?", "vMustReplyEmpty",
                "QStartNoAckMode", "QNonStop:0", "qSupported", "qP0000001f0000000000000001"):
        return
    if re.fullmatch(r"qSupported:[A-Za-z0-9:;=+_.\-]*", text):
        return
    if re.fullmatch(r"H[gc](?:-1|0|1|p1\.(?:-1|0|1))", text):
        return
    if re.fullmatch(r"T(?:1|p1\.1)|p[0-9a-f]+", text):
        return
    memory = re.fullmatch(r"m([0-9a-f]+),([0-9a-f]+)", text)
    if memory:
        address, size = (int(value, 16) for value in memory.groups())
        if 0 < size <= 16384 and any(low <= address and address + size <= high for low, high in
                ((0, 0x10000), (0x08000000, 0x08010000), (0x20000000, 0x20005000), (0x4001080c, 0x40010810))):
            return
        raise Rejected("memory read outside side-effect-free fixture ranges")
    if re.fullmatch(r"qXfer:(?:features|threads|memory-map):read:[A-Za-z0-9_.\-]*:[0-9a-f]+,[0-9a-f]+", text):
        return
    # Legacy read-only thread enumeration observed during the real GDB attach.
    if re.fullmatch(r"qL[01][0-9a-f]{18}", text):
        return
    if re.fullmatch(r"qThreadExtraInfo,(?:1|p1\.1)", text):
        return
    match = re.fullmatch(r"[Zz][01],([0-9a-f]+),([124])", text)
    if match and 0x08000000 <= int(match[1], 16) and int(match[1], 16) + int(match[2]) <= 0x08010000:
        return
    raise Rejected("packet is outside the read/fixture-breakpoint/authorized-step allowlist")


class Guard:
    def __init__(self, upstream_port: int):
        self.server = socket.socket()
        self.server.bind(("127.0.0.1", 0))
        self.server.listen(1)
        self.server.settimeout(0.1)
        self.port = self.server.getsockname()[1]
        self.upstream_port = upstream_port
        self.armed = False
        self.expected_exit = False
        self.closed = threading.Event()
        self.rejected = threading.Event()
        self.diagnostic = None
        self.records = deque(maxlen=2048)
        self.thread = threading.Thread(target=self._serve, daemon=True)
        self.thread.start()

    def arm(self, duration_us: int):
        if self.armed or self.closed.is_set() or self.rejected.is_set() or duration_us != 5000:
            raise Rejected("only one 5000 us grant may be authorized per guarded session")
        self.armed = True
        self.records.append({"host_grant_us": duration_us})

    def fail(self, reason, payload=None):
        self.diagnostic = {"reason": str(reason), "payload": payload.hex() if payload else None,
                           "forwarded": False, "committed": False}
        self.records.append(self.diagnostic)
        self.rejected.set()
        # Retain the upstream socket until the host destroys Renode. An early
        # socket close could otherwise release the CPU inside its pending grant.
        self.closed.wait()

    def _serve(self):
        try:
            while not self.closed.is_set():
                try:
                    client, _ = self.server.accept()
                except socket.timeout:
                    continue
                with client, socket.create_connection(("127.0.0.1", self.upstream_port), timeout=2) as upstream:
                    client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    upstream.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
                    client.settimeout(1)
                    upstream.settimeout(1)
                    self._relay(client, upstream)
        except OSError as error:
            if not self.closed.is_set() and not self.expected_exit:
                self.fail(error)

    def _relay(self, client, upstream):
        frames = Frames()
        while not self.closed.is_set():
            try:
                frames.expire(time.monotonic())
                readable, _, _ = select.select([client, upstream], [], [], 0.05)
                for source in readable:
                    data = source.recv(4096)
                    if not data:
                        if self.armed and not self.expected_exit:
                            self.fail("connection lost during pending grant")
                        return
                    if source is upstream:
                        client.sendall(data)
                    else:
                        for frame, payload in frames.feed(data, time.monotonic()):
                            if payload is not None:
                                try:
                                    authorize(payload, self.armed)
                                except (Rejected, UnicodeError) as error:
                                    self.fail(error, payload)
                                    return
                                self.records.append({"forwarded": True, "packet": payload.decode("ascii")})
                            upstream.sendall(frame)
            except Rejected as error:
                self.fail(error)
                return

    def close(self):
        self.closed.set()
        self.server.close()
        self.thread.join(timeout=3)
        if self.thread.is_alive():
            raise RuntimeError("GDB guard did not terminate")

    def evidence(self):
        return {"armed": self.armed, "diagnostic": self.diagnostic, "packets": list(self.records)}
