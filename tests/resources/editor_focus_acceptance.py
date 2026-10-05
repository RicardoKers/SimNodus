# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Transient circuit focus against independent declarations, layout and bytes."""
import argparse
import json
import os
from pathlib import Path
import subprocess

from editor_canvas_acceptance import (
    canonical_components,
    canonical_nets,
    expected_diagram,
    prepare_inputs,
)
from editor_caption_acceptance import expected_caption
from editor_pan_acceptance import verify_view
from native_capacitance_regression import verify_revision as verify_capacitance_revision

REQUIRED_CHECKS = {
    'empty_focus_roundtrip_inert',
    'inert_open_default_view',
    'R_zoom_pan_four_drafts',
    'focus_hides_panels_expands_canvas',
    'focus_blocks_details_panel_toggles',
    'focus_preserves_drafts_graph_view',
    'focus_R_C_hit_inspection_only',
    'analyzer_independent_close_reopen',
    'restores_panels_sizes_splitter_R_activation',
    'repeat_cycle_uses_new_panel_sizes',
    'previously_hidden_components_remain_hidden',
    'both_previously_hidden_remain_hidden',
    'floating_properties_visibility_geometry_restored',
    'details_refuses_focus',
    'focus_toggle_during_drag_cancels_view_only',
    'C_apply_retains_R_draft_view',
    'pending_R_refuses_copy_history',
    'focus_undo_redo_retains_layout_view_C',
    'focus_create_only_copy_and_occupied_refusal',
    'failed_open_retains_focus_selection_drafts',
    'successful_open_in_focus_resets_document_view_only',
    'unsupported_focus_remains_inert',
    'leave_focus_restores_adjustable_panels_and_images',
    'resources_unaccessed_source_unchanged',
}

