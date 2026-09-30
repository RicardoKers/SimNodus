# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Exact literal token and independent effective-parameter revision oracle."""
import argparse
import copy
import json
from pathlib import Path
import struct
import subprocess
import unittest

import parameters
import bindings
import project as reference
from native_graph_regression import expected_graph, expected_positions
from native_instance_label_regression import instance, name_span

RAW = (Path(__file__).parent / 'fixtures/two-rc-project.json').read_bytes()
PROBE = ''
REQUESTS = 0


def verify_revision(original, revised, circuit_id, instance_id, value):
    before = reference.parse(original)
    expected = copy.deepcopy(before)
    literal = instance(expected, circuit_id, instance_id)['overrides']['resistance']
    old = literal['value']
    literal['value'] = value
    reference.validate(expected)
    assert reference.parse(revised) == expected
    begin, end = name_span(original, circuit_id, instance_id, ('overrides', 'resistance', 'value'))
    encoded = json.dumps(value, ensure_ascii=False).encode()
    assert revised == (original if old == value else original[:begin] + encoded + original[end:])
    return expected


def invoke(raw, circuit_id, instance_id, value):
    global REQUESTS
    REQUESTS += 1
    fields = [v.encode() if isinstance(v, str) else v for v in (raw, circuit_id, instance_id, value)]
    packet = b''.join(struct.pack('<I', len(v)) + v for v in fields)
    run = subprocess.run([PROBE, '--resistance'], input=packet, capture_output=True, timeout=20)
    result = json.loads(run.stdout)
    assert run.returncode == (1 if 'error' in result else 0), run.stderr
    return result


