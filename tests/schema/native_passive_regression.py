"""Bounded inert passive source recognition; no engine or resource I/O."""
import argparse
from pathlib import Path
import subprocess
import unittest

PROBE = None
BASE = b'.subckt sample first second value=1000\nRbody first second {value}\n.ends sample\n'


def inspect(data):
    run = subprocess.run([PROBE], input=data, capture_output=True, timeout=10)
    assert run.returncode in (0, 1), run.stderr
    lines = run.stdout.decode('ascii').splitlines()
    if run.returncode:
        assert lines[0].startswith('ERR ')
        return None
    assert lines[0] == 'OK' and bytes.fromhex(lines[1]) == data
    return [line.split() for line in lines[2:]]


class PassiveSource(unittest.TestCase):
    def test_owned_fixture(self):
        raw = Path('tests/schema/fixtures/assets/passive.cir').read_bytes()
        models = inspect(raw)
        self.assertEqual([m[:6] for m in models], [
            ['declared_resistor', 'first', 'second', 'value', '1000', 'R'],
            ['declared_capacitor', 'first', 'second', 'value', '1u', 'C']])
        for model in models:
            span = raw[int(model[6]):int(model[7])].rstrip(b'\r')
            self.assertTrue(span.startswith(b'.subckt ' + model[0].encode()))
            self.assertTrue(span.endswith(b'.ends ' + model[0].encode()))

    def test_comments_case_and_crlf(self):
        for raw in (BASE, BASE.upper(), BASE.replace(b'\n', b'\r\n'),
                    b'* inert comment\n\n' + BASE.replace(b'\n', b'\n* comment\n'),
                    BASE.rstrip(b'\n'), BASE.replace(b' ', b'\t')):
            self.assertIsNotNone(inspect(raw))

    def test_reject_active_and_unsupported(self):
        for suffix in (b'.include other', b'.control\nshell command\n.endc', b'.model x r',
                       b'.param x=1', b'.end', b'+ continuation', b'R2 a b 1k'):
            with self.subTest(suffix=suffix):
                self.assertIsNone(inspect(BASE + suffix))
        for body in (b'Rbody second first {value}', b'Rbody first second 1k',
                     b'Cbody first second {value} IC=0', b'Xbody first second value',
                     b'Rbody first second {value*2}', b'.subckt nested a b v=1',
                     b'Rbody first second {value}\nRtwo first second {value}'):
            self.assertIsNone(inspect(BASE.replace(b'Rbody first second {value}', body)))

    def test_identity_and_incomplete(self):
        for raw in (b'', b'* comment\n', BASE.split(b'.ends')[0], BASE + BASE.upper(),
                    BASE.replace(b'.ends sample', b'.ends other'),
                    BASE.replace(b'first second', b'first FIRST'),
                    BASE.replace(b'first', b'0'), BASE.replace(b'sample', b'1sample'),
                    BASE.replace(b'sample', b'x' * 65)):
            self.assertIsNone(inspect(raw))

    def test_parameter_literals(self):
        for value in (b'1', b'.5', b'1.', b'1e-18', b'1e+18', b'2MEG', b'1m', b'1u', b'9.8e2k'):
            self.assertIsNotNone(inspect(BASE.replace(b'1000', value)))
        for value in (b'0', b'0.000', b'-1', b'+1', b'nan', b'inf', b'1e19', b'1e001',
                      b'1e', b'1mil', b'1Ohm', b'{x}', b'1/2', b'1;quit', b'12345678901234567'):
            self.assertIsNone(inspect(BASE.replace(b'1000', value)))
        for name in (b'm', b'TEMP', b'temper', b'time', b'hertz', b'pi', b'e'):
            self.assertIsNone(inspect(BASE.replace(b'value', name)))

    def test_encoding(self):
        for byte in (b'\x00', b'\x01', b'\x7f', b'\x80', b'\r'):
            self.assertIsNone(inspect(b'* comment' + byte + b'x\n' + BASE))

    def test_budgets(self):
        self.assertIsNotNone(inspect(b'*' * 1024 + b'\n' + BASE))
        self.assertIsNone(inspect(b'*' * 1025 + b'\n' + BASE))
        self.assertIsNotNone(inspect(b'\n' * 1021 + BASE))
        self.assertIsNone(inspect(b'\n' * 1022 + BASE))
        models = b''.join(BASE.replace(b'sample', ('s%d' % i).encode()) for i in range(32))
        self.assertEqual(len(inspect(models)), 32)
        self.assertIsNone(inspect(models + BASE))
        padding = 65536 - len(BASE)
        lines, rest = divmod(padding, 1025)
        exact = (b'*' * 1024 + b'\n') * lines + b'*' * (rest - 1) + b'\n' + BASE
        self.assertEqual(len(exact), 65536)
        self.assertIsNotNone(inspect(exact))
        self.assertIsNone(inspect(exact + b'\n'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--probe', required=True)
    args, remaining = parser.parse_known_args()
    PROBE = args.probe
    unittest.main(argv=[__file__] + remaining)
