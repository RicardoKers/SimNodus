"""Explicit Windows/NTFS snapshot verification; never interpret resource bytes."""
import argparse
from contextlib import ExitStack
import ctypes as c
from ctypes import wintypes as w
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import re
import sys
from types import MappingProxyType

# Fixed repository reference validators; never add project/resource directories.
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "schema"))
import resource_lock as lock
import topology as t


class UnicodeString(c.Structure):
    _fields_ = [("length", w.USHORT), ("maximum", w.USHORT), ("buffer", w.LPWSTR)]


class ObjectAttributes(c.Structure):
    _fields_ = [("length", w.ULONG), ("root", w.HANDLE),
                ("name", c.POINTER(UnicodeString)), ("attributes", w.ULONG),
                ("security", w.LPVOID), ("quality", w.LPVOID)]


class IoStatus(c.Structure):
    _fields_ = [("status", c.c_void_p), ("information", c.c_size_t)]


class FileInfo(c.Structure):
    _fields_ = [("attributes", w.DWORD), ("created", w.FILETIME),
                ("accessed", w.FILETIME), ("written", w.FILETIME),
                ("volume", w.DWORD), ("size_high", w.DWORD), ("size_low", w.DWORD),
                ("links", w.DWORD), ("index_high", w.DWORD), ("index_low", w.DWORD)]

    def identity(self):
        return self.volume, self.index_high, self.index_low

    def size(self):
        return (self.size_high << 32) | self.size_low


@dataclass(frozen=True)
class Snapshot:
    path: str
    data: bytes
    sha256: str
    identity: tuple


