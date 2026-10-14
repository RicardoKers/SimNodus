"""Native typed resource links against the unchanged Python reference."""
import argparse
import copy
import json
import subprocess
from types import SimpleNamespace
import unittest

import resource_links as reference
import resource_lock as lock
import test_resource_links as baseline
import topology as t

PROBE = ""
CASES = 0


def parse(raw):
    global CASES
    CASES += 1
    run = subprocess.run([PROBE], input=raw, capture_output=True, timeout=15)
    actual = json.loads(run.stdout)
    assert run.returncode == (1 if "error" in actual else 0), run.stderr
    try:
        document = reference.parse(raw)
    except t.Invalid as error:
        assert actual.get("error") == error.code, (actual, error.code)
        assert 0 <= actual["offset"] <= len(raw)
        raise
    expected = reference.validate(document)
    assert actual == expected, (actual, expected)
    return document


def validate(document):
    parse(json.dumps(document).encode())
    return reference.validate(document)


class NativeLinks(baseline.ResourceLinkTests):
    def test_exact_shared_budget_with_null_slots(self):
        d = self.document
        stats = validate(d)["topology_stats"]
        used = stats["declared_entities"] + stats["parameter_entries"] + stats["descriptor_entries"] + len(d["assets"]) + len(d["symbols"]) + len(d["models"])
        used += sum(1 + len(m["terminal_map"]) + len(m["parameter_map"]) for m in d["models"].values() if m is not None)
        count = (4096 - used) // 2
        for i in range(count):
            name = f"unused{i}"
            d["topology"]["symbols"].append(dict(id=name, name="Unused", pins=[]))
            d["symbols"][name] = None
        if (4096 - used) % 2:
            d["topology"]["symbols"][-1]["pins"].append(dict(id="p", name="P"))
        validate(d)
        d["topology"]["symbols"][-1]["pins"].append(dict(id="overflow", name="Overflow"))
        self.invalid("budget")

    def test_exact_asset_limit_and_unreferenced_roles(self):
        d = self.document
        dependency = d["lock"]["dependencies"][0]
        for i in range(253):
            fid = f"unused{i}"
            dependency["files"].append(dict(id=fid, path=f"unused/{fid}", bytes=0, sha256="0" * 64))
            d["assets"].append(dict(id=fid, kind="model-spice", dependency=dependency["id"], resource=fid))
        notice = dependency["license"]["notice"]
        d["assets"].append(dict(id="unused-notice", kind="symbol-svg", dependency=dependency["id"], resource=notice))
        dependency["content_sha256"] = lock.content_hash(dependency["files"])
        self.assertEqual(validate(d)["assets"], 256)
        d["assets"].append({})
        self.invalid("budget")

    def test_source_token_bounds_and_independent_namespaces(self):
        m = self.document["models"]["resistor-interface"]
        for value in ("_", "A" * 64, "_0"):
            m["entrypoint"] = value
            validate(self.document)
        for value in ("A" * 65, "0start", "with-hyphen", "café", "a.b"):
            m["entrypoint"] = value
            self.invalid("mapping")
        m["entrypoint"] = "UnknownButDeclared"
        m["terminal_map"] = {k:v for k,v in zip(m["terminal_map"], ("Shared", "Other"))}
        m["parameter_map"] = {k:"shared" for k in m["parameter_map"]}
        self.assertFalse(validate(self.document)["resource_interfaces_verified"])

    def test_all_nulls_empty_assets_and_unknown_nested_fields(self):
        for key in ("symbols", "models"):
            self.document[key] = {k:None for k in self.document[key]}
        self.document["assets"] = []
        stats = validate(self.document)
        self.assertEqual(stats["descriptors_linked"], 0)
        original = copy.deepcopy(self.document)
        for path in ([], ["topology"], ["lock"], ["topology","components",0], ["lock","dependencies",0,"origin"]):
            self.document = copy.deepcopy(original)
            target = self.document
            for part in path:
                target = target[part]
            target["execute"] = True
            self.invalid("shape")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    PROBE = parser.parse_args().probe
    baseline.l = SimpleNamespace(parse=parse, validate=validate)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(NativeLinks))
    print(f"{CASES} native/reference resource-link comparisons")
    raise SystemExit(0 if result.wasSuccessful() else 1)
