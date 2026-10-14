"""Audit retained SN-021 authenticated-request VM evidence as inert bytes."""

import argparse
import hashlib
import json
from pathlib import Path
import struct


CASES = (
    "open-authorized", "save-authorized", "reconcile-committed",
    "stale-version", "operation-collision", "unauthorized-principal",
    "wrong-generation", "wrong-document", "wrong-run", "malformed-magic",
    "malformed-length", "oversized-message", "save-return-to-a",
    "aba-old-token", "reconcile-absent-active-run", "old-run-after-restart",
    "older-receipt-after-restart", "save-lost-reply", "reconcile-lost-reply",
)
DECISIONS = (
    "ok", "ok", "ok", "request", "request", "principal",
    "request-generation", "request-document", "request-run", "request-magic",
    "request-length", "request-oversized", "ok", "request", "not-observed",
    "request-run", "ok", "reply-dropped", "ok",
)
SERVER = dict.fromkeys(CASES[:15], "requests") | dict.fromkeys(CASES[15:17], "restarted") | dict.fromkeys(CASES[17:], "lost-reply")
LIMIT = 2 * 1024 * 1024


def require(value, label):
    if not value:
        raise ValueError(label)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def sid(data):
    require(len(data) >= 8 and data[0] == 1 and len(data) == 8 + 4 * data[1], "sid-format")
    authority = int.from_bytes(data[2:8], "big")
    parts = [str(struct.unpack_from("<I", data, 8 + 4 * n)[0]) for n in range(data[1])]
    return "S-1-" + str(authority) + ("-" + "-".join(parts) if parts else "")


def one(items, case):
    found = [item for item in items if item.get("case") == case]
    require(len(found) == 1, "case-count:" + case)
    return found[0]


def load(root):
    result = json.loads((root / "result.json").read_text(encoding="utf-8"))
    inventory_data = (root / "inventory.json").read_bytes()
    report_data = (root / "probe.json").read_bytes()
    require(sha(inventory_data) == result["inventory_sha256"], "inventory-hash")
    require(sha(report_data) == result["probe_sha256"], "probe-hash")
    inventory = json.loads(inventory_data.decode("utf-8-sig"))
    report = json.loads(report_data.decode("utf-8-sig"))
    for name, digest in result["sources"].items():
        require(sha((root / name).read_bytes()) == digest, "source-hash:" + name)
    require(result["status"] == report["status"] == "observed-authenticated-managed-request-candidate-only"
            and result["stage"] == report["stage"] == "complete", "status")
    require(result["snapshot"]["name"] == "SN021-authority-baseline", "snapshot")
    require(report["filesystem"] == "NTFS" and report["token"]["administrator"]
            and not report["reboot_pending"], "guest-environment")
    staged = {"binary_sha256": result["sources"]["native_managed_request_probe.exe"],
              "store_binary_sha256": result["sources"]["native_managed_store_probe.exe"],
              "project_sha256": result["sources"]["two-rc-project.json"]}
    require(inventory["status"] == "observed" and inventory["staged"] == report["staged"] == staged,
            "staged-inputs")
    require(all(report[k] == v for k, v in staged.items()), "private-inputs")
    artifacts = report["artifacts"]
    require(len(artifacts) == len(result["artifacts"]) == 82, "artifact-count")
    for entry in artifacts:
        name = entry["name"]
        require(Path(name).name == name and name in result["artifacts"], "artifact-name")
        data = (root / "artifacts" / name).read_bytes()
        require(sha(data) == entry["sha256"] == result["artifacts"][name]["sha256"]
                and len(data) == result["artifacts"][name]["bytes"], "artifact-hash:" + name)
    return result, report


def token_record(token_hex, data, revision, snapshot):
    token = bytes.fromhex(token_hex)
    require(len(token) == 108 and token[:32] == data[32:64], "token-scope")
    require(struct.unpack_from("<I", token, 32)[0] == revision
            and token[36:52] == data[64:80] and token[52:84] == data[-32:], "token-record")
    identity = snapshot["identity"]
    require(f"{struct.unpack_from('<Q', token, 84)[0]:016x}" == identity[0]
            and token[92:108].hex() == identity[1], "token-file-identity")


