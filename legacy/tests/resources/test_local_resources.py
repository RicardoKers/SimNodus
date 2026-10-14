"""SN-021 real filesystem adversarial checks, separate from engine acceptance."""
import copy
from contextlib import ExitStack
import ctypes as c
from dataclasses import FrozenInstanceError
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import local_resources as local
import resource_lock as lock
import topology as t

REPO = Path(__file__).resolve().parents[2]


def inventory(entries):
    files = [dict(id=f"f{i}", path=path, bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
             for i, (path, data) in enumerate(entries)]
    return dict(format="simnodus-resource-lock", version="0.1", dependencies=[dict(
        id="owned", version="0.1.0", origin=dict(kind="owned", author="SimNodus",
        reference="Owned adversarial fixture", revision="SN-021"),
        license=dict(identifier="MIT", notice="f0"), files=files,
        content_sha256=lock.content_hash(files))])


def encode(document):
    value = copy.deepcopy(document)
    for dependency in value["dependencies"]:
        dependency["content_sha256"] = lock.content_hash(dependency["files"])
    return json.dumps(value).encode()


class InputBoundaryTests(unittest.TestCase):
    def test_invalid_metadata_precedes_os_access(self):
        for raw in (b"{}", b'{"version":1,"version":2}', b" " * (t.MAX_BYTES + 1)):
            with self.subTest(raw=raw[:30]), patch.object(local, "WindowsFiles", side_effect=AssertionError):
                with self.assertRaises(t.Invalid):
                    local.verify(raw, "C:/root")

    def test_root_syntax_before_os_access(self):
        raw = encode(inventory([("a", b"a")]))
        for root in (".", "C:foo", "//server/share", "\\\\?\\C:\\root", "C:/a/../b", "C:/a//b",
                     "C:/a~1", "C:/a.", "C:/a ", "C:/NUL", "C:/a:b", "C:/bad\ud800", "C:/" + "a/" * 65):
            with self.subTest(root=root), patch.object(local, "WindowsFiles", side_effect=AssertionError):
                with self.assertRaises(t.Invalid) as error:
                    local.verify(raw, root)
                self.assertEqual(error.exception.code, "root")

    def test_mutable_input_rejected(self):
        with self.assertRaises(t.Invalid):
            local.verify(bytearray(b"{}"), "C:/root")

    @unittest.skipIf(os.name == "nt", "Non-Windows rejection")
    def test_unsupported_platform_fails_closed(self):
        with self.assertRaises(t.Invalid) as error:
            local.verify(encode(inventory([("a", b"a")])), "C:/root")
        self.assertEqual(error.exception.code, "platform")


@unittest.skipUnless(os.name == "nt", "Requires real Windows NTFS")
class LocalResourceTests(unittest.TestCase):
    def setUp(self):
        parent = REPO / "build" / "sn021-tests"
        parent.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / "project"
        self.root.mkdir()
        (self.root / "assets").mkdir()
        self.path = self.root / "assets" / "data.bin"
        self.path.write_bytes(b"owned bytes\x00\xff\r\n")
        self.doc = inventory([("assets/data.bin", self.path.read_bytes())])

    def verify(self):
        return local.verify(encode(self.doc), str(self.root))

    def rejected(self, code=None):
        with self.assertRaises(t.Invalid) as error:
            self.verify()
        if code:
            self.assertEqual(error.exception.code, code)

    def junction(self, name, target):
        result = subprocess.run(["cmd.exe", "/d", "/c", "mklink", "/J", str(name), str(target)],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        # Remove the junction itself, never recurse through it.
        self.addCleanup(os.rmdir, name)

    def symlink(self, name, target, directory=False):
        try:
            os.symlink(target, name, target_is_directory=directory)
        except OSError as error:
            if error.winerror == 1314:
                if os.environ.get("GITHUB_ACTIONS") == "true":
                    self.fail("Hosted Windows acceptance requires symlink privileges")
                self.skipTest("Windows symlink privilege unavailable")
            raise

    def test_owned_existing_lock(self):
        raw = (REPO / "tests/schema/fixtures/owned-resource-lock.json").read_bytes()
        result = local.verify(raw, str(REPO))
        self.assertEqual(len(result), 2)

    def test_project_inventory_and_independent_readiness(self):
        project = json.loads((REPO / "tests/schema/fixtures/two-rc-project.json").read_bytes())
        result = local.verify(encode(project["sources"]["lock"]), str(REPO))
        self.assertEqual(len(result), 3)
        report = local.summary(result)
        self.assertTrue(report["resources_verified"])
        for gate in ("firmware_verified", "origin_verified", "redistribution_verified",
                     "resource_interfaces_verified", "execution_authorized", "simulation_ready",
                     "runtime_profile_verified"):
            self.assertFalse(report[gate])

    def test_exact_bytes_and_immutable_result(self):
        result = self.verify()
        self.assertEqual(result[("owned", "f0")].data, self.path.read_bytes())
        with self.assertRaises(TypeError):
            result[("owned", "f0")] = None
        with self.assertRaises(FrozenInstanceError):
            result[("owned", "f0")].data = b"other"

    def test_zero_byte_file(self):
        self.path.write_bytes(b"")
        self.doc = inventory([("assets/data.bin", b"")])
        self.assertEqual(self.verify()[("owned", "f0")].data, b"")

    def test_distinct_files_with_different_parent_case_spellings(self):
        (self.path.parent / "second.bin").write_bytes(b"second")
        self.doc = inventory([("assets/data.bin", self.path.read_bytes()),
                              ("ASSETS/second.bin", b"second")])
        result = self.verify()
        self.assertEqual(result[("owned", "f0")].data, self.path.read_bytes())
        self.assertEqual(result[("owned", "f1")].data, b"second")

    def test_chunk_boundary_and_maximum_file(self):
        for size in (65535, 65536, 65537, lock.MAX_FILE_BYTES):
            with self.subTest(size=size):
                data = b"x" * size
                self.path.write_bytes(data)
                self.doc = inventory([("assets/data.bin", data)])
                self.assertEqual(self.verify()[("owned", "f0")].data, data)

    def test_missing_file_and_missing_parent(self):
        self.path.unlink()
        self.rejected("filesystem")
        self.path.parent.rmdir()
        self.rejected("filesystem")

    def test_missing_root(self):
        self.root = self.base / "absent"
        self.rejected("filesystem")

    def test_directory_as_file_and_file_as_directory(self):
        self.path.unlink()
        self.path.mkdir()
        self.rejected("type")
        self.path.rmdir()
        self.path.parent.rmdir()
        self.path.parent.write_bytes(b"not directory")
        self.rejected("type")

    def test_size_and_hash_mismatch(self):
        for data, code in ((b"short", "size"), (b"x" * self.path.stat().st_size, "hash")):
            with self.subTest(code=code):
                self.path.write_bytes(data)
                self.rejected(code)

    def test_actual_oversize(self):
        with self.path.open("wb") as stream:
            stream.truncate(lock.MAX_FILE_BYTES + 1)
        self.rejected("budget")

    def test_declared_budgets_before_os(self):
        for count, size in ((257, 0), (5, lock.MAX_FILE_BYTES), (1, lock.MAX_FILE_BYTES + 1)):
            self.doc = inventory([(f"f{i}", b"") for i in range(count)])
            for entry in self.doc["dependencies"][0]["files"]:
                entry["bytes"] = size
            with patch.object(local, "WindowsFiles", side_effect=AssertionError):
                self.rejected("budget")

    def test_exact_total_byte_and_file_count_limits(self):
        data = b"z" * lock.MAX_FILE_BYTES
        for i in range(4):
            (self.root / f"large{i}").write_bytes(data)
        self.doc = inventory([(f"large{i}", data) for i in range(4)])
        self.assertEqual(local.summary(self.verify())["verified_bytes"], lock.MAX_TOTAL_BYTES)
        for i in range(lock.MAX_FILES):
            (self.root / f"empty{i}").write_bytes(b"")
        self.doc = inventory([(f"empty{i}", b"") for i in range(lock.MAX_FILES)])
        self.assertEqual(len(self.verify()), lock.MAX_FILES)

    def test_short_filename_alias_rejected_on_opened_handle(self):
        long = self.root / "long resource filename.bin"
        long.write_bytes(b"owned")
        fs = local.WindowsFiles()
        function = fs.k.GetShortPathNameW
        function.argtypes = [c.c_wchar_p, c.c_wchar_p, c.c_ulong]
        function.restype = c.c_ulong
        buffer = c.create_unicode_buffer(2048)
        self.assertGreater(function(str(long), buffer, len(buffer)), 0)
        alias = Path(buffer.value).name
        if alias.casefold() == long.name.casefold():
            self.skipTest("NTFS short filename generation unavailable on this volume")
        # Bypass lexical rejection here only to test the independent handle/name defense.
        drive, parts = local.root_parts(str(self.root))
        with ExitStack() as stack:
            parent = fs.volume(drive, stack)
            for part in parts:
                parent, _ = fs.child(parent, part, True, stack, "$root")
            with self.assertRaises(t.Invalid) as error:
                fs.child(parent, alias, False, stack, alias)
            self.assertEqual(error.exception.code, "alias")

    def test_hard_link_outside_and_inside(self):
        for target in (self.base / "outside.bin", self.root / "inside.bin"):
            with self.subTest(target=target.name):
                os.link(self.path, target)
                self.rejected("alias")
                target.unlink()
        self.verify()

    def test_file_symlink(self):
        outside = self.base / "outside.bin"
        self.path.rename(outside)
        self.symlink(self.path, outside)
        self.rejected("reparse")

    def test_directory_symlink(self):
        outside = self.base / "outside"
        self.path.parent.rename(outside)
        self.symlink(self.path.parent, outside, True)
        self.rejected("reparse")

    def test_junction_even_when_target_inside(self):
        destination = self.root / "other"
        self.path.parent.rename(destination)
        self.junction(self.path.parent, destination)
        self.rejected("reparse")

    def test_junction_escape_with_matching_prefix(self):
        destination = self.base / "project-escape"
        self.path.parent.rename(destination)
        self.junction(self.path.parent, destination)
        self.rejected("reparse")

    def test_junction_selected_root(self):
        destination = self.base / "elsewhere"
        self.root.rename(destination)
        self.junction(self.root, destination)
        self.rejected("reparse")

    def test_junction_root_ancestor(self):
        alias = self.base / "alias"
        self.junction(alias, self.root)
        self.root = alias / "assets"
        self.rejected("reparse")

    def test_existing_writer_and_exclusive_reader(self):
        fs = local.WindowsFiles()
        for access, share in ((0x40000000, 7), (0x80000000, 0)):
            handle = fs.k.CreateFileW(str(self.path), access, share, None, 3, 0, None)
            self.assertNotEqual(handle, c.c_void_p(-1).value)
            try:
                self.rejected("filesystem")
            finally:
                fs.k.CloseHandle(handle)
        self.verify()

    def test_write_delete_and_ancestor_rename_denied_during_read(self):
        original = local.WindowsFiles.read
        checked = []

        def intercepted(fs, handle, amount, loc):
            if not checked:
                for action in (lambda: self.path.write_bytes(b"changed"), self.path.unlink,
                               lambda: self.path.parent.rename(self.root / "moved"),
                               lambda: self.root.rename(self.base / "moved-root")):
                    with self.assertRaises(OSError):
                        action()
                checked.append(True)
            return original(fs, handle, amount, loc)

        with patch.object(local.WindowsFiles, "read", intercepted):
            self.verify()
        self.assertTrue(checked)

    def test_replacement_before_open_is_actually_verified(self):
        original = local.WindowsFiles.child
        replacement = self.root / "replacement.bin"
        replacement.write_bytes(b"x" * self.path.stat().st_size)

        def intercepted(fs, parent, name, directory, stack, loc):
            if not directory:
                os.replace(replacement, self.path)
            return original(fs, parent, name, directory, stack, loc)

        with patch.object(local.WindowsFiles, "child", intercepted):
            self.rejected("hash")

    def test_junction_substitution_before_directory_open(self):
        original = local.WindowsFiles.child
        destination = self.base / "outside"

        def intercepted(fs, parent, name, directory, stack, loc):
            if directory and name == "assets":
                self.path.parent.rename(destination)
                self.junction(self.path.parent, destination)
            return original(fs, parent, name, directory, stack, loc)

        with patch.object(local.WindowsFiles, "child", intercepted):
            self.rejected("reparse")

    def test_snapshot_survives_replacement_after_return(self):
        old = self.path.read_bytes()
        result = self.verify()
        replacement = self.root / "replacement.bin"
        replacement.write_bytes(b"new contents")
        os.replace(replacement, self.path)
        self.assertEqual(result[("owned", "f0")].data, old)
        self.assertNotEqual(self.path.read_bytes(), old)

    def test_failure_releases_all_handles_and_no_partial_result(self):
        self.doc = inventory([("assets/data.bin", self.path.read_bytes()), ("absent", b"x")])
        self.rejected("filesystem")
        self.path.write_bytes(b"reopened")
        self.path.parent.rename(self.root / "renamed")
        self.root.rename(self.base / "released")

    def test_binary_and_script_text_are_only_bytes(self):
        data = b"MZ\x00<script>throw 'must never run'</script>\n.include https://invalid/\n"
        self.path.write_bytes(data)
        self.doc = inventory([("assets/data.bin", data)])
        with patch.object(subprocess, "Popen", side_effect=AssertionError("Unexpected execution")):
            self.assertEqual(self.verify()[("owned", "f0")].data, data)


if __name__ == "__main__":
    unittest.main()
