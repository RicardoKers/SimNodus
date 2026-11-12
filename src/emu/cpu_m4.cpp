// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Cortex-M4F additions to the core (experiment E-12, SN-293). Section numbers
// refer to the ARMv7-M Architecture Reference Manual (DDI 0403E): A5.3.12 to
// A5.3.17 (the DSP encodings among the data-processing and multiply groups),
// A6 (the floating-point encodings), A7.7 (instruction details), B1.5.6 and
// B1.5.7 (the floating-point context on exception entry and return).
#include "cpu_m4.hpp"

#include "softfloat32.hpp"

#include <cstdlib>

namespace simnodus::emu {

namespace m4 {

namespace {

inline std::uint32_t bits(std::uint32_t x, unsigned hi, unsigned lo)
{
    return (x >> lo) & ((2u << (hi - lo)) - 1u);
}

inline std::uint8_t u8(std::uint32_t x) { return static_cast<std::uint8_t>(x); }

// A single-precision register number from its four bits and the extra low bit.
inline std::uint8_t single_reg(std::uint32_t v, std::uint32_t low) { return u8((v << 1) | low); }

Kind vfp(Insn& in, std::uint8_t operation)
{
    in.alu = operation;
    return Kind::Vfp;
}

// A6.4: floating-point data processing (h1 = 1110 1110 ..., h2 bit 4 clear).
Kind decode_vfp_data(std::uint32_t h1, std::uint32_t h2, Insn& in)
{
    if (h2 & 0x100u) { // double precision: not in the FPv4-SP FPU
        return vfp(in, V_UNDEFINED);
    }
    const std::uint32_t d_bit = bits(h1, 6, 6);
    const std::uint32_t n_bit = bits(h2, 7, 7);
    const std::uint32_t m_bit = bits(h2, 5, 5);
    const std::uint32_t op = bits(h2, 6, 6);
    const std::uint32_t opc1 = ((h1 >> 4) & 0x8u) | ((h1 >> 4) & 0x3u); // bits 23, 21, 20
    const std::uint32_t opc2 = h1 & 0xFu;
    in.rd = single_reg(bits(h2, 15, 12), d_bit);
    in.rn = single_reg(opc2, n_bit);
    in.rm = single_reg(h2 & 0xFu, m_bit);
    switch (opc1) {
    case 0x0: return vfp(in, op ? V_MLS : V_MLA);
    case 0x1: return vfp(in, op ? V_NMLA : V_NMLS);
    case 0x2: return vfp(in, op ? V_NMUL : V_MUL);
    case 0x3: return vfp(in, op ? V_SUB : V_ADD);
    case 0x8: return vfp(in, op ? V_UNDEFINED : V_DIV);
    case 0x9: return vfp(in, op ? V_FNMA : V_FNMS);
    case 0xA: return vfp(in, op ? V_FMS : V_FMA);
    default: break; // 0xB: the other operations
    }
    if (!op) {
        if (h2 & 0xA0u) {
            return vfp(in, V_UNDEFINED);
        }
        in.imm = fp::expand_imm((opc2 << 4) | (h2 & 0xFu));
        return vfp(in, V_MOV_IMM);
    }
    switch (opc2) {
    case 0x0: return vfp(in, n_bit ? V_ABS : V_MOV);
    case 0x1: return vfp(in, n_bit ? V_SQRT : V_NEG);
    case 0x2:
    case 0x3:
        in.mode = u8(n_bit); // T: the top half
        return vfp(in, opc2 == 2 ? V_CVT_FROM_HALF : V_CVT_TO_HALF);
    case 0x4:
    case 0x5:
        if (opc2 == 5 && (h2 & 0x2Fu)) {
            return vfp(in, V_UNDEFINED);
        }
        in.mode = u8(n_bit | (opc2 == 5 ? 2u : 0u));
        return vfp(in, V_CMP);
    case 0x8:
        in.mode = u8(n_bit ? 0u : 1u); // op 1: signed
        return vfp(in, V_CVT_FROM_INT);
    case 0xC:
    case 0xD:
        in.mode = u8((opc2 == 0xC ? 1u : 0u) | (n_bit ? 2u : 0u));
        return vfp(in, V_CVT_TO_INT);
    case 0xA:
    case 0xB:
    case 0xE:
    case 0xF: {
        // Fixed point, in place: the register is Vd; h2 bit 7 is the size and
        // bits 3:0 with bit 5 the number of bits before the point.
        const std::uint32_t size = n_bit ? 32u : 16u;
        const std::uint32_t imm5 = ((h2 & 0xFu) << 1) | m_bit;
        if (imm5 > size) {
            return vfp(in, V_UNDEFINED);
        }
        in.imm = size - imm5;
        in.mode = u8((opc2 & 1u) | ((opc2 & 4u) ? 2u : 0u) | (n_bit ? 4u : 0u));
        return vfp(in, V_CVT_FIXED);
    }
    default:
        return vfp(in, V_UNDEFINED); // 0x7: between double and single precision
    }
}

// A6.6: transfers between core registers and the FPU (h2 bit 4 set).
Kind decode_vfp_transfer(std::uint32_t h1, std::uint32_t h2, Insn& in)
{
    const bool to_core = (h1 & 0x10u) != 0;
    const std::uint32_t a = bits(h1, 7, 5);
    const std::uint32_t vn = h1 & 0xFu;
    in.rd = u8(bits(h2, 15, 12)); // Rt
    if ((h2 & 0x6Fu) != 0) {
        return vfp(in, V_UNDEFINED);
    }
    if (!(h2 & 0x100u)) {
        if (a == 0) {
            in.rn = single_reg(vn, bits(h2, 7, 7));
            in.mode = to_core ? 1 : 0;
            return vfp(in, V_MOV_CORE);
        }
        if (a == 7 && vn == 1) { // FPSCR
            return vfp(in, to_core ? V_MRS : V_MSR);
        }
        return vfp(in, V_UNDEFINED);
    }
    // One word of a D register: h1 bit 5 chooses the high one; D16 to D31 do not exist.
    if (a > 1 || (h2 & 0x80u)) {
        return vfp(in, V_UNDEFINED);
    }
    in.rn = u8(vn * 2 + a);
    in.mode = to_core ? 1 : 0;
    return vfp(in, V_MOV_SCALAR);
}

// A6.5 and A6.7: loads, stores and the transfers of two core registers.
Kind decode_vfp_memory(std::uint32_t h1, std::uint32_t h2, Insn& in)
{
    const bool p = (h1 & 0x100u) != 0;
    const bool u = (h1 & 0x80u) != 0;
    const std::uint32_t d_bit = bits(h1, 6, 6);
    const bool w = (h1 & 0x20u) != 0;
    const bool load = (h1 & 0x10u) != 0;
    const bool doubles = (h2 & 0x100u) != 0;
    const std::uint32_t vd = bits(h2, 15, 12);
    const std::uint32_t imm8 = h2 & 0xFFu;
    in.rn = u8(h1 & 0xFu);
    if (!p && !u && !w) {
        // Two core registers and two singles or one double: 1110 1100 010 L Rt2, Rt 101 C 00 M 1 Vm.
        if (!d_bit || (h2 & 0xD0u) != 0x10u) {
            return vfp(in, V_UNDEFINED);
        }
        const std::uint32_t m_bit = bits(h2, 5, 5);
        if (doubles ? m_bit != 0 : (((h2 & 0xFu) << 1) | m_bit) == 31) {
            return vfp(in, V_UNDEFINED);
        }
        in.rd = u8(vd);          // Rt
        in.ra = u8(h1 & 0xFu);   // Rt2
        in.rm = doubles ? u8((h2 & 0xFu) * 2) : single_reg(h2 & 0xFu, m_bit);
        in.mode = load ? 1 : 0;
        return vfp(in, V_MOV_PAIR);
    }
    if (doubles && d_bit) {
        return vfp(in, V_UNDEFINED); // D16 to D31
    }
    in.rd = doubles ? u8(vd * 2) : single_reg(vd, d_bit);
    if (p && !w) { // VLDR, VSTR
        in.imm = imm8 << 2;
        in.mode = u8((doubles ? 1u : 0u) | (u ? 2u : 0u));
        return vfp(in, load ? V_LDR : V_STR);
    }
    if (p == u) { // P and U both set with write-back: nothing
        return vfp(in, V_UNDEFINED);
    }
    // VLDM, VSTM, VPUSH, VPOP: imm8 is the number of words (an odd count with
    // doubles is the FLDMX form, which moves the same registers).
    const std::uint32_t words = doubles ? (imm8 & ~1u) : imm8;
    if (words == 0 || in.rd + words > 32) {
        return vfp(in, V_UNDEFINED);
    }
    in.imm = words;
    in.mode = u8((u ? 2u : 0u) | (w ? 4u : 0u));
    return vfp(in, load ? V_LDM : V_STM);
}

Kind dsp(Insn& in, std::uint8_t operation)
{
    in.alu = operation;
    return Kind::Dsp;
}

} // namespace

Kind decode(std::uint32_t h1, std::uint32_t h2, Insn& in)
{
    in = Insn{};
    in.size = 4;
    // The FPU: coprocessors 10 and 11.
    if ((h1 & 0xFC00u) == 0xEC00u && (h2 & 0x0E00u) == 0x0A00u) {
        if ((h1 & 0xFF00u) == 0xEE00u) {
            return (h2 & 0x10u) ? decode_vfp_transfer(h1, h2, in) : decode_vfp_data(h1, h2, in);
        }
        if ((h1 & 0xFE00u) == 0xEC00u) {
            return decode_vfp_memory(h1, h2, in);
        }
        return Kind::None;
    }
    in.rn = u8(h1 & 0xFu);
    in.rd = u8(bits(h2, 11, 8));
    in.rm = u8(h2 & 0xFu);
    if ((h1 & 0xFF80u) == 0xFA80u && (h2 & 0xF080u) == 0xF000u) {
        // A5.3.13 and A5.3.14: parallel addition and subtraction.
        const std::uint32_t prefix = bits(h2, 5, 4);
        static const std::int8_t operations[8] = {P_ADD8, P_ADD16, P_ASX, -1, P_SUB8, P_SUB16, P_SAX, -1};
        const std::int8_t operation = operations[bits(h1, 6, 4)];
        if (prefix == 3 || operation < 0) {
            return Kind::None;
        }
        in.mode = u8((prefix + ((h2 & 0x40u) ? 3u : 0u)) | (static_cast<std::uint32_t>(operation) << 4));
        return dsp(in, PARALLEL);
    }
    if ((h1 & 0xFFC0u) == 0xFA80u && (h2 & 0xF0C0u) == 0xF080u) {
        // A5.3.15: saturating add and subtract, SEL (REV and CLZ are the Cortex-M3's).
        const std::uint32_t op1 = bits(h1, 5, 4);
        const std::uint32_t op2 = bits(h2, 5, 4);
        if (op1 == 0) {
            static const std::uint8_t saturating[4] = {QADD, QDADD, QSUB, QDSUB};
            return dsp(in, saturating[op2]);
        }
        if (op1 == 2 && op2 == 0) {
            return dsp(in, SEL);
        }
        return Kind::None;
    }
    if ((h1 & 0xFF80u) == 0xFA00u && (h2 & 0xF0C0u) == 0xF080u) {
        // A5.3.12: extend, with an addition unless Rn is the PC.
        const std::uint32_t op1 = bits(h1, 6, 4);
        if (op1 > 5) {
            return Kind::None;
        }
        static const std::uint8_t extends[6] = {X_SXTAH, X_UXTAH, X_SXTAB16, X_UXTAB16, X_SXTAB, X_UXTAB};
        in.mode = extends[op1];
        in.shift_n = u8(bits(h2, 5, 4) * 8u);
        return dsp(in, EXTEND_ADD);
    }
    if ((h1 & 0xFFF0u) == 0xEAC0u && (h2 & 0x8010u) == 0) {
        // PKHBT, PKHTB: the shift is LSL, or ASR (0 meaning 32) for PKHTB.
        const std::uint32_t amount = (bits(h2, 14, 12) << 2) | bits(h2, 7, 6);
        const bool tb = (h2 & 0x20u) != 0;
        in.mode = tb ? 1 : 0;
        in.shift_n = u8(tb && amount == 0 ? 32u : amount);
        return dsp(in, PKH);
    }
    if ((h1 & 0xFF70u) == 0xF320u && (h2 & 0xF0F0u) == 0) {
        // SSAT16, USAT16 (h1 bit 7 chooses).
        in.imm = h2 & 0xFu;
        return dsp(in, (h1 & 0x80u) ? USAT16 : SSAT16);
    }
    if ((h1 & 0xFF80u) == 0xFB00u && (h2 & 0xC0u) == 0) {
        // A5.3.16: multiplies (MUL, MLA and MLS are the Cortex-M3's).
        const std::uint32_t op1 = bits(h1, 6, 4);
        const std::uint32_t op2 = bits(h2, 5, 4);
        in.ra = u8(bits(h2, 15, 12));
        switch (op1) {
        case 1:
            in.mode = u8(op2);
            return dsp(in, SMULXY);
        case 2:
        case 4:
            if (op2 > 1) {
                return Kind::None;
            }
            in.mode = u8(op2 | (op1 == 4 ? 2u : 0u));
            return dsp(in, SMUAD);
        case 3:
            if (op2 > 1) {
                return Kind::None;
            }
            in.mode = u8(op2);
            return dsp(in, SMULWY);
        case 5:
        case 6:
            if (op2 > 1) {
                return Kind::None;
            }
            in.mode = u8(op2 | (op1 == 6 ? 2u : 0u));
            return dsp(in, SMMUL);
        case 7:
            return op2 == 0 ? dsp(in, USAD8) : Kind::None;
        default:
            return Kind::None;
        }
    }
    if ((h1 & 0xFF80u) == 0xFB80u) {
        // A5.3.17: long multiplies. RdLo is h2 15:12, RdHi is h2 11:8.
        const std::uint32_t op1 = bits(h1, 6, 4);
        const std::uint32_t op2 = bits(h2, 7, 4);
        in.rd = u8(bits(h2, 15, 12));
        in.ra = u8(bits(h2, 11, 8));
        if (op1 == 4 && (op2 & 0xCu) == 0x8u) {
            in.mode = u8(op2 & 3u);
            return dsp(in, SMLALXY);
        }
        if ((op1 == 4 || op1 == 5) && (op2 & 0xEu) == 0xCu) {
            in.mode = u8((op2 & 1u) | (op1 == 5 ? 2u : 0u));
            return dsp(in, SMLALD);
        }
        if (op1 == 6 && op2 == 6) {
            return dsp(in, UMAAL);
        }
    }
    return Kind::None;
}

unsigned cycles(const Insn& in, bool is_dsp)
{
    if (is_dsp) {
        return 1; // every DSP instruction, the multiplies included
    }
    switch (in.alu) {
    case V_MLA: case V_MLS: case V_NMLS: case V_NMLA:
    case V_FNMS: case V_FNMA: case V_FMA: case V_FMS:
        return 3;
    case V_DIV:
    case V_SQRT:
        return 14;
    case V_LDR: case V_STR: case V_MOV_PAIR:
        return 2;
    case V_LDM: case V_STM:
        return 1 + in.imm;
    default:
        return 1;
    }
}

} // namespace m4

namespace {

constexpr std::uint32_t UF_UNDEFINSTR = 1u << 16;
constexpr std::uint32_t UF_NOCP = 1u << 19;

// FPCCR bits (B3.2.21).
constexpr std::uint32_t FPCCR_LSPACT = 1u << 0;
constexpr std::uint32_t FPCCR_USER = 1u << 1;
constexpr std::uint32_t FPCCR_THREAD = 1u << 3;
constexpr std::uint32_t FPCCR_HFRDY = 1u << 4;
constexpr std::uint32_t FPCCR_MMRDY = 1u << 5;
constexpr std::uint32_t FPCCR_BFRDY = 1u << 6;
constexpr std::uint32_t FPCCR_MONRDY = 1u << 8;
constexpr std::uint32_t FPCCR_ASPEN = 1u << 31;
// What FPDSCR gives a new floating-point context: AHP, DN, FZ and the rounding mode.
constexpr std::uint32_t fpscr_defaults = 0x07C00000u;
constexpr std::uint32_t fpscr_bits = 0xF7C0009Fu; // the bits FPSCR has

inline std::int32_t s32(std::uint32_t x) { return static_cast<std::int32_t>(x); }
inline std::int32_t s16(std::uint32_t x) { return static_cast<std::int16_t>(x & 0xFFFFu); }
inline std::int32_t s8(std::uint32_t x) { return static_cast<std::int8_t>(x & 0xFFu); }

// SignedSatQ and UnsignedSatQ (A2.2.1): the value in `width` bits, and whether it was cut.
std::int64_t signed_sat(std::int64_t value, unsigned width, bool& saturated)
{
    const std::int64_t high = (std::int64_t{1} << (width - 1)) - 1;
    const std::int64_t low = -(std::int64_t{1} << (width - 1));
    if (value > high) {
        saturated = true;
        return high;
    }
    if (value < low) {
        saturated = true;
        return low;
    }
    return value;
}

std::int64_t unsigned_sat(std::int64_t value, unsigned width, bool& saturated)
{
    const std::int64_t high = (std::int64_t{1} << width) - 1;
    if (value > high) {
        saturated = true;
        return high;
    }
    if (value < 0) {
        saturated = true;
        return 0;
    }
    return value;
}

inline std::uint32_t rotate_right(std::uint32_t x, unsigned n)
{
    n &= 31u;
    return n == 0 ? x : (x >> n) | (x << (32 - n));
}

} // namespace

void Cpu::execute_dsp(const Insn& in)
{
    using namespace m4;
    if (!dsp_) {
        raise_usage_fault(UF_UNDEFINSTR, "undefined instruction (a DSP instruction, which the Cortex-M3 does not have)");
        return;
    }
    const std::uint32_t n = read_reg(in.rn);
    const std::uint32_t m = read_reg(in.rm);
    bool saturated = false;
    switch (in.alu) {
    case QADD:
    case QDADD:
    case QSUB:
    case QDSUB: {
        std::int64_t second = s32(n);
        if (in.alu == QDADD || in.alu == QDSUB) {
            second = signed_sat(2 * second, 32, saturated);
        }
        const std::int64_t result = in.alu == QADD || in.alu == QDADD ? s32(m) + second : s32(m) - second;
        write_reg(in.rd, static_cast<std::uint32_t>(signed_sat(result, 32, saturated)));
        q_ = q_ || saturated;
        return;
    }
    case PARALLEL: {
        const unsigned prefix = in.mode & 0xFu;
        const unsigned operation = in.mode >> 4;
        const bool is_signed = prefix < P_U;
        const unsigned lanes = operation >= P_ADD8 ? 4 : 2;
        const unsigned width = lanes == 4 ? 8 : 16;
        const std::uint32_t lane_mask = (1u << width) - 1u;
        auto lane = [&](std::uint32_t x, unsigned i) -> std::int32_t {
            const std::uint32_t v = (x >> (width * i)) & lane_mask;
            return is_signed ? (width == 8 ? s8(v) : s16(v)) : static_cast<std::int32_t>(v);
        };
        std::uint32_t result = 0;
        std::uint32_t ge = 0;
        for (unsigned i = 0; i < lanes; ++i) {
            // Which lane of Rm meets lane i of Rn, and whether they are added.
            unsigned other = i;
            bool add = operation == P_ADD16 || operation == P_ADD8;
            if (operation == P_ASX) {
                other = 1 - i;
                add = i == 1;
            } else if (operation == P_SAX) {
                other = 1 - i;
                add = i == 0;
            }
            const std::int32_t value = add ? lane(n, i) + lane(m, other) : lane(n, i) - lane(m, other);
            std::int32_t out = value;
            bool unused = false;
            switch (prefix) {
            case P_Q: out = static_cast<std::int32_t>(signed_sat(value, width, unused)); break;
            case P_UQ: out = static_cast<std::int32_t>(unsigned_sat(value, width, unused)); break;
            case P_SH:
            case P_UH: out = value >> 1; break;
            default: {
                // S and U set the GE flags: the lane's result is not negative, or
                // an unsigned addition carried out.
                const bool flag = is_signed || !add ? value >= 0 : value > static_cast<std::int32_t>(lane_mask);
                if (flag) {
                    ge |= (lanes == 4 ? 1u : 3u) << (lanes == 4 ? i : 2 * i);
                }
                break;
            }
            }
            result |= (static_cast<std::uint32_t>(out) & lane_mask) << (width * i);
        }
        if (prefix == P_S || prefix == P_U) {
            ge_ = ge;
        }
        write_reg(in.rd, result);
        return;
    }
    case SEL: {
        std::uint32_t mask = 0;
        for (unsigned i = 0; i < 4; ++i) {
            mask |= (ge_ & (1u << i)) ? 0xFFu << (8 * i) : 0u;
        }
        write_reg(in.rd, (n & mask) | (m & ~mask));
        return;
    }
    case EXTEND_ADD: {
        const std::uint32_t rotated = rotate_right(m, in.shift_n);
        const std::uint32_t base = in.rn == 15 ? 0u : n;
        std::uint32_t result = 0;
        switch (in.mode) {
        case X_SXTAH: result = base + static_cast<std::uint32_t>(s16(rotated)); break;
        case X_UXTAH: result = base + (rotated & 0xFFFFu); break;
        case X_SXTAB: result = base + static_cast<std::uint32_t>(s8(rotated)); break;
        case X_UXTAB: result = base + (rotated & 0xFFu); break;
        case X_SXTAB16:
            result = ((base + static_cast<std::uint32_t>(s8(rotated))) & 0xFFFFu)
                | (((base >> 16) + static_cast<std::uint32_t>(s8(rotated >> 16))) << 16);
            break;
        default: // X_UXTAB16
            result = ((base + (rotated & 0xFFu)) & 0xFFFFu) | (((base >> 16) + ((rotated >> 16) & 0xFFu)) << 16);
            break;
        }
        write_reg(in.rd, result);
        return;
    }
    case PKH: {
        std::uint32_t operand = 0;
        if (in.mode) { // PKHTB: arithmetic shift right
            operand = static_cast<std::uint32_t>(s32(m) >> (in.shift_n >= 32 ? 31 : in.shift_n));
            write_reg(in.rd, (n & 0xFFFF0000u) | (operand & 0xFFFFu));
        } else {
            operand = m << in.shift_n;
            write_reg(in.rd, (operand & 0xFFFF0000u) | (n & 0xFFFFu));
        }
        return;
    }
    case SSAT16:
    case USAT16: {
        std::uint32_t result = 0;
        for (unsigned i = 0; i < 2; ++i) {
            const std::int32_t value = s16(n >> (16 * i));
            const std::int64_t out = in.alu == SSAT16 ? signed_sat(value, in.imm + 1, saturated)
                                                      : unsigned_sat(value, in.imm, saturated);
            result |= (static_cast<std::uint32_t>(out) & 0xFFFFu) << (16 * i);
        }
        write_reg(in.rd, result);
        q_ = q_ || saturated;
        return;
    }
    case USAD8: {
        std::uint32_t sum = in.ra == 15 ? 0u : read_reg(in.ra);
        for (unsigned i = 0; i < 4; ++i) {
            sum += static_cast<std::uint32_t>(
                std::abs(static_cast<int>((n >> (8 * i)) & 0xFFu) - static_cast<int>((m >> (8 * i)) & 0xFFu)));
        }
        write_reg(in.rd, sum);
        return;
    }
    case SMULXY:
    case SMLALXY: {
        const std::int64_t product = static_cast<std::int64_t>(s16((in.mode & 2u) ? n >> 16 : n))
            * s16((in.mode & 1u) ? m >> 16 : m);
        if (in.alu == SMLALXY) {
            const std::uint64_t accumulator = (static_cast<std::uint64_t>(read_reg(in.ra)) << 32) | read_reg(in.rd);
            const std::uint64_t result = accumulator + static_cast<std::uint64_t>(product);
            write_reg(in.rd, static_cast<std::uint32_t>(result));
            write_reg(in.ra, static_cast<std::uint32_t>(result >> 32));
            return;
        }
        std::int64_t result = product;
        if (in.ra != 15) {
            result += s32(read_reg(in.ra));
            q_ = q_ || result != s32(static_cast<std::uint32_t>(result));
        }
        write_reg(in.rd, static_cast<std::uint32_t>(result));
        return;
    }
    case SMULWY: {
        const std::int64_t product = static_cast<std::int64_t>(s32(n)) * s16((in.mode & 1u) ? m >> 16 : m);
        if (in.ra == 15) {
            write_reg(in.rd, static_cast<std::uint32_t>(product >> 16));
            return;
        }
        const std::int64_t result = product + (static_cast<std::int64_t>(s32(read_reg(in.ra))) << 16);
        write_reg(in.rd, static_cast<std::uint32_t>(result >> 16));
        q_ = q_ || (result >> 16) != s32(static_cast<std::uint32_t>(result >> 16));
        return;
    }
    case SMUAD:
    case SMLALD: {
        const std::uint32_t operand = (in.mode & 1u) ? rotate_right(m, 16) : m;
        const std::int64_t product1 = static_cast<std::int64_t>(s16(n)) * s16(operand);
        const std::int64_t product2 = static_cast<std::int64_t>(s16(n >> 16)) * s16(operand >> 16);
        const std::int64_t both = (in.mode & 2u) ? product1 - product2 : product1 + product2;
        if (in.alu == SMLALD) {
            const std::uint64_t accumulator = (static_cast<std::uint64_t>(read_reg(in.ra)) << 32) | read_reg(in.rd);
            const std::uint64_t result = accumulator + static_cast<std::uint64_t>(both);
            write_reg(in.rd, static_cast<std::uint32_t>(result));
            write_reg(in.ra, static_cast<std::uint32_t>(result >> 32));
            return;
        }
        const std::int64_t result = both + (in.ra == 15 ? 0 : s32(read_reg(in.ra)));
        write_reg(in.rd, static_cast<std::uint32_t>(result));
        q_ = q_ || result != s32(static_cast<std::uint32_t>(result));
        return;
    }
    case SMMUL: {
        const std::int64_t product = static_cast<std::int64_t>(s32(n)) * s32(m);
        std::uint64_t result = static_cast<std::uint64_t>(product);
        if (in.ra != 15 || (in.mode & 2u)) {
            const std::uint64_t accumulator = static_cast<std::uint64_t>(read_reg(in.ra)) << 32;
            result = (in.mode & 2u) ? accumulator - static_cast<std::uint64_t>(product)
                                    : accumulator + static_cast<std::uint64_t>(product);
        }
        if (in.mode & 1u) {
            result += 0x80000000u;
        }
        write_reg(in.rd, static_cast<std::uint32_t>(result >> 32));
        return;
    }
    case UMAAL: {
        const std::uint64_t result = static_cast<std::uint64_t>(n) * m + read_reg(in.ra) + read_reg(in.rd);
        write_reg(in.rd, static_cast<std::uint32_t>(result));
        write_reg(in.ra, static_cast<std::uint32_t>(result >> 32));
        return;
    }
    default:
        raise_usage_fault(UF_UNDEFINSTR, "undefined instruction");
        return;
    }
}

// PreserveFPState (B1.5.7): the lazily reserved part of an exception frame is
// filled in before the first floating-point instruction of the handler.
void Cpu::preserve_fp_state()
{
    for (unsigned i = 0; i < 16; ++i) {
        bus_.write(fpcar_ + 4 * i, 4, s_[i]);
    }
    bus_.write(fpcar_ + 0x40, 4, fpscr_);
    fpccr_ &= ~FPCCR_LSPACT;
}

// ExecuteFPCheck (B1.5.7); false after a fault.
bool Cpu::fp_check()
{
    const std::uint32_t access = (cpacr_ >> 20) & 3u;
    const bool privileged = ipsr_ != 0 || !npriv_;
    if (!fpu_ || access == 0 || (access == 1 && !privileged)) {
        raise_usage_fault(UF_NOCP, fpu_ ? "floating-point instruction with the FPU disabled (CPACR)"
                                        : "coprocessor or floating-point instruction (the Cortex-M3 has none)");
        return false;
    }
    if (fpccr_ & FPCCR_LSPACT) {
        preserve_fp_state();
    }
    if ((fpccr_ & FPCCR_ASPEN) && !fpca_) {
        fpscr_ = (fpscr_ & ~fpscr_defaults) | (fpdscr_ & fpscr_defaults);
        fpca_ = true;
    }
    return true;
}

// The floating-point part of an exception frame (PushStack, B1.5.6): written
// now, or only reserved when lazy stacking is on.
bool Cpu::push_fp_frame(std::uint32_t frame)
{
    if (!(fpccr_ & (1u << 30))) { // LSPEN clear
        for (unsigned i = 0; i < 16; ++i) {
            if (!bus_.write(frame + 0x20 + 4 * i, 4, s_[i])) {
                return false;
            }
        }
        return bus_.write(frame + 0x60, 4, fpscr_);
    }
    // UpdateFPCCR: where the registers go later, and who was running.
    fpcar_ = frame + 0x20;
    const bool thread = ipsr_ == 0;
    fpccr_ = (fpccr_ & ~(FPCCR_USER | FPCCR_THREAD | FPCCR_HFRDY | FPCCR_MMRDY | FPCCR_BFRDY | FPCCR_MONRDY))
        | FPCCR_LSPACT | (npriv_ && thread ? FPCCR_USER : 0u) | (thread ? FPCCR_THREAD : 0u) | FPCCR_HFRDY
        | FPCCR_MMRDY | FPCCR_BFRDY;
    return true;
}

// PopStack's floating-point part (B1.5.8).
bool Cpu::pop_fp_frame(std::uint32_t frame)
{
    if (fpccr_ & FPCCR_LSPACT) {
        fpccr_ &= ~FPCCR_LSPACT; // never saved: the registers still hold the context
        return true;
    }
    for (unsigned i = 0; i < 16; ++i) {
        if (!bus_.read(frame + 0x20 + 4 * i, 4, s_[i])) {
            return false;
        }
    }
    std::uint32_t value = 0;
    if (!bus_.read(frame + 0x60, 4, value)) {
        return false;
    }
    fpscr_ = value & fpscr_bits;
    return true;
}

void Cpu::execute_vfp(const Insn& in)
{
    using namespace m4;
    if (!fp_check()) {
        return;
    }
    std::uint32_t& d = s_[in.rd & 31u];
    const std::uint32_t n = s_[in.rn & 31u];
    const std::uint32_t m = s_[in.rm & 31u];
    constexpr std::uint32_t sign = 0x80000000u;
    switch (in.alu) {
    case V_MLA: d = fp::add(d, fp::mul(n, m, fpscr_), fpscr_); return;
    case V_MLS: d = fp::add(d, fp::mul(n, m, fpscr_) ^ sign, fpscr_); return;
    case V_NMLA: d = fp::add(d ^ sign, fp::mul(n, m, fpscr_) ^ sign, fpscr_); return;
    case V_NMLS: d = fp::add(d ^ sign, fp::mul(n, m, fpscr_), fpscr_); return;
    case V_NMUL: d = fp::mul(n, m, fpscr_) ^ sign; return;
    case V_MUL: d = fp::mul(n, m, fpscr_); return;
    case V_ADD: d = fp::add(n, m, fpscr_); return;
    case V_SUB: d = fp::sub(n, m, fpscr_); return;
    case V_DIV: d = fp::div(n, m, fpscr_); return;
    case V_FMA: d = fp::mul_add(d, n, m, fpscr_); return;
    case V_FMS: d = fp::mul_add(d, n ^ sign, m, fpscr_); return;
    case V_FNMS: d = fp::mul_add(d ^ sign, n, m, fpscr_); return;
    case V_FNMA: d = fp::mul_add(d ^ sign, n ^ sign, m, fpscr_); return;
    case V_MOV_IMM: d = in.imm; return;
    case V_MOV: d = m; return;
    case V_ABS: d = m & ~sign; return;
    case V_NEG: d = m ^ sign; return;
    case V_SQRT: d = fp::sqrt(m, fpscr_); return;
    case V_CVT_FROM_HALF:
        d = fp::half_to_single(static_cast<std::uint16_t>(in.mode ? m >> 16 : m), fpscr_);
        return;
    case V_CVT_TO_HALF: {
        const std::uint32_t half = fp::single_to_half(m, fpscr_);
        d = in.mode ? (d & 0x0000FFFFu) | (half << 16) : (d & 0xFFFF0000u) | half;
        return;
    }
    case V_CMP:
    {
        const std::uint32_t flags = fp::compare(d, (in.mode & 2u) ? 0u : m, (in.mode & 1u) != 0, fpscr_);
        fpscr_ = (fpscr_ & 0x0FFFFFFFu) | flags;
        return;
    }
    case V_CVT_FROM_INT: d = fp::from_fixed(m, 32, 0, (in.mode & 1u) != 0, false, fpscr_); return;
    case V_CVT_TO_INT: d = fp::to_fixed(m, 32, 0, (in.mode & 1u) != 0, (in.mode & 2u) != 0, fpscr_); return;
    case V_CVT_FIXED: {
        const unsigned size = (in.mode & 4u) ? 32 : 16;
        const bool is_unsigned = (in.mode & 1u) != 0;
        d = (in.mode & 2u) ? fp::to_fixed(d, size, in.imm, is_unsigned, true, fpscr_)
                           : fp::from_fixed(d, size, in.imm, is_unsigned, true, fpscr_);
        return;
    }
    case V_MOV_CORE:
    case V_MOV_SCALAR:
        if (in.mode & 1u) {
            write_reg(in.rd, n);
        } else {
            s_[in.rn & 31u] = read_reg(in.rd);
        }
        return;
    case V_MSR:
        fpscr_ = read_reg(in.rd) & fpscr_bits;
        return;
    case V_MRS:
        if (in.rd == 15) { // VMRS APSR_nzcv, FPSCR
            n_ = (fpscr_ >> 31) & 1u;
            z_ = (fpscr_ >> 30) & 1u;
            c_ = (fpscr_ >> 29) & 1u;
            v_ = (fpscr_ >> 28) & 1u;
        } else {
            write_reg(in.rd, fpscr_);
        }
        return;
    case V_MOV_PAIR:
        if (in.mode & 1u) {
            write_reg(in.rd, s_[in.rm & 31u]);
            write_reg(in.ra, s_[(in.rm + 1u) & 31u]);
        } else {
            s_[in.rm & 31u] = read_reg(in.rd);
            s_[(in.rm + 1u) & 31u] = read_reg(in.ra);
        }
        return;
    case V_LDR:
    case V_STR: {
        const std::uint32_t base = in.rn == 15 ? ((pc_ + 4) & ~3u) : read_reg(in.rn);
        const std::uint32_t address = (in.mode & 2u) ? base + in.imm : base - in.imm;
        const unsigned words = (in.mode & 1u) ? 2 : 1;
        for (unsigned i = 0; i < words; ++i) {
            if (in.alu == V_LDR) {
                std::uint32_t value = 0;
                if (!mem_read(address + 4 * i, 4, value)) {
                    return;
                }
                s_[(in.rd + i) & 31u] = value;
            } else if (!mem_write(address + 4 * i, 4, s_[(in.rd + i) & 31u])) {
                return;
            }
        }
        return;
    }
    case V_LDM:
    case V_STM: {
        const std::uint32_t base = read_reg(in.rn);
        const std::uint32_t bytes = 4 * in.imm;
        const std::uint32_t address = (in.mode & 2u) ? base : base - bytes;
        for (std::uint32_t i = 0; i < in.imm; ++i) {
            if (in.alu == V_LDM) {
                std::uint32_t value = 0;
                if (!mem_read(address + 4 * i, 4, value)) {
                    return;
                }
                s_[(in.rd + i) & 31u] = value;
            } else if (!mem_write(address + 4 * i, 4, s_[(in.rd + i) & 31u])) {
                return;
            }
        }
        if (in.mode & 4u) {
            write_reg(in.rn, (in.mode & 2u) ? base + bytes : base - bytes);
        }
        return;
    }
    default:
        raise_usage_fault(UF_UNDEFINSTR, "undefined instruction (double precision: the FPU is single precision)");
        return;
    }
}

} // namespace simnodus::emu
