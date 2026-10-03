# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""One native read-only occurrence path with independent copy and root audits."""
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
    reordered = json.loads(original)
    topology = reordered['sources']['topology']
    for collection in ('components', 'circuits'):
        topology[collection].reverse()
    for circuit in topology['circuits']:
        circuit['instances'].reverse()
    reference.validate(reordered)
    inputs['reordered.json'] = json.dumps(reordered, sort_keys=True).encode()
    swapped = json.loads(original)
    swapped['sources']['topology']['components'][0]['symbol']['pin_map'] = {'p': 'b', 'n': 'a'}
    reference.validate(swapped)
    inputs['swapped.json'] = json.dumps(swapped).encode()
    changed = json.loads(original)
    local = next(row for row in changed['sources']['topology']['circuits'] if row['id'] == 'rc')
    for net in local['nets']:
        net['name'] = '<b>same Ω label</b>'
        if net['id'] == 'drive':
            net['id'] = 'drive_alt'
        for terminal in net['terminals']:
            if terminal.get('instance') == 'r':
                terminal['terminal'] = {'p': 'n', 'n': 'p'}[terminal['terminal']]
    reference.validate(changed)
    inputs['changed-nets.json'] = json.dumps(changed, ensure_ascii=False).encode()
    unconnected = json.loads(original)
    local = next(row for row in unconnected['sources']['topology']['circuits'] if row['id'] == 'rc')
    for net in local['nets']:
        net['terminals'] = [row for row in net['terminals'] if row != {'instance': 'r', 'terminal': 'n'}]
    reference.validate(unconnected)
    inputs['unconnected.json'] = json.dumps(unconnected).encode()
    removed = json.loads(original)
    topology = removed['sources']['topology']
    resistor = next(row for row in topology['components'] if row['id'] == 'resistor')
    resistor['pins'] = [row for row in resistor['pins'] if row['id'] != 'n']
    resistor['symbol'] = None
    resistor['model'] = None
    for circuit in topology['circuits']:
        for net in circuit['nets']:
            net['terminals'] = [row for row in net['terminals'] if row != {'instance': 'r', 'terminal': 'n'}]
    reference.validate(removed)
    inputs['removed-pin.json'] = json.dumps(removed).encode()
    peer_labels = json.loads(original)
    topology = peer_labels['sources']['topology']
    topology['components'].reverse()
    topology['circuits'].reverse()
    for component in topology['components']:
        component['pins'].reverse()
        for pin in component['pins']:
            pin['name'] = '<b>same Ω label</b>'
    for circuit in topology['circuits']:
        circuit['instances'].reverse()
        for instance in circuit['instances']:
            instance['name'] = '<b>same Ω label</b>'
        for net in circuit['nets']:
            net['terminals'].reverse()
            net['name'] = '<b>same Ω label</b>'
    reference.validate(peer_labels)
    inputs['peer-labels.json'] = json.dumps(peer_labels, ensure_ascii=False, sort_keys=True).encode()
    ports = json.loads(original)
    main_circuit = next(row for row in ports['sources']['topology']['circuits'] if row['id'] == 'main')
    main_circuit['instances'].append(dict(id='probe', name='<b>same Ω label</b>', kind='component',
                                        definition='resistor', overrides={'resistance': {'value': '1', 'unit': 'kohm'}}))
    for pin, net_id in (('p', 'drive'), ('n', 'return')):
        next(row for row in main_circuit['nets'] if row['id'] == net_id)['terminals'].append(
            dict(instance='probe', terminal=pin))
    reference.validate(ports)
    inputs['peer-ports.json'] = json.dumps(ports, ensure_ascii=False).encode()
    missing_peer = json.loads(original)
    circuit = next(row for row in missing_peer['sources']['topology']['circuits'] if row['id'] == 'rc')
    net = next(row for row in circuit['nets'] if row['id'] == 'junction')
    net['terminals'].remove({'instance': 'c', 'terminal': 'p'})
    reference.validate(missing_peer)
    inputs['missing-peer.json'] = json.dumps(missing_peer).encode()
    for name, raw in inputs.items():
        (root / name).write_bytes(raw)
    resource_root = out / 'explicit-roots'
    art = (fixture / 'assets/passive.svg').read_bytes()
    assets = []
    for role in ('library', 'occurrence'):
        asset = resource_root / role / 'tests/schema/fixtures/assets/passive.svg'
        asset.parent.mkdir(parents=True)
        asset.write_bytes(art)
        assets.append(asset)
    env = dict(os.environ)
    env['PATH'] = str(Path(args.qt_kit) / 'bin') + os.pathsep + env.get('PATH', '')
    env['QT_QPA_PLATFORM'] = 'windows'
    env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(Path(args.qt_kit) / 'plugins/platforms')
    command = [str(Path(args.editor).resolve()), '--occurrence-acceptance-root', str(root), '--resource-root', str(resource_root), '--report', str(out / 'report.json')]
    try:
        run = subprocess.run(command, env=env, capture_output=True, timeout=30)
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
    assert observed['passed'] and len(observed['checks']) == 67 and all(observed['checks'].values())
    assert observed['peer_action_enabled'] and observed['peer_destination'] == 'main/right/c'
    assert observed['terminal_rows'] == [
        ['n', '2', 'junction', 'junction', 'main/right/junction'],
        ['p', '1', 'drive', 'drive', 'main/right/drive']]
    assert observed['endpoint_rows'] == [
        ['Local circuit port', 'output', 'rc', 'main/right/output'],
        ['Component pin', '1', 'capacitor', 'main/right/c/p'],
        ['Component pin', '2', 'resistor', 'main/right/r/n']]
    verify_revision(original, (root / 'occurrence-copy.json').read_bytes(), 'main', 'right', '3.5')
    for name, raw in inputs.items():
        assert (root / name).read_bytes() == raw
    assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'occurrence-copy.json'])
    assert sorted(path for path in resource_root.rglob('*') if path.is_file()) == sorted(assets)
    assert all(path.read_bytes() == art for path in assets)
    result['independent_persisted_byte_and_source_separate_root_audits'] = 'passed'
    (out / 'runner.json').write_text(json.dumps(result, indent=2) + '\n')
    print('PASS 67 occurrence/local-endpoint/explicit-peer controls, one persisted-byte audit and unchanged source/two single-file explicit roots')


if __name__ == '__main__':
    main()
