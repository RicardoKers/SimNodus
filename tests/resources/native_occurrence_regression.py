# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Owned occurrence identity, values and local terminals against JSON oracles."""
import argparse
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'schema'))
import bindings
import parameters
import project as reference
from native_resistance_regression import verify_revision

FIXTURES = Path(__file__).resolve().parents[1] / 'schema/fixtures'
RAW = (FIXTURES / 'two-rc-project.json').read_bytes()
ART = (FIXTURES / 'assets/passive.svg').read_bytes()
PROBE = ''
SELECT_REQUESTS = 0
LIFECYCLE_REQUESTS = 0
PATHS = [('main', 'left', 'r'), ('main', 'right', 'r'),
         ('main', 'left', 'c'), ('main', 'right', 'c')]


def expected_occurrence(raw, path):
    """Walk declared IDs, then reuse the accepted independent value resolver."""
    document = reference.parse(raw)
    topology = document['sources']['topology']
    circuits = {row['id']: row for row in topology['circuits']}
    components = {row['id']: row for row in topology['components']}
    if len(path) < 2 or path[0] != topology['root']:
        return None
    circuit = circuits[topology['root']]
    for index, instance_id in enumerate(path[1:], 1):
        selected = next((row for row in circuit['instances'] if row['id'] == instance_id), None)
        if selected is None:
            return None
        if index + 1 < len(path):
            if selected['kind'] != 'circuit':
                return None
            circuit = circuits[selected['definition']]
            continue
        if selected['kind'] != 'component' or selected['definition'] not in components:
            return None
        resolved = parameters.resolve(bindings.projection(topology))['instances']
        row = next(row for row in resolved if row['path'] == list(path))
        assert row['definition'] == selected['definition']
        memberships = {}
        for net in circuit['nets']:
            for terminal in net['terminals']:
                if terminal.get('instance') != selected['id']:
                    continue
                assert terminal['terminal'] not in memberships
                memberships[terminal['terminal']] = dict(id=net['id'], name=net['name'],
                                                        path=[*path[:-1], net['id']])
        terminals = [dict(pin=pin['id'], name=pin['name'], net=memberships.get(pin['id']))
                     for pin in sorted(components[selected['definition']]['pins'], key=lambda pin: pin['id'])]
        return dict(path=list(path), source_circuit=circuit['id'], source_instance=selected['id'],
                    component=selected['definition'], name=selected['name'],
                    parameters=copy.deepcopy(row['parameters']), terminals=terminals)
    return None


def invoke(raw, path):
    global SELECT_REQUESTS
    SELECT_REQUESTS += 1
    run = subprocess.run([PROBE, '--select', *path], input=raw, capture_output=True, timeout=20)
    result = json.loads(run.stdout)
    assert run.returncode == (1 if 'error' in result else 0), run.stderr
    return result


