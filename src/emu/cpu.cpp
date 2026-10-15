// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Cortex-M3 core. Section numbers refer to the ARMv7-M Architecture Reference
// Manual (DDI 0403E): A5.2 (16-bit encodings), A5.3 (32-bit encodings),
// A7 (instruction details), B1.5 (exception model), B3 (system address map).
#include "cpu.hpp"

#include <algorithm>
#include <cstdio>
#include <limits>

namespace simnodus::emu {

namespace {

enum Op : std::uint8_t {
    OP_NONE = 0,
    OP_UNDEF, OP_NOCP,
    OP_DP_IMM, OP_DP_REG, OP_DP_RSR, OP_ADR, OP_MOVW, OP_MOVT,
    OP_MUL, OP_MLA, OP_MLS, OP_SMULL, OP_UMULL, OP_SMLAL, OP_UMLAL, OP_SDIV, OP_UDIV,
    OP_CLZ, OP_RBIT, OP_REV, OP_REV16, OP_REVSH, OP_SXTB, OP_SXTH, OP_UXTB, OP_UXTH,
    OP_SSAT, OP_USAT, OP_SBFX, OP_UBFX, OP_BFI, OP_BFC,
    OP_LDR, OP_STR, OP_LDRD, OP_STRD, OP_LDREX, OP_STREX, OP_CLREX,
    OP_LDM, OP_STM, OP_TBB, OP_TBH,
    OP_B, OP_BCOND, OP_BL, OP_BX, OP_BLX, OP_CBZ, OP_CBNZ, OP_IT,
    OP_MRS, OP_MSR, OP_CPS, OP_NOP, OP_YIELD, OP_WFE, OP_WFI, OP_SEV, OP_BARRIER, OP_SVC, OP_BKPT, OP_UDF,
    OP_COUNT
};

const char* const op_names[OP_COUNT] = {
    "none", "undefined", "coprocessor",
    "data-processing immediate", "data-processing register", "shift by register", "ADR", "MOVW", "MOVT",
    "MUL", "MLA", "MLS", "SMULL", "UMULL", "SMLAL", "UMLAL", "SDIV", "UDIV",
    "CLZ", "RBIT", "REV", "REV16", "REVSH", "SXTB", "SXTH", "UXTB", "UXTH",
    "SSAT", "USAT", "SBFX", "UBFX", "BFI", "BFC",
    "load", "store", "LDRD", "STRD", "LDREX", "STREX", "CLREX",
    "LDM", "STM", "TBB", "TBH",
    "B", "B<cond>", "BL", "BX", "BLX", "CBZ", "CBNZ", "IT",
    "MRS", "MSR", "CPS", "NOP", "YIELD", "WFE", "WFI", "SEV", "barrier", "SVC", "BKPT", "UDF",
};

// Data-processing operations (Insn::alu).
enum Alu : std::uint8_t { AND, EOR, ORR, ORN, BIC, MOV, MVN, TST, TEQ, ADD, ADC, SUB, SBC, RSB, CMP, CMN };

// Shift types (Insn::shift_t).
enum Shift : std::uint8_t { LSL, LSR, ASR, ROR, RRX };

// Memory access flags (Insn::mode): bits 0..2 size in bytes.
constexpr std::uint8_t M_SIGNED = 0x08;
constexpr std::uint8_t M_REG = 0x10; // offset from Rm << shift_n
constexpr std::uint8_t M_P = 0x20;   // index (offset applies to the address)
constexpr std::uint8_t M_U = 0x40;   // add the offset
constexpr std::uint8_t M_W = 0x80;   // write the offset address back to Rn

// LDM/STM flags.
constexpr std::uint8_t LM_DB = 0x01;
constexpr std::uint8_t LM_WB = 0x02;

// Fault status bits (B3.2.15).
constexpr std::uint32_t MM_IACCVIOL = 1u << 0;
constexpr std::uint32_t BF_IBUSERR = 1u << 8;
constexpr std::uint32_t BF_PRECISERR = 1u << 9;
constexpr std::uint32_t BF_BFARVALID = 1u << 15;
constexpr std::uint32_t UF_UNDEFINSTR = 1u << 16;
constexpr std::uint32_t UF_INVSTATE = 1u << 17;
constexpr std::uint32_t UF_INVPC = 1u << 18;
constexpr std::uint32_t UF_NOCP = 1u << 19;
constexpr std::uint32_t UF_UNALIGNED = 1u << 24;
constexpr std::uint32_t UF_DIVBYZERO = 1u << 25;
constexpr std::uint32_t HF_VECTTBL = 1u << 1;
constexpr std::uint32_t HF_FORCED = 1u << 30;
constexpr std::uint32_t HF_DEBUGEVT = 1u << 31;

// Exception numbers.
constexpr int EX_RESET = 1, EX_NMI = 2, EX_HARDFAULT = 3, EX_MEMMANAGE = 4, EX_BUSFAULT = 5, EX_USAGEFAULT = 6,
              EX_SVCALL = 11, EX_PENDSV = 14, EX_SYSTICK = 15;

constexpr std::uint32_t flash_base = 0x08000000;
constexpr std::uint32_t cache_bytes = 128 * 1024;
constexpr std::uint64_t never = std::numeric_limits<std::uint64_t>::max();
constexpr std::uint32_t irq_count = static_cast<std::uint32_t>(Cpu::num_irqs);

inline std::uint32_t bits(std::uint32_t x, unsigned hi, unsigned lo)
{
    return (x >> lo) & ((2u << (hi - lo)) - 1u);
}

inline std::uint32_t sign_extend(std::uint32_t x, unsigned width)
{
    const std::uint32_t m = 1u << (width - 1);
    x &= (width == 32) ? 0xFFFFFFFFu : ((1u << width) - 1u);
    return (x ^ m) - m;
}

inline std::uint32_t ror(std::uint32_t x, unsigned n)
{
    n &= 31u;
    return n == 0 ? x : (x >> n) | (x << (32 - n));
}

inline std::uint8_t u8(std::uint32_t x) { return static_cast<std::uint8_t>(x); }

inline unsigned popcount(std::uint32_t x)
{
    unsigned count = 0;
    for (; x != 0; x &= x - 1) {
        ++count;
    }
    return count;
}

struct Shifted {
    std::uint32_t value;
    bool carry;
};

// Shift_C (A7.4.2) with the amount already decoded (0 = no shift).
Shifted shift_c(std::uint32_t x, unsigned type, unsigned n, bool carry_in)
{
    if (type == RRX) {
        return {(carry_in ? 0x80000000u : 0u) | (x >> 1), (x & 1u) != 0};
    }
    if (n == 0) {
        return {x, carry_in};
    }
    switch (type) {
    case LSL:
        if (n < 32) {
            return {x << n, ((x >> (32 - n)) & 1u) != 0};
        }
        return {0, n == 32 ? (x & 1u) != 0 : false};
    case LSR:
        if (n < 32) {
            return {x >> n, ((x >> (n - 1)) & 1u) != 0};
        }
        return {0, n == 32 ? (x >> 31) != 0 : false};
    case ASR:
        if (n < 32) {
            return {static_cast<std::uint32_t>(static_cast<std::int32_t>(x) >> n), ((x >> (n - 1)) & 1u) != 0};
        }
        return {(x & 0x80000000u) ? 0xFFFFFFFFu : 0u, (x >> 31) != 0};
    default: { // ROR
        const std::uint32_t result = ror(x, n);
        return {result, (result >> 31) != 0};
    }
    }
}

struct Sum {
    std::uint32_t value;
    bool carry;
    bool overflow;
};

inline Sum add_with_carry(std::uint32_t x, std::uint32_t y, bool carry_in)
{
    const std::uint64_t unsigned_sum = std::uint64_t{x} + y + (carry_in ? 1u : 0u);
    const std::int64_t signed_sum = std::int64_t{static_cast<std::int32_t>(x)} + static_cast<std::int32_t>(y)
        + (carry_in ? 1 : 0);
    const std::uint32_t result = static_cast<std::uint32_t>(unsigned_sum);
    return {result, (unsigned_sum >> 32) != 0, signed_sum != static_cast<std::int32_t>(result)};
}

// ThumbExpandImm_C (A5.3.2): value and carry (-1 = carry unchanged).
void expand_imm(std::uint32_t imm12, Insn& in)
{
    if ((imm12 >> 10) == 0) {
        const std::uint32_t b = imm12 & 0xFFu;
        switch ((imm12 >> 8) & 3u) {
        case 0: in.imm = b; break;
        case 1: in.imm = (b << 16) | b; break;
        case 2: in.imm = (b << 24) | (b << 8); break;
        default: in.imm = (b << 24) | (b << 16) | (b << 8) | b; break;
        }
        in.carry = -1;
    } else {
        const std::uint32_t unrotated = 0x80u | (imm12 & 0x7Fu);
        in.imm = ror(unrotated, (imm12 >> 7) & 31u);
        in.carry = static_cast<std::int8_t>(in.imm >> 31);
    }
}

// DecodeImmShift (A7.4.2).
void imm_shift(std::uint32_t type, std::uint32_t imm5, Insn& in)
{
    switch (type) {
    case 0: in.shift_t = LSL; in.shift_n = u8(imm5); break;
    case 1: in.shift_t = LSR; in.shift_n = u8(imm5 == 0 ? 32 : imm5); break;
    case 2: in.shift_t = ASR; in.shift_n = u8(imm5 == 0 ? 32 : imm5); break;
    default:
        if (imm5 == 0) {
            in.shift_t = RRX;
            in.shift_n = 1;
        } else {
            in.shift_t = ROR;
            in.shift_n = u8(imm5);
        }
        break;
    }
}

Insn undefined(unsigned size)
{
    Insn in;
    in.op = OP_UNDEF;
    in.size = u8(size);
    return in;
}

Insn memory(bool load, unsigned size, bool is_signed, unsigned rt, unsigned rn, std::uint32_t offset, bool p, bool u,
    bool w)
{
    Insn in;
    in.op = load ? OP_LDR : OP_STR;
    in.rd = u8(rt);
    in.rn = u8(rn);
    in.imm = offset;
    in.mode = u8(size | (is_signed ? M_SIGNED : 0) | (p ? M_P : 0) | (u ? M_U : 0) | (w ? M_W : 0));
    return in;
}

Insn memory_reg(bool load, unsigned size, bool is_signed, unsigned rt, unsigned rn, unsigned rm, unsigned shift)
{
    Insn in = memory(load, size, is_signed, rt, rn, 0, true, true, false);
    in.mode = u8(in.mode | M_REG);
    in.rm = u8(rm);
    in.shift_n = u8(shift);
    return in;
}

Insn data(std::uint8_t op, std::uint8_t alu, unsigned rd, unsigned rn, std::uint8_t s)
{
    Insn in;
    in.op = op;
    in.alu = alu;
    in.rd = u8(rd);
    in.rn = u8(rn);
    in.s = s;
    return in;
}

// A5.2: 16-bit Thumb instructions.
Insn decode16(std::uint32_t h)
{
    Insn in;
    switch (h >> 12) {
    case 0x0:
    case 0x1: {
        const std::uint32_t op = (h >> 11) & 3u;
        if (op < 3) { // LSL/LSR/ASR (immediate) = MOV with a shifted register
            in = data(OP_DP_REG, MOV, h & 7u, 0, 2);
            in.rm = u8((h >> 3) & 7u);
            imm_shift(op, (h >> 6) & 31u, in);
        } else {
            const bool sub = (h & 0x200u) != 0;
            if (h & 0x400u) {
                in = data(OP_DP_IMM, sub ? SUB : ADD, h & 7u, (h >> 3) & 7u, 2);
                in.imm = (h >> 6) & 7u;
            } else {
                in = data(OP_DP_REG, sub ? SUB : ADD, h & 7u, (h >> 3) & 7u, 2);
                in.rm = u8((h >> 6) & 7u);
            }
        }
        break;
    }
    case 0x2:
    case 0x3: {
        static const std::uint8_t ops[4] = {MOV, CMP, ADD, SUB};
        const std::uint32_t op = (h >> 11) & 3u;
        const std::uint32_t rdn = (h >> 8) & 7u;
        in = data(OP_DP_IMM, ops[op], rdn, rdn, op == 1 ? 1 : 2);
        in.imm = h & 0xFFu;
        break;
    }
    case 0x4:
        if ((h >> 10) == 0x10) { // A5.2.2 data processing
            const std::uint32_t rdn = h & 7u;
            const std::uint32_t rm = (h >> 3) & 7u;
            switch ((h >> 6) & 15u) {
            case 0: in = data(OP_DP_REG, AND, rdn, rdn, 2); in.rm = u8(rm); break;
            case 1: in = data(OP_DP_REG, EOR, rdn, rdn, 2); in.rm = u8(rm); break;
            case 2: in = data(OP_DP_RSR, MOV, rdn, 0, 2); in.rm = u8(rdn); in.ra = u8(rm); in.shift_t = LSL; break;
            case 3: in = data(OP_DP_RSR, MOV, rdn, 0, 2); in.rm = u8(rdn); in.ra = u8(rm); in.shift_t = LSR; break;
            case 4: in = data(OP_DP_RSR, MOV, rdn, 0, 2); in.rm = u8(rdn); in.ra = u8(rm); in.shift_t = ASR; break;
            case 5: in = data(OP_DP_REG, ADC, rdn, rdn, 2); in.rm = u8(rm); break;
            case 6: in = data(OP_DP_REG, SBC, rdn, rdn, 2); in.rm = u8(rm); break;
            case 7: in = data(OP_DP_RSR, MOV, rdn, 0, 2); in.rm = u8(rdn); in.ra = u8(rm); in.shift_t = ROR; break;
            case 8: in = data(OP_DP_REG, TST, 0, rdn, 1); in.rm = u8(rm); break;
            case 9: in = data(OP_DP_IMM, RSB, rdn, rm, 2); in.imm = 0; break;
            case 10: in = data(OP_DP_REG, CMP, 0, rdn, 1); in.rm = u8(rm); break;
            case 11: in = data(OP_DP_REG, CMN, 0, rdn, 1); in.rm = u8(rm); break;
            case 12: in = data(OP_DP_REG, ORR, rdn, rdn, 2); in.rm = u8(rm); break;
            case 13: in.op = OP_MUL; in.rd = u8(rdn); in.rn = u8(rm); in.rm = u8(rdn); in.s = 2; break;
            case 14: in = data(OP_DP_REG, BIC, rdn, rdn, 2); in.rm = u8(rm); break;
            default: in = data(OP_DP_REG, MVN, rdn, 0, 2); in.rm = u8(rm); break;
            }
        } else if ((h >> 10) == 0x11) { // A5.2.3 special data and branch exchange
            const std::uint32_t rdn = (h & 7u) | ((h >> 4) & 8u);
            const std::uint32_t rm = (h >> 3) & 15u;
            switch ((h >> 8) & 3u) {
            case 0: in = data(OP_DP_REG, ADD, rdn, rdn, 0); in.rm = u8(rm); break;
            case 1: in = data(OP_DP_REG, CMP, 0, rdn, 1); in.rm = u8(rm); break;
            case 2: in = data(OP_DP_REG, MOV, rdn, 0, 0); in.rm = u8(rm); break;
            default:
                in.op = (h & 0x80u) ? OP_BLX : OP_BX;
                in.rm = u8(rm);
                break;
            }
        } else { // 01001: LDR (literal)
            in = memory(true, 4, false, (h >> 8) & 7u, 15, (h & 0xFFu) * 4u, true, true, false);
        }
        break;
    case 0x5: { // A5.2.4 register offset
        static const unsigned size[8] = {4, 2, 1, 1, 4, 2, 1, 2};
        static const bool load[8] = {false, false, false, true, true, true, true, true};
        static const bool sign[8] = {false, false, false, true, false, false, false, true};
        const std::uint32_t op = (h >> 9) & 7u;
        in = memory_reg(load[op], size[op], sign[op], h & 7u, (h >> 3) & 7u, (h >> 6) & 7u, 0);
        break;
    }
    case 0x6:
        in = memory((h & 0x800u) != 0, 4, false, h & 7u, (h >> 3) & 7u, ((h >> 6) & 31u) * 4u, true, true, false);
        break;
    case 0x7:
        in = memory((h & 0x800u) != 0, 1, false, h & 7u, (h >> 3) & 7u, (h >> 6) & 31u, true, true, false);
        break;
    case 0x8:
        in = memory((h & 0x800u) != 0, 2, false, h & 7u, (h >> 3) & 7u, ((h >> 6) & 31u) * 2u, true, true, false);
        break;
    case 0x9:
        in = memory((h & 0x800u) != 0, 4, false, (h >> 8) & 7u, 13, (h & 0xFFu) * 4u, true, true, false);
        break;
    case 0xA:
        if (h & 0x800u) {
            in = data(OP_DP_IMM, ADD, (h >> 8) & 7u, 13, 0);
        } else {
            in.op = OP_ADR;
            in.rd = u8((h >> 8) & 7u);
            in.mode = 1; // add
        }
        in.imm = (h & 0xFFu) * 4u;
        break;
    case 0xB: // A5.2.5 miscellaneous
        if ((h & 0xFF00u) == 0xB000u) {
            in = data(OP_DP_IMM, (h & 0x80u) ? SUB : ADD, 13, 13, 0);
            in.imm = (h & 0x7Fu) * 4u;
        } else if ((h & 0xF500u) == 0xB100u) {
            in.op = (h & 0x800u) ? OP_CBNZ : OP_CBZ;
            in.rn = u8(h & 7u);
            in.imm = (((h >> 9) & 1u) << 6) | (((h >> 3) & 31u) << 1);
        } else if ((h & 0xFF00u) == 0xB200u) {
            static const std::uint8_t ops[4] = {OP_SXTH, OP_SXTB, OP_UXTH, OP_UXTB};
            in.op = ops[(h >> 6) & 3u];
            in.rd = u8(h & 7u);
            in.rm = u8((h >> 3) & 7u);
        } else if ((h & 0xFE00u) == 0xB400u) { // PUSH = STMDB SP!
            in.op = OP_STM;
            in.rn = 13;
            in.imm = (h & 0xFFu) | ((h & 0x100u) << 6);
            in.mode = LM_DB | LM_WB;
        } else if ((h & 0xFFE8u) == 0xB660u) {
            in.op = OP_CPS;
            in.mode = u8(h & 0x1Fu); // im (bit 4), I (bit 1), F (bit 0)
        } else if ((h & 0xFF00u) == 0xBA00u) {
            static const std::uint8_t ops[4] = {OP_REV, OP_REV16, OP_UNDEF, OP_REVSH};
            in.op = ops[(h >> 6) & 3u];
            in.rd = u8(h & 7u);
            in.rm = u8((h >> 3) & 7u);
        } else if ((h & 0xFE00u) == 0xBC00u) { // POP = LDMIA SP!
            in.op = OP_LDM;
            in.rn = 13;
            in.imm = (h & 0xFFu) | ((h & 0x100u) << 7);
            in.mode = LM_WB;
        } else if ((h & 0xFF00u) == 0xBE00u) {
            in.op = OP_BKPT;
            in.imm = h & 0xFFu;
        } else if ((h & 0xFF00u) == 0xBF00u) {
            if (h & 0xFu) {
                in.op = OP_IT;
                in.imm = h & 0xFFu;
            } else {
                static const std::uint8_t hints[5] = {OP_NOP, OP_YIELD, OP_WFE, OP_WFI, OP_SEV};
                const std::uint32_t hint = (h >> 4) & 15u;
                in.op = hint < 5 ? hints[hint] : OP_NOP;
            }
        } else {
            in = undefined(2);
        }
        break;
    case 0xC: {
        const std::uint32_t rn = (h >> 8) & 7u;
        const std::uint32_t list = h & 0xFFu;
        if (h & 0x800u) {
            in.op = OP_LDM;
            in.mode = (list & (1u << rn)) ? 0 : LM_WB;
        } else {
            in.op = OP_STM;
            in.mode = LM_WB;
        }
        in.rn = u8(rn);
        in.imm = list;
        break;
    }
    case 0xD: {
        const std::uint32_t cond = (h >> 8) & 15u;
        if (cond == 14) {
            in.op = OP_UDF;
        } else if (cond == 15) {
            in.op = OP_SVC;
            in.imm = h & 0xFFu;
        } else {
            in.op = OP_BCOND;
            in.alu = u8(cond);
            in.imm = sign_extend((h & 0xFFu) << 1, 9);
        }
        break;
    }
    default: // 0xE, 11100: B
        in.op = OP_B;
        in.imm = sign_extend((h & 0x7FFu) << 1, 12);
        break;
    }
    in.size = 2;
    return in;
}

// A5.3.1 data processing (modified immediate).
Insn decode_modified_immediate(std::uint32_t h1, std::uint32_t h2)
{
    const std::uint32_t op = bits(h1, 8, 5);
    const std::uint32_t s = bits(h1, 4, 4);
    const std::uint32_t rn = bits(h1, 3, 0);
    const std::uint32_t rd = bits(h2, 11, 8);
    const std::uint32_t imm12 = (bits(h1, 10, 10) << 11) | (bits(h2, 14, 12) << 8) | bits(h2, 7, 0);
    Insn in;
    const std::uint8_t sf = u8(s);
    switch (op) {
    case 0: in = (rd == 15 && s) ? data(OP_DP_IMM, TST, 0, rn, 1) : data(OP_DP_IMM, AND, rd, rn, sf); break;
    case 1: in = data(OP_DP_IMM, BIC, rd, rn, sf); break;
    case 2: in = rn == 15 ? data(OP_DP_IMM, MOV, rd, 0, sf) : data(OP_DP_IMM, ORR, rd, rn, sf); break;
    case 3: in = rn == 15 ? data(OP_DP_IMM, MVN, rd, 0, sf) : data(OP_DP_IMM, ORN, rd, rn, sf); break;
    case 4: in = (rd == 15 && s) ? data(OP_DP_IMM, TEQ, 0, rn, 1) : data(OP_DP_IMM, EOR, rd, rn, sf); break;
    case 8: in = (rd == 15 && s) ? data(OP_DP_IMM, CMN, 0, rn, 1) : data(OP_DP_IMM, ADD, rd, rn, sf); break;
    case 10: in = data(OP_DP_IMM, ADC, rd, rn, sf); break;
    case 11: in = data(OP_DP_IMM, SBC, rd, rn, sf); break;
    case 13: in = (rd == 15 && s) ? data(OP_DP_IMM, CMP, 0, rn, 1) : data(OP_DP_IMM, SUB, rd, rn, sf); break;
    case 14: in = data(OP_DP_IMM, RSB, rd, rn, sf); break;
    default: return undefined(4);
    }
    expand_imm(imm12, in);
    return in;
}

// A5.3.3 data processing (plain binary immediate).
Insn decode_plain_immediate(std::uint32_t h1, std::uint32_t h2)
{
    const std::uint32_t op = bits(h1, 8, 4);
    const std::uint32_t rn = bits(h1, 3, 0);
    const std::uint32_t rd = bits(h2, 11, 8);
    const std::uint32_t imm12 = (bits(h1, 10, 10) << 11) | (bits(h2, 14, 12) << 8) | bits(h2, 7, 0);
    const std::uint32_t imm5 = (bits(h2, 14, 12) << 2) | bits(h2, 7, 6);
    const std::uint32_t low5 = bits(h2, 4, 0);
    Insn in;
    in.rd = u8(rd);
    in.rn = u8(rn);
    switch (op) {
    case 0: // ADDW, ADR (add)
    case 10: // SUBW, ADR (sub)
        if (rn == 15) {
            in.op = OP_ADR;
            in.mode = op == 0 ? 1 : 0;
        } else {
            in = data(OP_DP_IMM, op == 0 ? ADD : SUB, rd, rn, 0);
            in.carry = -1;
        }
        in.imm = imm12;
        break;
    case 4:
    case 12:
        in.op = op == 4 ? OP_MOVW : OP_MOVT;
        in.imm = (rn << 12) | imm12;
        break;
    case 16:
    case 18:
        if (op == 18 && imm5 == 0) {
            return undefined(4); // SSAT16 (DSP extension)
        }
        in.op = OP_SSAT;
        in.imm = low5 + 1;
        in.shift_t = op == 18 ? ASR : LSL;
        in.shift_n = u8(imm5);
        break;
    case 24:
    case 26:
        if (op == 26 && imm5 == 0) {
            return undefined(4); // USAT16
        }
        in.op = OP_USAT;
        in.imm = low5;
        in.shift_t = op == 26 ? ASR : LSL;
        in.shift_n = u8(imm5);
        break;
    case 20:
    case 28:
        in.op = op == 20 ? OP_SBFX : OP_UBFX;
        in.shift_n = u8(imm5); // lsb
        in.imm = low5;         // width - 1
        break;
    case 22:
        in.op = rn == 15 ? OP_BFC : OP_BFI;
        in.shift_n = u8(imm5); // lsb
        in.imm = low5;         // msb
        break;
    default:
        return undefined(4);
    }
    return in;
}

// A5.3.4 branches and miscellaneous control.
Insn decode_branch_misc(std::uint32_t h1, std::uint32_t h2)
{
    const std::uint32_t op1 = bits(h2, 14, 12);
    const std::uint32_t s = bits(h1, 10, 10);
    const std::uint32_t j1 = bits(h2, 13, 13);
    const std::uint32_t j2 = bits(h2, 11, 11);
    Insn in;
    if ((op1 & 5u) == 0) {
        if (bits(h1, 9, 7) != 7) { // B<cond> (T3)
            in.op = OP_BCOND;
            in.alu = u8(bits(h1, 9, 6));
            const std::uint32_t imm = (s << 20) | (j2 << 19) | (j1 << 18) | (bits(h1, 5, 0) << 12) | (bits(h2, 10, 0) << 1);
            in.imm = sign_extend(imm, 21);
            return in;
        }
        const std::uint32_t op = bits(h1, 10, 4);
        if (op == 0x38 || op == 0x39) {
            in.op = OP_MSR;
            in.rn = u8(bits(h1, 3, 0));
            in.mode = u8(bits(h2, 11, 10));
            in.imm = bits(h2, 7, 0);
        } else if (op == 0x3A) {
            static const std::uint8_t hints[5] = {OP_NOP, OP_YIELD, OP_WFE, OP_WFI, OP_SEV};
            const std::uint32_t hint = bits(h2, 7, 0);
            if (bits(h2, 10, 8) != 0) {
                return undefined(4);
            }
            in.op = hint < 5 ? hints[hint] : OP_NOP; // DBG and unallocated hints execute as NOP
        } else if (op == 0x3B) {
            const std::uint32_t opm = bits(h2, 7, 4);
            if (opm == 2) {
                in.op = OP_CLREX;
            } else if (opm >= 4 && opm <= 6) {
                in.op = OP_BARRIER;
            } else {
                return undefined(4);
            }
        } else if (op == 0x3E || op == 0x3F) {
            in.op = OP_MRS;
            in.rd = u8(bits(h2, 11, 8));
            in.imm = bits(h2, 7, 0);
        } else {
            return undefined(4); // includes UDF.W
        }
        return in;
    }
    if ((op1 & 5u) == 4) {
        return undefined(4); // BLX (immediate) does not exist in ARMv7-M
    }
    const std::uint32_t i1 = (~(j1 ^ s)) & 1u;
    const std::uint32_t i2 = (~(j2 ^ s)) & 1u;
    const std::uint32_t imm = (s << 24) | (i1 << 23) | (i2 << 22) | (bits(h1, 9, 0) << 12) | (bits(h2, 10, 0) << 1);
    in.op = (op1 & 4u) ? OP_BL : OP_B;
    in.imm = sign_extend(imm, 25);
    return in;
}

// A5.3.7..A5.3.10 single loads and stores. h1: S = bit 8 (sign), bit 7 = 12-bit
// immediate form (or U for literals), bits 6:5 = size, bit 4 = load.
Insn decode_single(std::uint32_t h1, std::uint32_t h2)
{
    const bool is_signed = (h1 & 0x100u) != 0;
    const bool imm12_form = (h1 & 0x80u) != 0;
    const std::uint32_t size_code = bits(h1, 6, 5);
    const bool load = (h1 & 0x10u) != 0;
    const std::uint32_t rn = bits(h1, 3, 0);
    const std::uint32_t rt = bits(h2, 15, 12);
    if (size_code == 3 || (!load && is_signed)) {
        return undefined(4);
    }
    const unsigned size = 1u << size_code;
    if (load && rt == 15 && size < 4) {
        Insn hint; // PLD, PLI and unallocated memory hints
        hint.op = OP_NOP;
        return hint;
    }
    if (load && rn == 15) { // literal: bit 7 is U
        return memory(true, size, is_signed, rt, 15, bits(h2, 11, 0), true, imm12_form, false);
    }
    if (imm12_form) {
        return memory(load, size, is_signed, rt, rn, bits(h2, 11, 0), true, true, false);
    }
    const std::uint32_t op2 = bits(h2, 11, 6);
    if (op2 == 0) {
        return memory_reg(load, size, is_signed, rt, rn, bits(h2, 3, 0), bits(h2, 5, 4));
    }
    if ((op2 & 0x20u) == 0) {
        return undefined(4);
    }
    const bool p = (h2 & 0x400u) != 0;
    const bool u = (h2 & 0x200u) != 0;
    const bool w = (h2 & 0x100u) != 0;
    if (!p && !w) {
        return undefined(4);
    }
    // P=1 U=1 W=0 is the unprivileged form (LDRT/STRT); without an MPU it behaves
    // like the plain offset form.
    return memory(load, size, is_signed, rt, rn, bits(h2, 7, 0), p, u, w);
}

Insn decode32(std::uint32_t h1, std::uint32_t h2)
{
    const std::uint32_t op1 = bits(h1, 12, 11);
    const std::uint32_t op2 = bits(h1, 10, 4);
    Insn in;
    if (op1 == 1) {
        if ((op2 & 0x64u) == 0x00u) { // A5.3.5 load/store multiple
            const std::uint32_t op = bits(h1, 8, 7);
            if (op != 1 && op != 2) {
                return undefined(4);
            }
            in.op = (h1 & 0x10u) ? OP_LDM : OP_STM;
            in.rn = u8(bits(h1, 3, 0));
            in.imm = h2 & 0xFFFFu;
            in.mode = u8((op == 2 ? LM_DB : 0) | ((h1 & 0x20u) ? LM_WB : 0));
        } else if ((op2 & 0x64u) == 0x04u) { // A5.3.6 dual, exclusive, table branch
            const bool p = (h1 & 0x100u) != 0;
            const bool u = (h1 & 0x80u) != 0;
            const bool w = (h1 & 0x20u) != 0;
            const bool l = (h1 & 0x10u) != 0;
            const std::uint32_t rn = bits(h1, 3, 0);
            const std::uint32_t rt = bits(h2, 15, 12);
            in.rn = u8(rn);
            in.rd = u8(rt);
            if (!p && !w) {
                if (!u) {
                    in.op = l ? OP_LDREX : OP_STREX;
                    in.ra = u8(bits(h2, 11, 8)); // STREX status register
                    in.imm = bits(h2, 7, 0) * 4u;
                    in.mode = 4;
                } else {
                    const std::uint32_t op3 = bits(h2, 7, 4);
                    if (l && op3 == 0) {
                        in.op = OP_TBB;
                        in.rm = u8(bits(h2, 3, 0));
                    } else if (l && op3 == 1) {
                        in.op = OP_TBH;
                        in.rm = u8(bits(h2, 3, 0));
                    } else if (op3 == 4 || op3 == 5) {
                        in.op = l ? OP_LDREX : OP_STREX;
                        in.ra = u8(bits(h2, 3, 0));
                        in.imm = 0;
                        in.mode = op3 == 4 ? 1 : 2;
                    } else {
                        return undefined(4);
                    }
                }
            } else {
                in.op = l ? OP_LDRD : OP_STRD;
                in.ra = u8(bits(h2, 11, 8)); // Rt2
                in.imm = bits(h2, 7, 0) * 4u;
                in.mode = u8((p ? M_P : 0) | (u ? M_U : 0) | (w ? M_W : 0));
            }
        } else if ((op2 & 0x60u) == 0x20u) { // A5.3.11 data processing (shifted register)
            const std::uint32_t op = bits(h1, 8, 5);
            const std::uint32_t s = bits(h1, 4, 4);
            const std::uint32_t rn = bits(h1, 3, 0);
            const std::uint32_t rd = bits(h2, 11, 8);
            const std::uint8_t sf = u8(s);
            switch (op) {
            case 0: in = (rd == 15 && s) ? data(OP_DP_REG, TST, 0, rn, 1) : data(OP_DP_REG, AND, rd, rn, sf); break;
            case 1: in = data(OP_DP_REG, BIC, rd, rn, sf); break;
            case 2: in = rn == 15 ? data(OP_DP_REG, MOV, rd, 0, sf) : data(OP_DP_REG, ORR, rd, rn, sf); break;
            case 3: in = rn == 15 ? data(OP_DP_REG, MVN, rd, 0, sf) : data(OP_DP_REG, ORN, rd, rn, sf); break;
            case 4: in = (rd == 15 && s) ? data(OP_DP_REG, TEQ, 0, rn, 1) : data(OP_DP_REG, EOR, rd, rn, sf); break;
            case 8: in = (rd == 15 && s) ? data(OP_DP_REG, CMN, 0, rn, 1) : data(OP_DP_REG, ADD, rd, rn, sf); break;
            case 10: in = data(OP_DP_REG, ADC, rd, rn, sf); break;
            case 11: in = data(OP_DP_REG, SBC, rd, rn, sf); break;
            case 13: in = (rd == 15 && s) ? data(OP_DP_REG, CMP, 0, rn, 1) : data(OP_DP_REG, SUB, rd, rn, sf); break;
            case 14: in = data(OP_DP_REG, RSB, rd, rn, sf); break;
            default: return undefined(4);
            }
            in.rm = u8(bits(h2, 3, 0));
            imm_shift(bits(h2, 5, 4), (bits(h2, 14, 12) << 2) | bits(h2, 7, 6), in);
        } else {
            in.op = OP_NOCP;
        }
    } else if (op1 == 2) {
        if (h2 & 0x8000u) {
            in = decode_branch_misc(h1, h2);
        } else if ((op2 & 0x20u) == 0) {
            in = decode_modified_immediate(h1, h2);
        } else {
            in = decode_plain_immediate(h1, h2);
        }
    } else { // op1 == 3
        if ((op2 & 0x71u) == 0x00u || (op2 & 0x67u) == 0x01u || (op2 & 0x67u) == 0x03u || (op2 & 0x67u) == 0x05u) {
            in = decode_single(h1, h2);
        } else if ((op2 & 0x70u) == 0x20u) { // A5.3.12 data processing (register)
            const std::uint32_t op = bits(h1, 7, 4);
            const std::uint32_t rn = bits(h1, 3, 0);
            const std::uint32_t rd = bits(h2, 11, 8);
            const std::uint32_t opb = bits(h2, 7, 4);
            const std::uint32_t rm = bits(h2, 3, 0);
            if (bits(h2, 15, 12) != 15) {
                return undefined(4);
            }
            if (op < 8 && opb == 0) {
                static const std::uint8_t shifts[4] = {LSL, LSR, ASR, ROR};
                in = data(OP_DP_RSR, MOV, rd, 0, u8(op & 1u));
                in.rm = u8(rn);
                in.ra = u8(rm);
                in.shift_t = shifts[op >> 1];
            } else if ((op == 0 || op == 1 || op == 4 || op == 5) && (opb & 8u) && rn == 15) {
                static const std::uint8_t ext[6] = {OP_SXTH, OP_UXTH, 0, 0, OP_SXTB, OP_UXTB};
                in.op = ext[op];
                in.rd = u8(rd);
                in.rm = u8(rm);
                in.shift_n = u8(bits(h2, 5, 4) * 8u);
            } else if (op == 9 && (opb & 0xCu) == 8) {
                static const std::uint8_t misc[4] = {OP_REV, OP_REV16, OP_RBIT, OP_REVSH};
                in.op = misc[opb & 3u];
                in.rd = u8(rd);
                in.rm = u8(rm);
            } else if (op == 11 && opb == 8) {
                in.op = OP_CLZ;
                in.rd = u8(rd);
                in.rm = u8(rm);
            } else {
                return undefined(4); // DSP extension (parallel arithmetic, saturating add, SEL...)
            }
        } else if ((op2 & 0x78u) == 0x30u) { // A5.3.16 multiply
            const std::uint32_t op = bits(h1, 6, 4);
            const std::uint32_t opb = bits(h2, 5, 4);
            in.rn = u8(bits(h1, 3, 0));
            in.ra = u8(bits(h2, 15, 12));
            in.rd = u8(bits(h2, 11, 8));
            in.rm = u8(bits(h2, 3, 0));
            if (op != 0 || opb > 1) {
                return undefined(4);
            }
            in.op = opb == 1 ? OP_MLS : (in.ra == 15 ? OP_MUL : OP_MLA);
            in.s = 0;
        } else if ((op2 & 0x78u) == 0x38u) { // A5.3.17 long multiply, divide
            const std::uint32_t op = bits(h1, 6, 4);
            const std::uint32_t opb = bits(h2, 7, 4);
            in.rn = u8(bits(h1, 3, 0));
            in.rm = u8(bits(h2, 3, 0));
            in.rd = u8(bits(h2, 15, 12)); // RdLo
            in.ra = u8(bits(h2, 11, 8));  // RdHi
            if (op == 0 && opb == 0) {
                in.op = OP_SMULL;
            } else if (op == 2 && opb == 0) {
                in.op = OP_UMULL;
            } else if (op == 4 && opb == 0) {
                in.op = OP_SMLAL;
            } else if (op == 6 && opb == 0) {
                in.op = OP_UMLAL;
            } else if ((op == 1 || op == 3) && opb == 15) {
                in.op = op == 1 ? OP_SDIV : OP_UDIV;
                in.rd = u8(bits(h2, 11, 8));
            } else {
                return undefined(4);
            }
        } else if ((op2 & 0x67u) == 0x07u) {
            return undefined(4);
        } else {
            in.op = OP_NOCP;
        }
    }
    in.size = 4;
    return in;
}

} // namespace

// ---------------------------------------------------------------------------
// Construction and decoding

Cpu::Cpu(Bus& bus) : bus_(bus)
{
    flash_cache_.resize(cache_bytes / 2);
}

Insn Cpu::decode(std::uint32_t hw1, std::uint32_t hw2)
{
    if ((hw1 >> 11) >= 0x1Du) {
        return decode32(hw1, hw2);
    }
    return decode16(hw1);
}

const char* Cpu::op_name(std::uint8_t op)
{
    return op < OP_COUNT ? op_names[op] : "?";
}

int Cpu::op_count()
{
    return OP_COUNT;
}

const Insn& Cpu::fetch_decode()
{
    const std::uint32_t pc = pc_;
    Insn* slot = nullptr;
    if (pc - flash_base < cache_bytes) {
        slot = &flash_cache_[(pc - flash_base) >> 1];
    } else if (pc < cache_bytes) {
        slot = &flash_cache_[pc >> 1];
    }
    if (slot && slot->op != OP_NONE) {
        return *slot;
    }
    std::uint32_t hw1 = 0;
    std::uint32_t hw2 = 0;
    if ((pc >= 0x40000000u && pc < 0x60000000u) || pc >= 0xE0000000u) {
        // Execute-never regions (B3.1): a MemManage fault, escalated if disabled.
        mmfar_ = 0;
        cfsr_ |= MM_IACCVIOL;
        fault(EX_MEMMANAGE, "instruction fetch from an execute-never address");
        scratch_ = Insn{};
        scratch_.op = OP_NOP;
        return scratch_;
    }
    if (!bus_.read(pc, 2, hw1)) {
        raise_bus_fault(BF_IBUSERR, pc, false, "instruction fetch from an address with no memory");
        scratch_ = Insn{};
        scratch_.op = OP_NOP;
        return scratch_;
    }
    if ((hw1 >> 11) >= 0x1Du && !bus_.read(pc + 2, 2, hw2)) {
        raise_bus_fault(BF_IBUSERR, pc + 2, false, "instruction fetch from an address with no memory");
        scratch_ = Insn{};
        scratch_.op = OP_NOP;
        return scratch_;
    }
    if (slot) {
        *slot = decode(hw1, hw2);
        return *slot;
    }
    scratch_ = decode(hw1, hw2);
    return scratch_;
}

// ---------------------------------------------------------------------------
// Registers

std::uint32_t Cpu::read_reg(unsigned n) const
{
    return n == 15 ? pc_ + 4 : r_[n];
}

void Cpu::write_reg(unsigned n, std::uint32_t value)
{
    if (n == 15) {
        branch_write_pc(value);
    } else if (n == 13) {
        set_sp(value);
    } else {
        r_[n] = value;
    }
}

std::uint32_t Cpu::reg(int n) const
{
    return n == 15 ? pc_ : r_[static_cast<unsigned>(n)];
}

void Cpu::set_reg(int n, std::uint32_t value)
{
    if (n == 15) {
        pc_ = value & ~1u;
    } else if (n == 13) {
        set_sp(value);
    } else {
        r_[static_cast<unsigned>(n)] = value;
    }
}

void Cpu::set_sp(std::uint32_t value)
{
    r_[13] = value & ~3u;
}

void Cpu::select_sp(bool process)
{
    if (process != sp_process_) {
        std::swap(r_[13], sp_other_);
        sp_process_ = process;
    }
}

std::uint32_t Cpu::msp() const
{
    return sp_process_ ? sp_other_ : r_[13];
}

std::uint32_t Cpu::psp() const
{
    return sp_process_ ? r_[13] : sp_other_;
}

std::uint32_t Cpu::control() const
{
    return (npriv_ ? 1u : 0u) | (spsel_ ? 2u : 0u);
}

std::uint32_t Cpu::xpsr() const
{
    return (n_ ? 1u << 31 : 0) | (z_ ? 1u << 30 : 0) | (c_ ? 1u << 29 : 0) | (v_ ? 1u << 28 : 0) | (q_ ? 1u << 27 : 0)
        | ((itstate_ & 3u) << 25) | (thumb_ ? 1u << 24 : 0) | ((itstate_ >> 2) << 10) | ipsr_;
}

void Cpu::branch_write_pc(std::uint32_t address)
{
    next_pc_ = address & ~1u;
}

void Cpu::bx_write_pc(std::uint32_t address)
{
    if (ipsr_ != 0 && (address >> 28) == 0xFu) {
        exception_return(address);
        return;
    }
    thumb_ = (address & 1u) != 0; // a clear bit 0 faults (INVSTATE) on the next instruction
    next_pc_ = address & ~1u;
}

bool Cpu::condition_passed(unsigned cond) const
{
    bool result = false;
    switch (cond >> 1) {
    case 0: result = z_; break;
    case 1: result = c_; break;
    case 2: result = n_; break;
    case 3: result = v_; break;
    case 4: result = c_ && !z_; break;
    case 5: result = n_ == v_; break;
    case 6: result = n_ == v_ && !z_; break;
    default: return true; // AL
    }
    return (cond & 1u) ? !result : result;
}

void Cpu::it_advance()
{
    if ((itstate_ & 7u) == 0) {
        itstate_ = 0;
    } else {
        itstate_ = (itstate_ & 0xE0u) | ((itstate_ << 1) & 0x1Fu);
    }
}

bool Cpu::mem_read(std::uint32_t address, unsigned size, std::uint32_t& value)
{
    if (bus_.read(address, size, value)) {
        return true;
    }
    raise_bus_fault(BF_PRECISERR, address, true, "read from an address with no memory or peripheral");
    return false;
}

bool Cpu::mem_write(std::uint32_t address, unsigned size, std::uint32_t value)
{
    if (bus_.write(address, size, value)) {
        return true;
    }
    raise_bus_fault(BF_PRECISERR, address, true, "write to an address with no memory or peripheral, or to read-only memory");
    return false;
}

// ---------------------------------------------------------------------------
// Execution

void Cpu::reset()
{
    r_.fill(0);
    r_[14] = 0xFFFFFFFFu;
    sp_other_ = 0;
    sp_process_ = false;
    n_ = z_ = c_ = v_ = q_ = false;
    itstate_ = 0;
    ipsr_ = 0;
    primask_ = faultmask_ = basepri_ = 0;
    npriv_ = spsel_ = false;
    pending_.fill(false);
    active_.fill(false);
    irq_enabled_.fill(false);
    priority_.fill(0);
    vtor_ = 0;
    aircr_prigroup_ = 0;
    scr_ = 0;
    ccr_ = 0; // Cortex-M3 r1p1: STKALIGN resets to 0
    shcsr_ = 0;
    cfsr_ = hfsr_ = mmfar_ = bfar_ = 0;
    monitor_ = false;
    event_register_ = false;
    st_ctrl_ = 0;
    st_reload_ = 0;
    st_base_ = cycles_;
    st_start_ = 0;
    st_countflag_ = false;
    state_ = CoreState::Running;
    attention_ = true;
    faulted_ = false;
    last_fault_.clear();
    std::fill(flash_cache_.begin(), flash_cache_.end(), Insn{});
    for (int irq = 0; irq < num_irqs; ++irq) {
        if (irq_line_[static_cast<unsigned>(irq)]) {
            pending_[static_cast<unsigned>(16 + irq)] = true;
        }
    }

    std::uint32_t sp = 0;
    std::uint32_t entry = 0;
    if (!bus_.read(0, 4, sp) || !bus_.read(4, 4, entry)) {
        lockup("no vector table at address 0 (is the firmware loaded?)");
        return;
    }
    set_sp(sp);
    thumb_ = (entry & 1u) != 0;
    pc_ = entry & ~1u;
}

CoreState Cpu::run(std::uint64_t limit)
{
    stop_requested_ = false;
    while (cycles_ < limit && !stop_requested_) {
        if (cycles_ >= systick_next_event()) {
            systick_update(cycles_);
        }
        if (state_ == CoreState::Lockup) {
            cycles_ = limit;
            break;
        }
        if (state_ == CoreState::Sleeping) {
            // Wake on any pending exception that could preempt if PRIMASK were 0 (B1.5.19).
            const std::uint32_t saved = primask_;
            primask_ = 0;
            const bool wake = pending_exception() != 0;
            primask_ = saved;
            if (!wake) {
                cycles_ = std::min(limit, systick_next_event());
                continue;
            }
            state_ = CoreState::Running;
            attention_ = true;
        }
        step();
    }
    return state_;
}

void Cpu::advance_idle(std::uint64_t to)
{
    if (to > cycles_) {
        cycles_ = to;
    }
}

void Cpu::step()
{
    if (attention_) {
        attention_ = false;
        check_exceptions();
        if (state_ != CoreState::Running) {
            return;
        }
    }
    if (!thumb_) {
        raise_usage_fault(UF_INVSTATE, "execution with the Thumb bit clear (a branch to an even address)");
        faulted_ = false;
        take_exception(pending_exception() ? pending_exception() : EX_HARDFAULT, pc_);
        return;
    }
    const std::uint32_t it_before = itstate_;
    const Insn& in = fetch_decode();
    ++cycles_;
    ++instructions_;
    if (faulted_) {
        faulted_ = false;
        take_exception(pending_exception() ? pending_exception() : EX_HARDFAULT, pc_);
        return;
    }
    next_pc_ = pc_ + in.size;
    if (in_it_block() && in.op != OP_IT) {
        const bool pass = condition_passed(itstate_ >> 4);
        it_advance();
        if (!pass) {
            pc_ = next_pc_;
            return;
        }
        in_it_ = true;
    } else {
        in_it_ = false;
    }
    if (count_ops_) {
        ++op_hits_[in.op];
    }
    execute(in);
    if (faulted_) {
        faulted_ = false;
        itstate_ = it_before;
        const int number = pending_exception();
        take_exception(number ? number : EX_HARDFAULT, pc_);
        return;
    }
    pc_ = next_pc_;
}

void Cpu::execute(const Insn& in)
{
    const bool setflags = in.s == 1 || (in.s == 2 && !in_it_);
    switch (in.op) {
    case OP_DP_IMM:
    case OP_DP_REG:
    case OP_DP_RSR: {
        std::uint32_t operand;
        bool carry = c_;
        if (in.op == OP_DP_IMM) {
            operand = in.imm;
            if (in.carry >= 0) {
                carry = in.carry != 0;
            }
        } else {
            const unsigned amount = in.op == OP_DP_RSR ? (read_reg(in.ra) & 0xFFu) : in.shift_n;
            const Shifted sh = shift_c(read_reg(in.rm), in.shift_t, amount, c_);
            operand = sh.value;
            carry = sh.carry;
        }
        const std::uint32_t a = read_reg(in.rn);
        std::uint32_t result = 0;
        bool overflow = v_;
        bool write = true;
        switch (in.alu) {
        case AND: result = a & operand; break;
        case EOR: result = a ^ operand; break;
        case ORR: result = a | operand; break;
        case ORN: result = a | ~operand; break;
        case BIC: result = a & ~operand; break;
        case MOV: result = operand; break;
        case MVN: result = ~operand; break;
        case TST: result = a & operand; write = false; break;
        case TEQ: result = a ^ operand; write = false; break;
        default: {
            Sum sum{};
            switch (in.alu) {
            case ADD: sum = add_with_carry(a, operand, false); break;
            case ADC: sum = add_with_carry(a, operand, c_); break;
            case SUB: sum = add_with_carry(a, ~operand, true); break;
            case SBC: sum = add_with_carry(a, ~operand, c_); break;
            case RSB: sum = add_with_carry(~a, operand, true); break;
            case CMP: sum = add_with_carry(a, ~operand, true); write = false; break;
            default: sum = add_with_carry(a, operand, false); write = false; break; // CMN
            }
            result = sum.value;
            carry = sum.carry;
            overflow = sum.overflow;
            break;
        }
        }
        if (write) {
            write_reg(in.rd, result);
            if (in.rd == 15) {
                return; // ALUWritePC: flags are not updated
            }
        }
        if (setflags) {
            n_ = (result >> 31) != 0;
            z_ = result == 0;
            c_ = carry;
            v_ = overflow;
        }
        return;
    }
    case OP_ADR: {
        const std::uint32_t base = (pc_ + 4) & ~3u;
        write_reg(in.rd, in.mode ? base + in.imm : base - in.imm);
        return;
    }
    case OP_MOVW:
        write_reg(in.rd, in.imm);
        return;
    case OP_MOVT:
        write_reg(in.rd, (read_reg(in.rd) & 0xFFFFu) | (in.imm << 16));
        return;
    case OP_MUL: {
        const std::uint32_t result = read_reg(in.rn) * read_reg(in.rm);
        write_reg(in.rd, result);
        if (setflags) {
            n_ = (result >> 31) != 0;
            z_ = result == 0;
        }
        return;
    }
    case OP_MLA:
        write_reg(in.rd, read_reg(in.rn) * read_reg(in.rm) + read_reg(in.ra));
        return;
    case OP_MLS:
        write_reg(in.rd, read_reg(in.ra) - read_reg(in.rn) * read_reg(in.rm));
        return;
    case OP_SMULL:
    case OP_SMLAL: {
        std::int64_t product = std::int64_t{static_cast<std::int32_t>(read_reg(in.rn))}
            * static_cast<std::int32_t>(read_reg(in.rm));
        if (in.op == OP_SMLAL) {
            product += static_cast<std::int64_t>((std::uint64_t{read_reg(in.ra)} << 32) | read_reg(in.rd));
        }
        write_reg(in.rd, static_cast<std::uint32_t>(product));
        write_reg(in.ra, static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32));
        return;
    }
    case OP_UMULL:
    case OP_UMLAL: {
        std::uint64_t product = std::uint64_t{read_reg(in.rn)} * read_reg(in.rm);
        if (in.op == OP_UMLAL) {
            product += (std::uint64_t{read_reg(in.ra)} << 32) | read_reg(in.rd);
        }
        write_reg(in.rd, static_cast<std::uint32_t>(product));
        write_reg(in.ra, static_cast<std::uint32_t>(product >> 32));
        return;
    }
    case OP_SDIV:
    case OP_UDIV: {
        const std::uint32_t n = read_reg(in.rn);
        const std::uint32_t m = read_reg(in.rm);
        if (m == 0) {
            if (ccr_ & (1u << 4)) {
                raise_usage_fault(UF_DIVBYZERO, "division by zero (CCR.DIV_0_TRP is set)");
                return;
            }
            write_reg(in.rd, 0);
            return;
        }
        if (in.op == OP_UDIV) {
            write_reg(in.rd, n / m);
        } else if (n == 0x80000000u && m == 0xFFFFFFFFu) {
            write_reg(in.rd, 0x80000000u);
        } else {
            write_reg(in.rd, static_cast<std::uint32_t>(static_cast<std::int32_t>(n) / static_cast<std::int32_t>(m)));
        }
        return;
    }
    case OP_CLZ: {
        std::uint32_t x = read_reg(in.rm);
        std::uint32_t count = 0;
        while (count < 32 && !(x & 0x80000000u)) {
            x <<= 1;
            ++count;
        }
        write_reg(in.rd, count);
        return;
    }
    case OP_RBIT: {
        std::uint32_t x = read_reg(in.rm);
        std::uint32_t result = 0;
        for (int i = 0; i < 32; ++i) {
            result = (result << 1) | (x & 1u);
            x >>= 1;
        }
        write_reg(in.rd, result);
        return;
    }
    case OP_REV: {
        const std::uint32_t x = read_reg(in.rm);
        write_reg(in.rd, (x >> 24) | ((x >> 8) & 0xFF00u) | ((x << 8) & 0xFF0000u) | (x << 24));
        return;
    }
    case OP_REV16: {
        const std::uint32_t x = read_reg(in.rm);
        write_reg(in.rd, ((x >> 8) & 0x00FF00FFu) | ((x << 8) & 0xFF00FF00u));
        return;
    }
    case OP_REVSH: {
        const std::uint32_t x = read_reg(in.rm);
        write_reg(in.rd, sign_extend(((x >> 8) & 0xFFu) | ((x & 0xFFu) << 8), 16));
        return;
    }
    case OP_SXTB:
        write_reg(in.rd, sign_extend(ror(read_reg(in.rm), in.shift_n), 8));
        return;
    case OP_SXTH:
        write_reg(in.rd, sign_extend(ror(read_reg(in.rm), in.shift_n), 16));
        return;
    case OP_UXTB:
        write_reg(in.rd, ror(read_reg(in.rm), in.shift_n) & 0xFFu);
        return;
    case OP_UXTH:
        write_reg(in.rd, ror(read_reg(in.rm), in.shift_n) & 0xFFFFu);
        return;
    case OP_SSAT:
    case OP_USAT: {
        const std::int64_t value = static_cast<std::int32_t>(shift_c(read_reg(in.rn), in.shift_t, in.shift_n, c_).value);
        std::int64_t low;
        std::int64_t high;
        if (in.op == OP_SSAT) {
            high = (std::int64_t{1} << (in.imm - 1)) - 1;
            low = -(std::int64_t{1} << (in.imm - 1));
        } else {
            high = (std::int64_t{1} << in.imm) - 1;
            low = 0;
        }
        std::int64_t result = value;
        if (value > high) {
            result = high;
            q_ = true;
        } else if (value < low) {
            result = low;
            q_ = true;
        }
        write_reg(in.rd, static_cast<std::uint32_t>(result));
        return;
    }
    case OP_SBFX:
    case OP_UBFX: {
        const unsigned lsb = in.shift_n;
        const unsigned width = in.imm + 1;
        if (lsb + width > 32) {
            raise_usage_fault(UF_UNDEFINSTR, "bitfield beyond bit 31 (UNPREDICTABLE)");
            return;
        }
        const std::uint32_t field = (read_reg(in.rn) >> lsb) & (width == 32 ? 0xFFFFFFFFu : ((1u << width) - 1u));
        write_reg(in.rd, in.op == OP_SBFX ? sign_extend(field, width) : field);
        return;
    }
    case OP_BFI:
    case OP_BFC: {
        const unsigned lsb = in.shift_n;
        const unsigned msb = in.imm;
        if (msb < lsb) {
            raise_usage_fault(UF_UNDEFINSTR, "bitfield with msb below lsb (UNPREDICTABLE)");
            return;
        }
        const unsigned width = msb - lsb + 1;
        const std::uint32_t mask = (width == 32 ? 0xFFFFFFFFu : ((1u << width) - 1u)) << lsb;
        const std::uint32_t source = in.op == OP_BFI ? (read_reg(in.rn) << lsb) : 0;
        write_reg(in.rd, (read_reg(in.rd) & ~mask) | (source & mask));
        return;
    }
    case OP_LDR:
    case OP_STR: {
        const unsigned size = in.mode & 7u;
        const std::uint32_t base = in.rn == 15 ? ((pc_ + 4) & ~3u) : read_reg(in.rn);
        const std::uint32_t offset = (in.mode & M_REG) ? (read_reg(in.rm) << in.shift_n) : in.imm;
        const std::uint32_t offset_address = (in.mode & M_U) ? base + offset : base - offset;
        const std::uint32_t address = (in.mode & M_P) ? offset_address : base;
        if ((ccr_ & (1u << 3)) && (address & (size - 1u))) {
            raise_usage_fault(UF_UNALIGNED, "unaligned access with CCR.UNALIGN_TRP set");
            return;
        }
        if (in.op == OP_LDR) {
            std::uint32_t value = 0;
            if (!mem_read(address, size, value)) {
                return;
            }
            if (in.mode & M_SIGNED) {
                value = sign_extend(value, size * 8);
            }
            if (in.mode & M_W) {
                write_reg(in.rn, offset_address);
            }
            if (in.rd == 15) {
                if (address & 3u) {
                    raise_usage_fault(UF_UNALIGNED, "unaligned load into the PC");
                    return;
                }
                bx_write_pc(value);
            } else {
                write_reg(in.rd, value);
            }
        } else {
            if (!mem_write(address, size, read_reg(in.rd))) {
                return;
            }
            if (in.mode & M_W) {
                write_reg(in.rn, offset_address);
            }
        }
        return;
    }
    case OP_LDRD:
    case OP_STRD: {
        const std::uint32_t base = in.rn == 15 ? ((pc_ + 4) & ~3u) : read_reg(in.rn);
        const std::uint32_t offset_address = (in.mode & M_U) ? base + in.imm : base - in.imm;
        const std::uint32_t address = (in.mode & M_P) ? offset_address : base;
        if (address & 3u) {
            raise_usage_fault(UF_UNALIGNED, "unaligned LDRD/STRD");
            return;
        }
        if (in.op == OP_LDRD) {
            std::uint32_t first = 0;
            std::uint32_t second = 0;
            if (!mem_read(address, 4, first) || !mem_read(address + 4, 4, second)) {
                return;
            }
            write_reg(in.rd, first);
            write_reg(in.ra, second);
        } else {
            if (!mem_write(address, 4, read_reg(in.rd)) || !mem_write(address + 4, 4, read_reg(in.ra))) {
                return;
            }
        }
        if (in.mode & M_W) {
            write_reg(in.rn, offset_address);
        }
        return;
    }
    case OP_LDREX: {
        const unsigned size = in.mode;
        const std::uint32_t address = read_reg(in.rn) + in.imm;
        if (address & (size - 1u)) {
            raise_usage_fault(UF_UNALIGNED, "unaligned exclusive access");
            return;
        }
        std::uint32_t value = 0;
        if (!mem_read(address, size, value)) {
            return;
        }
        monitor_ = true;
        write_reg(in.rd, value);
        return;
    }
    case OP_STREX: {
        const unsigned size = in.mode;
        const std::uint32_t address = read_reg(in.rn) + in.imm;
        if (address & (size - 1u)) {
            raise_usage_fault(UF_UNALIGNED, "unaligned exclusive access");
            return;
        }
        if (monitor_) {
            if (!mem_write(address, size, read_reg(in.rd))) {
                return;
            }
            write_reg(in.ra, 0);
        } else {
            write_reg(in.ra, 1);
        }
        monitor_ = false;
        return;
    }
    case OP_CLREX:
        monitor_ = false;
        return;
    case OP_LDM:
    case OP_STM: {
        const std::uint32_t list = in.imm;
        const unsigned count = popcount(list);
        const std::uint32_t base = read_reg(in.rn);
        std::uint32_t address = (in.mode & LM_DB) ? base - 4u * count : base;
        if (address & 3u) {
            raise_usage_fault(UF_UNALIGNED, "unaligned LDM/STM");
            return;
        }
        const std::uint32_t final_base = (in.mode & LM_DB) ? base - 4u * count : base + 4u * count;
        if (in.op == OP_STM) {
            for (unsigned i = 0; i < 15; ++i) {
                if (list & (1u << i)) {
                    if (!mem_write(address, 4, read_reg(i))) {
                        return;
                    }
                    address += 4;
                }
            }
            if (in.mode & LM_WB) {
                write_reg(in.rn, final_base);
            }
        } else {
            std::uint32_t values[16] = {};
            for (unsigned i = 0; i < 16; ++i) {
                if (list & (1u << i)) {
                    if (!mem_read(address, 4, values[i])) {
                        return;
                    }
                    address += 4;
                }
            }
            if ((in.mode & LM_WB) && !(list & (1u << in.rn))) {
                write_reg(in.rn, final_base);
            }
            for (unsigned i = 0; i < 15; ++i) {
                if (list & (1u << i)) {
                    write_reg(i, values[i]);
                }
            }
            if (list & 0x8000u) {
                bx_write_pc(values[15]);
            }
        }
        return;
    }
    case OP_TBB:
    case OP_TBH: {
        const std::uint32_t base = read_reg(in.rn);
        const std::uint32_t index = read_reg(in.rm);
        std::uint32_t entry = 0;
        if (in.op == OP_TBB) {
            if (!mem_read(base + index, 1, entry)) {
                return;
            }
        } else if (!mem_read(base + (index << 1), 2, entry)) {
            return;
        }
        branch_write_pc(pc_ + 4 + entry * 2u);
        return;
    }
    case OP_B:
        branch_write_pc(pc_ + 4 + in.imm);
        return;
    case OP_BCOND:
        if (condition_passed(in.alu)) {
            branch_write_pc(pc_ + 4 + in.imm);
        }
        return;
    case OP_BL:
        r_[14] = next_pc_ | 1u;
        branch_write_pc(pc_ + 4 + in.imm);
        return;
    case OP_BX:
        bx_write_pc(read_reg(in.rm));
        return;
    case OP_BLX: {
        const std::uint32_t target = read_reg(in.rm);
        r_[14] = next_pc_ | 1u;
        bx_write_pc(target);
        return;
    }
    case OP_CBZ:
    case OP_CBNZ:
        if ((read_reg(in.rn) == 0) == (in.op == OP_CBZ)) {
            branch_write_pc(pc_ + 4 + in.imm);
        }
        return;
    case OP_IT:
        itstate_ = in.imm;
        return;
    case OP_MRS: {
        std::uint32_t value = 0;
        switch (in.imm) {
        case 0: case 1: case 2: case 3: case 5: case 6: case 7: // xPSR views; EPSR bits read as zero
            value = ((in.imm & 4u) ? 0 : (xpsr() & 0xF8000000u)) | ((in.imm & 1u) ? ipsr_ : 0);
            break;
        case 8: value = msp(); break;
        case 9: value = psp(); break;
        case 16: value = primask_; break;
        case 17: case 18: value = basepri_; break;
        case 19: value = faultmask_; break;
        case 20: value = control(); break;
        default: value = 0; break;
        }
        write_reg(in.rd, value);
        return;
    }
    case OP_MSR: {
        const std::uint32_t value = read_reg(in.rn);
        const bool privileged = ipsr_ != 0 || !npriv_;
        if (in.imm < 8) {
            if (in.imm < 4 && (in.mode & 2u)) { // APSR NZCVQ
                n_ = (value >> 31) & 1u;
                z_ = (value >> 30) & 1u;
                c_ = (value >> 29) & 1u;
                v_ = (value >> 28) & 1u;
                q_ = (value >> 27) & 1u;
            }
            return;
        }
        if (!privileged) {
            return;
        }
        switch (in.imm) {
        case 8:
            if (sp_process_) {
                sp_other_ = value & ~3u;
            } else {
                set_sp(value);
            }
            break;
        case 9:
            if (sp_process_) {
                set_sp(value);
            } else {
                sp_other_ = value & ~3u;
            }
            break;
        case 16: primask_ = value & 1u; break;
        case 17: basepri_ = value & 0xF0u; break;
        case 18:
            if ((value & 0xF0u) != 0 && ((value & 0xF0u) < basepri_ || basepri_ == 0)) {
                basepri_ = value & 0xF0u;
            }
            break;
        case 19:
            if (execution_priority() > -1) {
                faultmask_ = value & 1u;
            }
            break;
        case 20:
            npriv_ = (value & 1u) != 0;
            if (ipsr_ == 0) {
                spsel_ = (value & 2u) != 0;
                select_sp(spsel_);
            }
            break;
        default: break;
        }
        attention_ = true;
        return;
    }
    case OP_CPS: {
        const bool privileged = ipsr_ != 0 || !npriv_;
        if (!privileged) {
            return;
        }
        const bool disable = (in.mode & 0x10u) != 0;
        if (in.mode & 2u) {
            primask_ = disable ? 1u : 0u;
        }
        if ((in.mode & 1u) && (!disable || execution_priority() > -1)) {
            faultmask_ = disable ? 1u : 0u;
        }
        attention_ = true;
        return;
    }
    case OP_NOP:
    case OP_YIELD:
    case OP_BARRIER:
        return;
    case OP_SEV:
        event_register_ = true;
        return;
    case OP_WFE:
        if (event_register_) {
            event_register_ = false;
            return;
        }
        state_ = CoreState::Sleeping;
        return;
    case OP_WFI:
        state_ = CoreState::Sleeping;
        return;
    case OP_SVC:
        if (exception_priority(EX_SVCALL) < execution_priority()) {
            pending_[EX_SVCALL] = true;
        } else if (execution_priority() <= -1) {
            lockup("SVC called from HardFault or NMI");
        } else {
            hfsr_ |= HF_FORCED;
            pending_[EX_HARDFAULT] = true;
        }
        attention_ = true;
        return;
    case OP_BKPT:
        hfsr_ |= HF_DEBUGEVT;
        fault(EX_HARDFAULT, "BKPT instruction executed without a debugger");
        return;
    case OP_NOCP:
        raise_usage_fault(UF_NOCP, "coprocessor or floating-point instruction (the Cortex-M3 has none)");
        return;
    case OP_UDF:
    case OP_UNDEF:
    default:
        raise_usage_fault(UF_UNDEFINSTR, "undefined instruction");
        return;
    }
}

