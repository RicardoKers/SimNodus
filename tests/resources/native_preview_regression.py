# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Independent original SVG geometry, declared mapping and selected-only capture."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from collections import Counter

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'schema'))
import resource_lock as lock
import project as reference
from native_resistance_regression import verify_revision

FIXTURES = Path(__file__).resolve().parents[1] / 'schema/fixtures'
RAW = (FIXTURES / 'two-rc-project.json').read_bytes()
ART = (FIXTURES / 'assets/passive.svg').read_bytes()
PROBE = ''
REQUESTS = 0


def expected_geometry():
    """Walk the fixture's explicit M/H/V/Z commands independently of C++ constants."""
    svg = ET.fromstring(ART)
    x, y, width, height = map(float, svg.attrib['viewBox'].split())
    assert x == y == 0
    tokens = re.findall(r'[MHVZ]|-?\d+(?:\.\d+)?', next(iter(svg)).attrib['d'])
    lines = []
    position = 0
    while position < len(tokens):
        command = tokens[position]
        position += 1
        if command == 'M':
            px, py = float(tokens[position]), float(tokens[position + 1])
            start = (px, py)
            position += 2
            continue
        if command == 'Z':
            nx, ny = start
        elif command == 'H':
            nx, ny = float(tokens[position]), py
            position += 1
        else:
            assert command == 'V'
            nx, ny = px, float(tokens[position])
            position += 1
        lines.append([px, py, nx, ny])
        px, py = nx, ny
    return dict(width=width, height=height, lines=lines)


def selected_request(raw):
    document = reference.parse(raw)
    sources = document['sources']
    component = next(row for row in sources['topology']['components'] if row['id'] == 'resistor')
    symbol = component['symbol']['definition']
    asset_id = sources['symbols'][symbol]
    asset = next(row for row in sources['assets'] if row['id'] == asset_id)
    assert asset['kind'] == 'symbol-svg'
    dependency = next(row for row in sources['lock']['dependencies'] if row['id'] == asset['dependency'])
    resource = next(row for row in dependency['files'] if row['id'] == asset['resource'])
    request = dict(dependency=dependency['id'], resource=resource['id'], **{key: resource[key] for key in ('path', 'bytes', 'sha256')})
    return dict(component='resistor', symbol=symbol, asset=asset_id, request=request)


def expected_pins(raw):
    """Split touched edges, find degree-one lead ends, then apply named convention."""
    lines = expected_geometry()['lines']
    points = {(x, y) for x1, y1, x2, y2 in lines for x, y in ((x1, y1), (x2, y2))}
    degrees = Counter()
    for x1, y1, x2, y2 in lines:
        touched = sorted((x, y) for x, y in points if min(x1, x2) <= x <= max(x1, x2) and
                         min(y1, y2) <= y <= max(y1, y2))
        for a, b in zip(touched, touched[1:]):
            degrees[a] += 1
            degrees[b] += 1
    ends = sorted(point for point, degree in degrees.items() if degree == 1)
    assert len(ends) == 2 and ends[0][0] < ends[1][0]
    component = next(row for row in reference.parse(raw)['sources']['topology']['components'] if row['id'] == 'resistor')
    mapping = component['symbol']['pin_map']
    return [dict(logical_pin=next(key for key, value in mapping.items() if value == anchor),
                 symbol_pin=anchor, x=x, y=y) for anchor, (x, y) in zip(('a', 'b'), ends)]


def alternate_document():
    document = json.loads(RAW)
    document['sources']['topology']['components'][0]['symbol'] = dict(definition='two-pin-alt', pin_map={'p': 'left', 'n': 'right'})
    return document


def extra_pin_document():
    document = json.loads(RAW)
    topology = document['sources']['topology']
    topology['symbols'].append(dict(id='three-pin', name='Extra pin negative', pins=[dict(id=pin, name=pin) for pin in ('a', 'b', 'c')]))
    component = topology['components'][0]
    component['pins'].append(dict(id='third', name='third', domain='electrical'))
    component['symbol'] = dict(definition='three-pin', pin_map={'p': 'a', 'n': 'b', 'third': 'c'})
    component['model'] = None
    document['sources']['symbols']['three-pin'] = 'passive-symbol'
    reference.validate(document)
    return document


def invoke(mode, raw, *args):
    global REQUESTS
    REQUESTS += 1
    run = subprocess.run([PROBE, mode, *map(str, args)], input=raw, capture_output=True, timeout=25)
    result = json.loads(run.stdout)
    assert run.returncode == (1 if 'error' in result else 0), run.stderr
    return result


