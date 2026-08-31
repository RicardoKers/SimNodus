"""Verify pinned read-only model sources; download only when explicitly requested."""
from __future__ import annotations
import argparse
import hashlib
import json
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=HERE.parents[2] / 'build/sn012/audit')
    parser.add_argument('--download', action='store_true')
    args = parser.parse_args()
    args.root.mkdir(parents=True, exist_ok=True)
    entries = json.loads((HERE / 'audit-sources.json').read_text(encoding='utf-8'))['files']
    for entry in entries:
        path = args.root / entry['file']
        if args.download:
            with urllib.request.urlopen(entry['url'], timeout=30) as response:
                data = response.read(1024 * 1024)
        else:
            data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != entry['sha256']:
            raise ValueError(f'Source fingerprint mismatch: {path.name}')
        if args.download:
            path.write_bytes(data)
    print(f'Verified {len(entries)} read-only audit sources. No source was compiled or server started.')


if __name__ == '__main__':
    main()
