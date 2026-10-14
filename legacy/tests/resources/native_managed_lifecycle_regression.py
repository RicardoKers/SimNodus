"""Local NTFS preflight for a synthetic Open envelope; no IPC/authentication claim."""
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/schema'))
import native_replay_regression as replay
sys.path.insert(0, str(ROOT / 'tests/resources'))
import native_acquisition_regression as acquisition

PROBE = ACQUISITION = ''


def field(value):
    return struct.pack('<I', len(value)) + value


def frame(body, operation, correlation):
    return b'SN21SAVE' + struct.pack('<HHI', 3, operation, len(body)) + correlation + body


def run(*arguments):
    result = subprocess.run([PROBE, *map(str, arguments)], capture_output=True, timeout=30)
    assert result.returncode in (0, 2), result.stderr
    return result


@unittest.skipUnless(os.name == 'nt', 'Physical preflight requires Windows/NTFS')
class LocalComposition(unittest.TestCase):
    def setUp(self):
        parent = ROOT / 'build/sn021-managed-lifecycle-local'
        parent.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.root = self.base / 'external'
        self.root.mkdir()
        self.doc = replay.package(self.root)
        self.source = self.root / 'source.json'
        self.original = (json.dumps(self.doc, indent=2, ensure_ascii=False) + '\n').encode()
        self.source.write_bytes(self.original)
        observed = subprocess.run([ACQUISITION], input=acquisition.packet(self.root, 'source.json'),
                                  capture_output=True, timeout=15, check=True)
        identity = json.loads(observed.stdout)
        self.volume = identity['root_volume']
        self.file_id = bytes.fromhex(identity['root_file'])
        self.correlation = b'c' * 16
        generation, document = b'g' * 16, b'd' * 16
        self.request = self.base / 'open.request.bin'
        self.request.write_bytes(frame(b'r' * 16 + generation + document, 1, self.correlation))
        locator = str(self.root).encode()
        context = b'SRC1' + b'b' * 16 + struct.pack('<Q', self.volume) + self.file_id
        context += struct.pack('<II', 1, len(locator)) + locator
        token = generation + document + struct.pack('<I', 1) + b'k' * 16 + b'h' * 32
        token += struct.pack('<Q', self.volume) + self.file_id
        self.response = self.base / 'open.response.bin'
        self.response.write_bytes(frame(struct.pack('<I', 0) + token + field(context)
                                        + field(self.original), 0x8001, self.correlation))

    def test_edit_export_and_bound_compile(self):
        edited = self.base / 'edited.json'
        result = run('edit', self.request, self.response, edited, 'Managed RC copy')
        self.assertEqual(result.returncode, 0, result.stderr)
        expected = json.loads(self.original)
        expected['name'] = 'Managed RC copy'
        self.assertEqual(json.loads(edited.read_bytes()), expected)
        self.assertEqual(self.source.read_bytes(), self.original)

        netlist = self.base / 'netlist.cir'
        result = run('compile', self.request, self.response, netlist)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b'R1 in out 1000\nC1 out 0 0.000001 IC=0', netlist.read_bytes())
        self.assertFalse(json.loads(result.stdout)['readiness'])

        result = run('export', self.request, self.response, self.root, 'export.json')
        self.assertEqual(result.returncode, 0, result.stderr)
        target = self.root / 'export.json'
        self.assertEqual(target.read_bytes(), self.original)
        result = run('export', self.request, self.response, self.root, 'export.json')
        self.assertEqual(result.returncode, 2)
        self.assertEqual(target.read_bytes(), self.original)
        self.assertEqual(self.source.read_bytes(), self.original)

    def test_byte_identical_root_substitution_blocks_compile(self):
        with tempfile.TemporaryDirectory(dir=self.base) as holding:
            old = Path(holding) / 'original-root'
            self.root.rename(old)
            shutil.copytree(old, self.root)
            self.assertEqual(self.source.read_bytes(), self.original)
            result = run('compile', self.request, self.response, self.base / 'rejected.cir')
            self.assertEqual(result.returncode, 2)
            self.assertIn(b'compile-root_identity', result.stderr)
            self.assertFalse((self.base / 'rejected.cir').exists())

    def test_changed_and_missing_resources_block_compile(self):
        schedule = self.root / 'fixed-drive.csv'
        original = schedule.read_bytes()
        schedule.write_bytes(original.replace(b'3300000', b'3200000'))
        result = run('compile', self.request, self.response, self.base / 'changed.cir')
        self.assertEqual(result.returncode, 2)
        self.assertIn(b'compile-hash', result.stderr)
        self.assertFalse((self.base / 'changed.cir').exists())
        schedule.write_bytes(original)

        passive = self.root / 'tests/schema/fixtures/assets/passive.cir'
        missing = self.base / 'missing-passive.cir'
        passive.rename(missing)
        result = run('compile', self.request, self.response, self.base / 'missing.cir')
        self.assertEqual(result.returncode, 2)
        self.assertIn(b'compile-filesystem', result.stderr)
        self.assertFalse((self.base / 'missing.cir').exists())
        self.assertEqual(self.source.read_bytes(), self.original)

    def test_corrupt_envelope_rejected_without_output(self):
        corrupted = self.base / 'corrupt.bin'
        raw = self.response.read_bytes()
        for data in (raw[:-1], raw[:32], raw[:40] + b'x' + raw[41:]):
            corrupted.write_bytes(data)
            result = run('compile', self.request, corrupted, self.base / 'corrupt.cir')
            self.assertEqual(result.returncode, 2)
            self.assertFalse((self.base / 'corrupt.cir').exists())


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    parser.add_argument('--acquisition', required=True)
    args = parser.parse_args()
    PROBE, ACQUISITION = args.probe, args.acquisition
    unittest.main(argv=[__file__], verbosity=2)