// ---------------------------------------------------------------------------
// Exceptions (B1.5)

int Cpu::exception_priority(int number) const
{
    switch (number) {
    case EX_RESET: return -3;
    case EX_NMI: return -2;
    case EX_HARDFAULT: return -1;
    default: return priority_[static_cast<unsigned>(number)];
    }
}

int Cpu::group_priority(int number) const
{
    const int priority = exception_priority(number);
    if (priority < 0) {
        return priority;
    }
    const int mask = ~((2 << aircr_prigroup_) - 1);
    return priority & mask;
}

int Cpu::execution_priority() const
{
    int priority = 256;
    for (int n = 2; n < num_exceptions; ++n) {
        if (active_[static_cast<unsigned>(n)]) {
            priority = std::min(priority, group_priority(n));
        }
    }
    if (basepri_ != 0) {
        const int mask = ~((2 << aircr_prigroup_) - 1);
        priority = std::min(priority, static_cast<int>(basepri_) & mask);
    }
    if (primask_) {
        priority = std::min(priority, 0);
    }
    if (faultmask_) {
        priority = std::min(priority, -1);
    }
    return priority;
}

int Cpu::pending_exception() const
{
    int best = 0;
    int best_group = 0;
    int best_priority = 0;
    for (int n = 2; n < num_exceptions; ++n) {
        if (!pending_[static_cast<unsigned>(n)]) {
            continue;
        }
        if (n >= 16 && !irq_enabled_[static_cast<unsigned>(n - 16)]) {
            continue;
        }
        const int group = group_priority(n);
        const int priority = exception_priority(n);
        if (best == 0 || group < best_group || (group == best_group && priority < best_priority)) {
            best = n;
            best_group = group;
            best_priority = priority;
        }
    }
    if (best != 0 && best_group < execution_priority()) {
        return best;
    }
    return 0;
}

