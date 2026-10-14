"""Experimental 0.3 descriptor references; never opens symbol/model resources."""
import argparse
import copy
import json
from pathlib import Path

import parameters as p
import topology as t


def projection(document):
    result = copy.deepcopy(document)
    result.pop("symbols")
    result.pop("models")
    result["version"] = "0.2"
    for collection in ("components", "circuits"):
        for definition in t.array(result[collection], "$" + collection):
            if type(definition) is dict:
                definition.pop("symbol", None)
                if collection == "components":
                    definition.pop("model", None)
    return result


def validate(document):
    t.nesting(document)
    t.fields(document, "format version root components circuits symbols models", "$")
    t.require(document["format"] == "simnodus-topology" and document["version"] == "0.3",
              "version", "$", "Expected experimental topology 0.3")
    stats, parameters = p.validate(projection(document))
    extra = 0

    def records(values, loc, fields):
        nonlocal extra
        result = {}
        for index, value in enumerate(t.array(values, loc)):
            where = f"{loc}/{index}"
            t.fields(value, fields, where)
            t.named(value, where)
            t.require(value["id"] not in result, "id", where, "Duplicate descriptor/local ID")
            result[value["id"]] = value
            extra += 1
        return result

    symbols = records(document["symbols"], "$/symbols", "id name pins")
    models = records(document["models"], "$/models", "id name terminals parameters")
    anchors, terminals, slots = {}, {}, {}
    for sid, symbol in symbols.items():
        anchors[sid] = records(symbol["pins"], f"$/symbols/{sid}/pins", "id name")
    for mid, model in models.items():
        terminals[mid] = records(model["terminals"], f"$/models/{mid}/terminals", "id name domain")
        for terminal in terminals[mid].values():
            t.require(terminal["domain"] == "electrical", "value", f"$/models/{mid}", "Unsupported terminal domain")
        slots[mid] = records(model["parameters"], f"$/models/{mid}/parameters", "id name unit minimum maximum")
        for slot in slots[mid].values():
            loc = f"$/models/{mid}/parameters/{slot['id']}"
            t.require(type(slot["unit"]) is str and slot["unit"] in p.BASE_UNITS, "unit", loc, "Model slot requires a base unit")
            low, high = p.number(slot["minimum"], loc), p.number(slot["maximum"], loc)
            t.require(low <= high, "range", loc, "Invalid model slot range")

    def mapping(value, source, target, loc):
        nonlocal extra
        t.require(type(value) is dict, "shape", loc, "Expected explicit mapping object")
        t.require(set(value) == set(source), "mapping", loc, "Mapping must cover every logical ID exactly once")
        for target_id in value.values():
            t.identifier(target_id, loc)
        t.require(len(set(value.values())) == len(value) and set(value.values()) == set(target),
                  "mapping", loc, "Mapping must cover descriptor IDs without aliases or omissions")
        extra += len(value)

    bound_symbols, bound_models = 0, 0
    for collection in ("components", "circuits"):
        for definition in document[collection]:
            did = definition["id"]
            loc = f"$/{collection}/{did}"
            pins = {pin["id"]: pin for pin in definition["pins" if collection == "components" else "ports"]}
            t.require("symbol" in definition, "shape", loc, "Symbol binding must be explicit or null")
            binding = definition["symbol"]
            if binding is not None:
                t.fields(binding, "definition pin_map", loc + "/symbol")
                t.identifier(binding["definition"], loc)
                t.require(binding["definition"] in symbols, "reference", loc, "Unknown symbol descriptor")
                mapping(binding["pin_map"], pins, anchors[binding["definition"]], loc + "/symbol/pin_map")
                bound_symbols += 1
                extra += 1
            if collection == "components":
                t.require("model" in definition, "shape", loc, "Model binding must be explicit or null")
                binding = definition["model"]
                if binding is None:
                    continue
                t.fields(binding, "definition pin_map parameter_map", loc + "/model")
                mid = binding["definition"]
                t.identifier(mid, loc)
                t.require(mid in models, "reference", loc, "Unknown model interface descriptor")
                mapping(binding["pin_map"], pins, terminals[mid], loc + "/model/pin_map")
                mapping(binding["parameter_map"], parameters[did], slots[mid], loc + "/model/parameter_map")
                for source_id, target_id in binding["pin_map"].items():
                    t.require(pins[source_id]["domain"] == terminals[mid][target_id]["domain"], "mapping", loc, "Terminal domains differ")
                for source_id, target_id in binding["parameter_map"].items():
                    source, target = parameters[did][source_id], slots[mid][target_id]
                    t.require(source["unit"] == target["unit"], "unit", loc, "Model parameter dimensions differ")
                    t.require(p.number(target["minimum"], loc) <= source["minimum"] <= source["maximum"] <= p.number(target["maximum"], loc),
                              "range", loc, "Full declared range must fit the model interface slot")
                bound_models += 1
                extra += 1
    t.require(stats["declared_entities"] + stats["parameter_entries"] + extra <= t.MAX_ENTITIES,
              "budget", "$", "Combined descriptor/binding budget exceeded")
    return dict(**stats, descriptor_entries=extra, symbol_bindings=bound_symbols,
                model_bindings=bound_models, simulation_ready=False)


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
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise t.Invalid("input", "$", "Invalid encoding, JSON or nesting") from error


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("document", type=Path)
    args = parser.parse_args()
    try:
        with args.document.open("rb") as source:
            document = parse(source.read(t.MAX_BYTES + 1))
        print(json.dumps(dict(status="valid-descriptors-only", **validate(document))))
        return 0
    except (t.Invalid, OSError) as error:
        diagnostic = error.diagnostic() if isinstance(error, t.Invalid) else dict(code="input", location="$", message="Cannot read input")
        print(json.dumps(dict(status="invalid", **diagnostic)))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
