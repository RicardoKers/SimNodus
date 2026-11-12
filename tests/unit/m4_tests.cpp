// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Cortex-M4F core of experiment E-12 (SN-293, gate G12a): the FPU's
// arithmetic against the ARMv7-M pseudocode's special cases and against the
// host's IEEE 754 single precision in all four rounding modes, the DSP and
// FPU instructions, lazy stacking, and the decoder with random patterns.
//
//   m4_tests <repository-dir> [filter [strict]]
#include "test_support.hpp"

#include "emu/bus.hpp"
#include "emu/cpu.hpp"
#include "emu/softfloat32.hpp"

#include <cfenv>
#include <algorithm>
#include <chrono>
#include <utility>
#include <initializer_list>
#include <memory>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#if defined(_MSC_VER)
#pragma fenv_access(on)
#endif

using namespace simnodus::emu;

namespace {

std::string repository;
bool strict_benchmark = false;

constexpr std::uint32_t qnan = 0x7FC00000u;
constexpr std::uint32_t snan = 0x7FA00000u; // signaling, with a payload
constexpr std::uint32_t inf = 0x7F800000u;
constexpr std::uint32_t minus = 0x80000000u;
constexpr std::uint32_t one = 0x3F800000u;
constexpr std::uint32_t two = 0x40000000u;
constexpr std::uint32_t min_normal = 0x00800000u;
constexpr std::uint32_t max_finite = 0x7F7FFFFFu;
constexpr std::uint32_t min_denormal = 0x00000001u;

std::uint32_t mode(unsigned rounding)
{
    return rounding << fp::rmode_shift;
}

float as_float(std::uint32_t bits)
{
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}

std::uint32_t as_bits(float f)
{
    std::uint32_t bits;
    std::memcpy(&bits, &f, 4);
    return bits;
}

bool is_nan(std::uint32_t bits)
{
    return (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x7FFFFFu) != 0;
}

// Operands that find mistakes: every special value, the edges of the ranges,
// and random patterns; `near` gives a second operand close to the first, for
// cancellation and ties.
struct Operands {
    std::mt19937 random{20261111u};

    std::uint32_t any()
    {
        static const std::uint32_t special[] = {0, minus, one, one | minus, two, inf, inf | minus, qnan, snan,
            min_normal, min_normal | minus, max_finite, max_finite | minus, min_denormal, 0x007FFFFFu, 0x807FFFFFu,
            0x3F000000u, 0x3FC00000u, 0x4B000000u, 0x4B800000u, 0x4F000000u, 0xCF000000u, 0x4F800000u, 0x00FFFFFFu,
            0x33800000u, 0x7F000000u, 0x00800001u, 0x01000000u};
        const std::uint32_t pick = random() % 16;
        if (pick == 0) {
            return special[random() % (sizeof special / sizeof special[0])];
        }
        if (pick < 4) { // a small exponent range, so that sums and products stay in range and cancel
            return (random() & 0x807FFFFFu) | ((120u + random() % 16) << 23);
        }
        if (pick == 4) { // denormals and the smallest normals
            return (random() & 0x807FFFFFu) | ((random() % 3) << 23);
        }
        if (pick == 5) { // the largest numbers
            return (random() & 0x807FFFFFu) | ((252u + random() % 3) << 23);
        }
        return random();
    }

    std::uint32_t near(std::uint32_t a)
    {
        const std::uint32_t pick = random() % 8;
        if (pick == 0) {
            return a ^ minus;
        }
        if (pick == 1) {
            return (a ^ minus) + (random() % 5) - 2u;
        }
        if (pick == 2) {
            return a + (random() % 5) - 2u;
        }
        if (pick == 3) { // the same mantissa a few exponents away
            return (a & 0x807FFFFFu) | ((((a >> 23) + random() % 5 - 2u) & 0xFFu) << 23);
        }
        return any();
    }
};

const int host_rounding[4] = {FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO};

std::uint32_t host_flags()
{
    const int raised = std::fetestexcept(FE_ALL_EXCEPT);
    return ((raised & FE_INVALID) ? fp::IOC : 0u) | ((raised & FE_DIVBYZERO) ? fp::DZC : 0u)
        | ((raised & FE_OVERFLOW) ? fp::OFC : 0u) | ((raised & FE_UNDERFLOW) ? fp::UFC : 0u)
        | ((raised & FE_INEXACT) ? fp::IXC : 0u);
}

int reported = 0;

void report(const char* what, unsigned rounding, std::uint32_t a, std::uint32_t b, std::uint32_t c, std::uint32_t got,
    std::uint32_t want, std::uint32_t got_flags, std::uint32_t want_flags)
{
    if (reported++ < 20) {
        std::printf("  %s mode %u: %08X %08X %08X -> %08X flags %02X, the host %08X flags %02X\n", what, rounding,
            static_cast<unsigned>(a), static_cast<unsigned>(b), static_cast<unsigned>(c), static_cast<unsigned>(got),
            static_cast<unsigned>(got_flags), static_cast<unsigned>(want), static_cast<unsigned>(want_flags));
    }
}

// Value and flags against the host. A NaN result only has to be a NaN (the
// host's choice of NaN is not ARM's; ARM's is checked by the directed cases).
// Underflow: ARM detects tininess before rounding, the host after it, so a
// result rounded up to the smallest normal number is flagged by ARM alone.
bool same(std::uint32_t got, std::uint32_t got_flags, std::uint32_t want, std::uint32_t want_flags, bool check_flags)
{
    if (is_nan(want)) {
        return is_nan(got) && (!check_flags || (got_flags & fp::IOC) == (want_flags & fp::IOC));
    }
    if (got != want) {
        return false;
    }
    if (!check_flags) {
        return true;
    }
    std::uint32_t mask = fp::IOC | fp::DZC | fp::OFC | fp::UFC | fp::IXC;
    if ((got & 0x7FFFFFFFu) == min_normal) {
        mask &= ~fp::UFC;
    }
    return (got_flags & mask) == (want_flags & mask);
}

} // namespace