void Cpu::check_exceptions()
{
    if (state_ == CoreState::Lockup) {
        return;
    }
    const int number = pending_exception();
    if (number != 0) {
        take_exception(number, pc_);
    }
}

void Cpu::set_pending(int number)
{
    if (number > 0 && number < num_exceptions) {
        pending_[static_cast<unsigned>(number)] = true;
        attention_ = true;
    }
}

void Cpu::set_irq_line(int irq, bool level)
{
    if (irq < 0 || irq >= num_irqs) {
        return;
    }
    irq_line_[static_cast<unsigned>(irq)] = level;
    if (level && !active_[static_cast<unsigned>(16 + irq)]) {
        pending_[static_cast<unsigned>(16 + irq)] = true;
        attention_ = true;
    }
}

void Cpu::raise_usage_fault(std::uint32_t cfsr_bit, const char* what)
{
    cfsr_ |= cfsr_bit;
    fault(EX_USAGEFAULT, what);
}

void Cpu::raise_bus_fault(std::uint32_t bfsr_bit, std::uint32_t address, bool address_valid, const char* what)
{
    cfsr_ |= bfsr_bit;
    if (address_valid) {
        cfsr_ |= BF_BFARVALID;
        bfar_ = address;
    }
    char text[160];
    std::snprintf(text, sizeof text, "%s (address 0x%08X)", what, address);
    fault(EX_BUSFAULT, text);
}

