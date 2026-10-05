# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Bounded RC zoom with independent geometry, declarations and saved bytes."""
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
    'empty_controls_disabled_and_fit_default',
    'inert_open_fit_without_selection_or_edits',
    'zoom_125_preserves_graph_and_selection',
    'zoom_150_upper_limit_and_R_hit',
    'C_hit_at_150_preserves_R_draft',
    'zoom_retains_four_drafts_and_independent_editors',
    'fit_and_details_transfer_keep_active_C_and_drafts',
    'zoom_50_lower_limit_and_independent_R_hit',
    'resize_at_50_retains_scale_and_C_hit',
    'context_switch_retains_zoom_and_refuses_stale_apply',
    'return_right_recovers_R_draft',
    'C_apply_at_125_preserves_R_draft_and_zoom',
    'pending_R_refuses_history_and_copy',
    'R_apply_at_125_preserves_C_and_zoom',
    'native_undo_at_150_retains_view_and_C',
    'native_redo_at_150_retains_view_and_R',
    'create_only_copy_keeps_zoom_and_association',
    'occupied_copy_refusal_retains_zoom',
    'failed_open_retains_zoom_and_drafts',
    'copy_open_resets_fit_selection_activation_history',
    'unsupported_shape_clears_zoom_and_disables_controls',
    'left_context_can_zoom_inspect_and_activate_independent_R',
    'zoom_actions_do_not_access_resources',
    'fit_restores_full_default_view_without_edit',
    'independent_windows_panels_and_final_image',
}

STAGES = {
    'original': ('original.json', 100, [], [], '', ''),
    'zoom125': ('original.json', 125, [], [], '', ''),
    'zoom150_R': ('original.json', 150, ['main', 'right', 'r'], ['main', 'right'], '3.5', '470'),
    'zoom150_C': ('original.json', 150, ['main', 'right', 'c'], ['main', 'right'], '3.5', '470'),
    'fit_C': ('original.json', 100, ['main', 'right', 'c'], ['main', 'right'], '3.5', '470'),
    'zoom50_R': ('original.json', 50, ['main', 'right', 'r'], ['main', 'right'], '3.5', '470'),
    'resize50_C': ('original.json', 50, ['main', 'right', 'c'], ['main', 'right'], '3.5', '470'),
    'left50_R': ('original.json', 50, ['main', 'left', 'r'], ['main', 'right'], '3.5', '470'),
    'C_applied': ('zoom-C-copy.json', 125, ['main', 'right', 'c'], ['main', 'right'], '3.5', '470'),
    'R_applied': ('zoom-RC-copy.json', 125, ['main', 'right', 'r'], ['main', 'right'], '3.5', '470'),
    'undo150_R': ('zoom-C-copy.json', 150, ['main', 'right', 'r'], ['main', 'right'], '2.2', '470'),
    'redo150_R': ('zoom-RC-copy.json', 150, ['main', 'right', 'r'], ['main', 'right'], '3.5', '470'),
    'reopened': ('zoom-RC-copy.json', 100, [], [], '', ''),
    'unsupported': ('changed-net.json', 100, [], [], '', ''),
    'left125_R': ('zoom-RC-copy.json', 125, ['main', 'left', 'r'], ['main', 'left'], '1', ''),
    'final_fit_R': ('zoom-RC-copy.json', 100, ['main', 'right', 'r'], ['main', 'right'], '3.5', '470'),
}
R_ACTIVE = {'zoom150_R', 'zoom50_R', 'R_applied', 'undo150_R', 'redo150_R', 'left125_R', 'final_fit_R'}
C_ACTIVE = {'zoom150_C', 'fit_C', 'resize50_C', 'C_applied'}


