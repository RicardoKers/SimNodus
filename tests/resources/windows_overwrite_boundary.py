# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Physical counterexamples to candidate overwrite protocols; not a save API.

Every mutation is confined to an exclusively created fixture directory. Calls
use Windows system APIs, never a library selected by a project document.
"""
import ctypes as c
from ctypes import wintypes as w
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest

if sys.platform != "win32":
    raise SystemExit("This acceptance probe requires Windows/NTFS")

kernel = c.WinDLL("kernel32", use_last_error=True)
native = c.WinDLL("ntdll")
READ, WRITE, DELETE = 0x80000000, 0x40000000, 0x10000
SHARE_READ, SHARE_DELETE = 1, 4
INVALID = c.c_void_p(-1).value


class IoStatus(c.Structure):
    _fields_ = [("status", c.c_void_p), ("information", c.c_size_t)]


class Rename(c.Structure):
    _fields_ = [("flags", w.DWORD), ("root", w.HANDLE),
                ("length", w.DWORD), ("name", w.WCHAR * 81)]


class FileInfo(c.Structure):
    _fields_ = [("attributes", w.DWORD), ("created", w.FILETIME),
                ("accessed", w.FILETIME), ("written", w.FILETIME),
                ("volume", w.DWORD), ("size_high", w.DWORD),
                ("size_low", w.DWORD), ("links", w.DWORD),
                ("index_high", w.DWORD), ("index_low", w.DWORD)]


kernel.CreateFileW.argtypes = [w.LPCWSTR, w.DWORD, w.DWORD, c.c_void_p,
                              w.DWORD, w.DWORD, w.HANDLE]
kernel.CreateFileW.restype = w.HANDLE
kernel.CloseHandle.argtypes = [w.HANDLE]
kernel.CloseHandle.restype = w.BOOL
kernel.ReadFile.argtypes = [w.HANDLE, c.c_void_p, w.DWORD, c.POINTER(w.DWORD), c.c_void_p]
kernel.ReadFile.restype = w.BOOL
kernel.GetFileInformationByHandle.argtypes = [w.HANDLE, c.POINTER(FileInfo)]
kernel.GetFileInformationByHandle.restype = w.BOOL
kernel.GetVolumeInformationByHandleW.argtypes = [w.HANDLE, w.LPWSTR, w.DWORD,
    c.POINTER(w.DWORD), c.POINTER(w.DWORD), c.POINTER(w.DWORD), w.LPWSTR, w.DWORD]
kernel.GetVolumeInformationByHandleW.restype = w.BOOL
kernel.ReplaceFileW.argtypes = [w.LPCWSTR, w.LPCWSTR, w.LPCWSTR,
                               w.DWORD, c.c_void_p, c.c_void_p]
kernel.ReplaceFileW.restype = w.BOOL
native.NtSetInformationFile.argtypes = [w.HANDLE, c.POINTER(IoStatus),
                                      c.c_void_p, w.ULONG, w.ULONG]
native.NtSetInformationFile.restype = c.c_int32


def opened(path, access=READ, share=SHARE_READ, directory=False):
    flags = 0x00200000 | (0x02000000 if directory else 0)  # no reparse traversal
    handle = kernel.CreateFileW(str(path), access, share, None, 3, flags, None)
    if handle == INVALID:
        raise c.WinError(c.get_last_error())
    return handle


def identity(handle):
    info = FileInfo()
    if not kernel.GetFileInformationByHandle(handle, c.byref(info)):
        raise c.WinError(c.get_last_error())
    return info.volume, info.index_high, info.index_low


def read(handle):
    buffer, amount = c.create_string_buffer(128), w.DWORD()
    if not kernel.ReadFile(handle, buffer, 128, c.byref(amount), None):
        raise c.WinError(c.get_last_error())
    return buffer.raw[:amount.value]


def rename(handle, parent, leaf, posix=False):
    assert 0 < len(leaf) <= 80 and "/" not in leaf and "\\" not in leaf
    request = Rename(3 if posix else 1, parent, len(leaf.encode("utf-16-le")), leaf)
    status = IoStatus()
    # FileRenameInformation=10; FileRenameInformationEx=65.
    return native.NtSetInformationFile(handle, c.byref(status), c.byref(request),
                                       c.sizeof(request), 65 if posix else 10)


class OverwriteBoundary(unittest.TestCase):
    def setUp(self):
        self.fixture = tempfile.TemporaryDirectory(prefix="sn-overwrite-")
        self.root = Path(self.fixture.name)
        self.handles = []
        self.addCleanup(self.fixture.cleanup)
        self.addCleanup(self.close_all)
        self.parent = self.open(self.root, access=0x80, directory=True)
        filesystem = c.create_unicode_buffer(32)
        self.assertTrue(kernel.GetVolumeInformationByHandleW(self.parent, None, 0,
            None, None, None, filesystem, len(filesystem)), c.get_last_error())
        self.assertEqual(filesystem.value, "NTFS")
        self.target = self.root / "project.json"
        self.target.write_bytes(b"original")

    def open(self, path, **kwargs):
        handle = opened(path, **kwargs)
        self.handles.append(handle)
        return handle

    def close(self, handle):
        self.assertTrue(kernel.CloseHandle(handle), c.get_last_error())
        self.handles.remove(handle)

    def close_all(self):
        for handle in self.handles:
            kernel.CloseHandle(handle)
        self.handles.clear()

    def replacement(self, leaf, contents):
        path = self.root / leaf
        path.write_bytes(contents)
        return self.open(path, access=READ | DELETE, share=0)

    def replace_entry(self, leaf, contents, posix=False):
        handle = self.replacement(leaf, contents)
        self.assertEqual(rename(handle, self.parent, "project.json", posix), 0)
        return handle

    def test_retained_no_delete_share_blocks_both_renames(self):
        old = self.open(self.target)
        self.assertEqual(read(old), b"original")
        with self.assertRaises(OSError):
            self.open(self.target, access=WRITE)
        for posix in (False, True):
            with self.subTest(posix=posix):
                replacement = self.replacement(f"new-{int(posix)}.tmp", b"new")
                status = rename(replacement, self.parent, "project.json", posix)
                print(f"Retained no-delete-share, posix={posix}: status=0x{status & 0xffffffff:08x}")
                self.assertLess(status, 0)
                self.close(replacement)
        self.assertEqual(self.target.read_bytes(), b"original")

    def test_close_then_rename_overwrites_concurrent_owner(self):
        old = self.open(self.target)
        expected = identity(old)
        self.assertEqual(read(old), b"original")
        self.close(old)
        concurrent = self.replace_entry("concurrent.tmp", b"concurrent owner")
        self.assertNotEqual(identity(concurrent), expected)
        self.close(concurrent)
        own = self.replace_entry("own.tmp", b"our save")
        self.close(own)
        self.assertEqual(self.target.read_bytes(), b"our save")
        print("Close-then-rename: concurrent owner's entry was overwritten")

    def test_posix_replacement_separates_handle_from_name(self):
        old = self.open(self.target, share=SHARE_READ | SHARE_DELETE)
        expected = identity(old)
        concurrent = self.replace_entry("concurrent.tmp", b"concurrent owner", True)
        self.assertNotEqual(identity(concurrent), expected)
        self.close(concurrent)
        self.assertEqual(identity(old), expected)
        self.assertEqual(read(old), b"original")
        self.assertEqual(self.target.read_bytes(), b"concurrent owner")
        print("POSIX: retained handle reads original; pathname names concurrent owner")

    def test_identity_recheck_then_posix_rename_still_has_gap(self):
        old = self.open(self.target, share=SHARE_READ | SHARE_DELETE)
        current = self.open(self.target, share=SHARE_READ | SHARE_DELETE)
        self.assertEqual(identity(current), identity(old))
        self.close(current)
        concurrent = self.replace_entry("concurrent.tmp", b"concurrent owner", True)
        self.close(concurrent)
        own = self.replace_entry("own.tmp", b"our save", True)
        self.close(own)
        self.assertEqual(read(old), b"original")
        self.assertEqual(self.target.read_bytes(), b"our save")
        print("Identity-recheck-then-POSIX-rename: concurrent owner was overwritten")

    def test_matching_bytes_do_not_establish_destination_identity(self):
        old = self.open(self.target, share=SHARE_READ | SHARE_DELETE)
        concurrent = self.replace_entry("concurrent.tmp", b"original", True)
        self.assertNotEqual(identity(concurrent), identity(old))
        self.close(concurrent)
        self.assertEqual(hashlib.sha256(read(old)).digest(),
                         hashlib.sha256(self.target.read_bytes()).digest())
        print("Matching hashes: distinct original and current destination identities")

    def test_two_rename_workaround_exposes_absent_destination(self):
        old = self.open(self.target, access=READ | DELETE, share=0)
        self.assertEqual(rename(old, self.parent, "backup.json"), 0)
        self.assertFalse(self.target.exists())
        self.target.write_bytes(b"concurrent owner")
        self.assertEqual(read(old), b"original")
        self.assertEqual(self.target.read_bytes(), b"concurrent owner")
        print("Two-renames: original retained as backup, destination gap admitted another owner")

    def test_replacefile_cannot_keep_no_delete_share_lock(self):
        path = self.root / "own.tmp"
        path.write_bytes(b"our save")
        old = self.open(self.target)
        self.assertFalse(kernel.ReplaceFileW(str(self.target), str(path), None, 0, None, None))
        error = c.get_last_error()
        print(f"ReplaceFile with retained no-delete-share: error={error}")
        self.assertEqual(self.target.read_bytes(), b"original")
        self.assertEqual(path.read_bytes(), b"our save")
        self.close(old)
        concurrent = self.replace_entry("concurrent.tmp", b"concurrent owner")
        self.close(concurrent)
        self.assertTrue(kernel.ReplaceFileW(str(self.target), str(path), None, 0, None, None),
                        c.get_last_error())
        self.assertEqual(self.target.read_bytes(), b"our save")
        print("ReplaceFile after unlock: concurrent owner's entry was overwritten")


if __name__ == "__main__":
    unittest.main(verbosity=2)