void Cpu::fault(int number, const std::string& what)
{
    faulted_ = true;
    char where[48];
    std::snprintf(where, sizeof where, " at PC 0x%08X", pc_);
    last_fault_ = what + where;
    bool enabled = true;
    if (number == EX_MEMMANAGE) {
        enabled = (shcsr_ & (1u << 16)) != 0;
    } else if (number == EX_BUSFAULT) {
        enabled = (shcsr_ & (1u << 17)) != 0;
    } else if (number == EX_USAGEFAULT) {
        enabled = (shcsr_ & (1u << 18)) != 0;
    }
    if (number != EX_HARDFAULT && enabled && exception_priority(number) < execution_priority()) {
        pending_[static_cast<unsigned>(number)] = true;
        return;
    }
    if (execution_priority() <= -1) {
        lockup(last_fault_ + " while in HardFault or NMI");
        return;
    }
    if (number != EX_HARDFAULT) {
        hfsr_ |= HF_FORCED;
    }
    pending_[EX_HARDFAULT] = true;
}

void Cpu::lockup(const std::string& what)
{
    state_ = CoreState::Lockup;
    faulted_ = false;
    last_fault_ = "Lockup: " + what;
    stop_requested_ = true;
}

void Cpu::take_exception(int number, std::uint32_t return_address)
{
    if (state_ == CoreState::Lockup) {
        return;
    }
    // Stacking (B1.5.6) on the stack in use.
    const bool frame_align = (ccr_ & (1u << 9)) && (r_[13] & 4u);
    const std::uint32_t frame = (r_[13] - 0x20u) & ~(frame_align ? 4u : 0u);
    const std::uint32_t psr = (xpsr() & ~(1u << 9)) | (frame_align ? (1u << 9) : 0u);
    const std::uint32_t values[8] = {r_[0], r_[1], r_[2], r_[3], r_[12], r_[14], return_address & ~1u, psr};
    for (unsigned i = 0; i < 8; ++i) {
        if (!bus_.write(frame + 4 * i, 4, values[i])) {
            char text[120];
            std::snprintf(text, sizeof text,
                "the stack pointer 0x%08X is outside RAM during exception entry (stack overflow?)", r_[13]);
            lockup(text);
            return;
        }
    }
    set_sp(frame);
    std::uint32_t exc_return;
    if (ipsr_ != 0) {
        exc_return = 0xFFFFFFF1u;
    } else {
        exc_return = sp_process_ ? 0xFFFFFFFDu : 0xFFFFFFF9u;
    }
    r_[14] = exc_return;
    std::uint32_t vector = 0;
    if (!bus_.read(vtor_ + 4u * static_cast<std::uint32_t>(number), 4, vector)) {
        if (number == EX_HARDFAULT) {
            lockup("cannot read the HardFault vector");
            return;
        }
        hfsr_ |= HF_VECTTBL;
        number = EX_HARDFAULT;
        if (!bus_.read(vtor_ + 12u, 4, vector)) {
            lockup("cannot read the HardFault vector");
            return;
        }
    }
    select_sp(false);
    ipsr_ = static_cast<std::uint32_t>(number);
    itstate_ = 0;
    pending_[static_cast<unsigned>(number)] = false;
    active_[static_cast<unsigned>(number)] = true;
    monitor_ = false;
    event_register_ = true;
    thumb_ = (vector & 1u) != 0;
    pc_ = vector & ~1u;
    state_ = CoreState::Running;
    attention_ = true;
}

