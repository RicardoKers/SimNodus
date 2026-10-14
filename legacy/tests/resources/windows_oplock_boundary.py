# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Manual owned-fixture oplock observations; no supported overwrite protocol."""
import argparse
from contextlib import ExitStack, contextmanager
import ctypes as c
from ctypes import wintypes as w
import hashlib
import json
from pathlib import Path
import sys
import threading

import local_resources as physical
import windows_overwrite_boundary as rename_api

OLD = b"original fixture"
NEW = b"replacement fixture"
INVALID = c.c_void_p(-1).value


class Overlapped(c.Structure):
    _fields_ = [("internal", c.c_size_t), ("high", c.c_size_t),
                ("offset", w.DWORD), ("offset_high", w.DWORD), ("event", w.HANDLE)]


class Request(c.Structure):
    _fields_ = [("version", w.WORD), ("length", w.WORD),
                ("level", w.DWORD), ("flags", w.DWORD)]


class Response(c.Structure):
    _fields_ = [("version", w.WORD), ("length", w.WORD),
                ("original", w.DWORD), ("new", w.DWORD), ("flags", w.DWORD),
                ("access", w.DWORD), ("share", w.WORD)]


class Api:
    def __init__(self):
        self.fs = physical.WindowsFiles()
        self.k = self.fs.k
        self.k.CreateEventW.argtypes = [c.c_void_p, w.BOOL, w.BOOL, w.LPCWSTR]
        self.k.CreateEventW.restype = w.HANDLE
        self.k.WaitForSingleObject.argtypes = [w.HANDLE, w.DWORD]
        self.k.WaitForSingleObject.restype = w.DWORD
        self.k.DeviceIoControl.argtypes = [w.HANDLE, w.DWORD, c.c_void_p,
            w.DWORD, c.c_void_p, w.DWORD, c.c_void_p, c.c_void_p]
        self.k.DeviceIoControl.restype = w.BOOL
        self.k.GetFinalPathNameByHandleW.argtypes = [w.HANDLE, w.LPWSTR, w.DWORD, w.DWORD]
        self.k.GetFinalPathNameByHandleW.restype = w.DWORD

    @contextmanager
    def directory(self, path):
        drive, parts = physical.root_parts(str(path))
        with ExitStack() as stack:
            parent = self.fs.volume(drive, stack)
            for part in parts:
                parent, _ = self.fs.child(parent, part, True, stack, "$fixture")
            buffer = c.create_unicode_buffer(32768)
            count = self.k.GetFinalPathNameByHandleW(parent, buffer, len(buffer), 1)
            assert 0 < count < len(buffer)
            yield parent, Path(buffer.value)


def observe(api, root, mode, level, directory, report):
    root.mkdir()
    (root / "project.json").write_bytes(OLD)
    (root / "new.tmp").write_bytes(NEW)
    with api.directory(root) as (parent, pinned):
        with ExitStack() as captured:
            handle, info = api.fs.child(parent, "project.json", False, captured, "$target")
            assert api.fs.read(handle, 64, "$target") == OLD
            expected = info.identity()
        source = rename_api.opened(pinned / "new.tmp", access=rename_api.READ | rename_api.DELETE, share=0)
        watched = pinned if directory else pinned / "project.json"
        flags = 0x40200000 | (0x02000000 if directory else 0)
        handle = api.k.CreateFileW(str(watched), 0x80000000, 7, None, 3, flags, None)
        assert handle != INVALID, c.get_last_error()
        event = api.k.CreateEventW(None, True, False, None)
        assert event, c.get_last_error()
        operation = None
        done = threading.Event()
        try:
            info = api.fs.info(handle, directory, "$watched")
            if not directory:
                assert info.identity() == expected
            request = Request(1, c.sizeof(Request), level, 1)
            response = Response()
            overlapped = Overlapped()
            overlapped.event = event
            success = api.k.DeviceIoControl(handle, 0x00090240,
                c.byref(request), c.sizeof(request), c.byref(response),
                c.sizeof(response), None, c.byref(overlapped))
            error = c.get_last_error()
            report["request_error"] = error
            assert not success and error == 997, f"Oplock was not granted: error={error}"

            def replace():
                # Independent FILE_OBJECT in this same process; no shared key.
                report["rename_status"] = rename_api.rename(source, parent, "project.json", True)
                done.set()

            operation = threading.Thread(target=replace, daemon=True)
            operation.start()
            report["break_event"] = api.k.WaitForSingleObject(event, 2000) == 0
            report["completed_before_release"] = done.wait(0.25)
            report["break_flags"] = response.flags
            report["ack_required"] = bool(response.flags & 1)
            report["new_level"] = response.new
            report["completion_status"] = int(overlapped.internal)
            # No other open/read is issued while the file oplock is retained.
            # Closing is the deliberate experimental release, never a safe-save claim.
        finally:
            api.k.CloseHandle(handle)
            if operation is not None:
                operation.join(5)
            # Keep request/response/OVERLAPPED storage alive until completion.
            report["request_completed_after_close"] = api.k.WaitForSingleObject(event, 2000) == 0
            api.k.CloseHandle(event)
            if operation is None or not operation.is_alive():
                api.k.CloseHandle(source)
        assert operation is not None and not operation.is_alive(), "Replacement did not finish after release"
        assert report["request_completed_after_close"]
        assert report["rename_status"] == 0, report
        report["final_bytes"] = (pinned / "project.json").read_bytes().decode()
        assert report["final_bytes"] == NEW.decode()
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    root = args.output.absolute()
    root.mkdir(parents=True, exist_ok=False)
    report = {"status": "started", "scope": "Fixture observations only; no safe overwrite acceptance", "cases": []}
    try:
        api = Api()
        for mode, level, directory in (("directory-rh", 3, True), ("file-rw", 5, False), ("file-rwh", 7, False)):
            case = {"mode": mode, "requested_level": level}
            report["cases"].append(case)
            observe(api, root / mode, mode, level, directory, case)
        report["status"] = "observed"
    except Exception as error:
        report["status"] = "failed"
        report["error"] = str(error)
        raise
    finally:
        report["script_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        (root / "result.json").write_bytes((json.dumps(report, indent=2) + "\n").encode())
        print(json.dumps(report))


if __name__ == "__main__":
    main()
