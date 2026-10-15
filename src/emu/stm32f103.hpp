// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F103 (medium density, x8/xB) machine of the SimNodus emulator: memory
// map (RM0008, section 3.3), timebase from HCLK, and the tier-1 devices of
// docs/architecture/MCU_EMULATOR.md (RCC, flash interface, PWR, GPIO A-E,
// AFIO, EXTI, USART1-3 at byte level). Other peripherals of the chip are
// stubs that keep their register values and report once that they are not
// simulated.
#pragma once

#include "bus.hpp"
#include "cpu.hpp"
#include "elf.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace simnodus::emu {

struct PinEvent {
    std::uint64_t time_ns = 0;
    int port = 0; // 0 = A ... 4 = E
    int pin = 0;
    bool level = false;
};

struct UartByte {
    std::uint64_t time_ns = 0;
    int usart = 1; // 1..3
    std::uint8_t value = 0;
};

struct Diagnostic {
    std::uint64_t time_ns = 0;
    std::uint32_t pc = 0;
    std::string text;
};

// Configuration of one GPIO pin as the firmware set it (CRL/CRH nibble).
struct PinConfig {
    std::uint8_t mode = 0; // MODE bits: 0 input, 1..3 output (10, 2, 50 MHz)
    std::uint8_t cnf = 1;  // CNF bits
    bool output_level = false; // level driven when the pin is an output
    bool pull_up = false;      // input with pull (CNF 10): ODR bit 1
};

struct MachineOptions {
    std::uint32_t flash_size = 64 * 1024;  // STM32F103C8
    std::uint32_t sram_size = 20 * 1024;
    std::uint32_t hse_hz = 8000000;        // Blue Pill crystal
};

class Stm32F103 {
public:
    static constexpr int num_ports = 5;

    explicit Stm32F103(MachineOptions options = {});
    ~Stm32F103();
    Stm32F103(const Stm32F103&) = delete;
    Stm32F103& operator=(const Stm32F103&) = delete;

    // Copies the image into flash and SRAM, then resets the chip (power-on).
    bool load(const ElfImage& image, std::string& error);
    // Power-on reset (true) or a system reset through NRST/SYSRESETREQ (false):
    // peripherals and core reset; SRAM keeps its contents; RCC_CSR records why.
    void reset(bool power_on);

    // Runs the firmware until virtual time `time_ns` (nanoseconds since power-on).
    void run_until_ns(std::uint64_t time_ns);
    std::uint64_t time_ns() const;
    std::uint64_t cycles() const { return cpu_.cycles(); }
    std::uint32_t hclk_hz() const { return hclk_; }

    // Inputs from outside the chip, effective now.
    void set_pin_input(int port, int pin, bool level);
    void uart_receive(int usart, std::uint8_t value);

    // Output: pin changes and transmitted bytes since the last call.
    std::vector<PinEvent> take_pin_events();
    std::vector<UartByte> take_uart_bytes();
    std::vector<Diagnostic> take_diagnostics();
    PinConfig pin_config(int port, int pin) const;
    bool pin_level(int port, int pin) const; // level on the pin (output or input)

    Cpu& cpu() { return cpu_; }
    Bus& bus() { return bus_; }
    const ElfImage* image() const { return image_.get(); }

    // Internal: used by the device models.
    void diagnostic(const std::string& text);
    void set_hclk(std::uint32_t hz);
    void pin_output_changed(int port, int pin, bool level);
    void pin_input_changed(int port, int pin, bool level);
    void uart_transmit(int usart, std::uint8_t value);
    void af_output(int port, int pin, int source, bool level); // source: peripheral id
    int exti_port(int line) const;
    void exti_input(int line, bool level);
    std::uint32_t sysclk_hz() const;
    std::uint32_t flash_latency() const;

private:
    class Rcc;
    class Flash;
    class Pwr;
    class Gpio;
    class Afio;
    class Exti;
    class Usart;
    class Stub;
    class Dwt;
    class Dbgmcu;
    class Zero;

    std::uint64_t cycles_at_ns(std::uint64_t ns) const;
    void remap_changed();
    void exti_selection_changed();

    MachineOptions options_;
    Bus bus_;
    Cpu cpu_;
    std::unique_ptr<Rcc> rcc_;
    std::unique_ptr<Flash> flash_;
    std::unique_ptr<Pwr> pwr_;
    std::array<std::unique_ptr<Gpio>, num_ports> gpio_;
    std::unique_ptr<Afio> afio_;
    std::unique_ptr<Exti> exti_;
    std::array<std::unique_ptr<Usart>, 3> usart_;
    std::vector<std::unique_ptr<Stub>> stubs_;
    std::unique_ptr<Dwt> dwt_;
    std::unique_ptr<Dbgmcu> dbgmcu_;
    std::unique_ptr<Zero> debug_zero_;
    std::unique_ptr<ElfImage> image_;

    std::uint32_t hclk_ = 8000000;
    std::uint64_t base_cycles_ = 0;
    std::uint64_t base_ns_ = 0;
    bool reset_requested_ = false;
    bool power_on_flags_ = true;
    std::uint32_t reset_flags_ = 0x0C000000u; // RCC_CSR: PORRSTF | PINRSTF

    std::vector<PinEvent> pin_events_;
    std::vector<UartByte> uart_bytes_;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace simnodus::emu
