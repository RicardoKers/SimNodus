"""Independently audit inert artifacts from the disposable-VM v3 boundary batch."""

import argparse
import hashlib
import json
from pathlib import Path
import struct

from audit_managed_request import request_fields, sid, token_record


def require(value, label):
    if not value:
        raise ValueError(label)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def one(items, case):
    found = [item for item in items if item.get("case") == case]
    require(len(found) == 1, "case-count:" + case)
    return found[0]


def lines(run, report, case):
    observation = one(report["observations"], case)
    role = observation["role"]
    raw = (run / "artifacts" / f"{case}.{role}.jsonl").read_bytes()
    actual = [json.loads(line) for line in raw.decode("utf-8").splitlines()]
    expected = [dict(line) for line in observation["lines"]]
    for item in expected:
        item.pop("decision", None)  # The coordinator attaches server decisions after child exit.
    require(actual == expected and actual, "child-lines:" + case)
    return actual


def frame(run, name, operation):
    raw = (run / "artifacts" / f"{name}.request.bin").read_bytes()
    return raw, request_fields(raw, operation)


def reply(run, name, request, operation, status):
    raw = (run / "artifacts" / f"{name}.response.bin").read_bytes()
    require(len(raw) >= 36 and raw[:8] == b"SN21SAVE"
            and struct.unpack_from("<HHI", raw, 8) == (3, operation | 0x8000, len(raw) - 32)
            and raw[16:32] == request[16:32]
            and struct.unpack_from("<I", raw, 32)[0] == status, "reply-frame:" + name)
    return raw


def pipe_descriptor(run, report, name, kind, owner, aces):
    ready = lines(run, report, name)[0]
    descriptor = ready["descriptor"]
    observed = [(ace["sid"], ace["mask"], ace["flags"])
                for ace in descriptor["aces"]]
    require(ready["event"] == "listener-ready" and ready["kind"] == kind
            and descriptor["owner"] == owner and descriptor["dacl_protected"]
            and len(observed) == len(aces) and set(observed) == set(aces),
            "pipe-descriptor:" + name)


