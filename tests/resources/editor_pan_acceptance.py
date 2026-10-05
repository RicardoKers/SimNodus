# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Bounded RC pan with independent geometry, declarations and saved bytes."""
import argparse
import json
import math
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
from native_capacitance_regression import verify_revision as verify_capacitance_revision
from native_resistance_regression import verify_revision as verify_resistance_revision

REQUIRED_CHECKS = {
    'empty_middle_refuses_and_cursor_unchanged',
    'inert_open_centered',
    'zoom150_R_activation_and_drafts',
    'middle_press_preserves_selection',
    'middle_move_updates_only_view',
    'left_press_during_middle_drag_does_not_select_C',
    'release_stops_and_cursor_restores',
    'moves_without_drag_and_right_press_inert',
    'upper_pan_bounds_and_C_hit',
    'lower_pan_bounds_and_R_hit',
    'lost_middle_button_cancels_view_only',
    'ungrab_and_deactivate_cancel_view_only',
    'fit_cancels_resets_and_preserves_drafts',
    'zoom_cancels_and_retains_pan_and_drafts',
    'resize_cancels_retains_pan_and_C_hit',
    'details_hide_cancels_retains_pan_fields',
    'context_retains_pan_refuses_stale_apply',
    'C_apply_retains_pan_R_draft',
    'pending_R_refuses_history_copy',
    'R_apply_retains_pan_C',
    'native_undo_retains_pan_C',
    'native_redo_retains_pan_R',
    'create_only_and_occupied_copy_keep_pan',
    'failed_open_keeps_pan_drafts',
    'copy_open_resets_pan_zoom_selection_activations_history',
    'unsupported_disables_pan_resets',
    'pan_reaches_original_input_reference_and_output',
    'final_fit_windows_panels_resources',
}

