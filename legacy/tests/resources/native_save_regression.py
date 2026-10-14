"""Real Windows create-only project persistence, collisions and failure phases."""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import queue
import struct
import subprocess
import tempfile
import threading
import unittest

PROBE = ""
HOOKS = ""
REPO = Path(__file__).resolve().parents[2]
RAW = (REPO / "tests/schema/fixtures/two-rc-project.json").read_bytes()


def field(value):
    raw = value.encode() if isinstance(value, str) else value
    return struct.pack("<I", len(raw)) + raw


def packet(root, filename, raw):
    return field(field("SNODSAV1") + field(str(root)) + field(filename) + field(raw))


@unittest.skipUnless(os.name == "nt", "Windows NTFS persistence profile")
class NativeSave(unittest.TestCase):
    def setUp(self):
        parent = REPO / "build" / "sn021-save-tests"
        parent.mkdir(exist_ok=True, parents=True)
        self.temp = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "project"
        self.root.mkdir()
        self.target = self.root / "project.json"

    def result(self, run):
        value = json.loads(run.stdout)
        self.assertEqual(run.returncode, 1 if "error" in value else 0, run.stderr)
        if "error" in value:
            self.assertFalse(value["cleanup_failed"])
        return value

    def save(self, raw=RAW, root=None, filename="project.json", fail=None):
        command = [HOOKS, fail, "fail"] if fail else [PROBE]
        return self.result(subprocess.run(command, input=packet(root or self.root, filename, raw),
            capture_output=True, timeout=20))

    @contextmanager
    def paused(self, phase="publish", raw=RAW):
        process = subprocess.Popen([HOOKS, phase], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            process.stdin.write(packet(self.root, "project.json", raw))
            process.stdin.flush()
            received = queue.Queue()
            thread = threading.Thread(target=lambda: received.put(process.stdout.readline()), daemon=True)
            thread.start()
            self.assertEqual(received.get(timeout=10), f"PAUSE {phase}\n".encode())
            yield process
        finally:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=10)
            for stream in (process.stdin, process.stdout, process.stderr):
                stream.close()

    def resume(self, process):
        stdout, stderr = process.communicate(b"!", timeout=15)
        return self.result(subprocess.CompletedProcess(process.args, process.returncode, stdout, stderr))

    def junction(self, path, target):
        run = subprocess.run(["cmd", "/c", "mklink", "/J", str(path), str(target)], capture_output=True)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.addCleanup(lambda: path.rmdir() if path.exists() else None)

    def test_exact_bytes_unicode_root_and_no_resource_import(self):
        self.root = self.base / "project caf\u00e9"
        self.root.mkdir()
        raw = b" \r\n" + RAW + b"\t\n"
        result = self.save(raw)
        self.assertEqual(result, dict(saved=True, bytes=len(raw)))
        self.assertEqual((self.root / "project.json").read_bytes(), raw)
        self.assertEqual({p.name for p in self.root.iterdir()}, {"project.json"})
        # The lock references resources that do not exist in this selected root.
        self.assertFalse((self.root / "tests").exists())

    def test_maximum_input_and_filename_limits(self):
        raw = RAW + b" " * (1024 * 1024 - len(RAW))
        name = "a" * 80
        self.assertTrue(self.save(raw, filename=name)["saved"])
        self.assertEqual((self.root / name).read_bytes(), raw)
        self.assertEqual(self.save(raw + b" ", filename="other.json")["error"], "input")
        self.assertEqual(self.save(filename="a" * 81)["error"], "path")
        self.assertEqual({p.name for p in self.root.iterdir()}, {name})
        self.assertTrue(self.save(filename="p")["saved"])
        self.assertEqual((self.root / "p").read_bytes(), RAW)

    def test_existing_file_and_case_alias_unchanged(self):
        self.target.write_bytes(b"original unrelated document")
        for name in ("project.json", "PROJECT.JSON"):
            self.assertIn("error", self.save(filename=name))
            self.assertEqual(self.target.read_bytes(), b"original unrelated document")
        self.assertEqual({p.name for p in self.root.iterdir()}, {"project.json"})

    def test_existing_directory_and_junction_unchanged(self):
        outside = self.base / "outside"
        outside.mkdir()
        (outside / "sentinel").write_bytes(b"outside")
        self.junction(self.target, outside)
        self.assertIn("error", self.save())
        self.assertEqual((outside / "sentinel").read_bytes(), b"outside")
        self.target.rmdir()
        self.target.mkdir()
        self.assertIn("error", self.save())
        self.assertEqual({p.name for p in self.root.iterdir()}, {"project.json"})

    def test_existing_hardlink_unchanged(self):
        outside = self.base / "outside.bin"
        outside.write_bytes(b"outside")
        os.link(outside, self.target)
        self.assertIn("error", self.save())
        self.assertEqual(outside.read_bytes(), b"outside")
        self.assertEqual(self.target.read_bytes(), b"outside")

    def test_existing_file_symlink_unchanged(self):
        outside = self.base / "outside.bin"
        outside.write_bytes(b"outside")
        try:
            self.target.symlink_to(outside)
        except OSError as error:
            if getattr(error, "winerror", None) == 1314:
                self.skipTest("Windows symlink privilege unavailable")
            raise
        self.assertIn("error", self.save())
        self.assertTrue(self.target.is_symlink())
        self.assertEqual(outside.read_bytes(), b"outside")

    def test_root_junction_and_ancestor_rejected(self):
        alias = self.base / "alias"
        self.junction(alias, self.root)
        self.assertEqual(self.save(root=alias)["error"], "reparse")
        child = self.root / "child"
        child.mkdir()
        self.assertEqual(self.save(root=alias / "child")["error"], "reparse")
        self.assertFalse(any(child.iterdir()))
        self.assertEqual({p.name for p in self.root.iterdir()}, {"child"})

    def test_missing_root_and_file_as_root(self):
        self.assertIn("error", self.save(root=self.base / "absent"))
        self.target.write_bytes(b"old")
        self.assertIn("error", self.save(root=self.target))
        self.assertEqual(self.target.read_bytes(), b"old")

    def test_invalid_project_and_paths_have_no_writes(self):
        for raw in (b"{}", b'{"x":1,"x":2}', RAW.replace(b'"unconfigured"', b'"rollback"')):
            self.assertIn("error", self.save(raw))
        for name in ("../outside", "C:/outside", "a/b", "a\\b", "a:stream", "CON.json", "a~1", "a."):
            self.assertEqual(self.save(filename=name)["error"], "path")
        self.assertFalse(any(self.root.iterdir()))

    def test_phase_failures_preserve_original_and_cleanup(self):
        self.target.write_bytes(b"original")
        raw = RAW + b" " * 70000
        for phase in ("root", "created", "chunk", "written", "flushed", "publish"):
            with self.subTest(phase=phase):
                self.assertEqual(self.save(raw, fail=phase)["error"], "injected")
                self.assertEqual(self.target.read_bytes(), b"original")
                self.assertEqual({p.name for p in self.root.iterdir()}, {"project.json"})

    def test_destination_absent_until_commit_and_temporary_locked(self):
        with self.paused() as process:
            self.assertFalse(self.target.exists())
            temporary, = self.root.iterdir()
            self.assertTrue(temporary.name.startswith("sn-save-"))
            for action in (lambda: temporary.write_bytes(b"tamper"), temporary.unlink,
                lambda: temporary.rename(self.root / "moved.tmp"),
                lambda: self.root.rename(self.base / "moved-root")):
                with self.assertRaises(OSError):
                    action()
            self.assertTrue(self.resume(process)["saved"])
        self.assertEqual(self.target.read_bytes(), RAW)
        self.assertEqual({p.name for p in self.root.iterdir()}, {"project.json"})

    def test_concurrent_destination_creation_is_not_overwritten(self):
        with self.paused() as process:
            self.target.write_bytes(b"concurrent owner")
            self.assertIn("error", self.resume(process))
        self.assertEqual(self.target.read_bytes(), b"concurrent owner")
        self.assertEqual({p.name for p in self.root.iterdir()}, {"project.json"})

    def test_two_prepared_writers_publish_only_one(self):
        other = RAW.replace(b'"two-rc-project"', b'"other-project"')
        with self.paused(raw=RAW) as first, self.paused(raw=other) as second:
            self.assertTrue(self.resume(second)["saved"])
            self.assertIn("error", self.resume(first))
        self.assertEqual(self.target.read_bytes(), other)
        self.assertEqual({p.name for p in self.root.iterdir()}, {"project.json"})

    def test_killed_writer_leaves_no_partial_destination(self):
        self.target.write_bytes(b"original")
        with self.paused("chunk", RAW + b" " * 70000) as process:
            process.kill()
            process.wait(timeout=10)
        self.assertEqual(self.target.read_bytes(), b"original")
        leftovers = list(self.root.glob("sn-save-*.tmp"))
        self.assertEqual(len(leftovers), 1)
        self.assertEqual(leftovers[0].stat().st_size, 65536)
        print("Killed writer: original preserved; one 65536-byte orphan temporary retained until fixture cleanup")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    parser.add_argument("--hooks", required=True)
    args = parser.parse_args()
    PROBE, HOOKS = args.probe, args.hooks
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(NativeSave))
    raise SystemExit(0 if result.wasSuccessful() else 1)
