"""Adversarial identity tests plus a real Windows parent/child smoke test."""
import os
import subprocess
import sys
import unittest

from process_memory import ProcessTreeSampler


class Backend:
    def __init__(self):
        self.data = {10: dict(created=100, exited=0), 20: dict(created=200, exited=0)}
        self.entries = [dict(pid=10, parent_pid=1, name="root"), dict(pid=20, parent_pid=10, name="child")]
        self.closed = []
        self.fail = set()

    def open(self, pid):
        if pid in self.fail:
            raise OSError("process exited or access denied")
        return pid

    def times(self, handle):
        return self.data[handle]

    def close(self, handle):
        self.closed.append(handle)

    def snapshot(self):
        return 500, self.entries

    def memory(self, handle):
        return dict(working_set=1000, private_bytes=500)


class IdentityTests(unittest.TestCase):
    def sample(self, backend):
        sampler = ProcessTreeSampler(backend)
        self.addCleanup(sampler.close)
        return sampler, sampler(10)

    def test_stale_parent_pid_does_not_capture_older_process_or_descendants(self):
        b = Backend()
        b.data[20]["created"] = 50
        b.entries.append(dict(pid=30, parent_pid=20, name="external-child"))
        b.data[30] = dict(created=300, exited=0)
        _, row = self.sample(b)
        self.assertEqual([p["pid"] for p in row["processes"]], [10])
        self.assertEqual(row["rejected"][0]["reason"], "child-not-newer-than-parent")

    def test_pid_reused_after_snapshot_is_rejected(self):
        b = Backend()
        b.data[20]["created"] = 501
        _, row = self.sample(b)
        self.assertEqual(row["rejected"][0]["reason"], "created-after-snapshot")

    def test_equal_creation_time_is_conservatively_rejected(self):
        b = Backend()
        b.data[20]["created"] = 100
        _, row = self.sample(b)
        self.assertEqual(len(row["processes"]), 1)

    def test_process_exit_during_open_does_not_add_identity(self):
        b = Backend()
        b.fail.add(20)
        _, row = self.sample(b)
        self.assertEqual(len(row["errors"]), 1)
        self.assertEqual(row["retained_handles"], 1)

    def test_child_discovered_after_parent_exit_within_lifetime(self):
        b = Backend()
        sampler, _ = self.sample(b)
        b.data[20]["exited"] = 400
        b.entries.append(dict(pid=30, parent_pid=20, name="grandchild"))
        b.data[30] = dict(created=350, exited=0)
        row = sampler(10)
        self.assertEqual({p["pid"] for p in row["processes"]}, {10, 30})
        self.assertEqual(row["processes"][-1]["parent_created"], 200)

    def test_child_after_parent_exit_rejected(self):
        b = Backend()
        b.data[10]["exited"] = 150
        _, row = self.sample(b)
        self.assertEqual(row["rejected"][0]["reason"], "child-created-after-parent-exit")

    def test_close_releases_all_handles(self):
        b = Backend()
        sampler, _ = self.sample(b)
        sampler.close()
        self.assertEqual(sorted(b.closed), [10, 20])

    def test_name_does_not_establish_membership(self):
        b = Backend()
        b.entries[1]["name"] = "arbitrary-name"
        _, row = self.sample(b)
        self.assertEqual(len(row["processes"]), 2)

    @unittest.skipUnless(os.name == "nt", "Windows API integration")
    def test_real_child_identity_and_exit(self):
        sampler = ProcessTreeSampler()
        child = subprocess.Popen([sys.executable, "-c", "import sys; sys.stdin.read()"], stdin=subprocess.PIPE,
                                 creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            row = sampler(os.getpid())
            member = next(p for p in row["processes"] if p["pid"] == child.pid)
            self.assertEqual(member["parent_pid"], os.getpid())
            self.assertGreater(member["created"], member["parent_created"])
            child.communicate(timeout=5)
            self.assertNotIn(child.pid, {p["pid"] for p in sampler(os.getpid())["processes"]})
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()
            sampler.close()


if __name__ == "__main__":
    unittest.main()
