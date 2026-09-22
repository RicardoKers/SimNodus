// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace simnodus::firmware {
inline constexpr std::size_t elf_max_bytes = 16 * 1024 * 1024;
inline constexpr std::size_t elf_max_programs = 64;
inline constexpr std::size_t elf_max_sections = 4096;
struct ElfProgram {
    std::uint32_t type, file_offset, virtual_address, physical_address;
    std::uint32_t file_size, memory_size, flags, alignment;
    std::size_t header_offset;
};
struct Elf32Inspection {
    std::string bytes;
    std::uint16_t machine;
    std::uint8_t os_abi, abi_version;
    std::uint32_t entry, flags, section_table_offset;
    std::uint16_t section_count, section_names_index;
    bool load_ordered = true;
    std::vector<ElfProgram> programs;
};
struct ElfInspectionError { const char* code; std::size_t offset; };
using ElfInspectionResult = std::variant<std::shared_ptr<const Elf32Inspection>, ElfInspectionError>;
// Pure bounded inspection of caller-supplied bytes, copied before parsing.
// No filesystem access, section-content validation, memory mapping, device/boot
// compatibility or execution permission. Borrowed input must stay stable in call.
ElfInspectionResult inspect_elf32(std::string_view bytes);
}
