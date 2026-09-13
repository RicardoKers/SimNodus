"""Untrusted lock metadata, portable path rejection and explicit trust limits."""
import copy
import hashlib
import json
from pathlib import Path
import unittest
from unittest.mock import patch

import resource_lock as r
import topology as t

FIXTURES = Path(__file__).with_name("fixtures")


class ResourceLockTests(unittest.TestCase):
    def setUp(self):
        self.document = json.loads((FIXTURES / "owned-resource-lock.json").read_bytes())
        self.dependency = self.document["dependencies"][0]
        self.files = self.dependency["files"]

    def seal(self):
        for dependency in self.document["dependencies"]:
            dependency["content_sha256"] = r.content_hash(dependency["files"])

    def invalid(self, code):
        with self.assertRaises(t.Invalid) as result:
            r.parse(json.dumps(self.document).encode())
        self.assertEqual(result.exception.code, code)

    def test_round_trip_and_no_mutation(self):
        original = copy.deepcopy(self.document)
        self.assertEqual(r.parse(json.dumps(self.document).encode()), original)
        stats = r.validate(self.document)
        for key in ("resources_verified", "containment_verified", "redistribution_verified", "simulation_ready"):
            self.assertIs(stats[key], False)
        self.assertEqual(stats["resources"], 2)
        self.assertEqual(self.document, original)

    def test_owned_fixture_bytes_match_declared_hashes(self):
        root = Path(__file__).resolve().parents[2]
        for file in self.files:
            raw = (root / file["path"]).read_bytes()
            self.assertEqual(len(raw), file["bytes"])
            self.assertEqual(hashlib.sha256(raw).hexdigest(), file["sha256"])

    def test_parse_has_no_resource_io(self):
        raw = json.dumps(self.document).encode()
        with patch("builtins.open", side_effect=AssertionError("Unexpected file access")), \
                patch.object(Path, "open", side_effect=AssertionError("Unexpected path access")), \
                patch.object(Path, "resolve", side_effect=AssertionError("Unexpected resolution")):
            r.parse(raw)

    def test_absolute_traversal_and_encoded_paths(self):
        for path in ("../x", "a/../x", "./x", "/x", "C:/x", "C:x", "//host/share", "a\\b",
                     "https://host/file", "a/%2e%2e/x", "a//b", "a/", "a:b", "a\x00b"):
            with self.subTest(path=path):
                self.files[0]["path"] = path
                self.invalid("path")

    def test_windows_device_and_ambiguous_names(self):
        for path in ("CON", "a/NUL.txt", "com1.json", "LPT9", "aux", "file.", "file ",
                     ".hidden", "a/PRN.log", "COM\u00b9", "caf\u00e9", "a~1.txt", "a?b"):
            with self.subTest(path=path):
                self.files[0]["path"] = path
                self.invalid("path")

    def test_portable_boundary_paths(self):
        for path in ("a" * 80, "a/COM10.txt", "a/NUL-safe.txt", "/".join(["a"] * 16)):
            r.portable_path(path, "$")
        for path in ("a" * 81, "/".join(["a"] * 17), "/".join(["a" * 80] * 3)):
            with self.assertRaises(t.Invalid):
                r.portable_path(path, "$")

    def test_case_collision_and_file_parent_collision(self):
        self.files[0]["path"] = "license"
        self.invalid("path")
        self.files[0]["path"] = "LICENSE/child"
        self.seal()
        self.invalid("path")

    def test_cross_dependency_path_collision(self):
        other = copy.deepcopy(self.dependency)
        other["id"] = "other"
        self.document["dependencies"].append(other)
        self.invalid("path")

    def test_missing_notice_and_duplicate_ids(self):
        self.dependency["license"]["notice"] = "missing"
        self.invalid("reference")
        self.dependency["license"]["notice"] = "license"
        self.files[0]["id"] = "license"
        self.invalid("id")

    def test_version_ranges_and_executable_fields_rejected(self):
        for version in ("latest", "*", "^1.0.0", "1.0", [], 1):
            self.dependency["version"] = version
            self.invalid("version")
        self.dependency["version"] = "0.3.0"
        for key in ("script", "download", "dll", "install", "dependencies"):
            self.dependency[key] = "untrusted"
            self.invalid("shape")
            del self.dependency[key]

    def test_hash_format_and_inventory_tampering(self):
        saved = self.files[0]["sha256"]
        for value in ("A" * 64, "0" * 63, [], None):
            self.files[0]["sha256"] = value
            self.invalid("hash")
        self.files[0]["sha256"] = saved
        self.files[0]["bytes"] += 1
        self.invalid("hash")

    def test_inventory_digest_is_order_independent_but_path_sensitive(self):
        expected = self.dependency["content_sha256"]
        self.files.reverse()
        self.assertEqual(r.content_hash(self.files), expected)
        r.validate(self.document)
        self.files[0]["path"] = "renamed"
        self.invalid("hash")

    def test_well_formed_forged_hash_is_not_verified(self):
        self.files[0]["sha256"] = "0" * 64
        self.seal()
        self.assertFalse(r.validate(self.document)["resources_verified"])

    def test_sizes_and_total_budget(self):
        for size in (-1, True, 1.0, "1", r.MAX_FILE_BYTES + 1):
            self.files[0]["bytes"] = size
            self.invalid("budget")
        self.dependency["files"] = [dict(id=f"f{i}", path=f"f{i}", bytes=r.MAX_FILE_BYTES, sha256="0" * 64)
                                    for i in range(5)]
        self.invalid("budget")

    def test_count_budgets(self):
        self.dependency["files"] = [{}] * (r.MAX_FILES + 1)
        self.invalid("budget")
        self.document["dependencies"] = [{}] * 33
        self.invalid("budget")

    def test_origin_and_license_are_required_declarations(self):
        self.dependency["origin"]["author"] = ""
        self.invalid("value")
        self.dependency["origin"]["author"] = "Declared author"
        self.dependency["origin"]["kind"] = "external"
        self.assertFalse(r.validate(self.document)["redistribution_verified"])
        del self.dependency["license"]
        self.invalid("shape")

    def test_malformed_json_and_unknown_version(self):
        for raw in (b'{"version":1,"version":2}', b'{"a":NaN}', b'\xff', b'[' * 40 + b']' * 40,
                    b' ' * (t.MAX_BYTES + 1), b'1' * 5000):
            with self.subTest(raw=raw[:20]), self.assertRaises(t.Invalid):
                r.parse(raw)
        self.document["version"] = "0.2"
        self.invalid("version")

    def test_invalid_path_fixture(self):
        with self.assertRaises(t.Invalid) as result:
            r.parse((FIXTURES / "invalid-resource-path.json").read_bytes())
        self.assertEqual(result.exception.code, "path")


if __name__ == "__main__":
    unittest.main()
