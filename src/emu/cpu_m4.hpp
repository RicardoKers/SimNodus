// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// What the Cortex-M4F adds to the Cortex-M3 core (experiment E-12, SN-293):
// the DSP extension of ARMv7E-M and the single-precision FPU (FPv4-SP-D16).
// The decoder is shared: these encodings decode on every core, and a core
// without the extension answers them as it always did (an undefined
// instruction, or no coprocessor). Internal to src/emu.
#pragma once

#include "cpu.hpp"

#include <cstdint>

namespace simnodus::emu::m4 {

enum class Kind { None, Dsp, Vfp };

// Decodes a 32-bit encoding of the DSP extension or of coprocessors 10 and 11
// into `in` (Insn::alu holds the operation below); None for anything else.
Kind decode(std::uint32_t h1, std::uint32_t h2, Insn& in);

// Cycles of an instruction (Cortex-M4 Technical Reference Manual, tables 3-1
// and 7-1); `registers`: how many a VLDM, VSTM, VPUSH or VPOP moves.
unsigned cycles(const Insn& in, bool dsp);

// DSP operations (Insn::alu when the kind is Dsp).
enum Dsp : std::uint8_t {
    QADD, QDADD, QSUB, QDSUB,
    PARALLEL, // mode: prefix (low nibble) and operation (high nibble)
    SEL,
    EXTEND_ADD, // mode: which; rn 15: no addition
    PKH,        // mode: 1 for PKHTB
    SSAT16, USAT16,
    USAD8, // ra 15: no accumulate
    SMULXY, // mode bit 1: top half of Rn, bit 0: top half of Rm; ra 15: no accumulate
    SMULWY, // mode bit 0: top half of Rm; ra 15: no accumulate
    SMUAD,  // mode bit 0: swap Rm's halves, bit 1: subtract (SMUSD); ra 15: no accumulate
    SMMUL,  // mode bit 0: round, bit 1: subtract (SMMLS); ra 15: no accumulate
    SMLALXY, // rd low, ra high; mode as SMULXY
    SMLALD,  // mode as SMUAD
    UMAAL,
};

// Parallel add and subtract: Insn::mode = prefix | operation << 4.
enum ParallelPrefix : std::uint8_t { P_S, P_Q, P_SH, P_U, P_UQ, P_UH };
enum ParallelOp : std::uint8_t { P_ADD16, P_ASX, P_SAX, P_SUB16, P_ADD8, P_SUB8 };

// Extend and add: Insn::mode.
enum ExtendOp : std::uint8_t { X_SXTAH, X_UXTAH, X_SXTAB16, X_UXTAB16, X_SXTAB, X_UXTAB };

// FPU operations (Insn::alu when the kind is Vfp). Registers are numbers of
// single-precision registers (S0..S31) unless said otherwise.
enum Vfp : std::uint8_t {
    V_UNDEFINED, // an encoding the single-precision FPU does not have (double precision)
    V_MLA, V_MLS, V_NMLS, V_NMLA, V_MUL, V_NMUL, V_ADD, V_SUB, V_DIV,
    V_FNMS, V_FNMA, V_FMA, V_FMS,
    V_MOV_IMM, V_MOV, V_ABS, V_NEG, V_SQRT,
    V_CVT_FROM_HALF, V_CVT_TO_HALF, // mode: 1 for the top half of the register
    V_CMP,      // mode bit 0: signal every NaN (VCMPE), bit 1: with zero
    V_CVT_FROM_INT, // mode bit 0: unsigned
    V_CVT_TO_INT,   // mode bit 0: unsigned, bit 1: toward zero
    V_CVT_FIXED,    // mode bit 0: unsigned, bit 1: to fixed point, bit 2: 32 bits; imm: fraction bits
    V_MOV_CORE,     // mode bit 0: to the core register rd; rn: the single register
    V_MSR, V_MRS,   // rd: the core register (15 for VMRS: the APSR flags)
    V_MOV_SCALAR,   // rn: the word (S register number) of a D register; mode bit 0: to the core register
    V_MOV_PAIR,     // rd, ra: core registers; rm: the first of two S registers; mode bit 0: to the core registers
    V_LDR, V_STR,   // mode bit 0: a D register (two words), bit 1: add the offset; imm: offset
    V_LDM, V_STM,   // imm: words; mode bit 1: add (increment after), bit 2: write back
};

} // namespace simnodus::emu::m4
