// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "io/memory_file.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>

namespace simnodus::io {

namespace {

int hex_digit(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

bool is_hex_file(const std::filesystem::path& file, const std::string& content)
{
    auto extension = file.extension().string();
    for (auto& c : extension) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (extension == ".hex" || extension == ".ihx") {
        return true;
    }
    if (extension == ".bin") {
        return false;
    }
    return !content.empty() && content[0] == ':';
}

} // namespace

bool parse_intel_hex(const std::string& text, std::vector<std::uint8_t>& bytes, std::string& error)
{
    std::istringstream lines(text);
    std::string line;
    std::uint32_t base = 0;
    int number = 0;
    while (std::getline(lines, line)) {
        ++number;
        while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back()))) {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        auto fail = [&](const std::string& why) {
            error = "line " + std::to_string(number) + ": " + why;
            return false;
        };
        if (line[0] != ':' || line.size() < 11 || (line.size() - 1) % 2 != 0) {
            return fail("not an Intel HEX record");
        }
        std::vector<std::uint8_t> record;
        for (std::size_t i = 1; i < line.size(); i += 2) {
            const int high = hex_digit(line[i]);
            const int low = hex_digit(line[i + 1]);
            if (high < 0 || low < 0) {
                return fail("not a hexadecimal digit");
            }
            record.push_back(static_cast<std::uint8_t>(high * 16 + low));
        }
        const std::size_t count = record[0];
        if (record.size() != count + 5) {
            return fail("the length does not match the record");
        }
        std::uint8_t sum = 0;
        for (const auto b : record) {
            sum = static_cast<std::uint8_t>(sum + b);
        }
        if (sum != 0) {
            return fail("wrong checksum");
        }
        const std::uint32_t offset = (static_cast<std::uint32_t>(record[1]) << 8) | record[2];
        switch (record[3]) {
        case 0x00: // data
            for (std::size_t i = 0; i < count; ++i) {
                const std::uint64_t address = static_cast<std::uint64_t>(base) + offset + i;
                if (address >= bytes.size()) {
                    char where[16];
                    std::snprintf(where, sizeof where, "0x%X", static_cast<unsigned>(address));
                    return fail(std::string("data at ") + where + " is beyond the memory's "
                        + std::to_string(bytes.size()) + " bytes");
                }
                bytes[static_cast<std::size_t>(address)] = record[4 + i];
            }
            break;
        case 0x01: // end of file
            return true;
        case 0x02: // extended segment address
            base = ((static_cast<std::uint32_t>(record[4]) << 8) | record[5]) << 4;
            break;
        case 0x04: // extended linear address
            base = ((static_cast<std::uint32_t>(record[4]) << 8) | record[5]) << 16;
            break;
        default: // start addresses: nothing to store
            break;
        }
    }
    return true;
}

std::string to_intel_hex(const std::vector<std::uint8_t>& bytes)
{
    std::string text;
    char line[64];
    std::uint32_t upper = 0;
    for (std::size_t at = 0; at < bytes.size(); at += 16) {
        const auto address = static_cast<std::uint32_t>(at);
        if ((address >> 16) != upper) {
            upper = address >> 16;
            const std::uint8_t sum = static_cast<std::uint8_t>(0x100 - ((2 + 4 + (upper >> 8) + (upper & 0xFF)) & 0xFF));
            std::snprintf(line, sizeof line, ":02000004%04X%02X\n", static_cast<unsigned>(upper), static_cast<unsigned>(sum));
            text += line;
        }
        const std::size_t count = std::min<std::size_t>(16, bytes.size() - at);
        std::uint8_t sum = static_cast<std::uint8_t>(count + ((address >> 8) & 0xFF) + (address & 0xFF));
        std::snprintf(line, sizeof line, ":%02X%04X00", static_cast<unsigned>(count), static_cast<unsigned>(address & 0xFFFF));
        text += line;
        for (std::size_t i = 0; i < count; ++i) {
            std::snprintf(line, sizeof line, "%02X", static_cast<unsigned>(bytes[at + i]));
            text += line;
            sum = static_cast<std::uint8_t>(sum + bytes[at + i]);
        }
        std::snprintf(line, sizeof line, "%02X\n", static_cast<unsigned>(static_cast<std::uint8_t>(-sum)));
        text += line;
    }
    text += ":00000001FF\n";
    return text;
}

std::optional<std::vector<std::uint8_t>> read_memory_file(const std::filesystem::path& file, std::size_t size,
    std::uint8_t fill, std::string& error)
{
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        error = "cannot open the file";
        return std::nullopt;
    }
    const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<std::uint8_t> bytes(size, fill);
    if (is_hex_file(file, content)) {
        if (!parse_intel_hex(content, bytes, error)) {
            return std::nullopt;
        }
        return bytes;
    }
    if (content.size() > size) {
        error = "the file has " + std::to_string(content.size()) + " bytes, more than the memory's "
            + std::to_string(size);
        return std::nullopt;
    }
    for (std::size_t i = 0; i < content.size(); ++i) {
        bytes[i] = static_cast<std::uint8_t>(content[i]);
    }
    return bytes;
}

bool write_memory_file(const std::filesystem::path& file, const std::vector<std::uint8_t>& bytes, std::string& error)
{
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "cannot write the file";
        return false;
    }
    if (is_hex_file(file, {})) {
        out << to_intel_hex(bytes);
    } else {
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!out) {
        error = "cannot write the file";
        return false;
    }
    return true;
}

} // namespace simnodus::io
