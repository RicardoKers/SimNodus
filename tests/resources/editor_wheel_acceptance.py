# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Bounded wheel zoom against independent declarations, geometry and bytes."""
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
from native_resistance_regression import verify_revision as verify_resistance_revision

REQUIRED_CHECKS = {
    'empty_wheel_ignored_no_view_change',
    'inert_open_centered',
    'positive_detent_updates_buttons_percentage',
    'repeated_detent_upper_bound_and_R_hit',
    'full_burst_limited_lower_bound',
    'partial_and_zero_deltas_ignored',
    'horizontal_pixel_inverted_phase_modified_buttons_ignored',
    'negative_detent_changes_one_step',
    'pan_then_wheel_keeps_offset_drafts_selection',
    'wheel_stops_middle_drag_and_updates_navigation',
    'boundary_wheel_cancels_drag_without_view_edit',
    'buttons_and_fit_interoperate_with_wheel',
    'resize_details_context_preserve_view_refuse_stale_apply',
    'C_apply_via_wheel_preserves_R_draft',
    'pending_R_refuses_copy_history',
    'R_apply_via_wheel_preserves_C',
    'undo_retains_wheel_zoom_pan',
    'redo_retains_wheel_zoom_pan',
    'create_only_occupied_copy_retains_view',
    'failed_open_retains_view_drafts',
    'reopen_resets_zoom_pan_selection_activation_history',
    'unsupported_wheel_ignored_reset_view',
    'source_resources_remain_inert',
    'wheel_hits_R_C_with_same_geometry',
    'final_fit_independent_windows_panels_images',
}

# filename, percent, scene pan, selected path, edit target, R/C drafts,
# R/C activation, and drag state; no runtime dependency on the ignored gate.
STAGES = {
    'original': ('original.json', 100, [0, 0], [], [], '', '', False, False, False),
    'wheel125': ('original.json', 125, [0, 0], [], [], '', '', False, False, False),
    'wheel150_R': ('original.json', 150, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'wheel50_R': ('original.json', 50, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'wheel125_C': ('original.json', 125, [0, 0], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False),
    'pan125_C': ('original.json', 125, [40, -20], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False),
    'wheel150_C': ('original.json', 150, [50, -10], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False),
    'boundary150_C': ('original.json', 150, [45, -5], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False),
    'retained125_R': ('original.json', 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'C_applied': ('wheel-C-copy.json', 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False),
    'R_applied': ('wheel-RC-copy.json', 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'undo_R': ('wheel-C-copy.json', 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '2.2', '470', True, False, False),
    'redo_R': ('wheel-RC-copy.json', 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'reopened': ('wheel-RC-copy.json', 100, [0, 0], [], [], '', '', False, False, False),
    'unsupported': ('changed-net.json', 100, [0, 0], [], [], '', '', False, False, False),
    'final_fit_R': ('wheel-RC-copy.json', 100, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
}


def verify_observations(observations, inputs, audit):
    assert [row['stage'] for row in observations] == list(STAGES), 'Missing, duplicate or reordered wheel stages'
    component_count = properties_count = 0
    for observation in observations:
        stage = observation['stage']
        filename, percent, pan, selected, target, r_draft, c_draft, r_active, c_active, panning = STAGES[stage]
        row_audit = dict(stage=stage)
        audit.append(row_audit)
        assert observation['input'] == filename and observation['edit_target'] == target, f'{stage}: wrong input or edit target'
        assert observation['r_draft'] == r_draft and observation['c_draft'] == c_draft, f'{stage}: changed R/C draft'
        assert observation['r_active'] is r_active and observation['c_active'] is c_active, f'{stage}: wrong R/C activation'
        canvas = observation['canvas']
        assert canvas['context'] == ['main', 'right'] and canvas['selected_path'] == selected, f'{stage}: wrong full selection IDs'
        supported = stage != 'unsupported'
        assert canvas['supported'] is supported, f'{stage}: wrong supported state'
        verify_view(canvas['view'], percent, pan, panning, supported, stage, row_audit)
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
    assert component_count == 30 and properties_count == 12, 'Wrong independent caption/Properties audit counts'


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
    command = [str(Path(args.editor).resolve()), '--wheel-acceptance-root', str(root), '--report', str(out / 'report.json')]
    result = dict(command=command, state='launch-requested', independent_observations=[])

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
        assert run.returncode == 0, 'Native wheel acceptance failed; inspect retained report.json and raw stdout/stderr'
        observed = json.loads((out / 'report.json').read_bytes())
        assert observed['passed'] is True and observed['checks'], 'Native wheel report did not pass'
        assert set(observed['checks']) == REQUIRED_CHECKS, 'Missing or unexpected native wheel control names'
        assert all(value is True for value in observed['checks'].values()), 'Native wheel control failed; inspect retained report.json'
        assert observed['platform'] == 'windows'
        c_copy = (root / 'wheel-C-copy.json').read_bytes()
        combined = (root / 'wheel-RC-copy.json').read_bytes()
        verify_capacitance_revision(original, c_copy, 'main', 'right', '470')
        verify_resistance_revision(c_copy, combined, 'main', 'right', '3.5')
        result['two_independent_saved_token_audits'] = 'passed'
        verify_observations(observed['observations'], {**inputs, 'wheel-C-copy.json': c_copy,
            'wheel-RC-copy.json': combined}, result['independent_observations'])
        for suffix in ('small', 'large', 'fit'):
            image = out / f'report.json.{suffix}.png'
            assert image.is_file() and image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), image
        for name, raw in inputs.items():
            assert (root / name).read_bytes() == raw, name
        assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'wheel-C-copy.json', 'wheel-RC-copy.json'])
        assert all(path.is_file() for path in root.iterdir()), 'No resource directories may be prepared in the document root'
        assert fixture.read_bytes() == original
    except Exception as error:
        result.update(state='audit-failed', error=f'{type(error).__name__}: {error}')
        retain(run.stdout, run.stderr)
        raise
    result.update(state='independent-audits-passed', source_diagrams_captions_properties_pan_and_geometry='passed',
        unchanged_inputs_and_no_resources='passed')
    retain(run.stdout, run.stderr)
    print(f'PASS {len(REQUIRED_CHECKS)} bounded wheel controls, 16 independent source/view snapshots, '
          '30 captions/12 Properties blocks, two saved-token audits, three PNGs and unchanged inputs without resources')


if __name__ == '__main__':
    main()
