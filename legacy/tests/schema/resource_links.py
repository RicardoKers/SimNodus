"""Pure typed links between descriptor catalogs and locked resource declarations."""
import argparse
import json
from pathlib import Path
import re

import bindings as b
import resource_lock as r
import topology as t

MODEL_TOKEN = re.compile(r"[A-Za-z_][A-Za-z0-9_]{0,63}\Z")


def validate(document):
    t.nesting(document)
    t.fields(document, "format version topology lock assets symbols models", "$")
    t.require(document["format"] == "simnodus-resource-links" and document["version"] == "0.1",
              "version", "$", "Expected resource links 0.1")
    topology = document["topology"]
    stats = b.validate(topology)
    inventory = r.validate(document["lock"])
    files = {(dependency["id"], file["id"])
             for dependency in document["lock"]["dependencies"] for file in dependency["files"]}
    assets, targets = {}, set()
    values = t.array(document["assets"], "$/assets")
    t.require(len(values) <= r.MAX_FILES, "budget", "$/assets", "Asset budget exceeded")
    for index, value in enumerate(values):
        loc = f"$/assets/{index}"
        t.fields(value, "id kind dependency resource", loc)
        for field in ("id", "dependency", "resource"):
            t.identifier(value[field], loc + "/" + field)
        t.require(type(value["kind"]) is str and value["kind"] in ("symbol-svg", "model-spice"),
                  "kind", loc, "Unsupported declared asset kind")
        t.require(value["id"] not in assets, "id", loc, "Duplicate asset ID")
        target = (value["dependency"], value["resource"])
        t.require(target in files, "reference", loc, "Unknown locked dependency/resource pair")
        t.require(target not in targets, "reference", loc, "One locked resource has one declared role")
        targets.add(target)
        assets[value["id"]] = value
    linked, model_entries = 0, 0
    for collection, kind in (("symbols", "symbol-svg"), ("models", "model-spice")):
        loc = "$/" + collection
        mapping = document[collection]
        expected = {descriptor["id"] for descriptor in topology[collection]}
        t.require(type(mapping) is dict and set(mapping) == expected, "mapping", loc,
                  "Every descriptor requires an explicit resource binding or null")
        for descriptor, target in mapping.items():
            if target is None:
                continue
            if collection == "models":
                where = loc + "/" + descriptor
                t.fields(target, "asset entrypoint terminal_map parameter_map", where)
                entrypoint = target["entrypoint"]
                t.require(type(entrypoint) is str and MODEL_TOKEN.fullmatch(entrypoint) is not None,
                          "mapping", where, "Invalid declared model entrypoint")
                definition = next(value for value in topology["models"] if value["id"] == descriptor)
                for key, source in (("terminal_map", "terminals"), ("parameter_map", "parameters")):
                    bindings = target[key]
                    t.require(type(bindings) is dict and set(bindings) == {v["id"] for v in definition[source]},
                              "mapping", where, "Model source mapping must cover the interface")
                    for token in bindings.values():
                        t.require(type(token) is str and MODEL_TOKEN.fullmatch(token) is not None,
                                  "mapping", where, "Invalid declared source token")
                    t.require(len({v.lower() for v in bindings.values()}) == len(bindings),
                              "mapping", where, "Aliased model source tokens")
                    model_entries += len(bindings)
                model_entries += 1
                target = target["asset"]
            t.identifier(target, loc + "/" + descriptor)
            t.require(target in assets, "reference", loc, "Unknown asset ID")
            t.require(assets[target]["kind"] == kind, "kind", loc, "Descriptor and asset kinds differ")
            linked += 1
    t.require(stats["declared_entities"] + stats["parameter_entries"] + stats["descriptor_entries"]
              + len(assets) + len(document["symbols"]) + len(document["models"]) + model_entries <= t.MAX_ENTITIES,
              "budget", "$", "Combined descriptor/resource binding budget exceeded")
    return dict(descriptors_linked=linked, assets=len(assets), **inventory,
                resource_interfaces_verified=False, topology_stats=stats)


def parse(raw):
    t.require(len(raw) <= t.MAX_BYTES, "input", "$", "Input byte budget exceeded")

    def pairs(items):
        result = {}
        for key, value in items:
            t.require(key not in result, "input", "$", "Duplicate JSON key")
            result[key] = value
        return result

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
        print(json.dumps(dict(status="valid-resource-links-only", **validate(document))))
        return 0
    except (t.Invalid, OSError) as error:
        diagnostic = error.diagnostic() if isinstance(error, t.Invalid) else dict(code="input", location="$", message="Cannot read input")
        print(json.dumps(dict(status="invalid", **diagnostic)))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
