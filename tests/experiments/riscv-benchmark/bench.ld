/* Copyright (c) 2026 Ricardo Kerschbaumer
   SPDX-License-Identifier: MIT
   The RISC-V benchmark in RAM at 0x80000000 (test machine and QEMU virt). */
OUTPUT_ARCH("riscv")
ENTRY(_start)
MEMORY
{
    RAM (rwx) : ORIGIN = 0x80000000, LENGTH = 1M
}
SECTIONS
{
    .text : { *(.text.start) *(.text*) *(.rodata*) *(.srodata*) } > RAM
    .data : { *(.data*) *(.sdata*) } > RAM
    .bss (NOLOAD) : { _bss_start = .; *(.bss*) *(.sbss*) *(COMMON) . = ALIGN(4); _bss_end = .; } > RAM
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
