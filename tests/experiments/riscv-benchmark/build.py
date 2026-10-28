#!/usr/bin/env python3
# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
"""Builds bench.elf and busy.elf (RV32IMC benchmarks, E-08 gate G8a.3) with ESP-IDF's RISC-V compiler."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
DEFAULT = ROOT / "build" / "tools" / "riscv32-esp-elf" / "bin" / "riscv32-esp-elf-gcc"
FLAGS = ["-march=rv32imc_zicsr", "-mabi=ilp32", "-std=c11", "-Os", "-ffreestanding", "-fno-builtin",
         "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror", "-nostdlib",
         "-Wl,--build-id=none", "-Wl,--gc-sections", "-Wl,--no-warn-rwx-segments", f"-ffile-prefix-map={ROOT.as_posix()}=."]


def main() -> int:
    compiler = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT
    for source, flags in (("bench.c", FLAGS), ("busy.S", ["-march=rv32imc", "-mabi=ilp32", *FLAGS[-4:]])):
        elf = Path(source).with_suffix(".elf").name
        subprocess.run([str(compiler), *flags, "-nostdlib", "-T", "tests/experiments/riscv-benchmark/bench.ld",
                        f"tests/experiments/riscv-benchmark/{source}", "-o", f"tests/experiments/riscv-benchmark/{elf}"],
                       cwd=ROOT, check=True, timeout=120)
        print(f"built tests/experiments/riscv-benchmark/{elf}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
