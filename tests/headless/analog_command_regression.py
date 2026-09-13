"""Test native analog command causality and inherited-pipe cleanup on Windows."""
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
    names=("valid","before-grant","before-ack","unsolicited-response","duplicate-request","broken-pipe",
           "caller-target","wrong-time","nonfinite","late-response","commit-pending","duplicate-response","recreated-grant")
    try:
        for name in names:
            directory=(args.output/name).resolve();directory.mkdir()
            rr,rw=os.pipe();ar,aw=os.pipe()
            with os.fdopen(rw,"wb",buffering=0) as renode,os.fdopen(aw,"wb",buffering=0) as analog:
                client=SessionContract(args.runner,renode,analog)
            case={"name":name,"wire":[]};report["cases"].append(case)
            def command(text):
                try:return client.command(text)
                except RuntimeError:
                    if not client.records[-1]["snapshot"]["error"]:raise
                    return client.records[-1]["snapshot"]
            def setup(index=0):
                path=directory/f"grant-{index}.txt";start=index*2010875;end=start+2010875
                command(f'grant-native 5000000 0 "{path.as_posix()}"');os.read(rr,4096)
                Path(str(path)+".ready").write_text("active",encoding="utf-8");command("poll-ready")
                if name=="before-ack":return command("advance-native")
                command(f"stop-native {end}");os.read(rr,4096)
                path.write_text(f"start={start}\nrequested=5000000\nend={end}\nreason=cancelled\nunused=2989125\nsinks={end}\ncancelled=True",encoding="utf-8")
                value=command("poll-result")
                if value["error"]:raise RuntimeError("Setup failed")
                return value
            def advance(target):
                snap=command("advance-native")
                if snap["error"]:return snap
                actual=os.read(ar,4096).decode("ascii");case["wire"].append(actual)
                if actual!=f"advance {target}\n":raise RuntimeError("Wrong native target")
                return snap
            try:
                if name=="before-grant":final=command("advance-native")
                else:
                    final=setup()
                    if not final["error"]:
                        if name=="unsolicited-response":final=command("analog 2010875 0.002010875 0")
                        elif name=="caller-target":final=command("advance-native 123")
                        else:
                            if name=="broken-pipe":os.close(ar);ar=None
                            final=advance(2010875)
                            if not final["error"]:
                                if name=="duplicate-request":final=command("advance-native")
                                elif name=="commit-pending":final=command("commit")
                                else:
                                    if name=="late-response":time.sleep(2.01)
                                    text="analog 2010876 0.002010876 0" if name=="wrong-time" else "analog 2010875 0.002010875 nan" if name=="nonfinite" else "analog 2010875 0.002010875 0"
                                    final=command(text)
                                    if name=="duplicate-response":final=command(text)
                                    elif not final["error"]:
                                        final=command("commit")
                                        if name=="recreated-grant":
                                            setup(1);advance(4021750)
                                            command("analog 4021750 0.004021750 0")
                                            final=command("commit")
                if name in ("valid","recreated-grant"):
                    count=2 if name=="recreated-grant" else 1
                    if final["error"] or final["commits"]!=count or final["advance_writes"]!=count or final["analog_pending"]:raise RuntimeError("Valid advance failed")
                elif not final["error"] or final["commits"]:raise RuntimeError("Invalid sequence committed")
                case["final"]=final
            finally:
                try:command("abort")
                finally:
                    client.close();case["records"]=client.records;case["exit"]=client.process.returncode
                    case["renode_eof"]=os.read(rr,4096)==b"";os.close(rr)
                    if ar is not None:case["analog_eof"]=os.read(ar,4096)==b"";os.close(ar)
            if case["exit"] or not case["renode_eof"] or case.get("analog_eof") is False:raise RuntimeError("Pipe leak or extra command")
        report["status"]="passed"
    except (RuntimeError,OSError,ValueError) as error:report.update(status="failed",error=str(error))
    (args.output/"summary.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(report["status"],len(report["cases"]),"cases",report.get("error",""))
    return int(report["status"]!="passed")


if __name__=="__main__":raise SystemExit(main())
