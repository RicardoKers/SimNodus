// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The ESP32-C3's 384 KiB mask ROM, rebuilt from Espressif's ROM ELF
// (third_party/esp-rom-elfs, Apache-2.0; experiment E-08). The ELF's
// program headers do not cover the whole ROM: the read-only data of
// `.rodata.interface` and the initial values of the ROM's own RAM data exist
// only as sections. The image is the ROM and data-ROM sections at their
// addresses, plus each RAM data section copied to the place in ROM that the
// ROM's start-up code copies it from: the table that `_init` walks
// (`_data_end_btdm_rom` to `_data_end`, 16 bytes per entry: start, end,
// source, unused) and the Bluetooth ROM's entry just before it (12 bytes).
// For esp32c3_rev3_rom.elf (esp-rom-elfs 20260528) the result equals a ROM
// read from a chip, SHA-256 0de1e650...73f99 (docs/experiments/E-08-results.md).
#pragma once

#include "elf.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace simnodus::emu {

inline constexpr std::uint32_t esp32c3_rom_base = 0x40000000u;  // instruction bus
inline constexpr std::uint32_t esp32c3_rom_size = 0x60000u;     // 384 KiB
inline constexpr std::uint32_t esp32c3_drom_base = 0x3FF00000u; // data bus view of the last 128 KiB
inline constexpr std::uint32_t esp32c3_drom_offset = 0x40000u;
inline constexpr std::uint32_t esp32c3_drom_size = 0x20000u;

// Returns false with a sentence in `error` if the ELF is not an ESP32-C3 ROM.
bool build_esp32c3_rom(const ElfImage& elf, std::vector<std::uint8_t>& rom, std::string& error);

} // namespace simnodus::emu
