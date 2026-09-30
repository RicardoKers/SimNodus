# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Resolved rows and immediate binding spans against independent Python oracles."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import unittest

import bindings
import parameters
import project as reference

PROBE = ''
REQUESTS = 0
RAW = (Path(__file__).parent / 'fixtures/two-rc-project.json').read_bytes()


def byte_spans(raw):
    text = raw.decode('utf-8')
    decoder = json.JSONDecoder()
    spans = {}

    def skip(position):
        while text[position].isspace():
            position += 1
        return position

    def walk(position, path):
        position = skip(position)
        begin = position
        if text[position] == '{':
            position = skip(position + 1)
            while text[position] != '}':
                key, position = decoder.raw_decode(text, position)
                position = skip(position)
                assert text[position] == ':'
                position = skip(walk(position + 1, path + (key,)))
                if text[position] == ',':
                    position = skip(position + 1)
            position += 1
        elif text[position] == '[':
            position = skip(position + 1)
            index = 0
            while text[position] != ']':
                position = skip(walk(position, path + (index,)))
                index += 1
                if text[position] == ',':
                    position = skip(position + 1)
            position += 1
        else:
            _, position = decoder.raw_decode(text, position)
        spans[path] = (len(text[:begin].encode()), len(text[:position].encode()))
        return position

    walk(0, ())
    return spans


def expected_rows(raw, circuit_id, instance_id):
    project = reference.parse(raw)
    topology = project['sources']['topology']
    resolved = parameters.resolve(bindings.projection(topology))['instances']
    definitions = {row['id']: row for row in topology['components'] + topology['circuits']}
    prefix = ('sources', 'topology')
    locations = {row['id']: prefix + (collection, index) for collection in ('components', 'circuits')
                 for index, row in enumerate(topology[collection])}
    spans = byte_spans(raw)
    selected = []

    def visit(did, path):
        for index, instance in enumerate(definitions[did].get('instances', [])):
            child = path + [instance['id']]
            if did == circuit_id and instance['id'] == instance_id:
                row = copy.deepcopy(next(row for row in resolved if row['path'] == child))
                source = locations[did] + ('instances', index)
                row['source_offset'] = spans[source][0]
                target = definitions[instance['definition']]
                for pid, parameter in row['parameters'].items():
                    override = instance['overrides'].get(pid)
                    if override is not None:
                        token = source + ('overrides', pid, 'parameter' if 'parameter' in override else 'value')
                    else:
                        pindex = next(i for i, p in enumerate(target['parameters']) if p['id'] == pid)
                        token = locations[target['id']] + ('parameters', pindex, 'default')
                    parameter['source_offset'] = spans[token][0]
                selected.append(row)
            if instance['kind'] == 'circuit':
                visit(instance['definition'], child)

    visit(topology['root'], [topology['root']])
    return sorted(selected, key=lambda row: row['path'])


def invoke(raw, circuit, instance):
    global REQUESTS
    REQUESTS += 1
    run = subprocess.run([PROBE, circuit, instance], input=raw, capture_output=True, timeout=20)
    result = json.loads(run.stdout)
    assert run.returncode == (1 if 'error' in result else 0), run.stderr
    return result


class Inspection(unittest.TestCase):
    def accepted(self, raw, circuit, instance):
        expected = expected_rows(raw, circuit, instance)
        actual = invoke(raw, circuit, instance)
        self.assertEqual(actual, dict(borrowed_owned=True, nonmutating=True, rows=expected))
        return actual['rows']

    def test_literal_default_forward(self):
        for circuit, instance in [('main', 'left'), ('main', 'right'), ('rc', 'r'), ('rc', 'c')]:
            with self.subTest(circuit=circuit, instance=instance):
                self.accepted(RAW, circuit, instance)

    def test_missing_and_noninstance_selectors(self):
        for circuit, instance in [('', ''), ('main', ''), ('', 'left'), ('main', 'absent'), ('missing', 'r'),
                                  ('main', 'drive'), ('main', 'main'), ('x' * 65, 'left'), ('rc', 'r' * 65), ('Main', 'left')]:
            self.assertEqual(self.accepted(RAW, circuit, instance), [])

    def test_order_escaped_keys_and_labels(self):
        project = json.loads(RAW)
        topology = project['sources']['topology']
        topology['circuits'].reverse()
        topology['components'].reverse()
        for circuit in topology['circuits']:
            circuit['instances'].reverse()
            for instance in circuit['instances']:
                instance['name'] = '<b>same Ω label</b>'
        raw = json.dumps(project, ensure_ascii=False, sort_keys=True).encode()
        raw = raw.replace(b'"instances"', b'"\\u0069nstances"').replace(b'"resistance"', b'"\\u0072esistance"')
        for circuit, instance in [('main', 'left'), ('main', 'right'), ('rc', 'r'), ('rc', 'c')]:
            self.accepted(raw, circuit, instance)

    def test_unreachable_same_ids_different_definition(self):
        project = json.loads(RAW)
        circuits = project['sources']['topology']['circuits']
        unused = copy.deepcopy(next(row for row in circuits if row['id'] == 'main'))
        unused['id'] = 'unused'
        circuits.append(unused)
        raw = json.dumps(project).encode()
        self.assertEqual(self.accepted(raw, 'unused', 'left'), [])
        self.assertEqual(self.accepted(raw, 'unused', 'right'), [])
        self.accepted(raw, 'main', 'left')
        self.accepted(raw, 'rc', 'r')

    def test_changed_upstream_quantities(self):
        project = json.loads(RAW)
        main = next(row for row in project['sources']['topology']['circuits'] if row['id'] == 'main')
        main['instances'][0]['overrides']['resistance']['value'] = '4.7'
        main['instances'][1]['overrides']['capacitance']['value'] = '470'
        raw = json.dumps(project, indent=1).encode()
        for circuit, instance in [('main', 'left'), ('main', 'right'), ('rc', 'r'), ('rc', 'c')]:
            self.accepted(raw, circuit, instance)

    def test_untrusted_input_rejected_before_inspection(self):
        for raw in (b'{', b'\xff', RAW.replace(b'"version": "0.1"', b'"version": "0.1", "version": "0.1"', 1)):
            self.assertIn('error', invoke(raw, 'main', 'left'))


def main():
    global PROBE
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    PROBE = str(Path(parser.parse_args().probe).resolve())
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Inspection))
    print(f'{REQUESTS} independent parameter inspection requests')
    raise SystemExit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()
