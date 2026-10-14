"""Exercise ADC preparation/confirmation gates using actual fixture pipes."""
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
    names=("valid","initial","wrong-boundary","missing-high","before-agreement","negative-voltage","over-rail",
           "duplicate-prepare","commit-pending","wrong-channel","wrong-voltage","wrong-before","changed-time",
           "late","unsolicited-confirmation","duplicate-confirmation","repeat-preparation","caller-voltage")
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
            def publish(ns,volts,samples):
                os.write(out_write,(json.dumps({"time_ns":ns,"actual_s":ns*1e-9,"output_v":volts,"samples":samples},separators=(",",":"))+"\n").encode())
            def boundary(index,start,end,volts):
                path=directory/f"grant-{index}.txt"
                command(f'grant-native 5000000 0 "{path.as_posix()}"');os.read(rr,4096)
                Path(str(path)+".ready").write_text("active",encoding="utf-8");command("poll-ready")
                command(f"stop-native {end}");os.read(rr,4096)
                path.write_text(f"start={start}\nrequested=5000000\nend={end}\nreason=cancelled\nunused={5000000-end+start}\nsinks={end}\ncancelled=True",encoding="utf-8")
                if command("poll-result")["error"]:raise RuntimeError("CPU setup failed")
                if index==1 and name=="before-agreement":return
                command("advance-native");os.read(ar,4096);publish(end,volts,(index+1)*2)
                if command("poll-worker")["error"]:raise RuntimeError("Analog setup failed")
            prior=0
            try:
                publish(0,0,0);command("read-worker");command("poll-worker")
                if name=="initial":final=command("prepare-adc")
                else:
                    boundary(0,0,2011375,0)
                    if name!="missing-high":
                        command("high-native");os.read(ar,4096);publish(2011375,0,2);command("poll-worker")
                    if name=="wrong-boundary":final=command("prepare-adc")
                    else:
                        command("commit");prior=1
                        volts=-0.1 if name=="negative-voltage" else 3.4 if name=="over-rail" else 2.853114
                        boundary(1,2011375,4010750,volts)
                        if name=="unsolicited-confirmation":final=command("adc-applied 0 2853114 4010 4010")
                        else:
                            final=command("prepare-adc 1") if name=="caller-voltage" else command("prepare-adc")
                            if not final["error"]:
                                if final["adc_request"]!={"channel":0,"microvolts":2853114,"expected_code":3541,"time_ns":4010750}:raise RuntimeError("Wrong prepared input")
                                if name=="duplicate-prepare":final=command("prepare-adc")
                                elif name=="commit-pending":final=command("commit")
                                else:
                                    if name=="late":time.sleep(2.01)
                                    fields=[1 if name=="wrong-channel" else 0,2853115 if name=="wrong-voltage" else 2853114,4011 if name=="wrong-before" else 4010,4011 if name=="changed-time" else 4010]
                                    final=command("adc-applied "+" ".join(map(str,fields)))
                                    if name=="duplicate-confirmation":final=command("adc-applied 0 2853114 4010 4010")
                                    if name=="repeat-preparation":final=command("prepare-adc")
                if name=="valid":
                    if final["error"] or final["adc_pending"] or final["adc_confirmations"]!=1:raise RuntimeError("Valid confirmation failed")
                elif not final["error"]:raise RuntimeError("Invalid ADC transition accepted")
                if final["commits"]!=prior:raise RuntimeError("ADC transition committed")
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
