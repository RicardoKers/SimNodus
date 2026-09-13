"""Bounded reference validator for the experimental topology-only 0.1 draft."""
import argparse
import json
import re
from pathlib import Path

MAX_BYTES = 1024 * 1024
MAX_ENTITIES = 4096
MAX_DEPTH = 8
MAX_EXPANSION = 16384
ID = re.compile(r"[a-z][a-z0-9_-]{0,63}\Z")


class Invalid(ValueError):
    def __init__(self, code, location, message):
        self.code, self.location, self.message = code, location, message
        super().__init__(message)

    def diagnostic(self):
        return dict(code=self.code, location=self.location, message=self.message)


def require(condition, code, location, message):
    if not condition:
        raise Invalid(code, location, message)


def fields(value, expected, location):
    require(type(value) is dict and set(value) == set(expected.split()),
            "shape", location, "Object fields do not match the draft")


def array(value, location, minimum=0):
    require(type(value) is list and len(value) >= minimum, "shape", location, "Invalid array")
    return value


def identifier(value, location):
    require(type(value) is str and ID.fullmatch(value) is not None, "id", location, "Invalid ID")


def named(value, location):
    identifier(value["id"], location + "/id")
    name = value["name"]
    require(type(name) is str and 0 < len(name) <= 80 and all(ord(c) >= 32 and not 127 <= ord(c) <= 159 for c in name),
            "value", location + "/name", "Invalid display name")


def nesting(value, depth=0):
    if type(value) in (dict, list):
        require(depth < 32, "input", "$", "JSON nesting limit exceeded")
        for child in value.values() if type(value) is dict else value:
            nesting(child, depth + 1)


def validate(document):
    nesting(document)
    fields(document, "format version root components circuits", "$")
    require(document["format"] == "simnodus-topology" and document["version"] == "0.1",
            "version", "$", "Unsupported topology format/version")
    identifier(document["root"], "$/root")
    definitions = {}
    count = 0

    def register(value, scope, location):
        nonlocal count
        named(value, location)
        require(value["id"] not in scope, "id", location + "/id", "Duplicate ID")
        scope[value["id"]] = value
        count += 1
        require(count <= MAX_ENTITIES, "budget", location, "Declared entity budget exceeded")

    for kind, collection in (("component", "components"), ("circuit", "circuits")):
        for index, definition in enumerate(array(document[collection], "$/" + collection, int(kind == "circuit"))):
            loc = f"$/{collection}/{index}"
            fields(definition, "id name pins" if kind == "component" else "id name ports instances nets", loc)
            register(definition, definitions, loc)
    components = {d["id"]: d for d in document["components"]}
    circuits = {d["id"]: d for d in document["circuits"]}
    require(document["root"] in circuits, "reference", "$/root", "Root must reference a circuit")
    pins = {}
    ports = {}
    for collection, key in ((components, "pins"), (circuits, "ports")):
        for did, definition in collection.items():
            scope = {}
            for index, pin in enumerate(array(definition[key], f"$/{did}/{key}", int(key == "pins"))):
                loc = f"$/{did}/{key}/{index}"
                fields(pin, "id name domain" + (" direction" if key == "ports" else ""), loc)
                register(pin, scope, loc)
                require(pin["domain"] == "electrical", "value", loc, "Unsupported domain")
                if key == "ports":
                    require(pin["direction"] in ("input", "output", "bidirectional"), "value", loc, "Invalid direction")
            (pins if key == "pins" else ports)[did] = scope
    edges = {}
    for did, circuit in circuits.items():
        scope = dict(ports[did])
        instances = {}
        edges[did] = []
        for index, instance in enumerate(array(circuit["instances"], f"$/{did}/instances")):
            loc = f"$/{did}/instances/{index}"
            fields(instance, "id name kind definition", loc)
            register(instance, scope, loc)
            require(instance["kind"] in ("component", "circuit"), "value", loc, "Invalid instance kind")
            identifier(instance["definition"], loc + "/definition")
            targets = components if instance["kind"] == "component" else circuits
            require(instance["definition"] in targets, "reference", loc, "Missing or wrong-kind definition")
            instances[instance["id"]] = instance
            if instance["kind"] == "circuit":
                edges[did].append(instance["definition"])
        connected = set()
        for index, net in enumerate(array(circuit["nets"], f"$/{did}/nets")):
            loc = f"$/{did}/nets/{index}"
            fields(net, "id name terminals", loc)
            register(net, scope, loc)
            for terminal in array(net["terminals"], loc + "/terminals", 2):
                if type(terminal) is dict and set(terminal) == {"port"}:
                    identifier(terminal["port"], loc)
                    require(terminal["port"] in ports[did], "reference", loc, "Unknown local port")
                    identity = (None, terminal["port"])
                else:
                    fields(terminal, "instance terminal", loc)
                    identifier(terminal["instance"], loc)
                    identifier(terminal["terminal"], loc)
                    require(terminal["instance"] in instances, "reference", loc, "Unknown instance")
                    instance = instances[terminal["instance"]]
                    terminals = pins if instance["kind"] == "component" else ports
                    require(terminal["terminal"] in terminals[instance["definition"]], "reference", loc, "Unknown instance terminal")
                    identity = (terminal["instance"], terminal["terminal"])
                require(identity not in connected, "connection", loc, "Terminal connected more than once")
                connected.add(identity)
    memo = {}

    def walk(did, active):
        require(did not in active, "hierarchy", f"$/{did}", "Recursive circuit definitions")
        require(len(active) < MAX_DEPTH, "hierarchy", f"$/{did}", "Hierarchy depth exceeded")
        if did in memo:
            depth, expanded = memo[did]
            require(len(active) + depth <= MAX_DEPTH, "hierarchy", f"$/{did}", "Hierarchy depth exceeded")
            return depth, expanded
        depth, expanded = 1, len(circuits[did]["instances"])
        for child in edges[did]:
            child_depth, child_expanded = walk(child, active | {did})
            depth = max(depth, 1 + child_depth)
            expanded += child_expanded
            require(expanded <= MAX_EXPANSION, "hierarchy", f"$/{did}", "Expanded instance budget exceeded")
        memo[did] = depth, expanded
        return depth, expanded

    for did in circuits:
        walk(did, set())
    return dict(declared_entities=count, root_expanded_instances=memo[document["root"]][1],
                root_depth=memo[document["root"]][0])


def parse(raw):
    require(len(raw) <= MAX_BYTES, "input", "$", "Input byte budget exceeded")

    def pairs(items):
        value = {}
        for key, item in items:
            require(key not in value, "input", "$", "Duplicate JSON key")
            value[key] = item
        return value

    def constant(_):
        raise Invalid("input", "$", "Non-finite JSON literal")

    try:
        document = json.loads(raw.decode("utf-8"), object_pairs_hook=pairs, parse_constant=constant)
        validate(document)
        return document
    except (UnicodeError, json.JSONDecodeError, RecursionError) as error:
        raise Invalid("input", "$", "Invalid encoding, JSON or nesting") from error


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("document", type=Path)
    args = parser.parse_args()
    try:
        with args.document.open("rb") as source:
            document = parse(source.read(MAX_BYTES + 1))
        print(json.dumps(dict(status="valid-topology-only", **validate(document))))
        return 0
    except (Invalid, OSError) as error:
        diagnostic = error.diagnostic() if isinstance(error, Invalid) else dict(code="input", location="$", message="Cannot read input")
        print(json.dumps(dict(status="invalid", **diagnostic)))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
