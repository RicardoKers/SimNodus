"""Independently audit retained SN-021 managed RC lifecycle bytes."""

import argparse
import hashlib
import json
from pathlib import Path
import struct

from audit_managed_request import request_fields, reply_fields, sid, token_record


def require(value, code):
    if not value:
        raise ValueError(code)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def one(items, case):
    found = [row for row in items if row.get("case") == case]
    require(len(found) == 1, "case-count:" + case)
    return found[0]


def artifact(root, name):
    return (root / "artifacts" / name).read_bytes()


def load(root):
    result_bytes = (root / "result.json").read_bytes()
    result = json.loads(result_bytes)
    inventory_bytes = (root / "inventory.json").read_bytes()
    probe_bytes = (root / "probe.json").read_bytes()
    require(digest(inventory_bytes) == result["inventory_sha256"], "inventory-digest")
    require(digest(probe_bytes) == result["probe_sha256"], "probe-digest")
    inventory = json.loads(inventory_bytes.decode("utf-8-sig"))
    report = json.loads(probe_bytes.decode("utf-8-sig"))
    require(result["status"] == report["status"] == "observed-managed-rc-lifecycle-candidate-only"
            and result["stage"] == report["stage"] == "complete", "status")
    require(result["snapshot"]["name"] == "SN021-authority-baseline", "snapshot")
    require(inventory["status"] == "observed" and report["filesystem"] == "NTFS"
            and report["token"]["administrator"] and not report["reboot_pending"], "environment")
    for name, expected in result["sources"].items():
        require(Path(name).name == name and digest((root / name).read_bytes()) == expected,
                "source:" + name)
    staged = {
        "store_binary_sha256": result["sources"]["native_managed_store_probe.exe"],
        "binary_sha256": result["sources"]["native_managed_request_probe.exe"],
        "lifecycle_binary_sha256": result["sources"]["native_managed_lifecycle_probe.exe"],
        "project_sha256": result["sources"]["source.json"],
        "license_sha256": result["sources"]["LICENSE"],
        "svg_sha256": result["sources"]["passive.svg"],
        "cir_sha256": result["sources"]["passive.cir"],
        "drive_sha256": result["sources"]["fixed-drive.csv"],
    }
    require(inventory["staged"] == report["staged"] == staged, "staged")
    require(all(report[name] == staged[name] for name in
                ("store_binary_sha256", "binary_sha256", "lifecycle_binary_sha256", "project_sha256")),
            "private-inputs")
    listed = report["artifacts"]
    require(len(listed) == len(result["artifacts"]) and len(listed) <= 128, "artifact-count")
    for entry in listed:
        name = entry["name"]
        require(Path(name).name == name and name in result["artifacts"], "artifact-name")
        data = artifact(root, name)
        require(digest(data) == entry["sha256"] == result["artifacts"][name]["sha256"]
                and len(data) == result["artifacts"][name]["bytes"], "artifact:" + name)
    require(len({row["name"] for row in listed}) == len(listed), "artifact-duplicates")
    return result, report, digest(result_bytes)


def record(root, report, revision, manifest):
    name = f"{revision:08x}.commit"
    data = artifact(root, name)
    total, reserved, number, sid_size, context_size, project_size = struct.unpack_from("<IIIIII", data, 8)
    require(data[:8] == b"SNMC0001" and 268 <= len(data) <= 2 * 1024 * 1024
            and total == len(data) == 268 + sid_size + context_size + project_size
            and reserved == 0 and number == revision, "record-frame:" + name)
    require(data[32:64] == manifest[8:40]
            and data[236:236 + sid_size] == manifest[120 + struct.unpack_from("<I", manifest, 112)[0]:],
            "record-scope-principal:" + name)
    context = data[236 + sid_size:236 + sid_size + context_size]
    project = data[236 + sid_size + context_size:-32]
    require(len(context) == context_size and len(project) == project_size
            and data[-32:] == hashlib.sha256(data[:-32]).digest(), "record-body:" + name)
    request = hashlib.sha256(b"SNREQ001" + data[32:64] + data[80:96]
                             + data[96:204] + data[20:32] + data[236:-32]).digest()
    require(data[204:236] == request, "request-digest:" + name)
    snapshot = report["records_final"][name]
    require(digest(data) == snapshot["sha256"] and snapshot["length"] == len(data),
            "record-snapshot:" + name)
    return {"name": name, "data": data, "context": context, "project": project,
            "snapshot": snapshot}


