# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Compact circuit resistance editing against independent snapshots and bytes."""
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
from editor_caption_acceptance import verify_observations as verify_captions
from native_resistance_regression import verify_revision
import project as reference

REQUIRED_CHECKS = {
    'empty_document_refuses_compact_edit',
    'inert_open_does_not_activate_or_select_target',
    'unselected_apply_refused',
    'capacitor_is_read_only_for_R',
    'R_inspection_does_not_activate_edit',
    'explicit_button_activates_existing_containing_literal',
    'target_unit_and_limits_visible_plain_text',
    'active_image',
    'draft_does_not_mutate_applied_canvas',
    'repeated_view_transfer_preserves_one_widget_and_four_drafts',
    'draft_image',
    'C_inspection_hides_R_and_refuses_stale_apply',
    'same_R_return_recovers_existing_draft_without_rebinding',
    'left_existing_requires_activation_and_retains_right_draft',
    'pending_drafts_block_copy_and_history',
    'invalid_value_keeps_draft_graph_and_target',
    'malformed_value_is_not_normalized_or_applied',
    'compact_apply_changes_R_and_preserves_other_three_drafts',
    'other_drafts_still_block_copy_and_history_after_R_apply',
    'native_undo_refreshes_active_R_and_canvas',
    'invalid_edit_preserves_redo',
    'native_redo_refreshes_active_R_and_canvas',
    'left_inspection_keeps_independent_applied_value',
    'create_only_copy_keeps_current_association_and_history',
    'occupied_copy_retains_active_editor',
    'failed_open_retains_active_editor_and_history',
    'successful_open_clears_activation_drafts_and_history',
    'reopened_R_requires_explicit_reactivation',
    'different_target_refused_for_instance_name_draft',
    'different_target_refused_for_R_draft',
    'different_target_refused_for_C_draft',
    'clean_instance_switch_preserves_project_draft',
    'explicit_target_replacement_requires_activation',
    'reordered_IDs_keep_explicit_target',
    'HTML_labels_are_plain_and_not_edit_identity',
    'missing_parent_literal_refuses_creation_or_activation',
    'unsupported_binding_refuses_stale_editor',
    'wrong_dimension_refuses_compact_edit',
    'final_copy_reactivates_shared_R_editor',
    'independent_windows_and_adjustable_panels_preserved',
    'final_active_R_image',
    'R_C_activation_shares_target_but_keeps_separate_widgets_drafts',
    'opposite_C_Apply_refused_while_R_selected',
}


def prepare_circuit_inputs(original):
    inputs = prepare_inputs(original)
    document = json.loads(original)
    main = next(row for row in document['sources']['topology']['circuits'] if row['id'] == 'main')
    right = next(row for row in main['instances'] if row['id'] == 'right')
    del right['overrides']['resistance']
    reference.validate(document)
    raw = json.dumps(document, ensure_ascii=False).encode()
    reference.parse(raw)
    inputs['missing-parent.json'] = raw
    return inputs