# Input, zoom, pan, selection, edit target, R/C drafts and visible activation;
# focus, retained R/C targets, project/instance drafts. No ignored runtime files.
STAGES = {
    'original': ('original.json', 100, [0, 0], [], [], '', '', False, False, False, [], [], '@input', ''),
    'normal_R': ('original.json', 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False, ['main', 'right'], [], 'Pending project', 'Pending right'),
    'focus_R': ('original.json', 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', False, False, True, ['main', 'right'], [], 'Pending project', 'Pending right'),
    'focus_C': ('original.json', 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, False, True, ['main', 'right'], [], 'Pending project', 'Pending right'),
    'restored_R': ('original.json', 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False, ['main', 'right'], [], 'Pending project', 'Pending right'),
    'C_applied': ('focus-C-copy.json', 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False, ['main', 'right'], ['main', 'right'], '@input', 'right'),
    'focus_C_applied': ('focus-C-copy.json', 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '2.2', '470', False, False, True, ['main', 'right'], ['main', 'right'], '@input', 'right'),
    'undo_focus_C': ('original.json', 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '2.2', '220', False, False, True, ['main', 'right'], ['main', 'right'], '@input', 'right'),
    'redo_focus_C': ('focus-C-copy.json', 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '2.2', '470', False, False, True, ['main', 'right'], ['main', 'right'], '@input', 'right'),
    'reopened_focus': ('focus-C-copy.json', 100, [0, 0], [], [], '', '', False, False, True, [], [], '@input', ''),
    'unsupported_focus': ('changed-net.json', 100, [0, 0], [], [], '', '', False, False, True, [], [], '@input', ''),
    'final_restored_C': ('focus-C-copy.json', 100, [0, 0], ['main', 'right', 'c'], ['main', 'right'], '2.2', '470', False, True, False, [], ['main', 'right'], '@input', 'right'),
}

# Components hidden, Properties hidden, Components floating, Properties floating.
LAYOUT_CASES = {
    'initial': (False, False, False, False),
    'resized': (False, False, False, False),
    'hidden_components': (True, False, False, False),
    'both_hidden': (True, True, False, False),
    'floating_properties': (False, False, False, True),
}


def verify_layout_roundtrips(roundtrips, audit):
    assert [row['name'] for row in roundtrips] == list(LAYOUT_CASES), 'Missing, duplicate or reordered panel round trips'
    flags = ('components_hidden', 'properties_hidden', 'components_floating', 'properties_floating')
    for row in roundtrips:
        name = row['name']
        before, after = row['before'], row['after']
        row_audit = dict(name=name, before=before, after=after, pixel_tolerance=2)
        audit.append(row_audit)
        for snapshot in (before, after):
            for field, expected in zip(flags, LAYOUT_CASES[name]):
                assert snapshot[field] is expected, f'{name}: wrong panel visibility or floating state'
            for panel in ('components', 'properties'):
                assert type(snapshot[panel + '_area']) is int and snapshot[panel + '_area'] in (1, 2, 4, 8), f'{name}: invalid dock area'
                assert type(snapshot[panel + '_width']) is int and snapshot[panel + '_width'] > 0, f'{name}: invalid dock width'
            splitter = snapshot['splitter']
            assert type(splitter) is list and len(splitter) == 2 and all(type(value) is int and value >= 0 for value in splitter), f'{name}: invalid Components/Preview splitter'
            geometry = snapshot['properties_geometry']
            assert type(geometry) is list and len(geometry) == 4 and all(type(value) is int for value in geometry), f'{name}: invalid Properties geometry'
            assert geometry[2] > 0 and geometry[3] > 0, f'{name}: empty Properties geometry'
        for panel in ('components', 'properties'):
            assert before[panel + '_area'] == after[panel + '_area'], f'{name}: changed dock area'
            if not before[panel + '_hidden']:
                assert abs(before[panel + '_width'] - after[panel + '_width']) <= 2, f'{name}: changed visible dock width'
        assert all(abs(first - second) <= 2 for first, second in zip(before['splitter'], after['splitter'])), f'{name}: changed Components/Preview splitter'
        if name == 'floating_properties':
            assert all(abs(first - second) <= 2 for first, second in zip(before['properties_geometry'], after['properties_geometry'])), f'{name}: changed floating Properties geometry'


def verify_observations(observations, inputs, audit):
    assert [row['stage'] for row in observations] == list(STAGES), 'Missing, duplicate or reordered focus stages'
    component_count = properties_count = 0
    for observation in observations:
        stage = observation['stage']
        (filename, percent, pan, selected, target, r_draft, c_draft, r_active, c_active,
         focused, r_activation, c_activation, project_draft, instance_draft) = STAGES[stage]
        if project_draft == '@input':
            project_draft = json.loads(inputs[filename])['name']
        row_audit = dict(stage=stage, expected_project_draft=project_draft, expected_instance_draft=instance_draft,
            expected_r_activation=r_activation, expected_c_activation=c_activation, expected_focused=focused)
        audit.append(row_audit)
        assert observation['input'] == filename and observation['edit_target'] == target, f'{stage}: wrong input or edit target'
        assert observation['r_draft'] == r_draft and observation['c_draft'] == c_draft, f'{stage}: changed R/C draft'
        assert observation['project_draft'] == project_draft and observation['instance_draft'] == instance_draft, f'{stage}: changed project/instance draft'
        assert observation['r_activation'] == r_activation and observation['c_activation'] == c_activation, f'{stage}: changed retained activation target'
        assert observation['r_active'] is r_active and observation['c_active'] is c_active, f'{stage}: wrong visible R/C activation'
        assert observation['focused'] is focused, f'{stage}: wrong focus mode'
        canvas = observation['canvas']
        assert canvas['context'] == ['main', 'right'] and canvas['selected_path'] == selected, f'{stage}: wrong full selection IDs'
        supported = stage != 'unsupported_focus'
        assert canvas['supported'] is supported, f'{stage}: wrong supported state'
        verify_view(canvas['view'], percent, pan, False, supported, stage, row_audit)
        properties = observation['properties_text']
        assert type(properties) is str, f'{stage}: Properties text missing'
        if not supported:
            assert canvas['components'] == [] and canvas['nets'] == [], f'{stage}: unsupported diagram retained components or nets'
        else:
            components, nets = expected_diagram(inputs[filename], ['main', 'right'])
            assert canonical_components(canvas['components']) == canonical_components(components), f'{stage}: component declaration mismatch'
            assert canonical_nets(canvas['nets']) == canonical_nets(nets), f'{stage}: net declaration mismatch'
            row_audit['captions'] = []
            for component in canvas['components']:
                parameter_id = 'resistance' if component['component'] == 'resistor' else 'capacitance'
                parameter = component['parameters'][parameter_id]
                caption = expected_caption(parameter['value'], parameter['unit'])
                row_audit['captions'].append(dict(path=component['path'], expected=caption, actual=component['caption']))
                assert component['caption'] == caption, f'{stage}: painted caption mismatch'
                component_count += 1
        if not selected:
            assert 'Applied ' not in properties and 'Base value:' not in properties, f'{stage}: unselected Properties retained values'
            continue
        matching = [component for component in canvas['components'] if component['path'] == selected]
        assert len(matching) == 1, f'{stage}: selected component unavailable'
        component = matching[0]
        parameter_id = 'resistance' if component['component'] == 'resistor' else 'capacitance'
        parameter = component['parameters'][parameter_id]
        caption = expected_caption(parameter['value'], parameter['unit'])
        base = 'Base value: ' + parameter['value'] + ' ' + parameter['unit']
        block = f'Applied {parameter_id}:\n{caption}\n{base}'
        row_audit['properties'] = dict(expected=block, actual=properties)
        assert block in properties, f'{stage}: Properties caption/base-value mismatch'
        assert [line for line in properties.splitlines() if line.startswith('Base value:')] == [base], f'{stage}: base value missing or duplicated'
        properties_count += 1
    assert component_count == 22 and properties_count == 9, 'Wrong independent caption/Properties audit counts'


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
    command = [str(Path(args.editor).resolve()), '--focus-acceptance-root', str(root), '--report', str(out / 'report.json')]
    result = dict(command=command, state='launch-requested', independent_observations=[], independent_layout_roundtrips=[])

    def retain(stdout, stderr):
        (out / 'stdout.log').write_bytes(stdout)
        (out / 'stderr.log').write_bytes(stderr)
        (out / 'runner.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')

    retain(b'', b'')
    try:
        run = subprocess.run(command, env=env, capture_output=True, timeout=30)
    except subprocess.TimeoutExpired as error:
        result.update(state='timeout', timeout_seconds=30)
        retain(error.stdout or b'', error.stderr or b'')
        raise
    except Exception as error:
        result.update(state='launch-failed', error=f'{type(error).__name__}: {error}')
        retain(b'', b'')
        raise
    result.update(state='process-exited', exit_code=run.returncode)
    retain(run.stdout, run.stderr)
    try:
        assert run.returncode == 0, 'Native focus acceptance failed; inspect retained report.json and raw stdout/stderr'
        observed = json.loads((out / 'report.json').read_bytes())
        assert observed['passed'] is True and observed['checks'], 'Native focus report did not pass'
        assert set(observed['checks']) == REQUIRED_CHECKS, 'Missing or unexpected native focus control names'
        assert all(value is True for value in observed['checks'].values()), 'Native focus control failed; inspect retained report.json'
        assert observed['platform'] == 'windows'
        copy = (root / 'focus-C-copy.json').read_bytes()
        verify_capacitance_revision(original, copy, 'main', 'right', '470')
        result['independent_saved_token_audit'] = 'passed'
        verify_observations(observed['observations'], {**inputs, 'focus-C-copy.json': copy}, result['independent_observations'])
        verify_layout_roundtrips(observed['layout_roundtrips'], result['independent_layout_roundtrips'])
        for suffix in ('normal', 'focus', 'restored'):
            image = out / f'report.json.{suffix}.png'
            assert image.is_file() and image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), image
        for name, raw in inputs.items():
            assert (root / name).read_bytes() == raw, name
        assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'focus-C-copy.json'])
        assert all(path.is_file() for path in root.iterdir()), 'No resource directories may be prepared in the document root'
        assert fixture.read_bytes() == original
    except Exception as error:
        result.update(state='audit-failed', error=f'{type(error).__name__}: {error}')
        retain(run.stdout, run.stderr)
        raise
    result.update(state='independent-audits-passed', source_diagrams_captions_properties_pan_geometry_and_layout='passed',
        unchanged_inputs_and_no_resources='passed')
    retain(run.stdout, run.stderr)
    print(f'PASS {len(REQUIRED_CHECKS)} transient focus controls, 12 independent source/view snapshots, '
          '22 captions/9 Properties blocks, five panel round trips, one saved-token audit, three PNGs '
          'and unchanged inputs without resources')


if __name__ == '__main__':
    main()