TEST_CASE("m4: the FPU's special cases are those of the ARM pseudocode")
{
    std::uint32_t f = 0;
    // NaNs: a signaling one is quieted and invalid; signaling before quiet, then the first operand.
    CHECK(fp::add(snan, one, f) == (snan | 0x400000u));
    CHECK(f == fp::IOC);
    f = 0;
    CHECK(fp::add(qnan | 5u, snan, f) == (snan | 0x400000u)); // the signaling one wins, though second
    CHECK(f == fp::IOC);
    f = 0;
    CHECK(fp::add(qnan | 5u, qnan | 9u, f) == (qnan | 5u));
    CHECK(f == 0);
    CHECK(fp::mul(one, qnan | 9u | minus, f) == (qnan | 9u | minus)); // payload and sign kept
    f = fp::DN;
    CHECK(fp::add(qnan | 5u, one, f) == fp::default_nan); // default NaN mode
    CHECK(f == fp::DN);
    CHECK(fp::add(snan | minus, one, f) == fp::default_nan);
    CHECK(f == (fp::DN | fp::IOC));
    // Invalid operations give the default (positive, quiet) NaN.
    f = 0;
    CHECK(fp::add(inf, inf | minus, f) == fp::default_nan);
    CHECK(fp::sub(inf, inf, f) == fp::default_nan);
    CHECK(fp::mul(inf, 0, f) == fp::default_nan);
    CHECK(fp::div(0, minus, f) == fp::default_nan);
    CHECK(fp::div(inf, inf | minus, f) == fp::default_nan);
    CHECK(fp::sqrt(one | minus, f) == fp::default_nan);
    CHECK(f == fp::IOC);
    f = 0;
    CHECK(fp::sub(qnan | minus | 3u, one, f) == (qnan | minus | 3u)); // a NaN's sign is not flipped by the subtraction
    CHECK(fp::sqrt(minus, f) == minus); // -0
    CHECK(fp::sqrt(inf, f) == inf);
    CHECK(f == 0);
    CHECK(fp::div(one, 0, f) == inf);
    CHECK(f == fp::DZC);
    f = 0;
    CHECK(fp::div(one | minus, 0, f) == (inf | minus));
    CHECK(fp::div(inf, 0, f) == inf); // not a division by zero
    f = 0;
    CHECK(fp::div(inf, 0, f) == inf);
    CHECK(f == 0);
    // Zeros: x - x is +0, or -0 when rounding toward minus infinity.
    CHECK(fp::sub(one, one, f) == 0);
    CHECK(fp::add(minus, minus, f) == minus);
    CHECK(fp::add(0, minus, f) == 0);
    f = mode(fp::MinusInfinity);
    CHECK(fp::sub(one, one, f) == minus);
    CHECK(fp::add(0, minus, f) == minus);
    // Overflow by the rounding mode.
    f = 0;
    CHECK(fp::add(max_finite, max_finite, f) == inf);
    CHECK(f == (fp::OFC | fp::IXC));
    f = mode(fp::Zero);
    CHECK(fp::add(max_finite, max_finite, f) == max_finite);
    f = mode(fp::MinusInfinity);
    CHECK(fp::mul(max_finite, two, f) == max_finite);
    CHECK(fp::mul(max_finite | minus, two, f) == (inf | minus));
    // Underflow: tiny before rounding and inexact; an exact denormal is not one.
    f = 0;
    CHECK(fp::div(min_normal, two, f) == 0x00400000u);
    CHECK(f == 0);
    CHECK(fp::mul(min_denormal, 0x3F000000u, f) == 0); // half of the smallest: a tie to even, zero
    CHECK(f == (fp::UFC | fp::IXC));
    f = 0;
    CHECK(fp::mul(0x007FFFFFu, 0x3F800001u, f) == min_normal); // rounds up to the smallest normal: still an underflow
    CHECK(f == (fp::UFC | fp::IXC));
    // Flush to zero: denormal operands are zero (IDC), tiny results are zero (UFC alone).
    f = fp::FZ;
    CHECK(fp::add(min_denormal, one, f) == one);
    CHECK(f == (fp::FZ | fp::IDC));
    f = fp::FZ;
    CHECK(fp::mul(min_normal, 0x3F000000u, f) == 0);
    CHECK(f == (fp::FZ | fp::UFC));
    f = fp::FZ;
    CHECK(fp::mul(min_normal | minus, 0x3F000000u, f) == minus);
    // Fused multiply-add: one rounding. 1 + 2^-23 squared is 1 + 2^-22 + 2^-46:
    // minus (1 + 2^-22) leaves 2^-46, which the two-step form loses.
    f = 0;
    const std::uint32_t x = 0x3F800001u;
    CHECK(fp::mul_add(0xBF800002u, x, x, f) == 0x28800000u); // 2^-46
    CHECK(f == 0);
    CHECK(fp::add(0xBF800002u, fp::mul(x, x, f), f) == 0);
    f = 0;
    CHECK(fp::mul_add(qnan | 7u, inf, 0, f) == fp::default_nan); // a quiet NaN addend does not hide infinity times zero
    CHECK(f == fp::IOC);
    f = 0;
    CHECK(fp::mul_add(snan, inf, 0, f) == (snan | 0x400000u));
    CHECK(fp::mul_add(inf, inf, one | minus, f) == fp::default_nan); // inf - inf
    f = 0;
    CHECK(fp::mul_add(one, qnan | 1u, snan | 2u, f) == (snan | 2u | 0x400000u));
    CHECK(fp::mul_add(minus, 0, one, f) == 0);      // -0 + (+0)
    CHECK(fp::mul_add(minus, 0, one | minus, f) == minus); // -0 + (-0)
    CHECK(fp::mul_add(one, 0, two, f) == one);
    // Comparison: N Z C V.
    f = 0;
    CHECK(fp::compare(one, two, false, f) == 0x80000000u);
    CHECK(fp::compare(two, one, false, f) == 0x20000000u);
    CHECK(fp::compare(one, one, false, f) == 0x60000000u);
    CHECK(fp::compare(0, minus, false, f) == 0x60000000u);
    CHECK(fp::compare(one | minus, 0, false, f) == 0x80000000u);
    CHECK(fp::compare(inf | minus, max_finite | minus, false, f) == 0x80000000u);
    CHECK(fp::compare(qnan, one, false, f) == 0x30000000u);
    CHECK(f == 0);
    CHECK(fp::compare(qnan, one, true, f) == 0x30000000u); // VCMPE
    CHECK(f == fp::IOC);
    f = 0;
    CHECK(fp::compare(one, snan, false, f) == 0x30000000u);
    CHECK(f == fp::IOC);
    // To integers: saturation is an invalid operation, a lost fraction is inexact.
    f = 0;
    CHECK(fp::to_fixed(0x4039999Au, 32, 0, false, true, f) == 2); // 2.9 toward zero
    CHECK(f == fp::IXC);
    f = 0;
    CHECK(fp::to_fixed(0xC039999Au, 32, 0, false, true, f) == 0xFFFFFFFEu);
    CHECK(fp::to_fixed(0x40200000u, 32, 0, false, false, f) == 2); // 2.5 to nearest even
    CHECK(fp::to_fixed(0x40600000u, 32, 0, false, false, f) == 4); // 3.5
    CHECK(fp::to_fixed(0xBF000000u, 32, 0, true, true, f) == 0);   // -0.5 unsigned: 0, only inexact
    CHECK(f == fp::IXC);
    f = 0;
    CHECK(fp::to_fixed(0xBF800000u, 32, 0, true, true, f) == 0); // -1 unsigned: saturated
    CHECK(f == fp::IOC);
    f = 0;
    CHECK(fp::to_fixed(0x4F000000u, 32, 0, false, true, f) == 0x7FFFFFFFu); // 2^31
    CHECK(f == fp::IOC);
    f = 0;
    CHECK(fp::to_fixed(0xCF000000u, 32, 0, false, true, f) == 0x80000000u); // -2^31 fits
    CHECK(f == 0);
    CHECK(fp::to_fixed(0x4F800000u, 32, 0, true, true, f) == 0xFFFFFFFFu);
    CHECK(fp::to_fixed(qnan, 32, 0, false, true, f) == 0);
    CHECK(fp::to_fixed(inf | minus, 32, 0, false, true, f) == 0x80000000u);
    f = 0;
    CHECK(fp::to_fixed(0x3FC00000u, 32, 8, false, true, f) == 0x180u); // 1.5 with 8 fraction bits
    CHECK(fp::to_fixed(0x43000000u, 16, 8, false, true, f) == 0x7FFFu); // 128.0 does not fit 16 bits with 8 fraction bits
    CHECK(f == fp::IOC);
    f = 0;
    CHECK(fp::to_fixed(0xC3000000u, 16, 8, false, true, f) == 0xFFFF8000u); // -128.0 fits, sign extended
    CHECK(f == 0);
    // From integers.
    CHECK(fp::from_fixed(1, 32, 0, false, false, f) == one);
    CHECK(fp::from_fixed(0xFFFFFFFFu, 32, 0, false, false, f) == (one | minus));
    CHECK(fp::from_fixed(0xFFFFFFFFu, 32, 0, true, false, f) == 0x4F800000u); // rounds up to 2^32
    CHECK(f == fp::IXC);
    f = mode(fp::Zero);
    CHECK(fp::from_fixed(0xFFFFFFFFu, 32, 0, true, false, f) == 0x4F7FFFFFu);
    CHECK(fp::from_fixed(0xFFFFFFFFu, 32, 0, true, true, f) == 0x4F800000u); // fixed point: always to nearest
    f = 0;
    CHECK(fp::from_fixed(0x180u, 32, 8, false, true, f) == 0x3FC00000u);
    CHECK(fp::from_fixed(0x8000u, 16, 15, false, true, f) == (one | minus));
    CHECK(fp::from_fixed(0x8000u, 16, 15, true, true, f) == one);
    CHECK(fp::from_fixed(0, 32, 0, false, false, f) == 0);
    // Half precision.
    CHECK(fp::half_to_single(0x3C00, f) == one);
    CHECK(fp::half_to_single(0xC000, f) == (two | minus));
    CHECK(fp::half_to_single(0x0001, f) == 0x33800000u); // the smallest denormal, 2^-24
    CHECK(fp::half_to_single(0x7C00, f) == inf);
    CHECK(fp::half_to_single(0x7E01, f) == (qnan | (1u << 13)));
    CHECK(f == 0);
    CHECK(fp::half_to_single(0x7D00, f) == (0x7FC00000u | (0x100u << 13))); // signaling: quieted
    CHECK(f == fp::IOC);
    f = fp::AHP;
    CHECK(fp::half_to_single(0x7C00, f) == 0x47800000u); // no infinity in the alternative format: 65536
    f = 0;
    CHECK(fp::single_to_half(one, f) == 0x3C00);
    CHECK(fp::single_to_half(0x33800000u, f) == 0x0001);
    CHECK(f == 0);
    CHECK(fp::single_to_half(0x33000000u, f) == 0); // 2^-25: a tie, to even
    CHECK(f == (fp::UFC | fp::IXC));
    f = 0;
    CHECK(fp::single_to_half(0x47800000u, f) == 0x7C00); // 65536 overflows
    CHECK(f == (fp::OFC | fp::IXC));
    f = fp::AHP;
    CHECK(fp::single_to_half(0x47800000u, f) == 0x7C00); // and is a number in the alternative format
    CHECK(f == fp::AHP);
    CHECK(fp::single_to_half(0x48800000u, f) == 0x7FFF); // 262144 is past it
    CHECK(f == (fp::AHP | fp::IOC));
    f = fp::AHP;
    CHECK(fp::single_to_half(qnan, f) == 0);
    f = 0;
    CHECK(fp::single_to_half(qnan | minus | (5u << 13), f) == (0xFE00 | 5));
    CHECK(fp::single_to_half(inf | minus, f) == 0xFC00);
    // The immediate of VMOV.
    CHECK(fp::expand_imm(0x70) == one);
    CHECK(fp::expand_imm(0x00) == two);
    CHECK(fp::expand_imm(0xF0) == (one | minus));
    CHECK(fp::expand_imm(0x60) == 0x3F000000u); // 0.5
    CHECK(fp::expand_imm(0x24) == 0x41200000u); // 10.0
}

TEST_CASE("m4-diff: FPU arithmetic equals the host's IEEE single precision in the four rounding modes")
{
    // G12a.2: at least 1 000 000 operand pairs for each operation.
    Operands operands;
    const int pairs = 250000; // in each of the four modes
    long long compared = 0;
    int differences = 0;
    for (unsigned rounding = 0; rounding < 4; ++rounding) {
        std::fesetround(host_rounding[rounding]);
        for (int i = 0; i < pairs; ++i) {
            const std::uint32_t a = operands.any();
            const std::uint32_t b = operands.near(a);
            const std::uint32_t c = operands.near(a);
            volatile float fa = as_float(a);
            volatile float fb = as_float(b);
            volatile float fc = as_float(c);
            struct Case {
                const char* name;
                std::uint32_t got;
                std::uint32_t got_flags;
                std::uint32_t want;
                std::uint32_t want_flags;
                bool flags;
            };
            Case cases[6];
            std::uint32_t f = mode(rounding);
            volatile float r = 0;
            auto host = [&](const char* name, std::uint32_t got, bool flags, auto&& compute) {
                Case out{name, got, f & 0x9Fu, 0, 0, flags};
                f = mode(rounding);
                std::feclearexcept(FE_ALL_EXCEPT);
                r = compute();
                out.want_flags = host_flags();
                out.want = as_bits(r);
                return out;
            };
            cases[0] = host("add", fp::add(a, b, f), true, [&] { return fa + fb; });
            cases[1] = host("sub", fp::sub(a, b, f), true, [&] { return fa - fb; });
            cases[2] = host("mul", fp::mul(a, b, f), true, [&] { return fa * fb; });
            cases[3] = host("div", fp::div(a, b, f), true, [&] { return fa / fb; });
            cases[4] = host("sqrt", fp::sqrt(a, f), true, [&] { return std::sqrt(static_cast<float>(fa)); });
            // The C library's fma: its value is correctly rounded; its flags are not relied on.
            cases[5] = host("fma", fp::mul_add(c, a, b, f), false,
                [&] { return std::fma(static_cast<float>(fa), static_cast<float>(fb), static_cast<float>(fc)); });
            for (const Case& k : cases) {
                ++compared;
                if (!same(k.got, k.got_flags, k.want, k.want_flags, k.flags)) {
                    ++differences;
                    report(k.name, rounding, a, b, c, k.got, k.want, k.got_flags, k.want_flags);
                }
            }
        }
    }
    std::fesetround(FE_TONEAREST);
    std::printf("  %lld results compared with the host, %d differences\n", compared, differences);
    CHECK(differences == 0);
}