def inspect(run):
    result_data = (run / "result.json").read_bytes()
    result = json.loads(result_data)
    inventory_data = (run / "inventory.json").read_bytes()
    report_data = (run / "boundary.json").read_bytes()
    require(sha(inventory_data) == result["inventory_sha256"]
            and sha(report_data) == result["boundary_sha256"], "host-report-hashes")
    inventory = json.loads(inventory_data.decode("utf-8-sig"))
    report = json.loads(report_data.decode("utf-8-sig"))
    require(result["status"] == report["status"] == "observed-v3-boundary-candidate-only"
            and result["stage"] == report["stage"] == "complete", "completion")
    require(result["snapshot"]["name"] == "SN021-authority-baseline"
            and report["filesystem"] == "NTFS" and report["token"]["administrator"]
            and not report["reboot_pending"], "guest-environment")
    for name, digest in result["sources"].items():
        require(sha((run / name).read_bytes()) == digest, "source:" + name)
    require(result["sources"]["audit_managed_v3_boundary.py"] == sha(Path(__file__).read_bytes()),
            "auditor-source")
    require(result["sources"]["audit_managed_request.py"] == sha((Path(__file__).parent / "audit_managed_request.py").read_bytes()),
            "auditor-dependency-source")
    expected_staged = {"binary_sha256": result["sources"]["native_managed_request_probe.exe"],
                       "store_binary_sha256": result["sources"]["native_managed_store_probe.exe"],
                       "project_sha256": result["sources"]["two-rc-project.json"]}
    require(inventory["staged"] == report["staged"] == expected_staged
            and inventory["status"] == "observed"
            and all(report[key] == value for key, value in expected_staged.items()), "staged-inputs")
    principals = report["principals"]
    require(set(principals) == {"w", "c", "u"} and len(set(principals.values())) == 3,
            "physical-principals")
    artifacts = result["artifacts"]
    # Nineteen JSONL/stderr pairs, nine requests, six responses, two gates,
    # four records and one manifest are retained on the complete path.
    require(len(artifacts) == len(report["artifacts"]) == 60,
            "artifact-budget")
    names = set()
    for item in report["artifacts"]:
        name = item["name"]
        require(name == Path(name).name and name not in names and name in artifacts,
                "artifact-name:" + name)
        names.add(name)
        raw = (run / "artifacts" / name).read_bytes()
        require(sha(raw) == item["sha256"] == artifacts[name]["sha256"]
                and len(raw) == artifacts[name]["bytes"], "artifact-hash:" + name)
    require((run / "artifacts/competing.start").read_bytes() == b"\x01", "gate-byte")
    require((run / "artifacts/occupied.release").read_bytes() == b"\x01", "release-byte")

    roles = {"boundary-provision": "a", "boundary-seed": "w", "boundary-competing": "w",
             "compete-a": "c", "compete-b": "c", "wrong-owner": "u",
             "wrong-owner-client": "c", "wrong-rights": "w", "wrong-rights-client": "c",
             "occupied-name": "w", "occupied-second-writer": "w",
             "terminal-close-writer": "w", "terminal-close-save": "c", "terminal-close-reconcile": "c",
             "terminal-stall-writer": "w", "terminal-stall-save": "c", "terminal-stall-reconcile": "c",
             "silent-client-writer": "w", "silent-client": "c"}
    processes = {entry["case"]: entry for entry in report["processes"]}
    require(len(processes) == len(report["processes"]) == 19 and set(processes) == set(roles),
            "process-count")
    for case, entry in processes.items():
        observation = one(report["observations"], case)
        require(observation["pid"] == entry["pid"] and observation["role"] == entry["role"] == roles[case]
                and observation["exited"] and observation["exit_code"] == (2 if case == "occupied-second-writer" else 0),
                "process:" + case)
        lines(run, report, case)

    source = (run / "two-rc-project.json").read_bytes()
    first_project = source.ljust(71680, b" ")
    second_project = first_project + b" "
    ready = one(report["observations"], "boundary-competing-ready")["state"]
    require(len(ready["token"]) == 216 and len(ready["run"]) == 32, "ready")
    clients = {name: lines(run, report, name) for name in ("compete-a", "compete-b")}
    require(all(any(item.get("event") == "client-ready" and item.get("status") == "waiting-for-gate"
                    and item.get("pid") == processes[name]["pid"] for item in child)
                for name, child in clients.items()), "concurrent-ready")
    gate = one(report["observations"], "competing-gate")
    require(set(gate["client_pids"]) == {processes[name]["pid"] for name in clients}, "gate-pids")
    outcomes = {name: child[-1] for name, child in clients.items()}
    winners = [name for name, value in outcomes.items() if value["status"] == "accepted"]
    losers = [name for name, value in outcomes.items() if value["status"] == "rejected"]
    require(len(winners) == len(losers) == 1 and all(value["server_verified"] for value in outcomes.values()),
            "competing-outcome")
    winner, loser = winners[0], losers[0]
    frames = {name: frame(run, name, 2) for name in clients}
    require(frames["compete-a"][1]["expected"] == frames["compete-b"][1]["expected"] == bytes.fromhex(ready["token"])
            and frames["compete-a"][1]["operation_id"] == bytes.fromhex("61" * 16)
            and frames["compete-b"][1]["operation_id"] == bytes.fromhex("62" * 16)
            and frames["compete-a"][1]["project"] == first_project
            and frames["compete-b"][1]["project"] == second_project, "competing-frames")
    decisions = [item for item in lines(run, report, "boundary-competing") if item.get("event") == "decision"]
    require(len(decisions) == 2 and sum(item["code"] == "ok" and item["token"] == outcomes[winner]["token"]
                                        for item in decisions) == 1
            and sum(item["code"] == "request" and item["store_system"] == 10 for item in decisions) == 1
            and all(item["authenticated_sid"] == principals["c"] and item["reverted"] and item["dispatched"]
                    for item in decisions), "competing-decisions")
    accepted_reply = reply(run, winner, frames[winner][0], 2, 0)
    refused_reply = reply(run, loser, frames[loser][0], 2, 1)
    require(len(accepted_reply) == 192 and len(refused_reply) == 36, "competing-replies")

    manifest = (run / "artifacts/generation.manifest").read_bytes()
    require(manifest[:8] == b"SNST0001" and 120 <= len(manifest) <= 4096, "manifest-format")
    writer_size, client_size = struct.unpack_from("<II", manifest, 112)
    require(len(manifest) == 120 + writer_size + client_size
            and sid(manifest[120:120 + writer_size]) == principals["w"]
            and sid(manifest[120 + writer_size:]) == principals["c"], "manifest-principals")
    require(report["manifest"]["preserved"]
            and report["manifest"]["before"] == report["manifest"]["after"]
            and sha(manifest) == report["manifest"]["after"]["sha256"], "manifest-preservation")
    final = report["final"]
    competing = report["competing"]["final"]
    require(set(final) == {f"{n:08x}.commit" for n in range(1, 5)}
            and set(competing) == {"00000001.commit", "00000002.commit"}, "record-set")
    records = {}
    projects = (first_project, frames[winner][1]["project"],
                second_project if winner == "compete-a" else first_project,
                frames[winner][1]["project"])
    operations = (bytes([1]) * 16, frames[winner][1]["operation_id"],
                  bytes.fromhex("63" * 16), bytes.fromhex("64" * 16))
    token_by_revision = (ready["token"], outcomes[winner]["token"],
                         lines(run, report, "terminal-close-reconcile")[-1]["token"],
                         lines(run, report, "terminal-stall-reconcile")[-1]["token"])
    for number in range(1, 5):
        name = f"{number:08x}.commit"
        data = (run / "artifacts" / name).read_bytes()
        require(data[:8] == b"SNMC0001" and 268 <= len(data) <= 2 * 1024 * 1024,
                "record-format:" + name)
        total, reserved, revision, sid_size, context_size, project_size = struct.unpack_from("<IIIIII", data, 8)
        require(total == len(data) == 268 + sid_size + context_size + project_size
                and reserved == 0 and revision == number, "record-bounds:" + name)
        context = data[236 + sid_size:236 + sid_size + context_size]
        project = data[236 + sid_size + context_size:-32]
        require(data[32:64] == manifest[8:40] and data[80:96] == operations[number - 1]
                and sid(data[236:236 + sid_size]) == principals["c"]
                and project == projects[number - 1]
                and data[-32:] == hashlib.sha256(data[:-32]).digest(), "record-correspondence:" + name)
        require(data[204:236] == hashlib.sha256(b"SNREQ001" + data[32:64] + data[80:96]
                + data[96:204] + data[20:32] + data[236:-32]).digest(), "record-request-digest:" + name)
        if number > 1:
            require(data[96:204] == records[number - 1]["token"]
                    and context == records[1]["context"], "record-chain:" + name)
        else:
            require(data[96:204] == bytes(108), "first-record-predecessor")
        require(sha(data) == final[name]["sha256"], "final-record-hash:" + name)
        token_record(token_by_revision[number - 1], data, number, final[name])
        records[number] = {"token": bytes.fromhex(token_by_revision[number - 1]),
                           "context": context, "sha256": sha(data), "data": data}
    require(all(competing[name] == final[name] for name in competing), "competing-record-preservation")
    require(accepted_reply[36:52] == operations[1] and accepted_reply[52:160] == records[2]["token"]
            and accepted_reply[160:192] == records[2]["data"][204:236], "winner-receipt")

    pipe_descriptor(run, report, "wrong-owner", "owner", principals["u"],
                    [(principals["u"], 0x1f01ff, 0), (principals["c"], 0x120183, 0)])
    pipe_descriptor(run, report, "wrong-rights", "rights", principals["w"],
                    [(principals["w"], 0x1f01ff, 0), (principals["c"], 0x12018b, 0),
                     (principals["u"], 0x120183, 0)])
    pipe_descriptor(run, report, "occupied-name", "occupied", principals["w"],
                    [(principals["w"], 0x1f01ff, 0), (principals["c"], 0x120183, 0),
                     (principals["u"], 0x120183, 0)])
    for name, code in (("wrong-owner", "pipe-owner"), ("wrong-rights", "pipe-rights")):
        client = lines(run, report, name + "-client")[-1]
        listener = lines(run, report, name)[-1]
        require(client["status"] == "failed" and client["stage"] == "verify-server"
                and client["code"] == code and not client["server_verified"]
                and not client["request_sent"] and listener["status"] == "closed-without-request"
                and listener["request_bytes"] == 0, "hostile-endpoint:" + name)
        raw, _ = frame(run, name, 1)
        require(len(raw) == 80, "endpoint-input:" + name)
    occupied = one(report["observations"], "occupied-name-refusal")
    require(not occupied["ready_exists"] and occupied["listener_alive_at_refusal"]
            and occupied["refusal"]["code"] == "pipe-create"
            and occupied["refusal"]["system"] != 0
            and occupied["listener"]["status"] == "held-name"
            and occupied["listener"]["release_observed"], "occupied-name")

    for number, label, code in ((3, "terminal-close", "terminal-closed"),
                                (4, "terminal-stall", "terminal-stalled")):
        save_name, reconcile_name = label + "-save", label + "-reconcile"
        save = lines(run, report, save_name)[-1]
        reconcile = lines(run, report, reconcile_name)[-1]
        writer = lines(run, report, label + "-writer")
        writer_decisions = [item for item in writer if item.get("event") == "decision"]
        require(save["status"] == "failed" and save["stage"] == "terminal"
                and save["server_verified"] and save["request_sent"] and save["indeterminate"]
                and reconcile["status"] == "accepted" and reconcile["token"] == token_by_revision[number - 1]
                and len(writer_decisions) == 2 and writer_decisions[0]["code"] == code
                and writer_decisions[0]["indeterminate"] and writer_decisions[1]["code"] == "ok"
                and all(item["authenticated_sid"] == principals["c"] and item["reverted"] and item["dispatched"]
                        for item in writer_decisions), "terminal-decision:" + label)
        save_raw, save_fields = frame(run, save_name, 2)
        reconcile_raw, reconcile_fields = frame(run, reconcile_name, 3)
        require(save_fields == reconcile_fields
                and save_fields["operation_id"] == operations[number - 1]
                and save_fields["expected"] == records[number - 1]["token"]
                and save_fields["project"] == projects[number - 1], "terminal-request:" + label)
        undelivered = reply(run, save_name, save_raw, 2, 0)
        require(len(undelivered) == 192
                and undelivered[36:52] == operations[number - 1]
                and undelivered[52:160] == records[number]["token"]
                and undelivered[160:192] == records[number]["data"][204:236],
                "terminal-undelivered-reply:" + label)
        accepted = reply(run, reconcile_name, reconcile_raw, 3, 0)
        require(len(accepted) == 192 and accepted[36:52] == operations[number - 1]
                and accepted[52:160] == records[number]["token"]
                and accepted[160:192] == records[number]["data"][204:236], "reconciled-receipt:" + label)
        if label == "terminal-stall":
            cancellation = [item for item in lines(run, report, save_name) if item.get("event") == "cancellation"]
            require(len(cancellation) == 1 and cancellation[0]["cancel_error"] == 0
                    and cancellation[0]["completion"] == 995, "terminal-cancellation")
    idle = lines(run, report, "silent-client")[-1]
    idle_writer = lines(run, report, "silent-client-writer")
    denial = [item for item in idle_writer if item.get("event") == "decision"]
    cancellation = [item for item in idle_writer if item.get("event") == "cancellation"]
    require(idle["status"] == "held-idle" and idle["server_verified"] and not idle["request_sent"]
            and len(denial) == len(cancellation) == 1 and denial[0]["code"] == "deadline"
            and denial[0]["stage"] == "read" and not denial[0]["dispatched"]
            and cancellation[0]["cancel_error"] == 0 and cancellation[0]["completion"] == 995,
            "silent-client-deadline")
    _, idle_fields = frame(run, "silent-client", 1)
    require(idle_fields["run"] == bytes.fromhex(one(report["observations"], "silent-client-writer-ready")["state"]["run"]),
            "idle-input")
    return {"status": "audited-v3-boundary-candidate-only",
            "run": run.name, "host_result_sha256": sha(result_data),
            "guest_report_sha256": sha(report_data), "inventory_sha256": sha(inventory_data),
            "sources": len(result["sources"]), "artifacts": len(artifacts),
            "processes": len(processes), "records": [records[n]["sha256"] for n in range(1, 5)],
            "competing_winner": winner, "net_new_competing_commits": 1,
            "limits": "Test-only disposable-VM v3 boundary; no service, external overwrite, power-loss or general runtime acceptance."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    summary = inspect(args.run)
    rendered = json.dumps(summary, indent=2) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    print("Native v3 boundary byte audit passed: two competing Saves, endpoint refusals, two reconciled terminal failures, and silent-client deadline.")


if __name__ == "__main__":
    main()
