"""Verify and consolidate the bounded 35-session IDE integration evidence."""
import argparse
import json
from pathlib import Path

import run as e


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--matrix", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    opts = parser.parse_args()
    matrix = opts.matrix.resolve()
    manifest = json.loads((matrix / "matrix.json").read_text(encoding="utf-8"))
    e.check(manifest["status"] == "passed" and len(manifest["runs"]) == 35,
            "The complete matrix must pass before consolidation")
    report = {"status": "extended_steps_fault_arbitration_and_fresh_recovery_verified",
              "complete_e05_profile": False, "raw_evidence": matrix.relative_to(e.ROOT).as_posix(),
              "matrix_sha256": e.sha(matrix / "matrix.json"), "runs": [],
              "scope": "Initially paced arbitration, extended paced faults, fresh recovery and lifecycle controls",
              "limits": ["Initial CPU pacing is released only after retained MI interruption and pre-ADC observation.",
                         "Not general unpaced support, solver nonconvergence recovery or rollback.",
                         "The full E-05 capability decision and SN-017 remain pending."]}
    reference = None
    for entry in manifest["runs"]:
        folder = matrix / entry["name"]
        summary = folder / "summary.json"
        data = json.loads(summary.read_text(encoding="utf-8"))
        e.check(entry["exit_code"] == 0 and data["status"] == entry["expected_status"]
                and e.sha(summary) == entry["summary_sha256"], "Matrix result changed")
        e.check(data["ide_exit"] == 0 and data["workbench_close_accepted"]
                and data["listeners_removed"] and data["guard_listener_removed"], "Cleanup failed")
        hashes = {key: data[key] for key in ("inputs_sha256", "sources_sha256", "guard_sha256")}
        if data["profile"] == "joint":
            hashes.update({key: data[key] for key in ("joint_inputs_sha256", "cooperative_inputs_sha256")})
            if reference is None:
                reference = hashes
                report["matching_joint_hashes"] = hashes
            e.check(hashes == reference, "Joint matrix source/binary mismatch")
        result = {"name": entry["name"], "raw_evidence": folder.relative_to(e.ROOT).as_posix(),
                  "summary_sha256": entry["summary_sha256"], "status": data["status"],
                  "ide_exit": data["ide_exit"], "close_accepted": True, "listeners_removed": True}
        if data["profile"] == "joint":
            result["cooperative"] = data["cooperative"]
            result["cpu_acknowledgements"] = sum("acknowledged_outcome" in p for p in data["guard"]["packets"])
            result["retained_interrupts"] = sum(bool(p.get("interrupt_intercepted")) for p in data["guard"]["packets"])
            transcript = (folder / "runner.txt").read_text(encoding="utf-8")
            e.check("attachTimeNs=0" in transcript, "Fresh IDE attach was not at zero")
            if data["joint_race"]:
                e.check(data["joint_race_initial_pace"] and data["race_host_poll_ms"] == 1
                        and data["cooperative"]["pacing_released_after_retained_interrupt"],
                        "Initially paced arbitration evidence missing")
            if data["joint_lifecycle"]:
                result["first_session"] = data["joint_first_session"]["cooperative"]
                result["disconnected_analog"] = data["joint_disconnected_analog"]
                e.check(data["joint_first_session"]["listeners_removed"]
                        and data["joint_first_session"]["guard_listener_removed"], "Old lifecycle resources remain")
        else:
            baseline = json.loads((e.ROOT / "build/sn016/resume-normal-04/summary.json").read_text(encoding="utf-8"))
            e.check(data["stops"] == baseline["stops"] and data["circuits"] == baseline["circuits"],
                    "Default lifecycle differs from reference")
            result.update(exact_reference="build/sn016/resume-normal-04", hashes=hashes)
        report["runs"].append(result)
    report["completed_utc"] = data["completed_utc"]
    # Retain the unsuccessful candidates, rather than rewriting their outcomes.
    report["predecessors"] = []
    for name in ("steps-fault-matrix-01", "steps-fault-matrix-02", "steps-fault-matrix-03", "steps-fault-matrix-04"):
        folder = e.ROOT / "build/sn016" / name
        prior = json.loads((folder / "matrix.json").read_text(encoding="utf-8"))
        failed = folder / prior["runs"][-1]["name"]
        data = json.loads((failed / "summary.json").read_text(encoding="utf-8"))
        e.check(prior["status"] == data["status"] == "failed", "Predecessor outcome changed")
        report["predecessors"].append({"raw_evidence": folder.relative_to(e.ROOT).as_posix(),
            "status": "failed", "error": prior["error"], "completed_passes": len(prior["runs"]) - 1,
            "failed_summary_sha256": e.sha(failed / "summary.json"),
            "retained_checkpoints": len(data["cooperative"]["checkpoints"]),
            "joint_pause_published": (failed / "joint-stopped-7").exists(),
            "verified_notification_published": (failed / "joint-notified").exists()})
    report["isolated_predecessor"] = "build/sn016/steps-race-analog-01"
    with opts.output.open("x", encoding="utf-8") as output:
        output.write(json.dumps(report, indent=2) + "\n")
    print("Verified and consolidated 35 IDE sessions; full E-05 remains open.")


if __name__ == "__main__":
    main()
