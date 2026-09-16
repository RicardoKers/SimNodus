"""Explicit E-01 compilation; real NTFS capture on Windows, no engine launch."""
import argparse
import copy
import json
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from native_interface_regression import repin

ROOT = Path(__file__).resolve().parents[2]
PROBE = None


def project():
    doc = json.loads((ROOT / 'tests/schema/fixtures/two-rc-project.json').read_text(encoding='utf-8'))
    main = doc['sources']['topology']['circuits'][0]
    main['instances'] = [i for i in main['instances'] if i['id'] == 'left']
    main['ports'] = [p for p in main['ports'] if p['id'] != 'right_out']
    for net in main['nets']:
        net['terminals'] = [t for t in net['terminals'] if t.get('instance') != 'right' and t.get('port') != 'right_out']
    main['nets'] = [n for n in main['nets'] if len(n['terminals']) >= 2]
    return doc


def populate(root, doc):
    for dep in doc['sources']['lock']['dependencies']:
        for row in dep['files']:
            target = root / row['path'];target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((ROOT / row['path']).read_bytes())


def compile_project(doc, root, profile='e01', reference='reference', drive='source', output='left_out'):
    raw = json.dumps(doc, ensure_ascii=False).encode()
    packet = b''.join(struct.pack('<I', len(v)) + v for v in [raw, str(root).encode(), profile.encode(), reference.encode(), drive.encode(), output.encode()])
    run = subprocess.run([PROBE], input=packet, capture_output=True, timeout=30)
    assert run.returncode in (0, 1), run.stderr
    lines = run.stdout.decode().splitlines()
    if run.returncode:
        assert lines[0].startswith('ERR ')
        return lines[0].split()[1:]
    assert lines[0] == 'OK' and bytes.fromhex(lines[2]) == raw and lines[3] == '0 0'
    assert lines[6] == 'NODES'
    elements = [line.split() for line in lines[4:6]]
    decoder = json.JSONDecoder()
    for e in elements:
        assert json.loads(raw[int(e[3]):int(e[4])])['id'] == e[0].split('/')[-1]
        decoder.raw_decode(raw[int(e[2]):].decode())
    return {'netlist': bytes.fromhex(lines[1]), 'elements': elements, 'nodes': [l.split() for l in lines[7:]]}


class CompileRc(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='simnodus-rc-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name);self.doc = project();populate(self.root, self.doc)

    def test_explicit_request_and_complete_metadata(self):
        self.assertEqual(compile_project(self.doc, self.root, profile='')[:2], ['1', 'profile'])
        self.assertEqual(compile_project(self.doc, self.root, output='unknown')[:2], ['1', 'terminal'])
        self.assertEqual(compile_project(self.doc, self.root, output='source')[:2], ['1', 'distinct-nodes'])
        self.doc['extra'] = True
        self.assertEqual(compile_project(self.doc, self.root)[0], '0')

    def test_extra_connectivity_rejected_before_io(self):
        doc = json.loads((ROOT / 'tests/schema/fixtures/two-rc-project.json').read_text(encoding='utf-8'))
        self.assertEqual(compile_project(doc, self.root / 'absent')[:2], ['4', 'connections'])

    def test_aliases_are_physical_connections_not_port_names(self):
        main = self.doc['sources']['topology']['circuits'][0]
        alias = copy.deepcopy(main['ports'][0]);alias['id'] = 'drive_alias';main['ports'].append(alias)
        main['nets'][0]['terminals'].append({'port': 'drive_alias'})
        self.assertEqual(compile_project(self.doc, self.root, output='drive_alias')[:2], ['1', 'distinct-nodes'])
        main['nets'][0]['terminals'].pop()
        self.assertEqual(compile_project(self.doc, self.root)[:2], ['4', 'connections'])

    @unittest.skipUnless(os.name != 'nt', 'Unsupported-platform rejection is a non-Windows check')
    def test_unsupported_platform(self):
        self.assertEqual(compile_project(self.doc, self.root)[:2], ['2', 'root'])
        self.assertEqual(compile_project(self.doc, r'C:\unused-package')[:2], ['2', 'platform'])

    @unittest.skipUnless(os.name == 'nt', 'Physical compiler requires Windows/NTFS')
    def test_compilation_and_provenance(self):
        result = compile_project(self.doc, self.root)
        self.assertEqual(result['netlist'].splitlines()[1:4], [b'Vdrive in 0 3.3', b'R1 in out 1000', b'C1 out 0 0.000001 IC=0'])
        self.assertEqual([e[:2] for e in result['elements']], [['main/left/r', '3'], ['main/left/c', '4']])
        self.assertIn(['0', 'main', 'reference'], result['nodes'])
        self.assertIn(['in', 'main/left/r', 'p'], result['nodes'])
        self.assertIn(['out', 'main/left/c', 'p'], result['nodes'])
        source = (self.root / 'tests/schema/fixtures/assets/passive.cir').read_bytes()
        for e in result['elements']:
            self.assertTrue(source[int(e[7]):int(e[8])].startswith(b'.subckt '))
        doc = copy.deepcopy(self.doc)
        for c in doc['sources']['topology']['circuits']:
            c['ports'].reverse();c['instances'].reverse();c['nets'].reverse()
        self.assertEqual(compile_project(doc, self.root)['netlist'], result['netlist'])

    @unittest.skipUnless(os.name == 'nt', 'Physical compiler requires Windows/NTFS')
    def test_physical_gate(self):
        path = self.root / 'tests/schema/fixtures/assets/passive.cir'
        data = path.read_bytes();path.write_bytes(data.replace(b'1000', b'2000'))
        self.assertEqual(compile_project(self.doc, self.root)[:2], ['2', 'hash'])
        path.unlink()
        self.assertEqual(compile_project(self.doc, self.root)[0], '2')

    @unittest.skipUnless(os.name == 'nt', 'Physical compiler requires Windows/NTFS')
    def test_profile_values_and_wiring(self):
        doc = copy.deepcopy(self.doc)
        doc['sources']['topology']['circuits'][0]['instances'][0]['overrides']['resistance']['value'] = '2'
        self.assertEqual(compile_project(doc, self.root)[:2], ['4', 'value'])
        doc = copy.deepcopy(self.doc)
        doc['sources']['models']['resistor-interface']['terminal_map'] = {'first': 'second', 'second': 'first'}
        self.assertEqual(compile_project(doc, self.root)[:2], ['4', 'wiring'])
        doc['sources']['models']['resistor-interface'] = None
        self.assertEqual(compile_project(doc, self.root)[:2], ['3', 'absent'])
        doc = copy.deepcopy(self.doc);doc['sources']['topology']['components'][0]['model'] = None
        self.assertEqual(compile_project(doc, self.root)[:2], ['4', 'missing-model'])

    @unittest.skipUnless(os.name == 'nt', 'Physical compiler requires Windows/NTFS')
    def test_matching_hash_does_not_approve_directives(self):
        path = self.root / 'tests/schema/fixtures/assets/passive.cir'
        data = path.read_bytes() + b'.include external\n'
        path.write_bytes(data);repin(self.doc['sources'], data)
        result = compile_project(self.doc, self.root)
        self.assertEqual(result[:2], ['3', 'header'])
        self.assertEqual(result[-1], 'model-source')


if __name__ == '__main__':
    parser = argparse.ArgumentParser();parser.add_argument('--probe', required=True)
    args, remaining = parser.parse_known_args();PROBE = args.probe
    unittest.main(argv=[__file__] + remaining)