class Resistance(unittest.TestCase):
    def accepted(self, raw, circuit_id, instance_id, value):
        result = invoke(raw, circuit_id, instance_id, value)
        self.assertNotIn('error', result)
        revised = bytes.fromhex(result['source_hex'])
        expected = verify_revision(raw, revised, circuit_id, instance_id, value)
        topology = expected['sources']['topology']
        self.assertEqual(result['graph'], expected_graph(topology))
        positions = expected_positions(topology)
        self.assertEqual({tuple(row['identity']) for row in result['positions']}, set(positions))
        for row in result['positions']:
            self.assertEqual(json.loads(revised[row['begin']:row['end']]), positions[tuple(row['identity'])])
        self.assertEqual(result['parameters'], parameters.resolve(bindings.projection(topology))['instances'])
        literal = instance(expected, circuit_id, instance_id)['overrides']['resistance']
        target_id = instance(expected, circuit_id, instance_id)['definition']
        target = next(d for d in topology['components'] + topology['circuits'] if d['id'] == target_id)
        declaration = next(p for p in target['parameters'] if p['id'] == 'resistance')
        self.assertEqual(result['literal'], dict(value=value, unit=literal['unit'], minimum=declaration['minimum'],
            maximum=declaration['maximum'], base_unit=declaration['unit']))
        return revised

    def refused(self, raw, circuit, selected, value, stage='revision', code=None):
        result = invoke(raw, circuit, selected, value)
        self.assertEqual(result.get('stage'), stage)
        self.assertIn('error', result)
        if code:
            self.assertEqual(result['error'], code)
        self.assertNotIn('source_hex', result)

    def test_exact_target_range_and_effective_subtree(self):
        for value in ('0.1', '10', '3.5', '1.0', '1e0', '1e-1', '10e-1'):
            revised = self.accepted(RAW, 'main', 'left', value)
            old = parameters.resolve(bindings.projection(json.loads(RAW)['sources']['topology']))['instances']
            new = parameters.resolve(bindings.projection(json.loads(revised)['sources']['topology']))['instances']
            for a, b in zip(old, new):
                if a['path'][:2] != ['main', 'left']:
                    self.assertEqual(a, b)
                else:
                    self.assertEqual({k: v for k, v in a['parameters'].items() if k != 'resistance'},
                                     {k: v for k, v in b['parameters'].items() if k != 'resistance'})
        for value in ('0.099999999999999999999', '10.000000000000000001', '-1', '0'):
            self.refused(RAW, 'main', 'left', value, code='range')

    def test_decimal_and_invalid_input(self):
        for value in ('', '+1', '01', '1E0', '1.', '.5', '1e25', '1e01', '1e', 'NaN', 'inf', '1 kohm', '1\n',
                      '1"}', '1' * 33, '\u03a9', b'\xff', b'\xed\xa0\x80', 'x' * 65):
            self.refused(RAW, 'main', 'left', value)

    def test_missing_forwarded_and_dimension(self):
        for circuit, selected in (('', 'left'), ('main', ''), ('absent', 'left'), ('main', 'absent'),
                                  ('rc', 'r'), ('rc', 'c'), ('main' * 17, 'left')):
            self.refused(RAW, circuit, selected, '2', code='selector')
        value = json.loads(RAW)
        del instance(value, 'main', 'left')['overrides']['resistance']
        self.refused(json.dumps(value).encode(), 'main', 'left', '2', code='selector')
        value = json.loads(RAW)
        instance(value, 'main', 'left')['overrides']['resistance'] = {'value': '1000', 'unit': 'ohm'}
        self.accepted(json.dumps(value).encode(), 'main', 'left', '3500')
        value = json.loads(RAW)
        for definition in value['sources']['topology']['components'] + value['sources']['topology']['circuits']:
            for p in definition['parameters']:
                if p['id'] == 'resistance':
                    p['unit'] = 'F'
        for model in value['sources']['topology']['models']:
            for p in model['parameters']:
                if p['unit'] == 'ohm': p['unit'] = 'F'
        # A fully valid alternate dimension must not become a resistance editor.
        for selected in ('left', 'right'):
            instance(value, 'main', selected)['overrides']['resistance']['unit'] = 'F'
            instance(value, 'main', selected)['overrides']['resistance']['value'] = '1000'
        raw = json.dumps(value).encode()
        reference.validate(reference.parse(raw))
        self.refused(raw, 'main', 'left', '2000', code='selector')

    def test_reordered_escaped_keys_and_labels(self):
        value = json.loads(RAW)
        value['sources']['topology']['circuits'].reverse()
        for circuit in value['sources']['topology']['circuits']:
            circuit['instances'].reverse()
            for selected in circuit['instances']: selected['name'] = 'same label'
        raw = json.dumps(value, sort_keys=True).encode().replace(b'"overrides"', b'"ov\\u0065rrides"').replace(b'"value"', b'"val\\u0075e"').replace(b'"resistance"', b'"resist\\u0061nce"')
        self.accepted(raw, 'main', 'left', '3.5')
        self.accepted(raw, 'main', 'right', '4.7')

    def test_noop_and_repeated_revision(self):
        escaped = RAW.replace(b'"value": "1"', b'"value": "\\u0031"', 1)
        self.assertEqual(self.accepted(escaped, 'main', 'left', '1'), escaped)
        revised = self.accepted(escaped, 'main', 'left', '1.0')
        self.assertNotEqual(revised, escaped)
        for value in ('3.5', '4.7', '1'):
            revised = self.accepted(revised, 'main', 'left', value)

    def test_byte_ceiling(self):
        padded = RAW + b' ' * (1048576 - len(RAW))
        self.assertEqual(self.accepted(padded, 'main', 'left', '1'), padded)
        self.refused(padded, 'main', 'left', '2.2', code='bytes')
        longer = RAW.replace(b'"value": "1"', b'"value": "1.0000"', 1)
        longer += b' ' * (1048576 - len(longer))
        self.accepted(longer, 'main', 'left', '1')
        self.refused(padded + b' ', 'main', 'left', '1', stage='base', code='input')

    def test_invalid_base_never_repaired(self):
        self.refused(b'{', 'main', 'left', '2', stage='base')
        invalid = RAW.replace(b'"unit": "kohm"', b'"unit": "nF"', 1)
        self.refused(invalid, 'main', 'left', '2', stage='base', code='unit')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    PROBE = parser.parse_args().probe
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Resistance))
    print(f'{REQUESTS} resistance requests checked against independent bytes, graph/source and exact parameter expectations')
    raise SystemExit(not result.wasSuccessful())