void Cpu::exception_return(std::uint32_t exc_return)
{
    const std::uint32_t kind = exc_return & 0xFu;
    const int returning = static_cast<int>(ipsr_);
    if ((exc_return & 0x0FFFFFF0u) != 0x0FFFFFF0u || (kind != 1 && kind != 9 && kind != 0xD)) {
        raise_usage_fault(UF_INVPC, "invalid EXC_RETURN value");
        return;
    }
    active_[static_cast<unsigned>(returning)] = false;
    if (returning != EX_NMI) {
        faultmask_ = 0;
    }
    const bool to_thread = kind != 1;
    const bool process = kind == 0xD;
    if (to_thread) {
        select_sp(process);
        spsel_ = process;
    } else {
        select_sp(false);
    }
    const std::uint32_t frame = r_[13];
    std::uint32_t values[8] = {};
    for (unsigned i = 0; i < 8; ++i) {
        if (!bus_.read(frame + 4 * i, 4, values[i])) {
            lockup("the stack pointer is outside RAM during exception return");
            return;
        }
    }
    r_[0] = values[0];
    r_[1] = values[1];
    r_[2] = values[2];
    r_[3] = values[3];
    r_[12] = values[4];
    r_[14] = values[5];
    const std::uint32_t psr = values[7];
    set_sp(frame + 0x20u + ((psr & (1u << 9)) ? 4u : 0u));
    n_ = (psr >> 31) & 1u;
    z_ = (psr >> 30) & 1u;
    c_ = (psr >> 29) & 1u;
    v_ = (psr >> 28) & 1u;
    q_ = (psr >> 27) & 1u;
    itstate_ = ((psr >> 25) & 3u) | (((psr >> 10) & 0x3Fu) << 2);
    thumb_ = (psr >> 24) & 1u;
    ipsr_ = psr & 0x1FFu;
    if ((to_thread && ipsr_ != 0) || (!to_thread && ipsr_ == 0)) {
        raise_usage_fault(UF_INVPC, "exception return to the wrong mode");
        return;
    }
    next_pc_ = values[6] & ~1u;
    monitor_ = false;
    event_register_ = true;
    if (returning >= 16 && irq_line_[static_cast<unsigned>(returning - 16)]) {
        pending_[static_cast<unsigned>(returning)] = true;
    }
    if (to_thread && (scr_ & 2u)) {
        state_ = CoreState::Sleeping; // SLEEPONEXIT
    }
    attention_ = true;
}

