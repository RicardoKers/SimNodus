"""Exercise native ADC invocation with real Windows child processes and jobs."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"experiments/debugging"))
from session_contract import SessionContract


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--runner",type=Path,required=True)
    parser.add_argument("--helper",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    kernel=ctypes.WinDLL("kernel32",use_last_error=True)
    kernel.OpenProcess.argtypes=[wintypes.DWORD,wintypes.BOOL,wintypes.DWORD];kernel.OpenProcess.restype=wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes=[wintypes.HANDLE,wintypes.DWORD]
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    report={"status":"running","cases":[],"sha256":{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.runner,args.helper,Path(__file__))}}
    names=("valid","fragmented","malformed","duplicate","wrongtime","stderr","oversize","exit7","timeout",
           "valid-but-alive","host-bypass","duplicate-start","commit-pending","duplicate-poll","missing-exe",
           "expired-before-start","descendant","runner-loss","spaced-path","before-prepare","shared-deadline")
    try:
        for name in names:
            directory=(args.output/name).resolve();directory.mkdir()
            helper=args.helper.resolve()
            if name=="missing-exe":helper=directory/"missing.exe"
            if name=="spaced-path":helper=Path(shutil.copy2(helper,directory/"helper with spaces.exe"))
            child_record=directory/"child.pid"
            environment={"SIMNODUS_TEST_ADC_MODE":"timeout" if name=="shared-deadline" else name,"SIMNODUS_TEST_ADC_CHILD":str(child_record)}
            previous={k:os.environ.get(k) for k in environment}
            rr,rw=os.pipe();ar,aw=os.pipe();out_read,out_write=os.pipe()
            try:
                os.environ.update(environment)
                with os.fdopen(rw,"wb",buffering=0) as renode,os.fdopen(aw,"wb",buffering=0) as analog,os.fdopen(out_read,"rb",buffering=0) as output:
                    client=SessionContract(args.runner,renode,analog,output,helper,12345)
            finally:
                for k,v in previous.items():
                    if v is None:os.environ.pop(k,None)
                    else:os.environ[k]=v
            case={"name":name};report["cases"].append(case);child_handle=None
            def command(text):
                try:return client.command(text)
                except RuntimeError:
                    if not client.records[-1]["snapshot"]["error"]:raise
                    return client.records[-1]["snapshot"]
            def publish(ns,volts,samples):
                os.write(out_write,(json.dumps({"time_ns":ns,"actual_s":ns*1e-9,"output_v":volts,"samples":samples},separators=(",",":"))+"\n").encode())
            def boundary(index,start,end,volts):
                path=directory/f"grant-{index}.txt"
                command(f'grant-native 5000000 0 "{path.as_posix()}"');os.read(rr,4096)
                Path(str(path)+".ready").write_text("active",encoding="utf-8");command("poll-ready")
                command(f"stop-native {end}");os.read(rr,4096)
                path.write_text(f"start={start}\nrequested=5000000\nend={end}\nreason=cancelled\nunused={5000000-end+start}\nsinks={end}\ncancelled=True",encoding="utf-8")
                command("poll-result");command("advance-native");os.read(ar,4096);publish(end,volts,(index+1)*2)
                if command("poll-worker")["error"]:raise RuntimeError("Setup failed")
            try:
                publish(0,0,0);command("read-worker");command("poll-worker")
                boundary(0,0,2011375,0);command("high-native");os.read(ar,4096);publish(2011375,0,2);command("poll-worker");command("commit")
                boundary(1,2011375,4010750,2.853114)
                if name!="before-prepare":command("prepare-adc")
                if name=="expired-before-start":time.sleep(2.01)
                if name=="shared-deadline":time.sleep(1.1)
                started=time.monotonic()
                final=command("adc-applied 0 2853114 4010 4010") if name=="host-bypass" else command("apply-adc")
                if not final["error"]:
                    if name=="runner-loss":
                        child_handle=kernel.OpenProcess(0x00100000,False,final["adc_helper_pid"])
                        if not child_handle:raise ctypes.WinError(ctypes.get_last_error())
                        client.process.kill();client.process.wait(timeout=3)
                        case["child_exit_observed"]=kernel.WaitForSingleObject(child_handle,3000)==0
                        if not case["child_exit_observed"]:raise RuntimeError("Helper survived runner loss")
                    elif name=="duplicate-start":final=command("apply-adc")
                    elif name=="commit-pending":final=command("commit")
                    else:
                        if name=="descendant":
                            until=time.monotonic()+1
                            while not child_record.exists() and time.monotonic()<until:time.sleep(.005)
                            child_pid=int(child_record.read_text())
                            case["descendant_pid"]=child_pid
                            child_handle=kernel.OpenProcess(0x00100000,False,child_pid)
                            if not child_handle:raise ctypes.WinError(ctypes.get_last_error())
                        until=time.monotonic()+4
                        while True:
                            final=command("poll-adc")
                            if final["error"] or final.get("adc_process_status")=="ready":break
                            if time.monotonic()>=until:raise RuntimeError("Native helper exceeded test envelope")
                            time.sleep(.002)
                        if name=="duplicate-poll":final=command("poll-adc")
                        if name=="descendant":
                            case["child_exit_observed"]=kernel.WaitForSingleObject(child_handle,1000)==0
                            if not case["child_exit_observed"]:raise RuntimeError("Descendant survived cleanup")
                case["elapsed_s"]=time.monotonic()-started
                if name in ("valid","fragmented","descendant","spaced-path"):
                    if final["error"] or final["adc_confirmations"]!=1 or final["adc_helper_exit"]!=0 or not final["adc_helper_cleaned"]:raise RuntimeError("Valid process rejected")
                    if json.loads(bytes.fromhex(final["adc_stdout_hex"]))!={"command":"adc","before_us":4010,"after_us":4010}:raise RuntimeError("Raw result differs")
                elif name!="runner-loss" and not final["error"]:raise RuntimeError("Invalid process accepted")
                if final["commits"]!=1:raise RuntimeError("Invocation committed")
                if name=="shared-deadline" and not .5<case["elapsed_s"]<1.5:raise RuntimeError("Preparation deadline was renewed")
                if name in ("timeout","valid-but-alive","shared-deadline") and final["adc_process_status"]!="timeout":raise RuntimeError("Missing timeout")
                case["final"]=final
            finally:
                if client.process.poll() is None:case["cleanup"]=command("abort")
                client.close();case["records"]=client.records
                if child_handle:kernel.CloseHandle(child_handle)
                case["renode_eof"]=os.read(rr,4096)==b"";os.close(rr)
                case["analog_eof"]=os.read(ar,4096)==b"";os.close(ar);os.close(out_write)
            if not case["renode_eof"] or not case["analog_eof"]:raise RuntimeError("Pipe leak")
            if name!="runner-loss" and case["cleanup"].get("adc_helper_pid",0) and not case["cleanup"]["adc_helper_cleaned"]:raise RuntimeError("Helper tree not cleaned")
        report["status"]="passed"
    except (RuntimeError,OSError,ValueError) as error:report.update(status="failed",error=str(error))
    (args.output/"summary.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(report["status"],len(report["cases"]),"cases",report.get("error",""))
    return int(report["status"]!="passed")


if __name__=="__main__":raise SystemExit(main())
