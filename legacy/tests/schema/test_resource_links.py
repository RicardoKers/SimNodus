"""Typed resources preserve graph/value identity without trusting asset contents."""
import copy
import hashlib
import json
from pathlib import Path
import unittest
from unittest.mock import patch

import resource_links as l
import resource_lock as r
import topology as t

FIXTURES = Path(__file__).with_name("fixtures")


class ResourceLinkTests(unittest.TestCase):
    def setUp(self):
        self.document = json.loads((FIXTURES / "two-rc-resource-links.json").read_bytes())

    def invalid(self, code):
        with self.assertRaises(t.Invalid) as result:
            l.parse(json.dumps(self.document).encode())
        self.assertEqual(result.exception.code, code)

    def test_round_trip_and_readiness(self):
        before = copy.deepcopy(self.document)
        restored = l.parse(json.dumps(self.document, sort_keys=True).encode())
        self.assertEqual(before, restored)
        self.assertEqual(before, self.document)
        stats = l.validate(restored)
        self.assertEqual(stats["descriptors_linked"], 5)
        self.assertFalse(stats["simulation_ready"])
        self.assertFalse(stats["resource_interfaces_verified"])

    def test_owned_bytes_and_preserved_embedded_topology(self):
        root = Path(__file__).resolve().parents[2]
        for dependency in self.document["lock"]["dependencies"]:
            for file in dependency["files"]:
                raw = (root / file["path"]).read_bytes()
                self.assertEqual(len(raw), file["bytes"])
                self.assertEqual(hashlib.sha256(raw).hexdigest(), file["sha256"])
        self.assertEqual(self.document["topology"], json.loads((FIXTURES / "two-rc-bindings.json").read_bytes()))

    def test_null_is_absence_without_fallback(self):
        for key in self.document["models"]:
            self.document["models"][key] = None
        self.assertEqual(l.validate(self.document)["descriptors_linked"], 3)
        self.document["models"].pop(next(iter(self.document["models"])))
        self.invalid("mapping")

    def test_missing_asset_dependency_and_resource(self):
        asset = self.document["assets"][0]
        for field in ("dependency", "resource"):
            saved = asset[field]
            asset[field] = "absent"
            self.invalid("reference")
            asset[field] = saved
        self.document["symbols"][next(iter(self.document["symbols"]))] = "absent"
        self.invalid("reference")

    def test_wrong_kind_and_unsupported_kind(self):
        self.document["assets"][0]["kind"] = "model-spice"
        self.invalid("kind")
        for value in ("native-dll", "python", "svg", [], None):
            self.document["assets"][0]["kind"] = value
            self.invalid("kind")

    def test_duplicate_asset_and_role_alias(self):
        first = copy.deepcopy(self.document["assets"][0])
        self.document["assets"].append(first)
        self.invalid("id")
        first["id"] = "other"
        self.invalid("reference")

    def test_unused_asset_is_validated(self):
        self.document["assets"].append(dict(id="unused", kind="model-spice", dependency="missing", resource="model"))
        self.invalid("reference")

    def test_symbol_rebinding_and_rename_preserve_model_and_topology(self):
        before = copy.deepcopy(self.document)
        self.document["assets"][0]["id"] = "renamed-artwork"
        self.document["symbols"] = {key: "renamed-artwork" for key in self.document["symbols"]}
        l.validate(self.document)
        self.assertEqual(before["topology"], self.document["topology"])
        self.assertEqual(before["models"], self.document["models"])
        self.assertEqual(before["lock"], self.document["lock"])

    def test_no_io_or_resolution(self):
        raw = json.dumps(self.document).encode()
        with patch("builtins.open", side_effect=AssertionError("Unexpected I/O")), \
                patch.object(Path, "open", side_effect=AssertionError("Unexpected I/O")), \
                patch.object(Path, "resolve", side_effect=AssertionError("Unexpected resolution")):
            l.parse(raw)

    def test_nested_validation_not_bypassed(self):
        self.document["topology"]["root"] = "missing"
        self.invalid("reference")
        self.document["topology"]["root"] = "main"
        self.document["lock"]["dependencies"][0]["files"][0]["path"] = "../license"
        self.invalid("path")

    def test_budget_and_unknown_execution_fields(self):
        original = copy.deepcopy(self.document["assets"])
        self.document["assets"] = [{}] * (r.MAX_FILES + 1)
        self.invalid("budget")
        self.document["assets"] = original
        self.document["assets"][0]["load"] = True
        self.invalid("shape")

    def test_malformed_and_version(self):
        for raw in (b'{"a":1,"a":2}', b'NaN', b'\xff', b'[' * 40 + b']' * 40,
                    b' ' * (t.MAX_BYTES + 1)):
            with self.assertRaises(t.Invalid):
                l.parse(raw)
        self.document["version"] = "0.2"
        self.invalid("version")

    def test_explicit_entrypoint_and_complete_source_maps(self):
        model = self.document["models"]["resistor-interface"]
        self.assertEqual(model["entrypoint"], "declared_resistor")
        for token in ("R;run", "R\nquit", "", "R(x)", 1, []):
            model["entrypoint"] = token
            self.invalid("mapping")
        model["entrypoint"] = "declared_resistor"
        model["terminal_map"] = {}
        self.invalid("mapping")

    def test_model_tokens_do_not_alias_case_insensitively(self):
        model = self.document["models"]["resistor-interface"]
        model["terminal_map"] = {key: "same" for key in model["terminal_map"]}
        model["terminal_map"][next(iter(model["terminal_map"]))] = "SAME"
        self.invalid("mapping")

    def test_declared_entrypoint_does_not_prove_source_contents(self):
        self.document["models"]["resistor-interface"]["entrypoint"] = "missing_in_file"
        self.assertFalse(l.validate(self.document)["resource_interfaces_verified"])


if __name__ == "__main__":
    unittest.main()
