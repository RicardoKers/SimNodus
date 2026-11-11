// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Protocol decoders for the logic lanes (SN-286): I2C, SPI and UART frames
// read from the levels of digital signals, as a logic analyzer shows them
// over its lanes. The signals are what the lanes hold: a level at the start
// and the changes after it. A decoder that starts in the middle of a frame
// finds its place at the next start condition, chip select or idle line.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace simnodus::sim {

struct LogicSignal {
    bool initial = false;                          // level before the first change
    std::vector<std::pair<double, bool>> changes; // seconds, new level; in time order
};

struct DecodedItem {
    enum class Kind {
        Start,   // I2C start or repeated start
        Stop,    // I2C stop
        Address, // I2C address byte with its direction and acknowledge
        Data,    // a byte or word
        Error,   // a UART framing or parity error
    };
    double start = 0.0; // seconds
    double end = 0.0;
    Kind kind = Kind::Data;
    std::string text;         // "S", "50 W A", "3C A", "41 'A'", "9F/EF"
    unsigned value = 0;       // the byte or word (SPI: MOSI's); the 7-bit address
    unsigned second = 0;      // SPI: MISO's word
    bool acknowledged = true; // I2C bytes

    friend bool operator==(const DecodedItem&, const DecodedItem&) = default;
};

// Level of a signal at a time (the change at exactly that time counts).
bool level_at(const LogicSignal& signal, double time);

// I2C: start (S), repeated start (Sr), stop (P), the address byte ("50 W A":
// 7-bit address, direction, A or N) and data bytes ("3C A").
std::vector<DecodedItem> decode_i2c(const LogicSignal& scl, const LogicSignal& sda);

struct SpiOptions {
    int mode = 0;          // 0..3: CPOL in bit 1, CPHA in bit 0
    int bits = 8;          // per word, 1..32
    bool msb_first = true;
    bool use_select = false; // words start anew when the select line becomes active
    bool select_active_low = true;
    bool has_miso = true;
};
// SPI: one item per word, "9F/EF" (MOSI/MISO) or "9F" without MISO.
std::vector<DecodedItem> decode_spi(const LogicSignal& clock, const LogicSignal& mosi, const LogicSignal& miso,
    const LogicSignal& select, const SpiOptions& options);

struct UartOptions {
    double baud = 0.0; // 0: taken from the shortest pulse of the signal (uart_baud)
    int data_bits = 8; // 5..9
    char parity = 'N'; // 'N', 'E' or 'O'
    bool inverted = false; // idle low (an RS-232 line before its receiver)
};
// UART: one item per frame, "41 'A'" or "0A"; Error items for a missing stop
// bit ("frame error") or a wrong parity bit ("parity error").
std::vector<DecodedItem> decode_uart(const LogicSignal& line, const UartOptions& options);

// The standard rate nearest to the shortest pulse of the line; 0 without a pulse.
double uart_baud(const LogicSignal& line);

} // namespace simnodus::sim
