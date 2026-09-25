"""Replay selected inert VM record evidence without guest access or execution."""

import base64
import hashlib
import json
from pathlib import Path
from tempfile import TemporaryDirectory

from managed_record_correspondence import inspect


ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / "docs/experiments/evidence"
ARCHIVE = EVIDENCE / "SN-021-managed-record-correspondence-bytes.json"
REPORT = EVIDENCE / "SN-021-managed-store-attempt9-probe.json"
RESULT = EVIDENCE / "SN-021-managed-record-correspondence-result.json"


def replay():
    package = json.loads(ARCHIVE.read_text(encoding="utf-8"))
    expected = json.loads(RESULT.read_text(encoding="utf-8"))
    assert hashlib.sha256(REPORT.read_bytes()).hexdigest() == package["published_report_sha256"]
    with TemporaryDirectory(prefix="sn021-record-correspondence-") as directory:
        destination = Path(directory)
        (destination / "collection.json").write_text(json.dumps(package["collection"]), encoding="utf-8")
        assert set(package["files"]) == set(package["collection"]["files"])
        for name, encoded in package["files"].items():
            assert name in {"generation.manifest", "project.json", "00000001.commit",
                            "00000002.commit", "00000003.commit", "0000003f.commit", "00000040.commit"}
            (destination / name).write_bytes(base64.b64decode(encoded, validate=True))
        actual = inspect(destination, REPORT)
    assert actual == expected
    return actual


if __name__ == "__main__":
    print(json.dumps(replay(), indent=2))
