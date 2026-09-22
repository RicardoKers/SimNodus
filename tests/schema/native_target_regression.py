"""Reference target gates; synthetic failures do not replace real boot acceptance."""
import argparse
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
import native_firmware_regression as fw
import native_boot_regression as boot
import resource_lock

PROBE = None
ROOT = fw.rc.ROOT
PLATFORM = ROOT / 'tests/experiments/renode-stm32/stm32f103c8.repl'


def package(root, image):
    doc = fw.package(root, image);dep = doc['sources']['lock']['dependencies'][0]
    data = PLATFORM.read_bytes();(root / 'platform.repl').write_bytes(data)
    dep['files'].append({'id': 'platform', 'path': 'platform.repl', 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()})
    dep['content_sha256'] = resource_lock.content_hash(dep['files'])
    pins = [f'pa{i}' for i in range(5)]
    topology = doc['sources']['topology']
    topology['components'] = [{'id': 'mcu', 'name': 'Reference MCU', 'pins': [{'id': p, 'name': p, 'domain': 'electrical'} for p in pins],
                               'parameters': [], 'symbol': None, 'model': None}]
    topology['circuits'] = [{'id': 'main', 'name': 'Reference root', 'ports': [], 'parameters': [], 'nets': [], 'symbol': None,
                             'instances': [{'id': 'cpu', 'name': 'Reference target', 'kind': 'component', 'definition': 'mcu', 'overrides': {}}]}]
    doc['firmware'][0]['architecture'] = 'arm-cortex-m3'
    doc['platforms'] = [{'id': 'reference', 'name': 'SN-012 offline subset', 'mcu': {'device': 'stm32f103c8',
                        'architecture': 'arm-cortex-m3', 'resource': {'dependency': dep['id'], 'resource': 'platform'}},
                        'board': None, 'pins': [{'id': p, 'name': p} for p in pins]}]
    doc['targets'] = [{'path': ['main', 'cpu'], 'platform': 'reference', 'firmware': 'selected', 'pin_map': {p: p for p in pins}}]
    return doc


def inspect(doc, root, path='main/cpu', profile='sn012', after_capture=None):
    raw = json.dumps(doc).encode();fields = [raw, str(root).encode(), path.encode()]
    packet = b''.join(struct.pack('<I', len(v)) + v for v in fields)
    process = subprocess.Popen([PROBE, profile], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        process.stdin.write(packet);process.stdin.flush();received = queue.Queue()
        threading.Thread(target=lambda: received.put(process.stdout.readline()), daemon=True).start()
        first = received.get(timeout=20).decode().strip()
        if first == 'CAPTURED':
            if after_capture:after_capture()
            output, error = process.communicate(b'c', timeout=30);lines = output.decode().splitlines()
        else:
            output, error = process.communicate(timeout=30);lines = [first] + output.decode().splitlines()
        assert process.returncode in (0, 1), error
        if process.returncode:
            assert lines[0].startswith('ERR '), lines
            return {'error': lines[0].split()[1:]}
        assert lines[0] == 'OK' and lines[2] == '0 0 0' and bytes.fromhex(lines[3]) == raw
        target, platform, index = map(int, lines[1].split())
        decoder = json.JSONDecoder()
        assert decoder.raw_decode(raw[target:].decode())[0] == doc['targets'][0]
        assert decoder.raw_decode(raw[platform:].decode())[0]['id'] == doc['targets'][0]['platform']
        assert dict(line.split() for line in lines[7:]) == doc['targets'][0]['pin_map']
        return {'firmware': bytes.fromhex(lines[4]), 'platform': bytes.fromhex(lines[5]), 'boot': list(map(int, lines[6].split())), 'platform_index': index}
    finally:
        if process.poll() is None:process.kill();process.wait(timeout=10)
        for stream in (process.stdin, process.stdout, process.stderr):stream.close()


class Target(unittest.TestCase):
    def setUp(self):
        parent = ROOT / 'build/sn021-target-tests';parent.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=parent);self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name);self.doc = package(self.root, bytes(boot.image()))

    def test_explicit_profile_path_and_metadata(self):
        self.assertEqual(inspect(self.doc, self.root, profile='')['error'][:2], ['request', 'profile'])
        self.assertEqual(inspect(self.doc, self.root, path='main/other')['error'][:2], ['selection', 'target'])
        self.doc['unexpected'] = 1
        self.assertEqual(inspect(self.doc, self.root)['error'][0], 'declaration')

    def test_device_architecture_and_interface(self):
        mcu = self.doc['platforms'][0]['mcu'];mcu['device'] = 'another-device'
        self.assertEqual(inspect(self.doc, self.root)['error'][1], 'device')
        mcu['device'] = 'stm32f103c8';mcu['architecture'] = self.doc['firmware'][0]['architecture'] = 'other-isa'
        self.assertEqual(inspect(self.doc, self.root)['error'][1], 'architecture')
        mcu['architecture'] = self.doc['firmware'][0]['architecture'] = 'arm-cortex-m3'
        self.doc['platforms'][0]['pins'][0]['id'] = 'pb0';self.doc['targets'][0]['pin_map']['pa0'] = 'pb0'
        self.assertEqual(inspect(self.doc, self.root)['error'][1], 'pins')

    def test_board_and_target_count(self):
        self.doc['platforms'][0]['board'] = {'id': 'board', 'name': 'Unverified board', 'resource': self.doc['platforms'][0]['mcu']['resource']}
        self.assertEqual(inspect(self.doc, self.root)['error'][1], 'offline-board')
        self.doc['targets'] = []
        self.assertEqual(inspect(self.doc, self.root)['error'][1], 'target-count')

    @unittest.skipUnless(os.name == 'nt', 'Windows physical gate')
    def test_unpinned_firmware_rejected(self):
        self.assertEqual(inspect(self.doc, self.root)['error'][:2], ['selection', 'firmware-bytes'])

    @unittest.skipUnless(os.name == 'nt', 'Windows physical gate')
    def test_unpinned_platform_even_with_matching_manifest_hash(self):
        data = PLATFORM.read_bytes() + b'\n';(self.root / 'platform.repl').write_bytes(data)
        dep = self.doc['sources']['lock']['dependencies'][0];row = next(r for r in dep['files'] if r['id'] == 'platform')
        row.update(bytes=len(data), sha256=hashlib.sha256(data).hexdigest());dep['content_sha256'] = resource_lock.content_hash(dep['files'])
        self.assertEqual(inspect(self.doc, self.root)['error'][:2], ['selection', 'platform-bytes'])

    @unittest.skipUnless(os.name == 'nt', 'Windows physical gate')
    def test_missing_resource(self):
        (self.root / 'platform.repl').unlink()
        self.assertEqual(inspect(self.doc, self.root)['error'][0], 'firmware')

    @unittest.skipUnless(os.name != 'nt', 'Unsupported platform only')
    def test_unsupported_platform(self):
        self.assertEqual(inspect(self.doc, r'C:\unused-package')['error'][:2], ['firmware', 'platform'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser();parser.add_argument('--probe', required=True)
    args, remaining = parser.parse_known_args();PROBE = args.probe
    unittest.main(argv=[__file__] + remaining)
