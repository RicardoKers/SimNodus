# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""One explicit native Qt fixture Preview, with independent copy/resource audits."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'schema'))
from native_instance_label_regression import name_span, verify_revision as verify_name
from native_capacitance_regression import verify_revision as verify_capacitance
from native_resistance_regression import verify_revision as verify_resistance


def verify_outputs(root, resource_root, original, art):
    names = original.replace(b'"Two RC declaration project"', b'"Preview project"', 1)
    begin, end = name_span(names, 'main', 'right', ('overrides', 'resistance', 'value'))
    resistance = names[:begin] + b'"3.5"' + names[end:]
    verify_resistance(names, resistance, 'main', 'right', '3.5')
    undone = (root / 'preview-undone.json').read_bytes()
    verify_capacitance(resistance, undone, 'main', 'right', '470')
    verify_name(undone, (root / 'preview-copy.json').read_bytes(), 'main', 'right', 'Preview right')
    assert (root / 'original.json').read_bytes() == original
    assert sorted(p.name for p in root.iterdir()) == ['invalid.json', 'original.json', 'preview-copy.json', 'preview-undone.json']
    files = [path for path in resource_root.rglob('*') if path.is_file()]
    assert len(files) == 1 and files[0].relative_to(resource_root).as_posix() == 'tests/schema/fixtures/assets/passive.svg'
    assert files[0].read_bytes() == art


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
    fixture = Path(__file__).resolve().parents[1] / 'schema/fixtures'
    original = (fixture / 'two-rc-project.json').read_bytes()
    art = (fixture / 'assets/passive.svg').read_bytes()
    (root / 'original.json').write_bytes(original)
    (root / 'invalid.json').write_bytes(b'{')
    resource_root = out / 'explicit-resource-root'
    asset = resource_root / 'tests/schema/fixtures/assets/passive.svg'
    asset.parent.mkdir(parents=True)
    asset.write_bytes(art)
    env = dict(os.environ)
    env['PATH'] = str(Path(args.qt_kit) / 'bin') + os.pathsep + env.get('PATH', '')
    env['QT_QPA_PLATFORM'] = 'windows'
    env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(Path(args.qt_kit) / 'plugins/platforms')
    command = [str(Path(args.editor).resolve()), '--preview-acceptance-root', str(root), '--resource-root', str(resource_root), '--report', str(out / 'report.json')]
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
    assert observed['passed'] and len(observed['checks']) == 31 and all(observed['checks'].values())
    verify_outputs(root, resource_root, original, art)
    result['independent_byte_and_resource_audits'] = 'passed'
    (out / 'runner.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS 31 native fixture Preview controls, two persisted-byte audits and selected-only resource inventory')


if __name__ == '__main__':
    main()
