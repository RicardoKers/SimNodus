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
from native_capacitance_regression import verify_revision as verify_capacitance_revision
from native_occurrence_regression import expected_occurrence
import project as reference

EXPECTED_CONTROL_COUNT = 80


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
    wrong_dimension = json.loads(original)
    topology = wrong_dimension['sources']['topology']
    # Reuse the accepted valid wrong-dimension declaration; do not mislabel it as F.
    for definition in topology['components'] + topology['circuits']:
        for parameter in definition['parameters']:
            if parameter['id'] == 'capacitance':
                parameter['unit'] = 's'
    for model in topology['models']:
        for parameter in model['parameters']:
            if parameter['unit'] == 'F':
                parameter['unit'] = 's'
    main_circuit = next(row for row in topology['circuits'] if row['id'] == 'main')
    right = next(row for row in main_circuit['instances'] if row['id'] == 'right')
    right['overrides']['capacitance'] = {'value': '0.00000022', 'unit': 's'}
    reference.validate(wrong_dimension)
    inputs['wrong-capacitance-dimension.json'] = json.dumps(wrong_dimension).encode()
    default_capacitance = json.loads(original)
    circuit = next(row for row in default_capacitance['sources']['topology']['circuits'] if row['id'] == 'rc')
    capacitor = next(row for row in circuit['instances'] if row['id'] == 'c')
    del capacitor['overrides']['capacitance']
    reference.validate(default_capacitance)
    inputs['default-capacitance.json'] = json.dumps(default_capacitance).encode()
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
    assert EXPECTED_CONTROL_COUNT is not None, 'Final Qt control count must be recorded before acceptance'
    assert observed['passed'] and len(observed['checks']) == EXPECTED_CONTROL_COUNT and all(observed['checks'].values())
    assert observed['peer_action_enabled'] and observed['peer_destination'] == 'main/right/c'
    assert observed['terminal_rows'] == [
        ['n', '2', 'junction', 'junction', 'main/right/junction'],
        ['p', '1', 'drive', 'drive', 'main/right/drive']]
    assert observed['endpoint_rows'] == [
        ['Local circuit port', 'output', 'rc', 'main/right/output'],
        ['Component pin', '1', 'capacitor', 'main/right/c/p'],
        ['Component pin', '2', 'resistor', 'main/right/r/n']]
    verify_revision(original, (root / 'occurrence-copy.json').read_bytes(), 'main', 'right', '3.5')
    capacitance_copy = (root / 'capacitance-copy.json').read_bytes()
    verify_capacitance_revision(original, capacitance_copy, 'main', 'right', '470')
    right_path, left_path = ['main', 'right', 'c'], ['main', 'left', 'c']
    stages = {
        'original_right': (right_path, original),
        'original_left': (left_path, original),
        'draft': (right_path, original),
        'applied': (right_path, capacitance_copy),
        'undo': (right_path, original),
        'redo': (right_path, capacitance_copy),
        'left_after': (left_path, capacitance_copy),
        'reopen': (right_path, capacitance_copy),
        'wrong_dimension': (right_path, inputs['wrong-capacitance-dimension.json']),
        'default_origin': (right_path, inputs['default-capacitance.json']),
        'final': (right_path, capacitance_copy),
    }
    observations = observed['capacitance_observations']
    assert [row['stage'] for row in observations] == list(stages), 'Missing, duplicate or reordered C observations'
    for row in observations:
        path, raw = stages[row['stage']]
        assert row['path'] == path, row
        expected = expected_occurrence(raw, path)['parameters']['capacitance']
        assert {key: row[key] for key in ('value', 'unit', 'origin')} == expected, row
        if expected['unit'] == 'F':
            line = f"Applied capacitance: {expected['value']} F ({expected['origin']})"
            assert line in row['caption'], row
        else:
            assert 'Applied capacitance' not in row['caption'], row
    assert (out / 'report.json.capacitance.png').is_file(), 'Missing final capacitance image'
    for name, raw in inputs.items():
        assert (root / name).read_bytes() == raw
    assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'occurrence-copy.json', 'capacitance-copy.json'])
    assert sorted(path for path in resource_root.rglob('*') if path.is_file()) == sorted(assets)
    assert all(path.read_bytes() == art for path in assets)
    result['independent_persisted_byte_and_source_separate_root_audits'] = 'passed'
    result['independent_capacitance_observation_and_saved_token_audits'] = 'passed'
    (out / 'runner.json').write_text(json.dumps(result, indent=2) + '\n')
    print(f'PASS {EXPECTED_CONTROL_COUNT} occurrence/local-endpoint/explicit-peer/C controls, '
          '11 independent capacitance observations, two persisted-byte audits and unchanged source/two single-file explicit roots')


if __name__ == '__main__':
    main()
