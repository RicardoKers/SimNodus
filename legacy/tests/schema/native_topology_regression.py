"""Native topology differential acceptance; no resources or engine integration."""
import argparse
import copy
import json
import random
import subprocess
import unittest

import topology
import test_topology as baseline

PROBE = ""
CASES = 0
REFERENCE_PARSE = topology.parse
REFERENCE_VALIDATE = topology.validate


def native(raw):
    global CASES
    CASES += 1
    result = subprocess.run([PROBE], input=raw, capture_output=True, timeout=15)
    words = result.stdout.decode("ascii").split()
    if result.returncode == 1:
        assert len(words) == 3 and words[0] == "ERR", (result.stdout, result.stderr)
        assert 0 <= int(words[2]) <= len(raw)
        return words[1]
    assert result.returncode == 0 and len(words) == 4 and words[0] == "OK", (result.stdout, result.stderr)
    return dict(declared_entities=int(words[1]), root_depth=int(words[2]), root_expanded_instances=int(words[3]))


def parse(raw):
    actual = native(raw)
    try:
        document = REFERENCE_PARSE(raw)
    except topology.Invalid as error:
        assert actual == error.code, (actual, error.code, raw[:200])
        raise
    expected = REFERENCE_VALIDATE(document)
    assert actual == expected, (actual, expected)
    return document


def validate(document):
    actual = native(json.dumps(document, ensure_ascii=True).encode())
    expected = REFERENCE_VALIDATE(document)
    assert actual == expected, (actual, expected)
    return expected


class NativeTopology(baseline.TopologyTests):
    def test_unicode_names_and_scalar_count(self):
        for name in ("é" * 80, "𝄞" * 80, "\U0010ffff", " ", "e\u0301" * 40):
            document = baseline.empty()
            document["circuits"][0]["name"] = name
            parse(json.dumps(document, ensure_ascii=False).encode())
        for name in ("𝄞" * 81, "", "\x00", "\x1f", "\x7f", "\x80", "\x9f"):
            document = baseline.empty()
            document["circuits"][0]["name"] = name
            self.invalid(document, "value")

    def test_exact_entity_limit(self):
        document = baseline.empty()
        document["circuits"][0]["ports"] = [dict(id=f"p{i}", name="P", domain="electrical", direction="input") for i in range(4095)]
        self.assertEqual(validate(document)["declared_entities"], 4096)
        document["circuits"][0]["ports"].append(dict(id="overflow", name="P", domain="electrical", direction="input"))
        self.invalid(document, "budget")

    def test_exact_expansion_and_depth_limits(self):
        document = baseline.empty()
        document["components"] = [dict(id="leaf", name="Leaf", pins=[dict(id="p", name="P", domain="electrical")])]
        for level in range(7):
            children = [] if level == 6 else [dict(id=f"c{i}", name="Child", kind="circuit", definition=f"level{level+1}") for i in range(4)]
            document["circuits"].append(dict(id=f"level{level}", name="Level", ports=[], nets=[], instances=children))
        document["circuits"][0]["instances"] = [dict(id=f"fan{i}", name="Fan", kind="circuit", definition="level0") for i in range(3)]
        document["circuits"][0]["instances"].append(dict(id="leaf", name="Leaf", kind="component", definition="leaf"))
        result = validate(document)
        self.assertEqual(result["root_expanded_instances"], 16384)
        self.assertEqual(result["root_depth"], 8)
        document["circuits"][0]["instances"].append(dict(id="extra", name="Leaf", kind="component", definition="leaf"))
        self.invalid(document, "hierarchy")

    def test_port_instance_net_namespace(self):
        for collection in ("instances", "nets"):
            document = copy.deepcopy(self.document)
            circuit = document["circuits"][0]
            circuit["ports"] = [dict(id=circuit[collection][0]["id"], name="P", domain="electrical", direction="input")]
            self.invalid(document, "id")

    def test_all_empty_shapes_and_unknown_fields(self):
        for field in ("components", "circuits"):
            for value in (None, True, 3, "", {}):
                document = baseline.empty()
                document[field] = value
                self.invalid(document, "shape")
        for field in ("ports", "instances", "nets"):
            document = baseline.empty()
            document["circuits"][0][field] = {}
            self.invalid(document, "shape")
        document = baseline.empty()
        document["circuits"][0]["script"] = "do not run"
        self.invalid(document, "shape")
        document = baseline.empty()
        document["version"] = "0.3"
        self.invalid(document, "version")

    def test_generated_dags_and_order_independence(self):
        rng = random.Random(21056)
        for _ in range(40):
            document = baseline.empty()
            for level in reversed(range(6)):
                targets = [f"level{i}" for i in range(level+1, 6)]
                children = [dict(id=f"i{n}", name="Same name", kind="circuit", definition=rng.choice(targets)) for n in range(rng.randrange(4))] if targets else []
                document["circuits"].append(dict(id=f"level{level}", name="Level", ports=[], instances=children, nets=[]))
            document["circuits"][0]["instances"] = [dict(id="child", name="Child", kind="circuit", definition="level0")]
            expected = validate(document)
            rng.shuffle(document["circuits"])
            self.assertEqual(validate(document), expected)

    def test_nested_field_type_and_shape_mutations(self):
        paths = []

        def visit(value, path):
            if isinstance(value, dict):
                paths.append((path, True))
                for key, child in value.items():
                    paths.append((path + [key], False))
                    visit(child, path + [key])
            elif isinstance(value, list):
                for index, child in enumerate(value):
                    visit(child, path + [index])

        visit(self.document, [])
        for path, extra_field in paths:
            document = copy.deepcopy(self.document)
            value = document
            for part in path if extra_field else path[:-1]:
                value = value[part]
            if extra_field:
                value["unrecognized"] = "inert"
            else:
                value[path[-1]] = None
            with self.assertRaises(topology.Invalid):
                parse(json.dumps(document).encode())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    PROBE = parser.parse_args().probe
    # Reuse the unchanged suite while comparing every parse/validation to native.
    baseline.parse = parse
    baseline.validate = validate
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(NativeTopology))
    print(f"{CASES} native/reference comparisons")
    raise SystemExit(0 if result.wasSuccessful() else 1)
