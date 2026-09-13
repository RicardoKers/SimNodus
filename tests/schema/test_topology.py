"""Round-trip invariants and adversarial checks for topology 0.1."""
import copy
import json
from pathlib import Path
import unittest

from topology import Invalid, MAX_BYTES, parse, validate

FIXTURES = Path(__file__).with_name("fixtures")


def empty():
    return dict(format="simnodus-topology", version="0.1", root="main", components=[],
                circuits=[dict(id="main", name="Empty", ports=[], instances=[], nets=[])])


class TopologyTests(unittest.TestCase):
    def setUp(self):
        self.document = json.loads((FIXTURES / "two-rc.json").read_text())

    def invalid(self, document, code):
        with self.assertRaises(Invalid) as result:
            parse(json.dumps(document).encode())
        self.assertEqual(result.exception.code, code)

    def test_round_trip_preserves_tree_and_distinct_instance_paths(self):
        first = parse((FIXTURES / "two-rc.json").read_bytes())
        second = parse(json.dumps(first, sort_keys=True).encode())
        self.assertEqual(first, second)
        definitions = {d["id"]: d for d in second["circuits"]}
        paths = [(second["root"], instance["id"], leaf["id"])
                 for instance in definitions[second["root"]]["instances"]
                 for leaf in definitions[instance["definition"]]["instances"]]
        self.assertEqual(set(paths), {("main", "left", "r"), ("main", "left", "c"),
                                     ("main", "right", "r"), ("main", "right", "c")})
        self.assertEqual(validate(second)["root_expanded_instances"], 6)

    def test_read_only_validation_and_no_name_based_net_join(self):
        before = copy.deepcopy(self.document)
        validate(self.document)
        self.assertEqual(self.document, before)
        for net in self.document["circuits"][0]["nets"]:
            net["name"] = "GND"
        validate(self.document)
        self.assertEqual(len(self.document["circuits"][0]["nets"]), 4)

    def test_definition_order_does_not_change_resolution(self):
        expected = validate(self.document)
        self.document["circuits"].reverse()
        self.document["components"].reverse()
        self.assertEqual(validate(self.document), expected)

    def test_empty_round_trip(self):
        self.assertEqual(parse(json.dumps(empty()).encode()), empty())

    def test_invalid_fixtures_after_json_round_trip(self):
        for filename, code in (("invalid-cycle.json", "hierarchy"), ("invalid-terminal.json", "reference")):
            with self.subTest(filename=filename):
                self.invalid(json.loads((FIXTURES / filename).read_text()), code)

    def test_unknown_fields_and_execution_requests(self):
        for key, value in (("model", "evil.dll"), ("script", "run host code"), ("layout", {})):
            document = empty()
            document[key] = value
            self.invalid(document, "shape")

    def test_unsupported_version(self):
        self.document["version"] = "1.0"
        self.invalid(self.document, "version")

    def test_duplicate_global_and_local_ids(self):
        self.document["components"][0]["id"] = "main"
        self.invalid(self.document, "id")
        document = empty()
        document["circuits"][0]["ports"] = [dict(id="p", name="P", domain="electrical", direction="input")] * 2
        self.invalid(document, "id")

    def test_duplicate_terminal_across_nets(self):
        net = copy.deepcopy(self.document["circuits"][0]["nets"][0])
        net["id"] = "duplicate"
        self.document["circuits"][0]["nets"].append(net)
        self.invalid(self.document, "connection")

    def test_missing_and_wrong_kind_reference(self):
        for field, value in (("kind", "component"), ("definition", "absent")):
            document = copy.deepcopy(self.document)
            document["circuits"][0]["instances"][0][field] = value
            self.invalid(document, "reference")

    def test_unreachable_indirect_recursion(self):
        document = empty()
        for did, target in (("a", "b"), ("b", "a")):
            document["circuits"].append(dict(id=did, name=did, ports=[], nets=[], instances=[
                dict(id="child", name="Child", kind="circuit", definition=target)]))
        self.invalid(document, "hierarchy")

    def test_deep_hierarchy_and_shallow_exponential_expansion(self):
        for levels, width in ((9, 1), (8, 5)):
            document = empty()
            previous = document["circuits"][0]
            for level in range(1, levels):
                did = f"level{level}"
                previous["instances"] = [dict(id=f"child{i}", name="Child", kind="circuit", definition=did) for i in range(width)]
                previous = dict(id=did, name=did, ports=[], nets=[], instances=[])
                document["circuits"].append(previous)
            self.invalid(document, "hierarchy")

    def test_entity_budget(self):
        document = empty()
        document["circuits"][0]["ports"] = [dict(id=f"p{i}", name="P", domain="electrical", direction="input") for i in range(4096)]
        self.invalid(document, "budget")

    def test_wrong_types_keep_structured_errors(self):
        for value in (None, True, 3, [], {}):
            document = copy.deepcopy(self.document)
            document["circuits"][0]["instances"][0]["definition"] = value
            self.invalid(document, "id")

    def test_bad_json_encoding_constants_size_and_nesting(self):
        for raw in (b'{"format":1,"format":2}', b'\xff', b'{"x":NaN}', b' ' * (MAX_BYTES + 1),
                    b'[' * 40 + b'0' + b']' * 40, b'{'):
            with self.subTest(length=len(raw)):
                with self.assertRaises(Invalid) as result:
                    parse(raw)
                self.assertEqual(result.exception.code, "input")


if __name__ == "__main__":
    unittest.main()
