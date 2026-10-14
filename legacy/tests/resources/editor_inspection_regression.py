# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Independent byte audit of copies made during read-only inspection lifecycle."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'schema'))
from native_capacitance_regression import verify_revision as verify_capacitance
from native_resistance_regression import verify_revision as verify_resistance


def verify_outputs(root, original):
    cap = (root / 'cap.json').read_bytes()
    verify_capacitance(original, cap, 'main', 'right', '470')
    assert (root / 'undone.json').read_bytes() == original
    verify_resistance(cap, (root / 'rc.json').read_bytes(), 'main', 'right', '3.5')
    assert (root / 'original.json').read_bytes() == original
    assert (root / 'occupied.json').read_bytes() == b'occupied sentinel'
    assert sorted(p.name for p in root.iterdir()) == ['cap.json', 'invalid.json', 'occupied.json', 'original.json', 'rc.json', 'undone.json']


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    args = parser.parse_args()
    original = (Path(__file__).resolve().parents[1] / 'schema/fixtures/two-rc-project.json').read_bytes()
    base = Path('build/sn023-effective-parameters/application-tests').resolve()
    base.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='inspection-', dir=base) as temporary:
        root = Path(temporary)
        for name, content in [('original.json', original), ('invalid.json', b'{'), ('occupied.json', b'occupied sentinel')]:
            (root / name).write_bytes(content)
        run = subprocess.run([str(Path(args.probe).resolve()), str(root)], env=dict(os.environ), capture_output=True, text=True, timeout=30)
        print(run.stdout, end='')
        assert run.returncode == 0, run.stderr
        if os.name == 'nt':
            verify_outputs(root, original)
            print('PASS three independent persisted byte audits and unchanged source/sentinel')


if __name__ == '__main__':
    main()