# filename, percent, scene pan, selected path, edit target, R/C drafts,
# R/C activation, and drag state; these are fixed before the native run.
STAGES = {
    'original': ('original.json', 100, [0, 0], [], [], '', '', False, False, False),
    'start_R': ('original.json', 150, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'press_R': ('original.json', 150, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, True),
    'drag_R': ('original.json', 150, [40, -25], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, True),
    'released_R': ('original.json', 150, [40, -25], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'max_C': ('original.json', 150, [140, 84], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False),
    'min_R': ('original.json', 150, [-140, -84], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'lost_button_R': ('original.json', 150, [-130, -74], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'ungrab_R': ('original.json', 150, [-120, -64], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'fit_R': ('original.json', 100, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'zoom_R': ('original.json', 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'resize_C': ('original.json', 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False),
    'details_C': ('original.json', 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False),
    'left_R': ('original.json', 125, [35, 20], ['main', 'left', 'r'], ['main', 'right'], '3.5', '470', False, False, False),
    'C_applied': ('pan-C-copy.json', 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False),
    'R_applied': ('pan-RC-copy.json', 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'undo_R': ('pan-C-copy.json', 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '2.2', '470', True, False, False),
    'redo_R': ('pan-RC-copy.json', 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'reopened': ('pan-RC-copy.json', 100, [0, 0], [], [], '', '', False, False, False),
    'unsupported': ('changed-net.json', 100, [0, 0], [], [], '', '', False, False, False),
    'pan_input_R': ('pan-RC-copy.json', 150, [90, 30], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'pan_output_R': ('pan-RC-copy.json', 150, [-90, 30], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
    'final_fit_R': ('pan-RC-copy.json', 100, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False),
}


def nearest_pixel(value):
    return math.floor(value + 0.5) if value >= 0 else math.ceil(value - 0.5)


def verify_view(view, percent, pan, panning, supported, stage, audit):
    width, height = view['width'], view['height']
    assert type(width) is int and type(height) is int and width > 0 and height > 0, view
    assert type(view['zoom_percent']) is int and view['zoom_percent'] == percent, view
    assert view['panning'] is panning, view
    actual_pan = view['pan']
    assert len(actual_pan) == 2, view
    for actual, wanted, limit in zip(actual_pan, pan, (140, 84)):
        assert type(actual) in (int, float) and math.isfinite(actual), view
        assert abs(actual - wanted) <= 1e-6 and abs(actual) <= limit + 1e-6, view
    # Expected geometry uses measured dimensions and the predetermined scene pan.
    scale = min((width - 24) / 700, (height - 24) / 420) * percent / 100
    assert scale > 0, view
    expected = [(width - 700 * scale) / 2 + pan[0] * scale,
                (height - 420 * scale) / 2 + pan[1] * scale, 700 * scale, 420 * scale]
    audit.update(expected_pan=pan, actual_pan=actual_pan, expected_rectangle=expected,
        actual_rectangle=view['rectangle'], expected_panning=panning, actual_panning=view['panning'])
    rectangle = view['rectangle']
    assert len(rectangle) == 4, view
    for actual, wanted in zip(rectangle, expected):
        assert type(actual) in (int, float) and math.isfinite(actual) and abs(actual - wanted) <= 1e-6, view
    points = view['component_points']
    audit['points'] = {}
    if not supported:
        assert points == {}, 'Unsupported diagrams must not report selectable component points'
        return
    assert set(points) == {'r', 'c'}, view
    for component, center in (('r', (255, 190)), ('c', (445, 280))):
        wanted = [nearest_pixel(expected[0] + center[0] * scale), nearest_pixel(expected[1] + center[1] * scale)]
        actual = points[component]
        audit['points'][component] = dict(expected=wanted, actual=actual)
        assert len(actual) == 2 and all(type(value) is int for value in actual), view
        assert all(abs(value - target) <= 1 for value, target in zip(actual, wanted)), view
    if stage in ('pan_input_R', 'pan_output_R'):
        # Do not use reported rectangles or points to establish port visibility.
        ports = [('input', (70, 190)), ('reference', (70, 350))] if stage == 'pan_input_R' else [('output', (625, 190))]
        audit['visible_ports'] = []
        for name, center in ports:
            x = width / 2 + (center[0] - 350 + pan[0]) * scale
            y = height / 2 + (center[1] - 210 + pan[1]) * scale
            audit['visible_ports'].append(dict(port=name, expected_point=[x, y], widget=[width, height]))
            assert 0 <= x < width and 0 <= y < height, f'{stage}: original {name} point is outside the viewport'


def verify_observations(observations, inputs, audit):
    assert [row['stage'] for row in observations] == list(STAGES), 'Missing, duplicate or reordered pan stages'
    component_count = properties_count = 0
    for observation in observations:
        stage = observation['stage']
        filename, percent, pan, selected, target, r_draft, c_draft, r_active, c_active, panning = STAGES[stage]
        row_audit = dict(stage=stage)
        audit.append(row_audit)
        assert observation['input'] == filename and observation['edit_target'] == target, observation
        assert observation['r_draft'] == r_draft and observation['c_draft'] == c_draft, observation
        assert observation['r_active'] is r_active and observation['c_active'] is c_active, observation
        canvas = observation['canvas']
        context = ['main', 'left' if stage == 'left_R' else 'right']
        assert canvas['context'] == context and canvas['selected_path'] == selected, observation
        supported = stage != 'unsupported'
        assert canvas['supported'] is supported, observation
        verify_view(canvas['view'], percent, pan, panning, supported, stage, row_audit)
        properties = observation['properties_text']
        assert type(properties) is str, observation
        if not supported:
            assert canvas['components'] == [] and canvas['nets'] == [], observation
        else:
            components, nets = expected_diagram(inputs[filename], context)
            assert canonical_components(canvas['components']) == canonical_components(components), observation
            assert canonical_nets(canvas['nets']) == canonical_nets(nets), observation
            row_audit['captions'] = []
            for component in canvas['components']:
                parameter_id = 'resistance' if component['component'] == 'resistor' else 'capacitance'
                parameter = component['parameters'][parameter_id]
                caption = expected_caption(parameter['value'], parameter['unit'])
                row_audit['captions'].append(dict(path=component['path'], expected=caption, actual=component['caption']))
                assert component['caption'] == caption, component
                component_count += 1
        if not selected:
            assert 'Applied ' not in properties and 'Base value:' not in properties, observation
            continue
        matching = [component for component in canvas['components'] if component['path'] == selected]
        assert len(matching) == 1, observation
        component = matching[0]
        parameter_id = 'resistance' if component['component'] == 'resistor' else 'capacitance'
        parameter = component['parameters'][parameter_id]
        caption = expected_caption(parameter['value'], parameter['unit'])
        base = 'Base value: ' + parameter['value'] + ' ' + parameter['unit']
        block = f'Applied {parameter_id}:\n{caption}\n{base}'
        row_audit['properties'] = dict(expected=block, actual=properties)
        assert block in properties, observation
        assert [line for line in properties.splitlines() if line.startswith('Base value:')] == [base], observation
        properties_count += 1
    assert component_count == 44 and properties_count == 20


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
    command = [str(Path(args.editor).resolve()), '--pan-acceptance-root', str(root), '--report', str(out / 'report.json')]
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
        assert run.returncode == 0, 'Native pan acceptance failed; inspect retained report.json and raw stdout/stderr'
        observed = json.loads((out / 'report.json').read_bytes())
        assert observed['passed'] is True and observed['checks']
        assert set(observed['checks']) == REQUIRED_CHECKS, observed['checks']
        assert all(value is True for value in observed['checks'].values()), observed['checks']
        assert observed['platform'] == 'windows'
        c_copy = (root / 'pan-C-copy.json').read_bytes()
        combined = (root / 'pan-RC-copy.json').read_bytes()
        verify_capacitance_revision(original, c_copy, 'main', 'right', '470')
        verify_resistance_revision(c_copy, combined, 'main', 'right', '3.5')
        result['two_independent_saved_token_audits'] = 'passed'
        verify_observations(observed['observations'], {**inputs, 'pan-C-copy.json': c_copy,
            'pan-RC-copy.json': combined}, result['independent_observations'])
        for suffix in ('pan-input', 'pan-output', 'fit'):
            image = out / f'report.json.{suffix}.png'
            assert image.is_file() and image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), image
        for name, raw in inputs.items():
            assert (root / name).read_bytes() == raw, name
        assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'pan-C-copy.json', 'pan-RC-copy.json'])
        assert all(path.is_file() for path in root.iterdir()), 'No resource directories may be prepared in the document root'
        assert fixture.read_bytes() == original
    except Exception as error:
        result.update(state='audit-failed', error=f'{type(error).__name__}: {error}')
        retain(run.stdout, run.stderr)
        raise
    result.update(state='independent-audits-passed', source_diagrams_captions_properties_pan_and_geometry='passed',
        independent_original_port_visibility='passed', unchanged_inputs_and_no_resources='passed')
    retain(run.stdout, run.stderr)
    print(f'PASS {len(REQUIRED_CHECKS)} bounded pan controls, 23 independent source/view snapshots, '
          '44 captions/20 Properties blocks, two saved-token audits, original port visibility, '
          'three PNGs and unchanged inputs without resources')


if __name__ == '__main__':
    main()
