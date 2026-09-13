"""Read-only SN-018 bounded acceptance audit; does not start engines."""
import argparse
import json
from pathlib import Path

from collect_identity import audit
from measure_baseline import ROOT, sha, write
from measure_observer import session_metrics, verify


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Audit output exists")
    evidence = ROOT / "docs/experiments/evidence/SN-018-identity-summary.json"
    data = json.loads(evidence.read_text())
    hashes = dict(data["manifest"]["sha256"])
    hashes.update(data["manifest"]["preserved_sha256"])
    hashes.update(data["raw_files"])
    hashes[data["raw_summary"]["path"]] = data["raw_summary"]["sha256"]
    verify(hashes)
    if data["status"] != "passed" or len(data["batches"]) != 6:
        raise RuntimeError("Corrected campaign is incomplete")
    reference = json.loads((ROOT / "build/sn017/application-session-recovery-01/summary.json").read_text())
    session_count = 0
    row_count = 0
    for batch, command in zip(data["batches"], data["manifest"]["commands"]):
        report = json.loads((Path(command[-1]) / "summary.json").read_text())
        metrics = session_metrics(report, reference)
        if metrics != batch["sessions"]:
            raise RuntimeError("Recorded session metrics differ from raw reports")
        session_count += len(metrics)
        if batch["mode"] == "S":
            samples = json.loads((Path(command[-1]).parent / f"memory-{batch['index']:02}.json").read_text())
            checked = audit(samples)
            if not checked["passed"]:
                raise RuntimeError("Ancestry audit failed")
            row_count += checked["sampled_rows"]
    result = dict(task="SN-018", status="passed", complete_SN018=True,
        accepted_scope="Local Windows Debug reproducibility/performance baseline of the ADR 0014/0043 native/Python fixture composition and its fixed step, pause/resume and recreated-session scenarios.",
        method="Read-only hash, raw-session metric and recorded-ancestry revalidation. No new engines, CTest or CubeIDE execution.",
        checked_file_hashes=len(hashes), all_checked_hashes_match=True,
        corrected_campaign_sessions=session_count, fixed_checkpoints=session_count*9,
        analog_checks=session_count*10, ancestry_rows=row_count,
        reference_scenarios=["Fixed GPIO/RC/ADC stepping sequence", "Paced cooperative pause, stable inspection and resume", "Fresh recreated session and stopped reconnect"],
        reference_circuit_count=1, independent_host_count=1,
        source_evidence=dict(path=str(evidence), sha256=sha(evidence)),
        historical_inconclusive_evidence=dict(path="docs/experiments/evidence/SN-018-observer-summary.json", sha256=sha(ROOT / "docs/experiments/evidence/SN-018-observer-summary.json")),
        audit_script_sha256=sha(__file__),
        limitations=["Fixed scenarios share one circuit/firmware fixture; they are not different supported circuits.",
            "Mixed paired timing signs do not establish stable observer slowdown or zero overhead.",
            "Memory remains conservative sampled working set/private commit, not exact peaks or leak/scaling evidence.",
            "No Release/clean-machine/portable setup or complete transitive runtime closure is validated.",
            "No arbitrary firmware, physical ADC, general causal feedback, unpaced support or autonomous all-C++ simulator.",
            "Product performance targets and classroom readiness remain separate acceptance work."],
        publication="Local audit only; no commit, remote issue synchronization or publication.")
    write(args.output, result)


if __name__ == "__main__":
    main()
