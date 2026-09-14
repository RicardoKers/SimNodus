"""Inert native lock metadata and bounded SHA-256 against preserved Python oracles."""
import argparse
import copy
import hashlib
import json
import random
import subprocess
from types import SimpleNamespace
import unittest

import resource_lock as reference
import test_resource_lock as baseline
import topology as t

PROBE = ""
CASES = 0
HASH_CASES = 0
RUNTIME_DIFFERENCES = 0


def native(raw):
    global CASES
    CASES += 1
    run = subprocess.run([PROBE], input=raw, capture_output=True, timeout=15)
    result = json.loads(run.stdout)
    assert run.returncode == (1 if "error" in result else 0), run.stderr
    if "error" in result:
        assert 0 <= result["offset"] <= len(raw)
    return result


def parse(raw):
    global RUNTIME_DIFFERENCES
    actual = native(raw)
    try:
        document = reference.parse(raw)
    except t.Invalid as error:
        if raw == b"1" * 5000 and error.code == "input" and actual.get("error") == "shape":
            # Python's host integer-digit cap rejects before its schema phase.
            RUNTIME_DIFFERENCES += 1
        else:
            assert actual.get("error") == error.code, (actual, error.code)
        raise
    requests = [dict(dependency=d["id"], resource=f["id"], **{k:f[k] for k in ("path", "bytes", "sha256")})
                for d in document["dependencies"] for f in d["files"]]
    expected = dict(stats=reference.validate(document), requests=requests)
    assert actual == expected, (actual, expected)
    return document


def validate(document):
    parse(json.dumps(document).encode())
    return reference.validate(document)


def one_file(path="file", size=0):
    files = [dict(id="f", path=path, bytes=size, sha256="0" * 64)]
    return dict(format="simnodus-resource-lock", version="0.1", dependencies=[dict(id="dep", version="1.0.0",
        origin=dict(kind="owned", author="Owner", reference="inert", revision="fixed"),
        license=dict(identifier="MIT", notice="f"), files=files, content_sha256=reference.content_hash(files))])


def portable_path(value, loc):
    actual = native(json.dumps(one_file(value)).encode())
    try:
        expected = reference.portable_path(value, loc)
    except t.Invalid as error:
        assert actual.get("error") == error.code
        raise
    assert "error" not in actual
    return expected


