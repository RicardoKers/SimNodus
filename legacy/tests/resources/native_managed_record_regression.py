# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Independent struct/hashlib record vectors; no managed store or resource access."""
import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile


def u32(value):
    return struct.pack("<I", value)


def root(locator=b"C:/missing-inert-root", policy=1):
    return b"SRC1" + b"\x05" * 16 + struct.pack("<Q", 7) + b"\x06" * 16 + u32(policy) + u32(len(locator)) + locator


GEN = b"\x01" * 16
DOC = b"\x02" * 16
COMMIT = b"\x03" * 16
OP = b"\x04" * 16
SID = bytes.fromhex("010100000000000515000000")
NULL = bytes(108)
PROJECT = (Path(__file__).resolve().parents[1] / "schema/fixtures/two-rc-project.json").read_bytes()


def record(project=PROJECT, context=None, sid=SID, revision=1, parent=NULL,
           generation=GEN, document=DOC, commit=COMMIT, operation=OP):
    context = root() if context is None else context
    lengths = u32(len(sid)) + u32(len(context)) + u32(len(project))
    payload = sid + context + project
    request = b"SNREQ001" + generation + document + operation + parent + lengths + payload
    body = (b"SNMC0001" + u32(268 + len(payload)) + u32(0) + u32(revision) + lengths
            + generation + document + commit + operation + parent + hashlib.sha256(request).digest() + payload)
    return body + hashlib.sha256(body).digest()


def mutate(value, offset, replacement, resign=False):
    changed = value[:offset] + replacement + value[offset + len(replacement):]
    return changed[:-32] + hashlib.sha256(changed[:-32]).digest() if resign else changed


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", required=True)
    args = parser.parse_args()
    cases = []

    def add(name, data, accepted=False):
        cases.append((name, data, accepted))

    first = record()
    parent = GEN + DOC + u32(1) + COMMIT + first[-32:] + struct.pack("<Q", 10) + b"\x06" * 16
    add("independent-import", first, True)
    add("same-content-new-revision", record(revision=2, parent=parent, commit=b"\x08" * 16), True)
    parent63 = GEN + DOC + u32(63) + COMMIT + first[-32:] + struct.pack("<Q", 10) + b"\x06" * 16
    add("last-revision", record(revision=64, parent=parent63, commit=b"\x08" * 16), True)
    add("max-document", record(project=PROJECT + b" " * (1024 * 1024 - len(PROJECT))), True)
    add("max-sid", record(sid=b"\x01\x0f" + bytes(6) + bytes(range(60))), True)
    add("utf8-root", record(context=root("D:/caf\u00e9/\U0001f600".encode())), True)
    add("backslash-root", record(context=root(b"D:\\fixture\\root")), True)
    add("drive-root", record(context=root(b"C:/")), True)
    add("max-root", record(context=root(b"C:/" + b"a" * 4093)), True)
    # Every fixed header/token/digest cut and representative variable boundaries.
    for size in sorted(set(range(269)) | {len(first) - 33, len(first) - 32, len(first) - 1}):
        add(f"truncated-{size}", first[:size])
    add("trailing-byte", first + b"x")
    add("concatenated-records", first * 2)
    add("record-budget", bytes(2 * 1024 * 1024 + 1))
    for offset in (8, 20, 24, 28):
        for number in (0, 0xffffffff, 0x80000000):
            add(f"hostile-length-{offset}-{number}", mutate(first, offset, u32(number), True))
    for offset in range(8):
        add(f"magic-{offset}", mutate(first, offset, b"X", True))
    for number in (1, 0xffffffff):
        add(f"reserved-{number}", mutate(first, 12, u32(number), True))
    for revision in (0, 65, 0xffffffff):
        add(f"revision-{revision}", record(revision=revision))
    for field in ("generation", "document", "commit", "operation"):
        add(f"null-{field}", record(**{field: bytes(16)}))
    add("import-with-parent", record(parent=parent))
    add("revision-without-parent", record(revision=2))
    add("reused-commit-id", record(revision=2, parent=parent))
    for offset, count in ((0, 16), (16, 16), (32, 4), (36, 16), (52, 32), (92, 16)):
        add(f"invalid-parent-{offset}", record(revision=2, commit=b"\x08" * 16,
            parent=mutate(parent, offset, bytes(count))))
    for sid in (b"", SID[:8], SID + b"x", b"\x02" + SID[1:], b"\x01\x10" + bytes(70)):
        add(f"sid-{sid.hex()}", record(sid=sid))
    for locator in (b"", b"C:relative", b"//host/share", b"\\\\?\\C:\\root", b"C:/../root",
                    b"C:/NUL", b"C:/a~1", b"C:/a.", b"C:/a ", b"C:/a:stream", b"C:/a\x00b",
                    b"C:/a//b", b"C:/" + b"a" * 4094, b"C:/\xc0\xaf", b"C:/\xed\xa0\x80",
                    b"C:/\xf4\x90\x80\x80", b"C:/\xe2\x82", b"C:/\x80"):
        add(f"root-{locator[:40]!r}", record(context=root(locator)))
    add("unknown-policy", record(context=root(policy=2)))
    add("context-trailing", record(context=root() + b"x"))
    add("context-zero-binding", record(context=mutate(root(), 4, bytes(16))))
    add("context-zero-identity", record(context=mutate(root(), 28, bytes(16))))
    add("context-version", record(context=mutate(root(), 0, b"SRC2")))
    add("context-budget", record(context=bytes(16 * 1024 + 1)))
    for project in (b"", b"{}", PROJECT + b"x", b" " * (1024 * 1024 + 1), b"\xff"):
        add(f"project-{len(project)}", record(project=project))
    # A valid outer hash must not conceal a changed request digest or request input.
    for offset in (32, 48, 80, 204, 244, 252, 300):
        add(f"request-binding-{offset}", mutate(first, offset, bytes([first[offset] ^ 1]), True))
    for offset in (0, 32, 64, 204, 236, len(first) - 33, len(first) - 1):
        add(f"integrity-{offset}", mutate(first, offset, bytes([first[offset] ^ 1])))
    with tempfile.TemporaryDirectory(prefix="sn021-codec-") as directory:
        paths = []
        for index, (_, data, _) in enumerate(cases):
            path = Path(directory) / f"{index}.bin"
            path.write_bytes(data)
            paths.append(str(path))
        result = subprocess.run([args.probe], input="\n".join(paths) + "\n", text=True,
                                capture_output=True, timeout=100, check=True)
        lines = result.stdout.splitlines()
        assert len(lines) == len(cases), (len(lines), len(cases), result.stderr)
        for (name, _, accepted), line in zip(cases, lines):
            assert (line == "accept") == accepted, (name, line, accepted)
    print(f"{len(cases)} independent canonical record cases passed; no storage/authority claim")


if __name__ == "__main__":
    main()
