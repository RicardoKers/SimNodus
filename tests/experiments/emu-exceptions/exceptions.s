@ Copyright (c) 2026 Ricardo Kerschbaumer
@ SPDX-License-Identifier: MIT
@
@ Exception-model scenarios for the SimNodus emulator (E-07, gate G0a.1). The
@ test writes a scenario number to SCENARIO, resets the core and runs; the
@ program logs handler entries and exits (and some register values) to LOG
@ and writes 0xD1FF to DONE. Expected logs are worked out from the ARMv7-M
@ manual (B1.5) in tests/unit/emu_tests.cpp.
@
@ Log codes: 0xA0+n IRQn entry, 0xB0+n IRQn exit, 0x5C SVC entry, 0x4E NMI,
@ 0xFF HardFault (then CFSR and HFSR), 0xC0.. thread markers, 0xEE thread end.

    .syntax unified
    .cpu cortex-m3
    .thumb

    .equ LOG_INDEX, 0x200000F0
    .equ SCENARIO,  0x200000F4
    .equ DONE,      0x200000F8
    .equ ACTION,    0x20000080      @ 8 words: ISPR value an IRQ handler writes
    .equ LOG,       0x20000100
    .equ ISER,      0xE000E100
    .equ ISPR,      0xE000E200
    .equ IPR,       0xE000E400
    .equ ICSR,      0xE000ED04
    .equ AIRCR,     0xE000ED0C
    .equ CCR,       0xE000ED14
    .equ CFSR,      0xE000ED28
    .equ HFSR,      0xE000ED2C

    .section .vectors, "a"
    .word 0x20004000, reset + 1, nmi + 1, hardfault + 1
    .word hardfault + 1, hardfault + 1, hardfault + 1, 0
    .word 0, 0, 0, svc + 1
    .word 0, 0, 0, 0
    .word irq0 + 1, irq1 + 1, irq2 + 1, irq3 + 1, irq4 + 1, irq5 + 1, irq6 + 1, irq7 + 1

    .text

