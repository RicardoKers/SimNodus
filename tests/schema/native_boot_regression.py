"""Static SN-012 boot candidate constraints; no loader or engine execution."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import unittest
import native_elf_regression as elf

PROBE = None


def image(programs=None):
    if programs is None:programs = [(1, 256, 0x08000000, 0x08000000, 16, 16, 5, 4)]
    raw = elf.changed(elf.image(programs), 36, 0x05000200)
    raw.extend(bytes(max(0, 272 - len(raw))))
    struct.pack_into('<II', raw, 256, 0x20005000, 0x08000009)
    struct.pack_into('<I', raw, 24, 0x08000009)
    return raw


def inspect(raw, profile='sn012', architecture='arm-cortex-m3'):
    run = subprocess.run([PROBE, profile, architecture], input=raw, capture_output=True, timeout=20)
    assert run.returncode in (0, 1), run.stderr
    value = json.loads(run.stdout);assert run.returncode == (1 if 'error' in value else 0)
    return value


class BootCandidate(unittest.TestCase):
    def reject(self, raw, code):self.assertEqual(inspect(raw).get('error'), code)

    def test_explicit_request(self):
        self.assertEqual(inspect(b'', profile='')['error'], 'profile')
        self.assertEqual(inspect(b'', architecture='arm')['error'], 'architecture')
        self.assertEqual(inspect(b'')['stage'], 'elf')

    def test_owned_vector_and_entry(self):
        raw = image()
        self.assertEqual(inspect(raw), {'stack': 0x20005000, 'reset': 0x08000009,
            'vector_offset': 256, 'load_ordered': True, 'bytes': len(raw)})

    def test_architecture_evidence(self):
        for at, value, fmt, code in [(18, 83, 'H', 'machine'), (7, 1, 'B', 'abi'), (8, 1, 'B', 'abi'), (36, 0x05000400, 'I', 'flags')]:
            with self.subTest(at=at):self.reject(elf.changed(image(), at, value, fmt), code)

    def test_load_regions_and_permissions(self):
        self.reject(elf.changed(image(), 52, 4), 'load-absent')
        for at, value, code in [(60, 0x08010000, 'code-region'), (64, 0x08000004, 'code-region'),
                (72, 20, 'code-region'), (76, 7, 'permissions'), (76, 6, 'data-region'), (72, 0, 'segment-size')]:
            with self.subTest(at=at):self.reject(elf.changed(image(), at, value), code)
        self.reject(image([(1, 256, 0x08000000, 0x08000000, 16, 16, 5, 4), (4, 0, 0, 0, 0, 0, 0, 0)]), 'program')

    def test_overlaps(self):
        code = (1, 256, 0x08000000, 0x08000000, 16, 16, 5, 4)
        self.reject(image([code, code]), 'virtual-overlap')
        self.reject(image([code, (1, 268, 0x20000000, 0x08000000, 4, 8, 6, 4)]), 'physical-overlap')
        self.reject(image([code, (1, 268, 0x20000000, 0x08000010, 4, 8, 6, 4)]), 'file-overlap')

    def test_data_regions_zero_fill_and_reserve(self):
        code = (1, 256, 0x08000000, 0x08000000, 16, 16, 5, 4)
        raw = image([code, (1, 0, 0x20000100, 0x20000100, 0, 8, 6, 4), (1, 0, 0x20000000, 0x20000000, 0, 256, 6, 4)])
        self.assertFalse(inspect(raw)['load_ordered'])
        for row, error in [((1, 0, 0x20005000, 0x20005000, 0, 8, 6, 4), 'data-region'),
                           ((1, 0, 0x20000000, 0x08000000, 0, 8, 6, 4), 'data-source'),
                           ((1, 0, 0x20004c00, 0x20004c00, 0, 8, 6, 4), 'stack-reserve')]:
            self.reject(image([code, row]), error)
        raw = image([code, (1, 272, 0x20000100, 0x08000010, 4, 8, 6, 4)]);raw.extend(bytes(4))
        self.assertNotIn('error', inspect(raw))
        self.reject(elf.changed(raw, 96, 0x20000100), 'data-source')

    def test_stack_boundaries(self):
        for stack in (0, 0x20000000, 0x20005008, 0x20004fff):self.reject(elf.changed(image(), 256, stack), 'stack')
        self.reject(elf.changed(image(), 256, 0x20000008), 'stack-reserve')
        self.assertNotIn('error', inspect(elf.changed(image(), 256, 0x20000400)))

    def test_vectors_and_reset(self):
        self.reject(image([(1, 256, 0x08000004, 0x08000004, 16, 16, 5, 4)]), 'vectors')
        self.reject(image([(1, 256, 0x08000000, 0x08000000, 4, 4, 5, 4)]), 'vectors')
        for reset in (0x08000008, 0x0800000b):self.reject(elf.changed(image(), 260, reset), 'reset')
        for reset in (0x08000011, 0x20000001):
            raw = elf.changed(elf.changed(image(), 260, reset), 24, reset);self.reject(raw, 'reset-region')


if __name__ == '__main__':
    parser = argparse.ArgumentParser();parser.add_argument('--probe', required=True)
    parser.add_argument('--image', type=Path);parser.add_argument('--build-record', type=Path);parser.add_argument('--output', type=Path)
    args, remaining = parser.parse_known_args();PROBE = args.probe
    if args.image:
        if not args.build_record or not args.output:parser.error('--image requires --build-record and --output')
        with args.output.open('x', encoding='utf-8') as output:
            report = {'status': 'started', 'engine_run': False, 'firmware_loaded': False}
            try:
                raw = args.image.read_bytes();record = json.loads(args.build_record.read_bytes())
                report['image_sha256'] = hashlib.sha256(raw).hexdigest()
                report['record_sha256'] = hashlib.sha256(args.build_record.read_bytes()).hexdigest()
                report['probe_sha256'] = hashlib.sha256(Path(PROBE).read_bytes()).hexdigest()
                assert report['image_sha256'] == record['elf_sha256']
                report['inspection'] = inspect(raw);assert 'error' not in report['inspection'], report['inspection']
                assert report['inspection']['stack'] == record['symbols']['_stack_top']
                assert report['inspection']['reset'] == record['symbols']['Reset_Handler'] | 1
                report['status'] = 'passed'
            except Exception as error:
                report['status'] = 'failed';report['error'] = str(error);raise
            finally:
                output.write(json.dumps(report, indent=2) + '\n');print(json.dumps(report, indent=2))
    else:
        unittest.main(argv=[__file__] + remaining)
