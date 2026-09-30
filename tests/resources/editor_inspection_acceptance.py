# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""One native Qt resolved-parameter path and independent persisted-byte audits."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'schema'))
from native_instance_label_regression import verify_revision as verify_name
from native_capacitance_regression import verify_revision as verify_capacitance
from native_resistance_regression import verify_revision as verify_resistance


def verify_outputs(root, original):
    final = (root / 'inspected.json').read_bytes()
    names_only = original.replace(b'"Two RC declaration project"', b'"Inspection project"', 1)
    undone = (root / 'inspection-undone.json').read_bytes()
    # Each independent selected-byte oracle checks the intermediate construction.
    from native_instance_label_regression import name_span
    begin, end = name_span(names_only, 'main', 'right', ('overrides', 'resistance', 'value'))
    resistance = names_only[:begin] + b'"3.5"' + names_only[end:]
    verify_resistance(names_only, resistance, 'main', 'right', '3.5')
    verify_capacitance(resistance, undone, 'main', 'right', '470')
    verify_name(undone, final, 'main', 'right', 'Inspection right')
    assert (root / 'original.json').read_bytes() == original
    assert sorted(p.name for p in root.iterdir()) == ['inspected.json', 'inspection-undone.json', 'invalid.json', 'original.json']


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
    command = [str(Path(args.editor).resolve()), '--inspection-acceptance-root', str(root), '--report', str(out / 'report.json')]
    try:
        run = subprocess.run(command, env=env, capture_output=True, timeout=25)
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
    assert observed['passed'] and len(observed['checks']) == 33 and all(observed['checks'].values())
    verify_outputs(root, original)
    result['independent_byte_audit'] = 'passed'
    (out / 'runner.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS 33 native read-only inspection controls and two independent persisted byte audits')


if __name__ == '__main__':
    main()
