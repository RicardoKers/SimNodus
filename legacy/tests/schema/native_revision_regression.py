# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Exact source revision against the unchanged Python project/graph reference."""
import argparse
import copy
import json
from pathlib import Path
import struct
import subprocess
import unittest

import project as reference
from native_graph_regression import expected_graph, expected_positions

PROBE = ""
COMPARISONS = 0
RAW = (Path(__file__).parent / "fixtures/two-rc-project.json").read_bytes()


def name_span(raw):
    """Independent JSON decoder locates the top-level value in character space."""
    text = raw.decode()
    decoder = json.JSONDecoder()
    position = text.index("{") + 1
    while True:
        while text[position].isspace() or text[position] == ',': position += 1
        key, position = decoder.raw_decode(text, position)
        while text[position].isspace(): position += 1
        assert text[position] == ':'
        position += 1
        while text[position].isspace(): position += 1
        begin = position
        _, position = decoder.raw_decode(text, position)
        if key == "name": return len(text[:begin].encode()), len(text[:position].encode())


def invoke(raw, name):
    global COMPARISONS
    COMPARISONS += 1
    if isinstance(name, str): name = name.encode()
    packet = struct.pack('<I', len(raw)) + raw + struct.pack('<I', len(name)) + name
    run = subprocess.run([PROBE], input=packet, capture_output=True, timeout=20)
    value = json.loads(run.stdout)
    assert run.returncode == (1 if "error" in value else 0), run.stderr
    return value


class Revision(unittest.TestCase):
    def accepted(self, raw, name):
        before = reference.parse(raw)
        expected = copy.deepcopy(before)
        expected["name"] = name
        reference.validate(expected)
        result = invoke(raw, name)
        self.assertNotIn("error", result)
        actual = bytes.fromhex(result["source_hex"])
        self.assertEqual(reference.parse(actual), expected)
        begin, end = name_span(raw)
        replacement = json.dumps(name, ensure_ascii=False).encode()
        wanted = raw if before["name"] == name else raw[:begin] + replacement + raw[end:]
        self.assertEqual(actual, wanted)
        topology = expected["sources"]["topology"]
        self.assertEqual(result["graph"], expected_graph(topology))
        positions = expected_positions(topology)
        self.assertEqual({tuple(p["identity"]) for p in result["positions"]}, set(positions))
        self.assertEqual(len(result["positions"]), len(positions))
        for position in result["positions"]:
            span = actual[position["begin"]:position["end"]]
            self.assertEqual(json.loads(span), positions[tuple(position["identity"])])
        return actual

    def test_simple_rename_preserves_other_bytes_and_ids(self):
        self.accepted(b" \r\n" + RAW + b"\t\n", "New lesson")

    def test_quotes_backslashes_unicode_and_injection_text(self):
        for name in ('A "quoted" name \\ path', 'caf\u00e9 \U0001f600 \u03a9', '"},"sources":{},"name":"'):
            with self.subTest(name=name): self.accepted(RAW, name)

    def test_eighty_unicode_scalars(self):
        for name in ("x" * 80, "\U0001f600" * 80): self.accepted(RAW, name)

    def test_semantic_noop_preserves_original_escape_spelling(self):
        value = json.loads(RAW); value["name"] = "caf\u00e9"
        raw = json.dumps(value, ensure_ascii=True).encode().replace(b'"name": "caf', b'"na\\u006de": "caf', 1)
        self.accepted(raw, value["name"])

    def test_escaped_key_selects_only_top_level_name(self):
        raw = RAW.replace(b'"name"', b'"na\\u006de"', 1)
        self.accepted(raw, "changed")

    def test_reordered_top_level_name_after_graph(self):
        value = json.loads(RAW); name = value.pop("name"); value["name"] = name
        self.accepted(json.dumps(value, indent=3).encode(), "longer renamed project")

    def test_repeated_revisions_keep_complete_graph_provenance(self):
        raw = RAW
        for name in ("a", "a longer revision", "\u03a9", "\u03a9", json.loads(RAW)["name"]):
            raw = self.accepted(raw, name)

    def test_invalid_names_leave_base_unchanged(self):
        for name in ("", "x" * 81, "\U0001f600" * 81, "line\nfeed", "nul\0", "\x7f", "\x85", b'\xff', b'\xed\xa0\x80'):
            with self.subTest(name=name):
                result = invoke(RAW, name)
                self.assertEqual(result["stage"], "revision")
                self.assertNotIn("source_hex", result)
                self.assertLessEqual(result["offset"], len(RAW))
        self.accepted(RAW, json.loads(RAW)["name"])

    def test_invalid_base_is_not_repaired(self):
        for raw in (b"", b"{}", b'{"name":"x","name":"y"}', RAW.replace(b'"unconfigured"', b'"rollback"'), b" " * (1024 * 1024 + 1)):
            with self.subTest(size=len(raw)):
                result = invoke(raw, "valid name")
                self.assertEqual(result["stage"], "base")
                self.assertNotIn("source_hex", result)

    def test_byte_ceiling_noop_shrink_and_growth(self):
        raw = RAW + b" " * (1024 * 1024 - len(RAW))
        self.accepted(raw, json.loads(RAW)["name"])
        self.accepted(raw, "x")
        result = invoke(raw, "x" * 80)
        self.assertEqual((result["stage"], result["error"]), ("revision", "bytes"))

    def test_escaped_original_can_shrink_with_longer_decoded_name(self):
        begin, end = name_span(RAW)
        raw = RAW[:begin] + b'"' + b'\\u0078' * 70 + b'"' + RAW[end:]
        raw += b" " * (1024 * 1024 - len(raw))
        self.accepted(raw, "y" * 80)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(); parser.add_argument("--probe", required=True)
    PROBE = parser.parse_args().probe
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Revision))
    print(f"{COMPARISONS} revision requests checked against independent source/graph expectations")
    raise SystemExit(0 if result.wasSuccessful() else 1)