// ---------------------------------------------------------------------------
// SysTick (B3.3)

std::uint64_t Cpu::systick_next_event() const
{
    if ((st_ctrl_ & 1u) == 0) {
        return never;
    }
    const std::uint64_t period = (st_ctrl_ & 4u) ? 1u : 8u;
    if (st_start_ > 0) {
        return st_base_ + std::uint64_t{st_start_} * period;
    }
    if (st_reload_ == 0) {
        return never;
    }
    return st_base_ + (std::uint64_t{st_reload_} + 1u) * period;
}

void Cpu::systick_update(std::uint64_t now)
{
    std::uint64_t zero = systick_next_event();
    while (zero <= now) {
        st_countflag_ = true;
        if (st_ctrl_ & 2u) {
            pending_[EX_SYSTICK] = true;
            attention_ = true;
        }
        st_base_ = zero;
        st_start_ = 0;
        zero = systick_next_event();
    }
}

std::uint32_t Cpu::systick_value(std::uint64_t now) const
{
    if ((st_ctrl_ & 1u) == 0) {
        return st_start_;
    }
    const std::uint64_t period = (st_ctrl_ & 4u) ? 1u : 8u;
    const std::uint64_t ticks = (now - st_base_) / period;
    if (ticks <= st_start_ && st_start_ > 0) {
        return static_cast<std::uint32_t>(st_start_ - ticks);
    }
    const std::uint64_t since_zero = ticks - st_start_;
    if (st_reload_ == 0 || since_zero == 0) {
        return 0;
    }
    return static_cast<std::uint32_t>(st_reload_ - ((since_zero - 1) % (std::uint64_t{st_reload_} + 1u)));
}

