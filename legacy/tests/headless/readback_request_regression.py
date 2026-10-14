"""Validate native mailbox request correlation and deadline using actual fixture pipes."""
import argparse
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
    report={"status":"running","cases":[],"sha256":{str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in (args.runner,Path(__file__))}}
    names=("valid","wrong-token","legacy-token","unarmed","duplicate-arm","timeout","near-deadline",
           "commit-pending","other-command","arm-argument","early-arm","duplicate","normalized-bypass","disabled")
    try:
        for name in names:
            directory=(args.output/name).resolve();directory.mkdir()
            rr,rw=os.pipe();ar,aw=os.pipe();out_read,out_write=os.pipe()
            with os.fdopen(rw,"wb",buffering=0) as renode,os.fdopen(aw,"wb",buffering=0) as analog,os.fdopen(out_read,"rb",buffering=0) as output:
                client=SessionContract(args.runner,renode,analog,output)
            case={"name":name};report["cases"].append(case)
            request = None
            def command(text):
                if text.startswith("readback ") and name != "normalized-bypass":
                    fields=list(map(int,text.split()[1:]))
                    content=b"".join(word.to_bytes(4,"little") for word in fields[1:]).hex()
                    raw='91^done,memory=[{begin="0x20000000",offset="0x00000000",end="0x20000020",contents="'+content+'"}]'
                    selected = request["token"] if request else 1000003
                    if name=="wrong-token":selected+=1
                    if name=="legacy-token":selected=91
                    raw=raw.replace("91^",str(selected)+"^",1)
                    text=f"readback-mi {fields[0]} "+json.dumps(raw)
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
                if command("poll-result")["error"]:raise RuntimeError("CPU setup failed")
                command("advance-native");os.read(ar,4096);publish(end,volts,(index+1)*2)
                if command("poll-worker")["error"]:raise RuntimeError("Analog setup failed")
            prior=0
            try:
                if name != "disabled":
                    command("enable-readback-request")
                publish(0,0,0);command("read-worker");command("poll-worker")
                boundary(0,0,2011375,0)
                command("high-native");os.read(ar,4096);publish(2011375,0,2);command("poll-worker")
                command("commit");prior=1
                boundary(1,2011375,4010750,2.853114)
                command("prepare-adc")
                command("adc-applied 0 2853114 4010 4010")
                fields=[4027000,0x534E3035,4,1,1,3541,1,0,0]
                if name == "early-arm":
                    final=command("arm-readback")
                else:
                    command("commit");prior=2
                    boundary(2,4010750,4027000,2.8604)
                    if name not in ("unarmed","disabled"):
                        armed=command("arm-readback 1" if name=="arm-argument" else "arm-readback")
                        request=armed.get("readback_request")
                        if not armed["error"]:
                            if request!={"token":1000003,"command":"1000003-data-read-memory-bytes 0x20000000 32"}:raise RuntimeError("Wrong native request")
                    if name=="arm-argument":final=armed
                    elif name=="duplicate-arm":final=command("arm-readback")
                    elif name=="commit-pending":final=command("commit")
                    elif name=="other-command":final=command("inspect-native")
                    else:
                        if name=="timeout":time.sleep(2.01)
                        if name=="near-deadline":time.sleep(1.1)
                        final=command("readback "+" ".join(map(str,fields)))
                        if name=="duplicate":final=command("readback "+" ".join(map(str,fields)))
                if name in ("valid","near-deadline"):
                    if final["error"] or not final["readback_verified"]:raise RuntimeError("Valid readback failed")
                    if final["commits"]!=prior:raise RuntimeError("Readback implicitly committed")
                    final=command("commit");prior+=1
                    if final["error"] or final["commits"]!=prior:raise RuntimeError("Verified commit rejected")
                else:
                    if not final["error"]:raise RuntimeError("Invalid readback accepted")
                    if command("commit")["commits"]!=prior:raise RuntimeError("Rejected readback committed")
                if final["commits"]!=prior:raise RuntimeError("Unexpected commit count")
                case["final"]=final
            finally:
                command("abort");client.close();case["records"]=client.records;case["exit"]=client.process.returncode
                case["renode_eof"]=os.read(rr,4096)==b"";os.close(rr)
                case["analog_eof"]=os.read(ar,4096)==b"";os.close(ar);os.close(out_write)
            if case["exit"] or not case["renode_eof"] or not case["analog_eof"]:raise RuntimeError("Unexpected pipe bytes or failed cleanup")
        report["status"]="passed"
    except (RuntimeError,OSError,ValueError) as error:report.update(status="failed",error=str(error))
    (args.output/"summary.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(report["status"],len(report["cases"]),"cases",report.get("error",""))
    return int(report["status"]!="passed")


if __name__=="__main__":raise SystemExit(main())
