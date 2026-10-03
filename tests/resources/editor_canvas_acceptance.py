# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Fixed RC canvas observations against declared JSON and exact saved bytes."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'schema'))
from native_capacitance_regression import verify_revision
from native_occurrence_regression import expected_occurrence
import project as reference

REQUIRED_CHECKS = {
    'default_canvas_without_document_or_selection',
    'inert_open_supported_exact_shape',
    'canvas_mode_hides_declaration_fields',
    'resistor_click_inspects_full_ids_without_edit_target',
    'capacitor_click_inspects_without_capture_or_library_selection',
    'blank_click_does_not_create_or_rebind',
    'fitted_resize_hit_test_uses_same_component_identity',
    'initial_canvas_image',
    'explicit_containing_action_uses_existing_forwarded_target',
    'canvas_clicks_preserve_four_drafts_and_edit_target',
    'context_inspection_preserves_drafts_and_refuses_target_replacement',
    'pending_text_does_not_apply_or_allow_copy_history',
    'existing_C_apply_refreshes_canvas_and_selection',
    'invalid_C_edit_retains_current_graph_and_canvas',
    'native_undo_refreshes_canvas',
    'native_redo_refreshes_canvas',
    'left_context_applied_value_is_independent',
    'create_only_copy_retains_original_association_and_history',
    'occupied_copy_refused_without_canvas_change',
    'failed_open_preserves_current_inspection_and_revision',
    'successful_copy_open_clears_selection_and_history',
    'copy_reselection_recovers_applied_C',
    'reordered_arrays_and_keys_keep_shape_and_ID_click',
    'equal_HTML_labels_are_plain_and_not_identity',
    'own_notation_is_independent_of_project_symbol_maps',
    'missing_endpoint_refuses_false_fixed_wires',
    'changed_net_ID_refuses_known_layout',
    'extra_pin_refuses_incomplete_fixed_layout',
    'wrong_dimension_refuses_R_C_notation',
    'nonforwarded_component_refuses_containing_edit',
    'final_copy_restores_supported_current_shape',
    'independent_analyzer_and_adjustable_existing_docks',
    'final_canvas_image_and_compact_properties',
}


def prepare_inputs(original):
    inputs = {'original.json': original, 'invalid.json': b'{'}

    def reverse_keys(value):
        if isinstance(value, dict):
            return {key: reverse_keys(value[key]) for key in reversed(value)}
        if isinstance(value, list):
            return [reverse_keys(row) for row in value]
        return value

    def retain(name, document, reordered=False):
        reference.validate(document)
        raw = json.dumps(reverse_keys(document) if reordered else document, ensure_ascii=False).encode()
        reference.parse(raw)
        inputs[name] = raw

    document = json.loads(original)
    topology = document['sources']['topology']
    for collection in ('components', 'circuits', 'symbols', 'models'):
        topology[collection].reverse()
    for definition in topology['components'] + topology['circuits']:
        for key in ('pins', 'ports', 'parameters', 'instances', 'nets'):
            if key in definition:
                definition[key].reverse()
        for net in definition.get('nets', []):
            net['terminals'].reverse()
    retain('reordered.json', document, reordered=True)

    document = json.loads(original)
    topology = document['sources']['topology']
    for definition in topology['components'] + topology['circuits']:
        definition['name'] = '<b>same Ω label</b>'
        for key in ('pins', 'ports', 'instances', 'nets'):
            for row in definition.get(key, []):
                row['name'] = '<b>same Ω label</b>'
    retain('labels.json', document)

    document = json.loads(original)
    for component in document['sources']['topology']['components']:
        component['symbol']['pin_map'] = {'p': 'b', 'n': 'a'}
    retain('swapped.json', document)

    document = json.loads(original)
    circuit = next(row for row in document['sources']['topology']['circuits'] if row['id'] == 'rc')
    next(row for row in circuit['nets'] if row['id'] == 'junction')['terminals'].remove(
        {'instance': 'c', 'terminal': 'p'})
    retain('missing-peer.json', document)

    document = json.loads(original)
    circuit = next(row for row in document['sources']['topology']['circuits'] if row['id'] == 'rc')
    next(row for row in circuit['nets'] if row['id'] == 'drive')['id'] = 'drive_alt'
    retain('changed-net.json', document)

    document = json.loads(original)
    capacitor = next(row for row in document['sources']['topology']['components'] if row['id'] == 'capacitor')
    capacitor['pins'].append({'id': 'spare', 'name': 'spare', 'domain': 'electrical'})
    # The extra logical pin deliberately has no artwork/model correspondence.
    capacitor['symbol'] = None
    capacitor['model'] = None
    retain('extra-pin.json', document)

    document = json.loads(original)
    topology = document['sources']['topology']
    for definition in topology['components'] + topology['circuits']:
        for parameter in definition['parameters']:
            if parameter['id'] == 'capacitance':
                parameter['unit'] = 's'
    for model in topology['models']:
        for parameter in model['parameters']:
            if parameter['unit'] == 'F':
                parameter['unit'] = 's'
    main = next(row for row in topology['circuits'] if row['id'] == 'main')
    right = next(row for row in main['instances'] if row['id'] == 'right')
    right['overrides']['capacitance'] = {'value': '0.00000022', 'unit': 's'}
    retain('wrong-dimension.json', document)

    document = json.loads(original)
    circuit = next(row for row in document['sources']['topology']['circuits'] if row['id'] == 'rc')
    capacitor = next(row for row in circuit['instances'] if row['id'] == 'c')
    del capacitor['overrides']['capacitance']
    retain('default-binding.json', document)
    reference.parse(original)
    return inputs