def verify_view(view, percent, supported, audit):
    width, height = view['width'], view['height']
    assert type(width) is int and type(height) is int and width > 0 and height > 0, view
    assert type(view['zoom_percent']) is int and view['zoom_percent'] == percent, view
    # This derives the fit from widget dimensions, not the reported rectangle.
    scale = min((width - 24) / 700, (height - 24) / 420) * percent / 100
    assert scale > 0, view
    expected = [(width - 700 * scale) / 2, (height - 420 * scale) / 2, 700 * scale, 420 * scale]
    rectangle = view['rectangle']
    assert len(rectangle) == 4, view
    audit['expected_rectangle'] = expected
    audit['actual_rectangle'] = rectangle
    for actual, wanted in zip(rectangle, expected):
        assert type(actual) in (int, float) and math.isfinite(actual) and abs(actual - wanted) <= 1e-6, view
    points = view['component_points']
    audit['points'] = {}
    if not supported:
        assert points == {}, 'Unsupported diagrams must not report selectable component points'
        return
    assert set(points) == {'r', 'c'}, view
    for component, center in (('r', (255, 190)), ('c', (445, 280))):
        # QPoint rounds the transformed literal center to the nearest pixel.
        wanted = [math.floor(expected[0] + center[0] * scale + 0.5),
                  math.floor(expected[1] + center[1] * scale + 0.5)]
        actual = points[component]
        audit['points'][component] = dict(expected=wanted, actual=actual)
        assert len(actual) == 2 and all(type(value) is int for value in actual), view
        assert all(abs(value - target) <= 1 for value, target in zip(actual, wanted)), view


def verify_observations(observations, inputs, audit):
    assert [row['stage'] for row in observations] == list(STAGES), 'Missing, duplicate or reordered zoom stages'
    component_count = properties_count = 0
    for observation in observations:
        stage = observation['stage']
        filename, percent, selected, target, r_draft, c_draft = STAGES[stage]
        row_audit = dict(stage=stage)
        audit.append(row_audit)
        assert observation['input'] == filename and observation['edit_target'] == target, observation
        assert observation['r_draft'] == r_draft and observation['c_draft'] == c_draft, observation
        assert observation['r_active'] is (stage in R_ACTIVE), observation
        assert observation['c_active'] is (stage in C_ACTIVE), observation
        canvas = observation['canvas']
        context = ['main', 'left' if stage in ('left50_R', 'left125_R') else 'right']
        assert canvas['context'] == context and canvas['selected_path'] == selected, observation
        supported = stage != 'unsupported'
        assert canvas['supported'] is supported, observation
        verify_view(canvas['view'], percent, supported, row_audit)
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
    assert component_count == 30 and properties_count == 12


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
    command = [str(Path(args.editor).resolve()), '--zoom-acceptance-root', str(root), '--report', str(out / 'report.json')]
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
        assert run.returncode == 0, run.stdout.decode(errors='replace') + run.stderr.decode(errors='replace')
        observed = json.loads((out / 'report.json').read_bytes())
        assert observed['passed'] is True and observed['checks']
        assert set(observed['checks']) == REQUIRED_CHECKS, observed['checks']
        assert all(value is True for value in observed['checks'].values()), observed['checks']
        assert observed['platform'] == 'windows'
        c_copy = (root / 'zoom-C-copy.json').read_bytes()
        combined = (root / 'zoom-RC-copy.json').read_bytes()
        verify_capacitance_revision(original, c_copy, 'main', 'right', '470')
        verify_resistance_revision(c_copy, combined, 'main', 'right', '3.5')
        result['two_independent_saved_token_audits'] = 'passed'
        verify_observations(observed['observations'], {**inputs, 'zoom-C-copy.json': c_copy,
            'zoom-RC-copy.json': combined}, result['independent_observations'])
        for suffix in ('fit', 'zoom150', 'final'):
            image = out / f'report.json.{suffix}.png'
            assert image.is_file() and image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), image
        for name, raw in inputs.items():
            assert (root / name).read_bytes() == raw, name
        assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'zoom-C-copy.json', 'zoom-RC-copy.json'])
        assert all(path.is_file() for path in root.iterdir()), 'No resource directories may be prepared in the document root'
        assert fixture.read_bytes() == original
    except Exception as error:
        result.update(state='audit-failed', error=f'{type(error).__name__}: {error}')
        retain(run.stdout, run.stderr)
        raise
    result.update(state='independent-audits-passed', source_diagrams_captions_properties_and_geometry='passed',
        unchanged_inputs_and_no_resources='passed')
    retain(run.stdout, run.stderr)
    print(f'PASS {len(REQUIRED_CHECKS)} bounded zoom controls, 16 independent source/view snapshots, '
          '30 captions/12 Properties blocks, two saved-token audits, three PNGs and unchanged inputs without resources')


if __name__ == '__main__':
    main()
