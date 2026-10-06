# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Selected R/C Enter activation with independent source, routing and bytes."""
import argparse
import hashlib
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
from native_resistance_regression import name_span, verify_revision as verify_resistance_revision

REQUIRED_CHECKS = {
    'empty_and_unselected_inert',
    'inert_open_and_single_inspection',
    'Return_activates_existing_R',
    'Enter_activates_existing_C',
    'repeat_modified_unknown_hidden_unfocused_inert',
    'native_fields_no_implicit_apply',
    'zoom_pan_context_full_ids',
    'active_middle_drag_refuses',
    'four_drafts_targets_retained',
    'pending_left_R_and_missing_left_C_refuse',
    'focus_restore_and_closed_properties_reopen',
    'explicit_apply_history_create_only_copy',
    'failed_open_reopen_unsupported_resources_inert',
}

# Input/context/zoom/pan, full selected path, source target, R/C drafts,
# visible activation, panel focus, retained targets, names and keyboard field.
STAGES = {
    'original': ('original.json', ['main', 'right'], 100, [0, 0], [], [], '', '', False, False, False, [], [], '@input', '', 'canvas', ''),
    'active_R': ('original.json', ['main', 'right'], 100, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '2.2', '220', True, False, False, ['main', 'right'], [], '@input', 'right', 'resistance', '2.2'),
    'active_C': ('original.json', ['main', 'right'], 100, [0, 0], ['main', 'right', 'c'], ['main', 'right'], '2.2', '220', False, True, False, ['main', 'right'], ['main', 'right'], '@input', 'right', 'capacitance', '220'),
    'draft_R': ('original.json', ['main', 'right'], 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False, ['main', 'right'], ['main', 'right'], 'Pending project', 'Pending right', 'resistance', '3.5'),
    'left_refused': ('original.json', ['main', 'left'], 125, [35, 20], ['main', 'left', 'r'], ['main', 'right'], '3.5', '470', False, False, False, ['main', 'right'], ['main', 'right'], 'Pending project', 'Pending right', 'canvas', ''),
    'left_uneditable_C': ('original.json', ['main', 'left'], 100, [0, 0], ['main', 'left', 'c'], [], '', '', False, False, False, [], [], '@input', '', 'canvas', ''),
    'restored_C': ('original.json', ['main', 'right'], 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, True, False, ['main', 'right'], ['main', 'right'], 'Pending project', 'Pending right', 'capacitance', '470'),
    'combined_applied': ('enter-RC-copy.json', ['main', 'right'], 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False, ['main', 'right'], ['main', 'right'], '@input', 'right', 'canvas', ''),
    'unsupported': ('changed-net.json', ['main', 'right'], 100, [0, 0], [], [], '', '', False, False, False, [], [], '@input', '', 'canvas', ''),
}

RETURN, ENTER, END = 0x01000004, 0x01000005, 0x01000011
CTRL, SHIFT, KEYPAD = 0x04000000, 0x02000000, 0x20000000
# Key/modifiers/repeat/zoom, actual initial focus, final focus, accepted.
# None means key consumption does not establish native edit success.
ROUTED_CASES = {
    'empty_Return': (RETURN, 0, False, 100, 'canvas', 'canvas', False),
    'empty_Enter': (ENTER, 0, False, 100, 'canvas', 'canvas', False),
    'unselected_Return': (RETURN, 0, False, 100, 'canvas', 'canvas', False),
    'selected_R_Return': (RETURN, 0, False, 100, 'canvas', 'resistance', True),
    'field_R_Return': (RETURN, 0, False, 100, 'resistance', 'resistance', None),
    'selected_C_Enter': (ENTER, 0, False, 100, 'canvas', 'capacitance', True),
    'field_C_Enter': (ENTER, 0, False, 100, 'capacitance', 'capacitance', None),
    'repeat_Return': (RETURN, 0, True, 100, 'canvas', 'canvas', False),
    'repeat_Enter': (ENTER, 0, True, 100, 'canvas', 'canvas', False),
    'ctrl_Return': (RETURN, CTRL, False, 100, 'canvas', 'canvas', False),
    'shift_Enter': (ENTER, SHIFT, False, 100, 'canvas', 'canvas', False),
    'keypad_Enter': (ENTER, KEYPAD, False, 100, 'canvas', 'canvas', False),
    'unknown_End': (END, 0, False, 100, 'canvas', 'canvas', False),
    'unfocused_canvas_Return': (RETURN, 0, False, 100, 'capacitance', 'capacitance', False),
    'hidden_canvas_Return': (RETURN, 0, False, 100, 'capacitance', 'capacitance', False),
    'draft_R_Return': (RETURN, 0, False, 125, 'canvas', 'resistance', True),
    'panning_Return': (RETURN, 0, False, 125, 'canvas', 'canvas', False),
    'left_R_Return': (RETURN, 0, False, 125, 'canvas', 'canvas', None),
    'left_C_Enter': (ENTER, 0, False, 100, 'canvas', 'canvas', None),
    'rebuild_R_Return': (RETURN, 0, False, 100, 'canvas', 'resistance', True),
    'rebuild_C_Enter': (ENTER, 0, False, 100, 'canvas', 'capacitance', True),
    'focus_restore_Return': (RETURN, 0, False, 125, 'canvas', 'resistance', True),
    'closed_properties_Enter': (ENTER, 0, False, 125, 'canvas', 'capacitance', True),
    'R_after_C_Return': (RETURN, 0, False, 125, 'canvas', 'resistance', True),
    'field_R_pending_Return': (RETURN, 0, False, 125, 'resistance', 'resistance', None),
    'unsupported_Return': (RETURN, 0, False, 100, 'canvas', 'canvas', False),
    'unsupported_Enter': (ENTER, 0, False, 100, 'canvas', 'canvas', False),
}


