"""Collect compact SN-018 evidence without rewriting raw measurements."""
import argparse
import json
from pathlib import Path
from measure_baseline import sha, write, describe


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--host-cpu", required=True, help="Recorded CPU model and logical processor count")
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Evidence output already exists")
    data = json.loads((args.input / "summary.json").read_text())
    manifest = json.loads((args.input / "manifest.json").read_text())
    data.update(task="SN-018", slice="local-debug-baseline-1", manifest=manifest,
                host_cpu=args.host_cpu,
                host_query_limitation="CIM denied; CPU name recovered from local registry, logical count from environment",
                power_scheme="Balanced (381b4222-f694-41f0-9685-ff5bb260df2e)")
    for batch in data["batches"]:
        samples = json.loads((args.input / f"memory-{batch['index']:02}.json").read_text())
        peaks = {}
        for sample in samples:
            for process in sample["processes"]:
                key = str(process["pid"]) + ":" + process["name"]
                peak = peaks.setdefault(key, dict(working_set=0, private_bytes=0, samples=0))
                peak["samples"] += 1
                for field in ("working_set", "private_bytes"):
                    peak[field] = max(peak[field], process[field])
        batch["per_process_sampled_peaks"] = peaks
        batch["sample_gap_ns"] = describe([b["elapsed_ns"] - a["elapsed_ns"]
                                             for a, b in zip(samples, samples[1:])])
        del batch["sample_gap_ns"]["values"]
    data["raw_files"] = {str(p): sha(p) for p in args.input.rglob("*") if p.is_file()}
    data["collector_sha256"] = sha(__file__)
    data["limitations"] = [
        "One uncontrolled Windows host, Debug binaries, no warm-up; no Release/clean-machine or scaling evidence.",
        "18 sessions are nested in three batches; no tail-percentile or independent-host inference.",
        "Batch wall includes preparation, waits, cleanup and up to one sample interval of exit detection delay.",
        "Pause wall ends after stop readback, before stability wait and joint commit; it is not interrupt-to-commit latency.",
        "Memory is a sampled process-tree lower bound; shared working sets can be counted more than once.",
        "Short-lived children and reparented processes can be missed; PID/name maxima are not allocator peaks or leak evidence.",
        "Observer overhead and host load were not isolated or measured against an unsampled control.",
        "No engine/profile changes, new IDE run, instrumentation implementation or general autonomous C++ simulator approval."]
    write(args.output, data)


if __name__ == "__main__":
    main()
