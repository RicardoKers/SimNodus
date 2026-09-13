"""Descriptor identity, mapping integrity and non-executing reference validation."""
import copy
import json
from pathlib import Path
import unittest
from unittest.mock import patch

import bindings as b
import parameters as p
import topology as t

FIXTURES = Path(__file__).with_name("fixtures")


class BindingTests(unittest.TestCase):
    def setUp(self):
        self.document = json.loads((FIXTURES / "two-rc-bindings.json").read_text())

    def invalid(self, code):
        with self.assertRaises(t.Invalid) as result:
            b.parse(json.dumps(self.document).encode())
        self.assertEqual(result.exception.code, code)

    def test_round_trip_preserves_full_document(self):
        before = copy.deepcopy(self.document)
        restored = b.parse(json.dumps(self.document, sort_keys=True).encode())
        self.assertEqual(before, restored)
        self.assertEqual(b.validate(restored), b.validate(before))
        self.assertEqual(before, self.document)
        self.assertFalse(b.validate(restored)["simulation_ready"])

    def test_symbol_replacement_preserves_topology_model_and_values(self):
        before = b.projection(self.document)
        model = copy.deepcopy(self.document["components"][0]["model"])
        self.document["components"][0]["symbol"] = dict(definition="two-pin-alt", pin_map={"p": "right", "n": "left"})
        b.validate(self.document)
        self.assertEqual(before, b.projection(self.document))
        self.assertEqual(p.resolve(before), p.resolve(b.projection(self.document)))
        self.assertEqual(model, self.document["components"][0]["model"])

    def test_display_name_and_descriptor_order_do_not_bind(self):
        expected = b.validate(self.document)
        self.document["symbols"].reverse()
        self.document["models"].reverse()
        for descriptor in self.document["symbols"] + self.document["models"]:
            descriptor["name"] = "Same display name"
        self.assertEqual(expected, b.validate(self.document))

    def test_null_model_is_unbound_not_a_fallback(self):
        self.document["components"][0]["model"] = None
        self.assertEqual(b.validate(self.document)["model_bindings"], 1)
        self.assertFalse(b.validate(self.document)["simulation_ready"])
        del self.document["components"][0]["model"]
        self.invalid("shape")

    def test_unknown_descriptor_and_wrong_category(self):
        binding = self.document["components"][0]["symbol"]
        for target in ("missing", "resistor-interface"):
            binding["definition"] = target
            self.invalid("reference")

    def test_missing_logical_pin_and_unknown_target(self):
        binding = self.document["components"][0]["symbol"]
        binding["pin_map"] = {"p": "a"}
        self.invalid("mapping")
        binding["pin_map"] = {"p": "a", "n": "absent"}
        self.invalid("mapping")

    def test_alias_fixture_rejected_after_round_trip(self):
        self.document = json.loads((FIXTURES / "invalid-model-alias.json").read_text())
        self.invalid("mapping")

    def test_unmapped_descriptor_anchor_rejected(self):
        self.document["symbols"][0]["pins"].append(dict(id="hidden", name="Hidden power pin"))
        self.invalid("mapping")

    def test_parameter_mapping_dimensions_and_complete_interval(self):
        slot = self.document["models"][0]["parameters"][0]
        slot["unit"] = "F"
        self.invalid("unit")
        slot["unit"] = "ohm"
        slot["maximum"] = "1000"
        self.invalid("range")

    def test_missing_parameter_mapping(self):
        self.document["components"][0]["model"]["parameter_map"] = {}
        self.invalid("mapping")

    def test_unused_descriptor_is_validated(self):
        self.document["models"].append(dict(id="unused", name="Unused", terminals=[], parameters=[
            dict(id="p", name="P", unit="ohm", minimum="2", maximum="1")]))
        self.invalid("range")

    def test_descriptor_and_local_duplicates(self):
        original = copy.deepcopy(self.document)
        self.document["symbols"].append(copy.deepcopy(self.document["symbols"][0]))
        self.invalid("id")
        self.document = original
        self.document["models"][0]["terminals"].append(copy.deepcopy(self.document["models"][0]["terminals"][0]))
        self.invalid("id")

    def test_resource_or_execution_fields_rejected(self):
        for field in ("path", "url", "dll", "script", "backend"):
            self.document["models"][0][field] = "untrusted"
            self.invalid("shape")
            del self.document["models"][0][field]
        self.document["circuits"][0]["model"] = None
        self.invalid("shape")

    def test_no_resource_io_during_parse_and_validate(self):
        raw = json.dumps(self.document).encode()
        with patch("builtins.open", side_effect=AssertionError("Unexpected resource read")), \
             patch.object(Path, "open", side_effect=AssertionError("Unexpected path read")):
            b.validate(b.parse(raw))

    def test_budget_counts_unused_descriptors(self):
        self.document["symbols"].extend(dict(id=f"unused{i}", name="Unused", pins=[]) for i in range(4096))
        self.invalid("budget")

    def test_previous_drafts_remain_separate(self):
        old = (FIXTURES / "two-rc-parameters.json").read_bytes()
        p.parse(old)
        with self.assertRaises(t.Invalid):
            b.parse(old)
        with self.assertRaises(t.Invalid):
            p.parse(json.dumps(self.document).encode())

    def test_mapping_type_and_duplicate_raw_keys(self):
        self.document["components"][0]["model"]["pin_map"]["p"] = []
        self.invalid("id")
        with self.assertRaises(t.Invalid) as result:
            b.parse(b'{"symbols":[],"symbols":[]}')
        self.assertEqual(result.exception.code, "input")


if __name__ == "__main__":
    unittest.main()