void Cpu::systick_restart(std::uint64_t now, std::uint32_t value)
{
    st_base_ = now;
    st_start_ = value;
    if (on_schedule_changed) {
        on_schedule_changed();
    }
}

// ---------------------------------------------------------------------------
// System control space (B3.2, B3.3, B3.4)

bool Cpu::Scs::read(std::uint32_t offset, unsigned size, std::uint32_t& value)
{
    std::uint32_t word = 0;
    if (!cpu_.scs_read(offset & ~3u, word)) {
        return false;
    }
    value = (word >> (8 * (offset & 3u))) & (size == 4 ? 0xFFFFFFFFu : ((1u << (8 * size)) - 1u));
    return true;
}

bool Cpu::Scs::write(std::uint32_t offset, unsigned size, std::uint32_t value)
{
    // Byte-addressable priority registers.
    if ((offset >= 0x400 && offset < 0x400 + irq_count) || (offset >= 0xD18 && offset < 0xD24)) {
        for (unsigned i = 0; i < size; ++i) {
            const std::uint32_t byte_offset = offset + i;
            const std::uint8_t byte = static_cast<std::uint8_t>((value >> (8 * i)) & 0xF0u); // 4 priority bits
            if (byte_offset >= 0x400 && byte_offset < 0x400 + irq_count) {
                cpu_.priority_[16 + (byte_offset - 0x400)] = byte;
            } else if (byte_offset >= 0xD18 && byte_offset < 0xD24) {
                cpu_.priority_[4 + (byte_offset - 0xD18)] = byte;
            }
        }
        cpu_.attention_ = true;
        return true;
    }
    if (size != 4) {
        std::uint32_t word = 0;
        cpu_.scs_read(offset & ~3u, word);
        const unsigned shift = 8 * (offset & 3u);
        const std::uint32_t mask = ((size == 2) ? 0xFFFFu : 0xFFu) << shift;
        value = (word & ~mask) | ((value << shift) & mask);
    }
    return cpu_.scs_write(offset & ~3u, value);
}

