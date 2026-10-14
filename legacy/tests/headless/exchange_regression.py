"""Validate native high/inspect ordering with actual Windows pipes."""
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
    names=("high","inspect","initial-inspect","initial-high","before-agreement-high","before-agreement-inspect",
           "wrong-edge","duplicate-high","duplicate-pending","commit-pending","changed-reply","missing-reply",
           "broken-high","broken-inspect","stale-reply","caller-argument","high-after-commit","inspect-after-commit","inspect-running")
    zero='{"time_ns":0,"actual_s":0,"output_v":0,"samples":0}'
    try:
        for name in names:
            directory=(args.output/name).resolve();directory.mkdir()
            rr,rw=os.pipe();ar,aw=os.pipe();out_read,out_write=os.pipe()
            with os.fdopen(rw,"wb",buffering=0) as renode,os.fdopen(aw,"wb",buffering=0) as analog,os.fdopen(out_read,"rb",buffering=0) as output:
                client=SessionContract(args.runner,renode,analog,output)
            case={"name":name,"wire":[]};report["cases"].append(case)
            def command(text):
                try:return client.command(text)
                except RuntimeError:
                    if not client.records[-1]["snapshot"]["error"]:raise
                    return client.records[-1]["snapshot"]
            def publish(text):os.write(out_write,(text+"\n").encode("ascii"))
            def exchange(verb):
                value=command(verb+"-native")
                if not value["error"]:
                    actual=os.read(ar,4096).decode("ascii");case["wire"].append(actual)
                    if actual!=verb+"\n":raise RuntimeError("Wrong worker command")
                return value
            prior_commits=0
            try:
                publish(zero);command("read-worker");command("poll-worker")
                target=2010875 if name=="wrong-edge" else 2011375
                good=json.dumps({"time_ns":target,"actual_s":target*1e-9,"output_v":0,"samples":2},separators=(",",":"))
                if name.startswith("initial-"):
                    final=exchange("inspect" if name=="initial-inspect" else "high")
                    if not final["error"]:publish(zero);final=command("poll-worker")
                else:
                    path=directory/"grant.txt"
                    command(f'grant-native 5000000 0 "{path.as_posix()}"');os.read(rr,4096)
                    Path(str(path)+".ready").write_text("active",encoding="utf-8");command("poll-ready")
                    if name=="inspect-running":final=exchange("inspect")
                    else:
                        command(f"stop-native {target}");os.read(rr,4096)
                        path.write_text(f"start=0\nrequested=5000000\nend={target}\nreason=cancelled\nunused={5000000-target}\nsinks={target}\ncancelled=True",encoding="utf-8")
                        if command("poll-result")["error"]:raise RuntimeError("CPU setup failed")
                        if name.startswith("before-agreement-"):final=exchange(name.removeprefix("before-agreement-"))
                        else:
                            command("advance-native");os.read(ar,4096);publish(good)
                            if command("poll-worker")["error"]:raise RuntimeError("Analog setup failed")
                            if name.endswith("after-commit"):command("commit");prior_commits=1
                            if name.startswith("broken-"):os.close(ar);ar=None
                            if name=="stale-reply":publish(good)
                            verb="inspect" if name in ("inspect","broken-inspect","inspect-after-commit") else "high"
                            final=command("high-native 1") if name=="caller-argument" else exchange(verb)
                            if not final["error"]:
                                if name=="duplicate-pending":final=command("inspect-native")
                                elif name=="commit-pending":final=command("commit")
                                else:
                                    if name=="missing-reply":time.sleep(2.01)
                                    else:publish(good.replace('"output_v":0','"output_v":1') if name=="changed-reply" else good)
                                    final=command("poll-worker")
                                    if name=="duplicate-high":final=command("high-native")
                success=name in ("high","inspect","initial-inspect","inspect-after-commit")
                if success:
                    if final["error"] or final["worker_status"]!="ready":raise RuntimeError("Valid exchange failed")
                elif not final["error"]:raise RuntimeError("Invalid exchange accepted")
                if final["commits"]!=prior_commits:raise RuntimeError("Exchange committed")
                case["final"]=final
            finally:
                command("abort");client.close();case["records"]=client.records;case["exit"]=client.process.returncode
                case["renode_eof"]=os.read(rr,4096)==b"";os.close(rr)
                if ar is not None:case["analog_eof"]=os.read(ar,4096)==b"";os.close(ar)
                os.close(out_write)
            if case["exit"] or not case["renode_eof"] or case.get("analog_eof") is False:raise RuntimeError("Pipe leak or extra command")
        report["status"]="passed"
    except (RuntimeError,OSError,ValueError) as error:report.update(status="failed",error=str(error))
    (args.output/"summary.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(report["status"],len(report["cases"]),"cases",report.get("error",""))
    return int(report["status"]!="passed")


if __name__=="__main__":raise SystemExit(main())
