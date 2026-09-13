"""Project declarations only: no firmware loading or runtime negotiation."""
import argparse
import json
from pathlib import Path

import bindings as b
import parameters as p
import resource_links as links
import topology as t


def validate(document):
    t.nesting(document)
    t.fields(document, "format version id name sources platforms firmware targets temporal", "$")
    t.require(document["format"] == "simnodus-project" and document["version"] == "0.1",
              "version", "$", "Expected project declarations 0.1")
    t.named(document, "$")
    stats = links.validate(document["sources"])
    topology = document["sources"]["topology"]
    files = {(dep["id"], file["id"]) for dep in document["sources"]["lock"]["dependencies"] for file in dep["files"]}

    def reference(value, loc):
        t.fields(value, "dependency resource", loc)
        for key in ("dependency", "resource"):
            t.identifier(value[key], loc)
        t.require((value["dependency"], value["resource"]) in files,
                  "reference", loc, "Unknown locked dependency/resource pair")

    count = 0

    def records(values, fields, loc):
        nonlocal count
        result = {}
        for index, value in enumerate(t.array(values, loc)):
            where = f"{loc}/{index}"
            t.fields(value, fields, where)
            t.named(value, where)
            t.require(value["id"] not in result, "id", where, "Duplicate declaration ID")
            result[value["id"]] = value
            count += 1
            t.require(count <= 256, "budget", where, "Project declaration budget exceeded")
        return result

    platforms = records(document["platforms"], "id name mcu board pins", "$/platforms")
    for pid, platform in platforms.items():
        loc = "$/platforms/" + pid
        mcu = platform["mcu"]
        t.fields(mcu, "device architecture resource", loc + "/mcu")
        t.identifier(mcu["device"], loc)
        t.identifier(mcu["architecture"], loc)
        reference(mcu["resource"], loc + "/mcu/resource")
        if platform["board"] is not None:
            board = platform["board"]
            t.fields(board, "id name resource", loc + "/board")
            t.named(board, loc + "/board")
            reference(board["resource"], loc + "/board/resource")
            count += 1
        records(platform["pins"], "id name", loc + "/pins")
    firmware = records(document["firmware"], "id name architecture image_format resource", "$/firmware")
    for fid, image in firmware.items():
        loc = "$/firmware/" + fid
        t.identifier(image["architecture"], loc)
        t.require(image["image_format"] == "elf", "kind", loc, "Only declared ELF images are supported")
        reference(image["resource"], loc + "/resource")
    paths = {tuple(row["path"]): row["definition"] for row in p.resolve(b.projection(topology))["instances"]}
    components = {value["id"]: value for value in topology["components"]}
    seen = set()
    targets = t.array(document["targets"], "$/targets")
    t.require(len(targets) <= 256, "budget", "$/targets", "Target count exceeded")
    for index, target in enumerate(targets):
        loc = f"$/targets/{index}"
        t.fields(target, "path platform firmware pin_map", loc)
        path = t.array(target["path"], loc + "/path", 2)
        t.require(len(path) <= t.MAX_DEPTH + 1, "budget", loc, "Target path depth exceeded")
        for part in path:
            t.identifier(part, loc)
        path = tuple(path)
        t.require(path in paths and paths[path] in components, "reference", loc, "Target must identify a component occurrence")
        t.require(path not in seen, "reference", loc, "Duplicate execution target")
        seen.add(path)
        for key, catalog in (("platform", platforms), ("firmware", firmware)):
            t.identifier(target[key], loc)
            t.require(target[key] in catalog, "reference", loc, "Missing target declaration")
        platform, image = platforms[target["platform"]], firmware[target["firmware"]]
        t.require(platform["mcu"]["architecture"] == image["architecture"], "mapping", loc, "Declared image/platform architectures differ")
        component = components[paths[path]]
        t.require(component["model"] is None, "mapping", loc, "MCU target cannot also select an electrical model")
        mapping = target["pin_map"]
        t.require(type(mapping) is dict and set(mapping) == {pin["id"] for pin in component["pins"]},
                  "mapping", loc, "Pin map must cover the component interface")
        for value in mapping.values():
            t.identifier(value, loc)
        t.require(len(set(mapping.values())) == len(mapping) and set(mapping.values()) == {pin["id"] for pin in platform["pins"]},
                  "mapping", loc, "Pin map must cover the platform interface without aliases")
        count += 1 + len(mapping)
    t.require(count <= 256, "budget", "$", "Project declaration budget exceeded")
    policy = document["temporal"]
    t.fields(policy, "mode fidelity duration_ns exchange_quantum_ns schedule debug pause_wall_timeout_ms voltage_tolerance_uv analog_time_tolerance_ps on_unsupported", "$/temporal")
    mode = policy["mode"]
    t.require(type(mode) is str and mode in ("unconfigured", "known-schedule-replay", "approximate-sampled"),
              "capability", "$/temporal", "Unsupported requested temporal mode")
    t.require(policy["on_unsupported"] == "reject", "capability", "$/temporal", "Unsupported runtime capabilities must fail closed")
    for key, expected in (("pause_wall_timeout_ms", 2000), ("voltage_tolerance_uv", 10), ("analog_time_tolerance_ps", 1)):
        t.require(type(policy[key]) is int and policy[key] == expected, "range", "$/temporal/" + key, "Reference tolerance/deadline must remain unchanged")
    if mode == "unconfigured":
        t.require(all(policy[key] is None for key in ("fidelity", "duration_ns", "exchange_quantum_ns", "schedule"))
                  and policy["debug"] == "disabled", "shape", "$/temporal", "Unconfigured timing cannot imply execution")
    else:
        expected = "causal-replay" if mode == "known-schedule-replay" else "approximate"
        t.require(policy["fidelity"] == expected, "capability", "$/temporal", "Fidelity must match the requested mode")
        for key in ("duration_ns", "exchange_quantum_ns"):
            value = policy[key]
            t.require(type(value) is int and 0 < value <= 2**64 - 1 and value % 1000 == 0,
                      "range", "$/temporal/" + key, "Expected positive microsecond-aligned virtual nanoseconds")
        t.require(policy["exchange_quantum_ns"] <= policy["duration_ns"], "range", "$/temporal", "Quantum exceeds virtual duration")
        if mode == "known-schedule-replay":
            reference(policy["schedule"], "$/temporal/schedule")
        else:
            t.require(policy["schedule"] is None, "shape", "$/temporal", "Sampled mode cannot consume a replay schedule")
        t.require(policy["debug"] in ("disabled", "bounded-cooperative"), "capability", "$/temporal", "Unsupported requested debugging mode")
    return dict(project_id=document["id"], targets=len(targets), project_entries=count,
                runtime_profile_verified=False, firmware_verified=False, **stats)


def parse(raw):
    t.require(len(raw) <= t.MAX_BYTES, "input", "$", "Input byte budget exceeded")

    def pairs(items):
        value = {}
        for key, item in items:
            t.require(key not in value, "input", "$", "Duplicate JSON key")
            value[key] = item
        return value

    def constant(_):
        raise t.Invalid("input", "$", "Non-finite JSON literal")

    try:
        document = json.loads(raw.decode("utf-8"), object_pairs_hook=pairs, parse_constant=constant)
        validate(document)
        return document
    except (UnicodeError, ValueError, RecursionError) as error:
        if isinstance(error, t.Invalid):
            raise
        raise t.Invalid("input", "$", "Invalid encoding, JSON or nesting") from error


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("document", type=Path)
    args = parser.parse_args()
    try:
        with args.document.open("rb") as source:
            document = parse(source.read(t.MAX_BYTES + 1))
        print(json.dumps(dict(status="valid-project-declarations-only", **validate(document))))
        return 0
    except (t.Invalid, OSError) as error:
        diagnostic = error.diagnostic() if isinstance(error, t.Invalid) else dict(code="input", location="$", message="Cannot read input")
        print(json.dumps(dict(status="invalid", **diagnostic)))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
