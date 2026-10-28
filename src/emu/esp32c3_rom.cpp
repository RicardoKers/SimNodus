// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "esp32c3_rom.hpp"

#include <algorithm>
#include <cstring>

namespace simnodus::emu {

namespace {

constexpr std::uint32_t dram_base = 0x3FC80000u;
constexpr std::uint32_t dram_end = 0x3FCE0000u;

std::uint32_t word_at(const std::vector<std::uint8_t>& rom, std::uint32_t address)
{
    std::uint32_t value = 0;
    std::memcpy(&value, rom.data() + (address - esp32c3_rom_base), 4);
    return value;
}

} // namespace

bool build_esp32c3_rom(const ElfImage& elf, std::vector<std::uint8_t>& rom, std::string& error)
{
    rom.assign(esp32c3_rom_size, 0);
    std::vector<const ElfSection*> ram;
    for (const ElfSection& section : elf.sections) {
        const std::uint64_t end = std::uint64_t{section.address} + section.data.size();
        if (section.address >= esp32c3_rom_base && end <= esp32c3_rom_base + std::uint64_t{esp32c3_rom_size}) {
            std::memcpy(rom.data() + (section.address - esp32c3_rom_base), section.data.data(), section.data.size());
        } else if (section.address >= esp32c3_drom_base && end <= esp32c3_drom_base + std::uint64_t{esp32c3_drom_size}) {
            std::memcpy(rom.data() + esp32c3_drom_offset + (section.address - esp32c3_drom_base), section.data.data(),
                section.data.size());
        } else if (section.address >= dram_base && end <= dram_end) {
            ram.push_back(&section);
        }
    }
    const auto btdm = elf.symbol("_data_start_btdm_rom");
    const auto first = elf.symbol("_data_end_btdm_rom");
    const auto last = elf.symbol("_data_end");
    if (!btdm || !first || !last || *btdm < esp32c3_rom_base + 8 || *first > *last
        || *last > esp32c3_rom_base + esp32c3_rom_size - 16 || word_at(rom, esp32c3_rom_base) == 0) {
        error = "The file is not an ESP32-C3 ROM ELF (its start-up copy table was not found).";
        return false;
    }
    struct Copy {
        std::uint32_t start, end, source;
    };
    std::vector<Copy> copies{{word_at(rom, *btdm - 8), word_at(rom, *btdm - 4), word_at(rom, *btdm)}};
    for (std::uint32_t entry = *first; entry < *last; entry += 16) {
        copies.push_back({word_at(rom, entry), word_at(rom, entry + 4), word_at(rom, entry + 8)});
    }
    for (const Copy& copy : copies) {
        for (const ElfSection* section : ram) {
            const std::uint32_t lo = std::max(copy.start, section->address);
            const std::uint32_t hi = std::min(copy.end, section->address + static_cast<std::uint32_t>(section->data.size()));
            const std::uint32_t at = copy.source + (lo - copy.start);
            if (lo >= hi || at < esp32c3_rom_base || at - esp32c3_rom_base + (hi - lo) > esp32c3_rom_size) {
                continue;
            }
            std::memcpy(rom.data() + (at - esp32c3_rom_base), section->data.data() + (lo - section->address), hi - lo);
        }
    }
    return true;
}

} // namespace simnodus::emu
