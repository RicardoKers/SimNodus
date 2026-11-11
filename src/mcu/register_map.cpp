// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mcu/register_map.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <sstream>

namespace simnodus::mcu {

// The built-in texts (register_map_data.cpp): pieces to join, ended by nullptr.
extern const char* const stm32f103_registers[];
extern const char* const atmega8_registers[];
extern const char* const atmega32_registers[];
extern const char* const atmega328p_registers[];

namespace {

bool number(const std::string& text, std::uint32_t& out)
{
    if (text.empty()) {
        return false;
    }
    try {
        std::size_t used = 0;
        const unsigned long long v = std::stoull(text, &used, 0);
        if (used != text.size() || v > 0xFFFFFFFFull) {
            return false;
        }
        out = static_cast<std::uint32_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

std::string trimmed(const std::string& s)
{
    const auto a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) {
        return {};
    }
    return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}

// Fields highest bit first, and none over another or past the register.
bool settle_fields(Register& reg)
{
    std::sort(reg.fields.begin(), reg.fields.end(),
        [](const RegisterField& a, const RegisterField& b) { return a.lsb > b.lsb; });
    unsigned top = reg.size * 8;
    for (const auto& f : reg.fields) {
        if (f.width == 0 || f.lsb + f.width > top) {
            return false;
        }
        top = f.lsb;
    }
    return true;
}

} // namespace

bool parse_register_map(const std::string& text, RegisterMap& map, std::string& error)
{
    std::map<std::string, std::vector<Register>> types;
    std::vector<Register>* block = nullptr; // the type or peripheral being read
    std::istringstream in(text);
    std::string raw;
    int line_number = 0;
    auto fail = [&](const std::string& why) {
        error = "line " + std::to_string(line_number) + ": " + why;
        return false;
    };
    while (std::getline(in, raw)) {
        ++line_number;
        std::string line = trimmed(raw);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::string description;
        if (const auto bar = line.find('|'); bar != std::string::npos) {
            description = trimmed(line.substr(bar + 1));
            line = trimmed(line.substr(0, bar));
        }
        std::istringstream words(line);
        std::vector<std::string> w;
        for (std::string word; words >> word;) {
            w.push_back(word);
        }
        if (w.empty()) {
            return fail("a description without a statement");
        }
        const std::string& kind = w[0];
        if (kind == "chip" && w.size() == 2) {
            map.chip = w[1];
        } else if (kind == "type" && (w.size() == 2 || w.size() == 3)) {
            std::vector<Register> registers;
            if (w.size() == 3) {
                const auto base = types.find(w[2]);
                if (base == types.end()) {
                    return fail("unknown type " + w[2]);
                }
                registers = base->second;
            }
            block = &(types[w[1]] = std::move(registers));
        } else if (kind == "per" && w.size() == 3) {
            Peripheral p;
            p.name = w[1];
            p.description = description;
            if (!number(w[2], p.base)) {
                return fail("bad address");
            }
            map.peripherals.push_back(std::move(p));
            block = &map.peripherals.back().registers;
        } else if (kind == "use" && w.size() == 2 && block) {
            const auto type = types.find(w[1]);
            if (type == types.end()) {
                return fail("unknown type " + w[1]);
            }
            block->insert(block->end(), type->second.begin(), type->second.end());
        } else if (kind == "reg" && w.size() == 4 && block) {
            Register r;
            r.name = w[1];
            r.description = description;
            std::uint32_t size = 0;
            if (!number(w[2], r.offset) || !number(w[3], size) || (size != 1 && size != 2 && size != 4)) {
                return fail("bad register");
            }
            r.size = size;
            block->push_back(std::move(r));
        } else if (kind == "f" && w.size() == 4 && block && !block->empty()) {
            RegisterField f;
            f.name = w[1];
            f.description = description;
            std::uint32_t lsb = 0;
            std::uint32_t width = 0;
            if (!number(w[2], lsb) || !number(w[3], width)) {
                return fail("bad field");
            }
            f.lsb = lsb;
            f.width = width;
            block->back().fields.push_back(std::move(f));
        } else if (kind == "fr" && w.size() == 7 && block && !block->empty()) {
            // fr PREFIX first count width lsb step: PREFIX<first>.. at lsb, lsb + step..
            std::uint32_t first = 0;
            std::uint32_t count = 0;
            std::uint32_t width = 0;
            std::uint32_t lsb = 0;
            std::uint32_t step = 0;
            if (!number(w[2], first) || !number(w[3], count) || !number(w[4], width) || !number(w[5], lsb)
                || !number(w[6], step)) {
                return fail("bad field range");
            }
            for (std::uint32_t i = 0; i < count; ++i) {
                RegisterField f;
                f.name = w[1] + std::to_string(first + i);
                f.lsb = lsb + i * step;
                f.width = width;
                f.description = description;
                if (const auto at = f.description.find('#'); at != std::string::npos) {
                    f.description.replace(at, 1, std::to_string(first + i));
                }
                block->back().fields.push_back(std::move(f));
            }
        } else if (kind == "bits" && block && !block->empty()) {
            // The bits' names, highest first; "-" for a bit without one.
            Register& r = block->back();
            if (w.size() - 1 != r.size * 8) {
                return fail("a name for each bit is needed");
            }
            for (std::size_t i = 1; i < w.size(); ++i) {
                if (w[i] != "-") {
                    r.fields.push_back({w[i], static_cast<unsigned>(r.size * 8 - i), 1, {}});
                }
            }
        } else if (kind == "like" && w.size() == 2 && block && !block->empty()) {
            const auto from = std::find_if(block->begin(), block->end() - 1,
                [&](const Register& r) { return r.name == w[1]; });
            if (from == block->end() - 1) {
                return fail("unknown register " + w[1]);
            }
            block->back().fields = from->fields;
        } else {
            return fail("not understood: " + line);
        }
    }
    line_number = 0;
    for (auto& p : map.peripherals) {
        for (auto& r : p.registers) {
            if (!settle_fields(r)) {
                error = p.name + "." + r.name + ": fields over each other or past the register";
                return false;
            }
        }
    }
    return true;
}

const RegisterMap* register_map(const std::string& chip)
{
    static std::mutex mutex;
    static std::map<std::string, RegisterMap> maps;
    const char* const* text = nullptr;
    std::string key;
    if (chip.empty() || chip == "stm32f103") {
        key = "stm32f103";
        text = stm32f103_registers;
    } else if (chip == "atmega8" || chip == "atmega8a") {
        key = "atmega8";
        text = atmega8_registers;
    } else if (chip == "atmega32" || chip == "atmega32a") {
        key = "atmega32";
        text = atmega32_registers;
    } else if (chip == "atmega328" || chip == "atmega328p") {
        key = "atmega328p";
        text = atmega328p_registers;
    } else {
        return nullptr;
    }
    std::lock_guard lock(mutex);
    if (const auto it = maps.find(key); it != maps.end()) {
        return &it->second;
    }
    std::string joined;
    for (; *text; ++text) {
        joined += *text;
    }
    RegisterMap map;
    std::string error;
    if (!parse_register_map(joined, map, error)) {
        return nullptr; // a mistake in a built-in text (the unit tests read every map)
    }
    return &(maps[key] = std::move(map));
}

RegisterBytes RegisterBytes::parse(const std::vector<std::uint8_t>& blob)
{
    RegisterBytes out;
    auto word = [&blob](std::size_t at) {
        return static_cast<std::uint32_t>(blob[at]) | static_cast<std::uint32_t>(blob[at + 1]) << 8
            | static_cast<std::uint32_t>(blob[at + 2]) << 16 | static_cast<std::uint32_t>(blob[at + 3]) << 24;
    };
    for (std::size_t at = 0; at + 8 <= blob.size();) {
        Range range;
        range.address = word(at);
        const std::uint32_t size = word(at + 4);
        at += 8;
        if (size > blob.size() - at) {
            break; // a damaged blob: what came before stays
        }
        range.bytes.assign(blob.begin() + static_cast<std::ptrdiff_t>(at),
            blob.begin() + static_cast<std::ptrdiff_t>(at + size));
        at += size;
        out.ranges.push_back(std::move(range));
    }
    return out;
}

bool RegisterBytes::value(std::uint32_t address, unsigned size, std::uint32_t& out) const
{
    for (const auto& range : ranges) {
        if (address >= range.address && address - range.address <= range.bytes.size()
            && size <= range.bytes.size() - (address - range.address)) {
            out = 0;
            for (unsigned i = 0; i < size; ++i) {
                out |= static_cast<std::uint32_t>(range.bytes[address - range.address + i]) << (8 * i);
            }
            return true;
        }
    }
    return false;
}

std::vector<std::pair<std::uint32_t, std::uint32_t>> register_ranges(const Peripheral& peripheral)
{
    // One range per register: a gap between two registers may be something
    // the chip does not have, and that would empty a range that crossed it.
    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
    for (const auto& r : peripheral.registers) {
        ranges.emplace_back(peripheral.base + r.offset, r.size);
    }
    return ranges;
}

} // namespace simnodus::mcu
