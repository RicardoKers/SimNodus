// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Single-precision floating point as the Cortex-M4's FPU (FPv4-SP) computes
// it (experiment E-12, SN-293), in integer arithmetic: the host's own float
// is not used, because its NaN results, its tininess detection and its
// default NaN differ from the ARM ones. Written from the pseudocode of the
// ARMv7-M Architecture Reference Manual (DDI 0403E, A2.5 and the shared
// floating-point functions): FPUnpack, FPProcessNaNs, FPRound, FPAdd, FPMul,
// FPDiv, FPSqrt, FPMulAdd, FPCompare, FPToFixed, FixedToFP, FPHalfToSingle,
// FPSingleToHalf.
//
// Values are the 32 bits of a register. `fpscr` gives the rounding mode
// (bits 23:22), flush-to-zero (24), default NaN (25) and the alternative
// half-precision format (26), and collects the cumulative exception flags.
#pragma once

#include <cstdint>

namespace simnodus::emu::fp {

// FPSCR bits.
inline constexpr std::uint32_t IOC = 1u << 0; // invalid operation
inline constexpr std::uint32_t DZC = 1u << 1; // division by zero
inline constexpr std::uint32_t OFC = 1u << 2; // overflow
inline constexpr std::uint32_t UFC = 1u << 3; // underflow
inline constexpr std::uint32_t IXC = 1u << 4; // inexact
inline constexpr std::uint32_t IDC = 1u << 7; // input denormal (flush-to-zero mode)
inline constexpr std::uint32_t FZ = 1u << 24;
inline constexpr std::uint32_t DN = 1u << 25;
inline constexpr std::uint32_t AHP = 1u << 26;
inline constexpr unsigned rmode_shift = 22;
// Rounding modes (FPSCR.RMode).
enum Rounding : unsigned { Nearest = 0, PlusInfinity = 1, MinusInfinity = 2, Zero = 3 };

inline constexpr std::uint32_t default_nan = 0x7FC00000u;

std::uint32_t add(std::uint32_t a, std::uint32_t b, std::uint32_t& fpscr);
std::uint32_t sub(std::uint32_t a, std::uint32_t b, std::uint32_t& fpscr);
std::uint32_t mul(std::uint32_t a, std::uint32_t b, std::uint32_t& fpscr);
std::uint32_t div(std::uint32_t a, std::uint32_t b, std::uint32_t& fpscr);
std::uint32_t sqrt(std::uint32_t a, std::uint32_t& fpscr);
// addend + a * b with one rounding (VFMA and its family).
std::uint32_t mul_add(std::uint32_t addend, std::uint32_t a, std::uint32_t b, std::uint32_t& fpscr);

// VCMP: the N, Z, C, V flags in bits 31..28. `signal_all`: any NaN is an
// invalid operation (VCMPE); otherwise only a signaling one.
std::uint32_t compare(std::uint32_t a, std::uint32_t b, bool signal_all, std::uint32_t& fpscr);

// To an integer or fixed-point number of `size` bits (16 or 32) with
// `fraction_bits` after the point, saturated; the result fills 32 bits (sign
// or zero extended). `round_zero`: toward zero, otherwise FPSCR's mode.
std::uint32_t to_fixed(std::uint32_t a, unsigned size, unsigned fraction_bits, bool is_unsigned, bool round_zero,
    std::uint32_t& fpscr);
// From an integer or fixed-point number. `round_nearest`: to nearest, otherwise FPSCR's mode.
std::uint32_t from_fixed(std::uint32_t value, unsigned size, unsigned fraction_bits, bool is_unsigned, bool round_nearest,
    std::uint32_t& fpscr);

std::uint32_t half_to_single(std::uint16_t half, std::uint32_t& fpscr);
std::uint16_t single_to_half(std::uint32_t a, std::uint32_t& fpscr);

// VMOV's immediate (VFPExpandImm).
constexpr std::uint32_t expand_imm(std::uint32_t imm8)
{
    const std::uint32_t a = (imm8 >> 7) & 1u;
    const std::uint32_t b = (imm8 >> 6) & 1u;
    return (a << 31) | ((b ^ 1u) << 30) | ((b ? 0x1Fu : 0u) << 25) | ((imm8 & 0x3Fu) << 19);
}

} // namespace simnodus::emu::fp
