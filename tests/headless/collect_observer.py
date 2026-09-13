"""Preserve functional results and audit observer membership separately."""
import argparse
from collections import Counter
import json
from pathlib import Path

from measure_baseline import ROOT, sha, write

# Diagnostic vocabulary only: names do not prove ownership and are not a filter.
EXPECTED_NAMES = {"python.exe", "conhost.exe", "simnodus_fixture_process.exe",
                  "renode.exe", "simnodus_session_contract.exe", "arm-none-eabi-gdb.exe",
                  "simnodus_rc_fixture.exe", "e05_control.exe"}


def audit(path):
    samples = json.loads(path.read_text())
    unexpected = Counter()
    affected = []
    for index, sample in enumerate(samples):
        names = [p["name"] for p in sample["processes"] if p["name"] not in EXPECTED_NAMES]
        unexpected.update(names)
        if names:
            affected.append(dict(index=index, elapsed_ns=sample["elapsed_ns"],
                                 unexpected_process_records=len(names)))
    return dict(path=str(path), sha256=sha(path), unexpected_names=dict(unexpected),
                affected_samples=affected, sample_count=len(samples),
                query_errors=sum(len(s["errors"]) for s in samples))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Evidence already exists")
    raw = args.input / "summary.json"
    data = json.loads(raw.read_text())
    audits = [audit(p) for folder in (ROOT / "build/sn018/baseline-01", args.input)
              for p in sorted(folder.glob("memory-*.json"))]
    contaminated = [a["path"] for a in audits if a["unexpected_names"]]
    evidence = dict(task="SN-018", status="inconclusive" if contaminated else "needs_review",
        complete_SN018=False, engine_validation_status=data["status"],
        raw_summary=dict(path=str(raw), sha256=sha(raw)), measurements=data,
        membership_audit=audits, contaminated_memory_files=contaminated,
        assessment=[
            "All 36 engine sessions passed; raw status passed is functional acceptance, not observer validity.",
            "Observer batch 3 included unrelated desktop processes for 80 samples and 12562 query errors.",
            "Batch 3 memory and pair 2 overhead attribution are invalid; the predeclared three-pair control is inconclusive.",
            "No pair is removed or replaced. Other raw differences are descriptive, not a corrected campaign.",
            "The sampler derives ancestry from numeric parent PIDs without creation-time identity checks.",
            "PID reuse/stale ancestry is a plausible mechanism; raw samples omit parent/creation times, so the exact chain is unproven.",
            "No unexpected names were observed in earlier baseline samples or observer batches 2/6; this is not proof of ownership.",
            "Fix and adversarially test process identity/ancestry before a newly predeclared control; do not use name filtering as ownership proof."],
        collector_sha256=sha(__file__))
    write(args.output, evidence)


if __name__ == "__main__":
    main()
