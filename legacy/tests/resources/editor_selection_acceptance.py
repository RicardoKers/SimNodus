# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Fixed RC arrow inspection against independent routing, declarations and bytes."""
import argparse
import hashlib
import json
import math
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
from native_resistance_regression import name_span, verify_revision

REQUIRED_CHECKS = {
    'right_arrows_inspect_full_ids_no_activation',
    'single_reversal_preserves_focus',
    'modified_repeat_other_keys_inert',
    'native_R_C_arrows_cursor_only',
    'hidden_unfocused_probes_keep_field_focus',
    'zoom_pan_and_drag_refusal',
    'four_drafts_targets_left_context_retained',
    'focused_inspection_and_explicit_restore',
    'closed_properties_inspection_then_Enter_reopens',
    'explicit_R_apply_create_only_copy_and_inert_resources',
    'empty_and_unsupported_refuse',
}

# Full selection, source target, retained R/C activation targets, R/C/name
# drafts, zoom/pan, drag, Focus Circuit, panel visibility and applied dirty state.
# @input is resolved from the original declaration, never from native output.
STATE_FIELDS = (
    'selected_path', 'edit_target', 'r_activation', 'c_activation',
    'r_draft', 'c_draft', 'project_draft', 'instance_draft',
    'zoom', 'pan', 'panning', 'focused', 'components_hidden',
    'properties_hidden', 'dirty',
)
R = ['main', 'right', 'r']
C = ['main', 'right', 'c']
TARGET = ['main', 'right']
STATES = {
    'empty': ([], [], [], [], '', '', '', '', 100, [0, 0], False, False, False, False, False),
    'original': ([], [], [], [], '', '', '@input', '', 100, [0, 0], False, False, False, False, False),
    'right_R': (R, [], [], [], '', '', '@input', '', 100, [0, 0], False, False, False, False, False),
    'right_C': (C, [], [], [], '', '', '@input', '', 100, [0, 0], False, False, False, False, False),
    'active_R': (R, TARGET, TARGET, [], '2.2', '220', '@input', 'right', 100, [0, 0], False, False, False, False, False),
    'draft_R_only': (R, TARGET, TARGET, [], '3.5', '470', 'Pending project', 'Pending right', 125, [35, 20], False, False, False, False, False),
    'draft_C_only': (C, TARGET, TARGET, [], '3.5', '470', 'Pending project', 'Pending right', 125, [35, 20], False, False, False, False, False),
    'draft_C': (C, TARGET, TARGET, TARGET, '3.5', '470', 'Pending project', 'Pending right', 125, [35, 20], False, False, False, False, False),
    'draft_R': (R, TARGET, TARGET, TARGET, '3.5', '470', 'Pending project', 'Pending right', 125, [35, 20], False, False, False, False, False),
    'drag_R': (R, TARGET, TARGET, TARGET, '3.5', '470', 'Pending project', 'Pending right', 125, [35, 20], True, False, False, False, False),
    'left_unselected': ([], TARGET, TARGET, TARGET, '3.5', '470', 'Pending project', 'Pending right', 125, [35, 20], False, False, False, False, False),
    'left_R': (['main', 'left', 'r'], TARGET, TARGET, TARGET, '3.5', '470', 'Pending project', 'Pending right', 125, [35, 20], False, False, False, False, False),
    'left_C': (['main', 'left', 'c'], TARGET, TARGET, TARGET, '3.5', '470', 'Pending project', 'Pending right', 125, [35, 20], False, False, False, False, False),
    'focus_unselected': ([], TARGET, TARGET, TARGET, '3.5', '470', 'Pending project', 'Pending right', 125, [35, 20], False, True, True, True, False),
    'focus_R': (R, TARGET, TARGET, TARGET, '3.5', '470', 'Pending project', 'Pending right', 125, [35, 20], False, True, True, True, False),
    'closed_R': (R, TARGET, TARGET, TARGET, '3.5', '470', 'Pending project', 'Pending right', 125, [35, 20], False, False, False, True, False),
    'closed_C': (C, TARGET, TARGET, TARGET, '3.5', '470', 'Pending project', 'Pending right', 125, [35, 20], False, False, False, True, False),
    'applied_R': (R, TARGET, TARGET, TARGET, '3.5', '220', '@input', 'right', 125, [35, 20], False, False, False, False, True),
}

