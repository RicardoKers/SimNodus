// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F103 (medium density, x8/xB) machine of the SimNodus emulator: memory
// map (RM0008, section 3.3), timebase from HCLK, and the tier-1 devices of
// docs/architecture/MCU_EMULATOR.md (RCC, flash interface, PWR, GPIO A-E,
// AFIO, EXTI, USART1-3 with frames at the baud rate), the timers TIM1-TIM4
// (stm32f1_timer.hpp), ADC1-2 (stm32f1_adc.hpp) and DMA1 (stm32f1_dma.hpp).
// Other peripherals of the chip are
// stubs that keep their register values and report once that they are not
// simulated.
#pragma once

#include "bus.hpp"
#include "cpu.hpp"
#include "elf.hpp"
#include "stm32f1_adc.hpp"
#include "stm32f1_dma.hpp"
#include "stm32f1_spi.hpp"
#include "stm32f1_system.hpp"
#include "stm32f1_timer.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::emu {

// A change of a pin's level (outputs) or configuration (any pin), with the
// pin's configuration at that instant.
struct PinEvent {
    std::uint64_t time_ns = 0;
    int port = 0; // 0 = A ... 4 = E
    int pin = 0;
    bool level = false;
    std::uint8_t mode = 0; // MODE bits
    std::uint8_t cnf = 1;  // CNF bits
    bool drive_high = false;
    bool pull_up = false;
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
    bool output_level = false; // level on the pin (outputs and inputs)
    bool drive_high = false;   // outputs: the ODR bit, or the peripheral's level for alternate functions
    bool pull_up = false;      // input with pull (CNF 10): ODR bit 1
};

// Why the chip resets (RCC_CSR reset flags, RM0008 7.3.10).
enum class ResetKind { PowerOn, Pin, Software, IndependentWatchdog, WindowWatchdog };

struct MachineOptions {
    std::uint32_t flash_size = 64 * 1024;  // STM32F103C8
    std::uint32_t sram_size = 20 * 1024;
    std::uint32_t hse_hz = 8000000;        // Blue Pill crystal
    bool uart_frames = true; // false: bytes move at once, without frames on the pins
    // RX from the pin (SN-192): a USART decodes frames from its RX pin's input
    // edges, taken at the times they happened. Edges reach the chip late (at a
    // coupling window boundary), so a frame is decided this long after the
    // middle of its stop bit, when every edge up to there has arrived.
    std::uint64_t uart_rx_lag_ns = 0;
    std::uint32_t uart_rx_pins = 0x7u; // bit n-1 set: USART n samples its RX pin
    // Bits shorter than this are not decoded (the circuit cannot show them; a
    // diagnostic says so once per USART); 0 decodes any baud rate.
    std::uint64_t uart_rx_min_bit_ns = 0;
};

class Stm32F103 : private TimerHost, private AdcHost, private DmaHost, private SystemHost, private SpiHost {
public:
    static constexpr int num_ports = 5;

    explicit Stm32F103(MachineOptions options = {});
    ~Stm32F103();
    Stm32F103(const Stm32F103&) = delete;
    Stm32F103& operator=(const Stm32F103&) = delete;

    // Copies the image into flash and SRAM, then resets the chip (power-on).
    bool load(const ElfImage& image, std::string& error);
    // Resets peripherals and core. SRAM keeps its contents except at power-on;
    // RCC_CSR records why (the flags accumulate until the firmware clears them).
    void reset(ResetKind kind);

    // Runs the firmware until virtual time `time_ns` (nanoseconds since power-on),
    // or until the core halts at a debugger breakpoint (halted()).
    void run_until_ns(std::uint64_t time_ns);
    bool halted() const { return cpu_.debug_halted(); }
    // Debugger: one instruction, with the devices' events due by then.
    void step_instruction();
    std::uint64_t time_ns() const;
    std::uint64_t cycles() const { return cpu_.cycles(); }
    std::uint32_t hclk_hz() const { return hclk_; }

    // Inputs from outside the chip, effective now; `changed_ns` (not after now)
    // is when the level changed outside, for receivers that decode edge times.
    void set_pin_input(int port, int pin, bool level);
    void set_pin_input(int port, int pin, bool level, std::uint64_t changed_ns);
    // Voltage on a pin for the ADC, asked at the sample instant (ns since power-on).
    // Without a source the ADC reads 3.3 V or 0 V from the pin's logic level.
    using AnalogSource = std::function<double(int port, int pin, std::uint64_t time_ns)>;
    void set_analog_source(AnalogSource source) { analog_source_ = std::move(source); }
    void uart_receive(int usart, std::uint8_t value);
    // Called inside the run whenever a pin's output level or configuration changes,
    // with the change's time: devices on the board's nets react at once (SN-203).
    using PinHook = std::function<void(int port, int pin, std::uint64_t time_ns)>;
    void set_pin_hook(PinHook hook) { pin_hook_ = std::move(hook); }
    // Events of something outside the chip that must happen at exact times
    // inside a run, like the chip's own timers (devices on its nets, SN-204).
    void add_timed(TimedDevice* device) { timed_.push_back(device); }
    void remove_timed(TimedDevice* device) { std::erase(timed_, device); }
    std::uint64_t cycles_at(std::uint64_t ns) const { return cycles_at_ns(ns); }
    std::uint64_t ns_at(std::uint64_t cycles) const { return ns_at_cycles(cycles); }

    // Output: pin changes and transmitted bytes since the last call.
    std::vector<PinEvent> take_pin_events();
    std::vector<UartByte> take_uart_bytes();
    std::vector<Diagnostic> take_diagnostics();
    PinConfig pin_config(int port, int pin) const;
    bool pin_level(int port, int pin) const override; // level on the pin (output or input)