def expected_diagram(raw, context):
    """Walk declared circuit IDs and interfaces, with no geometry assumptions."""
    topology = reference.parse(raw)['sources']['topology']
    circuits = {row['id']: row for row in topology['circuits']}
    components = {row['id']: row for row in topology['components']}
    assert context and context[0] == topology['root']
    circuit = circuits[topology['root']]
    for instance_id in context[1:]:
        instance = next(row for row in circuit['instances'] if row['id'] == instance_id)
        assert instance['kind'] == 'circuit'
        circuit = circuits[instance['definition']]
    owned_components = []
    for instance in circuit['instances']:
        path = [*context, instance['id']]
        selected = expected_occurrence(raw, path)
        assert selected is not None
        owned_components.append(dict(path=path, name=selected['name'], component=selected['component'],
            parameters=selected['parameters'], terminals=[dict(pin=row['pin'], name=row['name'],
                net_path=row['net']['path'] if row['net'] is not None else []) for row in selected['terminals']]))
    instances = {row['id']: row for row in circuit['instances']}
    ports = {row['id']: row for row in circuit['ports']}
    nets = []
    for net in circuit['nets']:
        endpoints = []
        for terminal in net['terminals']:
            if 'port' in terminal:
                port = ports[terminal['port']]
                endpoint = dict(kind='Local circuit port', instance='', terminal=port['id'], name=port['name'],
                                definition=circuit['id'], path=[*context, port['id']])
            else:
                instance = instances[terminal['instance']]
                definition = instance['definition']
                if instance['kind'] == 'component':
                    kind, interface = 'Component pin', components[definition]['pins']
                else:
                    kind, interface = 'Subcircuit port', circuits[definition]['ports']
                pin = next(row for row in interface if row['id'] == terminal['terminal'])
                endpoint = dict(kind=kind, instance=instance['id'], terminal=pin['id'], name=pin['name'],
                                definition=definition, path=[*context, instance['id'], pin['id']])
            endpoints.append(endpoint)
        nets.append(dict(id=net['id'], name=net['name'], path=[*context, net['id']], endpoints=endpoints))
    return owned_components, nets


def canonical_components(rows):
    projected = []
    for row in rows:
        value = {key: row[key] for key in ('path', 'name', 'component', 'parameters', 'terminals')}
        value['terminals'] = sorted(value['terminals'], key=lambda terminal: terminal['pin'])
        projected.append(value)
    return sorted(projected, key=lambda row: row['path'])


def canonical_nets(rows):
    projected = []
    for row in rows:
        value = {key: row[key] for key in ('id', 'name', 'path', 'endpoints')}
        value['endpoints'] = sorted(value['endpoints'], key=lambda endpoint: (
            endpoint['kind'], endpoint['instance'], endpoint['terminal']))
        projected.append(value)
    return sorted(projected, key=lambda row: row['id'])


