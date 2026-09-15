# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Physical project capture and ownership regression using the native API."""
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

REPO = Path(__file__).resolve().parents[2]
RAW = (REPO / "tests/schema/fixtures/two-rc-project.json").read_bytes()
PROBE = HOOKS = ""


def packet(root, name):
    values = [str(root).encode(), name.encode()]
    return b"".join(struct.pack("<I", len(v)) + v for v in values)


class Boundary(unittest.TestCase):
    def invoke(self, root, name):
        run = subprocess.run([PROBE], input=packet(root, name), capture_output=True, timeout=15)
        result = json.loads(run.stdout)
        self.assertEqual(run.returncode, 1 if "error" in result else 0, run.stderr)
        return result

    def test_invalid_roots_before_io(self):
        for root in ("", ".", "C:relative", "//server/share", "C:/a/../b", "C:/a~1", "C:/a.", "C:/a:stream", "C:/" + "a" * 4094):
            with self.subTest(root=root[:40]):
                result = self.invoke(root, "project.json")
                self.assertEqual((result["stage"], result["error"]), ("physical", "root"))

    def test_invalid_leaves_before_io(self):
        for name in ("", "../p", "a/b", "a\\b", "a:stream", "CON.json", "a~1", "a.", "a ", "a" * 81, "caf\u00e9.json"):
            with self.subTest(name=name):
                result = self.invoke("C:/", name)
                self.assertEqual((result["stage"], result["error"]), ("physical", "path"))

    @unittest.skipIf(os.name == "nt", "Linux platform rejection only")
    def test_unsupported_platform(self):
        self.assertEqual(self.invoke("C:/", "project.json")["error"], "platform")


