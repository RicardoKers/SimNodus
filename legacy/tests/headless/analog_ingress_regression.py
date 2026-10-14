"""Validate native analog frames with actual fragmented, stale and broken Windows pipes."""
import argparse
import ctypes
from ctypes import wintypes
import msvcrt
import hashlib
import json
import os
from pathlib import Path
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"experiments/debugging"))
from session_contract import SessionContract


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    kernel=ctypes.WinDLL("kernel32",use_last_error=True)
    kernel.WriteFile.argtypes=[wintypes.HANDLE,ctypes.c_void_p,wintypes.DWORD,ctypes.POINTER(wintypes.DWORD),ctypes.c_void_p]
    report={"status":"running","cases":[],"sha256":{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.runner,Path(__file__))}}
    names=("valid","fragmented","bytewise","crlf","missing","late","lost","partial-lost","oversize","duplicate-frame",
           "duplicate-key","unknown-key","missing-key","nan","overflow","wrong-time","wrong-endpoint","stale-samples",
           "stale-buffer","duplicate-poll","host-bypass","rearm","commit-pending","inspection-change","inspection-pending")
    zero='{"time_ns":0,"actual_s":0,"output_v":0,"samples":0}'
    good='{"time_ns":2010875,"actual_s":0.002010875,"output_v":0,"samples":2}'
    try:
        for name in names:
            directory=(args.output/name).resolve();directory.mkdir()
            rr,rw=os.pipe();ar,aw=os.pipe();out_read,out_write=os.pipe()
            with os.fdopen(rw,"wb",buffering=0) as renode,os.fdopen(aw,"wb",buffering=0) as analog,os.fdopen(out_read,"rb",buffering=0) as output:
                client=SessionContract(args.runner,renode,analog,output)
            case={"name":name};report["cases"].append(case)
            def command(text):
                try:return client.command(text)
                except RuntimeError:
                    if not client.records[-1]["snapshot"]["error"]:raise
                    return client.records[-1]["snapshot"]
            def publish(text):os.write(out_write,text.encode("ascii"))
            try:
                publish(zero+"\n");command("read-worker")
                initial=command("poll-worker")
                if initial["error"] or initial["worker_result"]!=json.loads(zero):raise RuntimeError("Initial state rejected")
                path=directory/"grant.txt"
                command(f'grant-native 5000000 0 "{path.as_posix()}"');os.read(rr,4096)
                Path(str(path)+".ready").write_text("active",encoding="utf-8");command("poll-ready")
                command("stop-native 2010875");os.read(rr,4096)
                path.write_text("start=0\nrequested=5000000\nend=2010875\nreason=cancelled\nunused=2989125\nsinks=2010875\ncancelled=True",encoding="utf-8")
                if command("poll-result")["error"]:raise RuntimeError("CPU setup failed")
                if name=="stale-buffer":publish(good+"\n")
                final=command("advance-native")
                if not final["error"]:
                    if os.read(ar,4096)!=b"advance 2010875\n":raise RuntimeError("Wrong native target")
                    if name=="host-bypass":final=command("analog 2010875 0.002010875 0")
                    elif name=="rearm":final=command("read-worker")
                    elif name=="commit-pending":final=command("commit")
                    else:
                        if name in ("missing","late"):
                            if command("poll-worker")["worker_status"]!="pending":raise RuntimeError("Missing frame consumed")
                            time.sleep(2.01)
                            if name=="late":publish(good+"\n")
                        elif name=="oversize":
                            publish("x"*4096);command("poll-worker");publish("x\n")
                        elif name in ("lost","partial-lost"):
                            if name=="partial-lost":publish(good[:8]);command("poll-worker")
                            os.close(out_write);out_write=None
                        elif name in ("fragmented","bytewise"):
                            chunks=[good[:11],good[11:]] if name=="fragmented" else list(good)
                            for chunk in chunks:
                                publish(chunk)
                                pending=command("poll-worker")
                                if pending["worker_status"]!="pending" or pending["phase"]!=3 or not pending["analog_pending"]:raise RuntimeError("Partial frame acknowledged")
                            publish("\n")
                        else:
                            payload={"oversize":"x"*4097,"duplicate-frame":good+"\n"+good,"duplicate-key":good.replace('"samples":2','"samples":2,"samples":2'),"unknown-key":good.replace('"samples"','"other"'),"missing-key":good.replace(',"samples":2',''),"nan":good.replace('"output_v":0','"output_v":NaN'),"overflow":good.replace('"samples":2','"samples":18446744073709551616'),"wrong-time":good.replace('2010875','2010876'),"wrong-endpoint":good.replace('0.002010875','0.004'),"stale-samples":good.replace('"samples":2','"samples":0')}.get(name,good)
                            publish(payload+("\r\n" if name=="crlf" else "\n"))
                        final=command("poll-worker")
                        if name=="duplicate-poll":final=command("poll-worker")
                        elif name in ("inspection-change","inspection-pending") and not final["error"]:
                            command("read-worker")
                            if name=="inspection-pending":final=command("commit")
                            else:
                                publish(good.replace('"output_v":0','"output_v":1')+"\n");final=command("poll-worker")
                success=name in ("valid","fragmented","bytewise","crlf")
                if success:
                    if final["error"] or final["phase"]!=4 or final["worker_result"]!=json.loads(good) or final["worker_raw"]!=good:raise RuntimeError("Complete frame rejected")
                elif not final["error"]:raise RuntimeError("Invalid frame accepted")
                if final["commits"] or final["acknowledgements"]!=1:raise RuntimeError("Reply advanced joint commit or CPU acknowledgement")
                if name in ("missing","late") and final["worker_status"]!="timeout":raise RuntimeError("Reply deadline renewed")
                case["final"]=final
            finally:
                command("abort")
                client.close();case["records"]=client.records
                case["renode_eof"]=os.read(rr,4096)==b"";os.close(rr)
                case["analog_eof"]=os.read(ar,4096)==b"";os.close(ar)
                if out_write is not None:
                    count=wintypes.DWORD()
                    ok=kernel.WriteFile(msvcrt.get_osfhandle(out_write),b"x",1,ctypes.byref(count),None)
                    case["closed_reader_winerror"]=ctypes.get_last_error()
                    case["reader_closed"]=not ok and case["closed_reader_winerror"] in (109,232)
                    os.close(out_write)
            if not case["renode_eof"] or not case["analog_eof"] or case.get("reader_closed") is False:raise RuntimeError("Native pipe leak")
        report["status"]="passed"
    except (RuntimeError,OSError,ValueError) as error:report.update(status="failed",error=str(error))
    (args.output/"summary.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(report["status"],len(report["cases"]),"cases",report.get("error",""))
    return int(report["status"]!="passed")


if __name__=="__main__":raise SystemExit(main())
