// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 machine of the SimNodus emulator (experiment E-12, SN-294;
// RM0090): a Cortex-M4F core, the memory map (512 KiB of flash, 112 + 16 KiB
// of SRAM, 64 KiB of CCM memory), the clock tree with the HSE frequency of
// the crystal on the diagram, PWR and the flash interface, GPIO ports with
// the alternate functions of the datasheet, SYSCFG, EXTI, and the six serial
// ports (stm32_usart.hpp). The chip's other peripherals are stubs that keep
// their register values and report once that they are not simulated
// (timers, ADC, DAC, SPI, I2C, DMA, RTC: SN-297, SN-298).
//
// Experimental: nothing here has been compared with a real chip yet.
#pragma once

#include "bus.hpp"
#include "cpu.hpp"
#include "elf.hpp"
#include "stm32_usart.hpp"
#include "stm32f103.hpp" // UartByte, Diagnostic, ResetKind
#include "timed_device.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::emu {

// A pin as the STM32F4's GPIO registers configure it.
struct F4PinConfig {
    std::uint8_t mode = 0;      // MODER: 0 input, 1 output, 2 alternate function, 3 analog
    bool open_drain = false;    // OTYPER
    std::uint8_t pull = 0;      // PUPDR: 0 none, 1 up, 2 down
    std::uint8_t function = 0;  // AFR: the alternate function number
    bool level = false;         // the level on the pin
    bool drive_high = false;    // outputs: the ODR bit, or the peripheral's level for an alternate function
};

// A change of a pin's level or configuration, with the configuration at that instant.
struct F4PinEvent {
    std::uint64_t time_ns = 0;
    int port = 0; // 0 = A ... 4 = E, 7 = H
    int pin = 0;
    F4PinConfig config;
};

struct F4MachineOptions {
    std::uint32_t flash_size = 512 * 1024; // STM32F407VE
    // The crystal between PH0 (OSC_IN) and PH1 (OSC_OUT) in Hz; 0: none, and
    // the HSE oscillator then never becomes ready.
    std::uint32_t hse_hz = 0;
    bool lse_crystal = false; // a 32.768 kHz crystal between PC14 and PC15
    // Cycle counts of the Cortex-M4 (its Technical Reference Manual) instead
    // of one cycle per instruction. The flash's wait states and the ART
    // accelerator are not modelled yet (SN-296).
    bool cortex_timing = true;
    UsartOptions usart;
};

class Stm32F407 : private UsartHost {
public:
    static constexpr int num_ports = 9; // A to I in the address map; the LQFP-100 chip has pins on A to E and PH0, PH1
    static constexpr int num_usarts = 6;

    explicit Stm32F407(F4MachineOptions options = {});
    ~Stm32F407() override;
    Stm32F407(const Stm32F407&) = delete;
    Stm32F407& operator=(const Stm32F407&) = delete;

    // Copies the image into the flash and the SRAM, then resets the chip (power-on).
    bool load(const ElfImage& image, std::string& error);
    void reset(ResetKind kind);

    // Runs the firmware until virtual time `time_ns`, or until the core halts
    // at a debugger breakpoint (halted()).
    void run_until_ns(std::uint64_t time_ns);
    bool halted() const { return cpu_.debug_halted(); }
    void step_instruction();
    std::uint64_t time_ns() const;
    std::uint64_t cycles() const { return cpu_.cycles(); }
    std::uint32_t hclk_hz() const { return hclk_; }
    std::uint32_t sysclk_hz() const;

    // Inputs from outside the chip, effective now; `changed_ns` (not after
    // now) is when the level changed outside, for receivers that decode edge times.
    void set_pin_input(int port, int pin, bool level);
    void set_pin_input(int port, int pin, bool level, std::uint64_t changed_ns);
    // A byte from a terminal for serial port `usart` (1 to 6).
    void uart_receive(int usart, std::uint8_t value);
    using PinHook = std::function<void(int port, int pin, std::uint64_t time_ns)>;
    void set_pin_hook(PinHook hook) { pin_hook_ = std::move(hook); }
    void add_timed(TimedDevice* device) { timed_.push_back(device); }
    void remove_timed(TimedDevice* device) { std::erase(timed_, device); }
    void timed_schedule_changed() { cpu_.request_stop(); }
    std::uint64_t cycles_at(std::uint64_t ns) const { return cycles_at_ns(ns); }
    std::uint64_t ns_at(std::uint64_t cycles) const { return ns_at_cycles(cycles); }