def verify_routing(events, audit):
    assert type(events) is list and [row['case'] for row in events] == list(ROUTED_CASES), 'Missing, duplicate or reordered Enter routing cases'
    for row in events:
        case = row['case']
        code, modifiers, repeat, percent, before, after, accepted = ROUTED_CASES[case]
        audit.append(dict(case=case, event=row, expected_focus_before=before, expected_focus_after=after))
        assert type(row['key']) is int and row['key'] == code and type(row['modifiers']) is int and row['modifiers'] == modifiers, f'{case}: wrong key or modifiers'
        assert row['repeat'] is repeat and type(row['accepted']) is bool, f'{case}: wrong repeat/accepted evidence'
        assert type(row['zoom_before']) is int and type(row['zoom_after']) is int and row['zoom_before'] == row['zoom_after'] == percent, f'{case}: Enter changed zoom'
        assert row['focus_before'] == before and row['focus_after_press'] == row['focus_after'] == after, f'{case}: wrong real focus transition'
        assert row['release_receiver'] == after, f'{case}: release failed to follow actual focus after activation'
        direct = case in ('unfocused_canvas_Return', 'hidden_canvas_Return')
        assert row['route'] == ('direct-negative-probe' if direct else 'focus-routed'), f'{case}: undisclosed direct routing'
        assert row['press_receiver'] == ('canvas' if direct else before), f'{case}: press bypassed expected owner'
        if accepted is not None:
            assert row['accepted'] is accepted, f'{case}: key consumption mismatch'


def verify_observations(observations, inputs, audit):
    assert [row['stage'] for row in observations] == list(STAGES), 'Missing, duplicate or reordered Enter stages'
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
    assert component_count == 16 and properties_count == 7, 'Wrong independent caption/Properties audit counts'


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
    command = [str(Path(args.editor).resolve()), '--enter-acceptance-root', str(root), '--report', str(out / 'report.json')]
    result = dict(command=command, state='launch-requested', independent_observations=[], independent_routed_events=[])

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
        assert run.returncode == 0, 'Native Enter acceptance failed; inspect retained report.json and raw stdout/stderr'
        observed = json.loads((out / 'report.json').read_bytes())
        assert observed['passed'] is True and observed['checks'], 'Native Enter report did not pass'
        assert set(observed['checks']) == REQUIRED_CHECKS, 'Missing or unexpected native Enter control names'
        assert all(value is True for value in observed['checks'].values()), 'Native Enter control failed; inspect retained report.json'
        assert observed['platform'] == 'windows'
        combined = (root / 'enter-RC-copy.json').read_bytes()
        begin, end = name_span(original, 'main', 'right', ('overrides', 'capacitance', 'value'))
        assert json.loads(original[begin:end]) == '220', 'Unexpected original fixture capacitance token'
        expected_C = original[:begin] + b'"470"' + original[end:]
        verify_capacitance_revision(original, expected_C, 'main', 'right', '470')
        verify_resistance_revision(expected_C, combined, 'main', 'right', '3.5')
        result.update(two_independent_token_audits='passed', in_memory_C_revision_sha256=hashlib.sha256(expected_C).hexdigest())
        verify_observations(observed['observations'], {**inputs, 'enter-RC-copy.json': combined}, result['independent_observations'])
        verify_routing(observed['routed_events'], result['independent_routed_events'])
        for suffix in ('R', 'restored'):
            image = out / f'report.json.{suffix}.png'
            assert image.is_file() and image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), image
        for name, raw in inputs.items():
            assert (root / name).read_bytes() == raw, name
        assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'enter-RC-copy.json'])
        assert all(path.is_file() for path in root.iterdir()), 'No resource directories may be prepared in the document root'
        assert fixture.read_bytes() == original
    except Exception as error:
        result.update(state='audit-failed', error=f'{type(error).__name__}: {error}')
        retain(run.stdout, run.stderr)
        raise
    result.update(state='independent-audits-passed', source_diagrams_captions_properties_pan_geometry_focus_and_routing='passed',
        unchanged_inputs_and_no_resources='passed')
    retain(run.stdout, run.stderr)
    print(f'PASS {len(REQUIRED_CHECKS)} selected Enter controls, nine independent source/view snapshots, '
          '16 captions/7 Properties blocks, 27 press/release routing cases, two token audits/one saved copy, '
          'two PNGs and unchanged inputs without resources')


if __name__ == '__main__':
    main()