# Input, independently declared state, current closed RC context, visible R/C
# activation, actual keyboard owner and selected native field text.
STAGES = {
    'original': ('original.json', 'original', TARGET, False, False, 'canvas', ''),
    'right_R': ('original.json', 'right_R', TARGET, False, False, 'canvas', ''),
    'right_C': ('original.json', 'right_C', TARGET, False, False, 'canvas', ''),
    'drafts_R': ('original.json', 'draft_R', TARGET, True, False, 'canvas', ''),
    'left_C': ('original.json', 'left_C', ['main', 'left'], False, False, 'canvas', ''),
    'focus_R': ('original.json', 'focus_R', TARGET, False, False, 'canvas', ''),
    'restored_R': ('original.json', 'draft_R', TARGET, True, False, 'resistance', '3.5'),
    'applied_R': ('selection-R-copy.json', 'applied_R', TARGET, True, False, 'canvas', ''),
    'unsupported': ('changed-net.json', 'original', TARGET, False, False, 'canvas', ''),
}

LEFT, UP, RIGHT, DOWN = 0x01000012, 0x01000013, 0x01000014, 0x01000015
RETURN, ENTER = 0x01000004, 0x01000005
CTRL, SHIFT = 0x04000000, 0x02000000
# Key/modifiers/repeat, before/after state, actual initial/final owner, accepted,
# native cursor before/after. Every normal press and release follows real focus.
ROUTED_CASES = {
    'empty_Left': (LEFT, 0, False, 'empty', 'empty', 'canvas', 'canvas', False, -1, -1),
    'empty_Right': (RIGHT, 0, False, 'empty', 'empty', 'canvas', 'canvas', False, -1, -1),
    'right_Left': (LEFT, 0, False, 'original', 'right_R', 'canvas', 'canvas', True, -1, -1),
    'right_Right': (RIGHT, 0, False, 'right_R', 'right_C', 'canvas', 'canvas', True, -1, -1),
    'reverse_Left': (LEFT, 0, False, 'right_C', 'right_R', 'canvas', 'canvas', True, -1, -1),
    'single_Left_again': (LEFT, 0, False, 'right_R', 'right_R', 'canvas', 'canvas', True, -1, -1),
    'ctrl_Left': (LEFT, CTRL, False, 'right_R', 'right_R', 'canvas', 'canvas', False, -1, -1),
    'shift_Right': (RIGHT, SHIFT, False, 'right_R', 'right_R', 'canvas', 'canvas', False, -1, -1),
    'repeat_Right': (RIGHT, 0, True, 'right_R', 'right_R', 'canvas', 'canvas', False, -1, -1),
    'other_Up': (UP, 0, False, 'right_R', 'right_R', 'canvas', 'canvas', False, -1, -1),
    'other_Down': (DOWN, 0, False, 'right_R', 'right_R', 'canvas', 'canvas', False, -1, -1),
    'activate_R_Return': (RETURN, 0, False, 'right_R', 'active_R', 'canvas', 'resistance', True, -1, 3),
    'field_R_Left': (LEFT, 0, False, 'draft_R_only', 'draft_R_only', 'resistance', 'resistance', True, 3, 2),
    'field_R_Right': (RIGHT, 0, False, 'draft_R_only', 'draft_R_only', 'resistance', 'resistance', True, 2, 3),
    'draft_Right': (RIGHT, 0, False, 'draft_R_only', 'draft_C_only', 'canvas', 'canvas', True, -1, -1),
    'activate_C_Enter': (ENTER, 0, False, 'draft_C_only', 'draft_C', 'canvas', 'capacitance', True, -1, 3),
    'field_C_Left': (LEFT, 0, False, 'draft_C', 'draft_C', 'capacitance', 'capacitance', True, 3, 2),
    'field_C_Right': (RIGHT, 0, False, 'draft_C', 'draft_C', 'capacitance', 'capacitance', True, 2, 3),
    'unfocused_canvas_Left': (LEFT, 0, False, 'draft_C', 'draft_C', 'capacitance', 'capacitance', False, 3, 3),
    'hidden_canvas_Right': (RIGHT, 0, False, 'draft_C', 'draft_C', 'capacitance', 'capacitance', False, 3, 3),
    'draft_Left': (LEFT, 0, False, 'draft_C', 'draft_R', 'canvas', 'canvas', True, -1, -1),
    'panning_Right': (RIGHT, 0, False, 'drag_R', 'drag_R', 'canvas', 'canvas', False, -1, -1),
    'left_Left': (LEFT, 0, False, 'left_unselected', 'left_R', 'canvas', 'canvas', True, -1, -1),
    'left_Right': (RIGHT, 0, False, 'left_R', 'left_C', 'canvas', 'canvas', True, -1, -1),
    'focus_Left': (LEFT, 0, False, 'focus_unselected', 'focus_R', 'canvas', 'canvas', True, -1, -1),
    'focus_Return': (RETURN, 0, False, 'focus_R', 'draft_R', 'canvas', 'resistance', True, -1, 3),
    'closed_Right': (RIGHT, 0, False, 'closed_R', 'closed_C', 'canvas', 'canvas', True, -1, -1),
    'closed_Enter': (ENTER, 0, False, 'closed_C', 'draft_C', 'canvas', 'capacitance', True, -1, 3),
    'save_Left': (LEFT, 0, False, 'draft_C', 'draft_R', 'canvas', 'canvas', True, -1, -1),
    'save_Return': (RETURN, 0, False, 'draft_R', 'draft_R', 'canvas', 'resistance', True, -1, 3),
    'unsupported_Left': (LEFT, 0, False, 'original', 'original', 'canvas', 'canvas', False, -1, -1),
    'unsupported_Right': (RIGHT, 0, False, 'original', 'original', 'canvas', 'canvas', False, -1, -1),
}


