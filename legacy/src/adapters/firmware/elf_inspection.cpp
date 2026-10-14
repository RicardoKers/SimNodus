// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adapters/firmware/elf_inspection.hpp"
#include <new>

namespace simnodus::firmware {
namespace {
std::uint32_t u32(const std::string& bytes, std::size_t at)
{
    std::uint32_t value = 0;
    for(unsigned i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at + i])) << (8 * i);
    return value;
}
std::uint16_t u16(const std::string& bytes, std::size_t at)
{
    return static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[at]) |
        (static_cast<unsigned>(static_cast<unsigned char>(bytes[at + 1])) << 8));
}
bool fits(std::uint64_t start, std::uint64_t length, std::uint64_t limit)
{
    return start <= limit && length <= limit - start;
}
}
ElfInspectionResult inspect_elf32(std::string_view input)
{
    try {
        if(input.size() > elf_max_bytes) return ElfInspectionError{"bytes", 0};
        if(input.size() < 52) return ElfInspectionError{"header", input.size()};
        auto result = std::make_shared<Elf32Inspection>();
        result->bytes.assign(input);
        const auto& b = result->bytes;
        if(b.compare(0, 4, "\x7f" "ELF") != 0) return ElfInspectionError{"magic", 0};
        if(b[4] != 1) return ElfInspectionError{"class", 4};
        if(b[5] != 1) return ElfInspectionError{"encoding", 5};
        if(b[6] != 1) return ElfInspectionError{"version", 6};
        if(u16(b, 16) != 2) return ElfInspectionError{"type", 16};
        if(u32(b, 20) != 1) return ElfInspectionError{"version", 20};
        if(u16(b, 40) != 52) return ElfInspectionError{"header-size", 40};
        const auto phoff = u32(b, 28);
        const auto phnum = u16(b, 44);
        if(phnum == 0 || phnum > elf_max_programs) return ElfInspectionError{"program-count", 44};
        if(u16(b, 42) != 32) return ElfInspectionError{"program-size", 42};
        if(phoff < 52 || !fits(phoff, static_cast<std::uint64_t>(phnum) * 32, b.size()))
            return ElfInspectionError{"program-table", 28};
        result->machine = u16(b, 18);
        result->os_abi = static_cast<std::uint8_t>(b[7]);
        result->abi_version = static_cast<std::uint8_t>(b[8]);
        result->entry = u32(b, 24);
        result->flags = u32(b, 36);
        result->section_table_offset = u32(b, 32);
        result->section_count = u16(b, 48);
        result->section_names_index = u16(b, 50);
        // Only the section table envelope is checked. No section is interpreted.
        if(result->section_count == 0) {
            if(result->section_table_offset != 0 || result->section_names_index != 0)
                return ElfInspectionError{"sections", 32};
        } else {
            if(result->section_count > elf_max_sections) return ElfInspectionError{"section-count", 48};
            if(u16(b, 46) != 40) return ElfInspectionError{"section-size", 46};
            if(result->section_names_index >= result->section_count) return ElfInspectionError{"section-index", 50};
            if(result->section_table_offset < 52 || !fits(result->section_table_offset,
                static_cast<std::uint64_t>(result->section_count) * 40, b.size()))
                return ElfInspectionError{"section-table", 32};
        }
        bool load_seen = false;
        std::uint32_t previous_address = 0;
        for(std::size_t i = 0; i < phnum; ++i) {
            const auto at = static_cast<std::size_t>(phoff) + i * 32;
            ElfProgram p{u32(b, at), u32(b, at + 4), u32(b, at + 8), u32(b, at + 12),
                u32(b, at + 16), u32(b, at + 20), u32(b, at + 24), u32(b, at + 28), at};
            if(p.type == 2 || p.type == 3 || p.type == 5 || p.type == 7)
                return ElfInspectionError{"unsupported-program", at};
            // PT_NULL fields are unspecified and are retained without use.
            if(p.type != 0 && !fits(p.file_offset, p.file_size, b.size()))
                return ElfInspectionError{"segment-file", at + 4};
            if(p.type == 1) {
                if(p.file_size > p.memory_size) return ElfInspectionError{"segment-size", at + 16};
                constexpr std::uint64_t address_space = std::uint64_t{1} << 32;
                if(!fits(p.virtual_address, p.memory_size, address_space))
                    return ElfInspectionError{"virtual-range", at + 8};
                if(!fits(p.physical_address, p.memory_size, address_space))
                    return ElfInspectionError{"physical-range", at + 12};
                if(p.alignment > 1 && ((p.alignment & (p.alignment - 1)) != 0 ||
                    p.virtual_address % p.alignment != p.file_offset % p.alignment))
                    return ElfInspectionError{"alignment", at + 28};
                if(load_seen && p.virtual_address < previous_address)
                    result->load_ordered = false;
                load_seen = true;previous_address = p.virtual_address;
            }
            result->programs.push_back(p);
        }
        if(!load_seen) return ElfInspectionError{"load-absent", 44};
        return std::shared_ptr<const Elf32Inspection>(std::move(result));
    } catch(const std::bad_alloc&) { return ElfInspectionError{"memory", 0}; }
}
}
