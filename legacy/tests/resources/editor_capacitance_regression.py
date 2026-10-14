# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Independent persisted-byte oracle for mixed R/C/name one-step history."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'schema'))
from native_capacitance_regression import verify_revision
from native_resistance_regression import verify_revision as verify_resistance
from native_instance_label_regression import verify_revision as verify_name


def verify_outputs(root, original, escaped):
    cap = (root / 'capacitance.json').read_bytes()
    verify_revision(original, cap, 'main', 'right', '470')
    assert (root / 'capacitance-undone.json').read_bytes() == original
    rc = (root / 'rc.json').read_bytes()
    verify_resistance(cap, rc, 'main', 'right', '3.5')
    verify_revision(rc, (root / 'branch.json').read_bytes(), 'main', 'right', '330')
    wanted = rc.replace(json.dumps(json.loads(rc)['name']).encode(), b'"Capacitance project"', 1)
    assert (root / 'project.json').read_bytes() == wanted
    verify_name(rc, (root / 'all.json').read_bytes(), 'main', 'right', 'Edited right')
    verify_revision(escaped, (root / 'escaped-spelling.json').read_bytes(), 'main', 'right', '220.0')
    assert (root / 'original.json').read_bytes() == original
    assert (root / 'escaped.json').read_bytes() == escaped
    assert (root / 'occupied.json').read_bytes() == b'occupied sentinel'
    assert sorted(p.name for p in root.iterdir()) == ['all.json', 'branch.json', 'capacitance-undone.json', 'capacitance.json',
        'escaped-spelling.json', 'escaped.json', 'invalid.json', 'occupied.json', 'original.json', 'project.json', 'rc.json']


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    args = parser.parse_args()
    original = (Path(__file__).resolve().parents[1] / 'schema/fixtures/two-rc-project.json').read_bytes()
    escaped = original.replace(b'"value": "220"', b'"value": "\\u0032\\u0032\\u0030"', 1)
    base = Path('build/sn023-capacitance-value/application-tests').resolve()
    base.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='capacitance-', dir=base) as temporary:
        root = Path(temporary)
        for name, content in [('original.json', original), ('escaped.json', escaped), ('invalid.json', b'{'), ('occupied.json', b'occupied sentinel')]:
            (root / name).write_bytes(content)
        run = subprocess.run([str(Path(args.probe).resolve()), str(root)], env=dict(os.environ), capture_output=True, text=True, timeout=30)
        print(run.stdout, end='')
        assert run.returncode == 0, run.stderr
        if os.name == 'nt':
            verify_outputs(root, original, escaped)
            print('PASS seven independent persisted snapshots and unchanged source/sentinel')


if __name__ == '__main__':
    main()
