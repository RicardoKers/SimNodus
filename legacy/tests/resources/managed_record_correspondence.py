"""Independent, inert byte audit of selected records from one measured VM store.

This reader neither opens guest paths nor interprets a project as executable input.
It checks correspondence with an immutable report, not origin or authorization.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct


REVISIONS = (1, 2, 3, 63, 64)
RUN = "20260925T163734Z-0bddcba4bab4"
LEAF = "SN021Managed-549cccf109a8"
LIMIT = 2 * 1024 * 1024


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, label):
    if not condition:
        raise ValueError(label)


def bounded(path, limit):
    require(path.is_file() and path.stat().st_size <= limit, "bounded-input:" + path.name)
    return path.read_bytes()


def manifest(data):
    require(120 <= len(data) <= 4096 and data[:8] == b"SNST0001", "manifest-format")
    writer_size, client_size = struct.unpack_from("<II", data, 112)
    require(12 <= writer_size <= 68 and 12 <= client_size <= 68
            and len(data) == 120 + writer_size + client_size, "manifest-lengths")
    return {
        "generation": data[8:24], "document": data[24:40],
        "volume": data[40:48], "client": data[120 + writer_size:]
    }


def record(data, revision, setup, project, locator):
    require(268 <= len(data) <= LIMIT and data[:8] == b"SNMC0001", "record-format")
    total, reserved, number, sid_size, context_size, project_size = struct.unpack_from("<IIIIII", data, 8)
    require(total == len(data) == 268 + sid_size + context_size + project_size
            and reserved == 0 and number == revision and 12 <= sid_size <= 68
            and 52 <= context_size <= 16 * 1024 and 1 <= project_size <= 1024 * 1024,
            "record-lengths")
    require(data[32:48] == setup["generation"] and data[48:64] == setup["document"], "record-scope")
    require(data[80:96] == bytes([revision]) * 16, "operation-id")
    start = 236
    require(data[start:start + sid_size] == setup["client"], "principal-bytes")
    start += sid_size
    context = data[start:start + context_size]
    require(context[:4] == b"SRC1" and context[4:20] == bytes([42]) * 16
            and context[20:28] == setup["volume"]
            and any(context[28:44]) and struct.unpack_from("<I", context, 44)[0] == 1,
            "context-binding")
    locator_size = struct.unpack_from("<I", context, 48)[0]
    require(context_size == 52 + locator_size and context[52:] == locator, "context-locator")
    start += context_size
    expected_project = project.ljust(71680, b" ") + (b" " if revision % 2 == 0 else b"")
    require(data[start:-32] == expected_project and project_size == len(expected_project),
            "exact-project-bytes")
    require(data[204:236] == hashlib.sha256(
        b"SNREQ001" + data[32:64] + data[80:96] + data[96:204]
        + data[20:32] + data[236:-32]).digest(), "request-digest")
    require(data[-32:] == hashlib.sha256(data[:-32]).digest(), "record-digest")
    return {
        "revision": revision, "generation": data[32:48], "document": data[48:64],
        "commit": data[64:80], "operation": data[80:96], "predecessor": data[96:204],
        "record_digest": data[-32:], "project_sha256": digest(data[start:-32]),
        "context_sha256": digest(context), "byte_sha256": digest(data)
    }


def inspect(directory, published):
    report = json.loads(published.read_text(encoding="utf-8"))
    require(report["status"] == "observed-managed-store-candidate-only"
            and report["tag"] == RUN.rsplit("-", 1)[1]
            and report["roots"][0] == {"leaf": LEAF, "case": "sequence"}, "published-run")
    cases = {item.get("case"): item for item in report["observations"]}
    snapshot = cases["sequence-independent-bytes"]["snapshot"]
    require(sum(name.endswith(".commit") for name in snapshot) == 64, "reported-chain-size")
    collection = json.loads((directory / "collection.json").read_text(encoding="utf-8"))
    require(collection["source_run"] == RUN and collection["root_leaf"] == LEAF,
            "collection-scope")
    expected_files = ["generation.manifest", "project.json"] + [f"{n:08x}.commit" for n in REVISIONS]
    require(set(collection["files"]) == set(expected_files), "collection-set")
    inputs = {}
    for name in expected_files:
        data = bounded(directory / name, 4096 if name.endswith(".manifest") else LIMIT)
        require(digest(data) == collection["files"][name]["sha256"], "collection-hash:" + name)
        inputs[name] = data
    require(digest(inputs["project.json"]) == report["project_sha256"], "fixture-project-hash")
    manifest_key = f"C:\\{LEAF}\\generation.manifest"
    require(digest(inputs["generation.manifest"]) == snapshot[manifest_key]["sha256"], "manifest-hash")
    setup = manifest(inputs["generation.manifest"])
    locator = f"C:\\SN021ManagedHarness-{RUN.rsplit('-', 1)[1]}".encode("utf-8")
    records = {}
    for revision in REVISIONS:
        name = f"{revision:08x}.commit"
        data = inputs[name]
        key = f"C:\\{LEAF}\\document\\{name}"
        require(digest(data) == snapshot[key]["sha256"]
                and digest(data[:-32]) == snapshot[key]["record_digest"],
                "physical-snapshot:" + name)
        records[revision] = record(data, revision, setup, inputs["project.json"], locator)
    for revision in REVISIONS:
        predecessor = records[revision]["predecessor"]
        if revision == 1:
            require(predecessor == bytes(108), "null-parent")
        else:
            require(predecessor[:32] == setup["generation"] + setup["document"]
                    and struct.unpack_from("<I", predecessor, 32)[0] == revision - 1
                    and predecessor[84:92] == setup["volume"]
                    and any(predecessor[92:108]), "predecessor-scope")
            if revision - 1 in records:
                previous = records[revision - 1]
                require(predecessor[36:52] == previous["commit"]
                        and predecessor[52:84] == previous["record_digest"], "predecessor-link")
    require(inputs["00000001.commit"] != inputs["00000003.commit"]
            and records[1]["project_sha256"] == records[3]["project_sha256"], "aba")
    return {
        "status": "observed-selected-record-correspondence-only", "source_run": RUN,
        "root_leaf": LEAF, "selected_revisions": list(REVISIONS),
        "manifest_sha256": digest(inputs["generation.manifest"]),
        "project_sha256": digest(inputs["project.json"]),
        "records": [{"revision": n, "byte_sha256": records[n]["byte_sha256"],
                     "project_sha256": records[n]["project_sha256"],
                     "context_sha256": records[n]["context_sha256"]} for n in REVISIONS],
        "limits": "Selected stored bytes only; no authenticated Save, physical resource-root recapture, complete chain reinspection, runtime compatibility or execution authorization."
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("published_report", type=Path)
    args = parser.parse_args()
    print(json.dumps(inspect(args.directory, args.published_report), indent=2))


if __name__ == "__main__":
    main()
