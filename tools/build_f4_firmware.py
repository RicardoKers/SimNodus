"""Build the STM32F407 test firmware in examples/firmware/f4_<name>/ into f4_<name>.elf.

Each folder holds one C source for the STM32F407VE that includes
examples/firmware/f4_common/f4.h (registers, vector table, start-up), or
f4_peripherals.h there (I2C, USART through DMA, the RTC, on top of f4.h), and
links with examples/firmware/stm32f407ve.ld, without a C library, for the
Cortex-M4 with its FPU (hard float ABI, as STM32CubeIDE builds). The ELF
files are committed so that the tests run without a compiler;
f4_firmware.json records the compiler, the flags and the SHA-256 of each ELF.

The compiler is found with --toolchain, then arm-none-eabi-gcc on PATH, then
the GNU Tools for STM32 inside an STM32CubeIDE installation (C:/ST/...).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_firmware import find_compiler  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "examples" / "firmware"
FLAGS = ["-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-std=c11", "-O2", "-g",
         "-ffreestanding", "-fno-builtin", "-fno-math-errno", "-fno-tree-loop-distribute-patterns", "-fno-common",
         "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror", "-nostdlib",
         "-Wl,--build-id=none", "-Wl,--gc-sections", "-ffile-prefix-map={root}=."]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--toolchain", type=Path, help="compiler executable or its bin folder")
    args = parser.parse_args()
    compiler = find_compiler(args.toolchain)
    version = subprocess.check_output([str(compiler), "--version"], text=True, timeout=30).splitlines()[0]
    print(f"compiler: {version}")
    flags = [f.format(root=ROOT.as_posix()) for f in FLAGS]
    manifest = {"compiler": version, "flags": FLAGS, "firmware": {}}
    for folder in sorted(p for p in FIRMWARE.glob("f4_*") if p.is_dir() and p.name != "f4_common"):
        sources = sorted(str(s.relative_to(ROOT).as_posix()) for s in folder.glob("*.c"))
        if not sources:
            continue
        elf = folder / f"{folder.name}.elf"
        command = [str(compiler), *flags, "-T", "examples/firmware/stm32f407ve.ld", *sources, "-o"]
        subprocess.run([*command, str(elf)], cwd=ROOT, check=True, timeout=120)
        repeat = folder / f"{folder.name}.repeat.elf"
        subprocess.run([*command, str(repeat)], cwd=ROOT, check=True, timeout=120)
        same = sha256(elf) == sha256(repeat)
        repeat.unlink()
        if not same:
            raise SystemExit(f"Repeated build of {folder.name} changed the ELF")
        text = "".join((ROOT / source).read_text(encoding="utf-8") for source in sources)
        headers = ["examples/firmware/f4_common/f4.h"] + sorted(
            h.relative_to(ROOT).as_posix() for h in (FIRMWARE / "f4_common").glob("*.h")
            if h.name != "f4.h" and h.name in text)
        manifest["firmware"][folder.name] = {"sources": sources + headers,
                                             "elf": elf.relative_to(ROOT).as_posix(), "sha256": sha256(elf),
                                             "size": elf.stat().st_size}
        print(f"built {elf.relative_to(ROOT)} ({elf.stat().st_size} bytes)")
    (FIRMWARE / "f4_firmware.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