    std::vector<F4PinEvent> take_pin_events();
    std::vector<UartByte> take_uart_bytes();
    std::vector<Diagnostic> take_diagnostics();
    F4PinConfig pin_config(int port, int pin) const;
    bool pin_level(int port, int pin) const;

    Cpu& cpu() { return cpu_; }
    Bus& bus() { return bus_; }
    const ElfImage* image() const { return image_.get(); }
    const F4MachineOptions& options() const { return options_; }

    // Internal: used by the device models.
    void diagnostic(const std::string& text);
    void set_hclk(std::uint32_t hz);
    void pin_output_changed(int port, int pin);
    void pin_input_changed(int port, int pin, bool level);
    // Output of a peripheral on a pin: `source` identifies it among the
    // drivers of the pin, `function` is its alternate function number there.
    void af_output(int port, int pin, int source, int function, bool level);
    void routing_changed(); // GPIO modes or alternate functions changed: the peripherals look for their pins again
    int exti_port(int line) const;
    void exti_selection_changed();
    std::uint32_t flash_latency() const;
    void flash_access_changed();
    std::uint32_t reset_flags() const { return reset_flags_; }
    void clear_reset_flags() { reset_flags_ = 0; }

private:
    class Rcc;
    class Flash;
    class Pwr;
    class Gpio;
    class Syscfg;
    class Exti;
    class Stub;
    class Dwt;
    class Dbgmcu;
    class Zero;

    std::uint64_t cycles_at_ns(std::uint64_t ns) const;
    std::uint64_t ns_at_cycles(std::uint64_t cycles) const;
    std::uint64_t next_timer_event() const;
    void run_timers();
    // The pin of a serial port's function: the first of its candidate pins
    // that is in alternate function mode with the port's function number.
    std::pair<int, int> usart_pin(int number, bool transmit) const;

    // UsartHost
    std::uint64_t usart_now_cycles() const override { return cpu_.cycles(); }
    std::uint32_t usart_hclk_hz() const override { return hclk_; }
    std::uint32_t usart_apb_divider(int number) const override;
    std::pair<int, int> usart_tx_pin(int number) const override { return usart_pin(number, true); }
    std::pair<int, int> usart_rx_pin(int number) const override { return usart_pin(number, false); }
    void usart_irq(int irq, bool level) override { cpu_.set_irq_line(irq, level); }
    void usart_line(int port, int pin, int number, bool level) override;
    void usart_line_at(int port, int pin, int number, bool level, std::uint64_t cycle) override;
    void usart_byte(int number, std::uint8_t value, std::uint64_t cycle) override;
    bool usart_dma(int, bool) override { return false; } // DMA: SN-298
    void usart_schedule_changed() override { cpu_.request_stop(); }
    void usart_diagnostic(const std::string& text) override { diagnostic(text); }

    F4MachineOptions options_;
    Bus bus_;
    Cpu cpu_;
    std::unique_ptr<Rcc> rcc_;
    std::unique_ptr<Flash> flash_;
    std::unique_ptr<Pwr> pwr_;
    std::array<std::unique_ptr<Gpio>, num_ports> gpio_;
    std::unique_ptr<Syscfg> syscfg_;
    std::unique_ptr<Exti> exti_;
    std::array<std::unique_ptr<Stm32Usart>, num_usarts> usart_;
    std::vector<std::unique_ptr<Stub>> stubs_;
    std::unique_ptr<Dwt> dwt_;
    std::unique_ptr<Dbgmcu> dbgmcu_;
    std::unique_ptr<Zero> debug_zero_;
    std::vector<TimedDevice*> timed_;
    PinHook pin_hook_;
    std::unique_ptr<ElfImage> image_;

    std::uint32_t hclk_ = 16000000;
    std::uint64_t base_cycles_ = 0;
    std::uint64_t base_ns_ = 0;
    bool reset_requested_ = false;
    ResetKind requested_kind_ = ResetKind::Software;
    std::uint32_t reset_flags_ = 0x0E000000u; // RCC_CSR: PORRSTF | PINRSTF | BORRSTF
    bool stamping_ = false; // pin events take the time of `stamp_cycles_`
    std::uint64_t stamp_cycles_ = 0;
    std::optional<std::uint64_t> input_changed_ns_;

    std::vector<F4PinEvent> pin_events_;
    std::vector<UartByte> uart_bytes_;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace simnodus::emu
