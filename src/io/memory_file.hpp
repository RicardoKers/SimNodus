// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Contents of memory chips in files (SN-208): raw binary (.bin) and Intel HEX
// (.hex, the format of EEPROM programmers and of STM32CubeIDE's hex output).
// Reading a file only parses data.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::io {

// The bytes of a .bin or .hex file (by extension, or by content: a file whose
// first character is ':' is Intel HEX). `size` bytes, starting erased
// (`fill`); data beyond `size` is an error.
std::optional<std::vector<std::uint8_t>> read_memory_file(const std::filesystem::path& file, std::size_t size,
    std::uint8_t fill, std::string& error);

// Intel HEX text of `bytes` from address 0 (16 bytes per record, an end record).
std::string to_intel_hex(const std::vector<std::uint8_t>& bytes);
// Parses Intel HEX text into `bytes` (already sized and filled); false with `error`.
bool parse_intel_hex(const std::string& text, std::vector<std::uint8_t>& bytes, std::string& error);

// Writes .bin (raw) or .hex (Intel HEX) by the file's extension.
bool write_memory_file(const std::filesystem::path& file, const std::vector<std::uint8_t>& bytes, std::string& error);

} // namespace simnodus::io
