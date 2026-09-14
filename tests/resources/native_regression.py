"""Real Windows differential and substitution checks for the native snapshot API."""
import argparse
from contextlib import contextmanager
import ctypes as c
import hashlib
import io
import json
import os
from pathlib import Path
import queue
import struct
import subprocess
import tempfile
import threading
from types import MappingProxyType
import unittest

import local_resources as reference
import resource_lock as lock
import test_local_resources as baseline
import topology as t

PROBE = None
HOOK_PROBE = None


def field(value):
    data = value.encode("utf-8") if isinstance(value, str) else value
    return struct.pack("<I", len(data)) + data


def packet(document, root):
    entries = [(dep["id"], row) for dep in document["dependencies"] for row in dep["files"]]
    data = field("SNODRES1") + field(root) + struct.pack("<I", len(entries))
    for dep, row in entries:
        data += field(dep) + field(row["id"]) + field(row["path"])
        data += struct.pack("<Q", row["bytes"]) + field(row["sha256"])
    return field(data)


def decode(output):
    if output.startswith(b"ERR "):
        _, code, index, system = output.decode().strip().split()
        raise t.Invalid(code, index, f"Native verification error ({system})")
    if not output.startswith(b"OK\n"):
        raise AssertionError(f"Invalid native output: {output[:100]!r}")
    stream = io.BytesIO(output[3:])

    def number(width):
        raw = stream.read(width)
        if len(raw) != width:
            raise AssertionError("Truncated native output")
        return int.from_bytes(raw, "little")

    def text():
        length = number(4)
        assert length <= 4096
        raw = stream.read(length)
        assert len(raw) == length
        return raw.decode()

    result = {}
    count = number(4)
    assert 0 < count <= lock.MAX_FILES
    for _ in range(count):
        dep, rid, path, digest = text(), text(), text(), text()
        size = number(8)
        assert size <= lock.MAX_FILE_BYTES
        data = stream.read(size)
        assert len(data) == size and hashlib.sha256(data).hexdigest() == digest
        assert (dep, rid) not in result
        result[(dep, rid)] = reference.Snapshot(path, data, digest, ())
    assert not stream.read(1)
    return MappingProxyType(result)


def native(document, root):
    run = subprocess.run([PROBE], input=packet(document, root), capture_output=True, timeout=30)
    assert run.returncode in (0, 1), run.stderr
    return decode(run.stdout)


