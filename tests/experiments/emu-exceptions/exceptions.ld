/* Copyright (c) 2026 Ricardo Kerschbaumer; SPDX-License-Identifier: MIT */
ENTRY(reset)
MEMORY { FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K }
SECTIONS { .vectors : { KEEP(*(.vectors)) } > FLASH .text : { *(.text*) } > FLASH }
