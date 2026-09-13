"""Opt-in transport to the extracted C++ session state gate."""
import json
import queue
import subprocess
import threading
import time
import ctypes
from ctypes import wintypes


class SessionContract:
    def __init__(self, binary, renode_stdin=None, analog_stdin=None, analog_stdout=None, adc_helper=None, adc_port=None):
        self.records = []
        self.pending = queue.Queue()
        arguments = [str(binary), "--cooperative-fixture"]
        if adc_helper is not None:
            if analog_stdout is None or adc_port is None:
                raise ValueError("Native ADC helper requires native analog replies and an explicit port")
        startup = None
        inherited = []
        if analog_stdin is not None and renode_stdin is None:
            raise ValueError("Analog command channel requires the native Renode channel")
        if analog_stdout is not None and analog_stdin is None:
            raise ValueError("Native analog replies require the analog command channel")
        try:
            if renode_stdin is not None:
                import msvcrt
                kernel = ctypes.WinDLL("kernel32", use_last_error=True)
                kernel.GetCurrentProcess.restype = wintypes.HANDLE
                kernel.DuplicateHandle.argtypes = [wintypes.HANDLE, wintypes.HANDLE, wintypes.HANDLE,
                    ctypes.POINTER(wintypes.HANDLE), wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
                kernel.CloseHandle.argtypes = [wintypes.HANDLE]
                process = kernel.GetCurrentProcess()
                for option, pipe in (("--renode-stdin", renode_stdin), ("--analog-stdin", analog_stdin), ("--analog-stdout", analog_stdout)):
                    if pipe is None:
                        continue
                    handle = wintypes.HANDLE()
                    if not kernel.DuplicateHandle(process, msvcrt.get_osfhandle(pipe.fileno()),
                                                  process, ctypes.byref(handle), 0, True, 2):
                        raise ctypes.WinError(ctypes.get_last_error())
                    inherited.append(handle)
                    arguments += [option, str(handle.value)]
                startup = subprocess.STARTUPINFO()
                startup.lpAttributeList = {"handle_list": [h.value for h in inherited]}
            if adc_helper is not None:
                arguments += ["--adc-helper", str(adc_helper), "--adc-port", str(adc_port)]
            self.process = subprocess.Popen(arguments,
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                text=True, encoding="utf-8", bufsize=1, creationflags=subprocess.CREATE_NO_WINDOW,
                startupinfo=startup, close_fds=True)
        finally:
            for handle in inherited:
                kernel.CloseHandle(handle)
        self.reader = threading.Thread(target=self.read, daemon=True)
        self.reader.start()
        try:
            self.response("initial")
        except Exception:
            self.close()
            raise

    def read(self):
        for line in self.process.stdout:
            self.pending.put(line)
        self.pending.put(None)

    def response(self, command, timeout=2):
        try:
            line = self.pending.get(timeout=timeout)
        except queue.Empty as error:
            raise RuntimeError("C++ session response timeout") from error
        if line is None:
            raise RuntimeError("C++ session process lost")
        snapshot = json.loads(line)
        self.records.append({"command": command, "snapshot": snapshot})
        if snapshot["error"] and command != "abort":
            raise RuntimeError("C++ session rejected transition: " + str(snapshot))
        return snapshot

    def command(self, text, timeout=2):
        self.process.stdin.write(text + "\n")
        self.process.stdin.flush()
        return self.response(text, timeout)

    def acknowledge(self, values):
        if values["cancelled"] != "True" or values["reason"] != "cancelled" or len(values["sinks"]) != 1:
            raise RuntimeError("Unsupported CPU outcome for extracted fixture contract")
        return self.command("ack " + " ".join(str(values[k]) for k in ("start", "end", "requested", "unused"))
                            + " 1 " + str(values["sinks"][0]))

    def start_native(self, path, grant=None):
        deadline = time.monotonic() + 2
        text = f'start-native "{path.resolve().as_posix()}"' if grant is None else f'grant-native {grant} {int(grant == 100000000)} "{path.resolve().as_posix()}"'
        self.command(text)
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise RuntimeError("CPU ready phase deadline expired")
            response = self.command("poll-ready", timeout=remaining)
            if response["ready_status"] == "ready":
                return response
            time.sleep(min(.001, max(0, deadline - time.monotonic())))

    def debug_reply(self, path, notification=False):
        deadline = self.result_deadline if notification else time.monotonic() + 2
        verb = "notify-ingress" if notification else "sample-ingress"
        self.command(f'{verb} "{path.resolve().as_posix()}"')
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise RuntimeError("Debug reply phase deadline expired")
            response = self.command("poll-debug", timeout=remaining)
            if response["debug_status"] == "ready":
                return response
            time.sleep(min(.001, max(0, deadline - time.monotonic())))

    def cancel_native(self, observed=None):
        self.result_deadline = time.monotonic() + 2
        self.command("cancel-native" if observed is None else f"stop-native {observed}")

    def expect_result(self, path):
        # Reserve 100 ms for IPC/diagnosis inside the unchanged two-second host limit.
        self.result_deadline = time.monotonic() + 2
        # POSIX separators avoid backslash escaping in the helper's quoted path.
        path_text = str(path.resolve().as_posix()).replace('"', '\\"')
        return self.command(f'expect-result "{path_text}" 1900')

    def read_result(self):
        while True:
            remaining = self.result_deadline - time.monotonic()
            if remaining <= 0:
                raise RuntimeError("CPU result transport deadline expired")
            response = self.command("poll-result", timeout=remaining)
            if response["result_status"] == "ready":
                return response["cpu_result"]
            time.sleep(min(0.001, max(0, self.result_deadline - time.monotonic())))

    def worker_response(self, armed=False):
        deadline = time.monotonic() + 2
        if not armed and not self.records[-1]["snapshot"].get("analog_pending"):
            self.command("read-worker")
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise RuntimeError("Native analog reply deadline expired")
            response = self.command("poll-worker", timeout=remaining)
            if response["worker_status"] == "ready":
                self.last_worker_raw = response["worker_raw"]
                return response["worker_result"]
            time.sleep(min(.001, max(0, deadline - time.monotonic())))

    def analog(self, sample):
        return self.command(f"analog {sample['time_ns']} {sample['actual_s']:.17g} {sample['output_v']:.17g}")

    def close(self):
        try:
            if self.process.poll() is None:
                self.process.stdin.write("quit\n")
                self.process.stdin.flush()
                self.process.wait(timeout=2)
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait(timeout=2)
            self.reader.join(timeout=2)
            for pipe in (self.process.stdin, self.process.stdout):
                try:
                    pipe.close()
                except OSError:
                    pass
