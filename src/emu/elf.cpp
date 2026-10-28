// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "elf.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace simnodus::emu {

namespace {

constexpr std::uint16_t et_exec = 2;
constexpr std::uint16_t em_arm = 40;
constexpr std::uint16_t em_riscv = 243;
constexpr std::uint32_t pt_load = 1;
constexpr std::uint32_t pf_x = 1;
constexpr std::uint32_t sht_symtab = 2;
constexpr std::uint8_t stt_func = 2;
constexpr std::uint8_t stt_section = 3;
constexpr std::uint8_t stt_file = 4;
constexpr std::size_t max_image = 64u * 1024u * 1024u;

template <typename T>
bool get(const std::vector<std::uint8_t>& bytes, std::size_t offset, T& value)
{
    if (offset > bytes.size() || bytes.size() - offset < sizeof(T)) {
        return false;
    }
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return true;
}

} // namespace

const ElfSymbol* ElfImage::function_at(std::uint32_t address) const
{
    auto it = std::upper_bound(functions.begin(), functions.end(), address,
        [](std::uint32_t a, const ElfSymbol& s) { return a < s.address; });
    if (it == functions.begin()) {
        return nullptr;
    }
    --it;
    const std::uint32_t size = std::max<std::uint32_t>(it->size, 2);
    return address - it->address < size ? &*it : nullptr;
}

