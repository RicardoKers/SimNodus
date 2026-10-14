"""Exercise startup PID retry with real Windows sharing locks and process lifetime."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import threading
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"experiments/debugging"))
from native_process import read_startup_pid


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    kernel=ctypes.WinDLL("kernel32",use_last_error=True)
    kernel.CreateFileW.argtypes=[wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
    kernel.CreateFileW.restype=wintypes.HANDLE
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    report={"status":"running","cases":[],"sha256":{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (Path(__file__),Path("tests/experiments/debugging/native_process.py"))}}
    try:
        for name in ("lock-released","lock-timeout","missing-timeout","malformed"):
            owner=subprocess.Popen([sys.executable,"-c","import time; time.sleep(3)"],creationflags=subprocess.CREATE_NO_WINDOW)
            handle=None;timer=None;path=(args.output/(name+".pid")).resolve();case={"name":name};report["cases"].append(case)
            try:
                if name!="missing-timeout":path.write_text("invalid" if name=="malformed" else str(owner.pid),encoding="utf-8")
                if name.startswith("lock"):
                    handle=kernel.CreateFileW(str(path),0x80000000,0,None,3,0x80,None)
                    if handle==ctypes.c_void_p(-1).value:raise ctypes.WinError(ctypes.get_last_error())
                    if name=="lock-released":
                        timer=threading.Timer(.08,lambda h=handle:kernel.CloseHandle(h));timer.start();handle=None
                started=time.monotonic()
                try:
                    pid,retries=read_startup_pid(path,owner,started+(.8 if name=="lock-released" else .15))
                    if name!="lock-released" or pid!=owner.pid or retries<1:raise AssertionError("Invalid startup reply accepted")
                    case.update(pid=pid,retries=retries)
                except (RuntimeError,ValueError) as error:
                    case["error"]=str(error)
                    if name=="lock-released":raise
                    if name.endswith("timeout") and "deadline" not in str(error):raise AssertionError("Deadline not preserved")
                case["elapsed_s"]=time.monotonic()-started
                if name.endswith("timeout") and not .15<=case["elapsed_s"]<.8:raise AssertionError("Retry renewed startup deadline")
            finally:
                if timer:timer.join()
                if handle is not None:kernel.CloseHandle(handle)
                owner.kill();owner.wait(timeout=3)
        report["status"]="passed"
    except (RuntimeError,ValueError,OSError,AssertionError) as error:report.update(status="failed",error=str(error))
    (args.output/"summary.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(report["status"],len(report["cases"]),"cases",report.get("error",""))
    return int(report["status"]!="passed")


if __name__=="__main__":raise SystemExit(main())