class NativeLock(baseline.ResourceLockTests):
    def test_exact_budgets_and_integer_spellings(self):
        self.document = one_file()
        dependency = self.document["dependencies"][0]
        dependency["files"] = [dict(id=f"f{i}", path=f"d/f{i}", bytes=0, sha256="0" * 64) for i in range(256)]
        dependency["license"]["notice"] = "f0"
        self.seal()
        self.assertEqual(validate(self.document)["resources"], 256)
        dependency["files"].append(dict(id="extra", path="extra", bytes=0, sha256="0" * 64))
        self.invalid("budget")
        self.document = one_file()
        dependency = self.document["dependencies"][0]
        dependency["files"] = [dict(id=f"f{i}", path=f"f{i}", bytes=reference.MAX_FILE_BYTES, sha256="0" * 64) for i in range(4)]
        dependency["license"]["notice"] = "f0"
        self.seal()
        self.assertEqual(validate(self.document)["declared_bytes"], reference.MAX_TOTAL_BYTES)
        dependency["files"].append(dict(id="extra", path="extra", bytes=1, sha256="0" * 64))
        self.invalid("budget")
        self.document = one_file()
        dependency = self.document["dependencies"][0]
        self.document["dependencies"] = [copy.deepcopy(dependency) for _ in range(32)]
        for i, d in enumerate(self.document["dependencies"]):
            d["id"] = f"d{i}"
            d["files"][0]["path"] = f"f{i}"
        self.seal()
        self.assertEqual(validate(self.document)["dependencies"], 32)
        self.document["dependencies"].append({})
        self.invalid("budget")
        raw = json.dumps(one_file()).encode()
        parse(raw.replace(b'"bytes": 0', b'"bytes": -0'))
        for value in (b'0.0', b'0e0', b'-1', b'18446744073709551616', b'9' * 100):
            with self.assertRaises(t.Invalid):
                parse(raw.replace(b'"bytes": 0', b'"bytes": ' + value))

    def test_canonical_order_ids_and_metadata_independence(self):
        self.document = one_file()
        d = self.document["dependencies"][0]
        d["files"] = [dict(id=f"f{i}", path=path, bytes=i, sha256=hashlib.sha256(path.encode()).hexdigest())
                      for i, path in enumerate(("a", "Z", "z-1", "A.x", "_", "-"))]
        d["license"]["notice"] = "f0"
        self.seal()
        expected = d["content_sha256"]
        validate(self.document)
        d["files"].reverse()
        d["origin"].update(kind="external", reference="https://invalid.example/no-download")
        d["version"] = "000000.999999.000001"
        d["files"][-1]["id"] = "notice"
        d["license"]["notice"] = "notice"
        self.assertEqual(reference.content_hash(d["files"]), expected)
        validate(self.document)

    def test_metadata_and_path_boundaries(self):
        for path in ("a" * 80 + "/" + "b" * 80 + "/" + "c" * 78, "/".join(["a"] * 16)):
            portable_path(path, "$")
        for path in ("a" * 80 + "/" + "b" * 80 + "/" + "c" * 79, "\\\\?\\C:\\file", "file:stream", "CONIN$", "NUL..txt"):
            with self.assertRaises(t.Invalid):
                portable_path(path, "$")
        for key in ("author", "reference", "revision"):
            self.dependency["origin"][key] = "A" * 512
            validate(self.document)
            for value in ("A" * 513, " leading", "trailing ", "a\u007fb", "a\nb", [], None, 1):
                self.dependency["origin"][key] = value
                self.invalid("value")
            self.dependency["origin"][key] = "Valid"

    def test_hash_padding_binary_and_maximum(self):
        global HASH_CASES
        rng = random.Random(21059)
        for size in list(range(130)) + [255, 256, 257, 1000, 65536, t.MAX_BYTES]:
            raw = rng.randbytes(size)
            run = subprocess.run([PROBE, "--sha"], input=raw, capture_output=True, timeout=15)
            self.assertEqual(run.returncode, 0)
            self.assertEqual(run.stdout.strip().decode(), hashlib.sha256(raw).hexdigest())
            HASH_CASES += 1
        run = subprocess.run([PROBE, "--sha"], input=b"a" * (t.MAX_BYTES + 1), capture_output=True, timeout=15)
        self.assertEqual(run.returncode, 1)
        HASH_CASES += 1

    def test_nested_unknown_fields_and_empty_arrays(self):
        original = copy.deepcopy(self.document)
        paths = [[], ["dependencies", 0], ["dependencies", 0, "origin"], ["dependencies", 0, "license"], ["dependencies", 0, "files", 0]]
        for path in paths:
            self.document = copy.deepcopy(original)
            target = self.document
            for part in path:
                target = target[part]
            target["execute"] = "untrusted"
            self.invalid("shape")
        self.document = copy.deepcopy(original)
        self.document["dependencies"][0]["files"] = []
        self.invalid("shape")
        self.document["dependencies"] = []
        self.invalid("shape")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    PROBE = parser.parse_args().probe
    baseline.r = SimpleNamespace(parse=parse, validate=validate, content_hash=reference.content_hash,
        portable_path=portable_path, MAX_FILES=reference.MAX_FILES, MAX_FILE_BYTES=reference.MAX_FILE_BYTES)
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(NativeLock))
    print(f"{CASES} native/reference lock comparisons; {HASH_CASES} digest cases; {RUNTIME_DIFFERENCES} Python integer-cap diagnostic differences")
    raise SystemExit(0 if result.wasSuccessful() else 1)
