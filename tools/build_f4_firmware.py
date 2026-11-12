"""Build the STM32F407 test firmware in examples/firmware/f4_<name>/ into f4_<name>.elf.

Each folder holds one C source for the STM32F407VE that includes
examples/firmware/f4_common/f4.h (registers, vector table, start-up), or
f4_peripherals.h there (I2C, USART through DMA, the RTC, on top of f4.h), and
links with examples/firmware/stm32f407ve.ld, without a C library, for the
Cortex-M4 with its FPU (hard float ABI, as STM32CubeIDE builds). A folder
with a cube.json is built like an STM32CubeIDE project instead: its sources
plus the HAL, CMSIS and startup files cube.json names from an STM32CubeF4
package (--cube-package, else ~/STM32Cube/Repository/<package>), with
newlib-nano and examples/firmware/stm32f407ve_cube.ld. The ELF
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
from build_firmware import find_compiler, find_cube_package  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "examples" / "firmware"
FLAGS = ["-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-std=c11", "-O2", "-g",
         "-ffreestanding", "-fno-builtin", "-fno-math-errno", "-fno-tree-loop-distribute-patterns", "-fno-common",
         "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror", "-nostdlib",
         "-Wl,--build-id=none", "-Wl,--gc-sections", "-ffile-prefix-map={root}=."]

# STM32CubeIDE's defaults for a release build, without -Werror: the HAL is not ours.
CUBE_FLAGS = ["-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-std=gnu11", "-Os", "-g",
              "-ffunction-sections", "-fdata-sections", "-Wall", "--specs=nano.specs", "--specs=nosys.specs",
              "-Wl,--build-id=none", "-Wl,--gc-sections", "-Wl,--no-warn-rwx-segments",
              "-ffile-prefix-map={root}=.", "-ffile-prefix-map={package}={package_name}"]


def cube_command(compiler: Path, folder: Path, sources: list[str], cube_package: Path | None) -> tuple[list[str], list[str]]:
    """Compiler command without the output file for a cube.json folder, and the flags to record."""
    cube = json.loads((folder / "cube.json").read_text(encoding="utf-8"))
    package = find_cube_package(cube["package"], cube_package)
    flags = [f.format(root=ROOT.as_posix(), package=package.as_posix(), package_name=cube["package"])
             for f in CUBE_FLAGS]
    defines = [f"-D{d}" for d in cube["defines"]]
    includes = [f"-I{folder.relative_to(ROOT).as_posix()}"] + [f"-I{(package / i).as_posix()}" for i in cube["includes"]]
    package_sources = []
    for name in cube["sources"]:
        source = package / name
        if source.suffix != ".s":
            package_sources.append(source.as_posix())
            continue
        # Assembled on its own, so the ELF does not name a temporary file.
        target = ROOT / "build" / "firmware" / folder.name / (source.stem + ".o")
        target.parent.mkdir(parents=True, exist_ok=True)
        relative = target.relative_to(ROOT).as_posix()
        subprocess.run([str(compiler), *flags, "-c", source.as_posix(), "-o", relative], cwd=ROOT, check=True, timeout=120)
        package_sources.append(relative)
    command = [str(compiler), *flags, *defines, *includes, "-T", "examples/firmware/stm32f407ve_cube.ld",
               *sources, *package_sources, "-o"]
    return command, CUBE_FLAGS + defines + [f"{cube['package']}/{s}" for s in cube["sources"]]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--toolchain", type=Path, help="compiler executable or its bin folder")
    parser.add_argument("--cube-package", type=Path, help="STM32CubeF4 package folder for cube.json firmware")
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
        recorded = None
        if (folder / "cube.json").is_file():
            command, recorded = cube_command(compiler, folder, sources, args.cube_package)
        else:
            command = [str(compiler), *flags, "-T", "examples/firmware/stm32f407ve.ld", *sources, "-o"]
        subprocess.run([*command, str(elf)], cwd=ROOT, check=True, timeout=120)
        repeat = folder / f"{folder.name}.repeat.elf"
        subprocess.run([*command, str(repeat)], cwd=ROOT, check=True, timeout=120)
        same = sha256(elf) == sha256(repeat)
        repeat.unlink()
        if not same:
            raise SystemExit(f"Repeated build of {folder.name} changed the ELF")
        if recorded is not None:
            manifest["firmware"][folder.name] = {"sources": sources + [f"{folder.relative_to(ROOT).as_posix()}/{h.name}" for h in sorted(folder.glob("*.h"))],
                                                 "elf": elf.relative_to(ROOT).as_posix(), "sha256": sha256(elf),
                                                 "size": elf.stat().st_size, "flags": recorded}
            print(f"built {elf.relative_to(ROOT)} ({elf.stat().st_size} bytes)")
            continue
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