def open_reply(raw, request, record, token):
    reply_fields(request, raw, 1)
    context_length = struct.unpack_from("<I", raw, 144)[0]
    require(55 <= context_length <= 16 * 1024 and 148 + context_length + 4 <= len(raw),
            "open-context-length")
    project_length = struct.unpack_from("<I", raw, 148 + context_length)[0]
    require(len(raw) == 152 + context_length + project_length
            and raw[36:144] == token and raw[148:148 + context_length] == record["context"]
            and raw[152 + context_length:] == record["project"], "open-record-correspondence")


def audit(root, report):
    observations = report["observations"]
    principals = report["principals"]
    require(set(principals) == {"w", "c", "u"} and len(set(principals.values())) == 3,
            "principals")
    manifest = artifact(root, "generation.manifest")
    require(manifest[:8] == b"SNST0001" and 120 <= len(manifest) <= 4096, "manifest")
    writer_size, client_size = struct.unpack_from("<II", manifest, 112)
    require(len(manifest) == 120 + writer_size + client_size
            and sid(manifest[120:120 + writer_size]) == principals["w"]
            and sid(manifest[120 + writer_size:]) == principals["c"]
            and digest(manifest) == report["manifest_before_sha256"], "manifest-principals-preserved")
    require(set(report["records_final"]) == {"00000001.commit", "00000002.commit"}, "record-set")
    first, second = (record(root, report, number, manifest) for number in (1, 2))
    source, edited = (artifact(root, name) for name in ("original-source.json", "edited.json"))
    require(source == (root / "source.json").read_bytes() == first["project"]
            and artifact(root, "import-record.bin") == first["data"]
            and edited == second["project"] == artifact(root, "managed-export.json"),
            "exact-project-correspondence")
    original_object, revised_object = json.loads(source), json.loads(edited)
    require(revised_object.pop("name") == "Managed RC copy"
            and revised_object == {key: value for key, value in original_object.items() if key != "name"},
            "name-only-edit")
    policy = original_object["temporal"]
    require(policy["mode"] == "known-schedule-replay" and policy["fidelity"] == "causal-replay"
            and policy["debug"] == "disabled" and policy["on_unsupported"] == "reject"
            and policy["duration_ns"] == policy["exchange_quantum_ns"] == 5_000_000
            and policy["voltage_tolerance_uv"] == 10 and policy["analog_time_tolerance_ps"] == 1
            and policy["schedule"] == {"dependency": "owned-link-fixture", "resource": "fixed-drive"},
            "fixed-policy")
    require(first["data"][96:204] == bytes(108), "initial-predecessor")
    import_result = one(observations, "import-exact")["lines"][-1]
    require(import_result["status"] == "imported-exact" and import_result["revision"] == 1
            and import_result["bytes"] == len(source) and import_result["project_equal"]
            and import_result["context_equal"], "import-result")
    context = first["context"]
    locator = ("C:\\SN021LifecycleHarness-" + report["tag"] + "\\external").encode("utf-8")
    require(report["external_root_identity"] ==
            [import_result["root_volume"], import_result["root_file"]], "observed-root-id")
    substitution = report["root_substitution"]
    require(substitution["former"] == report["external_root_identity"]
            and substitution["replacement"] != substitution["former"]
            and substitution["source_sha256"] == digest(source), "root-substitution")
    require(context == second["context"] and context[:4] == b"SRC1"
            and any(context[4:20]) and f"{struct.unpack_from('<Q', context, 20)[0]:016x}" == import_result["root_volume"]
            and context[28:44].hex() == import_result["root_file"]
            and struct.unpack_from("<II", context, 44) == (1, len(locator))
            and context[52:] == locator, "root-context")
    require(digest(artifact(root, "hash-divergent-drive.csv")) != digest((root / "fixed-drive.csv").read_bytes())
            and artifact(root, "held-missing-passive.cir") == (root / "passive.cir").read_bytes(),
            "negative-resource-inputs")

    server = one(observations, "lifecycle")["lines"]
    tokens = []
    for connection, case, revision in ((1, "open-initial", 1), (2, "save-edited", 2),
                                       (3, "open-revised", 2)):
        client = one(observations, case)["lines"][-1]
        observed = one(observations, case + "-decision")
        decisions = [row for row in server if row.get("event") == "decision"
                     and row.get("connection") == connection]
        require(len(decisions) == 1 and decisions[0] == observed["decision"]
                and decisions[0]["code"] == "ok" and decisions[0]["authenticated_sid"] == principals["c"]
                and decisions[0]["reverted"] and decisions[0]["dispatched"]
                and client["status"] == "accepted" and client["server_verified"]
                and client["reply_status"] == 0 and client["token"] == decisions[0]["token"],
                "authenticated-decision:" + case)
        selected = first if revision == 1 else second
        token_record(client["token"], selected["data"], revision, selected["snapshot"])
        raw_request = artifact(root, case + ".request.bin")
        raw_response = artifact(root, case + ".response.bin")
        require(digest(raw_request) == observed["request_sha256"], "request-hash:" + case)
        fields = request_fields(raw_request, 1 if connection != 2 else 2)
        require(fields["generation"] + fields["document"] == selected["data"][32:64],
                "request-scope:" + case)
        if connection == 2:
            reply_fields(raw_request, raw_response, 2)
            require(fields["expected"] == tokens[0]
                    and fields["operation_id"] == second["data"][80:96]
                    and fields["project"] == edited and second["data"][96:204] == tokens[0]
                    and len(raw_response) == 192 and raw_response[36:52] == second["data"][80:96]
                    and raw_response[52:160] == bytes.fromhex(client["token"])
                    and raw_response[160:192] == second["data"][204:236],
                    "save-record-receipt")
        else:
            open_reply(raw_response, raw_request, selected, bytes.fromhex(client["token"]))
        tokens.append(bytes.fromhex(client["token"]))
    require(tokens[1] == tokens[2] and tokens[0] != tokens[1], "revision-advance")

    for case, code in (("changed-schedule-compile", "compile-hash"),
                       ("missing-passive-compile", "compile-filesystem"),
                       ("replaced-root-compile", "compile-root_identity")):
        require(artifact(root, case + ".c.stderr").decode().strip() == code
                and json.loads(artifact(root, case + ".c.jsonl").splitlines()[-1])["status"] == "fixture-failed"
                and one(observations, case + "-refusal")["output_absent"],
                "physical-refusal:" + case)
    require(one(observations, "export-collision-refusal")["destination_sha256"] == digest(edited)
            and json.loads(artifact(root, "export-collision.c.jsonl").splitlines()[-1])["status"] == "fixture-failed"
            and b"export-" in artifact(root, "export-collision.c.stderr"), "export-collision")
    compile_result = one(observations, "compile-bound")["lines"][-1]
    netlist = artifact(root, "managed-rc.cir")
    dependency = next(row for row in original_object["sources"]["lock"]["dependencies"]
                      if row["id"] == policy["schedule"]["dependency"])
    schedule_index = compile_result["schedule_resource"]
    require(compile_result["status"] == "compiled-bound" and not compile_result["readiness"]
            and compile_result["duration_ns"] == 5_000_000
            and compile_result["exchange_quantum_ns"] == 5_000_000
            and type(schedule_index) is int and 0 <= schedule_index < len(dependency["files"])
            and dependency["files"][schedule_index]["id"] == "fixed-drive"
            and compile_result["netlist_bytes"] == len(netlist)
            and digest(netlist) == report["netlist_sha256"]
            and digest(netlist) == "95471148afe803f2e190fbc2f282fd321708b1b41d986587900dbb45f97f4362",
            "bounded-compile")
    return {"source_sha256": digest(source), "edited_sha256": digest(edited),
            "context_sha256": digest(context), "manifest_sha256": digest(manifest),
            "record_sha256": [digest(first["data"]), digest(second["data"])],
            "netlist_sha256": digest(netlist), "requests": 3,
            "refusals": 4, "artifacts": len(report["artifacts"])}


def inspect(root):
    result, report, result_digest = load(root)
    checked = audit(root, report)
    return {"status": "audited-managed-rc-lifecycle-candidate-only", "source_run": root.name,
            "host_result_sha256": result_digest, "guest_report_sha256": result["probe_sha256"],
            "inventory_sha256": result["inventory_sha256"],
            "host_runner_sha256": result["sources"]["sn021-vm-managed-lifecycle-run.py"],
            "coordinator_sha256": result["sources"]["windows_managed_lifecycle.ps1"],
            "auditor_sha256": digest(Path(__file__).read_bytes()),
            "elapsed_ms": report["elapsed_ms"], **checked,
            "limits": "Disposable-VM test-only composition; no production endpoint, service, external overwrite, power-loss durability, or general runtime readiness."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    summary = inspect(args.run)
    if args.output:
        args.output.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print("Managed RC lifecycle byte audit passed:", summary["requests"], "requests,",
          summary["artifacts"], "artifacts.")


if __name__ == "__main__":
    main()
