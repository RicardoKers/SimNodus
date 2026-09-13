"""Compatibility shim for the opt-in native fixture process supervisor."""
import json
from pathlib import Path
import subprocess
import time


def read_startup_pid(record, owner, deadline):
    """Atomic publication can still be temporarily inaccessible on Windows."""
    retries = 0
    while True:
        if time.monotonic() >= deadline:
            raise RuntimeError("Native fixture launch deadline expired")
        try:
            text = record.read_text(encoding="utf-8")
        except (FileNotFoundError, PermissionError):
            retries += 1
        else:
            pid = int(text)
            if pid <= 0:
                raise ValueError("Invalid native child PID")
            if time.monotonic() >= deadline:
                raise RuntimeError("Native fixture launch deadline expired")
            return pid, retries
        if owner.poll() is not None:
            raise RuntimeError("Native fixture process failed before startup publication")
        time.sleep(min(.005, max(0, deadline - time.monotonic())))


class NativeProcess:
    def __init__(self, runner, record, arguments, **kwargs):
        self.record = Path(record).resolve()
        if any(Path(str(self.record)+suffix).exists() for suffix in ("", ".stop", ".tmp", ".exit.json", ".exit.json.tmp")):
            raise ValueError("Native lifecycle record must be fresh")
        self.owner = subprocess.Popen([str(runner), "--record", str(self.record), "--", *map(str, arguments)], **kwargs)
        self.stdin, self.stdout, self.stderr = self.owner.stdin, self.owner.stdout, self.owner.stderr
        try:
            deadline = time.monotonic() + 5
            self.pid, self.startup_read_retries = read_startup_pid(self.record, self.owner, deadline)
        except BaseException:
            self.owner.kill()
            self.owner.wait(timeout=5)
            for pipe in (self.stdin, self.stdout, self.stderr):
                if pipe:
                    pipe.close()
            raise

    @property
    def returncode(self):
        return self.owner.returncode

    def poll(self):
        return self.owner.poll()

    def wait(self, timeout=None):
        return self.owner.wait(timeout=timeout)

    def communicate(self, input=None, timeout=None):
        return self.owner.communicate(input=input, timeout=timeout)

    def terminate(self):
        if self.poll() is None:
            Path(str(self.record)+".stop").write_text("stop", encoding="utf-8")

    def kill(self):
        # Requests termination of the owned job, not a PID-based external kill.
        self.terminate()
        try:
            self.owner.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.owner.kill()  # Closing the job on owner death also kills children.
            self.owner.wait(timeout=5)

    def evidence(self):
        path = Path(str(self.record)+".exit.json")
        result = {"supervisor_pid": self.owner.pid, "child_pid": self.pid,
                  "supervisor_exit": self.owner.poll(), "record": str(self.record),
                  "startup_read_retries": self.startup_read_retries}
        if path.exists():
            result["native_exit"] = json.loads(path.read_text(encoding="utf-8"))
        return result


def lose_supervisor_for_test(process):
    """Fault injection: kill the owner and independently wait on the real child."""
    import ctypes
    from ctypes import wintypes
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    child = kernel.OpenProcess(0x00100000, False, process.pid)
    if not child:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        process.owner.kill()
        process.owner.wait(timeout=5)
        if kernel.WaitForSingleObject(child, 5000) != 0:
            raise RuntimeError("Child survived native supervisor loss")
        return {"child_exit_observed": True, "supervisor_exit": process.owner.returncode}
    finally:
        kernel.CloseHandle(child)
