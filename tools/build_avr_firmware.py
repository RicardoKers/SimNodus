"""Build the AVR example firmware (experiment E-10) into .elf and .hex files.

Each examples/firmware/avr_<name>/avr_<name>.c is built for the ATmega8, and
each avr328_<name>/avr328_<name>.c for the ATmega328P and each
avr32_<name>/avr32_<name>.c for the ATmega32, with avr-gcc and avr-libc. The default clock is the factory one (internal RC,
1 MHz); VARIANTS adds builds at another F_CPU (the firmware's FUSES select
the clock source). The ELF and HEX files are committed so that the examples
run without a compiler; avr_firmware.json records the compiler and the
SHA-256 of each file.

The compiler is found with --toolchain, then avr-gcc on PATH, then the one
the Arduino AVR core installs for arduino-cli (D:/data/packages/arduino/
tools/avr-gcc/*/bin, the data folder of build/tools/arduino-cli).

    python tools/build_avr_firmware.py [--toolchain DIR]
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
FLAGS = ["-std=c11", "-Os", "-g", "-Wall", "-Wextra", "-Werror", "-ffunction-sections",
         "-fdata-sections", "-Wl,--gc-sections", "-fdebug-prefix-map={root}=."]  # GCC 7: no -ffile-prefix-map
# name -> list of (suffix, F_CPU)
VARIANTS = {
    "avr_blink": [("", 1_000_000), ("_16mhz", 16_000_000)],
    "avr_bus": [("", 8_000_000)],
    "avr328_serial": [("", 8_000_000), ("_16mhz", 16_000_000)],
}


def find_compiler(explicit: Path | None) -> Path:
    name = "avr-gcc.exe" if sys.platform == "win32" else "avr-gcc"
    if explicit:
        candidate = explicit / name if explicit.is_dir() else explicit
        if candidate.is_file():
            return candidate
        raise SystemExit(f"No {name} at {explicit}")
    if found := shutil.which("avr-gcc"):
        return Path(found)
    for folder in sorted(glob.glob("D:/data/packages/arduino/tools/avr-gcc/*/bin"), reverse=True):
        if (Path(folder) / name).is_file():
            return Path(folder) / name
    raise SystemExit("avr-gcc not found: pass --toolchain or install the Arduino AVR core with arduino-cli")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--toolchain", type=Path)
    args = parser.parse_args()
    gcc = find_compiler(args.toolchain)
    objcopy = gcc.with_name(gcc.name.replace("gcc", "objcopy"))
    version = subprocess.run([str(gcc), "--version"], capture_output=True, text=True, check=True).stdout.splitlines()[0]
    record = {"compiler": version, "flags": FLAGS, "firmware": {}}
    flags = [f.replace("{root}", str(ROOT)) for f in FLAGS]
    for folder in sorted([*FIRMWARE.glob("avr_*"), *FIRMWARE.glob("avr328_*"), *FIRMWARE.glob("avr32_*")]):
        source = folder / f"{folder.name}.c"
        if not source.is_file():
            continue
        for suffix, f_cpu in VARIANTS.get(folder.name, [("", 1_000_000)]):
            name = folder.name + suffix
            elf = folder / f"{name}.elf"
            hex_file = folder / f"{name}.hex"
            mcu = ("atmega328p" if folder.name.startswith("avr328_")
                   else "atmega32" if folder.name.startswith("avr32_") else "atmega8")
            subprocess.run([str(gcc), f"-mmcu={mcu}", *flags, f"-DF_CPU={f_cpu}UL", str(source), "-o", str(elf)],
                           check=True)
            subprocess.run([str(objcopy), "-O", "ihex", "-R", ".eeprom", "-R", ".fuse", "-R", ".lock", str(elf),
                            str(hex_file)], check=True)
            record["firmware"][name] = {
                "source": source.relative_to(ROOT).as_posix(),
                "mcu": mcu,
                "f_cpu": f_cpu,
                "elf": elf.relative_to(ROOT).as_posix(),
                "elf_sha256": sha256(elf),
                "hex": hex_file.relative_to(ROOT).as_posix(),
                "hex_sha256": sha256(hex_file),
            }
            print(f"{name}: {elf.stat().st_size} bytes of ELF")
    with open(FIRMWARE / "avr_firmware.json", "w", encoding="utf-8", newline="\n") as out:
        out.write(json.dumps(record, indent=2) + "\n")


if __name__ == "__main__":
    main()
