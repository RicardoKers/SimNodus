"""Exact quantities, scoped overrides and immutable round-trip contracts."""
import copy
from decimal import localcontext
import json
from pathlib import Path
import unittest

import parameters as p
import topology as t

FIXTURES = Path(__file__).with_name("fixtures")


class ParameterTests(unittest.TestCase):
    def setUp(self):
        self.document = json.loads((FIXTURES / "two-rc-parameters.json").read_text())

    def invalid(self, code):
        with self.assertRaises(t.Invalid) as result:
            p.parse(json.dumps(self.document).encode())
        self.assertEqual(result.exception.code, code)

    def rows(self):
        return {tuple(row["path"]): row["parameters"] for row in p.resolve(self.document)["instances"]}

    def test_round_trip_and_distinct_forwarded_values(self):
        original = copy.deepcopy(self.document)
        restored = p.parse(json.dumps(self.document, sort_keys=True).encode())
        self.assertEqual(original, restored)
        self.assertEqual(p.resolve(restored), p.resolve(original))
        rows = self.rows()
        self.assertEqual(rows[("main", "left", "r")]["resistance"]["value"], "1000")
        self.assertEqual(rows[("main", "right", "r")]["resistance"]["value"], "2200")
        self.assertEqual(rows[("main", "right", "c")]["capacitance"]["value"], "0.000000220")
        self.assertEqual(rows[("main", "left", "c")]["capacitance"]["value"], "0.000001")
        self.assertEqual(original, self.document)

    def test_override_changes_one_instance_not_shared_definition(self):
        before = self.rows()
        definitions = copy.deepcopy(self.document["components"])
        self.document["circuits"][0]["instances"][1]["overrides"]["resistance"]["value"] = "3"
        after = self.rows()
        self.assertEqual(before[("main", "left", "r")], after[("main", "left", "r")])
        self.assertEqual(after[("main", "right", "r")]["resistance"]["value"], "3000")
        self.assertEqual(definitions, self.document["components"])

    def test_literal_units_normalize_exactly_independent_of_decimal_context(self):
        with localcontext() as context:
            context.prec = 3
            self.assertEqual(p.quantity({"value": "0.1", "unit": "kohm"}, "$"), p.quantity({"value": "100", "unit": "ohm"}, "$"))
            self.assertEqual(str(p.quantity({"value": "1.2345678901234567890123456789012", "unit": "mV"}, "$")[1]),
                             "0.0012345678901234567890123456789012")

    def test_inclusive_bounds(self):
        value = self.document["circuits"][0]["instances"][0]["overrides"]["resistance"]
        for literal in ("0.1", "10"):
            value["value"] = literal
            p.validate(self.document)
        value["value"] = "10.000000000000000000000000000001"
        self.invalid("range")

    def test_dimension_fixture_after_round_trip(self):
        self.document = json.loads((FIXTURES / "invalid-parameter-dimension.json").read_text())
        self.invalid("unit")

    def test_default_range_and_forwarded_full_interval(self):
        declaration = self.document["circuits"][1]["parameters"][0]
        declaration["default"] = "0"
        self.invalid("range")
        declaration["default"] = "1000"
        declaration["maximum"] = "1000001"
        # Default still fits the resistor, but one legal parent override would not.
        self.invalid("range")

    def test_unknown_units_and_json_numeric_values(self):
        override = self.document["circuits"][0]["instances"][0]["overrides"]["resistance"]
        for unit in ("Ohm", "kOhm", "m", "ohm*2", None, []):
            override["unit"] = unit
            self.invalid("unit")
        override["unit"] = "ohm"
        for value in (1000, True, None, [], "NaN", "Infinity", "1+2", "1e999999", "01", "+1", "1,2"):
            override["value"] = value
            self.invalid("quantity")

    def test_number_budgets(self):
        for value in ("1" * 33, "1e25", "1e-25", "1" * 1000):
            with self.assertRaises(t.Invalid):
                p.number(value, "$")
        p.number("1e24", "$")
        p.number("1e-24", "$")

    def test_parent_reference_scope_and_no_expressions(self):
        override = self.document["circuits"][1]["instances"][0]["overrides"]
        override["resistance"] = {"parameter": "main.resistance"}
        self.invalid("id")
        override["resistance"] = {"parameter": "absent"}
        self.invalid("reference")
        override["resistance"] = {"parameter": "capacitance"}
        self.invalid("unit")
        override["resistance"] = {"expression": "resistance*2"}
        self.invalid("shape")

    def test_unknown_target_parameter_and_duplicate_declaration(self):
        self.document["circuits"][0]["instances"][0]["overrides"]["unknown"] = {"value": "1", "unit": "ohm"}
        self.invalid("reference")
        del self.document["circuits"][0]["instances"][0]["overrides"]["unknown"]
        self.document["components"][0]["parameters"] *= 2
        self.invalid("id")

    def test_unknown_fields_not_dropped_by_topology_projection(self):
        self.document["components"][0]["model"] = "untrusted.dll"
        self.invalid("shape")

    def test_old_contract_remains_separate(self):
        old = (FIXTURES / "two-rc.json").read_bytes()
        t.parse(old)
        with self.assertRaises(t.Invalid):
            p.parse(old)
        with self.assertRaises(t.Invalid):
            t.parse(json.dumps(self.document).encode())

    def test_resolved_value_budget_not_only_topology_size(self):
        component = self.document["components"][0]
        component["parameters"] = [dict(id=f"p{i}", name="P", unit="1", default="1", minimum="0", maximum="2") for i in range(100)]
        leaf = dict(id="leaf", name="Leaf", ports=[], nets=[], parameters=[], instances=[
            dict(id=f"r{i}", name="R", kind="component", definition="resistor", overrides={}) for i in range(100)])
        root = dict(id="main", name="Main", ports=[], nets=[], parameters=[], instances=[
            dict(id=f"copy{i}", name="Copy", kind="circuit", definition="leaf", overrides={}) for i in range(2)])
        self.document["circuits"] = [root, leaf]
        self.invalid("budget")

    def test_raw_duplicate_keys_and_nonfinite(self):
        for raw in (b'{"version":"0.2","version":"0.2"}', b'{"value":NaN}', b'\xff'):
            with self.assertRaises(t.Invalid) as result:
                p.parse(raw)
            self.assertEqual(result.exception.code, "input")


if __name__ == "__main__":
    unittest.main()
