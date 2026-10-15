"""Build the example firmware in examples/firmware/<name>/ into <name>.elf.

Each folder holds C sources for the STM32F103C8 (Blue Pill); they link with
examples/firmware/stm32f103c8.ld, without a C library. The ELF files are
committed so that the examples run without a compiler; firmware.json records
the compiler and the SHA-256 of each ELF.

The compiler is found with --toolchain, then arm-none-eabi-gcc on PATH, then
the GNU Tools for STM32 inside an STM32CubeIDE installation (C:/ST/...).
"""

from __future__ import annotations

import argparse
import glob
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "examples" / "firmware"
FLAGS = ["-mcpu=cortex-m3", "-mthumb", "-std=c11", "-Os", "-g", "-ffreestanding", "-fno-builtin",
         "-fno-common", "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror",
         "-nostdlib", "-Wl,--build-id=none", "-Wl,--gc-sections", "-ffile-prefix-map={root}=."]


def find_compiler(explicit: Path | None) -> Path:
    name = "arm-none-eabi-gcc.exe" if sys.platform == "win32" else "arm-none-eabi-gcc"
    if explicit:
        candidate = explicit / name if explicit.is_dir() else explicit
        if candidate.is_file():
            return candidate
        raise SystemExit(f"No {name} at {explicit}")
    if found := shutil.which("arm-none-eabi-gcc"):
        return Path(found)
    pattern = "C:/ST/STM32CubeIDE_*/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*/tools/bin/"
    for folder in sorted(glob.glob(pattern), reverse=True):
        if (Path(folder) / name).is_file():
            return Path(folder) / name
    raise SystemExit("arm-none-eabi-gcc not found: pass --toolchain or install STM32CubeIDE / Arm GNU Toolchain")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--toolchain", type=Path, help="compiler executable or its bin folder")
    args = parser.parse_args()
    compiler = find_compiler(args.toolchain)
    version = subprocess.check_output([str(compiler), "--version"], text=True, timeout=30).splitlines()[0]
    print(f"compiler: {version}")
    manifest = {"compiler": version, "flags": FLAGS, "firmware": {}}
    flags = [f.format(root=ROOT.as_posix()) for f in FLAGS]
    for folder in sorted(p for p in FIRMWARE.iterdir() if p.is_dir()):
        sources = sorted(str(s.relative_to(ROOT).as_posix()) for s in folder.glob("*.c"))
        if not sources:
            continue
        elf = folder / f"{folder.name}.elf"
        command = [str(compiler), *flags, "-T", "examples/firmware/stm32f103c8.ld", *sources, "-o"]
        subprocess.run([*command, str(elf)], cwd=ROOT, check=True, timeout=120)
        repeat = folder / f"{folder.name}.repeat.elf"
        subprocess.run([*command, str(repeat)], cwd=ROOT, check=True, timeout=120)
        same = sha256(elf) == sha256(repeat)
        repeat.unlink()
        if not same:
            raise SystemExit(f"Repeated build of {folder.name} changed the ELF")
        manifest["firmware"][folder.name] = {"sources": sources, "elf": elf.relative_to(ROOT).as_posix(),
                                             "sha256": sha256(elf), "size": elf.stat().st_size}
        print(f"built {elf.relative_to(ROOT)} ({elf.stat().st_size} bytes)")
    (FIRMWARE / "firmware.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
