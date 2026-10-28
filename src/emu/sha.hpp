// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// SHA-1 and SHA-224/256 compression functions (FIPS 180-4), for the
// ESP32-C3's SHA accelerator (experiment E-08). The state is the eight (five
// for SHA-1) hash words; a block is 64 bytes in message order.
#pragma once

#include <array>
#include <cstdint>

namespace simnodus::emu {

using ShaState = std::array<std::uint32_t, 8>;

ShaState sha1_initial();
ShaState sha224_initial();
ShaState sha256_initial();
void sha1_compress(ShaState& state, const std::uint8_t* block);
void sha256_compress(ShaState& state, const std::uint8_t* block); // also SHA-224

} // namespace simnodus::emu
