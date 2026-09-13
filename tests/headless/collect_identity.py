"""Audit every recorded ancestry path before accepting corrected observer evidence."""
import argparse
import json
from pathlib import Path

from measure_baseline import describe, sha, write


def audit(samples):
    identities = {}
    issues = []
    for row in samples:
        for p in row["processes"]:
            key = (p["pid"], p["created"])
            parent = (p["parent_pid"], p["parent_created"]) if p["parent_pid"] is not None else None
            if key in identities and identities[key] != parent:
                issues.append("Inconsistent parent identity: " + repr(key))
            identities[key] = parent
    for index, row in enumerate(samples):
        root = (row["root_pid"], row["root_created"])
        for p in row["processes"]:
            key = (p["pid"], p["created"])
            visited = set()
            while key != root:
                if key in visited or key not in identities or identities[key] is None:
                    issues.append(f"Sample {index}: unproven ancestry {key}")
                    break
                visited.add(key)
                parent = identities[key]
                if key[1] <= parent[1] or key[1] > row["snapshot_filetime"]:
                    issues.append(f"Sample {index}: invalid creation chronology {key}")
                    break
                key = parent
    return dict(passed=not issues, issues=issues, verified_process_identities=len(identities),
                sampled_rows=sum(len(s["processes"]) for s in samples))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Evidence output exists")
    source = args.input / "summary.json"
    result = json.loads(source.read_text())
    result["raw_summary"] = dict(path=str(source), sha256=sha(source))
    result["ancestry_audits"] = []
    for batch in result["batches"]:
        if batch["mode"] != "S":
            continue
        path = args.input / f"memory-{batch['index']:02}.json"
        samples = json.loads(path.read_text())
        validation = audit(samples)
        validation.update(path=str(path), sha256=sha(path))
        validation["sample_gap_ns"] = describe([b["elapsed_ns"]-a["elapsed_ns"] for a,b in zip(samples,samples[1:])])
        del validation["sample_gap_ns"]["values"]
        validation["rejected_candidates"] = [p for s in samples for p in s["rejected"]]
        result["ancestry_audits"].append(validation)
    if not all(a["passed"] for a in result["ancestry_audits"]):
        result["status"] = "inconclusive"
    result["collector_sha256"] = sha(__file__)
    result["limitations"] = [
        "Conservative sampled attribution; short-lived or unverifiable processes can be missed.",
        "Retained handles and identity checks are part of this observer's measured cost.",
        "Three adjacent pairs on one uncontrolled Debug host do not establish causal slowdown or equivalence.",
        "No name allowlist, general capability expansion or autonomous C++ orchestration is implied.",
        "Historical PID-only campaigns remain unchanged; this evidence does not retroactively certify them."]
    write(args.output, result)


if __name__ == "__main__":
    main()
