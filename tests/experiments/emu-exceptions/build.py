"""Builds exceptions.elf (exception-model scenarios, E-07 gate G0a.1) from exceptions.s."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[2] / "tools"))
from build_firmware import find_compiler  # noqa: E402


def main() -> int:
    compiler = find_compiler(Path(sys.argv[1]) if len(sys.argv) > 1 else None)
    linker = HERE / "exceptions.ld"
    subprocess.run([str(compiler), "-mcpu=cortex-m3", "-mthumb", "-nostdlib", "-Wl,--build-id=none",
                    "-Wl,--strip-all", "-T", str(linker), str(HERE / "exceptions.s"), "-o", str(HERE / "exceptions.elf")],
                   check=True, timeout=120)
    print("built tests/experiments/emu-exceptions/exceptions.elf")
    return 0


if __name__ == "__main__":
    sys.exit(main())
