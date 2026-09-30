# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Independent persisted-byte oracle for one applied name transition."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'schema'))
from native_instance_label_regression import verify_revision


def verify_outputs(root, original, escaped):
    project = (root / 'project.json').read_bytes()
    before = json.loads(original)
    after = json.loads(project)
    assert after['name'] == 'History "project" \\ \u03a9'
    after['name'] = before['name']
    assert after == before
    # This owned fixture declares the root name before all other names.
    old = json.dumps(before['name']).encode()
    new = json.dumps('History "project" \\ \u03a9', ensure_ascii=False).encode()
    assert project == original.replace(old, new, 1)
    assert (root / 'clean.json').read_bytes() == original
    assert (root / 'undo-latest.json').read_bytes() == project
    both = (root / 'both.json').read_bytes()
    verify_revision(project, both, 'rc', 'r', 'History "instance" \\ \u03a9')
    assert (root / 'redo-latest.json').read_bytes() == both
    verify_revision(project, (root / 'branch.json').read_bytes(), 'rc', 'c', 'History branch')
    assert (root / 'escaped-restored.json').read_bytes() == original
    assert (root / 'escaped.json').read_bytes() == escaped
    assert (root / 'original.json').read_bytes() == original
    assert (root / 'occupied.json').read_bytes() == b'occupied sentinel'
    assert sorted(p.name for p in root.iterdir()) == ['both.json', 'branch.json', 'clean.json',
        'escaped-restored.json', 'escaped.json', 'invalid.json', 'occupied.json', 'original.json',
        'project.json', 'redo-latest.json', 'undo-latest.json']


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    args = parser.parse_args()
    original = (Path(__file__).resolve().parents[1] / 'schema/fixtures/two-rc-project.json').read_bytes()
    before = json.loads(original)
    escaped = original.replace(json.dumps(before['name']).encode(), b'"\\u0054wo RC declaration project"', 1)
    base = Path('build/sn023-name-history/application-tests').resolve()
    base.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='history-', dir=base) as temporary:
        root = Path(temporary)
        (root / 'original.json').write_bytes(original)
        (root / 'escaped.json').write_bytes(escaped)
        (root / 'invalid.json').write_bytes(b'{')
        (root / 'occupied.json').write_bytes(b'occupied sentinel')
        run = subprocess.run([str(Path(args.probe).resolve()), str(root)], env=dict(os.environ),
                             capture_output=True, text=True, timeout=30)
        print(run.stdout, end='')
        assert run.returncode == 0, run.stderr
        if os.name == 'nt':
            verify_outputs(root, original, escaped)
            print('PASS seven persisted snapshot byte audits and unchanged source/sentinel')


if __name__ == '__main__':
    main()
