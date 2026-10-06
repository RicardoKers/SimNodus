# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Explicit double-click editing against independent declarations and bytes."""
import argparse
import json
import os
from pathlib import Path
import subprocess

from editor_keyboard_acceptance import (
    canonical_components,
    canonical_nets,
    expected_caption,
    expected_diagram,
    prepare_inputs,
    verify_view,
)
from native_capacitance_regression import verify_revision as verify_capacitance_revision
from native_resistance_regression import verify_revision as verify_resistance_revision

REQUIRED_CHECKS = {
    'empty_double_click_inert',
    'inert_open_no_activation',
    'single_click_inspection_only',
    'R_double_click_existing_field',
    'repeat_R_preserves_four_drafts',
    'C_double_click_separate_field',
    'modified_other_blank_no_activation',
    'middle_drag_double_click_ignored',
    'zoom_pan_hit_full_ids',
    'pending_target_change_refused',
    'left_C_missing_literal_refused',
    'focus_double_click_restores_panels',
    'closed_properties_explicitly_reopened',
    'independent_analyzer_retained',
    'C_apply_R_pending_refusals',
    'C_history_then_copy',
    'R_apply_copy_preserves_C',
    'failed_open_reopen_resets',
    'unsupported_double_click_inert',
    'resources_source_unchanged',
}

# Input/context/zoom/pan, selected full path, source edit target, R/C drafts,
# visible activation, panel focus, retained activation targets, name drafts,
# keyboard owner and selected field text; no ignored runtime contract file.
STAGES = {
    'original': ('original.json', ['main', 'right'], 100, [0, 0], [], [], '', '', False, False, False, [], [], '@input', '', 'canvas', ''),
    'inspected_R': ('original.json', ['main', 'right'], 100, [0, 0], ['main', 'right', 'r'], [], '', '', False, False, False, [], [], '@input', '', 'canvas', ''),
    'active_R': ('original.json', ['main', 'right'], 100, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '2.2', '220', True, False, False, ['main', 'right'], [], '@input', 'right', 'resistance', '2.2'),
    'draft_R': ('original.json', ['main', 'right'], 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False, ['main', 'right'], [], 'Pending project', 'Pending right', 'resistance', '3.5'),
    'active_C': ('original.json', ['main', 'right'], 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False, ['main', 'right'], ['main', 'right'], 'Pending project', 'Pending right', 'capacitance', '470'),
    'left_refused': ('original.json', ['main', 'left'], 125, [35, 20], ['main', 'left', 'r'], ['main', 'right'], '3.5', '470', False, False, False, ['main', 'right'], ['main', 'right'], 'Pending project', 'Pending right', 'canvas', ''),
    'left_uneditable_C': ('original.json', ['main', 'left'], 100, [0, 0], ['main', 'left', 'c'], [], '', '', False, False, False, [], [], '@input', '', 'canvas', ''),
    'focused_R_restored': ('original.json', ['main', 'right'], 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False, ['main', 'right'], ['main', 'right'], 'Pending project', 'Pending right', 'resistance', '3.5'),
    'C_applied': ('double-C-copy.json', ['main', 'right'], 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False, ['main', 'right'], ['main', 'right'], '@input', 'right', 'canvas', ''),
    'R_and_C_applied': ('double-RC-copy.json', ['main', 'right'], 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False, ['main', 'right'], ['main', 'right'], '@input', 'right', 'canvas', ''),
    'unsupported': ('changed-net.json', ['main', 'right'], 100, [0, 0], [], [], '', '', False, False, False, [], [], '@input', '', 'canvas', ''),
}