    Cpu& cpu() { return cpu_; }
    Bus& bus() { return bus_; }
    const ElfImage* image() const { return image_.get(); }

    // Internal: used by the device models.
    void diagnostic(const std::string& text);
    void set_hclk(std::uint32_t hz);
    void pin_output_changed(int port, int pin, bool level);
    void pin_input_changed(int port, int pin, bool level);
    void uart_transmit(int usart, std::uint8_t value, std::uint64_t cycle); // the frame ended at `cycle`
    // Alternate-function output of peripheral `source` (USART number, or a timer
    // channel id); the pin is high while any source drives it high.
    void af_output(int port, int pin, int source, bool level);
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
    std::uint64_t ns_at_cycles(std::uint64_t cycles) const;
    void remap_changed();
    void apb_changed();
    std::uint64_t next_timer_event() const;
    void run_timers(); // processes the events of the timed devices due by now, in time order

    // TimerHost
    std::uint64_t now_cycles() const override { return cpu_.cycles(); }
    std::uint32_t hclk_per_kernel_tick(bool apb2) const override;
    std::uint32_t afio_mapr() const override;
    void timer_irq(int irq, bool level) override { cpu_.set_irq_line(irq, level); }
    void timer_output(int port, int pin, int source, bool level, std::uint64_t cycle) override;
    void timer_schedule_changed() override { cpu_.request_stop(); }
    void timer_diagnostic(const std::string& text) override { diagnostic(text); }

    // AdcHost
    std::uint32_t hclk_per_adc_clock() const override;
    double adc_channel_voltage(int channel, std::uint64_t cycle) override;
    void adc_irq_changed() override;
    void adc_schedule_changed() override { cpu_.request_stop(); }
    void adc_diagnostic(const std::string& text) override { diagnostic(text); }
    void adc_dma_request() override { dma_->request(1); }

    // DmaHost
    Bus& dma_bus() override { return bus_; }
    void dma_irq(int irq, bool level) override { cpu_.set_irq_line(irq, level); }
    void dma_diagnostic(const std::string& text) override { diagnostic(text); }
    void dma_channel_enabled(int channel) override;
    void exti_selection_changed();

    // SpiHost (SN-204)
    std::uint64_t spi_now_cycles() const override { return cpu_.cycles(); }
    std::uint32_t hclk_per_pclk(bool apb2) const override;
    void spi_irq(int irq, bool level) override { cpu_.set_irq_line(irq, level); }
    void spi_output(int port, int pin, int source, bool level, std::uint64_t cycle) override;
    bool spi_dma(int channel) override { return dma_->request(channel); }
    void spi_schedule_changed() override { cpu_.request_stop(); }
    void spi_diagnostic(const std::string& text) override { diagnostic(text); }

    // SystemHost (SN-187)
    std::uint64_t system_now_ns() const override { return time_ns(); }
    std::uint64_t system_cycles_at(std::uint64_t ns) const override { return cycles_at_ns(ns); }
    std::uint32_t pclk1_hz() const override;
    std::uint32_t rtc_clock_hz() const override;
    bool backup_writable() const override;
    void system_irq(int irq, bool level) override { cpu_.set_irq_line(irq, level); }
    void system_exti(int line, bool level) override { exti_input(line, level); }
    void watchdog_reset(bool independent) override;
    void system_schedule_changed() override { cpu_.request_stop(); }
    void system_diagnostic(const std::string& text) override { diagnostic(text); }

public:
    // Internal: used by RCC.
    void backup_domain_reset(); // RCC_BDCR.BDRST
    void rtc_clock_changed();

private:

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
    std::array<std::unique_ptr<Stm32F1Timer>, 4> timers_; // TIM1..TIM4
    std::array<std::unique_ptr<Stm32F1Adc>, 2> adcs_;     // ADC1, ADC2
    std::unique_ptr<Stm32F1Dma> dma_;                     // DMA1
    std::array<std::unique_ptr<Stm32F1Spi>, 2> spi_;       // SPI1, SPI2
    std::unique_ptr<Stm32F1Crc> crc_;
    std::unique_ptr<Stm32F1Iwdg> iwdg_;
    std::unique_ptr<Stm32F1Wwdg> wwdg_;
    std::unique_ptr<Stm32F1Rtc> rtc_;
    std::unique_ptr<Stm32F1Bkp> bkp_;
    std::vector<TimedDevice*> timed_;                     // timers, ADCs, USARTs, watchdogs, RTC
    AnalogSource analog_source_;
    PinHook pin_hook_;
    std::vector<std::unique_ptr<Stub>> stubs_;
    std::unique_ptr<Dwt> dwt_;
    std::unique_ptr<Dbgmcu> dbgmcu_;
    std::unique_ptr<Zero> debug_zero_;
    std::unique_ptr<ElfImage> image_;

    std::uint32_t hclk_ = 8000000;
    std::uint64_t base_cycles_ = 0;
    std::uint64_t base_ns_ = 0;
    bool reset_requested_ = false;
    ResetKind requested_kind_ = ResetKind::Software; // of reset_requested_
    bool reported_iwdg_ = false, reported_wwdg_ = false;
    std::uint32_t reset_flags_ = 0x0C000000u; // RCC_CSR: PORRSTF | PINRSTF

    // Alternate-function sources driving each pin high (port * 16 + pin).
    std::array<std::vector<int>, num_ports * 16> af_high_;
    bool stamping_ = false;       // pin events take the time of `stamp_cycles_`
    std::uint64_t stamp_cycles_ = 0;
    std::optional<std::uint64_t> input_changed_ns_; // set_pin_input's outside time

    std::vector<PinEvent> pin_events_;
    std::vector<UartByte> uart_bytes_;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace simnodus::emu