TEST_CASE("m4-diff: FPU conversions equal the host's in the four rounding modes")
{
    Operands operands;
    int differences = 0;
    long long compared = 0;
    for (unsigned rounding = 0; rounding < 4; ++rounding) {
        std::fesetround(host_rounding[rounding]);
        for (int i = 0; i < 250000; ++i) {
            std::uint32_t a = operands.any();
            if (i % 4 == 0) { // values around the integer ranges
                a = (a & 0x807FFFFFu) | ((126u + operands.random() % 36) << 23);
            }
            volatile float fa = as_float(a);
            std::uint32_t f = mode(rounding);
            // Float to integer with FPSCR's mode (VCVTR) and toward zero (VCVT), where the value is in range.
            const double exact = static_cast<double>(static_cast<float>(fa));
            if (!is_nan(a) && exact > -2147483648.0 && exact < 2147483647.0) {
                const std::int32_t want_mode = static_cast<std::int32_t>(std::nearbyint(exact));
                const std::int32_t want_zero = static_cast<std::int32_t>(std::trunc(exact));
                compared += 2;
                if (fp::to_fixed(a, 32, 0, false, false, f) != static_cast<std::uint32_t>(want_mode)
                    || fp::to_fixed(a, 32, 0, false, true, f) != static_cast<std::uint32_t>(want_zero)) {
                    ++differences;
                    report("to int", rounding, a, 0, 0, fp::to_fixed(a, 32, 0, false, false, f),
                        static_cast<std::uint32_t>(want_mode), 0, 0);
                }
            }
            // Integer to float with FPSCR's mode.
            const std::uint32_t n = operands.random() >> (operands.random() % 32);
            volatile std::int32_t sn = static_cast<std::int32_t>(n);
            volatile std::uint32_t un = n;
            volatile float from_signed = static_cast<float>(sn);
            volatile float from_unsigned = static_cast<float>(un);
            f = mode(rounding);
            compared += 2;
            if (fp::from_fixed(n, 32, 0, false, false, f) != as_bits(from_signed)
                || fp::from_fixed(n, 32, 0, true, false, f) != as_bits(from_unsigned)) {
                ++differences;
                report("from int", rounding, n, 0, 0, fp::from_fixed(n, 32, 0, false, false, f), as_bits(from_signed), 0, 0);
            }
        }
    }
    std::fesetround(FE_TONEAREST);
    std::printf("  %lld conversions compared with the host, %d differences\n", compared, differences);
    CHECK(differences == 0);
}

// ---- The core: a bare Cortex-M4F with flash, SRAM and the SCS ----

namespace {

constexpr std::uint32_t code_base = 0x08000100;
constexpr std::uint32_t fault_handler = 0x08000200;
constexpr std::uint32_t irq_handler = 0x08000300;
constexpr std::uint32_t stack_top = 0x20004000;
constexpr std::uint32_t ram = 0x20000100;

struct Core {
    Bus bus;
    Cpu cpu{bus};

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    explicit Core(const std::vector<std::uint16_t>& code, Cpu::Architecture architecture = Cpu::Architecture::CortexM4F)
    {
        bus.add_memory("flash", 0x08000000, 0x10000, false);
        bus.add_alias("boot alias", 0, "flash");
        bus.add_memory("SRAM", 0x20000000, 0x5000, true);
        bus.add_device(0xE000E000, 0x1000, &cpu.scs());
        std::vector<std::uint32_t> vectors(16 + 16, irq_handler | 1);
        vectors[0] = stack_top;
        vectors[1] = code_base | 1;
        for (int fault = 2; fault <= 6; ++fault) {
            vectors[static_cast<std::size_t>(fault)] = fault_handler | 1;
        }
        bus.load(0x08000000, reinterpret_cast<const std::uint8_t*>(vectors.data()),
            static_cast<std::uint32_t>(vectors.size() * 4));
        const std::uint16_t loop = 0xE7FE; // b .
        bus.load(fault_handler, reinterpret_cast<const std::uint8_t*>(&loop), 2);
        bus.load(irq_handler, reinterpret_cast<const std::uint8_t*>(&loop), 2);
        load(code_base, code);
        cpu.set_architecture(architecture);
        cpu.reset();
    }

