#!/usr/bin/env python3
"""Check the pinned experiment files without downloading or loading native code."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tools" / "backend-baseline.json"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT / "build" / "deps")
    parser.add_argument("--archives-only", action="store_true")
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--ngspice-only", action="store_true", help="Check only the E-01 ngspice packages and files")
    selection.add_argument("--renode-only", action="store_true", help="Check only the pinned Renode package and files")
    args = parser.parse_args()
    root = args.root.resolve()
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    records = manifest["archives"]
    if not args.archives_only:
        records = records + manifest["files"]
    if args.ngspice_only:
        records = [record for record in records
                   if record["path"].startswith(("downloads/ngspice-", "ngspice/", "ngspice-source/", "ngspice-console/"))]
    if args.renode_only:
        records = [record for record in records if record["path"].startswith(("downloads/renode-", "renode/"))]
    failures = 0
    for record in records:
        path = (root / record["path"]).resolve()
        if not path.is_relative_to(root):
            print(f"FAIL: path escapes dependency directory: {record['path']}")
            failures += 1
            continue
        try:
            digest = hashlib.sha256()
            with path.open("rb") as source:
                while block := source.read(1024 * 1024):
                    digest.update(block)
            if digest.hexdigest() != record["sha256"]:
                print(f"FAIL: checksum mismatch: {record['path']}")
                failures += 1
            else:
                print(f"OK: {record['path']}")
        except OSError:
            print(f"FAIL: missing or unreadable: {record['path']}")
            failures += 1
    print(f"Checked {len(records)} pinned files; {failures} failure(s).")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
