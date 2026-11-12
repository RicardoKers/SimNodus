// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "softfloat32.hpp"

#include <bit>
#include <cmath>

namespace simnodus::emu::fp {

namespace {

enum class Kind { Zero, Finite, Infinity, QuietNaN, SignalingNaN };

// A number taken apart (FPUnpack): the value of a finite one is
// mantissa * 2^exponent, with a mantissa of at most 24 bits.
struct Parts {
    Kind kind = Kind::Zero;
    bool sign = false;
    std::uint64_t mantissa = 0;
    int exponent = 0;
};

Parts unpack(std::uint32_t a, std::uint32_t& fpscr)
{
    Parts p;
    p.sign = (a >> 31) != 0;
    const std::uint32_t exponent = (a >> 23) & 0xFFu;
    const std::uint32_t fraction = a & 0x7FFFFFu;
    if (exponent == 0) {
        if (fraction == 0) {
            p.kind = Kind::Zero;
        } else if (fpscr & FZ) { // a denormal counts as zero, and says so
            p.kind = Kind::Zero;
            fpscr |= IDC;
        } else {
            p.kind = Kind::Finite;
            p.mantissa = fraction;
            p.exponent = -149;
        }
    } else if (exponent == 0xFF) {
        p.kind = fraction == 0 ? Kind::Infinity : (fraction & 0x400000u) ? Kind::QuietNaN : Kind::SignalingNaN;
    } else {
        p.kind = Kind::Finite;
        p.mantissa = fraction | 0x800000u;
        p.exponent = static_cast<int>(exponent) - 150;
    }
    return p;
}

bool is_nan(const Parts& p) { return p.kind == Kind::QuietNaN || p.kind == Kind::SignalingNaN; }

constexpr std::uint32_t zero(bool sign) { return sign ? 0x80000000u : 0u; }
constexpr std::uint32_t infinity(bool sign) { return sign ? 0xFF800000u : 0x7F800000u; }

unsigned rounding(std::uint32_t fpscr) { return (fpscr >> rmode_shift) & 3u; }

std::uint32_t invalid(std::uint32_t& fpscr)
{
    fpscr |= IOC;
    return default_nan;
}

// FPProcessNaN: a signaling NaN is quieted and is an invalid operation; in
// default NaN mode every NaN result is the default one.
std::uint32_t process_nan(const Parts& p, std::uint32_t a, std::uint32_t& fpscr)
{
    if (p.kind == Kind::SignalingNaN) {
        a |= 0x400000u;
        fpscr |= IOC;
    }
    return (fpscr & DN) ? default_nan : a;
}

// FPProcessNaNs for two and three operands: signaling ones first, in operand order.
bool process_nans(const Parts& pa, std::uint32_t a, const Parts& pb, std::uint32_t b, std::uint32_t& fpscr, std::uint32_t& out)
{
    if (pa.kind == Kind::SignalingNaN) {
        out = process_nan(pa, a, fpscr);
    } else if (pb.kind == Kind::SignalingNaN) {
        out = process_nan(pb, b, fpscr);
    } else if (pa.kind == Kind::QuietNaN) {
        out = process_nan(pa, a, fpscr);
    } else if (pb.kind == Kind::QuietNaN) {
        out = process_nan(pb, b, fpscr);
    } else {
        return false;
    }
    return true;
}

// x >> n with every bit shifted out kept in bit 0 ("sticky").
std::uint64_t shift_right_jam(std::uint64_t x, unsigned n)
{
    if (n == 0) {
        return x;
    }
    if (n >= 64) {
        return x != 0 ? 1u : 0u;
    }
    return (x >> n) | ((x & ((std::uint64_t{1} << n) - 1u)) != 0 ? 1u : 0u);
}

// A binary format: single precision, or half precision for the conversions.
struct Format {
    int fraction_bits; // 23 or 10
    int exponent_max;  // the exponent field of infinity: 255 or 31
    int bias;          // 127 or 15
};
constexpr Format single{23, 255, 127};
constexpr Format half{10, 31, 15};

// FPRound: the value mantissa * 2^exponent (mantissa not zero, its bit 0
// sticky) rounded into the format. `mode` is the rounding mode; `ahp`: the
// alternative half precision, which has no infinity.
std::uint32_t round_pack(bool sign, int exponent, std::uint64_t mantissa, unsigned mode, std::uint32_t& fpscr,
    const Format& format = single, bool ahp = false)
{
    const int lead = std::countl_zero(mantissa);
    mantissa <<= lead;
    exponent -= lead;
    // The leading bit is bit 63 now: the value is 1.xxx * 2^(exponent + 63).
    int biased = exponent + 63 + format.bias;
    const std::uint32_t sign_bit = sign ? (1u << (format.fraction_bits + (format.exponent_max == 255 ? 8 : 5))) : 0u;
    const bool tiny = biased < 1; // below the smallest normal number, before rounding
    if (tiny) {
        if (fpscr & FZ) { // flush to zero: no denormal results, and not inexact
            fpscr |= UFC;
            return sign_bit;
        }
        mantissa = shift_right_jam(mantissa, static_cast<unsigned>(1 - biased));
        biased = 1;
    }
    const int drop = 63 - format.fraction_bits; // bits below the result's last one
    const std::uint64_t rest = mantissa & ((std::uint64_t{1} << drop) - 1u);
    const std::uint64_t half_way = std::uint64_t{1} << (drop - 1);
    std::uint64_t kept = mantissa >> drop; // with the hidden bit, if the number is normal
    const bool inexact = rest != 0;
    bool up = false;
    switch (mode) {
    case Nearest: up = rest > half_way || (rest == half_way && (kept & 1u)); break;
    case PlusInfinity: up = inexact && !sign; break;
    case MinusInfinity: up = inexact && sign; break;
    default: break;
    }
    kept += up ? 1u : 0u;
    // The hidden bit adds one to the exponent field; so does a carry out of the rounding.
    const std::uint64_t packed = (static_cast<std::uint64_t>(biased - 1) << format.fraction_bits) + kept;
    const std::uint64_t field = packed >> format.fraction_bits;
    if (tiny && inexact) {
        fpscr |= UFC;
    }
    const std::uint64_t limit = static_cast<std::uint64_t>(ahp ? format.exponent_max + 1 : format.exponent_max);
    if (field >= limit) {
        if (ahp) { // no infinity: the largest number, and an invalid operation
            fpscr |= IOC;
            return sign_bit | ((1u << (format.fraction_bits + 5)) - 1u);
        }
        fpscr |= OFC | IXC;
        const bool to_infinity = mode == Nearest || (mode == PlusInfinity && !sign) || (mode == MinusInfinity && sign);
        const std::uint32_t top = static_cast<std::uint32_t>(format.exponent_max) << format.fraction_bits;
        return sign_bit | (to_infinity ? top : top - 1u);
    }
    if (inexact) {
        fpscr |= IXC;
    }
    return sign_bit | static_cast<std::uint32_t>(packed);
}

std::uint32_t add_parts(const Parts& pa, std::uint32_t a, const Parts& pb, std::uint32_t b, std::uint32_t& fpscr)
{
    std::uint32_t nan = 0;
    if (process_nans(pa, a, pb, b, fpscr, nan)) {
        return nan;
    }
    const unsigned mode = rounding(fpscr);
    const bool inf_a = pa.kind == Kind::Infinity;
    const bool inf_b = pb.kind == Kind::Infinity;
    if (inf_a && inf_b && pa.sign != pb.sign) {
        return invalid(fpscr);
    }
    if (inf_a || inf_b) {
        return infinity(inf_a ? pa.sign : pb.sign);
    }
    const bool zero_a = pa.kind == Kind::Zero;
    const bool zero_b = pb.kind == Kind::Zero;
    if (zero_a && zero_b) {
        return zero(pa.sign == pb.sign ? pa.sign : mode == MinusInfinity);
    }
    if (zero_a || zero_b) { // the other one, as it is (a denormal stays a denormal)
        const Parts& p = zero_a ? pb : pa;
        return round_pack(p.sign, p.exponent, p.mantissa, mode, fpscr);
    }
    // Both with 32 bits of room below, the smaller one moved to the other's exponent.
    std::uint64_t ma = pa.mantissa << 32;
    std::uint64_t mb = pb.mantissa << 32;
    int exponent = pa.exponent - 32;
    if (pa.exponent > pb.exponent) {
        mb = shift_right_jam(mb, static_cast<unsigned>(pa.exponent - pb.exponent));
    } else if (pb.exponent > pa.exponent) {
        ma = shift_right_jam(ma, static_cast<unsigned>(pb.exponent - pa.exponent));
        exponent = pb.exponent - 32;
    }
    if (pa.sign == pb.sign) {
        return round_pack(pa.sign, exponent, ma + mb, mode, fpscr);
    }
    if (ma == mb) {
        return zero(mode == MinusInfinity);
    }
    return ma > mb ? round_pack(pa.sign, exponent, ma - mb, mode, fpscr) : round_pack(pb.sign, exponent, mb - ma, mode, fpscr);
}

// Floor of the square root of a 64-bit number.
std::uint64_t integer_sqrt(std::uint64_t x)
{
    std::uint64_t r = static_cast<std::uint64_t>(std::sqrt(static_cast<double>(x)));
    while (r * r > x) { // the double's estimate may be one off either way
        --r;
    }
    while ((r + 1) * (r + 1) <= x) {
        ++r;
    }
    return r;
}

} // namespace

std::uint32_t add(std::uint32_t a, std::uint32_t b, std::uint32_t& fpscr)
{
    const Parts pa = unpack(a, fpscr);
    const Parts pb = unpack(b, fpscr);
    return add_parts(pa, a, pb, b, fpscr);
}

std::uint32_t sub(std::uint32_t a, std::uint32_t b, std::uint32_t& fpscr)
{
    const Parts pa = unpack(a, fpscr);
    Parts pb = unpack(b, fpscr);
    if (!is_nan(pb)) { // a NaN keeps its sign; anything else is negated
        pb.sign = !pb.sign;
    }
    return add_parts(pa, a, pb, b, fpscr);
}

std::uint32_t mul(std::uint32_t a, std::uint32_t b, std::uint32_t& fpscr)
{
    const Parts pa = unpack(a, fpscr);
    const Parts pb = unpack(b, fpscr);
    std::uint32_t nan = 0;
    if (process_nans(pa, a, pb, b, fpscr, nan)) {
        return nan;
    }
    const bool sign = pa.sign != pb.sign;
    const bool inf = pa.kind == Kind::Infinity || pb.kind == Kind::Infinity;
    const bool none = pa.kind == Kind::Zero || pb.kind == Kind::Zero;
    if (inf && none) {
        return invalid(fpscr);
    }
    if (inf) {
        return infinity(sign);
    }
    if (none) {
        return zero(sign);
    }
    return round_pack(sign, pa.exponent + pb.exponent, pa.mantissa * pb.mantissa, rounding(fpscr), fpscr);
}

std::uint32_t div(std::uint32_t a, std::uint32_t b, std::uint32_t& fpscr)
{
    const Parts pa = unpack(a, fpscr);
    const Parts pb = unpack(b, fpscr);
    std::uint32_t nan = 0;
    if (process_nans(pa, a, pb, b, fpscr, nan)) {
        return nan;
    }
    const bool sign = pa.sign != pb.sign;
    const bool inf_a = pa.kind == Kind::Infinity;
    const bool inf_b = pb.kind == Kind::Infinity;
    const bool zero_a = pa.kind == Kind::Zero;
    const bool zero_b = pb.kind == Kind::Zero;
    if ((inf_a && inf_b) || (zero_a && zero_b)) {
        return invalid(fpscr);
    }
    if (inf_a || zero_b) {
        if (!inf_a) {
            fpscr |= DZC;
        }
        return infinity(sign);
    }
    if (zero_a || inf_b) {
        return zero(sign);
    }
    // Both with the leading bit at bit 23, then 40 more bits of quotient; a remainder is sticky.
    const int la = std::countl_zero(pa.mantissa) - 40;
    const int lb = std::countl_zero(pb.mantissa) - 40;
    const std::uint64_t ma = pa.mantissa << la;
    const std::uint64_t mb = pb.mantissa << lb;
    std::uint64_t quotient = (ma << 40) / mb;
    if ((ma << 40) % mb != 0) {
        quotient |= 1u;
    }
    return round_pack(sign, (pa.exponent - la) - (pb.exponent - lb) - 40, quotient, rounding(fpscr), fpscr);
}

std::uint32_t sqrt(std::uint32_t a, std::uint32_t& fpscr)
{
    const Parts p = unpack(a, fpscr);
    if (is_nan(p)) {
        return process_nan(p, a, fpscr);
    }
    if (p.kind == Kind::Zero) {
        return zero(p.sign);
    }
    if (p.sign) {
        return invalid(fpscr);
    }
    if (p.kind == Kind::Infinity) {
        return infinity(false);
    }
    // The leading bit at bit 23 and an even exponent; then the root of the mantissa with 38 more bits.
    const int lead = std::countl_zero(p.mantissa) - 40;
    std::uint64_t mantissa = p.mantissa << lead;
    int exponent = p.exponent - lead;
    if (exponent & 1) {
        mantissa <<= 1;
        exponent -= 1;
    }
    const std::uint64_t radicand = mantissa << 38;
    std::uint64_t root = integer_sqrt(radicand);
    if (root * root != radicand) {
        root = (root << 1) | 1u; // one more bit, then the sticky one
        return round_pack(false, (exponent - 38) / 2 - 1, root, rounding(fpscr), fpscr);
    }
    return round_pack(false, (exponent - 38) / 2, root, rounding(fpscr), fpscr);
}

std::uint32_t mul_add(std::uint32_t addend, std::uint32_t a, std::uint32_t b, std::uint32_t& fpscr)
{
    const Parts pc = unpack(addend, fpscr);
    const Parts pa = unpack(a, fpscr);
    const Parts pb = unpack(b, fpscr);
    const bool inf_a = pa.kind == Kind::Infinity;
    const bool inf_b = pb.kind == Kind::Infinity;
    const bool zero_a = pa.kind == Kind::Zero;
    const bool zero_b = pb.kind == Kind::Zero;
    const bool bad_product = (inf_a && zero_b) || (zero_a && inf_b);
    // FPProcessNaNs3 with the addend first; a quiet NaN addend with infinity
    // times zero is the default NaN and an invalid operation.
    if (pc.kind == Kind::QuietNaN && bad_product) {
        return invalid(fpscr);
    }
    if (pc.kind == Kind::SignalingNaN) {
        return process_nan(pc, addend, fpscr);
    }
    if (pa.kind == Kind::SignalingNaN) {
        return process_nan(pa, a, fpscr);
    }
    if (pb.kind == Kind::SignalingNaN) {
        return process_nan(pb, b, fpscr);
    }
    if (pc.kind == Kind::QuietNaN) {
        return process_nan(pc, addend, fpscr);
    }
    if (pa.kind == Kind::QuietNaN) {
        return process_nan(pa, a, fpscr);
    }
    if (pb.kind == Kind::QuietNaN) {
        return process_nan(pb, b, fpscr);
    }
    const unsigned mode = rounding(fpscr);
    const bool inf_c = pc.kind == Kind::Infinity;
    const bool zero_c = pc.kind == Kind::Zero;
    const bool inf_p = inf_a || inf_b;
    const bool zero_p = zero_a || zero_b;
    const bool sign_p = pa.sign != pb.sign;
    if (bad_product || (inf_c && inf_p && pc.sign != sign_p)) {
        return invalid(fpscr);
    }
    if (inf_c || inf_p) {
        return infinity(inf_c ? pc.sign : sign_p);
    }
    if (zero_c && zero_p) {
        return zero(pc.sign == sign_p ? pc.sign : mode == MinusInfinity);
    }
    if (zero_p) {
        return round_pack(pc.sign, pc.exponent, pc.mantissa, mode, fpscr);
    }
    const std::uint64_t product = pa.mantissa * pb.mantissa; // up to 48 bits, exact
    const int product_exponent = pa.exponent + pb.exponent;
    if (zero_c) {
        return round_pack(sign_p, product_exponent, product, mode, fpscr);
    }
    // Both with the leading bit at bit 61; the smaller one moved to the other's
    // exponent. Bits are lost only when the two are far apart, where at most
    // one leading bit cancels, far above the sticky bit.
    const int lp = std::countl_zero(product) - 2;
    const int lc = std::countl_zero(pc.mantissa) - 2;
    std::uint64_t mp = product << lp;
    std::uint64_t mc = pc.mantissa << lc;
    const int ep = product_exponent - lp;
    const int ec = pc.exponent - lc;
    int exponent = ep;
    if (ep > ec) {
        mc = shift_right_jam(mc, static_cast<unsigned>(ep - ec));
    } else if (ec > ep) {
        mp = shift_right_jam(mp, static_cast<unsigned>(ec - ep));
        exponent = ec;
    }
    if (sign_p == pc.sign) {
        return round_pack(sign_p, exponent, mp + mc, mode, fpscr);
    }
    if (mp == mc) {
        return zero(mode == MinusInfinity);
    }
    return mp > mc ? round_pack(sign_p, exponent, mp - mc, mode, fpscr) : round_pack(pc.sign, exponent, mc - mp, mode, fpscr);
}

std::uint32_t compare(std::uint32_t a, std::uint32_t b, bool signal_all, std::uint32_t& fpscr)
{
    const Parts pa = unpack(a, fpscr);
    const Parts pb = unpack(b, fpscr);
    if (is_nan(pa) || is_nan(pb)) {
        if (signal_all || pa.kind == Kind::SignalingNaN || pb.kind == Kind::SignalingNaN) {
            fpscr |= IOC;
        }
        return 0x30000000u; // unordered: C and V
    }
    // The order of the values: sign and magnitude, with both zeros equal and
    // (in flush-to-zero mode) a denormal as zero.
    auto key = [](const Parts& p, std::uint32_t bits) -> std::int64_t {
        if (p.kind == Kind::Zero) {
            return 0;
        }
        const std::int64_t magnitude = bits & 0x7FFFFFFFu;
        return p.sign ? -magnitude : magnitude;
    };
    const std::int64_t ka = key(pa, a);
    const std::int64_t kb = key(pb, b);
    return ka == kb ? 0x60000000u : ka < kb ? 0x80000000u : 0x20000000u;
}

std::uint32_t to_fixed(std::uint32_t a, unsigned size, unsigned fraction_bits, bool is_unsigned, bool round_zero,
    std::uint32_t& fpscr)
{
    const Parts p = unpack(a, fpscr);
    const std::int64_t highest = is_unsigned ? (std::int64_t{1} << size) - 1 : (std::int64_t{1} << (size - 1)) - 1;
    const std::int64_t lowest = is_unsigned ? 0 : -(std::int64_t{1} << (size - 1));
    if (is_nan(p)) {
        fpscr |= IOC;
        return 0;
    }
    if (p.kind == Kind::Zero) {
        return 0;
    }
    if (p.kind == Kind::Infinity) {
        fpscr |= IOC;
        return static_cast<std::uint32_t>(p.sign ? lowest : highest);
    }
    const unsigned mode = round_zero ? static_cast<unsigned>(Zero) : rounding(fpscr);
    const int exponent = p.exponent + static_cast<int>(fraction_bits);
    std::uint64_t whole = 0;
    bool inexact = false;
    if (exponent >= 40) { // past any 32-bit range
        fpscr |= IOC;
        return static_cast<std::uint32_t>(p.sign ? lowest : highest);
    }
    if (exponent >= 0) {
        whole = p.mantissa << exponent;
    } else {
        const unsigned n = static_cast<unsigned>(-exponent);
        std::uint64_t rest = 0;
        std::uint64_t half_way = 0;
        if (n >= 63) { // far below one half
            rest = 1;
            half_way = 2;
        } else {
            whole = p.mantissa >> n;
            rest = p.mantissa & ((std::uint64_t{1} << n) - 1u);
            half_way = std::uint64_t{1} << (n - 1);
        }
        inexact = rest != 0;
        bool up = false;
        switch (mode) {
        case Nearest: up = rest > half_way || (rest == half_way && (whole & 1u)); break;
        case PlusInfinity: up = inexact && !p.sign; break;
        case MinusInfinity: up = inexact && p.sign; break;
        default: break;
        }
        whole += up ? 1u : 0u;
    }
    const std::int64_t value = p.sign ? -static_cast<std::int64_t>(whole) : static_cast<std::int64_t>(whole);
    if (value > highest || value < lowest) {
        fpscr |= IOC;
        return static_cast<std::uint32_t>(value > highest ? highest : lowest);
    }
    if (inexact) {
        fpscr |= IXC;
    }
    return static_cast<std::uint32_t>(value);
}

std::uint32_t from_fixed(std::uint32_t value, unsigned size, unsigned fraction_bits, bool is_unsigned, bool round_nearest,
    std::uint32_t& fpscr)
{
    std::int64_t number = 0;
    if (size == 16) {
        number = is_unsigned ? static_cast<std::int64_t>(value & 0xFFFFu) : static_cast<std::int16_t>(value & 0xFFFFu);
    } else {
        number = is_unsigned ? static_cast<std::int64_t>(value) : static_cast<std::int32_t>(value);
    }
    if (number == 0) {
        return 0;
    }
    const bool sign = number < 0;
    const std::uint64_t magnitude = static_cast<std::uint64_t>(sign ? -number : number);
    return round_pack(sign, -static_cast<int>(fraction_bits), magnitude,
        round_nearest ? static_cast<unsigned>(Nearest) : rounding(fpscr), fpscr);
}

std::uint32_t half_to_single(std::uint16_t value, std::uint32_t& fpscr)
{
    const bool sign = (value >> 15) != 0;
    const std::uint32_t exponent = (value >> 10) & 0x1Fu;
    const std::uint32_t fraction = value & 0x3FFu;
    if (exponent == 0) {
        if (fraction == 0) {
            return zero(sign);
        }
        return round_pack(sign, -24, fraction, Nearest, fpscr); // exact: a half denormal is a single normal
    }
    if (exponent == 0x1F && !(fpscr & AHP)) {
        if (fraction == 0) {
            return infinity(sign);
        }
        if (!(fraction & 0x200u)) { // signaling
            fpscr |= IOC;
        }
        return (fpscr & DN) ? default_nan : zero(sign) | 0x7FC00000u | (fraction << 13);
    }
    return zero(sign) | ((exponent + 112u) << 23) | (fraction << 13);
}

std::uint16_t single_to_half(std::uint32_t a, std::uint32_t& fpscr)
{
    // The input is not flushed to zero here: the conversion sees denormals as they are.
    std::uint32_t no_flush = fpscr & ~FZ;
    const Parts p = unpack(a, no_flush);
    const bool ahp = (fpscr & AHP) != 0;
    const std::uint16_t sign_bit = p.sign ? 0x8000u : 0u;
    if (is_nan(p)) {
        if (ahp) {
            fpscr |= IOC;
            return sign_bit; // no NaN in the alternative format: zero
        }
        if (p.kind == Kind::SignalingNaN) {
            fpscr |= IOC;
        }
        if (fpscr & DN) {
            return 0x7E00u;
        }
        return static_cast<std::uint16_t>(sign_bit | 0x7E00u | ((a >> 13) & 0x1FFu));
    }
    if (p.kind == Kind::Infinity) {
        if (ahp) {
            fpscr |= IOC;
            return static_cast<std::uint16_t>(sign_bit | 0x7FFFu);
        }
        return static_cast<std::uint16_t>(sign_bit | 0x7C00u);
    }
    if (p.kind == Kind::Zero) {
        return sign_bit;
    }
    // Rounded with FPSCR's mode; half precision is never flushed to zero.
    std::uint32_t flags = fpscr & ~FZ;
    const std::uint32_t result = round_pack(p.sign, p.exponent, p.mantissa, rounding(fpscr), flags, half, ahp);
    fpscr |= flags & (IOC | DZC | OFC | UFC | IXC);
    return static_cast<std::uint16_t>(result);
}

} // namespace simnodus::emu::fp
