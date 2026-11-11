// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Register maps of the simulated chips for the register view (SN-290): the
// peripherals, their registers and the fields of each register, with the
// names of the reference manual or datasheet. The maps are data inside the
// program (register_map_data.cpp); nothing is read from disk.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace simnodus::mcu {

struct RegisterField {
    std::string name;
    unsigned lsb = 0;   // position of the lowest bit
    unsigned width = 1; // bits
    std::string description;
};

struct Register {
    std::string name;
    std::uint32_t offset = 0; // from the peripheral's base
    unsigned size = 4;        // bytes: 1, 2 or 4 (little endian)
    std::string description;
    std::vector<RegisterField> fields; // highest bit first
};

struct Peripheral {
    std::string name;
    std::uint32_t base = 0; // in the chip's memory as McuMachine::peek() sees it
    std::string description;
    std::vector<Register> registers;
};

struct RegisterMap {
    std::string chip; // the map's own chip ("atmega8" also serves the ATmega8A)
    std::vector<Peripheral> peripherals;
};

// The map of a board's chip (library::McuBoard::chip); nullptr for a chip
// without one (the ESP32-C3).
const RegisterMap* register_map(const std::string& chip);

// Reads a map from its text (the form is described in register_map_data.cpp);
// on a mistake `error` names the line. For the built-in maps and tests.
bool parse_register_map(const std::string& text, RegisterMap& map, std::string& error);

// The value of a field in its register's value.
inline std::uint32_t field_value(std::uint32_t value, const RegisterField& field)
{
    return (value >> field.lsb) & (field.width >= 32 ? 0xFFFFFFFFu : ((1u << field.width) - 1u));
}

// The watched registers as the engine gives them (McuBackend::board_registers):
// for each range its address and size, then its bytes.
struct RegisterBytes {
    struct Range {
        std::uint32_t address = 0;
        std::vector<std::uint8_t> bytes;
    };
    std::vector<Range> ranges;

    static RegisterBytes parse(const std::vector<std::uint8_t>& blob);
    // The little-endian value of `size` bytes at `address`; false if they are not all there.
    bool value(std::uint32_t address, unsigned size, std::uint32_t& out) const;
};

// The ranges (address, bytes) to watch for a peripheral: one per register.
std::vector<std::pair<std::uint32_t, std::uint32_t>> register_ranges(const Peripheral& peripheral);

} // namespace simnodus::mcu