def rehashed_artwork(data):
    document = json.loads(RAW)
    dependency = document['sources']['lock']['dependencies'][0]
    resource = next(row for row in dependency['files'] if row['id'] == 'symbol')
    resource['bytes'] = len(data)
    resource['sha256'] = hashlib.sha256(data).hexdigest()
    dependency['content_sha256'] = lock.content_hash(dependency['files'])
    reference.validate(document)
    return json.dumps(document).encode()


class Preview(unittest.TestCase):
    def test_declared_pin_map_and_owned_positions(self):
        self.assertEqual(invoke('--pins', RAW, 'resistor'), expected_pins(RAW))
        document = json.loads(RAW)
        component = document['sources']['topology']['components'][0]
        component['symbol']['pin_map'] = {'n': 'a', 'p': 'b'}
        component['pins'].reverse()
        document['sources']['topology']['symbols'][0]['pins'].reverse()
        document['sources']['topology']['components'].reverse()
        component['pins'][0]['name'] = '<b>duplicate Ω</b>'
        component['pins'][1]['name'] = '<b>duplicate Ω</b>'
        raw = json.dumps(document, ensure_ascii=False, sort_keys=True).encode().replace(b'"pin_map"', b'"\\u0070in_map"')
        self.assertEqual(invoke('--pins', raw, 'resistor'), expected_pins(raw))

    def test_unknown_descriptor_and_anchor_policy(self):
        document = alternate_document()
        self.assertEqual(invoke('--pins', json.dumps(document).encode(), 'resistor'), dict(error='anchors', stage=0))
        document = json.loads(RAW)
        document['sources']['topology']['symbols'][0]['pins'][1]['id'] = 'unknown'
        document['sources']['topology']['components'][0]['symbol']['pin_map']['n'] = 'unknown'
        document['sources']['topology']['components'][1]['symbol']['pin_map']['n'] = 'unknown'
        self.assertEqual(invoke('--pins', json.dumps(document).encode(), 'resistor'), dict(error='anchors', stage=0))
        document = json.loads(RAW)
        document['sources']['topology']['components'][0]['symbol'] = None
        self.assertEqual(invoke('--pins', json.dumps(document).encode(), 'resistor'), dict(error='symbol', stage=0))
        self.assertEqual(invoke('--pins', RAW, 'capacitor'), dict(error='component', stage=0))
        self.assertIn('error', invoke('--pins', b'{', 'resistor'))
        self.assertEqual(invoke('--pins', json.dumps(extra_pin_document()).encode(), 'resistor'), dict(error='anchors', stage=0))
        document = json.loads(RAW)
        document['sources']['topology']['components'][0]['symbol']['pin_map']['n'] = 'a'
        self.assertIn('error', invoke('--pins', json.dumps(document).encode(), 'resistor'))

    def test_capture_pins_and_complete_unavailable_maps(self):
        if os.name != 'nt':
            print('SKIP Windows local NTFS owned pin capture')
            return
        base = Path('build/sn023-pin-preview/native-tests').resolve()
        base.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='pins-', dir=base) as temporary:
            root = Path(temporary)
            path = root / selected_request(RAW)['request']['path']
            path.parent.mkdir(parents=True)
            path.write_bytes(ART)
            self.assertEqual(invoke('--pin-capture', RAW, 'resistor', root), dict(pins=expected_pins(RAW), pin_map=[['n', 'b'], ['p', 'a']], captured_files=1))
            document = alternate_document()
            for mapping in ({'n': 'right', 'p': 'left'}, {'p': 'right', 'n': 'left'}):
                document['sources']['topology']['components'][0]['symbol']['pin_map'] = mapping
                result = invoke('--pin-capture', json.dumps(document).encode(), 'resistor', root)
                self.assertIsNone(result['pins'])
                self.assertEqual(result['captured_files'], 1)
                self.assertEqual(result['pin_map'], sorted([key, value] for key, value in mapping.items()))
            result = invoke('--pin-capture', json.dumps(extra_pin_document()).encode(), 'resistor', root)
            self.assertIsNone(result['pins'])
            self.assertEqual(result['pin_map'], [['n', 'b'], ['p', 'a'], ['third', 'c']])

    def test_owned_original_geometry(self):
        self.assertEqual(invoke('--decode', ART), expected_geometry())
        self.assertEqual(len(expected_geometry()['lines']), 6)

    def test_every_altered_artwork_refused(self):
        variants = [b'', ART[:-1], ART + b' ', ART.replace(b'\n', b'\r\n'), ART.replace(b'H25', b'H26', 1),
                    ART.replace(b'<path', b'<script>host()</script><path'),
                    ART.replace(b'<path', b'<image href="file:///external"/><path'), b'x' * (1024 * 1024 + 1)]
        for raw in variants:
            self.assertEqual(invoke('--decode', raw), dict(error='artwork', stage=2))

    def test_mapping_order_escaped_keys_duplicate_labels(self):
        self.assertEqual(invoke('--select', RAW, 'resistor'), selected_request(RAW))
        document = json.loads(RAW)
        sources = document['sources']
        sources['topology']['components'].reverse()
        sources['assets'].reverse()
        sources['lock']['dependencies'][0]['files'].reverse()
        for component in sources['topology']['components']:
            component['name'] = '<b>same Ω label</b>'
        raw = json.dumps(document, sort_keys=True, ensure_ascii=False).encode().replace(b'"symbol"', b'"\\u0073ymbol"')
        self.assertEqual(invoke('--select', raw, 'resistor'), selected_request(raw))

    def test_unsupported_null_and_untrusted(self):
        for component in ('capacitor', '', 'missing', 'resistor' * 10):
            self.assertEqual(invoke('--select', RAW, component), dict(error='component', stage=0))
        document = json.loads(RAW)
        document['sources']['topology']['components'][0]['symbol'] = None
        self.assertEqual(invoke('--select', json.dumps(document).encode(), 'resistor'), dict(error='symbol', stage=0))
        document = json.loads(RAW)
        document['sources']['symbols']['two-pin'] = None
        self.assertEqual(invoke('--select', json.dumps(document).encode(), 'resistor'), dict(error='symbol', stage=0))
        self.assertIn('error', invoke('--select', b'{', 'resistor'))

    def test_rehashed_valid_svg_still_unsupported(self):
        changed = ART.replace(b'H25', b'H26', 1)
        self.assertEqual(invoke('--select', rehashed_artwork(changed), 'resistor'), dict(error='artwork', stage=0))

    def test_selected_only_capture_and_refusals(self):
        base = Path('build/sn023-fixture-preview/native-tests').resolve()
        base.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='capture-', dir=base) as temporary:
            root = Path(temporary)
            path = root / selected_request(RAW)['request']['path']
            path.parent.mkdir(parents=True)
            path.write_bytes(ART)
            if os.name != 'nt':
                self.assertEqual(invoke('--capture', RAW, 'resistor', 'C:/explicit-fixture-root'), dict(error='platform', stage=1))
                print('SKIP Windows local NTFS selected-symbol capture')
                return
            self.assertEqual(invoke('--capture', RAW, 'resistor', root), dict(selection=selected_request(RAW),
                geometry=expected_geometry(), captured_files=1, captured_bytes=227))
            self.assertFalse((root / 'LICENSE').exists())
            self.assertFalse((root / 'tests/schema/fixtures/assets/passive.cir').exists())
            path.write_bytes(ART[:-1])
            self.assertEqual(invoke('--capture', RAW, 'resistor', root), dict(error='size', stage=1))
            path.write_bytes(ART.replace(b'H25', b'H26', 1))
            self.assertEqual(invoke('--capture', RAW, 'resistor', root), dict(error='hash', stage=1))
            path.unlink()
            self.assertEqual(invoke('--capture', RAW, 'resistor', root), dict(error='filesystem', stage=1))

    def test_application_lifecycle_and_exact_copy(self):
        base = Path('build/sn023-fixture-preview/native-tests').resolve()
        base.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix='lifecycle-', dir=base) as temporary:
            root = Path(temporary)
            path = root / selected_request(RAW)['request']['path']
            path.parent.mkdir(parents=True)
            path.write_bytes(ART)
            (root / 'original.json').write_bytes(RAW)
            run = subprocess.run([PROBE, '--lifecycle', str(root)], env=dict(os.environ), capture_output=True, text=True, timeout=30)
            print(run.stdout, end='')
            self.assertEqual(run.returncode, 0, run.stderr)
            if os.name == 'nt':
                revised = RAW.replace(b'"Two RC declaration project"', b'"Fixture preview project"', 1)
                verify_revision(revised, (root / 'preview-copy.json').read_bytes(), 'main', 'right', '3.5')
                self.assertEqual((root / 'original.json').read_bytes(), RAW)
                print('PASS independent persisted-byte audit and unchanged document source')


def main():
    global PROBE
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    PROBE = str(Path(parser.parse_args().probe).resolve())
    result = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(Preview))
    print(f'{REQUESTS} independent fixture artwork requests')
    raise SystemExit(0 if result.wasSuccessful() else 1)


if __name__ == '__main__':
    main()
