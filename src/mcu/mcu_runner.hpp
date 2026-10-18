// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Options of a microcontroller run and helpers shared by the session and the
// runner (NativeMcuRunner, SimNodus's own emulator; ADR 0094 removed Renode).
#pragma once

#include "mcu_backend.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace simnodus::mcu {

struct GpioPin {
    char port = 'A'; // GPIO port letter
    int number = 0;
};

// "PA0" -> {'A', 0}; nullopt for power and control pins.
std::optional<GpioPin> gpio_pin(const std::string& name);

struct McuRunOptions {
    std::filesystem::path firmware; // ELF; empty: loaded by the debugger
    std::vector<std::string> pins; // GPIO pin names to watch, e.g. "PC13"
    std::vector<std::string> uarts; // UART peripherals for the terminal, e.g. "usart1"
    PinModel pin_model;
    // Coupling window: the firmware runs to window boundaries and sees circuit
    // inputs there.
    std::uint64_t chunk_us = 1000;
    // ADC (SN-178): a sample taken in coupling window k reads the pin voltage at
    // the end of window k minus this lag, a circuit time the circuit has always
    // passed when the firmware may run window k (the lead plus the circuit's
    // maximum step). Same input, same codes, every run.
    std::uint64_t analog_lag_ns = 200000;
    // USART RX from the pin (SN-192): UARTs whose RX gets another board's bytes
    // directly ("usart1") do not decode their pin as well; bits shorter than
    // uart_rx_min_bit_ns are not decoded (the circuit's step cannot show them).
    std::set<std::string> uart_rx_byte_links;
    std::uint64_t uart_rx_min_bit_ns = 0;
    // GDB server on 127.0.0.1:gdb_port (SN-180), -1 for none. The firmware then
    // waits halted at its reset handler for the first client.
    int gdb_port = -1;
};

} // namespace simnodus::mcu