def request_fields(data, operation):
    require(len(data) >= 80 and data[:8] == b"SN21SAVE"
            and struct.unpack_from("<HHI", data, 8) == (3, operation, len(data) - 32),
            "request-frame")
    fields = {"run": data[32:48], "generation": data[48:64], "document": data[64:80]}
    if operation == 1:
        require(len(data) == 80, "open-request-length")
    else:
        require(len(data) >= 208 and struct.unpack_from("<I", data, 204)[0] == len(data) - 208,
                "save-request-length")
        fields.update(operation_id=data[80:96], expected=data[96:204], project=data[208:])
    return fields


def reply_fields(request, response, operation):
    require(len(response) >= 36 and response[:8] == b"SN21SAVE"
            and struct.unpack_from("<HHI", response, 8) == (3, operation | 0x8000, len(response) - 32)
            and response[16:32] == request[16:32]
            and struct.unpack_from("<I", response, 32)[0] == 0, "reply-frame")


def audit_records(root, report, cases):
    manifest = (root / "artifacts" / "generation.manifest").read_bytes()
    project = (root / "two-rc-project.json").read_bytes()
    require(manifest[:8] == b"SNST0001" and 120 <= len(manifest) <= 4096, "manifest-format")
    writer_size, client_size = struct.unpack_from("<II", manifest, 112)
    require(len(manifest) == 120 + writer_size + client_size and 12 <= writer_size <= 68
            and 12 <= client_size <= 68, "manifest-length")
    require(sid(manifest[120:120 + writer_size]) == report["principals"]["w"]
            and sid(manifest[120 + writer_size:]) == report["principals"]["c"],
            "manifest-principals")
    require(report["manifest"]["preserved"] and report["manifest"]["before"] == report["manifest"]["after"]
            and sha(manifest) == report["manifest"]["before"]["sha256"], "manifest-preservation")
    expected_projects = (project.ljust(71680, b" "), project.ljust(71680, b" ") + b" ")
    expected_operations = (bytes([1]) * 16, bytes.fromhex("51" * 16),
                           bytes.fromhex("57" * 16), bytes.fromhex("5a" * 16))
    final = report["final"]
    require(set(final) == {f"{n:08x}.commit" for n in range(1, 5)}, "final-record-set")
    records = {}
    for revision in range(1, 5):
        name = f"{revision:08x}.commit"
        data = (root / "artifacts" / name).read_bytes()
        require(268 <= len(data) <= LIMIT and data[:8] == b"SNMC0001", "record-format:" + name)
        total, reserved, number, sid_size, context_size, project_size = struct.unpack_from("<IIIIII", data, 8)
        require(total == len(data) == 268 + sid_size + context_size + project_size
                and reserved == 0 and number == revision, "record-length:" + name)
        require(data[32:64] == manifest[8:40] and data[80:96] == expected_operations[revision - 1],
                "record-scope-operation:" + name)
        require(data[236:236 + sid_size] == manifest[120 + writer_size:], "record-principal:" + name)
        context = data[236 + sid_size:236 + sid_size + context_size]
        expected_project = expected_projects[(revision + 1) % 2]
        locator = ("C:\\SN021SaveHarness-" + report["tag"]).encode("utf-8")
        require(data[236 + sid_size + context_size:-32] == expected_project, "record-project:" + name)
        require(len(context) == 52 + len(locator) and context[:4] == b"SRC1"
                and context[4:20] == bytes([42]) * 16 and context[20:28] == manifest[40:48]
                and any(context[28:44]) and struct.unpack_from("<II", context, 44) == (1, len(locator))
                and context[52:] == locator,
                "record-context:" + name)
        require(data[204:236] == hashlib.sha256(
            b"SNREQ001" + data[32:64] + data[80:96] + data[96:204]
            + data[20:32] + data[236:-32]).digest(), "request-digest:" + name)
        require(data[-32:] == hashlib.sha256(data[:-32]).digest()
                and sha(data) == final[name]["sha256"], "record-digest:" + name)
        if revision > 1:
            prior = records[revision - 1]
            require(data[96:204] == prior["token"], "record-predecessor:" + name)
        elif revision == 1:
            require(data[96:204] == bytes(108), "record-first-predecessor")
        records[revision] = {"data": data, "token": None, "sha256": sha(data),
                             "project_sha256": sha(expected_project), "context_sha256": sha(context)}
        token_case = ("open-authorized", "save-authorized", "save-return-to-a", "reconcile-lost-reply")[revision - 1]
        token = cases[token_case]["client"]["token"]
        token_record(token, data, revision, final[name])
        require(bytes.fromhex(token)[84:92] == manifest[40:48], "token-volume:" + name)
        records[revision]["token"] = bytes.fromhex(token)
    require(records[1]["project_sha256"] == records[3]["project_sha256"]
            and records[1]["sha256"] != records[3]["sha256"], "aba-distinct-record")
    for case, revision in (("reconcile-committed", 2), ("older-receipt-after-restart", 2)):
        require(cases[case]["client"]["token"] == records[revision]["token"].hex(),
                "receipt-token:" + case)
    open_request = cases["open-authorized"]["request"]
    open_fields = request_fields(open_request, 1)
    first = records[1]["data"]
    require(open_fields["generation"] + open_fields["document"] == first[32:64], "open-scope")
    open_reply = (root / "artifacts" / "open-authorized.response.bin").read_bytes()
    reply_fields(open_request, open_reply, 1)
    context_length = struct.unpack_from("<I", open_reply, 144)[0]
    project_length = struct.unpack_from("<I", open_reply, 148 + context_length)[0]
    sid_size, stored_context_size = struct.unpack_from("<II", first, 20)
    require(context_length == stored_context_size and project_length == 71680
            and len(open_reply) == 152 + context_length + project_length
            and open_reply[36:144] == records[1]["token"]
            and open_reply[148:148 + context_length] == first[236 + sid_size:236 + sid_size + context_length]
            and open_reply[152 + context_length:] == expected_projects[0], "open-exact-bytes")
    receipt_cases = (("save-authorized", 2, 2), ("reconcile-committed", 2, 3),
                     ("save-return-to-a", 3, 2), ("older-receipt-after-restart", 2, 3),
                     ("save-lost-reply", 4, 2), ("reconcile-lost-reply", 4, 3))
    for case, revision, operation in receipt_cases:
        request = cases[case]["request"]
        fields = request_fields(request, operation)
        record = records[revision]["data"]
        require(fields["generation"] + fields["document"] == record[32:64]
                and fields["operation_id"] == record[80:96]
                and fields["expected"] == records[revision - 1]["token"]
                and fields["project"] == expected_projects[(revision + 1) % 2],
                "request-record:" + case)
        if case == "save-lost-reply":
            continue
        response = (root / "artifacts" / (case + ".response.bin")).read_bytes()
        reply_fields(request, response, operation)
        require(len(response) == 192 and response[36:52] == record[80:96]
                and response[52:160] == records[revision]["token"]
                and response[160:192] == record[204:236], "receipt-exact-bytes:" + case)
        client = cases[case]["client"]
        require(client["operation_id"] == fields["operation_id"].hex()
                and client["request_digest"] == record[204:236].hex(), "client-receipt:" + case)
    return {"revisions": 4, "record_sha256": [records[n]["sha256"] for n in range(1, 5)],
            "project_sha256": [records[n]["project_sha256"] for n in range(1, 5)],
            "context_sha256": [records[n]["context_sha256"] for n in range(1, 5)],
            "manifest_sha256": sha(manifest)}