def expected_state(name, original):
    values = dict(zip(STATE_FIELDS, STATES[name]))
    if values['project_draft'] == '@input':
        values['project_draft'] = json.loads(original)['name']
    return values


def verify_state(actual, wanted, label):
    assert type(actual) is dict, f'{label}: state evidence missing'
    for field, expected in wanted.items():
        value = actual[field]
        if field == 'pan':
            assert type(value) is list and len(value) == 2, f'{label}: invalid pan'
            assert all(type(item) in (int, float) and math.isfinite(item) and abs(item - target) <= 1e-6
                       for item, target in zip(value, expected)), f'{label}: changed pan'
        else:
            assert type(value) is type(expected) and value == expected, f'{label}: wrong {field}'


def verify_routing(events, original, audit):
    assert type(events) is list and len(events) == 32, 'Wrong selection routing count'
    assert [row['case'] for row in events] == list(ROUTED_CASES), 'Missing, duplicate or reordered selection routing cases'
    for row in events:
        case = row['case']
        (code, modifiers, repeat, before_state, after_state, before_owner,
         after_owner, accepted, cursor_before, cursor_after) = ROUTED_CASES[case]
        before = expected_state(before_state, original)
        after = expected_state(after_state, original)
        audit.append(dict(case=case, event=row, expected_before=before, expected_after=after,
            expected_focus_before=before_owner, expected_focus_after=after_owner,
            expected_cursor_before=cursor_before, expected_cursor_after=cursor_after))
        assert type(row['key']) is int and row['key'] == code, f'{case}: wrong key'
        assert type(row['modifiers']) is int and row['modifiers'] == modifiers, f'{case}: wrong modifiers'
        assert row['repeat'] is repeat and row['accepted'] is accepted, f'{case}: wrong repeat/consumption evidence'
        assert row['same_graph'] is True, f'{case}: key changed the applied graph'
        assert row['focus_before'] == before_owner, f'{case}: wrong actual initial focus'
        assert row['focus_after_press'] == row['focus_after'] == after_owner, f'{case}: wrong actual final focus'
        assert row['release_receiver'] == after_owner, f'{case}: release bypassed actual focus after press'
        direct = case in ('unfocused_canvas_Left', 'hidden_canvas_Right')
        assert row['route'] == ('direct-negative-probe' if direct else 'focus-routed'), f'{case}: undisclosed direct routing'
        assert row['press_receiver'] == ('canvas' if direct else before_owner), f'{case}: press bypassed expected owner'
        for field, expected in (('cursor_before', cursor_before), ('cursor_after', cursor_after)):
            assert type(row[field]) is int and row[field] == expected, f'{case}: wrong native cursor position'
        assert set(row['before']) == set(row['after']) == set(STATE_FIELDS), f'{case}: missing or unexpected state fields'
        verify_state(row['before'], before, case + ':before')
        verify_state(row['after'], after, case + ':after')
    assert len(audit) == 32, 'Wrong independent routed-event audit count'


