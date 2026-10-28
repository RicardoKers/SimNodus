/* Copyright (c) 2026 Ricardo Kerschbaumer
   SPDX-License-Identifier: MIT
   riscv-arch-test programs at the start of the test machine's RAM. */
OUTPUT_ARCH("riscv")
ENTRY(rvtest_entry_point)
SECTIONS
{
    . = 0x80000000;
    .text.init : { *(.text.init) }
    . = ALIGN(0x1000);
    .text : { *(.text) }
    . = ALIGN(0x1000);
    .data : { *(.data) }
    .data.string : { *(.data.string) }
    .bss : { *(.bss) }
    _end = .;
}
