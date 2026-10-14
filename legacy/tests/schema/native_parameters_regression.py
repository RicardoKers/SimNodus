"""Differential native parameter snapshots without resource or engine execution."""
import argparse
import copy
import json
import random
import subprocess
from types import SimpleNamespace
import unittest

import parameters as reference
import test_parameters as baseline
import topology as t

PROBE = ""
CASES = 0


def native(raw):
    global CASES
    CASES += 1
    run = subprocess.run([PROBE], input=raw, capture_output=True, timeout=15)
    result = json.loads(run.stdout)
    if "error" in result:
        assert run.returncode == 1 and 0 <= result["offset"] <= len(raw)
    else:
        assert run.returncode == 0, run.stderr
    return result


def parse(raw):
    actual = native(raw)
    try:
        document = reference.parse(raw)
    except t.Invalid as error:
        assert actual.get("error") == error.code, (actual, error.code)
        raise
    assert actual == reference.resolve(document), (actual, reference.resolve(document))
    return document


def resolve(document):
    actual = native(json.dumps(document).encode())
    expected = reference.resolve(document)
    assert actual == expected, (actual, expected)
    return expected


def validate(document):
    parse(json.dumps(document).encode())
    return reference.validate(document)


def scalar_document(value, unit="1"):
    base = reference.UNITS.get(unit, ("1", 0))[0] if isinstance(unit, str) else "1"
    parameter = dict(id="p", name="P", unit=base, default="0", minimum="-" + "9" * 32 + "e24", maximum="9" * 32 + "e24")
    return dict(format="simnodus-topology", version="0.2", root="main", components=[dict(id="leaf", name="Leaf",
        pins=[dict(id="p", name="P", domain="electrical")], parameters=[parameter])], circuits=[dict(id="main", name="Main",
        ports=[], parameters=[], nets=[], instances=[dict(id="child", name="Child", kind="component", definition="leaf",
        overrides=dict(p=dict(value=value, unit=unit)))])])


def quantity(value, location):
    document = scalar_document(value["value"], value["unit"])
    result = native(json.dumps(document).encode())
    expected = reference.quantity(value, location)
    actual = result["instances"][0]["parameters"]["p"]
    assert (actual["unit"], actual["value"]) == (expected[0], format(expected[1], "f"))
    return expected


def number(value, location):
    actual = native(json.dumps(scalar_document(value)).encode())
    try:
        expected = reference.number(value, location)
    except t.Invalid as error:
        assert actual.get("error") == error.code
        raise
    assert actual["instances"][0]["parameters"]["p"]["value"] == format(expected, "f")
    return expected


