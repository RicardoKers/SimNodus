# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Closed capacitance command: independent bytes/graph/parameter expectations."""
import argparse
import json
import unittest

import bindings
import parameters
import project as reference
import native_resistance_regression as shared
from native_graph_regression import expected_graph, expected_positions
from native_instance_label_regression import instance

RAW = shared.RAW


def verify_revision(original, revised, circuit_id, instance_id, value):
    return shared.verify_revision(original, revised, circuit_id, instance_id, value, 'capacitance')


class Capacitance(unittest.TestCase):
    def accepted(self, raw, circuit, selected, value):
        result = shared.invoke(raw, circuit, selected, value, '--capacitance')
        self.assertNotIn('error', result)
        revised = bytes.fromhex(result['source_hex'])
        expected = verify_revision(raw, revised, circuit, selected, value)
        topology = expected['sources']['topology']
        self.assertEqual(result['graph'], expected_graph(topology))
        positions = expected_positions(topology)
        self.assertEqual({tuple(row['identity']) for row in result['positions']}, set(positions))
        for row in result['positions']:
            self.assertEqual(json.loads(revised[row['begin']:row['end']]), positions[tuple(row['identity'])])
        self.assertEqual(result['parameters'], parameters.resolve(bindings.projection(topology))['instances'])
        item = instance(expected, circuit, selected)
        target = next(d for d in topology['components'] + topology['circuits'] if d['id'] == item['definition'])
        declaration = next(p for p in target['parameters'] if p['id'] == 'capacitance')
        self.assertEqual(result['literal'], dict(value=value, unit=item['overrides']['capacitance']['unit'],
            minimum=declaration['minimum'], maximum=declaration['maximum'], base_unit='F'))
        return revised

    def refused(self, raw, circuit, selected, value, stage='revision', code=None):
        result = shared.invoke(raw, circuit, selected, value, '--capacitance')
        self.assertEqual(result.get('stage'), stage)
        self.assertIn('error', result)
        if code: self.assertEqual(result['error'], code)
        self.assertNotIn('source_hex', result)

    def test_boundaries_and_affected_subtree(self):
        for value in ('1', '1000000', '470', '220.0', '22e1', '1e0'):
            revised = self.accepted(RAW, 'main', 'right', value)
            old = parameters.resolve(bindings.projection(json.loads(RAW)['sources']['topology']))['instances']
            new = parameters.resolve(bindings.projection(json.loads(revised)['sources']['topology']))['instances']
            for a, b in zip(old, new):
                if a['path'][:2] != ['main', 'right']:
                    self.assertEqual(a, b)
                else:
                    self.assertEqual({k:v for k,v in a['parameters'].items() if k != 'capacitance'},
                                     {k:v for k,v in b['parameters'].items() if k != 'capacitance'})
        for value in ('0.999999999999999999', '1000000.000000000001', '-1', '0'):
            self.refused(RAW, 'main', 'right', value, code='range')

    def test_units_and_selected_target_limits(self):
        for unit, low, high, middle in [('F', '0.000000001', '0.001', '0.00000047'),
                                      ('uF', '0.001', '1000', '0.47'), ('nF', '1', '1000000', '470')]:
            value = json.loads(RAW)
            instance(value, 'main', 'right')['overrides']['capacitance'] = {'value': middle, 'unit': unit}
            raw = json.dumps(value).encode()
            for number in (low, high, middle): self.accepted(raw, 'main', 'right', number)
        value = json.loads(RAW)
        rc = next(c for c in value['sources']['topology']['circuits'] if c['id'] == 'rc')
        declaration = next(p for p in rc['parameters'] if p['id'] == 'capacitance')
        declaration.update(minimum='0.00000001', maximum='0.00001')
        raw = json.dumps(value).encode()
        for number in ('10', '10000'): self.accepted(raw, 'main', 'right', number)
        for number in ('1', '100000'): self.refused(raw, 'main', 'right', number, code='range')

    def test_invalid_numeric_text(self):
        for value in ('', '+470', '0470', '470E0', '470.', '.5', '1e25', '1e01', '1e', 'NaN', 'inf', '470 nF',
                      '470\n', '1"}', '1' * 33, '\u03a9', b'\xff', b'\xed\xa0\x80', 'x' * 65):
            self.refused(RAW, 'main', 'right', value)

    def test_missing_forwarded_and_wrong_dimension(self):
        for circuit, selected in (('', 'right'), ('main', ''), ('absent', 'right'), ('main', 'absent'),
                                  ('main', 'left'), ('rc', 'c'), ('rc', 'r'), ('main' * 17, 'right')):
            self.refused(RAW, circuit, selected, '470', code='selector')
        value = json.loads(RAW)
        del instance(value, 'main', 'right')['overrides']['capacitance']
        self.refused(json.dumps(value).encode(), 'main', 'right', '470', code='selector')
        value = json.loads(RAW)
        for d in value['sources']['topology']['components'] + value['sources']['topology']['circuits']:
            for p in d['parameters']:
                if p['id'] == 'capacitance': p['unit'] = 's'
        for model in value['sources']['topology']['models']:
            for p in model['parameters']:
                if p['unit'] == 'F': p['unit'] = 's'
        instance(value, 'main', 'right')['overrides']['capacitance'] = {'value': '0.00000022', 'unit': 's'}
        raw = json.dumps(value).encode()
        reference.validate(reference.parse(raw))
        self.refused(raw, 'main', 'right', '0.00000047', code='selector')

    def test_escaped_keys_reordering_noop_and_spelling(self):
        value = json.loads(RAW)
        value['sources']['topology']['circuits'].reverse()
        for circuit in value['sources']['topology']['circuits']:
            circuit['instances'].reverse()
            for item in circuit['instances']: item['name'] = 'same label'
        raw = json.dumps(value, sort_keys=True).encode().replace(b'"overrides"', b'"ov\\u0065rrides"').replace(b'"value"', b'"val\\u0075e"').replace(b'"capacitance"', b'"capacit\\u0061nce"')
        self.accepted(raw, 'main', 'right', '470')
        escaped = RAW.replace(b'"value": "220"', b'"value": "\\u0032\\u0032\\u0030"', 1)
        self.assertEqual(self.accepted(escaped, 'main', 'right', '220'), escaped)
        revised = self.accepted(escaped, 'main', 'right', '220.0')
        self.assertNotEqual(revised, escaped)
        for value in ('470', '330', '220'): revised = self.accepted(revised, 'main', 'right', value)

    def test_byte_ceiling_and_invalid_base(self):
        padded = RAW + b' ' * (1048576 - len(RAW))
        self.assertEqual(self.accepted(padded, 'main', 'right', '220'), padded)
        self.refused(padded, 'main', 'right', '220.0', code='bytes')
        longer = RAW.replace(b'"value": "220"', b'"value": "220.0000"', 1)
        longer += b' ' * (1048576 - len(longer))
        self.accepted(longer, 'main', 'right', '220')
        self.refused(padded + b' ', 'main', 'right', '220', stage='base', code='input')
        self.refused(b'{', 'main', 'right', '470', stage='base')
        self.refused(RAW.replace(b'"unit": "nF"', b'"unit": "ohm"', 1), 'main', 'right', '470', stage='base', code='unit')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    shared.PROBE = parser.parse_args().probe
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Capacitance))
    print(f'{shared.REQUESTS} capacitance requests checked against independent bytes, graph/source and exact parameter expectations')
    raise SystemExit(not result.wasSuccessful())
