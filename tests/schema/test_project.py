"""Project identities, declared targets and fail-closed temporal policies."""
import copy
import json
from pathlib import Path
import unittest
from unittest.mock import patch

import project as project
import resource_lock as lock
import topology as t

FIXTURES = Path(__file__).with_name("fixtures")


class ProjectTests(unittest.TestCase):
    def setUp(self):
        self.document = json.loads((FIXTURES / "two-rc-project.json").read_bytes())

    def invalid(self, code):
        with self.assertRaises(t.Invalid) as result:
            project.parse(json.dumps(self.document).encode())
        self.assertEqual(result.exception.code, code)

    def ref(self, rid):
        return dict(dependency="owned-link-fixture", resource=rid)

    def synthetic_target(self):
        """Synthetic declarations only, not files or fake-backend acceptance."""
        dep = self.document["sources"]["lock"]["dependencies"][0]
        for rid in ("mcu-profile", "board-profile", "image", "schedule"):
            dep["files"].append(dict(id=rid, path=f"synthetic/{rid}.data", bytes=0, sha256="0" * 64))
        dep["content_sha256"] = lock.content_hash(dep["files"])
        self.document["platforms"] = [dict(id="test-platform", name="Synthetic interface", mcu=dict(
            device="test-device", architecture="test-isa", resource=self.ref("mcu-profile")),
            board=dict(id="test-board", name="Synthetic board", resource=self.ref("board-profile")),
            pins=[dict(id="a", name="A"), dict(id="b", name="B")])]
        self.document["firmware"] = [dict(id="test-image", name="Synthetic image declaration",
            architecture="test-isa", image_format="elf", resource=self.ref("image"))]
        self.document["sources"]["topology"]["components"][0]["model"] = None
        self.document["targets"] = [dict(path=["main", "left", "r"], platform="test-platform",
                                        firmware="test-image", pin_map={"p": "a", "n": "b"})]

    def replay(self):
        self.synthetic_target()
        self.document["temporal"].update(mode="known-schedule-replay", fidelity="causal-replay",
            duration_ns=100000, exchange_quantum_ns=1000, schedule=self.ref("schedule"))

    def test_round_trip_preserves_sources_and_identity(self):
        before = copy.deepcopy(self.document)
        restored = project.parse(json.dumps(before, sort_keys=True).encode())
        self.assertEqual(restored, before)
        self.assertEqual(before, self.document)
        self.assertEqual(project.validate(restored)["project_id"], "two-rc-project")
        self.assertEqual(restored["sources"], json.loads((FIXTURES / "two-rc-resource-links.json").read_bytes()))

    def test_distinct_target_occurrences_and_declared_architecture(self):
        self.synthetic_target()
        other = copy.deepcopy(self.document["targets"][0])
        other["path"] = ["main", "right", "r"]
        self.document["targets"].append(other)
        stats = project.validate(self.document)
        self.assertEqual(stats["targets"], 2)
        self.assertFalse(stats["firmware_verified"])
        self.assertFalse(stats["runtime_profile_verified"])
        self.assertFalse(stats["simulation_ready"])
        self.document["firmware"][0]["architecture"] = "another-isa"
        self.invalid("mapping")

    def test_board_is_distinct_and_optional(self):
        self.synthetic_target()
        self.document["platforms"][0]["board"] = None
        project.validate(self.document)
        del self.document["platforms"][0]["board"]
        self.invalid("shape")

    def test_target_missing_circuit_and_duplicate_paths(self):
        self.synthetic_target()
        target = self.document["targets"][0]
        for path in (["main", "missing"], ["main", "left"]):
            target["path"] = path
            self.invalid("reference")
        target["path"] = ["main", "left", "r"]
        self.document["targets"].append(copy.deepcopy(target))
        self.invalid("reference")

    def test_electrical_model_and_mcu_ownership_conflict(self):
        model = copy.deepcopy(self.document["sources"]["topology"]["components"][0]["model"])
        self.synthetic_target()
        self.document["sources"]["topology"]["components"][0]["model"] = model
        self.invalid("mapping")

    def test_missing_firmware_and_wrong_image_format(self):
        self.synthetic_target()
        self.document["firmware"][0]["resource"] = self.ref("absent")
        self.invalid("reference")
        self.document["firmware"][0]["resource"] = self.ref("image")
        self.document["firmware"][0]["image_format"] = "script"
        self.invalid("kind")

    def test_pin_map_complete_and_unaliased(self):
        self.synthetic_target()
        self.document["targets"][0]["pin_map"] = {"p": "a", "n": "a"}
        self.invalid("mapping")
        self.document["targets"][0]["pin_map"] = {"p": "a"}
        self.invalid("mapping")

    def test_replay_schedule_required_and_no_capability_inference(self):
        self.replay()
        self.assertFalse(project.validate(self.document)["runtime_profile_verified"])
        self.document["temporal"]["schedule"] = None
        self.invalid("shape")

    def test_sampled_fidelity_is_explicit(self):
        self.replay()
        self.document["temporal"].update(mode="approximate-sampled", fidelity="approximate", schedule=None)
        project.validate(self.document)
        self.document["temporal"]["fidelity"] = "causal-replay"
        self.invalid("capability")

    def test_virtual_time_range_grid_and_wall_time_separation(self):
        self.replay()
        for value in (True, -1, 0, 1.0, 1001, 2**64, "1000"):
            self.document["temporal"]["exchange_quantum_ns"] = value
            self.invalid("range")
        self.document["temporal"]["exchange_quantum_ns"] = 101000
        self.invalid("range")

    def test_tolerances_and_unsupported_modes_fail_closed(self):
        for key in ("voltage_tolerance_uv", "analog_time_tolerance_ps", "pause_wall_timeout_ms"):
            saved = self.document["temporal"][key]
            self.document["temporal"][key] = saved + 1
            self.invalid("range")
            self.document["temporal"][key] = saved
        for mode in ("rollback", "live-causal-feedback", "real-time", [], None):
            self.document["temporal"]["mode"] = mode
            self.invalid("capability")

    def test_unconfigured_policy_and_unknown_fields(self):
        self.document["temporal"]["duration_ns"] = 1000
        self.invalid("shape")
        self.document["temporal"]["duration_ns"] = None
        self.document["on_open"] = "compile"
        self.invalid("shape")

    def test_no_resource_io_during_parse(self):
        self.replay()
        raw = json.dumps(self.document).encode()
        with patch("builtins.open", side_effect=AssertionError("Unexpected I/O")), \
                patch.object(Path, "open", side_effect=AssertionError("Unexpected I/O")), \
                patch.object(Path, "resolve", side_effect=AssertionError("Unexpected traversal")):
            project.parse(raw)

    def test_budget_duplicate_and_unused_declarations(self):
        self.synthetic_target()
        self.document["platforms"].append(copy.deepcopy(self.document["platforms"][0]))
        self.invalid("id")
        self.document["platforms"].pop()
        self.document["targets"] = []
        self.document["platforms"][0]["mcu"]["resource"] = self.ref("missing")
        self.invalid("reference")
        self.document["platforms"] = []
        self.document["targets"] = [{}] * 257
        self.invalid("budget")

    def test_bad_input_and_version(self):
        for raw in (b'{"a":1,"a":2}', b'NaN', b'\xff', b'[' * 40 + b']' * 40, b' ' * (t.MAX_BYTES + 1)):
            with self.assertRaises(t.Invalid):
                project.parse(raw)
        self.document["version"] = "1.0"
        self.invalid("version")


if __name__ == "__main__":
    unittest.main()