@ log rX: appends a register to LOG (clobbers r0-r2 except the value register).
    .macro LOGR reg
    ldr r1, =LOG_INDEX
    ldr r2, [r1]
    push {r2}
    ldr r1, =LOG
    str \reg, [r1, r2, lsl #2]
    pop {r2}
    adds r2, #1
    ldr r1, =LOG_INDEX
    str r2, [r1]
    .endm

    .macro LOGI value
    ldr r0, =\value
    LOGR r0
    .endm

    .macro IRQ n
    .thumb_func
irq\n:
    LOGI (0xA0 + \n)
    ldr r0, =(ACTION + 4 * \n)
    ldr r0, [r0]
    cbz r0, 1f
    ldr r1, =ISPR
    str r0, [r1]
    isb
1:
    .if \n == 6
    mov r3, lr                      @ EXC_RETURN
    LOGR r3
    .endif
    .if \n == 7
    mov r3, sp                      @ frame address
    LOGR r3
    ldr r3, [sp, #28]               @ stacked xPSR: bit 9 = alignment padding
    and r3, r3, #0x200
    LOGR r3
    .endif
    LOGI (0xB0 + \n)
    bx lr
    .ltorg
    .endm

    IRQ 0
    IRQ 1
    IRQ 2
    IRQ 3
    IRQ 4
    IRQ 5
    IRQ 6
    IRQ 7

    .thumb_func
nmi:
    LOGI 0x4E
    bx lr

    .thumb_func
svc:
    LOGI 0x5C
    ldr r0, =SCENARIO
    ldr r0, [r0]
    cmp r0, #7
    bne 1f
    ldr r0, =0xFFFFFFF5             @ invalid EXC_RETURN: INVPC
    bx r0
1:  bx lr

    .thumb_func
hardfault:
    LOGI 0xFF
    ldr r3, =CFSR
    ldr r3, [r3]
    LOGR r3
    ldr r3, =HFSR
    ldr r3, [r3]
    LOGR r3
    b finish
    .ltorg

@ Sets the priority byte of IRQ n.
    .macro PRIO n, value
    ldr r0, =(IPR + \n)
    movs r1, #\value
    strb r1, [r0]
    .endm

    .macro PEND mask
    ldr r0, =ISPR
    ldr r1, =\mask
    str r1, [r0]
    isb
    .endm

    .global reset
    .thumb_func
reset:
    ldr r0, =ISER
    movs r1, #0xFF                  @ enable IRQ0-7
    str r1, [r0]
    ldr r0, =SCENARIO
    ldr r0, [r0]
    tbb [pc, r0]
table:
    .byte (s0 - table) / 2, (s1 - table) / 2, (s2 - table) / 2, (s3 - table) / 2, (s4 - table) / 2
    .byte (s5 - table) / 2, (s6 - table) / 2, (s7 - table) / 2, (s8 - table) / 2, (s9 - table) / 2
    .byte (s10 - table) / 2
    .align 1

s0:                                 @ nothing: thread end only
    b end_ok

s1:                                 @ preemption by a higher priority
    PRIO 0, 0x80
    PRIO 1, 0x40
    ldr r0, =ACTION
    movs r1, #2                     @ IRQ0 pends IRQ1
    str r1, [r0]
    PEND 1
    b end_ok

s2:                                 @ same group (PRIGROUP 5): no preemption, tail-chain
    ldr r0, =AIRCR
    ldr r1, =0x05FA0500
    str r1, [r0]
    PRIO 0, 0x90
    PRIO 1, 0x80
    ldr r0, =ACTION
    movs r1, #2
    str r1, [r0]
    PEND 1
    b end_ok

s3:                                 @ BASEPRI masks an equal priority
    PRIO 0, 0x80
    movs r0, #0x80
    msr basepri, r0
    PEND 1
    LOGI 0xC0
    movs r0, #0
    msr basepri, r0
    isb
    LOGI 0xC1
    b end_ok

s4:                                 @ pending order: priority, then exception number
    PRIO 2, 0x40
    PRIO 3, 0x20
    PRIO 4, 0x60
    PRIO 5, 0x60
    cpsid i
    PEND 0x3C                       @ IRQ2-5
    cpsie i
    isb
    b end_ok

s5:                                 @ (reserved)
    b end_ok

s6:                                 @ thread on PSP: EXC_RETURN 0xFFFFFFFD
    ldr r0, =0x20003000
    msr psp, r0
    movs r0, #2
    msr control, r0
    isb
    PEND 0x40                       @ IRQ6
    mov r3, sp
    LOGR r3
    b end_ok

s7:                                 @ invalid EXC_RETURN from SVC -> UsageFault (INVPC) -> HardFault
    svc #0
    b end_ok

s8:                                 @ 8-byte stack alignment (STKALIGN) on an SP = 4 mod 8
    ldr r0, =CCR
    ldr r1, =0x200
    str r1, [r0]
    ldr r0, =0x20003FFC
    mov sp, r0
    PEND 0x80                       @ IRQ7
    mov r3, sp
    LOGR r3
    b end_ok

s9:                                 @ FAULTMASK blocks IRQs, not NMI
    cpsid f
    PEND 1
    ldr r0, =ICSR
    ldr r1, =0x80000000             @ NMIPENDSET
    str r1, [r0]
    isb
    LOGI 0xC8
    cpsie f
    isb
    LOGI 0xC9
    b end_ok

s10:                                @ WFI wakes on a masked pending IRQ; then SVC under PRIMASK escalates
    cpsid i
    PEND 1
    wfi
    LOGI 0xD0
    cpsie i
    isb
    LOGI 0xD1
    cpsid i
    svc #0                          @ cannot be taken: HardFault (FORCED)
    b end_ok

end_ok:
    LOGI 0xEE
finish:
    ldr r0, =DONE
    ldr r1, =0xD1FF
    str r1, [r0]
    b .
    .ltorg
