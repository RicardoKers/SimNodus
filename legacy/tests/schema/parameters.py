"""Experimental topology 0.2: exact quantities and scoped parameter forwarding."""
import argparse
import copy
from decimal import Decimal, localcontext
import json
from pathlib import Path
import re

import topology as t

# Closed vocabulary; spelling never selects a model or parser expression.
UNITS = {"ohm": ("ohm", 0), "kohm": ("ohm", 3), "F": ("F", 0),
         "uF": ("F", -6), "nF": ("F", -9), "V": ("V", 0), "mV": ("V", -3),
         "s": ("s", 0), "ms": ("s", -3), "1": ("1", 0)}
BASE_UNITS = {"ohm", "F", "V", "s", "1"}
NUMBER = re.compile(r"-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:e[+-]?(?:0|[1-9][0-9]*))?\Z")


def number(value, loc):
    t.require(type(value) is str and len(value) <= 64 and NUMBER.fullmatch(value) is not None,
              "quantity", loc, "Expected bounded decimal string")
    mantissa, _, exponent = value.partition("e")
    t.require(sum(c.isdigit() for c in mantissa) <= 32 and abs(int(exponent or "0")) <= 24,
              "quantity", loc, "Decimal precision/exponent budget exceeded")
    return Decimal(value)


def quantity(value, loc):
    t.fields(value, "value unit", loc)
    t.require(type(value["unit"]) is str and value["unit"] in UNITS, "unit", loc, "Unknown unit")
    base, exponent = UNITS[value["unit"]]
    with localcontext() as context:
        context.prec = 100
        normalized = number(value["value"], loc).scaleb(exponent)
    return base, normalized


def validate(document):
    t.nesting(document)
    t.fields(document, "format version root components circuits", "$")
    t.require(document["format"] == "simnodus-topology" and document["version"] == "0.2",
              "version", "$", "Expected experimental topology 0.2")
    # Projection validates unchanged topology rules without rewriting 0.1 or
    # accepting unknown fields. Only the two explicitly added fields are removed.
    projected = copy.deepcopy(document)
    projected["version"] = "0.1"
    for collection in ("components", "circuits"):
        for definition in t.array(projected[collection], "$/" + collection):
            if type(definition) is dict:
                definition.pop("parameters", None)
                if collection == "circuits" and type(definition.get("instances")) is list:
                    for instance in definition["instances"]:
                        if type(instance) is dict:
                            instance.pop("overrides", None)
    stats = t.validate(projected)
    declarations = {}
    definitions = {d["id"]: d for d in document["components"] + document["circuits"]}
    extra = 0
    for did, definition in definitions.items():
        t.require("parameters" in definition, "shape", f"$/{did}", "Missing parameter declarations")
        parameters = {}
        for index, declaration in enumerate(t.array(definition["parameters"], f"$/{did}/parameters")):
            loc = f"$/{did}/parameters/{index}"
            t.fields(declaration, "id name unit default minimum maximum", loc)
            t.named(declaration, loc)
            pid = declaration["id"]
            t.require(pid not in parameters, "id", loc, "Duplicate parameter ID")
            unit = declaration["unit"]
            t.require(type(unit) is str and unit in BASE_UNITS, "unit", loc, "Declaration requires a base unit")
            low, high, default = (number(declaration[key], loc + "/" + key) for key in ("minimum", "maximum", "default"))
            t.require(low <= default <= high, "range", loc, "Invalid bounds or default outside inclusive range")
            parameters[pid] = dict(unit=unit, minimum=low, maximum=high, default=default)
            extra += 1
        declarations[did] = parameters
    for circuit in document["circuits"]:
        source = declarations[circuit["id"]]
        for instance in circuit["instances"]:
            loc = f"$/{circuit['id']}/instances/{instance['id']}/overrides"
            t.require(type(instance.get("overrides")) is dict, "shape", loc, "Missing or invalid overrides")
            target = declarations[instance["definition"]]
            for pid, override in instance["overrides"].items():
                t.identifier(pid, loc)
                t.require(pid in target, "reference", loc, "Unknown target parameter")
                declaration = target[pid]
                if type(override) is dict and set(override) == {"parameter"}:
                    t.identifier(override["parameter"], loc)
                    t.require(override["parameter"] in source, "reference", loc, "Unknown containing-circuit parameter")
                    forwarded = source[override["parameter"]]
                    t.require(forwarded["unit"] == declaration["unit"], "unit", loc, "Incompatible forwarded dimensions")
                    t.require(declaration["minimum"] <= forwarded["minimum"] <= forwarded["maximum"] <= declaration["maximum"],
                              "range", loc, "Forwarded range must fit target range for all legal values")
                else:
                    unit, value = quantity(override, loc)
                    t.require(unit == declaration["unit"], "unit", loc, "Incompatible quantity dimension")
                    t.require(declaration["minimum"] <= value <= declaration["maximum"], "range", loc, "Override outside inclusive range")
                extra += 1
    t.require(stats["declared_entities"] + extra <= t.MAX_ENTITIES, "budget", "$", "Combined entity/parameter/override budget exceeded")
    costs = {}

    def expansion_cost(did):
        if did not in costs:
            cost = 0
            for instance in definitions[did].get("instances", []):
                target_id = instance["definition"]
                cost += 1 + len(declarations[target_id])
                if instance["kind"] == "circuit":
                    cost += expansion_cost(target_id)
                t.require(cost <= t.MAX_EXPANSION, "budget", f"$/{did}", "Resolved instance/parameter entry budget exceeded")
            costs[did] = cost
        return costs[did]

    for did in definitions:
        expansion_cost(did)
    return dict(**stats, parameter_entries=extra,
                resolved_entries=costs[document["root"]]), declarations


def resolve(document):
    """Return an inspection-only snapshot; do not mutate definitions or load models."""
    stats, declarations = validate(document)
    definitions = {d["id"]: d for d in document["components"] + document["circuits"]}
    rows = []

    def visit(did, path, values):
        for instance in definitions[did].get("instances", []):
            target = declarations[instance["definition"]]
            effective = {pid: declaration["default"] for pid, declaration in target.items()}
            origins = {pid: "default" for pid in target}
            for pid, override in instance["overrides"].items():
                if "parameter" in override:
                    effective[pid] = values[override["parameter"]]
                    origins[pid] = "containing-circuit:" + override["parameter"]
                else:
                    effective[pid] = quantity(override, "$/overrides")[1]
                    origins[pid] = "literal"
            child_path = path + [instance["id"]]
            rows.append(dict(path=child_path, definition=instance["definition"], parameters={pid:
                dict(value=format(value, "f"), unit=target[pid]["unit"], origin=origins[pid])
                for pid, value in effective.items()}))
            if instance["kind"] == "circuit":
                visit(instance["definition"], child_path, effective)

    root = document["root"]
    visit(root, [root], {pid: declaration["default"] for pid, declaration in declarations[root].items()})
    return dict(status="valid-parameters-only", **stats, instances=rows)


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
        print(json.dumps(resolve(document)))
        return 0
    except (t.Invalid, OSError) as error:
        diagnostic = error.diagnostic() if isinstance(error, t.Invalid) else dict(code="input", location="$", message="Cannot read input")
        print(json.dumps(dict(status="invalid", **diagnostic)))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