class WindowsFiles:
    """Owned handles, handle-relative names, fixed system DLLs only."""

    def __init__(self):
        t.require(os.name == "nt", "platform", "$", "Only local Windows NTFS is supported")
        self.k = c.WinDLL("kernel32.dll", use_last_error=True, winmode=0x800)
        self.nt = c.WinDLL("ntdll.dll", winmode=0x800)
        signatures = {
            "CreateFileW": (w.HANDLE, [w.LPCWSTR, w.DWORD, w.DWORD, w.LPVOID, w.DWORD, w.DWORD, w.HANDLE]),
            "CloseHandle": (w.BOOL, [w.HANDLE]),
            "GetFileInformationByHandle": (w.BOOL, [w.HANDLE, c.POINTER(FileInfo)]),
            "GetFileInformationByHandleEx": (w.BOOL, [w.HANDLE, c.c_int, w.LPVOID, w.DWORD]),
            "GetFileType": (w.DWORD, [w.HANDLE]),
            "ReadFile": (w.BOOL, [w.HANDLE, w.LPVOID, w.DWORD, c.POINTER(w.DWORD), w.LPVOID]),
            "GetDriveTypeW": (w.UINT, [w.LPCWSTR]),
            "QueryDosDeviceW": (w.DWORD, [w.LPCWSTR, w.LPWSTR, w.DWORD]),
            "GetVolumeInformationByHandleW": (w.BOOL, [w.HANDLE, w.LPWSTR, w.DWORD,
                c.POINTER(w.DWORD), c.POINTER(w.DWORD), c.POINTER(w.DWORD), w.LPWSTR, w.DWORD]),
        }
        for name, (result, args) in signatures.items():
            function = getattr(self.k, name)
            function.restype, function.argtypes = result, args
        self.nt.NtCreateFile.restype = w.LONG
        self.nt.NtCreateFile.argtypes = [c.POINTER(w.HANDLE), w.DWORD,
            c.POINTER(ObjectAttributes), c.POINTER(IoStatus), c.c_void_p,
            w.ULONG, w.ULONG, w.ULONG, w.ULONG, c.c_void_p, w.ULONG]

    def check(self, success, loc):
        t.require(bool(success), "filesystem", loc, "Windows filesystem operation failed")

    def own(self, handle, stack):
        stack.callback(self.k.CloseHandle, handle)
        return handle

    def info(self, handle, directory, loc):
        value = FileInfo()
        self.check(self.k.GetFileInformationByHandle(handle, c.byref(value)), loc)
        t.require(self.k.GetFileType(handle) == 1, "type", loc, "Expected disk object")
        t.require(not value.attributes & (0x400 | 0x1000), "reparse", loc,
                  "Reparse points and offline resources are unsupported")
        t.require(bool(value.attributes & 0x10) == directory, "type", loc,
                  "Unexpected file/directory type")
        if not directory:
            t.require(value.links == 1, "alias", loc, "Resource must have exactly one hard link")
        return value

    def volume(self, drive, stack):
        t.require(self.k.GetDriveTypeW(drive + "\\") == 3, "platform", "$root", "Expected fixed local drive")
        target = c.create_unicode_buffer(1024)
        self.check(self.k.QueryDosDeviceW(drive, target, len(target)), "$root")
        t.require(re.fullmatch(r"\\Device\\HarddiskVolume[0-9]+", target.value) is not None,
                  "platform", "$root", "Expected direct local volume mapping")
        # Open the captured device target, not a drive mapping that could change.
        handle = self.k.CreateFileW("\\\\?\\GLOBALROOT" + target.value + "\\", 0x100080, 1, None, 3,
                                    0x02300000, None)  # BACKUP_SEMANTICS, OPEN_REPARSE_POINT, OPEN_NO_RECALL
        t.require(handle != c.c_void_p(-1).value, "filesystem", "$root", "Cannot pin volume root")
        self.own(handle, stack)
        self.info(handle, True, "$root")
        name = c.create_unicode_buffer(32)
        self.check(self.k.GetVolumeInformationByHandleW(handle, None, 0, None, None, None,
                                                      name, len(name)), "$root")
        t.require(name.value == "NTFS", "platform", "$root", "Only NTFS is supported")
        return handle

    def child(self, parent, name, directory, stack, loc):
        text = c.create_unicode_buffer(name)
        size = len(name.encode("utf-16-le"))
        string = UnicodeString(size, size + 2, c.cast(text, w.LPWSTR))
        attributes = ObjectAttributes(c.sizeof(ObjectAttributes), parent, c.pointer(string),
                                      0x40, None, None)  # OBJ_CASE_INSENSITIVE
        handle, status = w.HANDLE(), IoStatus()
        # SYNCHRONIZE + READ_ATTRIBUTES; files additionally need READ_DATA.
        access = 0x100080 | (0 if directory else 1)
        result = self.nt.NtCreateFile(c.byref(handle), access, c.byref(attributes),
            c.byref(status), None, 0, 1, 1, 0x00600020, None, 0)  # OPEN_NO_RECALL too
        t.require(result >= 0, "filesystem", loc, "Cannot open child without write/delete sharing")
        self.own(handle.value, stack)
        info = self.info(handle.value, directory, loc)
        # FileNameInfo: compare the opened long basename, not a path prefix.
        buffer = c.create_string_buffer(65536)
        self.check(self.k.GetFileInformationByHandleEx(handle, 2, buffer, len(buffer)), loc)
        length = int.from_bytes(buffer.raw[:4], "little")
        t.require(0 < length <= len(buffer) - 4 and length % 2 == 0,
                  "alias", loc, "Invalid opened filename")
        actual = buffer.raw[4:4 + length].decode("utf-16-le").rsplit("\\", 1)[-1]
        t.require(actual.casefold() == name.casefold(), "alias", loc, "Alternate filename alias rejected")
        return handle.value, info

    def read(self, handle, amount, loc):
        buffer, count = c.create_string_buffer(amount), w.DWORD()
        self.check(self.k.ReadFile(handle, buffer, amount, c.byref(count), None), loc)
        return buffer.raw[:count.value]