def verify_observations(observations, inputs, audit):
    assert type(observations) is list and len(observations) == 9, 'Wrong selection observation count'
    assert [row['stage'] for row in observations] == list(STAGES), 'Missing, duplicate or reordered selection stages'
    component_count = properties_count = 0
    for observation in observations:
        stage = observation['stage']
        filename, state, context, r_active, c_active, focus_owner, selected_text = STAGES[stage]
        wanted = expected_state(state, inputs['original.json'])
        row_audit = dict(stage=stage, expected_state=wanted, expected_context=context,
            expected_r_active=r_active, expected_c_active=c_active,
            expected_focus_owner=focus_owner, expected_selected_text=selected_text)
        audit.append(row_audit)
        verify_state(observation, wanted, stage)
        assert observation['input'] == filename, f'{stage}: wrong source input'
        assert observation['r_active'] is r_active and observation['c_active'] is c_active, f'{stage}: wrong visible R/C activation'
        assert observation['focus_owner'] == focus_owner, f'{stage}: wrong actual keyboard owner'
        assert observation['selected_text'] == selected_text, f'{stage}: wrong selected field text'
        canvas = observation['canvas']
        assert canvas['context'] == context and canvas['selected_path'] == wanted['selected_path'], f'{stage}: wrong full context/selection IDs'
        supported = stage != 'unsupported'
        assert canvas['supported'] is supported, f'{stage}: wrong supported state'
        verify_view(canvas['view'], wanted['zoom'], wanted['pan'], wanted['panning'], supported, stage, row_audit)
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
        selected = wanted['selected_path']
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
        assert 'Occurrence: ' + '/'.join(selected) + '\n' in properties, f'{stage}: Properties occurrence IDs missing'
        assert 'Edit target: containing RC instance ' + '/'.join(context) + '\n' in properties, f'{stage}: Properties context IDs missing'
        assert 'Binding origin:\n' + parameter['origin'] + '\n' in properties, f'{stage}: binding origin mismatch'
        properties_count += 1
    assert len(audit) == 9 and component_count == 16 and properties_count == 7, 'Wrong independent source/caption/Properties audit counts'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--editor', required=True)
    parser.add_argument('--qt-kit', required=True)
    parser.add_argument('--out', required=True)
    args = parser.parse_args()
    out = Path(args.out).resolve()
    # Refuse an existing attempt directory before any launch or file mutation.
    out.mkdir(parents=True, exist_ok=False)
    root = out / 'document'
    report = out / 'report.json'
    command = [str(Path(args.editor).resolve()), '--selection-acceptance-root', str(root), '--report', str(report)]
    result = dict(command=command, state='preparing-inputs', independent_observations=[], independent_routed_events=[])
    stdout = stderr = b''
    try:
        root.mkdir()
        fixture = Path(__file__).resolve().parents[1] / 'schema/fixtures/two-rc-project.json'
        original = fixture.read_bytes()
        inputs = prepare_inputs(original)
        for name, raw in inputs.items():
            with (root / name).open('xb') as stream:
                stream.write(raw)
        env = {key: value for key, value in os.environ.items() if not key.startswith('QT_')}
        env['PATH'] = str(Path(args.qt_kit) / 'bin') + os.pathsep + env.get('PATH', '')
        env['QT_QPA_PLATFORM'] = 'windows'
        env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(Path(args.qt_kit) / 'plugins/platforms')
        result.update(state='launch-requested', input_sha256=hashlib.sha256(original).hexdigest())
        try:
            run = subprocess.run(command, env=env, capture_output=True, timeout=30)
        except subprocess.TimeoutExpired as error:
            stdout, stderr = error.stdout or b'', error.stderr or b''
            result.update(state='timeout', timeout_seconds=30)
            raise
        stdout, stderr = run.stdout, run.stderr
        result.update(state='process-exited', exit_code=run.returncode)
        assert run.returncode == 0, 'Native selection acceptance failed; inspect retained report.json and raw stdout/stderr'
        observed = json.loads(report.read_bytes())
        assert observed['passed'] is True, 'Native selection report did not pass'
        assert type(observed['checks']) is dict and len(observed['checks']) == 11, 'Wrong native selection control count'
        assert set(observed['checks']) == REQUIRED_CHECKS, 'Missing or unexpected native selection control names'
        assert all(value is True for value in observed['checks'].values()), 'Native selection control failed'
        assert observed['platform'] == 'windows', 'Unexpected Qt platform'
        assert type(observed['qt_version']) is str and observed['qt_version'], 'Missing Qt version'
        result.update(state='auditing-output', qt_version=observed['qt_version'], platform=observed['platform'])
        copy = (root / 'selection-R-copy.json').read_bytes()
        begin, end = name_span(original, 'main', 'right', ('overrides', 'resistance', 'value'))
        assert json.loads(original[begin:end]) == '2.2', 'Unexpected original right resistance token'
        verify_revision(original, copy, 'main', 'right', '3.5')
        result.update(independent_saved_token_audit='passed', saved_sha256=hashlib.sha256(copy).hexdigest())
        verify_observations(observed['observations'], {**inputs, 'selection-R-copy.json': copy}, result['independent_observations'])
        verify_routing(observed['routed_events'], original, result['independent_routed_events'])
        image = out / 'report.json.canvas.png'
        assert image.is_file() and image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), 'Missing representative PNG'
        assert sorted(path.name for path in out.glob('*.png')) == [image.name], 'Wrong screenshot count'
        for name, raw in inputs.items():
            assert (root / name).read_bytes() == raw, f'Changed input: {name}'
        assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'selection-R-copy.json']), 'Unexpected document/resource output'
        assert all(path.is_file() for path in root.iterdir()), 'No resource directories may be prepared in the document root'
        assert fixture.read_bytes() == original, 'Changed repository fixture'
        result.update(state='independent-audits-passed', source_diagrams_captions_properties_pan_geometry_focus_and_routing='passed',
            unchanged_inputs_and_no_resources='passed', controls=11, observations=9,
            captions=16, properties_blocks=7, routed_press_release_pairs=32,
            saved_token_audits=1, saved_copies=1, screenshots=1)
    except BaseException as error:
        if result['state'] != 'timeout':
            failure = {'preparing-inputs': 'preparation-failed', 'launch-requested': 'launch-failed'}
            result['state'] = failure.get(result['state'], 'audit-failed')
        result['error'] = f'{type(error).__name__}: {error}'
        raise
    finally:
        # Exactly one launch, no retry and no overwritten evidence. This also
        # retains preparation/launch failures and partial independent audits.
        with (out / 'stdout.log').open('xb') as stream:
            stream.write(stdout)
        with (out / 'stderr.log').open('xb') as stream:
            stream.write(stderr)
        with (out / 'runner.json').open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    print('PASS 11 selection controls, nine independent source/view snapshots, '
          '16 captions/7 Properties blocks, 32 press/release routing cases, '
          'one right R2.2->3.5 token audit/copy, one PNG and unchanged inputs without resources')


if __name__ == '__main__':
    main()
