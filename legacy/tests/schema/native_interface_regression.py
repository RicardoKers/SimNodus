"""Selected passive interface correspondence without filesystem/engine authority."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import unittest
import resource_lock

PROBE = None
DOCUMENT = json.loads(Path('tests/schema/fixtures/two-rc-resource-links.json').read_text())
SOURCE = Path('tests/schema/fixtures/assets/passive.cir').read_bytes()


def repin(doc, source):
    dep = doc['lock']['dependencies'][0]
    row = next(r for r in dep['files'] if r['id'] == 'model')
    row.update(bytes=len(source), sha256=hashlib.sha256(source).hexdigest())
    dep['content_sha256'] = resource_lock.content_hash(dep['files'])


def match(doc, source=SOURCE, selected='resistor-interface'):
    raw = json.dumps(doc, ensure_ascii=False).encode()
    packet = b''.join(struct.pack('<I', len(v)) + v for v in (raw, selected.encode(), source))
    process = subprocess.run([PROBE], input=packet, capture_output=True, timeout=10)
    assert process.returncode in (0, 1), process.stderr
    lines = process.stdout.decode().splitlines()
    if process.returncode:
        assert lines[0].startswith('ERR ')
        return lines[0].split()[1:]
    assert lines[0] == 'OK' and lines[4] == '0 0 0'
    assert bytes.fromhex(lines[5]) == raw and bytes.fromhex(lines[6]) == source
    index, desc, binding = map(int, lines[3].split())
    decoder = json.JSONDecoder()
    assert decoder.raw_decode(raw[desc:].decode())[0] == next(m for m in doc['topology']['models'] if m['id'] == selected)
    assert decoder.raw_decode(raw[binding:].decode())[0] == doc['models'][selected]
    return {'identity': lines[1].split(), 'interface': lines[2].split(), 'model_index': index}


class Interfaces(unittest.TestCase):
    def setUp(self):
        self.doc = copy.deepcopy(DOCUMENT)

    def test_owned_baseline(self):
        r = match(self.doc)
        self.assertEqual(r['identity'], ['resistor-interface', 'passive-model', 'owned-link-fixture', 'model'])
        self.assertEqual(r['interface'], ['first', 'second', 'value', 'ohm', '1', '1000000'])
        c = match(self.doc, selected='capacitor-interface')
        self.assertEqual(c['interface'], ['first', 'second', 'value', 'F', '0.000000001', '0.001'])
        self.assertEqual(c['model_index'], 1)

    def test_explicit_mapping_and_order(self):
        binding = self.doc['models']['resistor-interface']
        binding['terminal_map'] = {'first': 'second', 'second': 'first'}
        self.doc['topology']['models'].reverse()
        for model in self.doc['topology']['models']:
            model['terminals'].reverse()
        self.assertEqual(match(self.doc)['interface'][:2], ['second', 'first'])

    def test_case_is_source_only(self):
        b = self.doc['models']['resistor-interface']
        b['entrypoint'] = b['entrypoint'].upper()
        b['terminal_map'] = {k: v.upper() for k, v in b['terminal_map'].items()}
        b['parameter_map']['value'] = 'VALUE'
        self.assertIsInstance(match(self.doc), dict)
        self.assertEqual(match(self.doc, selected='RESISTOR-INTERFACE')[1], 'descriptor')

    def test_missing_absent_and_maps(self):
        self.assertEqual(match(self.doc, selected='missing')[1], 'descriptor')
        self.doc['models']['resistor-interface'] = None
        self.assertEqual(match(self.doc)[1], 'absent')
        for field, value, code in [('entrypoint', 'missing', 'entrypoint'),
                                   ('terminal_map', {'first': 'other', 'second': 'second'}, 'terminal'),
                                   ('parameter_map', {'value': 'other'}, 'parameter')]:
            doc = copy.deepcopy(DOCUMENT);doc['models']['resistor-interface'][field] = value
            self.assertEqual(match(doc)[1], code)

    def test_unit_from_primitive(self):
        source = SOURCE.replace(b'Rbody', b'Cbody')
        repin(self.doc, source)
        self.assertEqual(match(self.doc, source)[1], 'unit')

    def test_byte_identity_and_grammar(self):
        self.assertEqual(match(self.doc, SOURCE + b'\n')[1], 'size')
        changed = SOURCE.replace(b'1000', b'2000')
        self.assertEqual(match(self.doc, changed)[1], 'hash')
        bad = SOURCE + b'.include external\n';repin(self.doc, bad)
        self.assertEqual(match(self.doc, bad)[:2], ['2', 'header'])
        self.assertEqual(match(self.doc, b'*' * 65537)[:2], ['2', 'bytes'])

    def test_complete_metadata_gate(self):
        self.doc['unexpected'] = True
        self.assertEqual(match(self.doc)[0], '0')
        self.assertEqual(match(DOCUMENT, selected='x' * 65)[:2], ['1', 'descriptor'])

    def test_default_range_is_not_certified(self):
        # Correspondence must not silently claim numerical/default compatibility.
        source = SOURCE.replace(b'value=1000', b'value=1e18')
        repin(self.doc, source)
        self.assertIsInstance(match(self.doc, source), dict)


if __name__ == '__main__':
    parser = argparse.ArgumentParser();parser.add_argument('--probe', required=True)
    args, remaining = parser.parse_known_args();PROBE = args.probe
    unittest.main(argv=[__file__] + remaining)