    void load(std::uint32_t address, const std::vector<std::uint16_t>& words)
    {
        bus.load(address, reinterpret_cast<const std::uint8_t*>(words.data()), static_cast<std::uint32_t>(words.size() * 2));
        cpu.invalidate_code_cache();
    }
    void steps(int n)
    {
        for (int i = 0; i < n; ++i) {
            cpu.step_instruction();
        }
    }
    std::uint32_t r(int n) const { return cpu.reg(n); }
    void set(int n, std::uint32_t v) { cpu.set_reg(n, v); }
    std::uint32_t word(std::uint32_t address)
    {
        std::uint32_t value = 0;
        bus.read(address, 4, value);
        return value;
    }
    void poke(std::uint32_t address, std::uint32_t value) { bus.write(address, 4, value); }
    // The FPU on for every mode (CPACR's CP10 and CP11), as a start-up file does.
    void enable_fpu() { poke(0xE000ED88, 0x00F00000u); }
    bool faulted() const { return cpu.pc() == fault_handler; }
};

constexpr std::uint32_t Q = 1u << 27;

std::int64_t s32(std::uint32_t x) { return static_cast<std::int32_t>(x); }
std::int64_t s16(std::uint32_t x) { return static_cast<std::int16_t>(x & 0xFFFFu); }
std::int64_t s8(std::uint32_t x) { return static_cast<std::int8_t>(x & 0xFFu); }
std::uint32_t ror32(std::uint32_t x, unsigned n) { return n == 0 ? x : (x >> n) | (x << (32 - n)); }

std::int64_t clamp(std::int64_t v, std::int64_t low, std::int64_t high, bool& cut)
{
    if (v < low) {
        cut = true;
        return low;
    }
    if (v > high) {
        cut = true;
        return high;
    }
    return v;
}

// What an instruction with Rd = r0, Rn = r1, Rm = r2, Ra = r3 (long multiplies:
// RdLo = r0, RdHi = r1, Rn = r2, Rm = r3) must give, written from the
// manual's pseudocode on its own, apart from the core's code.
struct In {
    std::uint32_t r0, r1, r2, r3, ge;
};
struct Out {
    std::uint32_t r0;
    std::uint32_t r1;
    bool q = false;
    int ge = -1; // -1: unchanged
};

std::uint32_t halves(std::int64_t low, std::int64_t high)
{
    return (static_cast<std::uint32_t>(low) & 0xFFFFu) | ((static_cast<std::uint32_t>(high) & 0xFFFFu) << 16);
}

std::uint32_t bytes4(const std::int64_t b[4])
{
    std::uint32_t out = 0;
    for (int i = 0; i < 4; ++i) {
        out |= (static_cast<std::uint32_t>(b[i]) & 0xFFu) << (8 * i);
    }
    return out;
}

struct DspCase {
    const char* name;
    std::uint16_t h1, h2;
    Out (*reference)(const In&);
};

const DspCase dsp_cases[] = {
    {"qadd", 0xFA82, 0xF081, [](const In& i) { Out o{0, i.r1}; o.r0 = static_cast<std::uint32_t>(clamp(s32(i.r1) + s32(i.r2), INT32_MIN, INT32_MAX, o.q)); return o; }},
    {"qdadd", 0xFA82, 0xF091, [](const In& i) { Out o{0, i.r1}; const std::int64_t twice = clamp(2 * s32(i.r2), INT32_MIN, INT32_MAX, o.q); o.r0 = static_cast<std::uint32_t>(clamp(s32(i.r1) + twice, INT32_MIN, INT32_MAX, o.q)); return o; }},
    {"qsub", 0xFA82, 0xF0A1, [](const In& i) { Out o{0, i.r1}; o.r0 = static_cast<std::uint32_t>(clamp(s32(i.r1) - s32(i.r2), INT32_MIN, INT32_MAX, o.q)); return o; }},
    {"qdsub", 0xFA82, 0xF0B1, [](const In& i) { Out o{0, i.r1}; const std::int64_t twice = clamp(2 * s32(i.r2), INT32_MIN, INT32_MAX, o.q); o.r0 = static_cast<std::uint32_t>(clamp(s32(i.r1) - twice, INT32_MIN, INT32_MAX, o.q)); return o; }},
    {"sadd16", 0xFA91, 0xF002, [](const In& i) { const std::int64_t lo = s16(i.r1) + s16(i.r2), hi = s16(i.r1 >> 16) + s16(i.r2 >> 16); Out o{halves(lo, hi), i.r1}; o.ge = (lo >= 0 ? 3 : 0) | (hi >= 0 ? 12 : 0); return o; }},
    {"qadd8", 0xFA81, 0xF012, [](const In& i) { std::int64_t b[4]; bool cut = false; for (int k = 0; k < 4; ++k) b[k] = clamp(s8(i.r1 >> (8 * k)) + s8(i.r2 >> (8 * k)), -128, 127, cut); return Out{bytes4(b), i.r1}; }},
    {"uhsub16", 0xFAD1, 0xF062, [](const In& i) { const std::int64_t lo = (static_cast<std::int64_t>(i.r1 & 0xFFFF) - (i.r2 & 0xFFFF)) >> 1, hi = (static_cast<std::int64_t>(i.r1 >> 16) - (i.r2 >> 16)) >> 1; return Out{halves(lo, hi), i.r1}; }},
    {"sasx", 0xFAA1, 0xF002, [](const In& i) { const std::int64_t lo = s16(i.r1) - s16(i.r2 >> 16), hi = s16(i.r1 >> 16) + s16(i.r2); Out o{halves(lo, hi), i.r1}; o.ge = (lo >= 0 ? 3 : 0) | (hi >= 0 ? 12 : 0); return o; }},
    {"ssax", 0xFAE1, 0xF002, [](const In& i) { const std::int64_t lo = s16(i.r1) + s16(i.r2 >> 16), hi = s16(i.r1 >> 16) - s16(i.r2); Out o{halves(lo, hi), i.r1}; o.ge = (lo >= 0 ? 3 : 0) | (hi >= 0 ? 12 : 0); return o; }},
    {"uadd8", 0xFA81, 0xF042, [](const In& i) { std::int64_t b[4]; int ge = 0; for (int k = 0; k < 4; ++k) { b[k] = ((i.r1 >> (8 * k)) & 0xFF) + ((i.r2 >> (8 * k)) & 0xFF); ge |= b[k] >= 0x100 ? 1 << k : 0; } Out o{bytes4(b), i.r1}; o.ge = ge; return o; }},
    {"usub8", 0xFAC1, 0xF042, [](const In& i) { std::int64_t b[4]; int ge = 0; for (int k = 0; k < 4; ++k) { b[k] = static_cast<std::int64_t>((i.r1 >> (8 * k)) & 0xFF) - ((i.r2 >> (8 * k)) & 0xFF); ge |= b[k] >= 0 ? 1 << k : 0; } Out o{bytes4(b), i.r1}; o.ge = ge; return o; }},
    {"uqadd16", 0xFA91, 0xF052, [](const In& i) { bool cut = false; return Out{halves(clamp(static_cast<std::int64_t>(i.r1 & 0xFFFF) + (i.r2 & 0xFFFF), 0, 65535, cut), clamp(static_cast<std::int64_t>(i.r1 >> 16) + (i.r2 >> 16), 0, 65535, cut)), i.r1}; }},
    {"shadd8", 0xFA81, 0xF022, [](const In& i) { std::int64_t b[4]; for (int k = 0; k < 4; ++k) b[k] = (s8(i.r1 >> (8 * k)) + s8(i.r2 >> (8 * k))) >> 1; return Out{bytes4(b), i.r1}; }},
    {"sel", 0xFAA1, 0xF082, [](const In& i) { std::uint32_t out = 0; for (int k = 0; k < 4; ++k) out |= (((i.ge >> k) & 1u) ? i.r1 : i.r2) & (0xFFu << (8 * k)); return Out{out, i.r1}; }},
    {"sxtah ror 8", 0xFA01, 0xF092, [](const In& i) { return Out{i.r1 + static_cast<std::uint32_t>(s16(ror32(i.r2, 8))), i.r1}; }},
    {"uxtab16", 0xFA31, 0xF082, [](const In& i) { return Out{halves((i.r1 & 0xFFFF) + (i.r2 & 0xFF), (i.r1 >> 16) + ((i.r2 >> 16) & 0xFF)), i.r1}; }},
    {"sxtb16", 0xFA2F, 0xF082, [](const In& i) { return Out{halves(s8(i.r2), s8(i.r2 >> 16)), i.r1}; }},
    {"uxtab ror 24", 0xFA51, 0xF0B2, [](const In& i) { return Out{i.r1 + (ror32(i.r2, 24) & 0xFFu), i.r1}; }},
    {"pkhbt lsl 8", 0xEAC1, 0x2002, [](const In& i) { return Out{(i.r1 & 0xFFFFu) | ((i.r2 << 8) & 0xFFFF0000u), i.r1}; }},
    {"pkhtb asr 16", 0xEAC1, 0x4022, [](const In& i) { return Out{(i.r1 & 0xFFFF0000u) | (static_cast<std::uint32_t>(s32(i.r2) >> 16) & 0xFFFFu), i.r1}; }},
    {"ssat16 8", 0xF321, 0x0007, [](const In& i) { Out o{0, i.r1}; o.r0 = halves(clamp(s16(i.r1), -128, 127, o.q), clamp(s16(i.r1 >> 16), -128, 127, o.q)); return o; }},
    {"usat16 4", 0xF3A1, 0x0004, [](const In& i) { Out o{0, i.r1}; o.r0 = halves(clamp(s16(i.r1), 0, 15, o.q), clamp(s16(i.r1 >> 16), 0, 15, o.q)); return o; }},
    {"usad8", 0xFB71, 0xF002, [](const In& i) { std::uint32_t sum = 0; for (int k = 0; k < 4; ++k) { const int d = static_cast<int>((i.r1 >> (8 * k)) & 0xFF) - static_cast<int>((i.r2 >> (8 * k)) & 0xFF); sum += static_cast<std::uint32_t>(d < 0 ? -d : d); } return Out{sum, i.r1}; }},
    {"usada8", 0xFB71, 0x3002, [](const In& i) { std::uint32_t sum = i.r3; for (int k = 0; k < 4; ++k) { const int d = static_cast<int>((i.r1 >> (8 * k)) & 0xFF) - static_cast<int>((i.r2 >> (8 * k)) & 0xFF); sum += static_cast<std::uint32_t>(d < 0 ? -d : d); } return Out{sum, i.r1}; }},
    {"smulbb", 0xFB11, 0xF002, [](const In& i) { return Out{static_cast<std::uint32_t>(s16(i.r1) * s16(i.r2)), i.r1}; }},
    {"smultb", 0xFB11, 0xF022, [](const In& i) { return Out{static_cast<std::uint32_t>(s16(i.r1 >> 16) * s16(i.r2)), i.r1}; }},
    {"smlabt", 0xFB11, 0x3012, [](const In& i) { const std::int64_t v = s16(i.r1) * s16(i.r2 >> 16) + s32(i.r3); Out o{static_cast<std::uint32_t>(v), i.r1}; o.q = v != s32(o.r0); return o; }},
    {"smulwb", 0xFB31, 0xF002, [](const In& i) { return Out{static_cast<std::uint32_t>((s32(i.r1) * s16(i.r2)) >> 16), i.r1}; }},
    {"smlawt", 0xFB31, 0x3012, [](const In& i) { const std::int64_t v = (s32(i.r1) * s16(i.r2 >> 16) + s32(i.r3) * 65536) >> 16; Out o{static_cast<std::uint32_t>(v), i.r1}; o.q = v != s32(o.r0); return o; }},
    {"smuad", 0xFB21, 0xF002, [](const In& i) { const std::int64_t v = s16(i.r1) * s16(i.r2) + s16(i.r1 >> 16) * s16(i.r2 >> 16); Out o{static_cast<std::uint32_t>(v), i.r1}; o.q = v != s32(o.r0); return o; }},
    {"smusdx", 0xFB41, 0xF012, [](const In& i) { return Out{static_cast<std::uint32_t>(s16(i.r1) * s16(i.r2 >> 16) - s16(i.r1 >> 16) * s16(i.r2)), i.r1}; }},
    {"smlad", 0xFB21, 0x3002, [](const In& i) { const std::int64_t v = s16(i.r1) * s16(i.r2) + s16(i.r1 >> 16) * s16(i.r2 >> 16) + s32(i.r3); Out o{static_cast<std::uint32_t>(v), i.r1}; o.q = v != s32(o.r0); return o; }},
    {"smlsd", 0xFB41, 0x3002, [](const In& i) { const std::int64_t v = s16(i.r1) * s16(i.r2) - s16(i.r1 >> 16) * s16(i.r2 >> 16) + s32(i.r3); Out o{static_cast<std::uint32_t>(v), i.r1}; o.q = v != s32(o.r0); return o; }},
    {"smmul", 0xFB51, 0xF002, [](const In& i) { return Out{static_cast<std::uint32_t>((s32(i.r1) * s32(i.r2)) >> 32), i.r1}; }},
    {"smmulr", 0xFB51, 0xF012, [](const In& i) { return Out{static_cast<std::uint32_t>(static_cast<std::uint64_t>(s32(i.r1) * s32(i.r2) + 0x80000000ll) >> 32), i.r1}; }},
    {"smmla", 0xFB51, 0x3002, [](const In& i) { return Out{static_cast<std::uint32_t>(((static_cast<std::uint64_t>(i.r3) << 32) + static_cast<std::uint64_t>(s32(i.r1) * s32(i.r2))) >> 32), i.r1}; }},
    {"smmls", 0xFB61, 0x3002, [](const In& i) { return Out{static_cast<std::uint32_t>(((static_cast<std::uint64_t>(i.r3) << 32) - static_cast<std::uint64_t>(s32(i.r1) * s32(i.r2))) >> 32), i.r1}; }},
    {"smlalbt", 0xFBC2, 0x0193, [](const In& i) { const std::uint64_t v = ((static_cast<std::uint64_t>(i.r1) << 32) | i.r0) + static_cast<std::uint64_t>(s16(i.r2) * s16(i.r3 >> 16)); return Out{static_cast<std::uint32_t>(v), static_cast<std::uint32_t>(v >> 32)}; }},
    {"smlald", 0xFBC2, 0x01C3, [](const In& i) { const std::uint64_t v = ((static_cast<std::uint64_t>(i.r1) << 32) | i.r0) + static_cast<std::uint64_t>(s16(i.r2) * s16(i.r3) + s16(i.r2 >> 16) * s16(i.r3 >> 16)); return Out{static_cast<std::uint32_t>(v), static_cast<std::uint32_t>(v >> 32)}; }},
    {"smlsldx", 0xFBD2, 0x01D3, [](const In& i) { const std::uint64_t v = ((static_cast<std::uint64_t>(i.r1) << 32) | i.r0) + static_cast<std::uint64_t>(s16(i.r2) * s16(i.r3 >> 16) - s16(i.r2 >> 16) * s16(i.r3)); return Out{static_cast<std::uint32_t>(v), static_cast<std::uint32_t>(v >> 32)}; }},
    {"umaal", 0xFBE2, 0x0163, [](const In& i) { const std::uint64_t v = static_cast<std::uint64_t>(i.r2) * i.r3 + i.r0 + i.r1; return Out{static_cast<std::uint32_t>(v), static_cast<std::uint32_t>(v >> 32)}; }},
};

// Values at the edges of halfwords and bytes, and random ones.
std::uint32_t dsp_operand(std::mt19937& random)
{
    static const std::uint32_t edges[] = {0, 1, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu, 0x80008000u, 0x7FFF7FFFu,
        0x80808080u, 0x7F7F7F7Fu, 0x00FF00FFu, 0xFFFF0000u, 0x0000FFFFu, 0x40000000u, 0xC0000000u, 0x00010001u};
    const std::uint32_t pick = random() % 4;
    if (pick == 0) {
        return edges[random() % (sizeof edges / sizeof edges[0])];
    }
    if (pick == 1) { // each halfword an edge or small
        static const std::uint32_t half[] = {0, 1, 0x7FFF, 0x8000, 0xFFFF, 0x00FF, 0x0080, 0x7F80};
        return half[random() % 8] | (half[random() % 8] << 16);
    }
    return random();
}

} // namespace

TEST_CASE("m4: every DSP instruction gives the result, Q and GE of the manual's pseudocode")
{
    std::mt19937 random(407);
    int wrong = 0;
    for (const DspCase& k : dsp_cases) {
        Core core({k.h1, k.h2, 0xE7FE});
        for (int round = 0; round < 20000; ++round) {
            const In in{dsp_operand(random), dsp_operand(random), dsp_operand(random), dsp_operand(random), static_cast<std::uint32_t>(random() & 0xFu)};
            core.cpu.reset();
            core.set(0, in.r0);
            core.set(1, in.r1);
            core.set(2, in.r2);
            core.set(3, in.r3);
            core.cpu.set_xpsr((1u << 24) | (in.ge << 16));
            core.steps(1);
            const Out want = k.reference(in);
            const std::uint32_t psr = core.cpu.xpsr();
            const std::uint32_t ge = want.ge < 0 ? in.ge : static_cast<std::uint32_t>(want.ge);
            if (core.r(0) != want.r0 || core.r(1) != want.r1 || ((psr & Q) != 0) != want.q || ((psr >> 16) & 0xFu) != ge
                || core.faulted()) {
                if (wrong++ < 20) {
                    std::printf("  %s r0=%08X r1=%08X r2=%08X r3=%08X ge=%X: got r0=%08X r1=%08X q=%d ge=%X, the manual r0=%08X "
                                "r1=%08X q=%d ge=%X\n",
                        k.name, static_cast<unsigned>(in.r0), static_cast<unsigned>(in.r1), static_cast<unsigned>(in.r2),
                        static_cast<unsigned>(in.r3), static_cast<unsigned>(in.ge), static_cast<unsigned>(core.r(0)),
                        static_cast<unsigned>(core.r(1)), (psr & Q) != 0, static_cast<unsigned>((psr >> 16) & 0xFu),
                        static_cast<unsigned>(want.r0), static_cast<unsigned>(want.r1), want.q, static_cast<unsigned>(ge));
                }
            }
        }
    }
    CHECK(wrong == 0);
    // Q is sticky: a saturation sets it and a later result in range leaves it.
    Core sticky({0xFA82, 0xF081, 0xFA82, 0xF081, 0xE7FE}); // qadd twice
    sticky.set(1, 0x7FFFFFFFu);
    sticky.set(2, 1);
    sticky.steps(1);
    CHECK((sticky.cpu.xpsr() & Q) != 0);
    sticky.set(1, 1);
    sticky.steps(1);
    CHECK(sticky.r(0) == 2);
    CHECK((sticky.cpu.xpsr() & Q) != 0);
    // MSR and MRS reach the GE flags.
    Core flags({0xF380, 0x8400, 0xF3EF, 0x8100, 0xF380, 0x8C00, 0xF3EF, 0x8200, 0xE7FE}); // msr APSR_g; mrs r1; msr APSR_nzcvqg; mrs r2
    flags.set(0, 0xF00A0000u);
    flags.steps(2);
    CHECK(flags.r(1) == 0x000A0000u); // only GE was written
    flags.set(0, 0x90050000u);
    flags.steps(2);
    CHECK(flags.r(2) == 0x90050000u);
    // The Cortex-M3 has none of this: an undefined instruction, as before.
    Core m3({0xFA82, 0xF081, 0xE7FE}, Cpu::Architecture::CortexM3);
    m3.steps(2);
    CHECK(m3.faulted());
    CHECK((m3.word(0xE000ED28) & (1u << 16)) != 0); // UFSR.UNDEFINSTR
    Core m3_flags({0xF380, 0x8C00, 0xF3EF, 0x8100, 0xE7FE}, Cpu::Architecture::CortexM3);
    m3_flags.set(0, 0xF00F0000u);
    m3_flags.steps(2);
    CHECK(m3_flags.r(1) == 0xF0000000u); // no GE flags there
}