def verify_observations(observations, inputs, audit):
    assert [row['stage'] for row in observations] == list(STAGES), 'Missing, duplicate or reordered double-click stages'
    component_count = properties_count = 0
    for observation in observations:
        stage = observation['stage']
        (filename, context, percent, pan, selected, target, r_draft, c_draft, r_active, c_active,
         focused, r_activation, c_activation, project_draft, instance_draft, focus_owner, selected_text) = STAGES[stage]
        if project_draft == '@input':
            project_draft = json.loads(inputs[filename])['name']
        row_audit = dict(stage=stage, expected_context=context, expected_project_draft=project_draft,
            expected_instance_draft=instance_draft, expected_r_activation=r_activation,
            expected_c_activation=c_activation, expected_focus_owner=focus_owner, expected_selected_text=selected_text)
        audit.append(row_audit)
        assert observation['input'] == filename and observation['edit_target'] == target, f'{stage}: wrong input or edit target'
        assert observation['r_draft'] == r_draft and observation['c_draft'] == c_draft, f'{stage}: changed R/C draft'
        assert observation['project_draft'] == project_draft and observation['instance_draft'] == instance_draft, f'{stage}: changed project/instance draft'
        assert observation['r_activation'] == r_activation and observation['c_activation'] == c_activation, f'{stage}: changed retained activation target'
        assert observation['r_active'] is r_active and observation['c_active'] is c_active, f'{stage}: wrong visible R/C activation'
        assert observation['focused'] is focused and observation['focus_owner'] == focus_owner, f'{stage}: wrong panel/keyboard focus'
        assert observation['selected_text'] == selected_text, f'{stage}: wrong selected field text'
        canvas = observation['canvas']
        assert canvas['context'] == context and canvas['selected_path'] == selected, f'{stage}: wrong full selection IDs'
        supported = stage != 'unsupported'
        assert canvas['supported'] is supported, f'{stage}: wrong supported state'
        verify_view(canvas['view'], percent, pan, False, supported, stage, row_audit)
        properties = observation['properties_text']
        assert type(properties) is str, f'{stage}: Properties text missing'
        if not supported:
            assert canvas['components'] == [] and canvas['nets'] == [], f'{stage}: unsupported diagram retained components or nets'
        else:
            components, nets = expected_diagram(inputs[filename], context)
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
    assert component_count == 20 and properties_count == 9, 'Wrong independent caption/Properties audit counts'


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
    command = [str(Path(args.editor).resolve()), '--double-click-acceptance-root', str(root), '--report', str(out / 'report.json')]
    result = dict(command=command, state='launch-requested', independent_observations=[])

    def retain(stdout, stderr):
        (out / 'stdout.log').write_bytes(stdout)
        (out / 'stderr.log').write_bytes(stderr)
        (out / 'runner.json').write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8', newline='\n')

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
        assert run.returncode == 0, 'Native double-click acceptance failed; inspect retained report.json and raw stdout/stderr'
        observed = json.loads((out / 'report.json').read_bytes())
        assert observed['passed'] is True and observed['checks'], 'Native double-click report did not pass'
        assert set(observed['checks']) == REQUIRED_CHECKS, 'Missing or unexpected native double-click control names'
        assert all(value is True for value in observed['checks'].values()), 'Native double-click control failed; inspect retained report.json'
        assert observed['platform'] == 'windows'
        c_copy = (root / 'double-C-copy.json').read_bytes()
        combined = (root / 'double-RC-copy.json').read_bytes()
        verify_capacitance_revision(original, c_copy, 'main', 'right', '470')
        verify_resistance_revision(c_copy, combined, 'main', 'right', '3.5')
        result['two_independent_saved_token_audits'] = 'passed'
        verify_observations(observed['observations'], {**inputs, 'double-C-copy.json': c_copy,
            'double-RC-copy.json': combined}, result['independent_observations'])
        for suffix in ('R', 'C', 'restored'):
            image = out / f'report.json.{suffix}.png'
            assert image.is_file() and image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), image
        for name, raw in inputs.items():
            assert (root / name).read_bytes() == raw, name
        assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'double-C-copy.json', 'double-RC-copy.json'])
        assert all(path.is_file() for path in root.iterdir()), 'No resource directories may be prepared in the document root'
        assert fixture.read_bytes() == original
    except Exception as error:
        result.update(state='audit-failed', error=f'{type(error).__name__}: {error}')
        retain(run.stdout, run.stderr)
        raise
    result.update(state='independent-audits-passed', source_diagrams_captions_properties_pan_geometry_focus_and_selected_text='passed',
        unchanged_inputs_and_no_resources='passed')
    retain(run.stdout, run.stderr)
    print(f'PASS {len(REQUIRED_CHECKS)} bounded double-click controls, 11 independent source/view snapshots, '
          '20 captions/9 Properties blocks, two saved-token audits, three PNGs '
          'and unchanged inputs without resources')


if __name__ == '__main__':
    main()
