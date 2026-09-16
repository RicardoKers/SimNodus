"""Compose native create/open/rename/save-copy/reopen/compile on real NTFS."""
import argparse
import json
import os
from pathlib import Path
import queue
import struct
import subprocess
import tempfile
import threading
import unittest
import native_rc_compile_regression as rc

PROBE = None
NAME = 'Saved "RC" copy Ω'


def lifecycle(root, raw, name=NAME, after_capture=None):
    fields = [str(root).encode(), raw, name.encode()]
    packet = b''.join(struct.pack('<I', len(v)) + v for v in fields)
    process = subprocess.Popen([PROBE], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    try:
        process.stdin.write(packet);process.stdin.flush()
        received = queue.Queue()
        threading.Thread(target=lambda: received.put(process.stdout.readline()), daemon=True).start()
        first = received.get(timeout=20).decode().strip()
        if first == 'CAPTURED':
            if after_capture:
                after_capture()
            output, error = process.communicate(b'c', timeout=30)
            lines = output.decode().splitlines()
        else:
            output, error = process.communicate(timeout=30)
            lines = [first] + output.decode().splitlines()
        assert process.returncode in (0, 1), error
        if process.returncode:
            assert lines[0].startswith('ERR '), lines
            return {'error': lines[0].split()[1:]}
        assert lines[0] == 'OK' and len(lines) == 6, lines[:1]
        captured, revised, netlist = [bytes.fromhex(v) for v in lines[1:4]]
        assert captured == raw
        expected = json.loads(raw);expected['name'] = name
        assert json.loads(revised) == expected
        for line in lines[4:]:
            parameter, begin, end = map(int, line.split())
            assert json.loads(revised[begin:end])['id'] in ('r', 'c')
            json.JSONDecoder().raw_decode(revised[parameter:].decode())
        return {'captured': captured, 'revised': revised, 'netlist': netlist}
    finally:
        if process.poll() is None:
            process.kill();process.wait(timeout=10)
        for stream in (process.stdin, process.stdout, process.stderr):
            stream.close()


@unittest.skipUnless(os.name == 'nt', 'Lifecycle persistence requires Windows/NTFS')
class Lifecycle(unittest.TestCase):
    def setUp(self):
        parent = rc.ROOT / 'build/sn021-lifecycle-tests';parent.mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=parent)
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name);self.doc = rc.project();rc.populate(self.root, self.doc)
        self.raw = (json.dumps(self.doc, indent=2, ensure_ascii=False) + '\n').encode()

    def test_round_trip_and_source_maps(self):
        result = lifecycle(self.root, self.raw)
        self.assertNotIn('error', result)
        self.assertEqual((self.root / 'original.json').read_bytes(), self.raw)
        self.assertEqual((self.root / 'revised.json').read_bytes(), result['revised'])
        old = json.dumps(self.doc['name'], ensure_ascii=False).encode()
        new = json.dumps(NAME, ensure_ascii=False).encode()
        self.assertEqual(result['revised'], self.raw.replace(old, new, 1))
        self.assertIn(b'R1 in out 1000\nC1 out 0 0.000001 IC=0', result['netlist'])

    def test_original_collision_preserved(self):
        target = self.root / 'original.json';target.write_bytes(b'other owner')
        self.assertEqual(lifecycle(self.root, self.raw)['error'][0], 'create')
        self.assertEqual(target.read_bytes(), b'other owner')
        self.assertFalse((self.root / 'revised.json').exists())

    def test_save_copy_collision_preserved(self):
        target = self.root / 'revised.json';target.write_bytes(b'other owner')
        self.assertEqual(lifecycle(self.root, self.raw)['error'][0], 'save-copy')
        self.assertEqual(target.read_bytes(), b'other owner')
        self.assertEqual((self.root / 'original.json').read_bytes(), self.raw)

    def test_invalid_declaration_never_published(self):
        self.doc['unexpected'] = True
        self.assertEqual(lifecycle(self.root, json.dumps(self.doc).encode())['error'][0], 'create')
        self.assertFalse((self.root / 'original.json').exists())

    def test_invalid_edit_preserves_original(self):
        self.assertEqual(lifecycle(self.root, self.raw, name='')['error'][0], 'rename')
        self.assertEqual((self.root / 'original.json').read_bytes(), self.raw)
        self.assertFalse((self.root / 'revised.json').exists())

    def test_document_replacement_does_not_replace_capture(self):
        target = self.root / 'original.json'
        def replace():
            target.unlink();target.write_bytes(b'new owner after capture')
        result = lifecycle(self.root, self.raw, after_capture=replace)
        self.assertNotIn('error', result)
        self.assertEqual(target.read_bytes(), b'new owner after capture')
        self.assertEqual(result['captured'], self.raw)

    def test_resource_replacement_stops_at_compile(self):
        target = self.root / 'tests/schema/fixtures/assets/passive.cir'
        result = lifecycle(self.root, self.raw, after_capture=lambda: target.write_bytes(b'changed'))
        self.assertEqual(result['error'], ['compile', 'size'])
        self.assertEqual((self.root / 'original.json').read_bytes(), self.raw)
        self.assertEqual(json.loads((self.root / 'revised.json').read_bytes())['name'], NAME)

    def test_missing_resources_do_not_prevent_inert_save_open(self):
        (self.root / 'tests/schema/fixtures/assets/passive.cir').unlink()
        self.assertEqual(lifecycle(self.root, self.raw)['error'][0], 'compile')
        self.assertTrue((self.root / 'revised.json').exists())


if __name__ == '__main__':
    parser = argparse.ArgumentParser();parser.add_argument('--probe', required=True)
    args, remaining = parser.parse_known_args();PROBE = args.probe
    unittest.main(argv=[__file__] + remaining)
