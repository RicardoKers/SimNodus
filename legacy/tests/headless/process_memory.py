"""Windows experiment sampler with retained process identities, not PID-only ancestry."""
import ctypes as c
from ctypes import wintypes as w

from measure_baseline import Entry, Memory


def link_rejection(parent, child_created, snapshot_time):
    if child_created <= parent["created"]:
        return "child-not-newer-than-parent"
    if child_created > snapshot_time:
        return "created-after-snapshot"
    if parent["exited"] and child_created > parent["exited"]:
        return "child-created-after-parent-exit"
    return None


class Windows:
    def __init__(self):
        self.k = c.WinDLL("kernel32", use_last_error=True)
        self.p = c.WinDLL("psapi", use_last_error=True)
        self.k.CreateToolhelp32Snapshot.argtypes = [w.DWORD, w.DWORD]
        self.k.CreateToolhelp32Snapshot.restype = w.HANDLE
        self.k.Process32FirstW.argtypes = self.k.Process32NextW.argtypes = [w.HANDLE, c.POINTER(Entry)]
        self.k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
        self.k.OpenProcess.restype = w.HANDLE
        self.k.CloseHandle.argtypes = [w.HANDLE]
        self.k.GetProcessTimes.argtypes = [w.HANDLE] + [c.POINTER(w.FILETIME)] * 4
        self.k.GetSystemTimeAsFileTime.argtypes = [c.POINTER(w.FILETIME)]
        self.p.GetProcessMemoryInfo.argtypes = [w.HANDLE, c.POINTER(Memory), w.DWORD]

    @staticmethod
    def value(ft):
        return (ft.dwHighDateTime << 32) | ft.dwLowDateTime

    def times(self, handle):
        created, exited, kernel, user = (w.FILETIME() for _ in range(4))
        if not self.k.GetProcessTimes(handle, c.byref(created), c.byref(exited), c.byref(kernel), c.byref(user)):
            raise c.WinError(c.get_last_error())
        return dict(created=self.value(created), exited=self.value(exited))

    def open(self, pid):
        handle = self.k.OpenProcess(0x410, False, pid)
        if not handle:
            raise c.WinError(c.get_last_error())
        return handle

    def close(self, handle):
        self.k.CloseHandle(handle)

    def snapshot(self):
        ft = w.FILETIME()
        self.k.GetSystemTimeAsFileTime(c.byref(ft))
        started = self.value(ft)
        handle = self.k.CreateToolhelp32Snapshot(2, 0)
        if handle == c.c_void_p(-1).value:
            raise c.WinError(c.get_last_error())
        rows = []
        entry = Entry(size=c.sizeof(Entry))
        try:
            more = self.k.Process32FirstW(handle, c.byref(entry))
            while more:
                rows.append(dict(pid=entry.pid, parent_pid=entry.parent, name=entry.name))
                more = self.k.Process32NextW(handle, c.byref(entry))
            if c.get_last_error() != 18:  # ERROR_NO_MORE_FILES
                raise c.WinError(c.get_last_error())
        finally:
            self.close(handle)
        return started, rows

    def memory(self, handle):
        memory = Memory(cb=c.sizeof(Memory))
        if not self.p.GetProcessMemoryInfo(handle, c.byref(memory), memory.cb):
            raise c.WinError(c.get_last_error())
        return dict(working_set=memory.ws, private_bytes=memory.private)


class ProcessTreeSampler:
    """Keep handles until batch completion; skip unverifiable descendants."""
    def __init__(self, backend=None):
        self.backend = backend or Windows()
        self.root_pid = None
        self.owned = {}

    def close(self):
        for member in self.owned.values():
            self.backend.close(member["handle"])
        self.owned.clear()
        self.root_pid = None

    def __call__(self, root_pid):
        if self.root_pid != root_pid:
            self.close()
            handle = self.backend.open(root_pid)
            try:
                times = self.backend.times(handle)
            except BaseException:
                self.backend.close(handle)
                raise
            self.root_pid = root_pid
            self.owned[root_pid] = dict(handle=handle, **times, parent_pid=None,
                                        parent_created=None, name="root")
        snapshot_time, entries = self.backend.snapshot()
        rejected, errors = [], []
        pending = {entry["pid"]: entry for entry in entries if entry["pid"] not in self.owned}
        # Handles retain known parent identities even after their exit. Newly seen
        # descendants must fit their parent's observed creation/exit interval.
        while True:
            candidates = [pid for pid, entry in pending.items() if entry["parent_pid"] in self.owned]
            if not candidates:
                break
            for pid in candidates:
                entry = pending.pop(pid)
                parent = self.owned[entry["parent_pid"]]
                handle = None
                try:
                    parent_times = self.backend.times(parent["handle"])
                    handle = self.backend.open(pid)
                    child_times = self.backend.times(handle)
                    reason = link_rejection(parent_times, child_times["created"], snapshot_time)
                    if reason:
                        rejected.append(dict(**entry, **child_times, parent_created=parent_times["created"],
                                             parent_exited=parent_times["exited"], reason=reason))
                        continue
                    self.owned[pid] = dict(handle=handle, **entry, **child_times,
                                          parent_created=parent_times["created"])
                    handle = None
                except OSError as error:
                    errors.append(dict(pid=pid, phase="identity", error=str(error)))
                finally:
                    if handle is not None:
                        self.backend.close(handle)
        rows = []
        names = {entry["pid"]: entry["name"] for entry in entries}
        for pid, member in self.owned.items():
            try:
                times = self.backend.times(member["handle"])
                if times["created"] != member["created"]:
                    raise RuntimeError("Retained process handle changed identity")
                if times["exited"]:
                    continue
                rows.append(dict(pid=pid, name=names.get(pid, member["name"]),
                    parent_pid=member["parent_pid"], parent_created=member["parent_created"],
                    created=times["created"], **self.backend.memory(member["handle"])))
            except OSError as error:
                errors.append(dict(pid=pid, phase="memory", error=str(error)))
        return dict(snapshot_filetime=snapshot_time, root_pid=root_pid,
                    root_created=self.owned[root_pid]["created"], processes=rows,
                    errors=errors, rejected=rejected, retained_handles=len(self.owned))
