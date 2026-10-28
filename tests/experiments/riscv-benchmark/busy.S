# Copyright (c) 2026 Ricardo Kerschbaumer
# SPDX-License-Identifier: MIT
#
# Busy loop for the RV32IMC core (E-08, gate G8a.3): a counter and a branch,
# as firmware polling a flag; a dot on the UART every 2^24 iterations.
    .section .text.start
    .globl _start
_start:
    li t0, 0x10000000
    li t1, 0
loop:
    addi t1, t1, 1
    slli t2, t1, 8
    bnez t2, loop
    li t3, '.'
    sb t3, 0(t0)
    j loop
