# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Connectivity expansion against an independent adjacency/traversal reference."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import unittest

import project as reference

PROBE = ""
REQUESTS = 0
RAW = (Path(__file__).parent / "fixtures/two-rc-project.json").read_bytes()


def expanded(document):
    topology = document["sources"]["topology"]
    definitions = {d["id"]: d for d in topology["components"] + topology["circuits"]}
    occurrences, terminals, nets, adjacency = {}, {}, {}, {}

    def visit(definition, path, kind, source):
        occurrences[path] = (definition["id"], kind, source)
        for terminal in definition["pins" if kind == "component" else "ports"]:
            key = (path, terminal["id"])
            terminals[key] = terminal; adjacency[key] = set()
        if kind == "component": return
        for child in definition["instances"]:
            visit(definitions[child["definition"]], path + (child["id"],), child["kind"], child)
        for net in definition["nets"]:
            members = [(path, t["port"]) if "port" in t else (path + (t["instance"],), t["terminal"]) for t in net["terminals"]]
            nets[(path, net["id"])] = (net, members[0])
            for a, b in zip(members, members[1:]): adjacency[a].add(b); adjacency[b].add(a)

    root = topology["root"]
    visit(definitions[root], (root,), "circuit", root)
    unseen = set(adjacency)
    groups = []
    for seed in sorted(adjacency):
        if seed not in unseen: continue
        pending = [seed]; reached = set()
        while pending:
            node = pending.pop()
            if node in reached: continue
            reached.add(node); pending.extend(adjacency[node] - reached)
        unseen -= reached
        groups.append((sorted(reached), sorted(key for key, (_, member) in nets.items() if member in reached)))
    return occurrences, terminals, nets, groups


def invoke(raw):
    global REQUESTS
    REQUESTS += 1
    run = subprocess.run([PROBE], input=raw, capture_output=True, timeout=60)
    result = json.loads(run.stdout)
    assert run.returncode == (1 if "error" in result else 0), run.stderr
    return result


def minimal():
    document = json.loads(RAW)
    document["sources"]["topology"] = dict(format="simnodus-topology", version="0.3", root="main", components=[],
        circuits=[dict(id="main", name="Main", ports=[], instances=[], nets=[], parameters=[], symbol=None)], symbols=[], models=[])
    document["sources"].update(assets=[], symbols={}, models={})
    return document