def root_parts(root):
    t.require(type(root) is str and len(root) <= 1024 and
              re.match(r"^[A-Za-z]:[\\/]", root) is not None,
              "root", "$root", "Explicit drive-absolute root required")
    drive, tail = root[:2], root[3:].replace("\\", "/")
    parts = tail.split("/") if tail else []
    t.require(len(parts) <= 64 and all(part and part not in (".", "..")
              and not part.endswith((".", " "))
              and not any(ord(char) < 32 or 0xD800 <= ord(char) <= 0xDFFF
                          or char in ':~<>"|?*' for char in part)
              and part.split(".")[0].lower() not in lock.RESERVED for part in parts),
              "root", "$root", "Ambiguous root component")
    return drive, parts


def verify(raw, root):
    """Consume immutable lock bytes; return an immutable all-or-nothing snapshot map."""
    t.require(type(raw) is bytes, "input", "$", "Expected immutable lock bytes")
    document = lock.parse(raw)  # Always finish metadata validation before OS access.
    drive, parts = root_parts(root)
    fs = WindowsFiles()
    snapshots, identities = {}, set()
    with ExitStack() as stack:
        current = fs.volume(drive, stack)
        for part in parts:
            current, _ = fs.child(current, part, True, stack, "$root")
        root_identity = fs.info(current, True, "$root").identity()
        directories = {(): current}
        total = 0
        for dependency in document["dependencies"]:
            for entry in dependency["files"]:
                loc = entry["path"]
                segments = loc.split("/")
                parent = current
                for depth, segment in enumerate(segments[:-1], 1):
                    # Ask the filesystem about each supplied spelling; NTFS can
                    # distinguish case in individual directories.
                    key = tuple(segments[:depth])
                    if key not in directories:
                        directories[key], _ = fs.child(parent, segment, True, stack, loc)
                    parent = directories[key]
                # Resource handles need not accumulate; snapshots own the consumed bytes.
                with ExitStack() as file_stack:
                    handle, before = fs.child(parent, segments[-1], False, file_stack, loc)
                    t.require(before.volume == root_identity[0] and before.identity() not in identities,
                              "alias", loc, "Duplicate or cross-volume resource identity")
                    t.require(before.size() <= lock.MAX_FILE_BYTES, "budget", loc, "Actual file size exceeded")
                    t.require(before.size() == entry["bytes"], "size", loc, "Actual and declared sizes differ")
                    data = bytearray()
                    while True:
                        chunk = fs.read(handle, min(65536, entry["bytes"] + 1 - len(data)), loc)
                        if not chunk:
                            break
                        data.extend(chunk)
                        t.require(len(data) <= entry["bytes"], "size", loc, "Resource grew during read")
                    after = fs.info(handle, False, loc)
                    t.require(after.identity() == before.identity() and after.size() == len(data)
                              and len(data) == entry["bytes"], "size", loc, "Resource changed during read")
                    total += len(data)
                    t.require(total <= lock.MAX_TOTAL_BYTES, "budget", loc, "Actual total byte budget exceeded")
                    frozen = bytes(data)
                    digest = hashlib.sha256(frozen).hexdigest()
                    t.require(digest == entry["sha256"], "hash", loc, "Actual SHA-256 differs")
                    identities.add(before.identity())
                    snapshots[(dependency["id"], entry["id"])] = Snapshot(loc, frozen, digest, before.identity())
    return MappingProxyType(snapshots)


def summary(snapshots):
    return dict(status="verified-local-byte-snapshots-only", resources=len(snapshots),
                verified_bytes=sum(len(value.data) for value in snapshots.values()),
                containment_verified=True, resources_verified=True,
                resource_interfaces_verified=False, firmware_verified=False,
                origin_verified=False, redistribution_verified=False,
                execution_authorized=False, runtime_profile_verified=False, simulation_ready=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("lock", type=Path)
    parser.add_argument("--root", required=True)
    args = parser.parse_args()
    try:
        with args.lock.open("rb") as source:
            raw = source.read(t.MAX_BYTES + 1)
        print(json.dumps(summary(verify(raw, args.root))))
        return 0
    except (t.Invalid, OSError) as error:
        diagnostic = error.diagnostic() if isinstance(error, t.Invalid) else dict(
            code="filesystem", location="$", message="Cannot read local input")
        print(json.dumps(dict(status="invalid", **diagnostic)))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