bool Cpu::scs_read(std::uint32_t offset, std::uint32_t& value)
{
    value = 0;
    if (offset >= 0x100 && offset < 0x180) { // ISER
        const std::uint32_t base = (offset - 0x100) * 8;
        for (std::uint32_t i = 0; i < 32 && base + i < irq_count; ++i) {
            value |= irq_enabled_[base + i] ? (1u << i) : 0;
        }
        return true;
    }
    if (offset >= 0x180 && offset < 0x200) { // ICER
        return scs_read(offset - 0x80, value);
    }
    if (offset >= 0x200 && offset < 0x300) { // ISPR, ICPR
        const std::uint32_t base = ((offset - 0x200) & 0x7Fu) * 8;
        for (std::uint32_t i = 0; i < 32 && base + i < irq_count; ++i) {
            value |= pending_[16 + base + i] ? (1u << i) : 0;
        }
        return true;
    }
    if (offset >= 0x300 && offset < 0x380) { // IABR
        const std::uint32_t base = (offset - 0x300) * 8;
        for (std::uint32_t i = 0; i < 32 && base + i < irq_count; ++i) {
            value |= active_[16 + base + i] ? (1u << i) : 0;
        }
        return true;
    }
    if (offset >= 0x400 && offset < 0x5F0) { // IPR
        for (std::uint32_t i = 0; i < 4; ++i) {
            const std::uint32_t irq = offset - 0x400 + i;
            if (irq < irq_count) {
                value |= std::uint32_t{priority_[16 + irq]} << (8 * i);
            }
        }
        return true;
    }
    switch (offset) {
    case 0x004: value = 2; return true; // ICTR: up to 96 interrupts
    case 0x008: value = 0; return true; // ACTLR
    case 0x010: {
        systick_update(cycles_);
        value = st_ctrl_ | (st_countflag_ ? (1u << 16) : 0);
        st_countflag_ = false;
        return true;
    }
    case 0x014: value = st_reload_; return true;
    case 0x018: value = systick_value(cycles_); return true;
    case 0x01C: value = 9000; return true; // STK_CALIB: 1 ms at 9 MHz (HCLK/8 at 72 MHz), PM0056
    case 0xD00: value = 0x411FC231u; return true; // CPUID: Cortex-M3 r1p1
    case 0xD04: {
        const int vectpending = [this] {
            const std::uint32_t saved = primask_;
            int best = 0;
            for (int n = 2; n < num_exceptions; ++n) {
                if (pending_[static_cast<unsigned>(n)] && (n < 16 || irq_enabled_[static_cast<unsigned>(n - 16)])) {
                    if (best == 0 || exception_priority(n) < exception_priority(best)) {
                        best = n;
                    }
                }
            }
            (void)saved;
            return best;
        }();
        int active_count = 0;
        for (int n = 1; n < num_exceptions; ++n) {
            active_count += active_[static_cast<unsigned>(n)] ? 1 : 0;
        }
        bool isr_pending = false;
        for (int n = 16; n < num_exceptions; ++n) {
            isr_pending = isr_pending || pending_[static_cast<unsigned>(n)];
        }
        value = ipsr_ | (active_count <= 1 ? (1u << 11) : 0) | (static_cast<std::uint32_t>(vectpending) << 12)
            | (isr_pending ? (1u << 22) : 0) | (pending_[EX_SYSTICK] ? (1u << 26) : 0)
            | (pending_[EX_PENDSV] ? (1u << 28) : 0) | (pending_[EX_NMI] ? (1u << 31) : 0);
        return true;
    }
    case 0xD08: value = vtor_; return true;
    case 0xD0C: value = 0xFA050000u | (aircr_prigroup_ << 8); return true;
    case 0xD10: value = scr_; return true;
    case 0xD14: value = ccr_; return true;
    case 0xD18: case 0xD1C: case 0xD20:
        for (std::uint32_t i = 0; i < 4; ++i) {
            value |= std::uint32_t{priority_[4 + (offset - 0xD18) + i]} << (8 * i);
        }
        return true;
    case 0xD24: {
        value = shcsr_ & 0x70000u;
        value |= active_[EX_MEMMANAGE] ? 1u : 0;
        value |= active_[EX_BUSFAULT] ? 2u : 0;
        value |= active_[EX_USAGEFAULT] ? 8u : 0;
        value |= active_[EX_SVCALL] ? 0x80u : 0;
        value |= active_[EX_PENDSV] ? 0x400u : 0;
        value |= active_[EX_SYSTICK] ? 0x800u : 0;
        value |= pending_[EX_USAGEFAULT] ? 0x1000u : 0;
        value |= pending_[EX_MEMMANAGE] ? 0x2000u : 0;
        value |= pending_[EX_BUSFAULT] ? 0x4000u : 0;
        value |= pending_[EX_SVCALL] ? 0x8000u : 0;
        return true;
    }
    case 0xD28: value = cfsr_; return true;
    case 0xD2C: value = hfsr_; return true;
    case 0xD30: value = 0; return true; // DFSR
    case 0xD34: value = mmfar_; return true;
    case 0xD38: value = bfar_; return true;
    case 0xD3C: value = 0; return true; // AFSR
    default:
        value = 0; // other SCS registers (debug, feature IDs) read as zero
        return true;
    }
}

bool Cpu::scs_write(std::uint32_t offset, std::uint32_t value)
{
    attention_ = true;
    if (offset >= 0x100 && offset < 0x180) { // ISER
        const std::uint32_t base = (offset - 0x100) * 8;
        for (std::uint32_t i = 0; i < 32 && base + i < irq_count; ++i) {
            if (value & (1u << i)) {
                irq_enabled_[base + i] = true;
            }
        }
        return true;
    }
    if (offset >= 0x180 && offset < 0x200) { // ICER
        const std::uint32_t base = (offset - 0x180) * 8;
        for (std::uint32_t i = 0; i < 32 && base + i < irq_count; ++i) {
            if (value & (1u << i)) {
                irq_enabled_[base + i] = false;
            }
        }
        return true;
    }
    if (offset >= 0x200 && offset < 0x280) { // ISPR
        const std::uint32_t base = (offset - 0x200) * 8;
        for (std::uint32_t i = 0; i < 32 && base + i < irq_count; ++i) {
            if (value & (1u << i)) {
                pending_[16 + base + i] = true;
            }
        }
        return true;
    }
    if (offset >= 0x280 && offset < 0x300) { // ICPR
        const std::uint32_t base = (offset - 0x280) * 8;
        for (std::uint32_t i = 0; i < 32 && base + i < irq_count; ++i) {
            if (value & (1u << i)) {
                pending_[16 + base + i] = irq_line_[base + i]; // a level still asserted re-pends
            }
        }
        return true;
    }
    if (offset == 0xF00) { // STIR
        const std::uint32_t irq = value & 0x1FFu;
        if (irq < irq_count) {
            pending_[16 + irq] = true;
        }
        return true;
    }
    switch (offset) {
    case 0x010: {
        const std::uint32_t current = systick_value(cycles_);
        const bool was_enabled = (st_ctrl_ & 1u) != 0;
        st_ctrl_ = value & 7u;
        if (!was_enabled && (st_ctrl_ & 1u)) {
            systick_restart(cycles_, current);
        } else if (was_enabled && !(st_ctrl_ & 1u)) {
            st_start_ = current;
        } else if (on_schedule_changed) {
            on_schedule_changed();
        }
        return true;
    }
    case 0x014:
        st_reload_ = value & 0x00FFFFFFu;
        if (on_schedule_changed) {
            on_schedule_changed();
        }
        return true;
    case 0x018:
        st_countflag_ = false;
        if (st_ctrl_ & 1u) {
            systick_restart(cycles_, 0);
        } else {
            st_start_ = 0;
        }
        return true;
    case 0xD04: // ICSR
        if (value & (1u << 31)) {
            pending_[EX_NMI] = true;
        }
        if (value & (1u << 28)) {
            pending_[EX_PENDSV] = true;
        }
        if (value & (1u << 27)) {
            pending_[EX_PENDSV] = false;
        }
        if (value & (1u << 26)) {
            pending_[EX_SYSTICK] = true;
        }
        if (value & (1u << 25)) {
            pending_[EX_SYSTICK] = false;
        }
        return true;
    case 0xD08:
        vtor_ = value & 0x3FFFFF80u;
        return true;
    case 0xD0C:
        if ((value >> 16) != 0x05FAu) {
            return true; // wrong key: ignored
        }
        aircr_prigroup_ = (value >> 8) & 7u;
        if (value & 5u) { // SYSRESETREQ or VECTRESET
            if (on_system_reset) {
                on_system_reset();
            }
            stop_requested_ = true;
        }
        return true;
    case 0xD10:
        scr_ = value & 0x16u;
        return true;
    case 0xD14:
        ccr_ = value & 0x31Bu;
        return true;
    case 0xD24:
        shcsr_ = value & 0x70000u;
        return true;
    case 0xD28:
        cfsr_ &= ~value;
        return true;
    case 0xD2C:
        hfsr_ &= ~value;
        return true;
    case 0xD34:
        mmfar_ = value;
        return true;
    case 0xD38:
        bfar_ = value;
        return true;
    default:
        return true; // other SCS registers: writes ignored
    }
}

} // namespace simnodus::emu
