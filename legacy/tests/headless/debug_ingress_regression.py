"""Validate complete debug replies with actual Windows writers and native state gates."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"experiments/debugging"))
from session_contract import SessionContract


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.CreateFileW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD, ctypes.c_void_p, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    kernel.CreateFileW.restype = wintypes.HANDLE
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel.WriteFile.argtypes = [wintypes.HANDLE, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD), ctypes.c_void_p]
    report = {"status":"running", "cases":[], "sha256":{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.runner,Path(__file__))}}
    cases = ("valid", "writer-prefix", "empty-closed", "malformed", "oversize", "stale", "error-file",
             "missing-timeout", "late-file", "locked-timeout", "duplicate-poll", "host-bypass", "rearm")
    try:
        for notification in (False, True):
            for name in cases + (("rejected", "closed-prefix", "shared-deadline") if notification else ("overflow", "outside-grant", "negative", "sample-deadline")):
                directory = (args.output/(("notification-" if notification else "progress-")+name)).resolve()
                directory.mkdir()
                path, grant = directory/"reply.txt", directory/"grant.txt"
                read_fd, write_fd = os.pipe()
                writer = os.fdopen(write_fd,"wb",buffering=0)
                try: client = SessionContract(args.runner,writer)
                finally: writer.close()
                handle = None
                case = {"notification":notification,"name":name}
                report["cases"].append(case)
                def command(text):
                    try: return client.command(text)
                    except RuntimeError:
                        if not client.records[-1]["snapshot"]["error"]: raise
                        return client.records[-1]["snapshot"]
                def write(data):
                    count = wintypes.DWORD()
                    if not kernel.WriteFile(handle,data,len(data),ctypes.byref(count),None) or count.value != len(data):
                        raise ctypes.WinError(ctypes.get_last_error())
                try:
                    command("begin 100000000 1")
                    command(f'start-native "{grant.as_posix()}"')
                    os.read(read_fd,4096)
                    Path(str(grant)+".ready").write_text("active",encoding="utf-8")
                    command("poll-ready")
                    if notification:
                        command("observe 2010875")
                        command("cancel-native")
                        os.read(read_fd,4096)
                        grant.write_text("start=0\nrequested=100000000\nend=2010875\nreason=cancelled\nunused=97989125\nsinks=2010875\ncancelled=True",encoding="utf-8")
                        ack=command("poll-result")
                        if ack["error"]: raise RuntimeError("Setup acknowledgement failed")
                        command("analog 2010875 0.002010875 0")
                    good = "notified" if notification else "2010875"
                    if name=="stale": path.write_text(good,encoding="utf-8")
                    if name=="shared-deadline": time.sleep(1.1)
                    verb="notify-ingress" if notification else "sample-ingress"
                    final=command(f'{verb} "{path.as_posix()}"')
                    if not final["error"]:
                        os.read(read_fd,4096)
                        if name=="host-bypass": final=command("notified" if notification else "observe 2010875")
                        elif name=="rearm": final=command(f'{verb} "{(directory/"second.txt").as_posix()}"')
                        else:
                            if name in ("writer-prefix","locked-timeout"):
                                handle=kernel.CreateFileW(str(path),0x40000000,7,None,1,0x80,None)
                                if handle==ctypes.c_void_p(-1).value: raise ctypes.WinError(ctypes.get_last_error())
                                write(good[:2].encode())
                                pending=command("poll-debug")
                                if pending["debug_status"]!="pending" or pending["debug_retries"]<1 or pending["observed_ns"]!=(2010875 if notification else 0):
                                    raise RuntimeError("Writer prefix accepted before close")
                                write(good[2:].encode())
                                if command("poll-debug")["debug_status"]!="pending": raise RuntimeError("Open writer accepted")
                                if name=="locked-timeout": time.sleep(2.01)
                                kernel.CloseHandle(handle);handle=None
                            elif name in ("missing-timeout","late-file","shared-deadline"):
                                if command("poll-debug")["debug_status"]!="pending": raise RuntimeError("Missing file accepted")
                                time.sleep(1.0 if name=="shared-deadline" else 2.01)
                                if name!="missing-timeout":path.write_text(good,encoding="utf-8")
                            elif name=="error-file":Path(str(path)+".error").write_text("error",encoding="utf-8")
                            else:
                                value={"empty-closed":"","malformed":"?","oversize":"x"*4097,"rejected":"rejected: no joint state","closed-prefix":"noti","overflow":"18446744073709551616","outside-grant":"100000001","negative":"-1"}.get(name,good)
                                path.write_text(value,encoding="utf-8")
                            final=command("poll-debug")
                            if name=="duplicate-poll":final=command("poll-debug")
                            if name=="sample-deadline":
                                if final["error"]:raise RuntimeError("Initial sample failed")
                                time.sleep(2.01)
                                next_path=directory/"next.txt"
                                final=command(f'sample-ingress "{next_path.as_posix()}"')
                    success=name in ("valid","writer-prefix")
                    if success:
                        if final["error"] or final["debug_status"]!="ready" or final["observed_ns"]!=2010875:raise RuntimeError("Complete reply rejected")
                        if notification and final["notify_pending"]:raise RuntimeError("Notification not consumed")
                    elif not final["error"]:raise RuntimeError("Invalid reply accepted")
                    if final["commits"] or final["acknowledgements"]!=int(notification):raise RuntimeError("Reply advanced CPU or joint commit")
                    if name in ("missing-timeout","late-file","locked-timeout","shared-deadline","sample-deadline") and final["debug_status"]!="timeout":raise RuntimeError("Deadline renewed")
                    case["final"]=final
                finally:
                    if handle is not None:kernel.CloseHandle(handle)
                    try:command("abort")
                    finally:
                        client.close();case["records"]=client.records;case["exit"]=client.process.returncode
                        case["pipe_eof"]=os.read(read_fd,4096)==b"";os.close(read_fd)
                if case["exit"] or not case["pipe_eof"]:raise RuntimeError("Pipe cleanup or extra command failure")
        report["status"]="passed"
    except (RuntimeError,OSError,ValueError) as error:report.update(status="failed",error=str(error))
    (args.output/"summary.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(report["status"],len(report["cases"]),"cases",report.get("error",""))
    return int(report["status"]!="passed")


if __name__=="__main__":raise SystemExit(main())
