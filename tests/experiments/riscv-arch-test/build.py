#!/usr/bin/env python3
# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Build the riscv-arch-test 2.7.4 corpus for the RV32IMC core (E-08, gate G8a.1).

Compiles the rv32i_m tests that apply to an RV32IMC_Zicsr_Zifencei core
that traps on misaligned data accesses (each test's RVTEST_CASE lines say
which ISA and platform it needs and which macros to define), with
model/model_test.h and model/link.ld, and copies each ELF with its reference
signature into corpus/<suite>/. The emulator tests (ctest emu-riscv-arch)
compare the signatures.

Each test is compiled for the ISA its RVTEST_ISA line names (the reference
signatures depend on whether the assembler may use compressed encodings).
GNU as 14 rejects `la x0, label`, which jalr-01 uses; the preprocessed source
gets `auipc x0, 0; addi x0, x0, 0` instead, the same size and no effect.

    git clone --depth 1 --branch 2.7.4 https://github.com/riscv-non-isa/riscv-arch-test.git build/riscv-arch-test
    python tests/experiments/riscv-arch-test/build.py --gcc build/tools/riscv32-esp-elf/bin/riscv32-esp-elf-gcc
"""
import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
SUITES = ["I", "M", "C", "privilege", "Zifencei"]
ISA = "RV32IMCZicsr_Zifencei"
# The core traps on misaligned loads and stores (no hw_data_misaligned_support).
PLATFORM = {"hw_data_misaligned_support": "False"}
FLAGS = ["-mabi=ilp32", "-static", "-mcmodel=medany", "-fvisibility=hidden", "-nostdlib", "-nostartfiles",
         "-DXLEN=32"]
TEST_ISA = re.compile(r'RVTEST_ISA\("RV32([A-Z]+)(?:_[A-Za-z]+)*"\)')
LA_X0 = re.compile(r"\bla\s+x0\s*,\s*[^;\n]+")


def march_for(source: Path) -> str:
    match = TEST_ISA.search(source.read_text(encoding="utf-8"))
    letters = match.group(1).lower() if match else "i"
    return f"-march=rv32{letters}_zicsr_zifencei"
CASE = re.compile(r'RVTEST_CASE\(\s*\d+\s*,\s*"([^"]*)"')


def defines_for(source: Path):
    """The -D flags of the first RVTEST_CASE whose checks this core meets, or None."""
    for condition in CASE.findall(source.read_text(encoding="utf-8")):
        checks = re.findall(r"check\s+(\w+)\s*:=\s*([^;]+)", condition)
        ok = True
        for key, value in checks:
            value = value.strip()
            if key == "ISA":
                pattern = re.fullmatch(r"regex\((.*)\)", value)
                ok = ok and pattern is not None and re.match(pattern.group(1), ISA) is not None
            else:
                ok = ok and PLATFORM.get(key) == value
        if ok:
            return [f"-D{name}={val.strip()}" for name, val in re.findall(r"def\s+(\w+)\s*=\s*([^;]+)", condition)]
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--arch-test", type=Path, default=ROOT / "build" / "riscv-arch-test")
    parser.add_argument("--gcc", type=Path, default=ROOT / "build" / "tools" / "riscv32-esp-elf" / "bin" / "riscv32-esp-elf-gcc")
    parser.add_argument("--out", type=Path, default=HERE / "corpus")
    args = parser.parse_args()
    suite_root = args.arch_test / "riscv-test-suite"
    env = suite_root / "env"
    if not env.is_dir():
        print(f"riscv-arch-test not found at {args.arch_test}", file=sys.stderr)
        return 1
    if args.out.exists():
        shutil.rmtree(args.out)
    built = 0
    skipped = []
    for suite in SUITES:
        sources = sorted((suite_root / "rv32i_m" / suite / "src").glob("*.S"))
        target = args.out / suite
        target.mkdir(parents=True)
        for source in sources:
            name = source.stem
            defines = defines_for(source)
            if defines is None:
                skipped.append(name)
                continue
            elf = target / f"{name}.elf"
            flags = [march_for(source), *FLAGS, *defines]
            expanded = subprocess.run([str(args.gcc), *flags, "-E", "-I", str(HERE / "model"), "-I", str(env),
                                       str(source)], check=True, capture_output=True, text=True).stdout
            with tempfile.TemporaryDirectory() as scratch:
                assembly = Path(scratch) / f"{name}.s"
                assembly.write_text(LA_X0.sub("auipc x0, 0; addi x0, x0, 0", expanded), encoding="utf-8")
                subprocess.run([str(args.gcc), *flags, "-T", str(HERE / "model" / "link.ld"), str(assembly),
                                "-o", str(elf)], check=True)
            shutil.copyfile(suite_root / "rv32i_m" / suite / "references" / f"{name}.reference_output",
                            target / f"{name}.reference_output")
            built += 1
    print(f"{built} tests built into {args.out}; not applicable: {', '.join(skipped) or 'none'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
