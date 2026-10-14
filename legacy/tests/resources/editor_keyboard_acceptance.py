# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Focused canvas keys against independent routing, declarations and bytes."""
import argparse
import json
import os
from pathlib import Path
import subprocess

from editor_focus_acceptance import (
    canonical_components,
    canonical_nets,
    expected_caption,
    expected_diagram,
    prepare_inputs,
    verify_view,
)
from native_resistance_regression import verify_revision

REQUIRED_CHECKS = {
    'empty_keys_inert',
    'inert_open_default_view',
    'canvas_mouse_focus_indicator',
    'page_up_down_bounds_repeat',
    'home_recenter',
    'modified_unknown_keys_inert',
    'tab_roundtrip_native',
    'field_home_and_text_local',
    'field_page_keys_no_zoom',
    'focus_loss_cancels_drag',
    'keyboard_during_drag_cancels_retains_pan',
    'selection_hit_after_keyboard_zoom',
    'focus_mode_keyboard_restore_drafts',
    'independent_analyzer_keys_no_zoom',
    'R_apply_retains_view',
    'keyboard_undo_redo_native',
    'create_only_copy_and_occupied_refusal',
    'failed_open_retains_state',
    'reopen_unsupported_details_keys_inert',
    'resources_unaccessed_source_unchanged',
}

