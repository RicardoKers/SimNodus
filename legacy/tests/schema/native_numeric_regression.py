"""Exact Decimal oracle and inherited occurrence checks; no engine execution."""
import argparse
import copy
from decimal import Decimal, localcontext
import json
import struct
import subprocess
import unittest
from native_interface_regression import DOCUMENT, SOURCE, repin
import parameters

PROBE = None


def bind(doc, source=SOURCE, selected='resistor-interface'):
    raw = json.dumps(doc, ensure_ascii=False).encode()
    packet = b''.join(struct.pack('<I', len(v)) + v for v in (raw, selected.encode(), source))
    run = subprocess.run([PROBE], input=packet, capture_output=True, timeout=10)
    assert run.returncode in (0, 1), run.stderr
    lines = run.stdout.decode().splitlines()
    if run.returncode:
        return lines[0].split()[1:]
    assert lines[0] == 'OK' and lines[2] == '0 0 0'
    assert bytes.fromhex(lines[3]) == raw and bytes.fromhex(lines[4]) == source
    rows = [line.split() for line in lines[5:]]
    assert [row[0] for row in rows] == sorted(row[0] for row in rows)
    decoder = json.JSONDecoder()
    for row in rows:
        instance = decoder.raw_decode(raw[int(row[8]):].decode())[0]
        assert instance['definition'] == row[1] and instance['id'] == row[0].split('/')[-1]
        decoder.raw_decode(raw[int(row[9]):].decode())
    return {'default': lines[1], 'rows': rows}


def with_default(value):
    doc = copy.deepcopy(DOCUMENT)
    source = SOURCE.replace(b'value=1000', b'value=' + value.encode())
    repin(doc, source)
    return doc, source


class Numeric(unittest.TestCase):
    def test_fixture_inherited_values_and_provenance(self):
        doc = copy.deepcopy(DOCUMENT)
        topology = copy.deepcopy(doc['topology'])
        topology['version'] = '0.2';topology.pop('models');topology.pop('symbols')
        for definition in topology['components'] + topology['circuits']:
            definition.pop('symbol');definition.pop('model', None)
        oracle = parameters.resolve(topology)['instances']
        for selected, definition, parameter, fallback in [('resistor-interface', 'resistor', 'resistance', '1000'),
                                                         ('capacitor-interface', 'capacitor', 'capacitance', '0.000001')]:
            result = bind(doc, selected=selected)
            self.assertEqual(result['default'], fallback)
            expected = sorted([('/'.join(r['path']), r['parameters'][parameter]) for r in oracle if r['definition'] == definition])
            self.assertEqual(len(result['rows']), len(expected))
            for actual, (path, value) in zip(result['rows'], expected):
                self.assertEqual(actual[:5], [path, definition, parameter, 'p', 'n'])
                self.assertEqual(actual[5:8], [value['value'], value['unit'], value['origin']])

    def test_spice_suffixes_against_decimal(self):
        scales = {'': 0, 't': 12, 'G': 9, 'MEG': 6, 'k': 3, 'M': -3, 'u': -6, 'n': -9, 'p': -12, 'f': -15}
        for mantissa in ('1', '.125', '9.876543210123456e-18', '1e18'):
            for suffix, exponent in scales.items():
                with self.subTest(mantissa=mantissa, suffix=suffix), localcontext() as context:
                    context.prec = 100
                    doc, source = with_default(mantissa + suffix)
                    slot = doc['topology']['models'][0]['parameters'][0]
                    slot.update(minimum='0.00000000000000000001e-24', maximum='99999999999999999999999999999999e24')
                    expected = format((Decimal(mantissa) * Decimal(10) ** exponent).normalize(), 'f')
                    self.assertEqual(bind(doc, source)['default'], expected)

    def test_exact_default_boundaries(self):
        for value in ('1', '1meg', '1000000.00000000'):
            doc, source = with_default(value)
            self.assertIsInstance(bind(doc, source), dict)
        for value in ('.999999999999999', '1000000.000000001'):
            doc, source = with_default(value)
            self.assertEqual(bind(doc, source)[:2], ['1', 'range'])
        doc, source = with_default('1.000000000000001')
        doc['topology']['models'][0]['parameters'][0]['minimum'] = '1.0000000000000000000000000000001'
        # Keep complete declaration ranges consistent before testing the source boundary.
        for definition in doc['topology']['components'] + doc['topology']['circuits']:
            for p in definition['parameters']:
                if p['id'] == 'resistance': p['minimum'] = '2'
        self.assertIsInstance(bind(doc, source), dict)
        source = source.replace(b'1.000000000000001', b'1.000000000000000');repin(doc, source)
        self.assertEqual(bind(doc, source)[:2], ['1', 'range'])

    def test_zero_and_negative_effective_values(self):
        for value in ('0', '-0', '-1'):
            doc = copy.deepcopy(DOCUMENT)
            doc['topology']['models'][0]['parameters'][0]['minimum'] = '-1'
            for definition in doc['topology']['components'] + doc['topology']['circuits']:
                for p in definition['parameters']:
                    if p['id'] == 'resistance': p['minimum'] = '-1'
            doc['topology']['circuits'][0]['instances'][0]['overrides']['resistance'] = {'value': value, 'unit': 'ohm'}
            self.assertEqual(bind(doc)[:2], ['2', 'positive'])

    def test_source_order_reaches_logical_pins(self):
        doc = copy.deepcopy(DOCUMENT)
        doc['models']['resistor-interface']['terminal_map'] = {'first': 'second', 'second': 'first'}
        result = bind(doc)
        self.assertTrue(all(row[3:5] == ['n', 'p'] for row in result['rows']))
        doc['topology']['components'].reverse()
        for circuit in doc['topology']['circuits']: circuit['instances'].reverse()
        self.assertEqual([r[:8] for r in result['rows']], [r[:8] for r in bind(doc)['rows']])

    def test_interface_failures_are_retained(self):
        self.assertEqual(bind(DOCUMENT, SOURCE + b'\n')[:2], ['0', 'size'])
        self.assertEqual(bind(DOCUMENT, selected='missing')[:2], ['0', 'descriptor'])
        doc = copy.deepcopy(DOCUMENT);doc['extra'] = True
        self.assertEqual(bind(doc)[0], '0')

    def test_full_precision_effective_values(self):
        doc = copy.deepcopy(DOCUMENT)
        values = ['1000.' + '0' * 27 + '1', '1000.' + '0' * 27 + '2']
        for instance, value in zip(doc['topology']['circuits'][0]['instances'], values):
            instance['overrides']['resistance'] = {'value': value, 'unit': 'ohm'}
        self.assertEqual([row[5] for row in bind(doc)['rows']], values)

    def test_component_default_and_unused_descriptor(self):
        doc = copy.deepcopy(DOCUMENT)
        for circuit in doc['topology']['circuits']:
            for instance in circuit['instances']:
                if instance['definition'] == 'resistor': instance['overrides'] = {}
        result = bind(doc)
        self.assertTrue(all(row[5:8] == ['1000', 'ohm', 'default'] for row in result['rows']))
        doc['topology']['components'][0]['model'] = None
        self.assertEqual(bind(doc)['rows'], [])
        source = SOURCE.replace(b'value=1000', b'value=1e18');repin(doc, source)
        self.assertEqual(bind(doc, source)[:2], ['1', 'range'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser();parser.add_argument('--probe', required=True)
    args, remaining = parser.parse_known_args();PROBE = args.probe
    unittest.main(argv=[__file__] + remaining)