def verify_observations(observations, inputs):
    # Drafts are distinct from the immutable document inspected by the canvas.
    stages = {
        'original': ('original.json', [], [], '', '', False),
        'selected': ('original.json', ['main', 'right', 'r'], [], '', '', False),
        'draft': ('original.json', ['main', 'right', 'r'], ['main', 'right'], '3.5', 'kohm', True),
        'invalid': ('original.json', ['main', 'right', 'r'], ['main', 'right'], '0', 'kohm', True),
        'applied': ('circuit-resistance-copy.json', ['main', 'right', 'r'], ['main', 'right'], '3.5', 'kohm', True),
        'undo': ('original.json', ['main', 'right', 'r'], ['main', 'right'], '2.2', 'kohm', True),
        'redo': ('circuit-resistance-copy.json', ['main', 'right', 'r'], ['main', 'right'], '3.5', 'kohm', True),
        'left': ('circuit-resistance-copy.json', ['main', 'left', 'r'], ['main', 'right'], '3.5', 'kohm', False),
        'reopened': ('circuit-resistance-copy.json', ['main', 'right', 'r'], ['main', 'right'], '3.5', 'kohm', True),
        'reordered': ('reordered.json', ['main', 'right', 'r'], ['main', 'right'], '2.2', 'kohm', True),
        'labels': ('labels.json', ['main', 'right', 'r'], ['main', 'right'], '2.2', 'kohm', True),
        'missing_parent': ('missing-parent.json', ['main', 'right', 'r'], [], '', '', False),
        'default_binding': ('default-binding.json', [], [], '', '', False),
        'final': ('circuit-resistance-copy.json', ['main', 'right', 'r'], ['main', 'right'], '3.5', 'kohm', True),
    }
    assert [row['stage'] for row in observations] == list(stages), 'Missing, duplicate or reordered circuit-edit stages'
    for observation in observations:
        stage = observation['stage']
        filename, selected, target, draft, unit, active = stages[stage]
        assert observation['input'] == filename, observation
        assert observation['edit_target'] == target, observation
        assert observation['draft'] == draft and observation['unit'] == unit, observation
        assert observation['active'] is active and observation['editor_visible'] is active, observation
        canvas = observation['canvas']
        context = ['main', 'left' if stage == 'left' else 'right']
        assert canvas['context'] == context and canvas['selected_path'] == selected, observation
        supported = stage != 'default_binding'
        assert canvas['supported'] is supported, observation
        if not supported:
            assert canvas['components'] == [] and canvas['nets'] == [], observation
            continue
        components, nets = expected_diagram(inputs[filename], context)
        assert canonical_components(canvas['components']) == canonical_components(components), observation
        assert canonical_nets(canvas['nets']) == canonical_nets(nets), observation


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
    inputs = prepare_circuit_inputs(original)
    for name, raw in inputs.items():
        (root / name).write_bytes(raw)
    env = {key: value for key, value in os.environ.items() if not key.startswith('QT_')}
    env['PATH'] = str(Path(args.qt_kit) / 'bin') + os.pathsep + env.get('PATH', '')
    env['QT_QPA_PLATFORM'] = 'windows'
    env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(Path(args.qt_kit) / 'plugins/platforms')
    command = [str(Path(args.editor).resolve()), '--circuit-resistance-acceptance-root', str(root),
               '--report', str(out / 'report.json')]
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
    try:
        assert run.returncode == 0, run.stdout.decode(errors='replace') + run.stderr.decode(errors='replace')
        observed = json.loads((out / 'report.json').read_bytes())
        assert observed['passed'] is True and observed['checks']
        assert all(value is True for value in observed['checks'].values()), observed['checks']
        assert set(observed['checks']) == REQUIRED_CHECKS, observed['checks']
        assert observed['platform'] == 'windows'
        revised = (root / 'circuit-resistance-copy.json').read_bytes()
        verify_revision(original, revised, 'main', 'right', '3.5')
        verify_observations(observed['observations'], {**inputs, 'circuit-resistance-copy.json': revised})
        caption_audit = dict(components=[], properties=[])
        result['caption_observations'] = caption_audit
        verify_captions(observed['observations'], caption_audit)
        for suffix in ('active', 'draft', 'final'):
            image = out / f'report.json.{suffix}.png'
            assert image.is_file() and image.read_bytes().startswith(b'\x89PNG\r\n\x1a\n'), image
        for name, raw in inputs.items():
            assert (root / name).read_bytes() == raw, name
        assert sorted(path.name for path in root.iterdir()) == sorted([*inputs, 'circuit-resistance-copy.json'])
        assert all(path.is_file() for path in root.iterdir()), 'No resource directories may be prepared in the document root'
        assert fixture.read_bytes() == original
    except Exception as error:
        result.update(state='audit-failed', error=f'{type(error).__name__}: {error}')
        retain(run.stdout, run.stderr)
        raise
    result.update(state='independent-audits-passed',
        independent_circuit_observations_and_saved_token_audit='passed',
        independent_caption_and_base_value_audit='passed',
        unchanged_inputs_and_no_resource_files_audit='passed')
    retain(run.stdout, run.stderr)
    print(f'PASS {len(REQUIRED_CHECKS)} compact circuit resistance controls, 14 independent declared snapshots, '
          'one R-token saved-byte audit, 26 captions/12 Properties blocks, three PNG captures '
          'and unchanged inputs without resource files')


if __name__ == '__main__':
    main()
