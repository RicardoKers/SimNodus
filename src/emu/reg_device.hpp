// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Base of the STM32 device models whose registers are 32-bit words. Internal
// to src/emu.
#pragma once

#include "bus.hpp"

#include <cstdint>

namespace simnodus::emu {

// A device whose registers are 32-bit words; byte and halfword accesses become
// partial writes (`mask`) and reads of the containing word, as the APB bridge does.
class RegDevice : public Device {
public:
    bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override
    {
        std::uint32_t word = 0;
        if (!read32(offset & ~3u, word)) {
            return false;
        }
        const unsigned shift = 8 * (offset & 3u);
        value = (word >> shift) & (size == 4 ? 0xFFFFFFFFu : ((1u << (8 * size)) - 1u));
        return true;
    }
    bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override
    {
        const unsigned shift = 8 * (offset & 3u);
        const std::uint32_t mask = (size == 4 ? 0xFFFFFFFFu : ((1u << (8 * size)) - 1u)) << shift;
        return write32(offset & ~3u, (value << shift) & mask, mask);
    }

protected:
    virtual bool read32(std::uint32_t offset, std::uint32_t& value) = 0;
    // `value` is already in position; only bits in `mask` are written.
    virtual bool write32(std::uint32_t offset, std::uint32_t value, std::uint32_t mask) = 0;
    static std::uint32_t merge(std::uint32_t old, std::uint32_t value, std::uint32_t mask)
    {
        return (old & ~mask) | (value & mask);
    }
};

} // namespace simnodus::emu
