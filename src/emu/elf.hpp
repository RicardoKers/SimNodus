// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ELF32 (ARM, little-endian, executable) reader for firmware images: loadable
// segments at their physical addresses (as debuggers load them:
// initialized data lands in flash and the start-up code copies it to SRAM)
// and function symbols for diagnostics.
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::emu {

struct ElfSegment {
    std::uint32_t physical = 0; // load address
    std::uint32_t virtual_address = 0;
    std::vector<std::uint8_t> data; // file bytes
    std::uint32_t memory_size = 0;  // >= data.size(); the rest is zero
    bool executable = false;
};

struct ElfSymbol {
    std::string name;
    std::uint32_t address = 0; // without the Thumb bit
    std::uint32_t size = 0;
};

struct ElfImage {
    std::uint32_t entry = 0;
    std::vector<ElfSegment> segments;
    std::vector<ElfSymbol> functions; // sorted by address

    // Function containing `address`, if the ELF has symbols for it.
    const ElfSymbol* function_at(std::uint32_t address) const;
};

// Parses an ELF image; on failure returns nullopt and sets `error` to a
// sentence for the user.
std::optional<ElfImage> parse_elf(const std::vector<std::uint8_t>& bytes, std::string& error);
std::optional<ElfImage> read_elf(const std::filesystem::path& path, std::string& error);

} // namespace simnodus::emu