class NativeParameters(baseline.ParameterTests):
    def test_decimal_grammar_signed_zero_and_precision(self):
        for value in ("-0", "-0.000", "-0e24", "0e-24", "0.00e+2", "1.200e2", "1e+0", "1e-0", "9" * 32,
                      "1e24", "1e-24", "0." + "0" * 30 + "1", "-12345678901234567890123456789012e-24"):
            for unit in reference.UNITS:
                parse(json.dumps(scalar_document(value, unit)).encode())
        for value in ("", " ", "+0", "00", "-01", ".1", "1.", "1E0", "1e01", "1e+01", "1e-01", "1e", "1e+",
                      "1e25", "1e-25", "0." + "0" * 31 + "1", "1" * 65, "1e99999999999999999999"):
            with self.assertRaises(t.Invalid):
                parse(json.dumps(scalar_document(value)).encode())

    def test_generated_exact_quantities(self):
        rng = random.Random(21057)
        for _ in range(100):
            coefficient = str(rng.randrange(1, 10)) + "".join(str(rng.randrange(10)) for _ in range(rng.randrange(30)))
            decimal = coefficient[:1] + "." + coefficient[1:] if len(coefficient) > 1 else coefficient
            value = ("-" if rng.randrange(2) else "") + decimal + "e" + str(rng.randrange(-24, 25))
            parse(json.dumps(scalar_document(value, rng.choice(list(reference.UNITS)))).encode())

    def test_negative_inclusive_ranges_and_exact_neighbor(self):
        document = scalar_document("-0.100000000000000000000000000001")
        declaration = document["components"][0]["parameters"][0]
        declaration.update(minimum="-0.1", maximum="0.1", default="0")
        with self.assertRaises(t.Invalid):
            parse(json.dumps(document).encode())
        for value in ("-0.1", "0.1", "-0", "0.000"):
            document["circuits"][0]["instances"][0]["overrides"]["p"]["value"] = value
            parse(json.dumps(document).encode())

    def test_unused_declarations_and_missing_overrides(self):
        document = copy.deepcopy(self.document)
        document["root"] = "rc"
        document["circuits"][0]["instances"][0]["overrides"]["resistance"]["value"] = "-1"
        with self.assertRaises(t.Invalid):
            parse(json.dumps(document).encode())
        document = copy.deepcopy(self.document)
        del document["circuits"][0]["instances"][0]["overrides"]
        with self.assertRaises(t.Invalid):
            parse(json.dumps(document).encode())

    def test_parameter_names_and_duplicate_namespaces(self):
        document = scalar_document("1")
        parameter = document["components"][0]["parameters"][0]
        parameter["name"] = "𝄞" * 80
        parse(json.dumps(document, ensure_ascii=False).encode())
        for name in ("𝄞" * 81, "\x80", "\x00"):
            parameter["name"] = name
            with self.assertRaises(t.Invalid):
                parse(json.dumps(document).encode())

    def test_exact_resolved_budget(self):
        # 128 occurrences * (one row + 127 values) = 16384 entries.
        document = scalar_document("1")
        component = document["components"][0]
        component["parameters"] = [dict(id=f"p{i}", name="P", unit="1", default="0", minimum="-1", maximum="1") for i in range(127)]
        instances = [dict(id=f"i{i}", name="I", kind="component", definition="leaf", overrides={}) for i in range(128)]
        document["circuits"][0]["instances"] = instances
        self.assertEqual(resolve(document)["resolved_entries"], 16384)
        instances.append(dict(id="overflow", name="I", kind="component", definition="leaf", overrides={}))
        with self.assertRaises(t.Invalid):
            parse(json.dumps(document).encode())

    def test_exact_combined_declaration_override_budget(self):
        document = scalar_document("1")
        component = document["components"][0]
        component["parameters"] = [dict(id=f"p{i}", name="P", unit="1", default="0", minimum="-1", maximum="1") for i in range(4092)]
        override = document["circuits"][0]["instances"][0]["overrides"]
        override.clear()
        result = resolve(document)
        self.assertEqual(result["declared_entities"] + result["parameter_entries"], 4096)
        override["p0"] = dict(value="0", unit="1")
        with self.assertRaises(t.Invalid):
            parse(json.dumps(document).encode())

    def test_forwarding_and_literals_retain_use_site_origins(self):
        before = resolve(self.document)
        self.document["circuits"][0]["instances"][0]["overrides"]["resistance"]["value"] = "1.000"
        after = resolve(self.document)
        rows = {tuple(row["path"]): row for row in after["instances"]}
        value = rows[("main", "left", "r")]["parameters"]["resistance"]
        self.assertEqual(value, dict(value="1000", unit="ohm", origin="containing-circuit:resistance"))
        right_before = [row for row in before["instances"] if row["path"][:2] == ["main", "right"]]
        right_after = [row for row in after["instances"] if row["path"][:2] == ["main", "right"]]
        self.assertEqual(right_before, right_after)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    PROBE = parser.parse_args().probe
    baseline.p = SimpleNamespace(parse=parse, resolve=resolve, validate=validate, quantity=quantity, number=number)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(NativeParameters))
    print(f"{CASES} native/reference parameter comparisons")
    raise SystemExit(0 if result.wasSuccessful() else 1)
