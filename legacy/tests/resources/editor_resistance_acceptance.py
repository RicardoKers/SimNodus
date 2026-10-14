# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""One native Windows literal-resistance control path; retain each attempt."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'schema'))
from native_instance_label_regression import name_span, verify_revision as verify_name
from native_resistance_regression import verify_revision


def verify_outputs(root, original):
    project = original.replace(json.dumps(json.loads(original)['name']).encode(), b'"Resistance project"', 1)
    begin, end = name_span(project, 'main', 'left')
    names = project[:begin] + b'"Edited left"' + project[end:]
    verify_name(project, names, 'main', 'left', 'Edited left')
    assert (root / 'resistance-undone.json').read_bytes() == names
    verify_revision(names, (root / 'resistance-copy.json').read_bytes(), 'main', 'left', '3.5')
    assert (root / 'original.json').read_bytes() == original
    assert sorted(p.name for p in root.iterdir()) == ['invalid.json', 'original.json', 'resistance-copy.json', 'resistance-undone.json']


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
    original = (Path(__file__).resolve().parents[1] / 'schema/fixtures/two-rc-project.json').read_bytes()
    (root / 'original.json').write_bytes(original)
    (root / 'invalid.json').write_bytes(b'{')
    env = dict(os.environ)
    env['PATH'] = str(Path(args.qt_kit) / 'bin') + os.pathsep + env.get('PATH', '')
    env['QT_QPA_PLATFORM'] = 'windows'
    env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(Path(args.qt_kit) / 'plugins/platforms')
    command = [str(Path(args.editor).resolve()), '--resistance-acceptance-root', str(root), '--report', str(out / 'report.json')]
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
    observed = json.loads((out / 'report.json').read_bytes())
    assert observed['passed'] and len(observed['checks']) == 29 and all(observed['checks'].values())
    verify_outputs(root, original)
    result['independent_byte_audit'] = 'passed'
    (out / 'runner.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS 29 native resistance/draft/mixed-history controls and two exact persisted byte audits')


if __name__ == '__main__':
    main()
