"""Adversarial framing and authorization checks, separate from real integration."""
import unittest
from gdb_guard import Frames, Rejected, authorize, MAX_FRAME


def packet(payload):
    return b"$" + payload + b"#" + f"{sum(payload) % 256:02x}".encode()


class GuardTests(unittest.TestCase):
    def test_every_split_retains_complete_packets(self):
        data = packet(b"qSupported:multiprocess+;swbreak+")
        for split in range(1, len(data)):
            frames = Frames()
            self.assertEqual(list(frames.feed(data[:split], 0)), [])
            self.assertEqual(list(frames.feed(data[split:], 0.1)), [(data, data[1:-3])])

    def test_coalesced_ack_and_packets(self):
        frames = Frames()
        data = b"+" + packet(b"g") + packet(b"m20000000,20")
        self.assertEqual([p for _, p in frames.feed(data, 0)], [None, b"g", b"m20000000,20"])

    def test_corrupt_and_ambiguous_frames(self):
        for data in (b"$g#00", b"$g#+1", b"$g#zz", b"$g$g#67", b"$}#7d", b"$*#2a", b"\x03"):
            with self.subTest(data=data), self.assertRaises(Rejected):
                list(Frames().feed(data, 0))

    def test_frame_limits_and_deadline(self):
        with self.assertRaises(Rejected):
            list(Frames().feed(b"$" + b"a" * MAX_FRAME, 0))
        frames = Frames()
        list(frames.feed(b"$m20000000,20", 0))
        frames.expire(1.99)
        with self.assertRaises(Rejected): frames.expire(2.01)

    def test_unsafe_commands_rejected_in_both_states(self):
        for armed in (False, True):
            for payload in (b"R00", b"k", b"bs", b"bc", b"vCont;t", b"vFlashErase:8000000,100",
                            b"M20000000,4:01000000", b"X20000000,1:a", b"P0=1", b"G0000",
                            b"qRcmd," + b"start".hex().encode(), b"qRcmd," + b"reset".hex().encode(),
                            b"m4001244c,4", b"m20004fff,2", b"m20000000,ffffffffffffffff",
                            b"QNonStop:1", b"vCont;s:1;t", b"vCont;c:..--", b"unknown"):
                with self.subTest(payload=payload, armed=armed), self.assertRaises(Rejected):
                    authorize(payload, armed)

    def test_execution_and_detach_are_mutually_restricted(self):
        for payload in (b"c", b"s", b"vCont;c:p1.-1", b"vCont;s:1", b"vCont;s:1;c"):
            with self.assertRaises(Rejected): authorize(payload, False)
            authorize(payload, True)
        authorize(b"D", False)
        with self.assertRaises(Rejected): authorize(b"D", True)

    def test_only_bounded_fixture_reads_and_breakpoints(self):
        for payload in (b"g", b"qP0000001f0000000000000001", b"m0,4", b"m8000000,100", b"m20000000,20", b"m4001080c,4", b"Z0,80000f8,2"):
            authorize(payload, False)
        for payload in (b"qP0000001f0000000000000002", b"m10000,4", b"m20000000,0", b"Z0,20000000,2", b"Z0,800ffff,2"):
            with self.assertRaises(Rejected): authorize(payload, False)


if __name__ == "__main__": unittest.main()