@unittest.skipUnless(os.name == "nt", "Native Windows filesystem profile")
class NativeResources(baseline.LocalResourceTests):
    def verify(self):
        raw = baseline.encode(self.doc)
        document = lock.parse(raw)  # Caller metadata gate is explicitly still Python.
        try:
            expected = reference.verify(raw, str(self.root))
        except t.Invalid as error:
            with self.assertRaises(t.Invalid) as result:
                native(document, str(self.root))
            self.assertEqual(result.exception.code, error.code)
            raise
        actual = native(document, str(self.root))
        self.assertEqual(set(actual), set(expected))
        for key in actual:
            self.assertEqual(actual[key].path, expected[key].path)
            self.assertEqual(actual[key].data, expected[key].data)
        return actual

    @contextmanager
    def paused(self, phase):
        process = subprocess.Popen([HOOK_PROBE, phase], stdin=subprocess.PIPE,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        marker = queue.Queue()
        reader = threading.Thread(target=lambda: marker.put(process.stdout.readline()), daemon=True)
        try:
            process.stdin.write(packet(self.doc, str(self.root)))
            process.stdin.flush()
            reader.start()
            self.assertEqual(marker.get(timeout=10), f"PAUSE {phase}\n".encode())
            yield process
        finally:
            if process.poll() is None:
                process.kill()
            if reader.ident is not None:
                reader.join(timeout=10)
            process.communicate(timeout=10)

    def resume(self, process):
        output, errors = process.communicate(b"!", timeout=30)
        self.assertIn(process.returncode, (0, 1), errors)
        return decode(output)

    def test_owned_existing_lock(self):
        raw = (baseline.REPO / "tests/schema/fixtures/owned-resource-lock.json").read_bytes()
        expected = reference.verify(raw, str(baseline.REPO))
        actual = native(lock.parse(raw), str(baseline.REPO))
        self.assertEqual({k: v.data for k, v in actual.items()}, {k: v.data for k, v in expected.items()})

    def test_project_inventory_and_independent_readiness(self):
        project = json.loads((baseline.REPO / "tests/schema/fixtures/two-rc-project.json").read_bytes())
        self.doc = project["sources"]["lock"]
        self.root = baseline.REPO
        self.assertEqual(len(self.verify()), 3)

    def test_write_delete_and_ancestor_rename_denied_during_read(self):
        with self.paused("opened") as process:
            for action in (lambda: self.path.write_bytes(b"changed"), self.path.unlink,
                           lambda: self.path.parent.rename(self.root / "moved"),
                           lambda: self.root.rename(self.base / "moved-root")):
                with self.assertRaises(OSError):
                    action()
            result = self.resume(process)
            self.assertEqual(result[("owned", "f0")].data, self.path.read_bytes())

    def test_replacement_before_open_is_actually_verified(self):
        replacement = self.root / "replacement.bin"
        replacement.write_bytes(b"x" * self.path.stat().st_size)
        with self.paused("root") as process:
            os.replace(replacement, self.path)
            with self.assertRaises(t.Invalid) as error:
                self.resume(process)
            self.assertEqual(error.exception.code, "hash")

    def test_junction_substitution_before_directory_open(self):
        destination = self.base / "outside"
        with self.paused("root") as process:
            self.path.parent.rename(destination)
            self.junction(self.path.parent, destination)
            with self.assertRaises(t.Invalid) as error:
                self.resume(process)
            self.assertEqual(error.exception.code, "reparse")

    def test_replacement_after_capture_before_return(self):
        expected = self.path.read_bytes()
        replacement = self.root / "replacement.bin"
        replacement.write_bytes(b"different")
        with self.paused("captured") as process:
            os.replace(replacement, self.path)
            result = self.resume(process)
            self.assertEqual(result[("owned", "f0")].data, expected)

    def test_binary_and_script_text_are_only_bytes(self):
        marker = self.root / "must-not-exist"
        data = f"MZ\n<script>throw 'untrusted'</script>\nwrite {marker}\n".encode()
        self.path.write_bytes(data)
        self.doc = baseline.inventory([("assets/data.bin", data)])
        self.assertEqual(self.verify()[("owned", "f0")].data, data)
        self.assertFalse(marker.exists())

    def test_short_filename_alias_rejected_on_opened_handle(self):
        """Native long-name success and lexical rejection of an existing 8.3 alias."""
        # The system temporary volume can generate 8.3 names even when D: does not.
        with tempfile.TemporaryDirectory(prefix="sn021-native-") as directory:
            root = Path(directory)
            source = root / "long-resource-filename.bin"
            source.write_bytes(b"owned")
            fs = reference.WindowsFiles()
            function = fs.k.GetShortPathNameW
            function.argtypes = [c.c_wchar_p, c.c_wchar_p, c.c_ulong]
            function.restype = c.c_ulong
            buffer = c.create_unicode_buffer(2048)
            self.assertGreater(function(str(source), buffer, len(buffer)), 0)
            alias = Path(buffer.value).name
            if alias.casefold() == source.name.casefold():
                self.skipTest("No 8.3 alias on system temporary volume")
            self.assertEqual((root / alias).read_bytes(), source.read_bytes())
            accepted = native(baseline.inventory([(source.name, b"owned")]), str(root))
            self.assertEqual(accepted[("owned", "f0")].data, b"owned")
            # Real existing short alias: reject before any physical consumption.
            document = baseline.inventory([(alias, b"owned")])
            with self.assertRaises(t.Invalid) as error:
                native(document, str(root))
            self.assertEqual(error.exception.code, "path")

    def test_case_sensitive_directory_rejected(self):
        # Windows requires an empty directory when enabling this flag.
        saved = self.root / "saved.bin"
        self.path.rename(saved)
        fs = reference.WindowsFiles()
        function = fs.k.SetFileInformationByHandle
        function.argtypes = [c.c_void_p, c.c_int, c.c_void_p, c.c_ulong]
        function.restype = c.c_int
        handle = fs.k.CreateFileW(str(self.path.parent), 0x100, 7, None, 3, 0x02000000, None)
        self.assertNotEqual(handle, c.c_void_p(-1).value)
        try:
            flag = c.c_ulong(1)
            if not function(handle, 23, c.byref(flag), c.sizeof(flag)):
                error = c.get_last_error()
                if error in (5, 50, 87, 1314):
                    self.skipTest(f"Cannot enable directory case sensitivity: Windows error {error}")
                self.fail(f"Unexpected case-policy setup error {error}")
        finally:
            fs.k.CloseHandle(handle)
        saved.rename(self.path)
        with self.assertRaises(t.Invalid) as error:
            native(self.doc, str(self.root))
        self.assertEqual(error.exception.code, "case_sensitive")

    def test_native_typed_input_does_not_trust_caller(self):
        self.doc["dependencies"][0]["files"][0]["path"] = "../outside"
        with self.assertRaises(t.Invalid) as error:
            native(self.doc, str(self.root))
        self.assertEqual(error.exception.code, "path")

    def test_native_transport_rejects_malformed_input(self):
        good = packet(self.doc, str(self.root))
        for raw in (b"", good[:-1], good + b"extra", struct.pack("<I", 1024 * 1024 + 1)):
            with self.subTest(length=len(raw)):
                result = subprocess.run([PROBE], input=raw, capture_output=True, timeout=30)
                self.assertEqual(result.returncode, 1)
                self.assertTrue(result.stdout.startswith(b"ERR input "))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    parser.add_argument("--hooks", required=True)
    args = parser.parse_args()
    PROBE, HOOK_PROBE = args.probe, args.hooks
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(NativeResources)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    raise SystemExit(0 if result.wasSuccessful() else 1)
