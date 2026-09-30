"""Run native Windows document controls on original and untrusted-name fixtures.

Creates a fresh result directory; failures are retained, never replaced by retry.
Automates discard cancellation only; file dialogs and human usability are pending.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from editor_document_regression import verify


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--editor', required=True)
    parser.add_argument('--qt-kit', required=True)
    parser.add_argument('--out', required=True)
    args = parser.parse_args()
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=False)
    fixture = Path(__file__).resolve().parents[1] / 'schema/fixtures/two-rc-project.json'
    fixture_bytes = fixture.read_bytes()
    env = dict(os.environ)
    env['PATH'] = str(Path(args.qt_kit) / 'bin') + os.pathsep + env.get('PATH', '')
    env['QT_QPA_PLATFORM'] = 'windows'
    env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(Path(args.qt_kit) / 'plugins/platforms')
    results = []
    for variant in ('original', 'untrusted-name'):
        root = out / variant
        root.mkdir()
        original = fixture_bytes
        if variant == 'untrusted-name':
            declaration = json.loads(original)
            declaration['sources']['topology']['circuits'][0]['instances'][0]['name'] = '<img src="file:///C:/SN023-never-open.png">'
            original = (json.dumps(declaration, indent=2) + '\n').encode()
        (root / 'original.json').write_bytes(original)
        (root / 'invalid.json').write_bytes(b'{')
        report = out / f'{variant}-report.json'
        command = [str(Path(args.editor).resolve()), '--acceptance-root', str(root), '--report', str(report)]
        process = subprocess.run(command, env=env, capture_output=True, timeout=20)
        (out / f'{variant}-stdout.log').write_bytes(process.stdout)
        (out / f'{variant}-stderr.log').write_bytes(process.stderr)
        result = {'variant': variant, 'command': command, 'exit_code': process.returncode}
        results.append(result)
        (out / 'runner.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
        assert process.returncode == 0, process.stdout.decode(errors='replace') + process.stderr.decode(errors='replace')
        observed = json.loads(report.read_bytes())
        assert observed['passed'] and len(observed['checks']) == 23 and all(observed['checks'].values())
        if variant == 'untrusted-name':
            assert '<img src=' in observed['selected_instance_text'], 'Must retain literal untrusted name'
        assert (root / 'original.json').read_bytes() == original
        assert (root / 'invalid.json').read_bytes() == b'{'
        verify(original, (root / 'gui-copy.json').read_bytes())
        assert sorted(p.name for p in root.iterdir()) == ['gui-copy.json', 'invalid.json', 'original.json']
        result['independent_byte_audit'] = 'passed'
        result['original_sha256'] = hashlib.sha256(original).hexdigest()
        result['copy_sha256'] = hashlib.sha256((root / 'gui-copy.json').read_bytes()).hexdigest()
        (out / 'runner.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
        print(f'PASS {variant}: 23 native controls/structure checks and independent byte audit')


if __name__ == '__main__':
    main()