namespace {

constexpr std::uint32_t cpacr = 0xE000ED88, fpccr = 0xE000EF34, fpcar = 0xE000EF38, fpdscr = 0xE000EF3C,
                        cfsr = 0xE000ED28;

std::uint32_t f32(float value)
{
    return as_bits(value);
}

// A core with the FPU on, the registers set, run for `instructions`.
std::unique_ptr<Core> fpu_run(const std::vector<std::uint16_t>& code, std::initializer_list<std::pair<int, std::uint32_t>> s,
    int instructions, std::uint32_t fpscr = 0, std::initializer_list<std::pair<int, std::uint32_t>> r = {})
{
    std::vector<std::uint16_t> all = code;
    all.push_back(0xE7FE);
    auto core = std::make_unique<Core>(all);
    core->enable_fpu();
    // The first floating-point instruction of a context takes the rounding
    // mode and the DN, FZ and AHP bits from FPDSCR: the mode is set there too.
    core->poke(fpdscr, fpscr);
    core->cpu.set_fpscr(fpscr);
    for (const auto& [n, v] : s) {
        core->cpu.set_fp_reg(n, v);
    }
    for (const auto& [n, v] : r) {
        core->set(n, v);
    }
    core->steps(instructions);
    return core;
}

} // namespace

TEST_CASE("m4: FPU arithmetic instructions compute what their names say, in every rounding mode")
{
    // d = 10, n = 2, m = 3: the thirteen three-register operations by their definitions.
    struct Named {
        const char* name;
        std::uint16_t h1, h2;
        float want;
    };
    const Named named[] = {
        {"vadd", 0xEE30, 0x0A81, 5.0f},    {"vsub", 0xEE30, 0x0AC1, -1.0f},   {"vmul", 0xEE20, 0x0A81, 6.0f},
        {"vdiv", 0xEE80, 0x0A81, 2.0f / 3.0f}, {"vnmul", 0xEE20, 0x0AC1, -6.0f}, {"vmla", 0xEE00, 0x0A81, 16.0f},
        {"vmls", 0xEE00, 0x0AC1, 4.0f},    {"vnmla", 0xEE10, 0x0AC1, -16.0f}, {"vnmls", 0xEE10, 0x0A81, -4.0f},
        {"vfma", 0xEEA0, 0x0A81, 16.0f},   {"vfms", 0xEEA0, 0x0AC1, 4.0f},    {"vfnma", 0xEE90, 0x0AC1, -16.0f},
        {"vfnms", 0xEE90, 0x0A81, -4.0f},
    };
    for (const Named& k : named) {
        auto c = fpu_run({k.h1, k.h2}, {{0, f32(10.0f)}, {1, f32(2.0f)}, {2, f32(3.0f)}}, 1);
        if (c->cpu.fp_reg(0) != f32(k.want)) {
            std::printf("  %s gave %08X\n", k.name, static_cast<unsigned>(c->cpu.fp_reg(0)));
        }
        CHECK(c->cpu.fp_reg(0) == f32(k.want));
        CHECK(c->cpu.fp_reg(1) == f32(2.0f));
        CHECK(c->cpu.fp_reg(2) == f32(3.0f));
        CHECK(!c->faulted());
    }
    // The same operations with random operands and modes: the register and
    // FPSCR are those of the arithmetic functions (themselves compared with
    // the host in m4-diff).
    using Compute = std::uint32_t (*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t&);
    constexpr std::uint32_t sign = 0x80000000u;
    const Compute compute[] = {
        [](std::uint32_t, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { return fp::add(n, m, f); },
        [](std::uint32_t, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { return fp::sub(n, m, f); },
        [](std::uint32_t, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { return fp::mul(n, m, f); },
        [](std::uint32_t, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { return fp::div(n, m, f); },
        [](std::uint32_t, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { return fp::mul(n, m, f) ^ sign; },
        [](std::uint32_t d, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { const std::uint32_t p = fp::mul(n, m, f); return fp::add(d, p, f); },
        [](std::uint32_t d, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { const std::uint32_t p = fp::mul(n, m, f); return fp::add(d, p ^ sign, f); },
        [](std::uint32_t d, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { const std::uint32_t p = fp::mul(n, m, f); return fp::add(d ^ sign, p ^ sign, f); },
        [](std::uint32_t d, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { const std::uint32_t p = fp::mul(n, m, f); return fp::add(d ^ sign, p, f); },
        [](std::uint32_t d, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { return fp::mul_add(d, n, m, f); },
        [](std::uint32_t d, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { return fp::mul_add(d, n ^ sign, m, f); },
        [](std::uint32_t d, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { return fp::mul_add(d ^ sign, n ^ sign, m, f); },
        [](std::uint32_t d, std::uint32_t n, std::uint32_t m, std::uint32_t& f) { return fp::mul_add(d ^ sign, n, m, f); },
    };
    Operands operands;
    int wrong = 0;
    for (std::size_t k = 0; k < sizeof named / sizeof named[0]; ++k) {
        Core core({named[k].h1, named[k].h2, 0xE7FE});
        for (int round = 0; round < 5000; ++round) {
            const std::uint32_t d = operands.any();
            const std::uint32_t n = operands.any();
            const std::uint32_t m = operands.near(n);
            const std::uint32_t control = mode(static_cast<unsigned>(round) & 3u) | ((round & 4) ? fp::DN : 0u)
                | ((round & 8) ? fp::FZ : 0u);
            core.cpu.reset();
            core.enable_fpu();
            core.poke(fpdscr, control);
            core.cpu.set_fpscr(control);
            core.cpu.set_fp_reg(0, d);
            core.cpu.set_fp_reg(1, n);
            core.cpu.set_fp_reg(2, m);
            core.steps(1);
            std::uint32_t f = control;
            const std::uint32_t want = compute[k](d, n, m, f);
            if (core.cpu.fp_reg(0) != want || core.cpu.fpscr() != f) {
                if (wrong++ < 10) {
                    std::printf("  %s d=%08X n=%08X m=%08X fpscr=%08X: got %08X %08X, wanted %08X %08X\n", named[k].name,
                        static_cast<unsigned>(d), static_cast<unsigned>(n), static_cast<unsigned>(m),
                        static_cast<unsigned>(control), static_cast<unsigned>(core.cpu.fp_reg(0)),
                        static_cast<unsigned>(core.cpu.fpscr()), static_cast<unsigned>(want), static_cast<unsigned>(f));
                }
            }
        }
    }
    CHECK(wrong == 0);
    // The registers' numbers: S30 = S31 + S29.
    auto c = fpu_run({0xEE3F, 0xFAAE}, {{31, f32(1.5f)}, {29, f32(2.25f)}}, 1);
    CHECK(c->cpu.fp_reg(30) == f32(3.75f));
}

TEST_CASE("m4: FPU moves, comparisons and conversions")
{
    auto c = fpu_run({0xEEB1, 0x0AC1}, {{2, f32(9.0f)}}, 1); // vsqrt s0, s2
    CHECK(c->cpu.fp_reg(0) == f32(3.0f));
    c = fpu_run({0xEEB0, 0x0AC1}, {{2, f32(-9.0f)}}, 1); // vabs
    CHECK(c->cpu.fp_reg(0) == f32(9.0f));
    c = fpu_run({0xEEB1, 0x0A41}, {{2, qnan}}, 1); // vneg flips a NaN's sign too, without a flag
    CHECK(c->cpu.fp_reg(0) == (qnan | minus));
    CHECK(c->cpu.fpscr() == 0);
    c = fpu_run({0xEEB0, 0x0A41}, {{2, snan}}, 1); // vmov copies bits
    CHECK(c->cpu.fp_reg(0) == snan);
    CHECK(c->cpu.fpscr() == 0);
    c = fpu_run({0xEEB7, 0x0A00, 0xEEF8, 0xFA04}, {}, 2); // vmov s0, #1.0; vmov s31, #-2.5
    CHECK(c->cpu.fp_reg(0) == f32(1.0f));
    CHECK(c->cpu.fp_reg(31) == f32(-2.5f));

    // VCMP, then VMRS to the APSR: the flags a branch tests.
    auto compared = [](std::uint16_t h1, std::uint16_t h2, std::uint32_t a, std::uint32_t b) {
        auto core = fpu_run({h1, h2, 0xEEF1, 0xFA10}, {{1, a}, {2, b}}, 2);
        return std::pair<std::uint32_t, std::uint32_t>{core->cpu.xpsr() >> 28, core->cpu.fpscr() & 0xFFu};
    };
    CHECK(compared(0xEEF4, 0x0A41, f32(1.0f), f32(2.0f)) == (std::pair<std::uint32_t, std::uint32_t>{0x8, 0}));
    CHECK(compared(0xEEF4, 0x0A41, f32(2.0f), f32(1.0f)) == (std::pair<std::uint32_t, std::uint32_t>{0x2, 0}));
    CHECK(compared(0xEEF4, 0x0A41, f32(2.0f), f32(2.0f)) == (std::pair<std::uint32_t, std::uint32_t>{0x6, 0}));
    CHECK(compared(0xEEF4, 0x0A41, qnan, f32(2.0f)) == (std::pair<std::uint32_t, std::uint32_t>{0x3, 0}));
    CHECK(compared(0xEEF4, 0x0AC1, qnan, f32(2.0f)) == (std::pair<std::uint32_t, std::uint32_t>{0x3, fp::IOC})); // vcmpe
    CHECK(compared(0xEEF5, 0x0A40, f32(-1.0f), f32(7.0f)) == (std::pair<std::uint32_t, std::uint32_t>{0x8, 0})); // with zero
    CHECK(compared(0xEEF5, 0x0A40, minus, f32(7.0f)) == (std::pair<std::uint32_t, std::uint32_t>{0x6, 0}));
    CHECK(compared(0xEEF5, 0x0AC0, snan, 0) == (std::pair<std::uint32_t, std::uint32_t>{0x3, fp::IOC}));
    // VMSR and VMRS: only the bits FPSCR has.
    c = fpu_run({0xEEE1, 0x0A10, 0xEEF1, 0x1A10}, {}, 2, 0, {{0, 0xFFFFFFFFu}}); // vmsr fpscr, r0; vmrs r1, fpscr
    CHECK(c->r(1) == 0xF7C0009Fu);

    // Integers.
    c = fpu_run({0xEEB8, 0x0AC1}, {{2, 0xFFFFFFFEu}}, 1); // vcvt.f32.s32: -2
    CHECK(c->cpu.fp_reg(0) == f32(-2.0f));
    c = fpu_run({0xEEB8, 0x0A41}, {{2, 0x80000000u}}, 1); // vcvt.f32.u32: 2^31
    CHECK(c->cpu.fp_reg(0) == f32(2147483648.0f));
    c = fpu_run({0xEEBD, 0x0AC1}, {{2, f32(-2.9f)}}, 1); // vcvt.s32.f32: toward zero
    CHECK(c->cpu.fp_reg(0) == 0xFFFFFFFEu);
    CHECK(c->cpu.fpscr() == fp::IXC);
    c = fpu_run({0xEEBC, 0x0AC1}, {{2, f32(-2.9f)}}, 1); // vcvt.u32.f32: saturated at 0
    CHECK(c->cpu.fp_reg(0) == 0);
    CHECK(c->cpu.fpscr() == fp::IOC);
    c = fpu_run({0xEEBD, 0x0A41}, {{2, f32(2.5f)}}, 1); // vcvtr: FPSCR's mode, to nearest even
    CHECK(c->cpu.fp_reg(0) == 2);
    c = fpu_run({0xEEBD, 0x0A41}, {{2, f32(2.5f)}}, 1, mode(fp::PlusInfinity));
    CHECK(c->cpu.fp_reg(0) == 3);
    c = fpu_run({0xEEBC, 0x0A41}, {{2, f32(2.5f)}}, 1, mode(fp::MinusInfinity)); // vcvtr.u32
    CHECK(c->cpu.fp_reg(0) == 2);
    // Fixed point, in place.
    c = fpu_run({0xEEBE, 0x0ACC}, {{0, f32(1.5f)}}, 1); // vcvt.s32.f32 s0, s0, #8
    CHECK(c->cpu.fp_reg(0) == 0x180u);
    c = fpu_run({0xEEBF, 0x0A46}, {{0, f32(3.25f)}}, 1); // vcvt.u16.f32 s0, s0, #4
    CHECK(c->cpu.fp_reg(0) == 0x34u);
    c = fpu_run({0xEEBA, 0x0A60}, {{0, 0x12348000u}}, 1); // vcvt.f32.s16 s0, s0, #15: the low 16 bits, -1.0
    CHECK(c->cpu.fp_reg(0) == f32(-1.0f));
    c = fpu_run({0xEEBB, 0x0AC0}, {{0, 0x80000000u}}, 1); // vcvt.f32.u32 s0, s0, #32: 0.5
    CHECK(c->cpu.fp_reg(0) == f32(0.5f));
    // Half precision: the bottom or the top half of the register.
    c = fpu_run({0xEEB2, 0x0A41}, {{2, 0xC0003C00u}}, 1); // vcvtb.f32.f16
    CHECK(c->cpu.fp_reg(0) == f32(1.0f));
    c = fpu_run({0xEEB2, 0x0AC1}, {{2, 0xC0003C00u}}, 1); // vcvtt.f32.f16
    CHECK(c->cpu.fp_reg(0) == f32(-2.0f));
    c = fpu_run({0xEEB3, 0x0A41}, {{0, 0xAAAAAAAAu}, {2, f32(1.0f)}}, 1); // vcvtb.f16.f32: the other half stays
    CHECK(c->cpu.fp_reg(0) == 0xAAAA3C00u);
    c = fpu_run({0xEEB3, 0x0AC1}, {{0, 0xAAAAAAAAu}, {2, f32(-2.0f)}}, 1); // vcvtt.f16.f32
    CHECK(c->cpu.fp_reg(0) == 0xC000AAAAu);

    // Core registers.
    c = fpu_run({0xEE11, 0x0A90, 0xEE01, 0x1A90}, {{3, 0x12345678u}}, 2, 0, {{1, 0xCAFEF00Du}}); // vmov r0, s3; vmov s3, r1
    CHECK(c->r(0) == 0x12345678u);
    CHECK(c->cpu.fp_reg(3) == 0xCAFEF00Du);
    c = fpu_run({0xEC51, 0x0A12}, {{4, 0x11111111u}, {5, 0x22222222u}}, 1); // vmov r0, r1, s4, s5
    CHECK(c->r(0) == 0x11111111u);
    CHECK(c->r(1) == 0x22222222u);
    c = fpu_run({0xEC41, 0x0A12}, {}, 1, 0, {{0, 0x33333333u}, {1, 0x44444444u}}); // vmov s4, s5, r0, r1
    CHECK(c->cpu.fp_reg(4) == 0x33333333u);
    CHECK(c->cpu.fp_reg(5) == 0x44444444u);
    c = fpu_run({0xEC41, 0x0B13, 0xEC53, 0x2B13}, {}, 2, 0, {{0, 0x55555555u}, {1, 0x66666666u}}); // vmov d3, r0, r1; vmov r2, r3, d3
    CHECK(c->cpu.fp_reg(6) == 0x55555555u); // D3 is S6 (low) and S7 (high)
    CHECK(c->cpu.fp_reg(7) == 0x66666666u);
    CHECK(c->r(2) == 0x55555555u);
    CHECK(c->r(3) == 0x66666666u);
    c = fpu_run({0xEE22, 0x0B10, 0xEE12, 0x1B10}, {{4, 0x77777777u}}, 2, 0, {{0, 0x88888888u}}); // vmov.32 d2[1], r0; vmov.32 r1, d2[0]
    CHECK(c->cpu.fp_reg(5) == 0x88888888u);
    CHECK(c->r(1) == 0x77777777u);
}

TEST_CASE("m4: FPU loads and stores, with the registers of VPUSH and VPOP")
{
    auto c = fpu_run({}, {}, 0);
    c->poke(ram + 8, 0x3FC00000u);
    c->poke(ram - 4, 0x40490FDBu);
    c->load(code_base, {0xEDD1, 0x2A02, 0xED51, 0x3A01, 0xEDC1, 0x3A04, 0xE7FE}); // vldr s5,[r1,#8]; vldr s7,[r1,#-4]; vstr s7,[r1,#16]
    c->set(1, ram);
    c->steps(3);
    CHECK(c->cpu.fp_reg(5) == 0x3FC00000u);
    CHECK(c->cpu.fp_reg(7) == 0x40490FDBu);
    CHECK(c->word(ram + 16) == 0x40490FDBu);
    CHECK(c->r(1) == ram);
    // A double register is two words, low first.
    c = fpu_run({0xED91, 0x2B02, 0xED81, 0x2B08}, {}, 0, 0, {{1, ram}}); // vldr d2, [r1, #8]; vstr d2, [r1, #32]
    c->poke(ram + 8, 0x11112222u);
    c->poke(ram + 12, 0x33334444u);
    c->steps(2);
    CHECK(c->cpu.fp_reg(4) == 0x11112222u);
    CHECK(c->cpu.fp_reg(5) == 0x33334444u);
    CHECK(c->word(ram + 32) == 0x11112222u);
    CHECK(c->word(ram + 36) == 0x33334444u);
    // A literal after the code: vldr s0, [pc, #4].
    c = fpu_run({0xED9F, 0x0A01, 0xBF00, 0xBF00, 0x0000, 0x4170}, {}, 1); // the word 0x41700000 (15.0) at code_base + 8
    CHECK(c->cpu.fp_reg(0) == f32(15.0f));
    // VPUSH and VPOP.
    c = fpu_run({0xED2D, 0x8A04, 0xECBD, 0x8A04}, {{16, 1}, {17, 2}, {18, 3}, {19, 4}}, 1); // vpush {s16-s19}; vpop
    CHECK(c->r(13) == stack_top - 16);
    CHECK(c->word(stack_top - 16) == 1);
    CHECK(c->word(stack_top - 4) == 4);
    for (int n = 16; n < 20; ++n) {
        c->cpu.set_fp_reg(n, 0);
    }
    c->steps(1);
    CHECK(c->r(13) == stack_top);
    CHECK(c->cpu.fp_reg(16) == 1);
    CHECK(c->cpu.fp_reg(19) == 4);
    c = fpu_run({0xED2D, 0x8B04, 0xECBD, 0x8B04}, {{16, 1}, {17, 2}, {18, 3}, {19, 4}}, 1); // vpush {d8-d9}: the same registers
    CHECK(c->r(13) == stack_top - 16);
    CHECK(c->word(stack_top - 12) == 2);
    c->cpu.set_fp_reg(17, 0);
    c->steps(1);
    CHECK(c->cpu.fp_reg(17) == 2);
    CHECK(c->r(13) == stack_top);
    // VLDM and VSTM: increment after with and without write-back, decrement before.
    c = fpu_run({0xECB1, 0x1A03, 0xEC82, 0x1A03, 0xED33, 0x1B04}, {}, 0, 0, {{1, ram}, {2, ram + 64}, {3, ram + 16}});
    for (std::uint32_t i = 0; i < 4; ++i) {
        c->poke(ram + 4 * i, 0x100u + i);
    }
    c->steps(1); // vldmia r1!, {s2-s4}
    CHECK(c->cpu.fp_reg(2) == 0x100u);
    CHECK(c->cpu.fp_reg(4) == 0x102u);
    CHECK(c->r(1) == ram + 12);
    c->steps(1); // vstmia r2, {s2-s4}
    CHECK(c->word(ram + 64) == 0x100u);
    CHECK(c->word(ram + 72) == 0x102u);
    CHECK(c->r(2) == ram + 64);
    c->steps(1); // vldmdb r3!, {d1-d2}: S2..S5 from the four words below r3
    CHECK(c->cpu.fp_reg(2) == 0x100u);
    CHECK(c->cpu.fp_reg(5) == 0x103u);
    CHECK(c->r(3) == ram);
    // A bad address faults and names it.
    c = fpu_run({0xEDD1, 0x2A02}, {}, 1, 0, {{1, 0x50000000u}});
    c->steps(1);
    CHECK(c->faulted());
    CHECK(c->word(0xE000ED38) == 0x50000008u); // BFAR
}

TEST_CASE("m4: the FPU is off until CPACR turns it on, and has no double precision")
{
    // As after a reset: a floating-point instruction is a UsageFault (NOCP).
    Core off({0xEE30, 0x0A81, 0xE7FE});
    off.steps(2);
    CHECK(off.faulted());
    CHECK((off.word(cfsr) & (1u << 19)) != 0);
    CHECK(off.word(cpacr) == 0);
    // Privileged access only (CP10 = CP11 = 01): thread mode without privilege faults.
    Core user({0xF380, 0x8814, 0xEE30, 0x0A81, 0xE7FE}); // msr CONTROL, r0; vadd
    user.poke(cpacr, 0x00500000u);
    user.set(0, 1);
    user.steps(3);
    CHECK(user.faulted());
    CHECK((user.word(cfsr) & (1u << 19)) != 0);
    Core privileged({0xEE30, 0x0A81, 0xE7FE});
    privileged.poke(cpacr, 0x00500000u);
    privileged.steps(1);
    CHECK(!privileged.faulted());
    // Double precision: undefined, with the FPU on.
    for (const auto& code : {std::vector<std::uint16_t>{0xEE31, 0x0B02}, std::vector<std::uint16_t>{0xEEB7, 0x0AC1}}) { // vadd.f64; vcvt.f64.f32
        auto c = fpu_run(code, {}, 2);
        CHECK(c->faulted());
        CHECK((c->word(cfsr) & (1u << 16)) != 0);
        CHECK((c->word(cfsr) & (1u << 19)) == 0);
    }
    // The Cortex-M3 has no coprocessor, as before; its CPACR does not exist.
    Core m3({0xEE30, 0x0A81, 0xE7FE}, Cpu::Architecture::CortexM3);
    m3.poke(cpacr, 0x00F00000u);
    CHECK(m3.word(cpacr) == 0);
    m3.steps(2);
    CHECK(m3.faulted());
    CHECK((m3.word(cfsr) & (1u << 19)) != 0);
    CHECK(m3.word(0xE000ED00) == 0x411FC231u);
    CHECK(off.word(0xE000ED00) == 0x410FC241u);
    CHECK(off.word(0xE000EF40) == 0x10110021u); // MVFR0
    CHECK(off.word(fpccr) == 0xC0000000u);
}

TEST_CASE("m4: exception entry and return keep the floating-point context, lazily or at once")
{
    // A step that enters an exception also runs the handler's first
    // instruction, so the handlers here start with a NOP.
    const std::uint32_t value = f32(1.0f);
    const std::vector<std::uint16_t> thread = {0xEEB7, 0x0A00, 0xDF00, 0xE7FE};  // vmov s0, #1.0; svc 0; b .
    const std::vector<std::uint16_t> fp_handler = {0xBF00, 0xEEB1, 0x0A41, 0x4770}; // nop; vneg s0, s2; bx lr
    const std::vector<std::uint16_t> plain_handler = {0xBF00, 0x4770};            // nop; bx lr

    // (a) The thread used the FPU: the frame has room for S0-S15 and FPSCR,
    // filled only when the handler first uses the FPU (lazy stacking).
    {
        Core c(thread);
        c.load(irq_handler, fp_handler);
        c.enable_fpu();
        c.cpu.set_fp_reg(5, 0xABCD0005u);
        c.cpu.set_fp_reg(2, f32(8.0f));
        c.steps(1);
        CHECK((c.cpu.control() & 4u) != 0); // FPCA
        c.cpu.set_fpscr(mode(fp::Zero) | fp::IXC);
        c.steps(2); // svc; the entry with the handler's nop
        CHECK(c.cpu.pc() == irq_handler + 2);
        const std::uint32_t frame = stack_top - 0x68;
        CHECK(c.r(13) == frame);
        CHECK(c.r(14) == 0xFFFFFFE9u);
        CHECK((c.cpu.control() & 4u) == 0);
        CHECK((c.word(fpccr) & 1u) != 0); // LSPACT
        CHECK((c.word(fpccr) & 8u) != 0); // THREAD
        CHECK(c.word(fpcar) == frame + 0x20);
        CHECK(c.word(frame + 0x20) == 0); // nothing written yet
        CHECK((c.word(frame + 0x1C) & (1u << 9)) == 0); // the stack was aligned
        c.steps(1); // vneg in the handler
        CHECK(c.word(frame + 0x20) == value);
        CHECK(c.word(frame + 0x20 + 4 * 5) == 0xABCD0005u);
        CHECK(c.word(frame + 0x60) == (mode(fp::Zero) | fp::IXC));
        CHECK((c.word(fpccr) & 1u) == 0);
        CHECK((c.cpu.control() & 4u) != 0);
        CHECK(c.cpu.fp_reg(0) == f32(-8.0f));
        CHECK((c.cpu.fpscr() & 0x00C00000u) == 0); // a new context starts with FPDSCR's rounding mode
        c.steps(1); // bx lr
        CHECK(c.r(13) == stack_top);
        CHECK(c.cpu.fp_reg(0) == value);
        CHECK(c.cpu.fpscr() == (mode(fp::Zero) | fp::IXC));
        CHECK((c.cpu.control() & 4u) != 0);
        CHECK(c.cpu.active_exception() == 0);
        CHECK(!c.faulted());
    }
    // (b) The handler does not use the FPU: nothing is written, nothing is lost.
    {
        Core c(thread);
        c.load(irq_handler, plain_handler);
        c.enable_fpu();
        c.steps(3);
        const std::uint32_t frame = stack_top - 0x68;
        CHECK(c.r(13) == frame);
        c.steps(1);
        CHECK(c.word(frame + 0x20) == 0);
        CHECK((c.word(fpccr) & 1u) == 0);
        CHECK(c.r(13) == stack_top);
        CHECK(c.cpu.fp_reg(0) == value);
        CHECK((c.cpu.control() & 4u) != 0);
    }
    // (c) A thread that never used the FPU gets the short frame; a handler
    // that uses it leaves no trace in the thread.
    {
        Core c({0xDF00, 0xE7FE});
        c.load(irq_handler, fp_handler);
        c.enable_fpu();
        c.steps(2);
        CHECK(c.r(13) == stack_top - 0x20);
        CHECK(c.r(14) == 0xFFFFFFF9u);
        c.steps(1);
        CHECK((c.cpu.control() & 4u) != 0);
        c.steps(1);
        CHECK(c.r(13) == stack_top);
        CHECK((c.cpu.control() & 4u) == 0);
    }
    // (d) Lazy stacking off (FPCCR.LSPEN clear): the registers are written at entry.
    {
        Core c(thread);
        c.load(irq_handler, plain_handler);
        c.enable_fpu();
        c.poke(fpccr, 0x80000000u);
        c.cpu.set_fp_reg(15, 0xABCD000Fu);
        c.steps(3);
        const std::uint32_t frame = stack_top - 0x68;
        CHECK(c.word(frame + 0x20) == value);
        CHECK(c.word(frame + 0x20 + 4 * 15) == 0xABCD000Fu);
        CHECK((c.word(fpccr) & 1u) == 0);
        c.cpu.set_fp_reg(0, 0);
        c.steps(1);
        CHECK(c.cpu.fp_reg(0) == value); // read back from the frame
        CHECK(c.r(13) == stack_top);
    }
    // (e) Automatic preservation off (ASPEN clear): the FPU's use is not tracked, the frame is short.
    {
        Core c(thread);
        c.load(irq_handler, plain_handler);
        c.enable_fpu();
        c.poke(fpccr, 0);
        c.steps(3);
        CHECK((c.cpu.control() & 4u) == 0);
        CHECK(c.r(13) == stack_top - 0x20);
        CHECK(c.r(14) == 0xFFFFFFF9u);
    }
    // (f) A stack pointer that is not a multiple of 8: the frame is aligned and the pointer comes back.
    {
        Core c({0xB081, 0xEEB7, 0x0A00, 0xDF00, 0xE7FE}); // sub sp, #4; vmov; svc
        c.load(irq_handler, fp_handler);
        c.enable_fpu();
        c.cpu.set_fp_reg(2, f32(8.0f));
        c.steps(4);
        const std::uint32_t frame = stack_top - 4 - 0x68 - 4;
        CHECK(c.r(13) == frame);
        CHECK((c.word(frame + 0x1C) & (1u << 9)) != 0);
        c.steps(2);
        CHECK(c.r(13) == stack_top - 4);
        CHECK(c.cpu.fp_reg(0) == value);
    }
    // (g) Nested: an interrupt arrives while the SVC handler, which used the
    // FPU, runs. The registers of each level come back.
    {
        Core c(thread);
        c.load(irq_handler, {0xBF00, 0xEEB1, 0x0A41, 0xBF00, 0xBF00, 0x4770}); // nop; vneg s0, s2; nop; nop; bx lr
        const std::uint32_t second = 0x08000400;
        c.load(second, {0xBF00, 0xEEB7, 0x0A00, 0x4770}); // nop; vmov s0, #1.0; bx lr
        const std::uint32_t vector = second | 1;
        c.bus.load(0x08000000 + 4 * 16, reinterpret_cast<const std::uint8_t*>(&vector), 4); // IRQ 0
        c.enable_fpu();
        c.cpu.set_fp_reg(2, f32(8.0f));
        c.poke(0xE000E100, 1);           // NVIC ISER0: IRQ 0 on, at priority 0
        c.poke(0xE000ED1C, 0x80000000u); // SHPR2: SVCall at priority 0x80, below it
        c.steps(4); // vmov; svc; entry with nop; vneg in the SVC handler
        CHECK(c.cpu.active_exception() == 11);
        CHECK(c.cpu.fp_reg(0) == f32(-8.0f));
        c.cpu.set_irq_line(0, true);
        c.steps(1); // the entry into the interrupt, with its nop
        c.cpu.set_irq_line(0, false);
        CHECK(c.cpu.pc() == second + 2);
        CHECK(c.r(14) == 0xFFFFFFE1u); // back to handler mode, with a floating-point frame
        const std::uint32_t inner = stack_top - 0x68 - 0x68;
        CHECK(c.r(13) == inner);
        CHECK(c.word(inner + 0x20) == 0);
        c.steps(1); // vmov s0, #1.0: first the SVC handler's registers go to its frame
        CHECK(c.word(inner + 0x20) == f32(-8.0f));
        CHECK(c.cpu.fp_reg(0) == value);
        c.cpu.set_fp_reg(0, f32(99.0f));
        c.steps(1); // bx lr
        CHECK(c.cpu.active_exception() == 11);
        CHECK(c.cpu.fp_reg(0) == f32(-8.0f));
        c.steps(3); // nop; nop; bx lr
        CHECK(c.cpu.active_exception() == 0);
        CHECK(c.cpu.fp_reg(0) == value);
        CHECK(c.r(13) == stack_top);
    }
}

TEST_CASE("m4-diff: the instructions the two cores share give the same registers, flags and memory")
{
    // G12a.2: random encodings that the Cortex-M3 has run on both cores from
    // the same state; DSP and FPU encodings are the Cortex-M4F's alone.
    std::mt19937 random(1103);
    Core m3({}, Cpu::Architecture::CortexM3);
    Core m4({}, Cpu::Architecture::CortexM4F);
    int compared = 0;
    int different = 0;
    for (int round = 0; round < 400000; ++round) {
        std::uint16_t h1 = static_cast<std::uint16_t>(random());
        const std::uint16_t h2 = static_cast<std::uint16_t>(random());
        if (round & 1) { // half of them 32-bit encodings
            h1 = static_cast<std::uint16_t>(0xE800u | (h1 & 0x17FFu));
        }
        const std::string name = Cpu::op_name(Cpu::decode(h1, h2).op);
        if (name == "DSP extension" || name == "FPU") {
            continue;
        }
        std::uint32_t regs[13];
        for (auto& v : regs) {
            // Small numbers, addresses in the RAM and anything, so that loads and stores often work.
            const std::uint32_t pick = random() % 4;
            v = pick == 0 ? random() % 64 : pick == 1 ? ram + (random() % 64) * 4 : random();
        }
        const std::uint32_t flags = (random() & 0xF8000000u) | (1u << 24);
        std::uint32_t result[2][20] = {};
        int which = 0;
        for (Core* core : {&m3, &m4}) {
            core->load(code_base, {h1, h2, 0xE7FE, 0xE7FE});
            core->cpu.reset();
            // The one difference at reset: CCR.STKALIGN is set on the Cortex-M4
            // and clear on the Cortex-M3 r1p1. Set on both, a fault with an
            // unaligned stack pointer leaves the same frame.
            core->poke(0xE000ED14, 0x200u);
            for (std::uint32_t i = 0; i < 16; ++i) {
                core->poke(ram + 4 * i, 0x1000u * i + 7u);
            }
            for (int n = 0; n < 13; ++n) {
                core->set(n, regs[n]);
            }
            core->set(14, 0x08000101u);
            core->cpu.set_xpsr(flags);
            core->steps(1);
            for (int n = 0; n < 16; ++n) {
                result[which][n] = core->r(n);
            }
            result[which][16] = core->cpu.xpsr();
            result[which][17] = core->cpu.pc();
            for (std::uint32_t i = 0; i < 16; ++i) {
                result[which][18] += core->word(ram + 4 * i) * (i + 1);
            }
            result[which][19] = core->word(0xE000ED28); // the fault status
            ++which;
        }
        ++compared;
        if (std::memcmp(result[0], result[1], sizeof result[0]) != 0) {
            if (different++ < 10) {
                std::printf("  %04X %04X (%s): the two cores differ\n", h1, h2, name.c_str());
                for (int n = 0; n < 20; ++n) {
                    if (result[0][n] != result[1][n]) {
                        std::printf("    item %d: %08X on the Cortex-M3, %08X on the Cortex-M4F\n", n,
                            static_cast<unsigned>(result[0][n]), static_cast<unsigned>(result[1][n]));
                    }
                }
            }
        }
    }
    std::printf("  %d encodings compared on the two cores, %d different\n", compared, different);
    CHECK(compared > 300000);
    CHECK(different == 0);
}

TEST_CASE("m4: every encoding decodes and random code runs without crashing the host")
{
    // G12a.5: every 16-bit pattern and ten million 32-bit ones; an encoding
    // nobody has is an undefined instruction or a missing coprocessor.
    int bad = 0;
    for (std::uint32_t h = 0; h < 0x10000; ++h) {
        const Insn in = Cpu::decode(h, 0);
        bad += (in.op == 0 || in.op >= Cpu::op_count() || in.size != ((h >> 11) >= 0x1D ? 4 : 2)) ? 1 : 0;
    }
    std::mt19937 random(407407);
    int dsp = 0;
    int fpu = 0;
    for (int i = 0; i < 10000000; ++i) {
        const std::uint32_t h1 = 0xE800u | (random() & 0x17FFu);
        const Insn in = Cpu::decode(h1, random() & 0xFFFFu);
        bad += (in.op == 0 || in.op >= Cpu::op_count() || in.size != 4) ? 1 : 0;
        const char* name = Cpu::op_name(in.op);
        dsp += name[0] == 'D' && name[1] == 'S' ? 1 : 0;
        fpu += name[0] == 'F' && name[1] == 'P' ? 1 : 0;
    }
    CHECK(bad == 0);
    CHECK(dsp > 1000);
    CHECK(fpu > 1000);
    // Random flash images with the FPU on: faults, lockups and wild branches
    // are fine; a crash or a hang of the host is not.
    for (int run = 0; run < 200; ++run) {
        std::vector<std::uint16_t> code(512);
        for (auto& h : code) {
            h = static_cast<std::uint16_t>(random());
            if (random() % 3 == 0) { // more of the new encodings than chance gives
                static const std::uint16_t heads[] = {0xEE00, 0xEC00, 0xED00, 0xFA80, 0xFB00, 0xFB80, 0xEEB0, 0xEEF0};
                h = static_cast<std::uint16_t>(heads[random() % 8] | (random() & 0xFFu));
            }
        }
        Core c(code);
        c.enable_fpu();
        for (int n = 0; n < 13; ++n) {
            c.set(n, random());
        }
        for (int n = 0; n < 32; ++n) {
            c.cpu.set_fp_reg(n, random());
        }
        c.cpu.run(c.cpu.cycles() + 100000);
        CHECK(c.cpu.cycles() <= 100000 + 20);
    }
}

TEST_CASE("m4: cycle counts of the Cortex-M4's multiplier, DSP and FPU instructions")
{
    // The Cortex-M4 Technical Reference Manual's counts; code in RAM, where no flash wait state is added.
    auto cost = [](const std::vector<std::uint16_t>& instruction, Cpu::Architecture architecture) {
        std::vector<std::uint16_t> code = {0xBF00, 0xBF00, 0xBF00, 0xBF00};
        code.insert(code.end(), instruction.begin(), instruction.end());
        code.insert(code.end(), {0xBF00, 0xBF00, 0xE7FE});
        Core c(code, architecture);
        c.cpu.set_timing(Cpu::Timing::CortexM3);
        c.cpu.reset();
        if (architecture == Cpu::Architecture::CortexM4F) {
            c.enable_fpu();
        }
        c.set(1, ram);
        c.set(2, 0x12345678u);
        c.set(3, 0x00FEDCBAu);
        c.cpu.set_fp_reg(1, f32(3.0f));
        c.cpu.set_fp_reg(2, f32(7.0f));
        c.steps(4);
        const std::uint64_t before = c.cpu.cycles();
        c.steps(1);
        return static_cast<int>(c.cpu.cycles() - before);
    };
    const auto m4 = Cpu::Architecture::CortexM4F;
    const auto m3 = Cpu::Architecture::CortexM3;
    CHECK(cost({0xBF00}, m4) == 1);
    CHECK(cost({0xFA82, 0xF081}, m4) == 1); // qadd
    CHECK(cost({0xFB21, 0x3002}, m4) == 1); // smlad
    CHECK(cost({0xFBC2, 0x01C3}, m4) == 1); // smlald
    CHECK(cost({0xFB01, 0x3002}, m4) == 1); // mla: one cycle on the Cortex-M4
    CHECK(cost({0xFB01, 0x3002}, m3) == 2); // and two on the Cortex-M3, as before
    CHECK(cost({0xFBA2, 0x0103}, m4) == 1); // umull
    CHECK(cost({0xFBA2, 0x0103}, m3) >= 3);
    CHECK(cost({0xEE30, 0x0A81}, m4) == 1);  // vadd
    CHECK(cost({0xEE20, 0x0A81}, m4) == 1);  // vmul
    CHECK(cost({0xEE00, 0x0A81}, m4) == 3);  // vmla
    CHECK(cost({0xEEA0, 0x0A81}, m4) == 3);  // vfma
    CHECK(cost({0xEE80, 0x0A81}, m4) == 14); // vdiv
    CHECK(cost({0xEEB1, 0x0AC1}, m4) == 14); // vsqrt
    CHECK(cost({0xEDD1, 0x2A02}, m4) == 2);  // vldr
    CHECK(cost({0xEDC1, 0x2A02}, m4) == 2);  // vstr
    CHECK(cost({0xED2D, 0x8A04}, m4) == 5);  // vpush of four registers
    CHECK(cost({0xEEF4, 0x0A41}, m4) == 1);  // vcmp
    CHECK(cost({0xEC51, 0x0A12}, m4) == 2);  // vmov r0, r1, s4, s5
}

TEST_CASE("m4-bench: the Cortex-M4F core is as fast as the Cortex-M3's on integer code, and its FPU keeps pace")
{
    // G12a.4. Two loops: integer instructions (adds, eors, lsls, add with a
    // shift, str, ldr, mul, subs, b), and one with a third of FPU
    // instructions (vadd, vmul, vfma among adds, eors, lsls, str, ldr, b).
    const std::vector<std::uint16_t> integer = {0x1840, 0x4042, 0x00D3, 0xEB03, 0x0490, 0x603C, 0x687D, 0xFB05, 0xF600,
        0x3901, 0xE7F4};
    const std::vector<std::uint16_t> floating = {0x1840, 0x4042, 0xEE30, 0x0A20, 0x00D3, 0xEE20, 0x1A20, 0x603C, 0x687D,
        0xEEE0, 0x1A01, 0xE7F3};
    auto mips = [](const std::vector<std::uint16_t>& code, Cpu::Architecture architecture) {
        double best = 0;
        for (int attempt = 0; attempt < 5; ++attempt) {
            Core c(code, architecture);
            if (architecture == Cpu::Architecture::CortexM4F) {
                c.enable_fpu();
            }
            c.set(7, ram);
            c.set(1, 3);
            c.cpu.set_fp_reg(1, f32(1.0f));
            const std::uint64_t count = 40000000;
            const auto begin = std::chrono::steady_clock::now();
            c.cpu.run(c.cpu.cycles() + count);
            const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
            CHECK(!c.faulted());
            best = std::max(best, static_cast<double>(c.cpu.instructions()) / seconds / 1e6);
        }
        return best;
    };
    const double m3 = mips(integer, Cpu::Architecture::CortexM3);
    const double m4 = mips(integer, Cpu::Architecture::CortexM4F);
    const double fpu = mips(floating, Cpu::Architecture::CortexM4F);
    std::printf("  integer loop: Cortex-M3 %.1f MIPS, Cortex-M4F %.1f MIPS (%.1f %%); a third FPU: %.1f MIPS\n", m3, m4,
        100.0 * m4 / m3, fpu);
    CHECK(m3 > 0);
    CHECK(fpu > 0);
    // The gate's numbers are for the owner's laptop (E-12 plan): checked when
    // the program's third argument is "strict", recorded elsewhere (CI machines vary).
    if (strict_benchmark) {
        CHECK(m4 >= 0.95 * m3);
        CHECK(fpu >= 60.0);
    }
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: m4_tests <repository-dir> [filter [strict]]\n");
        return 2;
    }
    repository = argv[1];
    strict_benchmark = argc > 3 && std::string(argv[3]) == "strict";
    return simnodus::test::run_all(argc > 2 ? argv[2] : "");
}
