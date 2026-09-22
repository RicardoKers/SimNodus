// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adapters/firmware/boot_candidate.hpp"
#include <new>

namespace simnodus::firmware {
namespace {
constexpr std::uint64_t flash = 0x08000000, flash_end = flash + 0x10000;
constexpr std::uint64_t ram = 0x20000000, ram_end = ram + 0x5000;
bool inside(std::uint64_t start, std::uint64_t size, std::uint64_t low, std::uint64_t high)
{ return start >= low && start <= high && size <= high - start; }
bool overlaps(std::uint64_t a, std::uint64_t n, std::uint64_t b, std::uint64_t m)
{ return n != 0 && m != 0 && a < b + m && b < a + n; }
std::uint32_t word(const std::string& b, std::size_t offset)
{
    std::uint32_t value = 0;
    for(unsigned i = 0; i < 4; ++i)
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(b[offset + i])) << (8 * i);
    return value;
}
}
BootCandidateResult inspect_boot_candidate(std::string_view image,
    std::string_view architecture, BootProfile profile)
{
    try {
        if(profile != BootProfile::sn012_stm32f103c8) return BootCandidateError{"request", "profile", 0};
        if(architecture != "arm-cortex-m3") return BootCandidateError{"request", "architecture", 0};
        const auto parsed = inspect_elf32(image);
        if(const auto* e = std::get_if<ElfInspectionError>(&parsed)) return BootCandidateError{"elf", e->code, e->offset};
        const auto elf = std::get<0>(parsed);
        const auto require = [](bool ok, const char* code, std::size_t offset) {
            if(!ok) throw BootCandidateError{"candidate", code, offset};
        };
        require(elf->machine == 40, "machine", 18);
        require(elf->os_abi == 0 && elf->abi_version == 0, "abi", 7);
        // Exact observed SN-012 flags, not a general ARM ABI compatibility rule.
        require(elf->flags == 0x05000200, "flags", 36);
        const ElfProgram* vectors = nullptr;
        for(std::size_t i = 0; i < elf->programs.size(); ++i) {
            const auto& p = elf->programs[i];
            require(p.type == 1, "program", p.header_offset);
            require(p.memory_size != 0, "empty", p.header_offset + 20);
            if(p.flags == 5) {
                require(p.file_size == p.memory_size && p.physical_address == p.virtual_address &&
                    inside(p.virtual_address, p.memory_size, flash, flash_end), "code-region", p.header_offset);
                if(p.virtual_address == flash && p.file_size >= 8) vectors = &p;
            } else {
                require(p.flags == 6, "permissions", p.header_offset + 24);
                require(inside(p.virtual_address, p.memory_size, ram, ram_end), "data-region", p.header_offset);
                require(p.file_size == 0 ? p.physical_address == p.virtual_address :
                    inside(p.physical_address, p.file_size, flash, flash_end), "data-source", p.header_offset + 12);
            }
            for(std::size_t j = 0; j < i; ++j) {
                const auto& q = elf->programs[j];
                require(!overlaps(p.virtual_address, p.memory_size, q.virtual_address, q.memory_size), "virtual-overlap", p.header_offset);
                require(!overlaps(p.physical_address, p.file_size, q.physical_address, q.file_size), "physical-overlap", p.header_offset);
                require(!overlaps(p.file_offset, p.file_size, q.file_offset, q.file_size), "file-overlap", p.header_offset);
            }
        }
        require(vectors != nullptr, "vectors", 44);
        const auto at = static_cast<std::size_t>(vectors->file_offset);
        const auto stack = word(elf->bytes, at), reset = word(elf->bytes, at + 4);
        require(stack > ram && stack <= ram_end && stack % 8 == 0, "stack", at);
        require(stack >= ram + 1024, "stack-reserve", at);
        for(const auto& p : elf->programs)
            if(p.flags == 6) require(static_cast<std::uint64_t>(p.virtual_address) + p.memory_size <= stack - 1024ULL,
                "stack-reserve", p.header_offset);
        require((reset & 1) != 0 && reset == elf->entry, "reset", at + 4);
        bool executable = false;
        for(const auto& p : elf->programs)
            if(p.flags == 5 && inside(reset & ~std::uint32_t{1}, 2, p.virtual_address,
                static_cast<std::uint64_t>(p.virtual_address) + p.file_size)) executable = true;
        require(executable, "reset-region", at + 4);
        return std::make_shared<const BootCandidate>(BootCandidate{elf, profile,
            std::string(architecture), stack, reset, at});
    } catch(const BootCandidateError& e) { return e; }
    catch(const std::bad_alloc&) { return BootCandidateError{"operation", "memory", 0}; }
}
}
