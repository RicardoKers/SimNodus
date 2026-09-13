"""Validate exact MI mailbox frames and native verification using actual fixture pipes."""
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
    names=("valid","uppercase","token","address","offset","end","hex","short","extra","duplicate-frame",
           "trailing","error","normalized-bypass","missing","duplicate","early","unprepared","pending-adc",
           "disabled","duplicate-enable","late-enable","negative-time","overflow-time")
    try:
        for name in names:
            directory=(args.output/name).resolve();directory.mkdir()
            rr,rw=os.pipe();ar,aw=os.pipe();out_read,out_write=os.pipe()
            with os.fdopen(rw,"wb",buffering=0) as renode,os.fdopen(aw,"wb",buffering=0) as analog,os.fdopen(out_read,"rb",buffering=0) as output:
                client=SessionContract(args.runner,renode,analog,output)
            case={"name":name};report["cases"].append(case)
            def command(text):
                if text.startswith("readback ") and name != "normalized-bypass":
                    fields=list(map(int,text.split()[1:]))
                    if name=="negative-time":fields[0]-=2**64
                    if name=="overflow-time":fields[0]+=2**64
                    content=b"".join(word.to_bytes(4,"little") for word in fields[1:]).hex()
                    if name=="uppercase":content=content.upper()
                    raw='91^done,memory=[{begin="0x20000000",offset="0x00000000",end="0x20000020",contents="'+content+'"}]'
                    if name=="token":raw=raw.replace("91^","92^")
                    if name=="address":raw=raw.replace('begin="0x20000000"','begin="0x20000004"')
                    if name=="offset":raw=raw.replace('offset="0x00000000"','offset="0x00000001"')
                    if name=="end":raw=raw.replace('end="0x20000020"','end="0x20000024"')
                    if name=="hex":raw=raw.replace('contents="3','contents="g')
                    if name=="duplicate-frame":raw+=raw
                    if name=="trailing":raw+=" "
                    if name=="error":raw='91^error,msg="Cannot access memory"'
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
                if name not in ("disabled","late-enable"):
                    command("enable-readback-mi")
                publish(0,0,0);command("read-worker");command("poll-worker")
                if name in ("duplicate-enable","late-enable"):
                    final=command("enable-readback-mi")
                else:
                    boundary(0,0,2011375,0)
                    command("high-native");os.read(ar,4096);publish(2011375,0,2);command("poll-worker")
                    command("commit");prior=1
                    boundary(1,2011375,4010750,2.853114)
                    if name!="unprepared":
                        command("prepare-adc")
                        if name!="pending-adc":command("adc-applied 0 2853114 4010 4010")
                    fields=[4027000,0x534E3035,4,1,1,3541,1,0,0]
                    if name in ("early","pending-adc"):
                        final=command("readback "+" ".join(map(str,fields)))
                    else:
                        command("commit");prior=2
                        boundary(2,4010750,4027000,2.8604)
                        if name=="missing":final=command("commit")
                        else:
                            if name=="wrong-code":fields[5]=3540
                            if name=="wrong-magic":fields[1]=0
                            if name=="wrong-time":fields[0]-=1
                            if name=="short":fields.pop()
                            if name=="extra":fields.append(0)
                            if name=="negative":fields[5]=-1
                            if name=="overflow":fields[5]=18446744073709551616
                            final=command("readback "+" ".join(map(str,fields)))
                            if name=="duplicate":final=command("readback "+" ".join(map(str,fields)))
                if name in ("valid","uppercase"):
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
