"""Source graph content/provenance against preserved project declarations."""
import argparse
import copy
import json
import subprocess
from types import SimpleNamespace
import unittest

import project as reference
import test_project as baseline
import topology as t

PROBE = ""
CASES = 0


def expected_graph(topology):
    """Independent source view: connectivity fields only, with original order."""
    view = copy.deepcopy(topology)
    for key in ("format", "version", "symbols", "models"):
        del view[key]
    for definition in view["components"] + view["circuits"]:
        for key in ("parameters", "symbol", "model"):
            definition.pop(key, None)
        for instance in definition.get("instances", []):
            del instance["overrides"]
    return view


def expected_positions(topology):
    positions = {("root",): topology["root"]}
    for collection in ("components", "circuits"):
        for definition in topology[collection]:
            path = (collection, definition["id"])
            positions[path] = definition
            for kind in ("pins", "ports", "instances", "nets"):
                for entity in definition.get(kind, []):
                    subpath = path + (kind, entity["id"])
                    positions[subpath] = entity
                    for terminal in entity.get("terminals", []):
                        suffix = ("port", terminal["port"]) if "port" in terminal else ("instance", terminal["instance"], terminal["terminal"])
                        positions[subpath + ("terminals",) + suffix] = terminal
    return positions


def compare(raw):
    global CASES
    CASES += 1
    run = subprocess.run([PROBE], input=raw, capture_output=True, timeout=15)
    actual = json.loads(run.stdout)
    assert run.returncode == (1 if "error" in actual else 0), run.stderr
    try:
        document = reference.parse(raw)
    except t.Invalid as error:
        assert set(actual) == {"error", "offset"}, actual
        assert actual["error"] == error.code, (actual, error.code)
        assert 0 <= actual["offset"] <= len(raw)
        raise
    stats = {k:v for k,v in actual.items() if k not in ("graph", "positions")}
    assert stats == reference.validate(document)
    topology = document["sources"]["topology"]
    assert actual["graph"] == expected_graph(topology)
    expected = expected_positions(topology)
    identities = [tuple(p["identity"]) for p in actual["positions"]]
    assert len(set(identities)) == len(identities) and set(identities) == set(expected)
    for entry in actual["positions"]:
        begin, end = entry["begin"], entry["end"]
        assert 0 <= begin < end <= len(raw)
        assert json.loads(raw[begin:end]) == expected[tuple(entry["identity"])], entry
    return document, actual


def parse(raw):
    return compare(raw)[0]


def validate(document):
    parse(json.dumps(document).encode())
    return reference.validate(document)


def inspect(document, **options):
    return compare(json.dumps(document, **options).encode())[1]


class NativeGraph(baseline.ProjectTests):
    def empty(self):
        sources = self.document["sources"]
        sources["assets"] = []
        sources["symbols"] = {}
        sources["models"] = {}
        sources["topology"] = dict(format="simnodus-topology", version="0.3", root="main", components=[],
            circuits=[dict(id="main", name="Empty", ports=[], instances=[], nets=[], parameters=[], symbol=None)],
            symbols=[], models=[])

    def test_order_names_unicode_and_source_keys(self):
        before = inspect(self.document)
        topology = self.document["sources"]["topology"]
        def reorder(value):
            if isinstance(value, dict):
                if "name" in value:
                    value["name"] = 'GND "same" \\ \U0001f642'
                for item in value.values():
                    reorder(item)
            elif isinstance(value, list):
                value.reverse()
                for item in value:
                    reorder(item)
        reorder(topology)
        after = inspect(self.document, ensure_ascii=False, sort_keys=True, indent=1)
        self.assertEqual({tuple(x["identity"]) for x in before["positions"]}, {tuple(x["identity"]) for x in after["positions"]})
        self.assertEqual(len(before["graph"]["circuits"]), len(after["graph"]["circuits"]))

    def test_unreachable_definitions_and_disconnected_ports(self):
        topology = self.document["sources"]["topology"]
        extra = copy.deepcopy(topology["circuits"][1])
        extra["id"] = "unreachable"
        extra["symbol"] = None
        extra["ports"].append(dict(id="floating", name="GND", domain="electrical", direction="input"))
        topology["circuits"].append(extra)
        result = inspect(self.document)
        self.assertEqual(len(result["graph"]["circuits"]), 3)
        self.assertEqual(len(result["graph"]["circuits"][-1]["nets"]), 3)
        self.assertEqual(result["graph"]["circuits"][-1]["ports"][-1]["id"], "floating")

    def test_empty_graph_and_exact_4096_entity_boundary(self):
        self.empty()
        inspect(self.document)
        root = self.document["sources"]["topology"]["circuits"][0]
        root["ports"] = [dict(id=f"p{i}", name="Unconnected", domain="electrical",
            direction=("input", "output", "bidirectional")[i % 3]) for i in range(4095)]
        result = inspect(self.document)
        self.assertEqual(result["topology_stats"]["declared_entities"], 4096)
        self.assertEqual(len(result["graph"]["circuits"][0]["ports"]), 4095)
        root["ports"].append(dict(id="overflow", name="Overflow", domain="electrical", direction="input"))
        self.invalid("budget")

    def test_source_metadata_changes_do_not_rewire_connectivity(self):
        before = inspect(self.document)["graph"]
        topology = self.document["sources"]["topology"]
        for component in topology["components"]:
            component["symbol"] = None
            component["model"] = None
        for circuit in topology["circuits"]:
            circuit["symbol"] = None
        after = inspect(self.document)
        self.assertEqual(before, after["graph"])
        self.assertFalse(after["simulation_ready"])
        self.assertFalse(after["resource_interfaces_verified"])

    def test_invalid_unreachable_graph_and_connection_reject_all_output(self):
        saved = copy.deepcopy(self.document)
        root = self.document["sources"]["topology"]["circuits"][0]
        root["nets"][0]["terminals"].append(copy.deepcopy(root["nets"][0]["terminals"][0]))
        self.invalid("connection")
        self.document = copy.deepcopy(saved)
        topology = self.document["sources"]["topology"]
        extra = copy.deepcopy(topology["circuits"][1])
        extra["id"] = "unreachable"
        extra["instances"][0]["definition"] = "absent"
        topology["circuits"].append(extra)
        self.invalid("reference")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    PROBE = parser.parse_args().probe
    baseline.project = SimpleNamespace(parse=parse, validate=validate)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(NativeGraph))
    print(f"{CASES} native/reference source-graph comparisons with complete provenance checks")
    raise SystemExit(0 if result.wasSuccessful() else 1)