class Occurrence(unittest.TestCase):
    def accepted(self, raw, path):
        expected = expected_occurrence(raw, path)
        self.assertIsNotNone(expected)
        actual = invoke(raw, path)
        self.assertEqual(actual, expected)
        return actual

    def refused(self, raw, path):
        self.assertIsNone(expected_occurrence(raw, path))
        self.assertEqual(invoke(raw, path), dict(error='occurrence'))

    def test_existing_component_occurrences(self):
        for path in PATHS:
            with self.subTest(path=path):
                selected = self.accepted(RAW, path)
                expected_nets = {'r': {'n': 'junction', 'p': 'drive'},
                                 'c': {'n': 'return', 'p': 'junction'}}
                for terminal in selected['terminals']:
                    self.assertEqual(terminal['net']['path'],
                                     [*path[:-1], expected_nets[path[-1]][terminal['pin']]])

    def test_missing_circuit_rootless_and_untrusted_selection(self):
        missing = [(), ('main',), ('main', 'left'), ('main', 'right'), ('left', 'r'), ('rc', 'r'),
                   ('Main', 'left', 'r'), ('main', 'absent', 'r'), ('main', 'left', 'absent'),
                   ('main', 'left', 'r', 'extra'), ('main', 'drive'), ('main', 'left', 'R'),
                   ('main', *(['left'] * 9))]
        for path in missing:
            with self.subTest(path=path):
                self.refused(RAW, path)
        for raw in (b'{', b'\xff', RAW.replace(b'"version": "0.1"',
                                             b'"version": "0.1", "version": "0.1"', 1)):
            result = invoke(raw, PATHS[0])
            self.assertIn('error', result)
            self.assertNotEqual(result['error'], 'occurrence')

    def test_reordered_escaped_keys_and_untrusted_labels(self):
        document = json.loads(RAW)
        topology = document['sources']['topology']
        for collection in ('components', 'circuits', 'symbols', 'models'):
            topology[collection].reverse()
            for definition in topology[collection]:
                definition['name'] = '<b>same Ω label</b>'
                if 'pins' in definition:
                    definition['pins'].reverse()
                    for pin in definition['pins']:
                        pin['name'] = '<b>same Ω pin</b>'
        for circuit in topology['circuits']:
            circuit['instances'].reverse()
            for selected in circuit['instances']:
                selected['name'] = '<b>same Ω label</b>'
            circuit['nets'].reverse()
            for net in circuit['nets']:
                net['name'] = '<i>same Ω net</i>'
                net['terminals'].reverse()
        raw = json.dumps(document, ensure_ascii=False, sort_keys=True).encode()
        raw = raw.replace(b'"instances"', b'"\\u0069nstances"').replace(
            b'"definition"', b'"def\\u0069nition"').replace(b'"r"', b'"\\u0072"').replace(
            b'"terminals"', b'"\\u0074erminals"').replace(b'"pins"', b'"pi\\u006es"')
        for path in PATHS:
            self.accepted(raw, path)

    def test_reused_context_and_unreachable_definition_ids(self):
        document = json.loads(RAW)
        topology = document['sources']['topology']
        original_rc = next(row for row in topology['circuits'] if row['id'] == 'rc')
        shadow = copy.deepcopy(original_rc)
        shadow['id'] = 'shadow'
        for selected in shadow['instances']:
            selected['name'] = 'Other declared ' + selected['id']
        unused = copy.deepcopy(original_rc)
        unused['id'] = 'unused'
        wrapper = copy.deepcopy(original_rc)
        wrapper['id'] = 'wrapper'
        wrapper['instances'] = [dict(id='inner', name='Nested circuit occurrence', kind='circuit',
                                     definition='shadow', overrides={})]
        for net, port in zip(wrapper['nets'], ('input', 'output', 'reference')):
            net['terminals'] = [dict(port=port), dict(instance='inner', terminal=port)]
        topology['circuits'] = [unused, shadow, wrapper, *topology['circuits']]
        main = next(row for row in topology['circuits'] if row['id'] == 'main')
        main['instances'].append(dict(id='third', name='Additional declared occurrence',
                                      kind='circuit', definition='shadow', overrides={}))
        main['instances'].append(dict(id='nested', name='Nested declared occurrence',
                                      kind='circuit', definition='wrapper', overrides={}))
        raw = json.dumps(document).encode()
        reference.parse(raw)
        original = self.accepted(raw, PATHS[0])
        other = self.accepted(raw, ('main', 'third', 'r'))
        self.assertEqual(original['source_circuit'], 'rc')
        self.assertEqual(other['source_circuit'], 'shadow')
        self.assertNotEqual(original['name'], other['name'])
        self.accepted(raw, ('main', 'third', 'c'))
        for instance_id in ('r', 'c'):
            path = ('main', 'nested', 'inner', instance_id)
            nested = self.accepted(raw, path)
            self.assertEqual(nested['source_circuit'], 'shadow')
            for terminal in nested['terminals']:
                self.assertEqual(terminal['net']['path'][:-1], list(path[:-1]))
        for path in [('unused', 'r'), ('main', 'unused', 'r'), ('main', 'third')]:
            self.refused(raw, path)

    def test_unconnected_and_changed_local_membership(self):
        for move_to_drive in (False, True):
            document = json.loads(RAW)
            rc = next(row for row in document['sources']['topology']['circuits'] if row['id'] == 'rc')
            junction = next(row for row in rc['nets'] if row['id'] == 'junction')
            removed = next(row for row in junction['terminals']
                           if row.get('instance') == 'r' and row.get('terminal') == 'n')
            junction['terminals'].remove(removed)  # c/p and output still satisfy the two-terminal minimum.
            if move_to_drive:
                next(row for row in rc['nets'] if row['id'] == 'drive')['terminals'].append(removed)
            raw = json.dumps(document).encode()
            for path in PATHS:
                selected = self.accepted(raw, path)
                if path[-1] == 'r':
                    terminal = next(row for row in selected['terminals'] if row['pin'] == 'n')
                    if move_to_drive:
                        self.assertEqual(terminal['net']['path'], [*path[:-1], 'drive'])
                    else:
                        self.assertIsNone(terminal['net'])

    def test_symbol_mapping_and_availability_do_not_select_local_nets(self):
        for available in (False, True):
            document = json.loads(RAW)
            resistor = next(row for row in document['sources']['topology']['components'] if row['id'] == 'resistor')
            if available:
                resistor['symbol']['pin_map'] = {'p': 'b', 'n': 'a'}
            else:
                resistor['symbol'] = None
            raw = json.dumps(document).encode()
            for path in PATHS:
                self.assertEqual(self.accepted(raw, path), expected_occurrence(RAW, path))

    def test_upstream_literal_change_preserves_other_occurrences(self):
        document = json.loads(RAW)
        main = next(row for row in document['sources']['topology']['circuits'] if row['id'] == 'main')
        right = next(row for row in main['instances'] if row['id'] == 'right')
        right['overrides']['resistance']['value'] = '3.5'
        raw = json.dumps(document).encode()
        for path in PATHS:
            before = expected_occurrence(RAW, path)
            after = self.accepted(raw, path)
            self.assertEqual({key: value for key, value in after.items() if key != 'parameters'},
                             {key: value for key, value in before.items() if key != 'parameters'})
            if path == ('main', 'right', 'r'):
                self.assertEqual(after['parameters']['resistance']['value'], '3500')
                self.assertEqual(after['parameters']['resistance']['origin'], 'containing-circuit:resistance')
            else:
                self.assertEqual(after, before)

    def test_native_lifecycle_and_independent_persisted_copy(self):
        global LIFECYCLE_REQUESTS
        base = Path('build/sn023-terminal-membership/native-tests').resolve()
        base.mkdir(parents=True, exist_ok=True)
        # Keep every physical case, including failures and partial outputs, for audit.
        root = Path(tempfile.mkdtemp(prefix='occurrence-', dir=base))
        document_root, artwork_root = root / 'document', root / 'explicit-resource-root'
        document_root.mkdir()
        (document_root / 'original.json').write_bytes(RAW)
        asset = artwork_root / 'tests/schema/fixtures/assets/passive.svg'
        asset.parent.mkdir(parents=True)
        asset.write_bytes(ART)
        command = [PROBE, '--lifecycle', str(document_root), str(artwork_root)]
        record = dict(command=command, state='launch-requested')

        def retain(stdout, stderr):
            (root / 'stdout.log').write_bytes(stdout)
            (root / 'stderr.log').write_bytes(stderr)
            (root / 'runner.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')

        retain(b'', b'')
        print('Retained native lifecycle evidence: ' + str(root))
        LIFECYCLE_REQUESTS += 1
        try:
            run = subprocess.run(command, capture_output=True, timeout=30)
        except subprocess.TimeoutExpired as error:
            record.update(state='timeout', timeout_seconds=30)
            retain(error.stdout or b'', error.stderr or b'')
            self.fail('Native lifecycle timed out; evidence retained at ' + str(root))
        except OSError as error:
            record.update(state='launch-failed', error=str(error))
            retain(b'', b'')
            self.fail('Native lifecycle launch failed; evidence retained at ' + str(root))
        record.update(state='process-exited', exit_code=run.returncode)
        retain(run.stdout, run.stderr)
        output = run.stdout.decode('utf-8', errors='replace')
        print(output, end='')
        self.assertEqual(run.returncode, 0, run.stderr.decode('utf-8', errors='replace'))
        if os.name == 'nt':
            revised = (document_root / 'occurrence-copy.json').read_bytes()
            verify_revision(RAW, revised, 'main', 'right', '3.5')
            expected = expected_occurrence(revised, ('main', 'right', 'r'))
            self.assertEqual(expected['path'], ['main', 'right', 'r'])
            self.assertEqual(expected['source_circuit'], 'rc')
            self.assertEqual(expected['source_instance'], 'r')
            self.assertEqual(expected['terminals'], expected_occurrence(RAW, ('main', 'right', 'r'))['terminals'])
            self.assertEqual(sorted(path.name for path in document_root.iterdir()),
                             ['occurrence-copy.json', 'original.json'])
            print('PASS independent persisted-byte audit and root-prefixed occurrence identity')
        else:
            self.assertIn('SKIP Windows local NTFS occurrence artwork lifecycle', output)
            self.assertEqual(list(document_root.iterdir()), [document_root / 'original.json'])
        self.assertEqual((document_root / 'original.json').read_bytes(), RAW)
        self.assertEqual([path for path in artwork_root.rglob('*') if path.is_file()], [asset])
        self.assertEqual(asset.read_bytes(), ART)
        self.assertEqual((FIXTURES / 'two-rc-project.json').read_bytes(), RAW)
        self.assertEqual((FIXTURES / 'assets/passive.svg').read_bytes(), ART)


def main():
    global PROBE
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    PROBE = str(Path(parser.parse_args().probe).resolve())
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Occurrence))
    print(f'{SELECT_REQUESTS} independent occurrence selection requests; '
          f'{LIFECYCLE_REQUESTS} native lifecycle request; '
          f'{SELECT_REQUESTS + LIFECYCLE_REQUESTS} total probe requests')
    raise SystemExit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()
