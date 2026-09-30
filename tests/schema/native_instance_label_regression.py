# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Independent exact-byte and graph oracle for stable-ID instance name edits."""
import argparse
import copy
import json
from pathlib import Path
import struct
import subprocess
import unittest

import project as reference
from native_graph_regression import expected_graph, expected_positions

PROBE = ''
REQUESTS = 0
RAW = (Path(__file__).parent / 'fixtures/two-rc-project.json').read_bytes()


def instance(value, circuit_id, instance_id):
    circuit = next(row for row in value['sources']['topology']['circuits'] if row['id'] == circuit_id)
    return next(row for row in circuit['instances'] if row['id'] == instance_id)


def name_span(raw, circuit_id, instance_id):
    """Use Python's JSON decoder and declaration IDs, independent of C++ spans."""
    value = json.loads(raw)
    circuits = value['sources']['topology']['circuits']
    i = next(i for i, row in enumerate(circuits) if row['id'] == circuit_id)
    j = next(j for j, row in enumerate(circuits[i]['instances']) if row['id'] == instance_id)
    wanted = ('sources', 'topology', 'circuits', i, 'instances', j, 'name')
    text = raw.decode()
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
    return spans[wanted]


def verify_revision(original, revised, circuit_id, instance_id, name):
    before = reference.parse(original)
    expected = copy.deepcopy(before)
    selected = instance(expected, circuit_id, instance_id)
    old = selected['name']
    selected['name'] = name
    reference.validate(expected)
    assert reference.parse(revised) == expected
    begin, end = name_span(original, circuit_id, instance_id)
    encoded = json.dumps(name, ensure_ascii=False).encode()
    wanted = original if old == name else original[:begin] + encoded + original[end:]
    assert revised == wanted, 'Non-selected bytes or original no-op spelling changed'
    return expected


def invoke(raw, circuit_id, instance_id, name):
    global REQUESTS
    REQUESTS += 1
    fields = [value.encode() if isinstance(value, str) else value for value in (raw, circuit_id, instance_id, name)]
    packet = b''.join(struct.pack('<I', len(value)) + value for value in fields)
    run = subprocess.run([PROBE, '--instance'], input=packet, capture_output=True, timeout=20)
    result = json.loads(run.stdout)
    assert run.returncode == (1 if 'error' in result else 0), run.stderr
    return result


class InstanceLabel(unittest.TestCase):
    def accepted(self, raw, circuit_id, instance_id, name):
        result = invoke(raw, circuit_id, instance_id, name)
        self.assertNotIn('error', result)
        revised = bytes.fromhex(result['source_hex'])
        expected = verify_revision(raw, revised, circuit_id, instance_id, name)
        topology = expected['sources']['topology']
        self.assertEqual(result['graph'], expected_graph(topology))
        positions = expected_positions(topology)
        self.assertEqual({tuple(row['identity']) for row in result['positions']}, set(positions))
        for row in result['positions']:
            self.assertEqual(json.loads(revised[row['begin']:row['end']]), positions[tuple(row['identity'])])
        return revised

    def test_nested_definition_and_root_subcircuit_instance(self):
        for circuit_id, instance_id in [('main', 'left'), ('main', 'right'), ('rc', 'r'), ('rc', 'c')]:
            self.accepted(RAW, circuit_id, instance_id, 'Visible label')

    def test_unicode_escaping_and_literal_injection(self):
        for name in ['A "quoted" label \\ path', 'caf\u00e9 \U0001f600 \u03a9', '<img src="file:///C:/never-open.png">', '"},"id":"other']:
            self.accepted(RAW, 'rc', 'r', name)

    def test_reordering_and_escaped_keys(self):
        value = json.loads(RAW)
        value['sources']['topology']['circuits'].reverse()
        for circuit in value['sources']['topology']['circuits']:
            circuit['instances'].reverse()
        def reordered(row):
            if isinstance(row, dict):
                return {key: reordered(row[key]) for key in reversed(row)}
            if isinstance(row, list):
                return [reordered(item) for item in row]
            return row
        raw = json.dumps(reordered(value), indent=3).encode().replace(b'"name"', b'"na\\u006de"')
        self.accepted(b' \r\n' + raw + b'\t\n', 'rc', 'r', 'Reordered')

    def test_duplicate_labels_and_instance_ids_in_distinct_circuits(self):
        value = json.loads(RAW)
        main = next(row for row in value['sources']['topology']['circuits'] if row['id'] == 'main')
        for row in main['instances']:
            row['name'] = 'Same label'
        main['instances'][0]['id'] = 'r'
        for net in main['nets']:
            for terminal in net['terminals']:
                if terminal.get('instance') == 'left':
                    terminal['instance'] = 'r'
        raw = json.dumps(value).encode()
        self.accepted(raw, 'rc', 'r', 'Nested r only')
        self.accepted(raw, 'main', 'r', 'Root r only')

    def test_semantic_noop_and_repeated_revisions(self):
        value = json.loads(RAW)
        instance(value, 'rc', 'r')['name'] = 'caf\u00e9'
        raw = json.dumps(value, ensure_ascii=True).encode().replace(b'"name"', b'"na\\u006de"')
        for name in ['caf\u00e9', 'x', 'a longer label', '\u03a9', '\u03a9', 'caf\u00e9']:
            raw = self.accepted(raw, 'rc', 'r', name)

    def test_invalid_selectors_and_names(self):
        for circuit_id, instance_id in [('missing', 'r'), ('rc', 'missing'), ('RC', 'r'), ('rc', ''), ('', 'r'), ('x' * 65, 'r'), ('rc', 'x' * 65), ('rc', 'r/../c'), ('rc', b'\xff')]:
            result = invoke(RAW, circuit_id, instance_id, 'valid')
            self.assertEqual((result['stage'], result['error']), ('revision', 'selector'))
            self.assertNotIn('source_hex', result)
        for name in ['', 'x' * 81, '\U0001f600' * 81, 'line\nfeed', 'nul\0', '\x7f', '\x85', b'\xff', b'\xed\xa0\x80']:
            result = invoke(RAW, 'rc', 'r', name)
            self.assertEqual(result['stage'], 'revision')
            self.assertNotIn('source_hex', result)
        self.accepted(RAW, 'rc', 'r', 'Still valid')

    def test_scalar_and_document_byte_limits(self):
        for name in ['x' * 80, '\U0001f600' * 80]:
            self.accepted(RAW, 'main', 'left', name)
        raw = RAW + b' ' * (1024 * 1024 - len(RAW))
        self.accepted(raw, 'main', 'left', 'left')
        self.accepted(raw, 'main', 'left', 'x')
        result = invoke(raw, 'main', 'left', 'x' * 80)
        self.assertEqual((result['stage'], result['error']), ('revision', 'bytes'))

    def test_invalid_base_is_not_repaired(self):
        for raw in [b'', b'{}', b'{"name":"x","name":"y"}', RAW.replace(b'"unconfigured"', b'"rollback"'), b' ' * (1024 * 1024 + 1)]:
            result = invoke(raw, 'rc', 'r', 'valid')
            self.assertEqual(result['stage'], 'base')
            self.assertNotIn('source_hex', result)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    PROBE = parser.parse_args().probe
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(InstanceLabel))
    print(f'{REQUESTS} instance revision requests checked against independent source/graph expectations')
    raise SystemExit(0 if result.wasSuccessful() else 1)
