"""Build only the owned E-03 firmware with an explicitly selected ARM GCC."""
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
    parser.add_argument('--output', type=Path, default=ROOT / 'build/sn014/firmware')
    args = parser.parse_args()
    compiler = (args.toolchain / 'arm-none-eabi-gcc.exe').resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    version = subprocess.check_output([str(compiler), '--version'], text=True, timeout=10).splitlines()[0]
    flags = ['-mcpu=cortex-m3', '-mthumb', '-std=c11', '-Os', '-g0', '-ffreestanding',
             '-fno-builtin', '-fno-common', '-ffunction-sections', '-fdata-sections',
             '-Wall', '-Wextra', '-Werror', '-nostdlib', '-Wl,--build-id=none',
             '-Wl,--gc-sections', '-T', 'firmware.ld', 'firmware.c']
    for name in ('firmware.elf', 'repeat.elf'):
        subprocess.run([str(compiler), *flags, '-o', str(output / name)], cwd=HERE, check=True, timeout=60)
    if digest(output / 'firmware.elf') != digest(output / 'repeat.elf'):
        raise ValueError('Repeated firmware build changed the ELF')
    nm = args.toolchain / 'arm-none-eabi-nm.exe'
    symbols = subprocess.check_output([str(nm), '-n', str(output / 'firmware.elf')], text=True, timeout=10)
    selected = {}
    wanted = {'initialized_cookie', 'zero_cookie', '_stack_top', 'vectors', 'Reset_Handler'}
    for line in symbols.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[2] in wanted:
            selected[fields[2]] = int(fields[0], 16)
    if set(selected) != wanted:
        raise ValueError('Missing firmware symbols')
    if selected['vectors'] != 0x08000000 or selected['_stack_top'] != 0x20005000:
        raise ValueError('Wrong vector/stack map')
    (output / 'firmware_layout.h').write_text(
        f'#define SN_E03_DATA_ADDRESS {selected["initialized_cookie"]}u\n'
        f'#define SN_E03_BSS_ADDRESS {selected["zero_cookie"]}u\n', encoding='utf-8')
    sources = [HERE / name for name in ('firmware.c', 'firmware.ld', 'mailbox.h', 'build_firmware.py')]
    metadata = {'compiler_version': version, 'compiler_sha256': digest(compiler), 'flags': flags,
                'elf_sha256': digest(output / 'firmware.elf'), 'identical_rebuild': True,
                'symbols': selected, 'sources_sha256': {p.name: digest(p) for p in sources}}
    (output / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(metadata, indent=2))


if __name__ == '__main__':
    main()
