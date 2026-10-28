// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Target macros of riscv-arch-test 2.7.4 for SimNodus's RISC-V test machine
// (src/emu/riscv_test_machine.hpp), which has QEMU virt's addresses: the
// signature lies between begin_signature and end_signature, and the test
// ends by writing the test finisher. Nothing comes between the test's data
// and the signature: the trap signatures of the misaligned-access tests hold
// addresses relative to it, as the reference signatures were recorded.
#ifndef SIMNODUS_MODEL_TEST_H
#define SIMNODUS_MODEL_TEST_H

#define RVMODEL_HALT \
    li t0, 0x100000; \
    li t1, 0x5555; \
    sw t1, 0(t0); \
1:  j 1b;

#define RVMODEL_BOOT

#define RVMODEL_DATA_BEGIN \
    .align 4; .global begin_signature; begin_signature:

#define RVMODEL_DATA_END \
    .align 4; .global end_signature; end_signature:

#define RVMODEL_IO_INIT
#define RVMODEL_IO_WRITE_STR(_R, _STR)
#define RVMODEL_IO_CHECK()
#define RVMODEL_IO_ASSERT_GPR_EQ(_S, _R, _I)
#define RVMODEL_IO_ASSERT_SFPR_EQ(_F, _R, _I)
#define RVMODEL_IO_ASSERT_DFPR_EQ(_D, _R, _I)

// The CLINT's software interrupt (msip at 0x02000000).
#define RVMODEL_SET_MSW_INT \
    li t1, 1; \
    li t2, 0x2000000; \
    sw t1, 0(t2);
#define RVMODEL_CLEAR_MSW_INT \
    li t2, 0x2000000; \
    sw x0, 0(t2);
#define RVMODEL_CLEAR_MTIMER_INT
#define RVMODEL_CLEAR_MEXT_INT

#endif
