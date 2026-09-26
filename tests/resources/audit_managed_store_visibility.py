"""Audit retained SN-021 visibility/alias VM reports without guest execution."""

import argparse
import hashlib
import json
from pathlib import Path


CASES = ("root", "document", "manifest", "lock", "record")
ALIAS_CODES = {case: "name-alias" for case in CASES}
ALIAS_CODES["record"] = "unknown-entry"


def require(condition, label):
    if not condition:
        raise ValueError(label)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def load_run(root):
    result = json.loads((root / "result.json").read_text(encoding="utf-8"))
    report_bytes = (root / "visibility.json").read_bytes()
    require(sha(report_bytes) == result["visibility_sha256"], "report-sha")
    report = json.loads(report_bytes.decode("utf-8-sig"))
    require(report["binary_sha256"] == result["sources"]["native_managed_store_probe.exe"], "binary-sha")
    require(report["project_sha256"] == result["sources"]["two-rc-project.json"], "project-sha")
    require(sha((root / "inventory.json").read_bytes()) == result["inventory_sha256"], "inventory-sha")
    for name, digest in result["sources"].items():
        require(sha((root / name).read_bytes()) == digest, "source-sha:" + name)
    require(result["snapshot"]["name"] == "SN021-authority-baseline", "snapshot")
    require(report["filesystem"] == "NTFS" and report["token"]["administrator"], "guest-environment")
    require(not report["reboot_pending"], "reboot-pending")
    return result, report


def one(observations, case):
    matches = [entry for entry in observations if entry.get("case") == case]
    require(len(matches) == 1, "case-count:" + case)
    return matches[0]


def outcome(observations, case):
    entry = one(observations, case)
    require(entry["exit_code"] == 0 and len(entry["lines"]) >= 2, "child-exit:" + case)
    return entry["lines"][-1]


