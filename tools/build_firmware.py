"""Build the example firmware in examples/firmware/<name>/ into <name>.elf.

Each folder holds C sources for the STM32F103C8 (Blue Pill); they link with
examples/firmware/stm32f103c8.ld, without a C library. A folder with a
cube.json is built like an STM32CubeIDE project instead: its sources plus the
HAL, CMSIS and startup files cube.json names from an STM32CubeF1 package
(--cube-package, else ~/STM32Cube/Repository/<package>), with newlib-nano and
examples/firmware/stm32f103c8_cube.ld. The ELF files are committed so that the
examples run without a compiler; firmware.json records the compiler and the
SHA-256 of each ELF.

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
# STM32CubeIDE's defaults for a release build, without -Werror: the HAL is not ours.
CUBE_FLAGS = ["-mcpu=cortex-m3", "-mthumb", "-std=gnu11", "-Os", "-g", "-ffunction-sections", "-fdata-sections",
              "-Wall", "--specs=nano.specs", "--specs=nosys.specs", "-Wl,--build-id=none", "-Wl,--gc-sections",
              "-Wl,--no-warn-rwx-segments", "-ffile-prefix-map={root}=.", "-ffile-prefix-map={package}={package_name}"]


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


def find_cube_package(name: str, explicit: Path | None) -> Path:
    candidates = [explicit] if explicit else [Path.home() / "STM32Cube" / "Repository" / name]
    for candidate in candidates:
        if (candidate / "Drivers" / "STM32F1xx_HAL_Driver").is_dir():
            return candidate
    raise SystemExit(f"STM32CubeF1 package {name} not found: pass --cube-package or install it from STM32CubeIDE")


def build_command(compiler: Path, folder: Path, cube_package: Path | None) -> tuple[list[str], list[str]]:
    """Compiler command without the output file, and the flags recorded in firmware.json."""
    sources = sorted(str(s.relative_to(ROOT).as_posix()) for s in folder.glob("*.c"))
    cube_file = folder / "cube.json"
    if not cube_file.is_file():
        flags = [f.format(root=ROOT.as_posix()) for f in FLAGS]
        return [str(compiler), *flags, "-T", "examples/firmware/stm32f103c8.ld", *sources, "-o"], FLAGS
    cube = json.loads(cube_file.read_text(encoding="utf-8"))
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
        # Assembled on its own: in a one-step build the object's symbol table
        # names a random temporary file, so the ELF would differ on every build.
        target = ROOT / "build" / "firmware" / folder.name / (source.stem + ".o")
        target.parent.mkdir(parents=True, exist_ok=True)
        relative = target.relative_to(ROOT).as_posix()
        subprocess.run([str(compiler), *flags, "-c", source.as_posix(), "-o", relative], cwd=ROOT, check=True, timeout=120)
        package_sources.append(relative)
    command = [str(compiler), *flags, *defines, *includes, "-T", "examples/firmware/stm32f103c8_cube.ld",
               *sources, *package_sources, "-o"]
    return command, CUBE_FLAGS + defines + [f"{cube['package']}/{s}" for s in cube["sources"]]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--toolchain", type=Path, help="compiler executable or its bin folder")
    parser.add_argument("--cube-package", type=Path, help="STM32CubeF1 package folder for cube.json firmware")
    args = parser.parse_args()
    compiler = find_compiler(args.toolchain)
    version = subprocess.check_output([str(compiler), "--version"], text=True, timeout=30).splitlines()[0]
    print(f"compiler: {version}")
    manifest = {"compiler": version, "flags": FLAGS, "firmware": {}}
    for folder in sorted(p for p in FIRMWARE.iterdir() if p.is_dir()):
        sources = sorted(str(s.relative_to(ROOT).as_posix()) for s in folder.glob("*.c"))
        if not sources:
            continue
        elf = folder / f"{folder.name}.elf"
        command, recorded = build_command(compiler, folder, args.cube_package)
        subprocess.run([*command, str(elf)], cwd=ROOT, check=True, timeout=120)
        repeat = folder / f"{folder.name}.repeat.elf"
        subprocess.run([*command, str(repeat)], cwd=ROOT, check=True, timeout=120)
        same = sha256(elf) == sha256(repeat)
        repeat.unlink()
        if not same:
            raise SystemExit(f"Repeated build of {folder.name} changed the ELF")
        entry = {"sources": sources, "elf": elf.relative_to(ROOT).as_posix(),
                 "sha256": sha256(elf), "size": elf.stat().st_size}
        if recorded is not FLAGS:
            entry["flags"] = recorded
        manifest["firmware"][folder.name] = entry
        print(f"built {elf.relative_to(ROOT)} ({elf.stat().st_size} bytes)")
    (FIRMWARE / "firmware.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
