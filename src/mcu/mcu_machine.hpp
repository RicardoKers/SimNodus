// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The emulated chip behind a board (SN-246, ADR 0106): what NativeMcuRunner
// and DeviceBus need from it, whatever the chip. Pins and UARTs are named as
// on the board ("PA9", "GPIO5"; "usart1", "uart0", "usb") and resolved once
// to the machine's own numbers. Used only on the runner's worker thread.
#pragma once

#include "mcu_backend.hpp"
#include "mcu_runner.hpp"

#include "emu/timed_device.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace simnodus::emu {
class Stm32F103;
}

namespace simnodus::mcu {

class McuMachine {
public:
    struct PinEvent {
        std::uint64_t time_ns = 0;
        int pin = 0;
        PinState state; // level: the driven level, or the level on an input
        bool level = false; // the level on the pin
    };
    struct UartByte {
        std::uint64_t time_ns = 0;
        int uart = 0;
        std::uint8_t value = 0;
    };

    virtual ~McuMachine() = default;

    // The machine's number for a board pin or UART name, -1 if it has none.
    virtual int pin_id(const std::string& name) const = 0;
    virtual int uart_id(const std::string& name) const = 0;
    // Loads the run's firmware (or the idle image of chips without a board).
    virtual bool load(const McuRunOptions& options, std::string& error) = 0;

    virtual std::uint64_t time_ns() const = 0;
    virtual void run_until_ns(std::uint64_t ns) = 0;
    virtual void press_reset() = 0; // the board's reset button

    virtual PinState pin_state(int pin) const = 0;
    virtual bool pin_level(int pin) const = 0;
    // The circuit's level on an input, applied now; `changed_ns` estimates when
    // the voltage crossed the threshold (serial receivers decode edge times).
    virtual void set_pin_input(int pin, bool level, std::uint64_t changed_ns) = 0;
    // A level from a digital device on the pin's net (no edge time).
    virtual void set_pin_input(int pin, bool level) = 0;
    virtual void set_analog_source(std::function<double(int pin, std::uint64_t time_ns)> source) = 0;
    virtual void uart_receive(int uart, std::uint8_t byte) = 0;

    virtual std::vector<PinEvent> take_pin_events() = 0;
    virtual std::vector<UartByte> take_uart_bytes() = 0;
    virtual std::vector<std::string> take_diagnostics() = 0;

    // Digital devices (DeviceBus): the firmware's pin changes as they happen,
    // timed events in the machine's cycles, and its time conversions.
    virtual void set_pin_hook(std::function<void(int pin, std::uint64_t time_ns)> hook) = 0;
    virtual void add_timed(emu::TimedDevice* device) = 0;
    virtual void remove_timed(emu::TimedDevice* device) = 0;
    virtual void timed_schedule_changed() = 0;
    virtual std::uint64_t cycles_at(std::uint64_t ns) const = 0;
    virtual std::uint64_t ns_at(std::uint64_t cycle) const = 0;

    // The STM32F103 for the GDB server (SN-180); nullptr on other chips.
    virtual emu::Stm32F103* stm32() { return nullptr; }
};

// The machine for options.chip ("stm32f103" or "esp32c3"); nullptr with
// `error` for an unknown chip.
std::unique_ptr<McuMachine> make_machine(const McuRunOptions& options, std::string& error);

// An STM32F103 the caller owns, seen through the interface (tests that drive
// the machine directly); pin numbers are port * 16 + pin.
std::unique_ptr<McuMachine> wrap_stm32(emu::Stm32F103& machine);

// Board pins the chip has as GPIO ("PA9" on the STM32F103, "GPIO5" on the ESP32-C3).
bool chip_gpio(const std::string& chip, const std::string& name);

// The ESP32-C3's ROM ELF: `roms/esp32c3_rev3_rom.elf` beside the application,
// in its share directory (Linux), or in the source tree; empty if none.
std::filesystem::path find_esp32c3_rom(const std::filesystem::path& application_dir);

} // namespace simnodus::mcu