def audit_requests(root, report):
    observations = report["observations"]
    require(len(observations) == 55 and len(report["processes"]) == 24, "observation-count")
    require(len(report["roots"]) == 1, "root-count")
    principals = report["principals"]
    require(len(set(principals.values())) == 3, "distinct-principals")
    for entry in report["processes"]:
        case = entry["case"]
        role = entry["role"]
        process = one(observations, case)
        require(process["pid"] == entry["pid"] and process["role"] == role
                and process["exit_code"] == 0 and process["exited"], "process:" + case)
        raw = (root / "artifacts" / (case + "." + role + ".jsonl")).read_text(encoding="utf-8")
        reported_lines = [dict(line) for line in process["lines"]]
        if case in CASES:
            reported_lines[-1].pop("decision", None)  # Coordinator attaches the server decision later.
        require([json.loads(line) for line in raw.splitlines()] == reported_lines,
                "process-output:" + case)
    cases = {}
    for index, (case, code) in enumerate(zip(CASES, DECISIONS)):
        process = one(observations, case)
        decision_obs = one(observations, case + "-decision")
        decision = decision_obs["decision"]
        client = process["lines"][-1]
        server_name = SERVER[case]
        server_lines = one(observations, server_name)["lines"]
        matches = [line for line in server_lines if line.get("event") == "decision"
                   and line.get("connection") == decision_obs["connection"]]
        require(len(matches) == 1 and matches[0] == decision, "server-decision:" + case)
        request = (root / "artifacts" / (case + ".request.bin")).read_bytes()
        require(sha(request) == decision_obs["request_sha256"] and len(request) <= 1048785,
                "request-bytes:" + case)
        require(decision["code"] == code, "decision-code:" + case)
        require(decision["authenticated_sid"] == ("" if case == "oversized-message" else
                principals["u" if case == "unauthorized-principal" else "c"]), "decision-sid:" + case)
        require(decision["reverted"] == (case != "oversized-message"), "decision-reversion:" + case)
        if code == "ok":
            require(client["status"] == "accepted" and client["server_verified"]
                    and client["reply_status"] == 0 and decision["dispatched"]
                    and not decision["indeterminate"], "accepted:" + case)
        elif case == "save-lost-reply":
            require(client["status"] == "failed" and client["server_verified"]
                    and client["request_sent"] and client["indeterminate"]
                    and decision["indeterminate"] and decision["dispatched"]
                    and case + ".response.bin" not in {a["name"] for a in report["artifacts"]},
                    "dropped-reply")
        else:
            require(client["status"] != "accepted", "refusal-client:" + case)
        if case in ("stale-version", "operation-collision", "aba-old-token"):
            require(decision["store_system"] == (9 if case == "operation-collision" else 10),
                    "store-refusal:" + case)
        if case in ("unauthorized-principal", "wrong-generation", "wrong-document", "wrong-run",
                    "malformed-magic", "malformed-length", "oversized-message", "old-run-after-restart"):
            require(not decision["dispatched"], "pre-dispatch-refusal:" + case)
        if case.endswith("authorized") or case in ("reconcile-committed", "save-return-to-a",
                "older-receipt-after-restart", "reconcile-lost-reply"):
            require(client["token"] == decision["token"], "receipt-token:" + case)
        cases[case] = {"client": client, "decision": decision, "request": request}
    for case in CASES[3:12]:
        preservation = one(observations, case + "-preservation")
        require(preservation["preserved"] and preservation["before"] == preservation["after"],
                "negative-preservation:" + case)
    require(cases["reconcile-absent-active-run"]["client"]["reply_status"] == 4
            and cases["reconcile-absent-active-run"]["client"]["status"] == "not-observed",
            "absent-receipt")
    require(cases["old-run-after-restart"]["decision"]["code"] == "request-run", "old-run")
    return cases