std::optional<std::uint32_t> ElfImage::symbol(const std::string& name) const
{
    const auto it = symbols.find(name);
    if (it == symbols.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<ElfImage> parse_elf(const std::vector<std::uint8_t>& bytes, std::string& error, ElfMachine expected)
{
    if (bytes.size() < 52 || std::memcmp(bytes.data(), "\x7f" "ELF", 4) != 0) {
        error = "The file is not an ELF file.";
        return std::nullopt;
    }
    if (bytes[4] != 1 || bytes[5] != 1) {
        error = expected == ElfMachine::Arm
            ? "The ELF file is not a 32-bit little-endian image (expected one built for an STM32)."
            : "The ELF file is not a 32-bit little-endian image (expected one built for a RISC-V microcontroller).";
        return std::nullopt;
    }
    std::uint16_t type = 0;
    std::uint16_t machine = 0;
    std::uint32_t entry = 0;
    std::uint32_t phoff = 0;
    std::uint32_t shoff = 0;
    std::uint16_t phentsize = 0;
    std::uint16_t phnum = 0;
    std::uint16_t shentsize = 0;
    std::uint16_t shnum = 0;
    get(bytes, 16, type);
    get(bytes, 18, machine);
    get(bytes, 24, entry);
    get(bytes, 28, phoff);
    get(bytes, 32, shoff);
    get(bytes, 42, phentsize);
    get(bytes, 44, phnum);
    get(bytes, 46, shentsize);
    get(bytes, 48, shnum);
    if (expected == ElfMachine::Arm && machine != em_arm) {
        error = machine == em_riscv ? "The ELF file is built for a RISC-V processor, not for an ARM one."
                                    : "The ELF file is not built for an ARM processor.";
        return std::nullopt;
    }
    if (expected == ElfMachine::Riscv && machine != em_riscv) {
        error = machine == em_arm ? "The ELF file is built for an ARM processor, not for a RISC-V one."
                                  : "The ELF file is not built for a RISC-V processor.";
        return std::nullopt;
    }
    if (type != et_exec) {
        error = "The ELF file is not an executable (an object file or library cannot run).";
        return std::nullopt;
    }
    if (phentsize < 32 || phnum == 0) {
        error = "The ELF file has no program headers to load.";
        return std::nullopt;
    }

    ElfImage image;
    image.entry = entry;
    std::size_t total = 0;
    for (std::uint16_t i = 0; i < phnum; ++i) {
        const std::size_t at = phoff + static_cast<std::size_t>(i) * phentsize;
        std::uint32_t p_type = 0;
        std::uint32_t p_offset = 0;
        std::uint32_t p_vaddr = 0;
        std::uint32_t p_paddr = 0;
        std::uint32_t p_filesz = 0;
        std::uint32_t p_memsz = 0;
        std::uint32_t p_flags = 0;
        if (!get(bytes, at, p_type) || !get(bytes, at + 4, p_offset) || !get(bytes, at + 8, p_vaddr)
            || !get(bytes, at + 12, p_paddr) || !get(bytes, at + 16, p_filesz) || !get(bytes, at + 20, p_memsz)
            || !get(bytes, at + 24, p_flags)) {
            error = "The ELF file is truncated (program headers).";
            return std::nullopt;
        }
        if (p_type != pt_load || p_memsz == 0) {
            continue;
        }
        if (p_filesz > p_memsz || p_offset > bytes.size() || bytes.size() - p_offset < p_filesz) {
            error = "The ELF file is truncated or damaged (a segment lies outside the file).";
            return std::nullopt;
        }
        total += p_memsz;
        if (total > max_image) {
            error = "The ELF file is too large for a microcontroller image.";
            return std::nullopt;
        }
        ElfSegment segment;
        segment.physical = p_paddr;
        segment.virtual_address = p_vaddr;
        segment.data.assign(bytes.begin() + p_offset, bytes.begin() + p_offset + p_filesz);
        segment.memory_size = p_memsz;
        segment.executable = (p_flags & pf_x) != 0;
        image.segments.push_back(std::move(segment));
    }
    if (image.segments.empty()) {
        error = "The ELF file has nothing to load.";
        return std::nullopt;
    }

    // Function symbols (optional: a stripped file still runs).
    for (std::uint16_t i = 0; shentsize >= 40 && i < shnum; ++i) {
        const std::size_t at = shoff + static_cast<std::size_t>(i) * shentsize;
        std::uint32_t sh_type = 0;
        std::uint32_t sh_offset = 0;
        std::uint32_t sh_size = 0;
        std::uint32_t sh_link = 0;
        std::uint32_t sh_entsize = 0;
        if (!get(bytes, at + 4, sh_type) || sh_type != sht_symtab || !get(bytes, at + 16, sh_offset)
            || !get(bytes, at + 20, sh_size) || !get(bytes, at + 24, sh_link) || !get(bytes, at + 36, sh_entsize)
            || sh_entsize < 16) {
            continue;
        }
        std::uint32_t str_offset = 0;
        std::uint32_t str_size = 0;
        const std::size_t str_at = shoff + static_cast<std::size_t>(sh_link) * shentsize;
        if (sh_link >= shnum || !get(bytes, str_at + 16, str_offset) || !get(bytes, str_at + 20, str_size)
            || str_offset > bytes.size() || bytes.size() - str_offset < str_size) {
            continue;
        }
        for (std::uint32_t s = 0; s + sh_entsize <= sh_size; s += sh_entsize) {
            std::uint32_t name = 0;
            std::uint32_t value = 0;
            std::uint32_t size = 0;
            std::uint8_t info = 0;
            if (!get(bytes, sh_offset + s, name) || !get(bytes, sh_offset + s + 4, value)
                || !get(bytes, sh_offset + s + 8, size) || !get(bytes, sh_offset + s + 12, info)) {
                break;
            }
            std::uint16_t shndx = 0;
            const unsigned kind = info & 0xFu;
            if (name == 0 || name >= str_size || kind == stt_section || kind == stt_file
                || !get(bytes, sh_offset + s + 14, shndx) || shndx == 0) {
                continue; // unnamed, a section or file entry, or undefined
            }
            const char* text = reinterpret_cast<const char*>(bytes.data() + str_offset + name);
            const void* end = std::memchr(text, 0, str_size - name);
            const std::size_t length = end ? static_cast<std::size_t>(static_cast<const char*>(end) - text) : str_size - name;
            std::string symbol_name(text, length);
            // ARM function symbols carry the Thumb bit; RISC-V addresses are even.
            const std::uint32_t address = machine == em_arm && kind == stt_func ? value & ~1u : value;
            image.symbols.emplace(symbol_name, address);
            if (kind == stt_func) {
                image.functions.push_back({std::move(symbol_name), address, size});
            }
        }
    }
    std::sort(image.functions.begin(), image.functions.end(),
        [](const ElfSymbol& a, const ElfSymbol& b) { return a.address < b.address; });
    return image;
}

std::optional<ElfImage> read_elf(const std::filesystem::path& path, std::string& error, ElfMachine machine)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        error = "Cannot open the firmware file " + path.string() + ".";
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (bytes.size() > max_image) {
        error = "The firmware file is too large.";
        return std::nullopt;
    }
    return parse_elf(bytes, error, machine);
}

} // namespace simnodus::emu
