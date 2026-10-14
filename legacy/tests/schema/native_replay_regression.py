"""Fixed E-01 configured replay: inert binding and physical Windows rejection."""
import argparse
import copy
import hashlib
import os
from pathlib import Path
import tempfile
import unittest
import native_rc_compile_regression as rc
import resource_lock

SCHEDULE = b'time_ns,drive_uv\n0,3300000\n'


def package(root):
    doc = rc.project();rc.populate(root, doc)
    dep = doc['sources']['lock']['dependencies'][0]
    dep['files'].append({'id': 'fixed-drive', 'path': 'fixed-drive.csv',
        'bytes': len(SCHEDULE), 'sha256': hashlib.sha256(SCHEDULE).hexdigest()})
    dep['content_sha256'] = resource_lock.content_hash(dep['files'])
    (root / 'fixed-drive.csv').write_bytes(SCHEDULE)
    doc['temporal'].update(mode='known-schedule-replay', fidelity='causal-replay',
        duration_ns=5000000, exchange_quantum_ns=5000000,
        schedule={'dependency': dep['id'], 'resource': 'fixed-drive'}, debug='disabled')
    return doc


def compile(doc, root, **kwargs):
    return rc.compile_project(doc, root, operation='replay', **kwargs)


class Replay(unittest.TestCase):
    def setUp(self):
        parent = rc.ROOT / 'build/sn021-replay-tests';parent.mkdir(parents=True, exist_ok=True)
        temp = tempfile.TemporaryDirectory(dir=parent);self.addCleanup(temp.cleanup)
        self.root = Path(temp.name);self.doc = package(self.root)

    def test_policy_before_io(self):
        for key, value, code in [('duration_ns', 6000000, 'replay-time'),
                                  ('exchange_quantum_ns', 100000, 'replay-time'),
                                  ('debug', 'bounded-cooperative', 'replay-debug')]:
            doc = copy.deepcopy(self.doc);doc['temporal'][key] = value
            rc.declaration.validate(doc)
            self.assertEqual(compile(doc, 'relative-root')[:2], ['4', code])
        doc = copy.deepcopy(self.doc);doc['temporal'].update(mode='approximate-sampled', fidelity='approximate', schedule=None)
        self.assertEqual(compile(doc, 'relative-root')[:2], ['4', 'replay-mode'])
        self.assertEqual(compile(rc.project(), 'relative-root')[:2], ['4', 'replay-mode'])

    def test_explicit_request_and_metadata(self):
        self.assertEqual(compile(self.doc, self.root, profile='')[:2], ['1', 'profile'])
        self.assertEqual(compile(self.doc, self.root, drive='reference')[:2], ['1', 'distinct-nodes'])
        self.doc['temporal']['schedule'] = None
        self.assertEqual(compile(self.doc, 'relative-root')[0], '0')

    def test_existing_api_does_not_fallback(self):
        self.assertEqual(rc.compile_project(self.doc, 'relative-root')[:2], ['4', 'standalone'])

    @unittest.skipUnless(os.name == 'nt', 'Windows/NTFS capture')
    def test_owned_artifact_matches_existing_profile(self):
        result = compile(self.doc, self.root);self.assertIsInstance(result, dict)
        unconfigured = rc.project()
        original = rc.compile_project(unconfigured, self.root)
        self.assertEqual(result['netlist'], original['netlist'])
        self.assertEqual(result['replay'][:2], [5000000, 5000000])
        dep = self.doc['sources']['lock']['dependencies'][0]
        self.assertEqual(dep['files'][result['replay'][2]]['id'], 'fixed-drive')
        (self.root / 'fixed-drive.csv').write_bytes(b'replaced')
        self.assertEqual(compile(self.doc, self.root)[0], '2')
        self.assertEqual(result['netlist'], original['netlist'])

    @unittest.skipUnless(os.name == 'nt', 'Windows/NTFS capture')
    def test_matching_hash_does_not_accept_schedule_changes(self):
        dep = self.doc['sources']['lock']['dependencies'][0];row = dep['files'][-1]
        for data in (b'', SCHEDULE.replace(b'3300000', b'3200000'),
                     SCHEDULE + b'1000,0\n', SCHEDULE.replace(b'\n', b'\r\n'),
                     b'\xef\xbb\xbf' + SCHEDULE, SCHEDULE + b'.control\nquit\n'):
            with self.subTest(data=data):
                (self.root / row['path']).write_bytes(data)
                row.update(bytes=len(data), sha256=hashlib.sha256(data).hexdigest())
                dep['content_sha256'] = resource_lock.content_hash(dep['files'])
                self.assertEqual(compile(self.doc, self.root)[:2], ['4', 'replay-schedule'])

    @unittest.skipUnless(os.name == 'nt', 'Windows/NTFS capture')
    def test_physical_and_interface_mismatch(self):
        (self.root / 'fixed-drive.csv').unlink()
        self.assertEqual(compile(self.doc, self.root)[0], '2')
        (self.root / 'fixed-drive.csv').write_bytes(SCHEDULE)
        self.doc['sources']['topology']['circuits'][0]['instances'][0]['overrides']['resistance']['value'] = '2'
        self.assertEqual(compile(self.doc, self.root)[:2], ['4', 'value'])

    @unittest.skipUnless(os.name == 'nt', 'Windows/NTFS capture')
    def test_schedule_identity_is_not_a_path_hint(self):
        dep = self.doc['sources']['lock']['dependencies'][0]
        self.doc['temporal']['schedule']['resource'] = dep['files'][0]['id']
        self.assertEqual(compile(self.doc, self.root)[:2], ['4', 'replay-schedule'])
        self.doc['temporal']['schedule']['dependency'] = 'absent'
        self.assertEqual(compile(self.doc, self.root)[0], '0')

    @unittest.skipUnless(os.name != 'nt', 'Unsupported platform only')
    def test_unsupported_platform(self):
        self.assertEqual(compile(self.doc, r'C:\unused-package')[:2], ['2', 'platform'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser();parser.add_argument('--probe', required=True)
    args, remaining = parser.parse_known_args();rc.PROBE = args.probe
    unittest.main(argv=[__file__] + remaining)
