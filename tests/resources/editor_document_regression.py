"""Independent byte oracle for the inert editor application lifecycle."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile


def verify(original: bytes, copy: bytes) -> None:
    before, after = json.loads(original), json.loads(copy)
    assert after['name'] == 'Edited "RC" \\ \u03a9'
    after['name'] = before['name']
    assert before == after, 'Only display name may change'
    # Independent lexical oracle: this owned fixture has the top-level name
    # before sources. Compare exact prefix/suffix, not reserialized JSON.
    span = re.search(rb'"name":\s*("(?:[^"\\]|\\.)*")', original).span(1)
    changed = re.search(rb'"name":\s*("(?:[^"\\]|\\.)*")', copy).span(1)
    assert original[:span[0]] == copy[:changed[0]]
    assert original[span[1]:] == copy[changed[1]:]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    args = parser.parse_args()
    fixture = Path(__file__).resolve().parents[1] / 'schema/fixtures/two-rc-project.json'
    original = fixture.read_bytes()
    base = Path('build/sn023/application-tests').resolve()
    base.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='editor-', dir=base) as temporary:
        root = Path(temporary)
        (root / 'original.json').write_bytes(original)
        (root / 'invalid.json').write_bytes(b'{')
        (root / 'occupied.json').write_bytes(b'occupied sentinel')
        result = subprocess.run([str(Path(args.probe).resolve()), str(root)], env=dict(os.environ),
                                capture_output=True, text=True, timeout=30)
        print(result.stdout, end='')
        assert result.returncode == 0, result.stderr
        assert (root / 'original.json').read_bytes() == original
        assert (root / 'occupied.json').read_bytes() == b'occupied sentinel'
        if os.name == 'nt':
            verify(original, (root / 'copy.json').read_bytes())
            assert sorted(p.name for p in root.iterdir()) == ['copy.json', 'invalid.json', 'occupied.json', 'original.json']
            print('PASS independent exact byte/name oracle and source preservation')


if __name__ == '__main__':
    main()
