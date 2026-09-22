"""Adversarial ELF32 envelope inspection; never loads or executes an image."""
import argparse
import json
import struct
import subprocess
import unittest

PROBE = None


def image(programs=None):
    if programs is None:
        programs = [(1, 256, 0x08000000, 0x08000000, 4, 8, 5, 4)]
    size = max(260, 52 + 32 * len(programs))
    data = bytearray(size)
    data[:16] = b'\x7fELF\x01\x01\x01' + bytes(9)
    struct.pack_into('<HHIIIIIHHHHHH', data, 16, 2, 40, 1, 0x08000001, 52, 0, 0x05000000, 52, 32, len(programs), 0, 0, 0)
    for i, p in enumerate(programs):
        struct.pack_into('<8I', data, 52 + 32 * i, *p)
    return data


def changed(raw, at, value, fmt='I'):
    data = bytearray(raw);struct.pack_into('<' + fmt, data, at, value);return data


def inspect(raw):
    run = subprocess.run([PROBE], input=raw, capture_output=True, timeout=20)
    assert run.returncode in (0, 1), run.stderr
    value = json.loads(run.stdout)
    assert run.returncode == (1 if 'error' in value else 0)
    return value


def oracle(raw):
    # Independent stdlib binary decoding, not an acceptance/compatibility oracle.
    h = struct.unpack_from('<HHIIIIIHHHHHH', raw, 16)
    addresses = [struct.unpack_from('<I', raw, h[4] + i * h[8] + 8)[0] for i in range(h[9])
                 if struct.unpack_from('<I', raw, h[4] + i * h[8])[0] == 1]
    return {'load_ordered': addresses == sorted(addresses), 'bytes': len(raw), 'machine': h[1], 'os_abi': raw[7], 'abi_version': raw[8],
            'entry': h[3], 'flags': h[6], 'section_offset': h[5], 'section_count': h[11], 'section_names': h[12],
            'programs': [list(struct.unpack_from('<8I', raw, h[4] + i * h[8])) + [h[4] + i * h[8]] for i in range(h[9])]}


class ElfInspection(unittest.TestCase):
    def reject(self, raw, code, offset=None):
        value = inspect(raw);self.assertEqual(value.get('error'), code, value)
        if offset is not None:
            self.assertEqual(value['offset'], offset)

    def test_owned_metadata_and_raw_machine(self):
        for machine in (0, 40, 83, 243, 65535):
            raw = changed(image(), 18, machine, 'H')
            raw[7:9] = b'\xff\xfe'
            self.assertEqual(inspect(raw), oracle(raw))

    def test_all_truncations_before_payload_end(self):
        raw = image()
        for n in range(260):
            with self.subTest(n=n):
                self.assertIn('error', inspect(raw[:n]))

    def test_header_rejections(self):
        for at, value, fmt, code in [(0, 0, 'B', 'magic'), (4, 2, 'B', 'class'), (5, 2, 'B', 'encoding'),
                (6, 2, 'B', 'version'), (16, 3, 'H', 'type'), (20, 2, 'I', 'version'), (40, 51, 'H', 'header-size')]:
            with self.subTest(at=at):
                self.reject(changed(image(), at, value, fmt), code, at)

    def test_program_table_limits(self):
        for count in (0, 65, 65535):
            self.reject(changed(image(), 44, count, 'H'), 'program-count', 44)
        self.reject(changed(image(), 42, 31, 'H'), 'program-size', 42)
        for start in (0, 51, 229, 0xffffffff):
            self.reject(changed(image(), 28, start), 'program-table', 28)
        p = (1, 0, 0, 0, 0, 0, 0, 1)
        raw = image([p] * 64);self.assertEqual(inspect(raw), oracle(raw))

    def test_file_intervals_and_zero_fill(self):
        for start in (258, 261, 0xffffffff):
            self.reject(changed(image(), 56, start), 'segment-file', 56)
        self.reject(changed(image(), 68, 0xffffffff), 'segment-file', 56)
        self.reject(changed(image(), 72, 3), 'segment-size', 68)
        raw = image([(1, 260, 0, 0, 0, 0xffffffff, 6, 1)])
        self.assertEqual(inspect(raw), oracle(raw))  # No allocation by memory_size.

    def test_address_space_exact_end_and_overflow(self):
        raw = image([(1, 256, 0xfffffffc, 0xfffffffc, 4, 4, 5, 4)])
        self.assertEqual(inspect(raw), oracle(raw))
        self.reject(changed(raw, 72, 5), 'virtual-range', 60)
        raw = changed(raw, 60, 0)
        self.reject(changed(raw, 72, 5), 'physical-range', 64)

    def test_alignment(self):
        for align in (0, 1, 2, 4, 256):
            raw = changed(image(), 80, align);self.assertEqual(inspect(raw), oracle(raw))
        for align in (3, 6, 0xffffffff):
            self.reject(changed(image(), 80, align), 'alignment', 80)
        self.reject(changed(image(), 60, 0x08000001), 'alignment', 80)

    def test_dynamic_interpreter_tls_rejected(self):
        for kind in (2, 3, 5, 7):
            self.reject(changed(image(), 52, kind), 'unsupported-program', 52)

    def test_opaque_programs_and_null(self):
        load = (1, 256, 0, 0, 4, 8, 5, 1)
        for kind in (4, 6, 0x70000001, 0x6474e551, 0xffffffff):
            raw = image([load, (kind, 260, 0, 0, 0, 0, 0, 0)])
            self.assertEqual(inspect(raw), oracle(raw))
        raw = image([(0,) + (0xffffffff,) * 7, load])
        self.assertEqual(inspect(raw), oracle(raw))
        self.reject(image([(0,) * 8]), 'load-absent', 44)

    def test_order_and_overlap_not_mapping_approval(self):
        p = (1, 256, 0x100, 0x100, 4, 8, 5, 4)
        raw = image([p, p]);self.assertEqual(inspect(raw), oracle(raw))
        raw = changed(raw, 92, 0xfc)
        self.assertEqual(inspect(raw), oracle(raw))
        self.assertFalse(inspect(raw)['load_ordered'])

    def test_section_envelope_and_extended_counts(self):
        raw = image();struct.pack_into('<I', raw, 32, 84);struct.pack_into('<HHH', raw, 46, 40, 1, 0)
        self.assertEqual(inspect(raw), oracle(raw))
        self.reject(changed(raw, 32, 0xffffffff), 'section-table', 32)
        self.reject(changed(raw, 46, 39, 'H'), 'section-size', 46)
        for index in (1, 65535):self.reject(changed(raw, 50, index, 'H'), 'section-index', 50)
        self.reject(changed(raw, 48, 4097, 'H'), 'section-count', 48)
        self.reject(changed(raw, 48, 0, 'H'), 'sections', 32)
        self.reject(changed(image(), 50, 65535, 'H'), 'sections', 32)
        maximum = image();maximum.extend(bytes(52 + 4096 * 40 - len(maximum)))
        struct.pack_into('<I', maximum, 32, 52);struct.pack_into('<HHH', maximum, 46, 40, 4096, 0)
        self.assertEqual(inspect(maximum), oracle(maximum))

    def test_byte_budget(self):
        raw = image();raw.extend(bytes(16 * 1024 * 1024 - len(raw)))
        self.assertEqual(inspect(raw), oracle(raw))
        raw.append(0);self.reject(raw, 'bytes', 0)


if __name__ == '__main__':
    parser = argparse.ArgumentParser();parser.add_argument('--probe', required=True)
    args, remaining = parser.parse_known_args();PROBE = args.probe
    unittest.main(argv=[__file__] + remaining)
