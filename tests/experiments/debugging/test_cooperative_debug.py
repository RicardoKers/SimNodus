"""State and accounting tests for the opt-in cooperative gate."""
import unittest
import socket
import struct
import sys
import time
from pathlib import Path
from unittest.mock import patch
from cooperative_debug import CooperativeGuard, Coordinator
from gdb_guard import Rejected


class CooperativeStateTests(unittest.TestCase):
    def setUp(self):
        self.guard = CooperativeGuard(9)

    def tearDown(self):
        self.guard.close()

    def outcome(self):
        return {"start": 0, "end": 10000, "requested": 100000000,
                "unused": 99990000, "sinks": [10000]}

    def test_one_active_generation(self):
        self.guard.begin(100000)
        with self.assertRaises(Rejected): self.guard.begin(5000)
        self.guard.pause_requested.set()
        self.guard.complete(self.outcome())
        self.assertFalse(self.guard.armed)
        self.guard.begin(5000)
        self.assertEqual(self.guard.generation, 2)
        self.assertFalse(self.guard.pause_requested.is_set())

    def test_reject_false_completion(self):
        self.guard.begin(100000)
        for field, value in [("requested", 5000000), ("unused", -1), ("end", 10001), ("sinks", [9999])]:
            bad = self.outcome()
            bad[field] = value
            with self.assertRaises(Rejected): self.guard.complete(bad)
            self.assertTrue(self.guard.armed)
        self.guard.complete(self.outcome())
        with self.assertRaises(Rejected): self.guard.complete(self.outcome())

    def transport(self):
        self.guard.close()
        server = socket.socket()
        server.bind(("127.0.0.1", 0))
        server.listen(1)
        server.settimeout(2)
        self.addCleanup(server.close)
        self.guard = CooperativeGuard(server.getsockname()[1])
        client = socket.create_connection(("127.0.0.1", self.guard.port), timeout=2)
        self.addCleanup(client.close)
        upstream, _ = server.accept()
        upstream.settimeout(0.1)
        self.addCleanup(upstream.close)
        return client, upstream

    def test_interrupt_is_retained_and_execution_is_blocked(self):
        client, upstream = self.transport()
        self.guard.begin(100000)
        client.sendall(bytes([3]))
        self.assertTrue(self.guard.pause_requested.wait(1))
        with self.assertRaises(socket.timeout): upstream.recv(1)
        client.sendall(b"$c#63")
        self.assertTrue(self.guard.rejected.wait(1))
        with self.assertRaises(socket.timeout): upstream.recv(1)

    def test_non_ascii_frame_fails_closed(self):
        client, upstream = self.transport()
        client.sendall(b"$\xff#ff")
        self.assertTrue(self.guard.rejected.wait(1))
        self.assertIsNotNone(self.guard.diagnostic)
        with self.assertRaises(socket.timeout): upstream.recv(1)

    def test_connection_reset_retains_upstream_until_host_close(self):
        client, upstream = self.transport()
        self.guard.begin(100000)
        linger = struct.pack("HH" if sys.platform == "win32" else "ii", 1, 0)
        client.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, linger)
        client.close()
        self.assertTrue(self.guard.rejected.wait(2))
        self.assertEqual(self.guard.diagnostic["reason"], "cooperative transport failure")
        self.assertTrue(self.guard.armed)
        with self.assertRaises(socket.timeout): upstream.recv(1)
        self.guard.close()
        self.assertEqual(upstream.recv(1), b"")

    def test_unapproved_duration(self):
        for duration in (0, 1000, 100001):
            with self.assertRaises(Rejected): self.guard.begin(duration)
        self.assertFalse(self.guard.armed)

    def test_unreadable_result_never_acknowledges_and_keeps_deadline(self):
        coordinator = Coordinator.__new__(Coordinator)
        coordinator.output = Path("unused-result-directory")
        coordinator.evidence = {}
        coordinator.guard = self.guard
        coordinator.phase = "cancelling"
        self.guard.begin(100000)
        self.guard.request_wall = time.monotonic_ns()
        with patch.object(Path, "exists", side_effect=lambda: False):
            self.assertIsNone(coordinator.result("grant.txt"))
        with patch.object(Path, "exists", side_effect=[False, True]), \
                patch.object(Path, "read_text", side_effect=PermissionError("sharing violation")):
            self.assertIsNone(coordinator.result("grant.txt"))
        self.assertTrue(self.guard.armed)
        self.assertEqual(coordinator.evidence["result_read_retries"], 1)
        self.assertFalse(any("acknowledged_outcome" in p for p in self.guard.records))
        self.guard.request_wall = time.monotonic_ns() - 2_000_000_001
        with self.assertRaisesRegex(ValueError, "exceeded two seconds"):
            coordinator.poll("")
        self.assertTrue(self.guard.armed)


if __name__ == "__main__": unittest.main()