def audit_success(result, report):
    require(result["status"] == report["status"] == "observed-visibility-and-alias-candidate-only", "status")
    require(report["stage"] == result["guest_stage"] == "complete", "stage")
    require(len(report["roots"]) == 11 and len({r["leaf"] for r in report["roots"]}) == 11, "roots")
    observations = report["observations"]
    require(len(observations) == 54, "observations")
    leaf = one(report["roots"], "visibility")["leaf"]
    first = "C:\\" + leaf + "\\document\\00000001.commit"
    second = "C:\\" + leaf + "\\document\\00000002.commit"
    before = one(observations, "visibility-old-complete")["snapshot"]
    pre = one(observations, "visibility-before-rename-namespace")["snapshot"]
    recovered = one(observations, "visibility-after-prepublish-kill")["snapshot"]
    post = one(observations, "visibility-after-rename-namespace")["snapshot"]
    final = one(observations, "visibility-new-complete")["snapshot"]
    require(first in before and second not in before and second not in pre and second not in recovered, "prepublish-namespace")
    require(second in post and second in final, "postpublish-namespace")
    old = before[first]
    new = final[second]
    require(old["integrity_matches"] and new["integrity_matches"], "record-integrity")
    require(old["record_body_sha256"] == old["record_digest"]
            and new["record_body_sha256"] == new["record_digest"], "record-digest")
    require(old["sha256"] == recovered[first]["sha256"] == final[first]["sha256"], "old-record-changed")
    require(old["identity"][0] == final[first]["identity"][0]
            and old["identity"][0] != new["identity"][0], "record-identity")
    for phase, case in (("before-publish", "visibility-before-rename"), ("published", "visibility-after-rename")):
        child = one(observations, case)
        require(child["exit_code"] == 99 and any(line.get("reader") == "busy" and line.get("phase") == phase
                for line in child["lines"]) and any(line.get("barrier") == phase for line in child["lines"]), "reader-barrier:" + phase)
    for case in ("visibility-second-writer-before", "visibility-second-writer-after"):
        require(outcome(observations, case)["code"] == "child-open", "writer-exclusion:" + case)
    require(outcome(observations, "visibility-reopen-old")["revision"] == 1, "reopen-old")
    require(outcome(observations, "visibility-reopen-new")["revision"] == 2, "reopen-new")
    visibility = one(observations, "serialized-reader-old-new")
    require(visibility["read_during_save"] == "busy" and visibility["prepublish_new_absent"]
            and visibility["postpublish_new_present"] and visibility["reopened_revision"] == 2, "visibility-summary")
    aliases = {}
    reparses = {}
    for case in CASES:
        alias = one(observations, "case-alias-" + case)
        require(alias["preserved"] and alias["before"] == alias["after"], "alias-preservation:" + case)
        require(alias["rejection"]["status"] == "rejected"
                and alias["rejection"]["code"] == ALIAS_CODES[case], "alias-rejection:" + case)
        require(outcome(observations, "alias-open-" + case) == alias["rejection"], "alias-child:" + case)
        aliases[case] = alias["rejection"]["code"]
        reparse = one(observations, "reparse-" + case)
        require(reparse["preserved"] and reparse["before"] == reparse["after"], "reparse-preservation:" + case)
        require(int(reparse["point"][2]) & 0x400, "reparse-attribute:" + case)
        expected = "child-open" if case == "document" else "physical"
        require(reparse["rejection"]["status"] == "rejected"
                and reparse["rejection"]["code"] == expected, "reparse-rejection:" + case)
        if case == "document":
            require(reparse["rejection"]["system"] == 0xC0000022, "document-access-denial")
        require(outcome(observations, "reparse-open-" + case) == reparse["rejection"], "reparse-child:" + case)
        reparses[case] = {"code": expected, "system": reparse["rejection"]["system"]}
    return {
        "status": "observed-visibility-and-alias-candidate-only",
        "guest_report_sha256": result["visibility_sha256"],
        "coordinator_sha256": result["sources"]["windows_managed_store.ps1"],
        "probe_source_sha256": result["sources"]["native_managed_store_probe.cpp"],
        "probe_binary_sha256": result["sources"]["native_managed_store_probe.exe"],
        "project_sha256": result["sources"]["two-rc-project.json"],
        "os_build": report["os"]["build"],
        "filesystem": report["filesystem"],
        "elapsed_ms": report["elapsed_ms"],
        "roots": len(report["roots"]),
        "observations": len(observations),
        "visibility": {"old_record_sha256": old["sha256"], "new_record_sha256": new["sha256"],
                       "old_revision_after_prepublication_kill": 1, "new_revision_after_publication_kill": 2,
                       "same_store_reader_at_both_barriers": "busy"},
        "case_alias_rejections": aliases,
        "reparse_rejections": reparses,
        "limits": "Manual native store candidate; no authenticated Save transport, production reader endpoint, disk-full, power-loss or real RC integration claim.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("success", type=Path)
    parser.add_argument("--failed", type=Path, nargs=2, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    failed = []
    failures = (("case-alias-refusal", "case-only-rename-fixture-error"),
                ("reparse-refusal", "overstrict-document-junction-result-check"))
    for root, (stage, label) in zip(args.failed, failures):
        result, report = load_run(root)
        require(result["status"] == report["status"] == "failed" and report["stage"] == stage,
                "failed-attempt:" + stage)
        if stage == "case-alias-refusal":
            require(report["error_type"] == "System.IO.IOException", "case-rename-error")
        else:
            require(report["error"] == "PROBE:reparse-not-physically-refused", "document-junction-error")
        failed.append({"guest_report_sha256": result["visibility_sha256"], "stage": stage,
                       "failure": label})
    result, report = load_run(args.success)
    summary = audit_success(result, report)
    summary["retained_failed_attempts"] = failed
    rendered = json.dumps(summary, indent=2, ensure_ascii=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    print("Visibility evidence audit passed: 11 roots, 54 observations, 5 aliases, 5 reparse points.")


if __name__ == "__main__":
    main()
