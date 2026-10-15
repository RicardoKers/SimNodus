"""Builds bench.elf (emulator benchmark mix, E-07 gate G0a.3) with the example flags."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from build_firmware import FLAGS, find_compiler  # noqa: E402


def main() -> int:
    compiler = find_compiler(Path(sys.argv[1]) if len(sys.argv) > 1 else None)
    flags = [f.format(root=ROOT.as_posix()) for f in FLAGS]
    subprocess.run([str(compiler), *flags, "-T", "examples/firmware/stm32f103c8.ld",
                    "tests/experiments/emu-benchmark/bench.c", "-o", "tests/experiments/emu-benchmark/bench.elf"],
                   cwd=ROOT, check=True, timeout=120)
    print("built tests/experiments/emu-benchmark/bench.elf")
    return 0


if __name__ == "__main__":
    sys.exit(main())