def verify_observations(observations, inputs):
    stages = {
        'original': 'original.json', 'right_r': 'original.json', 'right_c': 'original.json',
        'draft': 'original.json', 'applied': 'canvas-copy.json', 'undo': 'original.json',
        'redo': 'canvas-copy.json', 'left': 'canvas-copy.json', 'reopen': 'canvas-copy.json',
        'reordered': 'reordered.json', 'labels': 'labels.json', 'swapped': 'swapped.json',
        'missing_peer': 'missing-peer.json', 'changed_net': 'changed-net.json',
        'extra_pin': 'extra-pin.json', 'wrong_dimension': 'wrong-dimension.json',
        'default_binding': 'default-binding.json', 'final': 'canvas-copy.json',
    }
    unsupported = {'missing_peer', 'changed_net', 'extra_pin', 'wrong_dimension', 'default_binding'}
    assert [row['stage'] for row in observations] == list(stages), 'Missing, duplicate or reordered canvas stages'
    for observation in observations:
        stage = observation['stage']
        assert observation['input'] == stages[stage], observation
        canvas = observation['canvas']
        context = ['main', 'left' if stage == 'left' else 'right']
        assert canvas['context'] == context, observation
        assert canvas['supported'] is (stage not in unsupported), observation
        if stage in unsupported:
            assert canvas['components'] == [] and canvas['nets'] == [] and canvas['selected_path'] == [], observation
            continue
        expected_components, expected_nets = expected_diagram(inputs[stages[stage]], context)
        assert canonical_components(canvas['components']) == canonical_components(expected_components), observation
        assert canonical_nets(canvas['nets']) == canonical_nets(expected_nets), observation
        selected = [] if stage == 'original' else [*context, 'r' if stage == 'right_r' else 'c']
        assert canvas['selected_path'] == selected, observation


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
    inputs = prepare_inputs(original)
    for name, raw in inputs.items():
        (root / name).write_bytes(raw)
    env = {key: value for key, value in os.environ.items() if not key.startswith('QT_')}
    env['PATH'] = str(Path(args.qt_kit) / 'bin') + os.pathsep + env.get('PATH', '')
    env['QT_QPA_PLATFORM'] = 'windows'
    env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(Path(args.qt_kit) / 'plugins/platforms')
    command = [str(Path(args.editor).resolve()), '--canvas-acceptance-root', str(root), '--report', str(out / 'report.json')]
    result = dict(command=command, state='launch-requested')

    def retain(stdout, stderr):
        (out / 'stdout.log').write_bytes(stdout)
        (out / 'stderr.log').write_bytes(stderr)
        (out / 'runner.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')

    retain(b'', b'')
    try:
        run = subprocess.run(command, env=env, capture_output=True, timeout=30)
    except subprocess.TimeoutExpired as error:
        result.update(state='timeout', timeout_seconds=30)
        retain(error.stdout or b'', error.stderr or b'')
        raise
    except OSError as error:
        result.update(state='launch-failed', error=str(error))
        retain(b'', b'')
        raise
    result.update(state='process-exited', exit_code=run.returncode)
    retain(run.stdout, run.stderr)
    assert run.returncode == 0, run.stdout.decode(errors='replace') + run.stderr.decode(errors='replace')
    observed = json.loads((out / 'report.json').read_bytes())
    assert observed['passed'] and observed['checks'] and all(value is True for value in observed['checks'].values())
    assert set(observed['checks']) == REQUIRED_CHECKS, observed['checks']
    assert observed['platform'] == 'windows'
    revised = (root / 'canvas-copy.json').read_bytes()
    verify_revision(original, revised, 'main', 'right', '470')
    verify_observations(observed['observations'], {**inputs, 'canvas-copy.json': revised})
    for suffix in ('initial', 'final'):
        image = out / f'report.json.{suffix}.png'
        assert image.is_file() and image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), image
    for name, raw in inputs.items():
        assert (root / name).read_bytes() == raw, name
    assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'canvas-copy.json'])
    assert all(path.is_file() for path in root.iterdir()), 'No resource directories may be prepared in the document root'
    assert fixture.read_bytes() == original
    result['independent_canvas_observations_and_saved_token_audit'] = 'passed'
    result['unchanged_inputs_and_no_resource_files_audit'] = 'passed'
    retain(run.stdout, run.stderr)
    print(f"PASS {len(REQUIRED_CHECKS)} fixed RC canvas controls, 18 independent declared snapshots, "
          'one C-token saved-byte audit, two PNG captures and unchanged inputs without resource files')


if __name__ == '__main__':
    main()
