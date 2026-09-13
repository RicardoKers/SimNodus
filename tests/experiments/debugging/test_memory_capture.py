"""Verify raw MI capture belongs to the latest memory-read call."""
import unittest
from run import MiSession
from unittest.mock import Mock, patch


class MemoryCaptureTests(unittest.TestCase):
    def test_time_query_preserves_remaining_budget(self):
        client = MiSession.__new__(MiSession)
        client.command = Mock(return_value=["reply"])
        with patch("run.parse_virtual_time", return_value=4027000):
            self.assertEqual(client.virtual_time_ns(timeout=.25), 4027000)
        self.assertEqual(client.command.call_args.kwargs, {"timeout": .25})

    def test_exact_line_and_request_are_retained(self):
        client = MiSession.__new__(MiSession)
        raw = '91^done,memory=[{begin="0x20000000",offset="0x00000000",end="0x20000004",contents="01020304"}]'
        client.command = lambda *_: ['~"console text"', raw, '(gdb)']
        self.assertEqual(client.read_memory(0x20000000, 4), bytes([1, 2, 3, 4]))
        self.assertEqual(client.last_memory_read, (0x20000000, 4, [raw]))

    def test_next_read_replaces_previous_capture(self):
        client = MiSession.__new__(MiSession)
        client.last_memory_read = (0x20000000, 32, ['old reply'])
        raw = '91^done,memory=[{contents="00"}]'
        client.command = lambda *_: [raw]
        client.read_memory(0x4001080c, 1)
        self.assertEqual(client.last_memory_read, (0x4001080c, 1, [raw]))

    def test_ambiguous_replies_are_not_silently_selected(self):
        client = MiSession.__new__(MiSession)
        raw = '91^done,memory=[{contents="00"}]'
        client.command = lambda *_: [raw, raw]
        client.read_memory(0x20000000, 1)
        self.assertEqual(client.last_memory_read[2], [raw, raw])


if __name__ == "__main__":
    unittest.main()
