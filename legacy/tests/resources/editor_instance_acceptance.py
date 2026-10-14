# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Dedicated Windows instance-label control path; retains each attempt."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'schema'))
from native_instance_label_regression import verify_revision


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--editor', required=True)
    parser.add_argument('--qt-kit', required=True)
    parser.add_argument('--out', required=True)
    args = parser.parse_args()
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    root = out / 'document'
    root.mkdir()
    fixture = Path(__file__).resolve().parents[1] / 'schema/fixtures/two-rc-project.json'
    original = fixture.read_bytes()
    (root / 'original.json').write_bytes(original)
    (root / 'invalid.json').write_bytes(b'{')
    report = out / 'report.json'
    env = dict(os.environ)
    env['PATH'] = str(Path(args.qt_kit) / 'bin') + os.pathsep + env.get('PATH', '')
    env['QT_QPA_PLATFORM'] = 'windows'
    env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(Path(args.qt_kit) / 'plugins/platforms')
    command = [str(Path(args.editor).resolve()), '--instance-acceptance-root', str(root), '--report', str(report)]
    try:
        run = subprocess.run(command, env=env, capture_output=True, timeout=20)
        (out / 'stdout.log').write_bytes(run.stdout)
        (out / 'stderr.log').write_bytes(run.stderr)
        result = {'command': command, 'exit_code': run.returncode}
    except subprocess.TimeoutExpired as error:
        (out / 'stdout.log').write_bytes(error.stdout or b'')
        (out / 'stderr.log').write_bytes(error.stderr or b'')
        (out / 'runner.json').write_text(json.dumps({'command': command, 'error': 'timeout'}, indent=2) + '\n')
        raise
    (out / 'runner.json').write_text(json.dumps(result, indent=2) + '\n')
    assert run.returncode == 0, run.stdout.decode(errors='replace') + run.stderr.decode(errors='replace')
    observed = json.loads(report.read_bytes())
    assert observed['passed'] and len(observed['checks']) == 21 and all(observed['checks'].values())
    assert (observed['selected_circuit'], observed['selected_instance']) == ('rc', 'r')
    assert (root / 'original.json').read_bytes() == original
    verify_revision(original, (root / 'instance-copy.json').read_bytes(), 'rc', 'r', 'Edited "R" \\ \u03a9')
    assert sorted(p.name for p in root.iterdir()) == ['instance-copy.json', 'invalid.json', 'original.json']
    result['independent_byte_audit'] = 'passed'
    (out / 'runner.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS 21 native instance/draft control checks and independent byte audit')


if __name__ == '__main__':
    main()