def inspect(root):
    result, report = load(root)
    cases = audit_requests(root, report)
    records = audit_records(root, report, cases)
    return {"status": report["status"], "source_run": root.name,
            "host_result_sha256": sha((root / "result.json").read_bytes()),
            "guest_report_sha256": result["probe_sha256"],
            "inventory_sha256": result["inventory_sha256"],
            "host_runner_sha256": result["sources"]["sn021-vm-managed-request-run.py"],
            "coordinator_sha256": result["sources"]["windows_managed_request.ps1"],
            "probe_source_sha256": result["sources"]["native_managed_request_probe.cpp"],
            "probe_binary_sha256": result["sources"]["native_managed_request_probe.exe"],
            "fixture_project_sha256": result["sources"]["two-rc-project.json"],
            "auditor_sha256": sha(Path(__file__).read_bytes()),
            "source_snapshots": len(result["sources"]), "artifacts": len(report["artifacts"]),
            "requests": len(CASES), "observations": len(report["observations"]),
            "processes": len(report["processes"]), "elapsed_ms": report["elapsed_ms"],
            "records": records,
            "limits": "Manual one-document authenticated request candidate only; no production endpoint, context rebind, external overwrite, power-loss durability or real RC lifecycle composition."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    summary = inspect(args.run)
    rendered = json.dumps(summary, indent=2) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    print("Authenticated request evidence audit passed: 19 requests, 82 artifacts, 4 records.")


if __name__ == "__main__":
    main()