class Connectivity(unittest.TestCase):
    def compare(self, document=None, raw=None):
        if raw is None: raw = json.dumps(document).encode()
        document = reference.parse(raw)
        expected_occ, expected_term, expected_net, expected_groups = expanded(document)
        result = invoke(raw)
        self.assertNotIn("error", result)
        self.assertEqual(bytes.fromhex(result["source_hex"]), raw)
        paths = [tuple(o["path"]) for o in result["occurrences"]]
        self.assertEqual(paths, sorted(expected_occ))
        for occurrence in result["occurrences"]:
            definition, kind, source = expected_occ[tuple(occurrence["path"])]
            self.assertEqual((occurrence["definition"], occurrence["kind"]), (definition, kind))
            self.assertEqual(json.loads(raw[occurrence["begin"]:occurrence["end"]]), source)
        groups = []
        for group in result["groups"]:
            members, aliases = [], []
            for terminal in group["terminals"]:
                key = (tuple(terminal["path"]), terminal["terminal"]); members.append(key)
                self.assertEqual(json.loads(raw[terminal["begin"]:terminal["end"]]), expected_term[key])
            for net in group["nets"]:
                key = (tuple(net["path"]), net["net"]); aliases.append(key)
                self.assertEqual(json.loads(raw[net["begin"]:net["end"]]), expected_net[key][0])
            groups.append((members, aliases))
        self.assertEqual(groups, expected_groups)
        return groups

    def test_two_rc_groups_and_occurrence_separation(self):
        groups = self.compare(raw=b" \r\n" + RAW + b"\t\n")
        self.assertEqual(len(groups), 4)
        containing = lambda key: next(i for i, (members, _) in enumerate(groups) if key in members)
        self.assertNotEqual(containing((("main", "left", "r"), "n")), containing((("main", "right", "r"), "n")))
        self.assertEqual(containing((("main", "left", "c"), "n")), containing((("main", "right", "c"), "n")))

    def test_empty_root(self):
        self.assertEqual(self.compare(minimal()), [])

    def test_disconnected_terminals_remain_singletons(self):
        document = json.loads(RAW)
        for circuit in document["sources"]["topology"]["circuits"]: circuit["nets"] = []
        groups = self.compare(document)
        self.assertEqual(len(groups), 18)
        self.assertTrue(all(len(members) == 1 and not nets for members, nets in groups))

    def test_labels_gnd_and_directions_do_not_join(self):
        document = json.loads(RAW)
        before = self.compare(document)
        for circuit in document["sources"]["topology"]["circuits"]:
            circuit["name"] = "GND"
            for record in circuit["nets"] + circuit["ports"] + circuit["instances"]: record["name"] = "GND"
            for port in circuit["ports"]: port["direction"] = "output"
        self.assertEqual(self.compare(document), before)

    def test_parameters_do_not_rewire_connectivity(self):
        document = json.loads(RAW); before = self.compare(document)
        document["sources"]["topology"]["circuits"][0]["instances"][0]["overrides"]["resistance"]["value"] = "3.3"
        self.assertEqual(self.compare(document), before)

    def test_source_array_reordering_keeps_canonical_partition(self):
        document = json.loads(RAW); before = self.compare(document)
        topology = document["sources"]["topology"]
        topology["components"].reverse(); topology["circuits"].reverse()
        for circuit in topology["circuits"]:
            for key in ("instances", "ports", "nets"): circuit[key].reverse()
            for net in circuit["nets"]: net["terminals"].reverse()
        self.assertEqual(self.compare(document), before)

    def test_unused_definitions_are_retained_but_not_expanded(self):
        document = json.loads(RAW); before = self.compare(document)
        spare = copy.deepcopy(document["sources"]["topology"]["circuits"][1]); spare["id"] = "unused"
        document["sources"]["topology"]["circuits"].append(spare)
        self.assertEqual(self.compare(document), before)

    def test_nested_hierarchical_port_aliases(self):
        document = json.loads(RAW); topology = document["sources"]["topology"]
        wrapper = copy.deepcopy(topology["circuits"][1]); wrapper["id"] = "wrapper"
        wrapper["instances"] = [dict(id="child", name="Child", kind="circuit", definition="rc",
            overrides={p["id"]: dict(parameter=p["id"]) for p in wrapper["parameters"]})]
        wrapper["nets"] = [dict(id="net_" + p["id"], name="Connection", terminals=[dict(port=p["id"]), dict(instance="child", terminal=p["id"])]) for p in wrapper["ports"]]
        topology["circuits"].append(wrapper); topology["circuits"][0]["instances"][0]["definition"] = "wrapper"
        self.assertEqual(len(self.compare(document)), 4)

    def test_invalid_complete_declarations_rejected(self):
        for raw in (b"{}", b'{"x":1,"x":2}', RAW.replace(b'"unconfigured"', b'"rollback"')):
            result = invoke(raw); self.assertEqual(result["stage"], "declaration")
        document = json.loads(RAW)
        spare = copy.deepcopy(document["sources"]["topology"]["circuits"][1]); spare["id"] = "unused"
        spare["instances"][0]["definition"] = "absent"
        document["sources"]["topology"]["circuits"].append(spare)
        self.assertEqual(invoke(json.dumps(document).encode())["stage"], "declaration")

    def test_exact_expansion_budget_and_one_over(self):
        document = minimal(); topology = document["sources"]["topology"]; root = topology["circuits"][0]
        topology["components"] = [dict(id="part", name="Part", pins=[dict(id=f"p{i}", name="Pin", domain="electrical") for i in range(254)], parameters=[], symbol=None, model=None)]
        root["instances"] = [dict(id=f"i{i}", name="Instance", kind="component", definition="part", overrides={}) for i in range(256)]
        root["ports"] = [dict(id=f"p{i}", name="Port", domain="electrical", direction="bidirectional") for i in range(255)]
        self.assertEqual(len(self.compare(document)), 65279)
        root["ports"].append(dict(id="extra", name="Extra", domain="electrical", direction="bidirectional"))
        raw = json.dumps(document).encode(); reference.parse(raw)
        result = invoke(raw)
        self.assertEqual((result["stage"], result["error"]), ("expansion", "expansion-budget"))
        self.assertLess(result["offset"], len(raw))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(); parser.add_argument("--probe", required=True)
    PROBE = parser.parse_args().probe
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Connectivity))
    print(f"{REQUESTS} connectivity requests checked against independent adjacency/source expectations")
    raise SystemExit(0 if result.wasSuccessful() else 1)
