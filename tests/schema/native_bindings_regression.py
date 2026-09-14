"""Compare inert native 0.3 declarations with the unchanged Python oracle."""
import argparse
import copy
import json
import subprocess
from types import SimpleNamespace
import unittest

import bindings as reference
import parameters
import test_bindings as baseline
import topology as t

PROBE = ""
CASES = 0


def native(raw):
    global CASES
    CASES += 1
    run = subprocess.run([PROBE], input=raw, capture_output=True, timeout=15)
    actual = json.loads(run.stdout)
    assert run.returncode == (1 if "error" in actual else 0), run.stderr
    if "error" in actual:
        assert 0 <= actual["offset"] <= len(raw)
    return actual


def parse(raw):
    actual = native(raw)
    try:
        document = reference.parse(raw)
    except t.Invalid as error:
        assert actual.get("error") == error.code, (actual, error.code)
        raise
    expected = dict(stats=reference.validate(document), parameters=parameters.resolve(reference.projection(document)))
    assert actual == expected, (actual, expected)
    return document


def validate(document):
    parse(json.dumps(document).encode())
    return reference.validate(document)


class NativeBindings(baseline.BindingTests):
    def test_exact_combined_budget_and_unused_records(self):
        stats = validate(self.document)
        remaining = 4096 - sum(stats[k] for k in ("declared_entities", "parameter_entries", "descriptor_entries"))
        self.document["symbols"].extend(dict(id=f"unused{i}", name="Unused", pins=[]) for i in range(remaining))
        stats = validate(self.document)
        self.assertEqual(sum(stats[k] for k in ("declared_entities", "parameter_entries", "descriptor_entries")), 4096)
        self.document["symbols"].append(dict(id="overflow", name="Overflow", pins=[]))
        self.invalid("budget")

    def test_unused_slot_exact_decimals_and_closed_units(self):
        slot = dict(id="p", name="P", unit="1", minimum="-0.00", maximum="0e-24")
        self.document["models"].append(dict(id="unused", name="Unused", terminals=[], parameters=[slot]))
        for value in ("-0", "-0.00", "0e24", "0e-24", "9" * 32 + "e24", "0." + "0" * 30 + "1"):
            slot["minimum"] = slot["maximum"] = value
            validate(self.document)
        for value in ("1E0", "1e25", "1e-25", "01", "+1", "1" * 33, 1, True, None):
            slot["minimum"] = value
            self.invalid("quantity")
        slot["minimum"] = slot["maximum"] = "0"
        for unit in ("ohm", "F", "V", "s", "1"):
            slot["unit"] = unit
            validate(self.document)
        for unit in ("kohm", "uF", "v", "", None, 1):
            slot["unit"] = unit
            self.invalid("unit")

    def test_exact_full_interval_neighbors(self):
        source = self.document["components"][0]["parameters"][0]
        slot = self.document["models"][0]["parameters"][0]
        source.update(minimum="-1.0000000000000000000000000000001", maximum="1", default="0")
        # Remove fixture forwarding before changing the component's legal interval.
        for circuit in self.document["circuits"]:
            for instance in circuit["instances"]:
                instance["overrides"] = {}
        slot.update(minimum=source["minimum"], maximum="1.000")
        validate(self.document)
        slot["minimum"] = "-1"
        self.invalid("range")
        slot["minimum"] = source["minimum"]
        slot["maximum"] = "0.9999999999999999999999999999999"
        self.invalid("range")

    def test_catalog_namespaces_nulls_and_empty_circuit_interface(self):
        # Identical IDs across catalogs and local terminal/parameter scopes are legal.
        self.document["symbols"].append(dict(id="unused", name="\U0001f30d" * 80, pins=[]))
        self.document["models"].append(dict(id="unused", name="Unused", terminals=[dict(id="p", name="P", domain="electrical")],
            parameters=[dict(id="p", name="P", unit="1", minimum="0", maximum="0")]))
        self.document["circuits"].append(dict(id="empty", name="Empty", ports=[], parameters=[], instances=[], nets=[],
            symbol=dict(definition="unused", pin_map={})))
        validate(self.document)
        self.document["symbols"][-1]["name"] += "x"
        self.invalid("value")
        self.document["symbols"][-1]["name"] = "control\u0085"
        self.invalid("value")
        self.document["symbols"][-1]["name"] = "Unused"
        for definition in self.document["components"]:
            definition.update(symbol=None, model=None)
        for definition in self.document["circuits"]:
            definition["symbol"] = None
        self.document.update(symbols=[], models=[])
        stats = validate(self.document)
        self.assertEqual((stats["descriptor_entries"], stats["model_bindings"], stats["symbol_bindings"]), (0, 0, 0))
        self.assertFalse(stats["simulation_ready"])

    def test_every_nested_field_is_retained_or_rejected(self):
        original = copy.deepcopy(self.document)
        paths = []

        def walk(value, path):
            if isinstance(value, dict):
                paths.append(path)
                for key, child in value.items():
                    walk(child, path + [key])
            elif isinstance(value, list):
                for i, child in enumerate(value):
                    walk(child, path + [i])
        walk(original, [])
        for path in paths:
            self.document = copy.deepcopy(original)
            target = self.document
            for key in path:
                target = target[key]
            target["executable"] = "untrusted"
            with self.assertRaises(t.Invalid):
                parse(json.dumps(self.document).encode())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    args = parser.parse_args()
    PROBE = args.probe
    baseline.b = SimpleNamespace(parse=parse, validate=validate, projection=reference.projection)
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(NativeBindings)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    print(f"{CASES} native/reference binding comparisons")
    raise SystemExit(0 if result.wasSuccessful() else 1)
