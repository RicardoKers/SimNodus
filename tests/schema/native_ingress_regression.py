"""Syntax differential tests; neither schema acceptance nor resource execution."""
import argparse
import json
from pathlib import Path
import random
import subprocess


def reference(raw):
    def pairs(items):
        result = {}
        for key, value in items:
            if key in result:
                raise ValueError("Duplicate decoded key")
            result[key] = value
        return result

    def constant(_):
        raise ValueError("Non-finite literal")

    return json.loads(raw.decode("utf-8"), object_pairs_hook=pairs, parse_constant=constant)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    args = parser.parse_args()
    cases = 0

    def run(raw, expected=True):
        nonlocal cases
        result = subprocess.run([args.probe], input=raw, capture_output=True, timeout=10)
        cases += 1
        assert result.returncode == (0 if expected else 1), (cases, raw[:100], result.stdout, result.stderr)
        if not expected:
            assert result.stdout.startswith(b"ERR ")
            return
        assert result.stdout.startswith(b"OK\n") or result.stdout.startswith(b"OK\r\n")
        tokens = []
        for line in result.stdout.decode("ascii").splitlines()[1:]:
            kind, begin, end, next_index, decoded = line.split(" ", 4)
            tokens.append((int(kind), int(begin), int(end), int(next_index), bytes.fromhex(decoded).decode("utf-8")))
        assert tokens and tokens[0][3] == len(tokens)

        def reconstruct(index):
            kind, begin, end, next_index, decoded = tokens[index]
            assert 0 <= begin < end <= len(raw) and index < next_index <= len(tokens)
            fragment = raw[begin:end]
            if kind in (0, 1):
                result = {} if kind == 0 else []
                child = index + 1
                while child < next_index:
                    if kind == 0:
                        assert tokens[child][0] == 2 and tokens[child][3] == child + 1
                        key = reconstruct(child)
                        child += 1
                        result[key] = reconstruct(child)
                    else:
                        result.append(reconstruct(child))
                    assert begin < tokens[child][1] < tokens[child][2] < end
                    child = tokens[child][3]
                assert child == next_index
                return result
            assert next_index == index + 1
            value = reference(fragment)
            expected_kind = 2 if type(value) is str else 4 if type(value) is bool else 5 if value is None else 3
            assert kind == expected_kind
            if kind == 2:
                assert decoded == value
            else:
                assert decoded == ""
            return value

        assert reconstruct(0) == reference(raw), raw[:100]

    for fixture in sorted(Path("tests/schema/fixtures").glob("*.json")):
        run(fixture.read_bytes())  # Invalid schema fixtures still have valid syntax.
    for value in (None, True, False, 0, -10, 2**64 - 1, 2**64, 1.25, "", "a\x00\b\f\n\r\t/\\\"", "é𝄞\U0010ffff", {"é": [1, {}, []]}):
        for ascii_only in (True, False):
            run(json.dumps(value, ensure_ascii=ascii_only).encode())
    rng = random.Random(21054)

    def generate(depth):
        if depth == 0:
            return rng.choice([None, False, True, rng.randrange(-(2**64), 2**64), "é𝄞", "line\n", ""])
        if rng.randrange(2):
            return [generate(depth - 1) for _ in range(rng.randrange(5))]
        return {f"key{i}": generate(depth - 1) for i in range(rng.randrange(5))}

    for _ in range(100):
        run(json.dumps(generate(4), ensure_ascii=bool(rng.randrange(2))).encode())
    run('{"é":1,"e\\u0301":2,"A":3,"a":4}'.encode())
    run('{"𝄞":1,"\\uD834\\uDD1E":2}'.encode(), False)
    for raw in (b'{"x":1,"\\u0078":2}', b'{"a":{"x":1,"x":2}}', b'{"x":1,}', b'[1,]',
                b'00', b'-01', b'1e+', b'1.', b'+1', b'--1', b'NaN', b'Infinity', b'-Infinity',
                b'{} false', b'\xef\xbb\xbf{}', b'\xff', b'"\xc0\xaf"', b'"\xed\xa0\x80"',
                b'"\xf4\x90\x80\x80"', b'"\x00"', b'"\\v"', b'"\\u12g4"', b'//comment\n{}', b'[\x0b0]'):
        try:
            reference(raw)
        except (ValueError, UnicodeError):
            pass
        else:
            raise AssertionError(("Oracle unexpectedly accepts", raw))
        run(raw, False)
    for raw in (b'"\\ud800"', b'"\\udfff"', b'"\\ud800\\u0000"'):
        reference(raw)  # Deliberately stricter native Unicode policy.
        run(raw, False)
    for raw in (b'{"x": ["\\uD834\\uDD1E", true, 12.3e-2]}', b'"\xf4\x8f\xbf\xbf"'):
        for end in range(len(raw)):
            run(raw[:end], False)
    print(f"{cases} native ingress differential/adversarial cases passed")


if __name__ == "__main__":
    main()
