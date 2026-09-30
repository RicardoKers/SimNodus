# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Independent byte oracle for mixed name/resistance history and Save Copy."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'schema'))
from native_resistance_regression import verify_revision
from native_instance_label_regression import verify_revision as verify_name


def verify_outputs(root, original, escaped):
    resistance = (root / 'resistance.json').read_bytes()
    verify_revision(original, resistance, 'main', 'left', '3.5')
    assert (root / 'resistance-undone.json').read_bytes() == original
    verify_name(resistance, (root / 'mixed.json').read_bytes(), 'main', 'left', 'Edited left')
    verify_revision(original, (root / 'branch.json').read_bytes(), 'main', 'left', '4.7')
    spelling = (root / 'escaped-spelling.json').read_bytes()
    verify_revision(escaped, spelling, 'main', 'left', '1.0')
    before = json.loads(spelling)
    wanted = spelling.replace(json.dumps(before['name']).encode(), b'"Final resistance project"', 1)
    assert (root / 'project-mixed.json').read_bytes() == wanted
    assert (root / 'original.json').read_bytes() == original
    assert (root / 'escaped.json').read_bytes() == escaped
    assert (root / 'occupied.json').read_bytes() == b'occupied sentinel'
    assert sorted(p.name for p in root.iterdir()) == ['branch.json', 'escaped-spelling.json', 'escaped.json',
        'invalid.json', 'mixed.json', 'occupied.json', 'original.json', 'project-mixed.json', 'resistance-undone.json', 'resistance.json']


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    args = parser.parse_args()
    original = (Path(__file__).resolve().parents[1] / 'schema/fixtures/two-rc-project.json').read_bytes()
    escaped = original.replace(b'"value": "1"', b'"value": "\\u0031"', 1)
    base = Path('build/sn023-resistance-value/application-tests').resolve()
    base.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='resistance-', dir=base) as temporary:
        root = Path(temporary)
        for name, content in [('original.json', original), ('escaped.json', escaped), ('invalid.json', b'{'), ('occupied.json', b'occupied sentinel')]:
            (root / name).write_bytes(content)
        run = subprocess.run([str(Path(args.probe).resolve()), str(root)], env=dict(os.environ), capture_output=True, text=True, timeout=30)
        print(run.stdout, end='')
        assert run.returncode == 0, run.stderr
        if os.name == 'nt':
            verify_outputs(root, original, escaped)
            print('PASS six independent persisted snapshots and unchanged source/sentinel')


if __name__ == '__main__':
    main()
