"""Native project declarations against the unchanged Python reference."""
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


class NativeProject(baseline.ProjectTests):
    def test_exact_project_budget_including_targets_boards_and_maps(self):
        self.synthetic_target()
        stats = validate(self.document)
        for i in range(256 - stats["project_entries"]):
            image = copy.deepcopy(self.document["firmware"][0])
            image["id"] = f"unused{i}"
            self.document["firmware"].append(image)
        self.assertEqual(validate(self.document)["project_entries"], 256)
        other = copy.deepcopy(self.document["targets"][0])
        other["path"] = ["main", "right", "r"]
        self.document["targets"].append(other)
        self.invalid("budget")

    def test_uint64_grid_boundaries_and_exact_integer_tokens(self):
        self.replay()
        policy = self.document["temporal"]
        maximum = ((2**64 - 1) // 1000) * 1000
        policy.update(duration_ns=maximum, exchange_quantum_ns=maximum)
        validate(self.document)
        for value in (maximum + 1000, 2**64 - 1, 2**64, -(2**64), 1e100, {}, []):
            policy["duration_ns"] = value
            self.invalid("range")
        policy.update(duration_ns=1000, exchange_quantum_ns=1000)
        raw = json.dumps(self.document).encode()
        for spelling in (b'1e3', b'1000.0', b'-0', b'18446744073709552000'):
            with self.assertRaises(t.Invalid):
                parse(raw.replace(b'"duration_ns": 1000', b'"duration_ns": ' + spelling))
        for key, good in (("pause_wall_timeout_ms", 2000), ("voltage_tolerance_uv", 10), ("analog_time_tolerance_ps", 1)):
            for bad in (True, float(good), str(good), None):
                policy[key] = bad
                self.invalid("range")
            policy[key] = good

    def test_closed_fields_catalog_scopes_and_unused_records(self):
        self.synthetic_target()
        saved = copy.deepcopy(self.document)
        for path in ([], ["platforms",0], ["platforms",0,"mcu"], ["platforms",0,"board"], ["platforms",0,"pins",0], ["firmware",0], ["targets",0], ["temporal"], ["sources"], ["sources","lock"]):
            self.document = copy.deepcopy(saved)
            value = self.document
            for part in path:
                value = value[part]
            value["execute"] = True
            self.invalid("shape")
        self.document = copy.deepcopy(saved)
        self.document["targets"] = []
        self.document["firmware"][0]["architecture"] = ""
        self.invalid("id")
        self.document = copy.deepcopy(saved)
        self.document["platforms"][0]["board"]["id"] = "test-platform"
        self.document["firmware"][0]["id"] = "test-platform"
        self.document["targets"][0]["firmware"] = "test-platform"
        validate(self.document)

    def test_names_identity_and_target_path_limits(self):
        for name in ("a" * 80, "\U0001f642" * 80, "\u00a0", "e\u0301"):
            self.document["name"] = name
            validate(self.document)
        for name in ("", "a" * 81, "\U0001f642" * 81, "\x00", "\x7f", "\x9f", 4):
            self.document["name"] = name
            self.invalid("value")
        self.document["name"] = "Project"
        self.synthetic_target()
        for path, code in ((["main"], "shape"), (["main"] * 10, "budget"), (["main", "invalid/path"], "id")):
            self.document["targets"][0]["path"] = path
            self.invalid(code)

    def test_policy_modes_never_grant_capabilities(self):
        self.replay()
        policy = self.document["temporal"]
        for mode, fidelity, schedule in (("known-schedule-replay", "causal-replay", self.ref("schedule")), ("approximate-sampled", "approximate", None)):
            policy.update(mode=mode, fidelity=fidelity, schedule=schedule)
            for debug in ("disabled", "bounded-cooperative"):
                policy["debug"] = debug
                stats = validate(self.document)
                for flag in ("firmware_verified", "runtime_profile_verified", "simulation_ready", "resources_verified", "resource_interfaces_verified"):
                    self.assertFalse(stats[flag])
            policy["debug"] = "raw-gdb"
            self.invalid("capability")
        policy["debug"] = "disabled"
        policy["on_unsupported"] = "fallback"
        self.invalid("capability")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    PROBE = parser.parse_args().probe
    baseline.project = SimpleNamespace(parse=parse, validate=validate)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(NativeProject))
    print(f"{CASES} native/reference project comparisons")
    raise SystemExit(0 if result.wasSuccessful() else 1)
