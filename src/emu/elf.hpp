// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ELF32 (ARM or RISC-V, little-endian, executable) reader for firmware images:
// loadable segments at their physical addresses (as debuggers load them:
// initialized data lands in flash and the start-up code copies it to SRAM),
// function symbols for diagnostics and the addresses of named symbols.
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

enum class ElfMachine { Arm, Riscv };

// A section with contents in the file and an address (PROGBITS). The
// ESP32-C3's ROM ELF keeps some of the ROM only in sections (esp32c3_rom.hpp).
struct ElfSection {
    std::string name;
    std::uint32_t address = 0;
    std::vector<std::uint8_t> data;
};

struct ElfImage {
    std::uint32_t entry = 0;
    std::vector<ElfSegment> segments;
    std::vector<ElfSection> sections;
    std::vector<ElfSymbol> functions; // sorted by address
    std::map<std::string, std::uint32_t> symbols; // every named symbol with a value

    // Function containing `address`, if the ELF has symbols for it.
    const ElfSymbol* function_at(std::uint32_t address) const;
    std::optional<std::uint32_t> symbol(const std::string& name) const;
};

// Parses an ELF image built for `machine`; on failure returns nullopt and sets
// `error` to a sentence for the user.
std::optional<ElfImage> parse_elf(
    const std::vector<std::uint8_t>& bytes, std::string& error, ElfMachine machine = ElfMachine::Arm);
std::optional<ElfImage> read_elf(
    const std::filesystem::path& path, std::string& error, ElfMachine machine = ElfMachine::Arm);

} // namespace simnodus::emu
