"""Build the owned E-04 firmware twice with an explicitly selected ARM GCC."""
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
    parser.add_argument('--toolchain', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'build/sn015/firmware')
    args = parser.parse_args()
    compiler = (args.toolchain / 'arm-none-eabi-gcc.exe').resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    version = subprocess.check_output([str(compiler), '--version'], text=True, timeout=10).splitlines()[0]
    flags = [
        '-mcpu=cortex-m3', '-mthumb', '-std=c11', '-Os', '-g0', '-ffreestanding',
        '-fno-builtin', '-fno-common', '-ffunction-sections', '-fdata-sections',
        '-Wall', '-Wextra', '-Werror', '-nostdlib', '-Wl,--build-id=none',
        '-Wl,--gc-sections', '-T', 'firmware.ld', 'firmware.c',
    ]
    for name in ('firmware.elf', 'repeat.elf'):
        subprocess.run([str(compiler), *flags, '-o', str(output / name)], cwd=HERE, check=True, timeout=60)
    if digest(output / 'firmware.elf') != digest(output / 'repeat.elf'):
        raise ValueError('Repeated E-04 firmware build changed the ELF')

    nm = args.toolchain / 'arm-none-eabi-nm.exe'
    symbols_text = subprocess.check_output([str(nm), '-n', str(output / 'firmware.elf')], text=True, timeout=10)
    selected = {}
    for line in symbols_text.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[2] in ('_stack_top', 'vectors', 'Reset_Handler', 'mailbox'):
            selected[fields[2]] = int(fields[0], 16)
    if set(selected) != {'_stack_top', 'vectors', 'Reset_Handler', 'mailbox'}:
        raise ValueError('Missing E-04 firmware symbols')
    if selected['vectors'] != 0x08000000 or selected['_stack_top'] != 0x20005000 or selected['mailbox'] != 0x20000000:
        raise ValueError('Wrong E-04 vector, stack, or mailbox map')
    (output / 'firmware_layout.h').write_text(
        '#define SN_E04_FIRMWARE_MAILBOX_ADDRESS 0x20000000u\n', encoding='utf-8')
    sources = (HERE / 'firmware.c', HERE / 'firmware.ld', HERE / 'mailbox.h', HERE / 'build_firmware.py')
    metadata = {
        'compiler_version': version,
        'compiler_sha256': digest(compiler),
        'flags': flags,
        'elf_sha256': digest(output / 'firmware.elf'),
        'identical_rebuild': True,
        'symbols': selected,
        'sources_sha256': {path.name: digest(path) for path in sources},
    }
    (output / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(metadata, indent=2))


if __name__ == '__main__':
    main()