# Input, zoom, pan, selection, edit target, R/C drafts and visible activation;
# panel focus, retained R/C targets and project/instance drafts. Literal gate.
STAGES = {
    'original': ('original.json', 100, [0, 0], [], [], '', '', False, False, False, [], [], '@input', ''),
    'R_drafts': ('original.json', 100, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False, ['main', 'right'], [], 'Pending project', 'Pending right'),
    'lower_bound': ('original.json', 50, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False, ['main', 'right'], [], 'Pending project', 'Pending right'),
    'reset_drafts': ('original.json', 100, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False, ['main', 'right'], [], 'Pending project', 'Pending right'),
    'panned_key': ('original.json', 125, [35, 20], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False, ['main', 'right'], [], 'Pending project', 'Pending right'),
    'keyboard_C': ('original.json', 125, [35, 20], ['main', 'right', 'c'], ['main', 'right'], '3.5', '470', False, False, False, ['main', 'right'], [], 'Pending project', 'Pending right'),
    'focused_keys': ('original.json', 125, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', False, False, True, ['main', 'right'], [], 'Pending project', 'Pending right'),
    'restored_R': ('original.json', 125, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '470', True, False, False, ['main', 'right'], [], 'Pending project', 'Pending right'),
    'R_applied': ('keyboard-R-copy.json', 125, [0, 0], ['main', 'right', 'r'], ['main', 'right'], '3.5', '220', True, False, False, ['main', 'right'], [], '@input', 'right'),
    'unsupported': ('changed-net.json', 100, [0, 0], [], [], '', '', False, False, False, [], [], '@input', ''),
}

KEY_HOME, KEY_END = 0x01000010, 0x01000011
KEY_PAGE_UP, KEY_PAGE_DOWN = 0x01000016, 0x01000017
KEY_TAB, KEY_BACKTAB = 0x01000001, 0x01000002
CTRL, SHIFT = 0x04000000, 0x02000000

# Case -> key, modifiers, repeat, initial/final zoom, accepted. Canvas routing
# and the missing nonfield cursor positions are checked separately.
CANVAS_EVENTS = {
    'canvas_page_up': (KEY_PAGE_UP, 0, False, 100, 125, True),
    'canvas_repeat_up': (KEY_PAGE_UP, 0, True, 125, 150, True),
    'upper_bound': (KEY_PAGE_UP, 0, False, 150, 150, True),
    'canvas_page_down': (KEY_PAGE_DOWN, 0, False, 150, 125, True),
    'lower_step_0': (KEY_PAGE_DOWN, 0, False, 125, 100, True),
    'lower_step_1': (KEY_PAGE_DOWN, 0, False, 100, 75, True),
    'lower_step_2': (KEY_PAGE_DOWN, 0, True, 75, 50, True),
    'lower_bound': (KEY_PAGE_DOWN, 0, False, 50, 50, True),
    'canvas_home': (KEY_HOME, 0, False, 50, 100, True),
    'modified_canvas': (KEY_PAGE_UP, CTRL, False, 100, 100, False),
    'unknown_canvas': (KEY_END, 0, False, 100, 100, False),
    'drag_ignored': (KEY_PAGE_UP, CTRL, False, 100, 100, False),
    'drag_key': (KEY_PAGE_UP, 0, False, 100, 125, True),
    'focus_mode_home': (KEY_HOME, 0, False, 125, 100, True),
    'focus_mode_up': (KEY_PAGE_UP, 0, False, 100, 125, True),
    'unsupported_home': (KEY_HOME, 0, False, 100, 100, False),
    'unsupported_up': (KEY_PAGE_UP, 0, False, 100, 100, False),
    'unsupported_down': (KEY_PAGE_DOWN, 0, False, 100, 100, False),
    'empty_home': (KEY_HOME, 0, False, 100, 100, False),
    'empty_up': (KEY_PAGE_UP, 0, False, 100, 100, False),
    'empty_down': (KEY_PAGE_DOWN, 0, False, 100, 100, False),
}

FIELD_EVENTS = {
    'field_R_home': ('resistance', KEY_HOME, 3, 0),
    'field_R_text': ('resistance', ord('9'), 0, 1),
    'field_R_page_up': ('resistance', KEY_PAGE_UP, None, None),
    'field_R_page_down': ('resistance', KEY_PAGE_DOWN, None, None),
    'field_C_home': ('capacitance', KEY_HOME, 3, 0),
    'field_project_home': ('project', KEY_HOME, 15, 0),
    'field_instance_home': ('instance', KEY_HOME, 13, 0),
    'details_page': ('project', KEY_PAGE_UP, None, None),
}


def verify_routed_events(events, audit):
    assert type(events) is list and events, 'Missing routed keyboard events'
    by_case = {}
    for event in events:
        case = event['case']
        assert type(case) is str and case, 'Invalid keyboard case identity'
        for field in ('receiver', 'focus_before', 'focus_after'):
            assert type(event[field]) is str and event[field], f'{case}: missing receiver or focus evidence'
        for field in ('key', 'modifiers', 'cursor_before', 'cursor_after'):
            assert type(event[field]) is int, f'{case}: invalid key/modifier/cursor evidence'
        for field in ('repeat', 'accepted'):
            assert type(event[field]) is bool, f'{case}: invalid repeat/accepted evidence'
        for field in ('zoom_before', 'zoom_after'):
            assert type(event[field]) is int and event[field] in (50, 75, 100, 125, 150), f'{case}: invalid zoom evidence'
        assert event['cursor_before'] >= -1 and event['cursor_after'] >= -1, f'{case}: invalid cursor position'
        by_case.setdefault(case, []).append(event)
    required = set(CANVAS_EVENTS) | set(FIELD_EVENTS) | {'tab_leave', 'tab_return', 'analyzer_home', 'analyzer_up'}
    assert required <= set(by_case), 'Missing required routed keyboard cases'

    def one(case):
        assert len(by_case[case]) == 1, f'{case}: duplicate required routing case'
        row = by_case[case][0]
        audit.append(dict(case=case, event=row))
        return row

    for case, expected in CANVAS_EVENTS.items():
        row = one(case)
        assert row['receiver'] == row['focus_before'] == row['focus_after'] == 'canvas', f'{case}: key bypassed focused canvas'
        actual = tuple(row[field] for field in ('key', 'modifiers', 'repeat', 'zoom_before', 'zoom_after', 'accepted'))
        assert actual == expected, f'{case}: canvas key/navigation semantics mismatch'
        assert row['cursor_before'] == row['cursor_after'] == -1, f'{case}: canvas reports an editable cursor'
    # Independently require each bounded downward transition, including the
    # final accepted boundary event; loop labels are not identity for navigation.
    downward = [row for row in events if row['receiver'] == row['focus_before'] == row['focus_after'] == 'canvas'
        and row['key'] == KEY_PAGE_DOWN and row['modifiers'] == 0 and row['accepted']]
    for before, after in ((125, 100), (100, 75), (75, 50), (50, 50)):
        assert any(row['zoom_before'] == before and row['zoom_after'] == after for row in downward), 'Missing bounded PageDown transition'
    audit.append(dict(case='bounded_PageDown_transitions', events=downward))
    for case, (receiver, key, before, after) in FIELD_EVENTS.items():
        row = one(case)
        assert row['receiver'] == row['focus_before'] == row['focus_after'] == receiver, f'{case}: field key rerouted to canvas'
        assert row['key'] == key and row['modifiers'] == 0 and row['repeat'] is False, f'{case}: wrong field key'
        assert row['zoom_before'] == row['zoom_after'] == 100, f'{case}: field key changed circuit zoom'
        assert row['cursor_before'] >= 0 and row['cursor_after'] >= 0, f'{case}: field cursor missing'
        if before is not None:
            assert row['cursor_before'] == before and row['cursor_after'] == after and row['accepted'] is True, f'{case}: local field cursor/edit mismatch'
    leave, returned = one('tab_leave'), one('tab_return')
    assert leave['receiver'] == leave['focus_before'] == 'canvas' and leave['focus_after'] != 'canvas', 'Tab did not leave canvas natively'
    assert leave['key'] == KEY_TAB and leave['modifiers'] == 0 and leave['repeat'] is False, 'Wrong outward Tab event'
    assert returned['receiver'] == returned['focus_before'] != 'canvas' and returned['focus_after'] == 'canvas', 'Reverse Tab did not return to canvas natively'
    assert returned['key'] in (KEY_TAB, KEY_BACKTAB) and returned['modifiers'] == SHIFT and returned['repeat'] is False, 'Wrong reverse Tab event'
    assert leave['zoom_before'] == leave['zoom_after'] == returned['zoom_before'] == returned['zoom_after'] == 100, 'Tab navigation changed circuit zoom'
    for case, key in (('analyzer_home', KEY_HOME), ('analyzer_up', KEY_PAGE_UP)):
        row = one(case)
        assert row['receiver'] == row['focus_before'] == row['focus_after'] == 'analyzer', f'{case}: analyzer key reached editor'
        assert row['key'] == key and row['modifiers'] == 0 and row['repeat'] is False, f'{case}: wrong analyzer key'
        assert row['zoom_before'] == row['zoom_after'] == 125 and row['cursor_before'] == row['cursor_after'] == -1, f'{case}: analyzer changed editor view'


def verify_observations(observations, inputs, audit):
    assert [row['stage'] for row in observations] == list(STAGES), 'Missing, duplicate or reordered keyboard stages'
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
        assert observation['focused'] is focused and observation['focus_owner'] == 'canvas', f'{stage}: wrong panel/keyboard focus'
        canvas = observation['canvas']
        assert canvas['context'] == ['main', 'right'] and canvas['selected_path'] == selected, f'{stage}: wrong full selection IDs'
        supported = stage != 'unsupported'
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
    assert component_count == 18 and properties_count == 8, 'Wrong independent caption/Properties audit counts'


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
    command = [str(Path(args.editor).resolve()), '--keyboard-acceptance-root', str(root), '--report', str(out / 'report.json')]
    result = dict(command=command, state='launch-requested', independent_observations=[], independent_routed_events=[])

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
        assert run.returncode == 0, 'Native keyboard acceptance failed; inspect retained report.json and raw stdout/stderr'
        observed = json.loads((out / 'report.json').read_bytes())
        assert observed['passed'] is True and observed['checks'], 'Native keyboard report did not pass'
        assert set(observed['checks']) == REQUIRED_CHECKS, 'Missing or unexpected native keyboard control names'
        assert all(value is True for value in observed['checks'].values()), 'Native keyboard control failed; inspect retained report.json'
        assert observed['platform'] == 'windows'
        copy = (root / 'keyboard-R-copy.json').read_bytes()
        verify_revision(original, copy, 'main', 'right', '3.5')
        result['independent_saved_token_audit'] = 'passed'
        verify_observations(observed['observations'], {**inputs, 'keyboard-R-copy.json': copy}, result['independent_observations'])
        verify_routed_events(observed['routed_events'], result['independent_routed_events'])
        for suffix in ('canvas', 'field'):
            image = out / f'report.json.{suffix}.png'
            assert image.is_file() and image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), image
        for name, raw in inputs.items():
            assert (root / name).read_bytes() == raw, name
        assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'keyboard-R-copy.json'])
        assert all(path.is_file() for path in root.iterdir()), 'No resource directories may be prepared in the document root'
        assert fixture.read_bytes() == original
    except Exception as error:
        result.update(state='audit-failed', error=f'{type(error).__name__}: {error}')
        retain(run.stdout, run.stderr)
        raise
    result.update(state='independent-audits-passed', source_diagrams_captions_properties_pan_geometry_focus_and_routing='passed',
        unchanged_inputs_and_no_resources='passed')
    retain(run.stdout, run.stderr)
    print(f'PASS {len(REQUIRED_CHECKS)} focused keyboard controls, 10 independent source/view snapshots, '
          '18 captions/8 Properties blocks, routed canvas/field/analyzer events, one saved-token audit, '
          'two PNGs and unchanged inputs without resources')


if __name__ == '__main__':
    main()
