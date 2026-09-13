"""Build the owned debug firmware twice with stable DWARF paths."""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--toolchain", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=ROOT / "build/sn016/firmware")
    args = parser.parse_args()
    compiler = (args.toolchain / "arm-none-eabi-gcc.exe").resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    version = subprocess.check_output([str(compiler), "--version"], text=True, timeout=10).splitlines()[0]
    flags = [
        "-mcpu=cortex-m3", "-mthumb", "-std=c11", "-Og", "-g3", "-gdwarf-4",
        "-ffreestanding", "-fno-builtin", "-fno-common", "-ffunction-sections",
        "-fdata-sections", f"-fdebug-prefix-map={HERE.as_posix()}=/simnodus/e05",
        "-Wall", "-Wextra", "-Werror", "-nostdlib", "-Wl,--build-id=none",
        "-Wl,--gc-sections", "-T", "firmware.ld", "firmware.c",
    ]
    for name in ("firmware.elf", "repeat.elf"):
        subprocess.run([str(compiler), *flags, "-o", str(output / name)], cwd=HERE, check=True, timeout=60)
    if digest(output / "firmware.elf") != digest(output / "repeat.elf"):
        raise ValueError("Repeated E-05 firmware build changed the ELF")

    nm = args.toolchain / "arm-none-eabi-nm.exe"
    symbols_text = subprocess.check_output([str(nm), "-n", str(output / "firmware.elf")], text=True, timeout=10)
    names = {"vectors", "Reset_Handler", "mailbox", "gpio_change_marker", "step_helper", "step_target", "adc_read_marker"}
    symbols = {}
    for line in symbols_text.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[2] in names:
            symbols[fields[2]] = int(fields[0], 16)
    if set(symbols) != names:
        raise ValueError("Missing E-05 firmware debug symbols")
    if symbols["vectors"] != 0x08000000 or symbols["mailbox"] != 0x20000000:
        raise ValueError("Wrong E-05 vector or mailbox map")
    sources = [HERE / name for name in ("firmware.c", "firmware.ld", "mailbox.h", "build_firmware.py")]
    metadata = {
        "compiler_version": version,
        "compiler_sha256": digest(compiler),
        "flags": flags,
        "elf_sha256": digest(output / "firmware.elf"),
        "identical_rebuild": True,
        "debug_information": "DWARF-4",
        "symbols": symbols,
        "sources_sha256": {path.name: digest(path) for path in sources},
    }
    (output / "build.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(metadata, indent=2))


if __name__ == "__main__":
    main()