@unittest.skipUnless(os.name == "nt", "Windows NTFS acquisition profile")
class Physical(Boundary):
    # Only physical cases belong here; boundary cases are collected separately.
    test_invalid_roots_before_io = None
    test_invalid_leaves_before_io = None
    test_unsupported_platform = None

    def setUp(self):
        parent = REPO / "build" / "sn021-acquisition-tests"
        parent.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "project"
        self.root.mkdir()
        self.target = self.root / "project.json"
        self.target.write_bytes(RAW)

    def accepted(self, result, raw=RAW):
        self.assertNotIn("error", result)
        self.assertEqual(bytes.fromhex(result["source_hex"]), raw)
        value = json.loads(raw)["sources"]["topology"]
        self.assertEqual(result["components"], len(value["components"]))
        self.assertEqual(result["circuits"], len(value["circuits"]))
        self.assertGreater(result["spans"], 0)

    def load(self, name="project.json", root=None):
        return self.invoke(root or self.root, name)

    @contextmanager
    def paused(self, phase):
        process = subprocess.Popen([HOOKS, phase], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            process.stdin.write(packet(self.root, "project.json")); process.stdin.flush()
            received = queue.Queue()
            threading.Thread(target=lambda: received.put(process.stdout.readline()), daemon=True).start()
            self.assertEqual(received.get(timeout=10), f"PAUSE {phase}\n".encode())
            yield process
        finally:
            if process.poll() is None: process.kill()
            process.wait(timeout=10)
            for stream in (process.stdin, process.stdout, process.stderr): stream.close()

    def resume(self, process):
        stdout, stderr = process.communicate(b"!", timeout=20)
        result = json.loads(stdout)
        self.assertEqual(process.returncode, 1 if "error" in result else 0, stderr)
        return result

    def junction(self, path, target):
        run = subprocess.run(["cmd", "/c", "mklink", "/J", str(path), str(target)], capture_output=True)
        self.assertEqual(run.returncode, 0, run.stderr)
        self.addCleanup(lambda: path.rmdir() if path.exists() else None)

    def test_exact_source_unicode_root_case_and_leaf_bounds(self):
        root = self.base / "caf\u00e9"; root.mkdir()
        raw = b" \r\n" + RAW + b"\t\n"
        for name in ("p", "a" * 80, "PROJECT.JSON"):
            (root / name).write_bytes(raw)
            self.accepted(self.load(name.lower(), root), raw)
        self.assertEqual(len(list(root.iterdir())), 3)

    def test_missing_resources_are_not_opened(self):
        self.accepted(self.load())
        self.assertEqual({p.name for p in self.root.iterdir()}, {"project.json"})

    def test_exact_byte_limit_and_over_limit(self):
        raw = RAW + b" " * (1024 * 1024 - len(RAW))
        self.target.write_bytes(raw); self.accepted(self.load(), raw)
        self.target.write_bytes(raw + b" ")
        self.assertEqual(self.load()["error"], "budget")

    def test_empty_malformed_and_invalid_project(self):
        for raw in (b"", b"{", b'{}', b'{"x":1,"x":2}', RAW.replace(b'"unconfigured"', b'"rollback"')):
            self.target.write_bytes(raw)
            result = self.load()
            self.assertEqual(result["stage"], "declaration")
            self.assertNotIn("source_hex", result)
            self.assertEqual(self.target.read_bytes(), raw)

    def test_missing_file_root_and_directory(self):
        self.assertEqual(self.load("missing.json")["stage"], "physical")
        self.assertEqual(self.load(root=self.base / "missing")["stage"], "physical")
        (self.root / "directory").mkdir()
        self.assertEqual(self.load("directory")["error"], "type")
        self.assertEqual(self.load(root=self.target)["error"], "type")

    def test_hardlink_rejected(self):
        os.link(self.target, self.root / "alias.json")
        self.assertEqual(self.load()["error"], "alias")
        self.assertEqual(self.load("alias.json")["error"], "alias")

    def test_existing_writer_blocks_capture(self):
        with self.target.open("r+b"):
            self.assertEqual(self.load()["stage"], "physical")
        self.accepted(self.load())

    def test_read_only_document_needs_no_write_access(self):
        self.target.chmod(0o444)
        try: self.accepted(self.load())
        finally: self.target.chmod(0o666)

    def test_destination_junction_rejected(self):
        self.junction(self.root / "entry.json", self.base)
        self.assertEqual(self.load("entry.json")["error"], "reparse")

    def test_root_and_ancestor_junctions_rejected(self):
        alias = self.base / "alias"; self.junction(alias, self.root)
        self.assertEqual(self.load(root=alias)["error"], "reparse")
        child = self.root / "child"; child.mkdir(); (child / "project.json").write_bytes(RAW)
        self.assertEqual(self.load(root=alias / "child")["error"], "reparse")

    def test_file_symlink_rejected(self):
        link = self.root / "link.json"
        try: link.symlink_to(self.target)
        except OSError as error:
            if error.winerror == 1314: self.skipTest("Symlink privilege unavailable")
            raise
        self.assertEqual(self.load("link.json")["error"], "reparse")

    def test_handles_block_mutation_and_root_rename(self):
        for phase in ("opened", "captured"):
            with self.paused(phase) as process:
                for action in (lambda: self.target.write_bytes(b"tamper"), self.target.unlink,
                               lambda: self.target.rename(self.root / "moved.json"),
                               lambda: self.root.rename(self.base / "moved-root")):
                    with self.assertRaises(OSError): action()
                self.accepted(self.resume(process))

    def test_entry_changed_before_open_is_validated(self):
        with self.paused("root") as process:
            self.target.write_bytes(b"{}")
            self.assertEqual(self.resume(process)["stage"], "declaration")

    def test_owned_capture_survives_post_close_replacement(self):
        with self.paused("released") as process:
            self.target.unlink(); self.target.write_bytes(b"different owner")
            self.accepted(self.resume(process))
        self.assertEqual(self.target.read_bytes(), b"different owner")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(); parser.add_argument("--probe", required=True); parser.add_argument("--hooks", default="")
    args = parser.parse_args(); PROBE, HOOKS = args.probe, args.hooks
    suite = unittest.TestSuite(unittest.defaultTestLoader.loadTestsFromTestCase(t) for t in (Boundary, Physical))
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(0 if result.wasSuccessful() else 1)
