"""Explicit physical firmware association; no firmware loader or engine call."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import queue
import struct
import subprocess
import tempfile
import threading
import unittest
import native_elf_regression as elf
import native_rc_compile_regression as rc
import resource_lock

PROBE = None


def package(root, image):
    doc = rc.project();rc.populate(root, doc)
    dep = doc['sources']['lock']['dependencies'][0]
    dep['files'].append({'id': 'firmware', 'path': 'firmware.elf', 'bytes': len(image), 'sha256': hashlib.sha256(image).hexdigest()})
    dep['content_sha256'] = resource_lock.content_hash(dep['files'])
    (root / 'firmware.elf').write_bytes(image)
    doc['firmware'] = [{'id': 'selected', 'name': 'Owned inspection image', 'architecture': 'declared-only',
                        'image_format': 'elf', 'resource': {'dependency': dep['id'], 'resource': 'firmware'}}]
    return doc


def inspect(doc, root, selected='selected', after_capture=None):
    raw = json.dumps(doc).encode()
    fields = [raw, str(root).encode(), selected.encode()]
    packet = b''.join(struct.pack('<I', len(v)) + v for v in fields)
    process = subprocess.Popen([PROBE], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        process.stdin.write(packet);process.stdin.flush()
        received = queue.Queue()
        threading.Thread(target=lambda: received.put(process.stdout.readline()), daemon=True).start()
        first = received.get(timeout=20).decode().strip()
        if first == 'CAPTURED':
            if after_capture:after_capture()
            output, error = process.communicate(b'c', timeout=30)
            lines = output.decode().splitlines()
        else:
            output, error = process.communicate(timeout=30)
            lines = [first] + output.decode().splitlines()
        assert process.returncode in (0, 1), error
        if process.returncode:
            assert lines[0].startswith('ERR '), lines
            return {'error': lines[0].split()[1:]}
        assert lines[0] == 'OK' and lines[4] == '0 0 0'
        assert bytes.fromhex(lines[5]) == raw
        identity, resource = lines[1].split(), lines[2].split()
        offset, architecture, index = map(int, lines[3].split())
        record = json.JSONDecoder().raw_decode(raw[offset:].decode())[0]
        assert record == next(row for row in doc['firmware'] if row['id'] == selected)
        assert json.JSONDecoder().raw_decode(raw[architecture:].decode())[0] == identity[1]
        data = bytes.fromhex(lines[6]);assert hashlib.sha256(data).hexdigest() == resource[2]
        return {'identity': identity, 'resource': resource, 'index': index, 'bytes': data}
    finally:
        if process.poll() is None:process.kill();process.wait(timeout=10)
        for stream in (process.stdin, process.stdout, process.stderr):stream.close()


class FirmwareInspection(unittest.TestCase):
    def setUp(self):
        parent = rc.ROOT / 'build/sn021-firmware-tests';parent.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=parent);self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name);self.image = bytes(elf.image());self.doc = package(self.root, self.image)

    def test_complete_validation_before_io(self):
        self.doc['unexpected'] = 1
        self.assertEqual(inspect(self.doc, self.root / 'absent')['error'][0], '0')

    def test_selection_before_io(self):
        for selected in ('', 'absent', 'x' * 65):
            self.assertEqual(inspect(self.doc, self.root / 'absent', selected)['error'][:2], ['1', 'firmware'])

    @unittest.skipUnless(os.name != 'nt', 'Unsupported platform only')
    def test_unsupported_platform(self):
        self.assertEqual(inspect(self.doc, r'C:\unused-package')['error'][:2], ['2', 'platform'])

    @unittest.skipUnless(os.name == 'nt', 'Windows NTFS capture')
    def test_association_and_owned_bytes_after_replacement(self):
        result = inspect(self.doc, self.root, after_capture=lambda: (self.root / 'firmware.elf').write_bytes(b'replaced'))
        self.assertEqual(result['bytes'], self.image)
        self.assertEqual(result['identity'], ['selected', 'declared-only', '40', '1'])
        self.assertEqual(result['resource'][1], 'firmware')

    @unittest.skipUnless(os.name == 'nt', 'Windows NTFS capture')
    def test_size_hash_and_missing(self):
        target = self.root / 'firmware.elf'
        for raw, code in [(b'changed', 'size'), (self.image[:-1] + b'!', 'hash')]:
            target.write_bytes(raw);self.assertEqual(inspect(self.doc, self.root)['error'][1], code)
        target.unlink();self.assertEqual(inspect(self.doc, self.root)['error'][0], '2')

    @unittest.skipUnless(os.name == 'nt', 'Windows NTFS capture')
    def test_matching_hash_is_not_elf_approval(self):
        doc = package(self.root, b'not an ELF')
        result = inspect(doc, self.root)['error']
        self.assertEqual(result[:2], ['3', 'header']);self.assertEqual(result[-1], 'elf-bytes')

    @unittest.skipUnless(os.name == 'nt', 'Windows NTFS capture')
    def test_explicit_reference_and_inventory_order(self):
        doc = copy.deepcopy(self.doc)
        doc['sources']['lock']['dependencies'][0]['files'].reverse()
        result = inspect(doc, self.root)
        self.assertEqual(result['bytes'], self.image)
        doc['firmware'][0]['resource']['resource'] = 'license'
        self.assertEqual(inspect(doc, self.root)['error'][0], '3')

    @unittest.skipUnless(os.name == 'nt', 'Windows NTFS capture')
    def test_dependency_identity_with_same_resource_id(self):
        other = copy.deepcopy(self.doc['sources']['lock']['dependencies'][0])
        other['id'] = 'other-package'
        for row in other['files']:
            data = (self.root / row['path']).read_bytes()
            if row['id'] == 'firmware':data = bytes(elf.changed(data, 18, 83, 'H'))
            row['path'] = 'other/' + row['path']
            target = self.root / row['path'];target.parent.mkdir(parents=True, exist_ok=True);target.write_bytes(data)
            row['sha256'] = hashlib.sha256(data).hexdigest()
        other['content_sha256'] = resource_lock.content_hash(other['files'])
        self.doc['sources']['lock']['dependencies'].append(other)
        self.doc['firmware'][0]['resource']['dependency'] = other['id']
        result = inspect(self.doc, self.root)
        self.assertEqual(result['resource'][:2], ['other-package', 'firmware'])
        self.assertEqual(result['identity'][2], '83')

    @unittest.skipUnless(os.name == 'nt', 'Windows NTFS capture')
    def test_entire_inventory_required(self):
        (self.root / 'tests/schema/fixtures/assets/passive.svg').unlink()
        self.assertEqual(inspect(self.doc, self.root)['error'][0], '2')


if __name__ == '__main__':
    parser = argparse.ArgumentParser();parser.add_argument('--probe', required=True)
    parser.add_argument('--image', type=Path);parser.add_argument('--build-record', type=Path);parser.add_argument('--output', type=Path)
    args, remaining = parser.parse_known_args();PROBE = args.probe
    if args.image:
        if not args.build_record or not args.output:parser.error('--image requires --build-record and --output')
        args.output.mkdir(parents=True, exist_ok=False)
        report = {'status': 'started', 'engine_run': False, 'firmware_loaded': False}
        try:
            raw = args.image.read_bytes();record = json.loads(args.build_record.read_bytes())
            report['elf_sha256'] = hashlib.sha256(raw).hexdigest()
            report['build_record_sha256'] = hashlib.sha256(args.build_record.read_bytes()).hexdigest()
            report['probe_sha256'] = hashlib.sha256(Path(PROBE).read_bytes()).hexdigest()
            assert report['elf_sha256'] == record['elf_sha256']
            root = args.output.resolve() / 'package';doc = package(root, raw)
            doc['firmware'][0]['architecture'] = 'arm-cortex-m3'
            (args.output / 'project.json').write_bytes((json.dumps(doc, indent=2) + '\n').encode())
            result = inspect(doc, root, after_capture=lambda: (root / 'firmware.elf').write_bytes(b'replaced after capture'))
            assert 'error' not in result, result
            assert result['bytes'] == raw
            assert result['identity'][2:] == [str(elf.oracle(raw)['machine']), str(int(elf.oracle(raw)['load_ordered']))]
            report['inspection'] = {k: v for k, v in result.items() if k != 'bytes'}
            report['retained_image_matches_after_replacement'] = True
            report['status'] = 'passed'
        except Exception as error:
            report['status'] = 'failed';report['error'] = str(error);raise
        finally:
            report['files_sha256'] = {p.relative_to(args.output).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                                       for p in args.output.rglob('*') if p.is_file()}
            (args.output / 'result.json').write_bytes((json.dumps(report, indent=2) + '\n').encode())
            print(json.dumps(report, indent=2))
    else:
        unittest.main(argv=[__file__] + remaining)
