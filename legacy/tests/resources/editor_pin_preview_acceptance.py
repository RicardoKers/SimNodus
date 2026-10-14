# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""One native declared-pin Preview path with independent saved-source auditing."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'schema'))
from native_resistance_regression import verify_revision
import project as reference


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
    inputs = {'original.json': original, 'invalid.json': b'{'}
    document = json.loads(original)
    document['sources']['topology']['components'][0]['symbol']['pin_map'] = {'p': 'b', 'n': 'a'}
    reference.validate(document)
    inputs['swapped.json'] = json.dumps(document).encode()
    document = json.loads(original)
    component = document['sources']['topology']['components'][0]
    component['symbol'] = dict(definition='two-pin-alt', pin_map={'p': 'left', 'n': 'right'})
    reference.validate(document)
    inputs['alternate.json'] = json.dumps(document).encode()
    inputs['alternate-reordered.json'] = json.dumps(document, sort_keys=True).encode()
    component['symbol']['pin_map'] = {'p': 'right', 'n': 'left'}
    reference.validate(document)
    inputs['alternate-swapped.json'] = json.dumps(document).encode()
    for name, raw in inputs.items():
        (root / name).write_bytes(raw)
    resource_root = out / 'explicit-resource-root'
    asset = resource_root / 'tests/schema/fixtures/assets/passive.svg'
    asset.parent.mkdir(parents=True)
    art = (fixture / 'assets/passive.svg').read_bytes()
    asset.write_bytes(art)
    env = dict(os.environ)
    env['PATH'] = str(Path(args.qt_kit) / 'bin') + os.pathsep + env.get('PATH', '')
    env['QT_QPA_PLATFORM'] = 'windows'
    env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(Path(args.qt_kit) / 'plugins/platforms')
    command = [str(Path(args.editor).resolve()), '--pin-preview-acceptance-root', str(root), '--resource-root', str(resource_root), '--report', str(out / 'report.json')]
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
    assert observed['passed'] and len(observed['checks']) == 20 and all(observed['checks'].values())
    verify_revision(original, (root / 'pin-copy.json').read_bytes(), 'main', 'right', '3.5')
    for name, raw in inputs.items():
        assert (root / name).read_bytes() == raw
    assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'pin-copy.json'])
    assert [path for path in resource_root.rglob('*') if path.is_file()] == [asset] and asset.read_bytes() == art
    result['independent_persisted_byte_and_source_resource_audits'] = 'passed'
    (out / 'runner.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS 20 declared-pin Preview controls, one persisted-byte audit and unchanged source/selected-only resource inventory')


if __name__ == '__main__':
    main()
