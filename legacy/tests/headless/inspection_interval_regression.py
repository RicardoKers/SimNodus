"""Validate native observation-interval release and the unchanged deadline using fixture pipes."""
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
    names=("valid","repeated-poll","release-before-first","early-second","wait-commit","wait-other",
           "release-extra","release-twice","expired-wait","shared-budget","late-second","wrong-second",
           "replay-first","changed-second","unarmed-release")
    try:
        for name in names:
            directory=(args.output/name).resolve();directory.mkdir()
            rr,rw=os.pipe();ar,aw=os.pipe();out_read,out_write=os.pipe()
            with os.fdopen(rw,"wb",buffering=0) as renode,os.fdopen(aw,"wb",buffering=0) as analog,os.fdopen(out_read,"rb",buffering=0) as output:
                client=SessionContract(args.runner,renode,analog,output)
            case={"name":name};report["cases"].append(case)
            request = None
            registers='93^done,register-values=[{number="0",value="0xdd5"},{number="13",value="0x20004fd8"},{number="14",value="0x80001b5"}]'
            inspection="verify-inspection "+json.dumps(registers)+" "+json.dumps(registers)
            def command(text):
                if text.startswith("readback ") and name != "normalized-bypass":
                    fields=list(map(int,text.split()[1:]))
                    content=b"".join(word.to_bytes(4,"little") for word in fields[1:]).hex()
                    raw='91^done,memory=[{begin="0x20000000",offset="0x00000000",end="0x20000020",contents="'+content+'"}]'
                    selected = request["token"] if request else 1000003
                    if name=="wrong-token":selected+=1
                    if name=="legacy-token":selected=91
                    raw=raw.replace("91^",str(selected)+"^",1)
                    time_token = request["time_token"] if request else 2000003
                    stream = r'@"[Domain = Elapsed Virtual Time: 00:00:00.004027000\r\n"'
                    completion = str(time_token) + "^done"
                    if name=="wrong-time-token":completion=str(time_token+1)+"^done"
                    if name=="missing-done":completion=""
                    if name=="error-done":completion=str(time_token)+'^error,msg="failed"'
                    if name=="changed-time":stream=stream.replace("004027000","004027001")
                    if name=="bad-minute":stream=stream.replace("00:00:00", "00:60:00")
                    if name=="duplicate-stream":stream+=stream
                    if name=="actual-newline":stream=stream.replace(r"\r\n", "\r\n")
                    text = "readback-timed " + " ".join(json.dumps(value) for value in (raw,stream,completion))
                    if name=="normalized-time":text=f"readback-mi {fields[0]} "+json.dumps(raw)
                if name=="before-readback" and text.startswith("readback-timed "):text="arm-inspection"
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
                command("enable-inspection-interval")
                publish(0,0,0);command("read-worker");command("poll-worker")
                boundary(0,0,2011375,0)
                command("high-native");os.read(ar,4096);publish(2011375,0,2);command("poll-worker")
                command("commit");prior=1
                boundary(1,2011375,4010750,2.853114)
                command("prepare-adc");command("adc-applied 0 2853114 4010 4010")
                command("commit");prior=2
                boundary(2,4010750,4027000,2.8604)
                request=command("arm-readback")["readback_request"]
                fields=[4027000,0x534E3035,4,1,1,3541,1,0,0]
                final=command("readback "+" ".join(map(str,fields)))
                if final["error"]:raise RuntimeError("Timed readback setup failed")
                first=registers.replace("93^","3000003^")
                second=registers.replace("93^","4000003^")
                if name=="unarmed-release":final=command("release-inspection")
                else:
                    if name=="shared-budget":time.sleep(1.05)
                    if command("arm-inspection")["error"]:raise RuntimeError("Arming failed")
                    if name=="release-before-first":final=command("release-inspection")
                    else:
                        started=time.monotonic()
                        waiting=command("inspection-result "+json.dumps(first))
                        if waiting["error"] or waiting["inspection_step"]!=4 or "inspection_request" in waiting or waiting["inspection_interval_ns"]:
                            raise RuntimeError("Second request disclosed before release")
                        if name=="early-second":final=command("inspection-result "+json.dumps(second))
                        elif name=="wait-commit":final=command("commit")
                        elif name=="wait-other":final=command("inspect-native")
                        elif name=="release-extra":final=command("release-inspection 1")
                        else:
                            if name=="expired-wait":time.sleep(2.01)
                            if name=="shared-budget":time.sleep(.95)
                            while True:
                                final=command("release-inspection")
                                if final["error"] or final["inspection_step"]!=4:break
                                if "inspection_request" in final:raise RuntimeError("Pending poll disclosed request")
                                time.sleep(.001 if name=="repeated-poll" else max(.001,final["inspection_wait_ms"]/1000))
                            case["host_release_elapsed_s"]=time.monotonic()-started
                            if not final["error"]:
                                if final["inspection_interval_ns"]<100000000 or case["host_release_elapsed_s"]<.1:raise RuntimeError("Interval released early")
                                if final["inspection_request"]!={"token":4000003,"command":"4000003-data-list-register-values x 0 13 14"}:raise RuntimeError("Wrong second request")
                                if name=="release-twice":final=command("release-inspection")
                                else:
                                    if name=="late-second":time.sleep(2.01)
                                    if name=="wrong-second":second=second.replace("4000003^","4000004^")
                                    if name=="replay-first":second=first
                                    if name=="changed-second":second=second.replace("0xdd5","0xdd6")
                                    final=command("inspection-result "+json.dumps(second))
                if name in ("valid","repeated-poll"):
                    if final["error"] or not final["inspection_verified"]:raise RuntimeError("Valid readback failed")
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
