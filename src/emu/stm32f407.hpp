// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 machine of the SimNodus emulator (experiment E-12, SN-294;
// RM0090): a Cortex-M4F core, the memory map (512 KiB of flash, 112 + 16 KiB
// of SRAM, 64 KiB of CCM memory), the clock tree with the HSE frequency of
// the crystal on the diagram, PWR and the flash interface, GPIO ports with
// the alternate functions of the datasheet, SYSCFG, EXTI, and the six serial
// ports (stm32_usart.hpp), the fourteen timers (stm32f1_timer.hpp), ADC1 to
// ADC3 and the DAC (stm32f4_analog.hpp; SN-297), SPI1 to SPI3
// (stm32f1_spi.hpp), I2C1 to I2C3 (stm32f1_i2c.hpp), DMA1 and DMA2
// (stm32f4_dma.hpp), the RTC with its backup registers and the RNG
// (stm32f4_system.hpp), the watchdogs and the CRC unit (stm32f1_system.hpp),
// and the 4 KiB backup SRAM (SN-298). The chip's other peripherals are stubs
// that keep their register values and report once that they are not
// simulated (CAN, USB, Ethernet, SDIO, DCMI, FSMC, I2S).
//
// Experimental: nothing here has been compared with a real chip yet.
#pragma once

#include "bus.hpp"
#include "cpu.hpp"
#include "elf.hpp"
#include "stm32_usart.hpp"
#include "stm32f103.hpp" // UartByte, Diagnostic, ResetKind
#include "stm32f1_i2c.hpp"
#include "stm32f1_spi.hpp"
#include "stm32f1_system.hpp"
#include "stm32f1_timer.hpp"
#include "stm32f4_analog.hpp"
#include "stm32f4_dma.hpp"
#include "stm32f4_system.hpp"
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
    bool af_input = false;      // an alternate function no peripheral drives (an input: RX, a capture)
    bool dac = false;           // PA4, PA5: the DAC channel drives the pin
    bool dac_buffered = false;  // through its output buffer
    std::uint16_t dac_code = 0; // 12 bits of VREF+
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

class Stm32F407 : private UsartHost, private TimerHost, private F4AnalogHost, private SpiHost, private I2cHost,
                  private DmaHost, private SystemHost {
public:
    static constexpr int num_ports = 9; // A to I in the address map; the LQFP-100 chip has pins on A to E and PH0, PH1
    static constexpr int num_usarts = 6;
    static constexpr int num_timers = 14;
    static constexpr int num_spis = 3;
    static constexpr int num_i2cs = 3;
    static constexpr std::uint32_t lsi_hz = 32000; // the LSI's typical frequency (DS8626)
    static constexpr double vref = Stm32F4Adc::vref; // VREF+ and VDDA, not read from the circuit

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
    // The NRST pin: low holds the chip in reset; when it goes high the chip
    // starts from its reset vector (RCC_CSR records a pin reset).
    void set_reset_pin(bool low);
    // The BOOT0 pin, read when a reset ends: high would start the system
    // memory's bootloader, which is not simulated; the chip then stands still
    // and the log says why.
    void set_boot0(bool high) { boot0_ = high; }
    bool held() const { return reset_low_ || bootloader_; }
    // The voltage on a pin for the ADC, from the circuit. Without one a pin
    // reads its DAC channel if that drives it, else 0 V or VREF+ by its level.
    using AnalogSource = std::function<double(int port, int pin, std::uint64_t time_ns)>;
    void set_analog_source(AnalogSource source) { analog_source_ = std::move(source); }
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
    bool pin_level(int port, int pin) const override;

    Cpu& cpu() { return cpu_; }
    Bus& bus() { return bus_; }
    const ElfImage* image() const { return image_.get(); }
    const F4MachineOptions& options() const { return options_; }

    // Internal: used by the device models.
    void diagnostic(const std::string& text);
    void set_hclk(std::uint32_t hz);
    void clocks_changed(); // the bus prescalers changed
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
    void rtc_clock_changed(); // the RTC's clock source, its enable or readiness changed
    void backup_domain_reset(); // power-on or RCC_BDCR.BDRST

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
    bool usart_dma(int number, bool transmit) override;
    void usart_schedule_changed() override { cpu_.request_stop(); }
    void usart_diagnostic(const std::string& text) override { diagnostic(text); }

    // TimerHost
    std::uint64_t now_cycles() const override { return cpu_.cycles(); }
    std::uint32_t hclk_per_kernel_tick(bool apb2) const override;
    std::pair<int, int> timer_pin(int number, int channel, bool complementary) const override;
    void timer_irq(int number, int irq, bool level) override;
    void timer_output(int port, int pin, int source, bool level, std::uint64_t cycle) override;
    void timer_schedule_changed() override { cpu_.request_stop(); }
    void timer_diagnostic(const std::string& text) override { diagnostic(text); }
    void timer_trigger(int number, int signal, std::uint64_t cycle) override;

    // F4AnalogHost
    std::uint64_t analog_now_cycles() const override { return cpu_.cycles(); }
    std::uint32_t hclk_per_pclk2() const override;
    double adc_voltage(int adc, int channel, std::uint64_t cycle) override;
    void adc_irq(bool level) override { cpu_.set_irq_line(18, level); }
    void analog_schedule_changed() override { cpu_.request_stop(); }
    void analog_diagnostic(const std::string& text) override { diagnostic(text); }
    void dac_output(int channel, bool enabled, bool buffered, std::uint32_t code, std::uint64_t cycle) override;
    bool adc_dma(int adc) override;
    bool dac_dma(int channel) override;
    void dac_irq(bool level) override { timer_irq(20, 54, level); } // shares TIM6_DAC with TIM6

    // SpiHost and I2cHost
    std::uint64_t spi_now_cycles() const override { return cpu_.cycles(); }
    std::uint32_t hclk_per_pclk(bool apb2) const override;
    std::uint32_t afio_mapr() const override { return 0; } // the STM32F1's remap register; not on this chip
    void spi_irq(int irq, bool level) override { cpu_.set_irq_line(irq, level); }
    void spi_output(int port, int pin, int source, bool level, std::uint64_t cycle) override;
    bool spi_dma(int request) override { return dma_request(static_cast<F4DmaRequest>(request)); }
    void spi_schedule_changed() override { cpu_.request_stop(); }
    void spi_diagnostic(const std::string& text) override { diagnostic(text); }
    SpiPins spi_pins(int number) const override;
    std::uint64_t i2c_now_cycles() const override { return cpu_.cycles(); }
    std::uint32_t hclk_per_pclk1() const override;
    void i2c_irq(int irq, bool level) override { cpu_.set_irq_line(irq, level); }
    void i2c_output(int port, int pin, int source, bool level, std::uint64_t cycle) override;
    bool i2c_dma(int request) override { return dma_request(static_cast<F4DmaRequest>(request)); }
    void i2c_schedule_changed() override { cpu_.request_stop(); }
    void i2c_diagnostic(const std::string& text) override { diagnostic(text); }
    std::pair<int, int> i2c_pin(int number, bool scl) const override;
    // An output at `cycle` of a peripheral whose alternate function number is `function`.
    void af_output_at(int port, int pin, int source, int function, bool level, std::uint64_t cycle);

    // DmaHost
    bool dma_request(F4DmaRequest request);
    Bus& dma_bus() override { return bus_; }
    void dma_irq(int irq, bool level) override { cpu_.set_irq_line(irq, level); }
    void dma_diagnostic(const std::string& text) override { diagnostic(text); }
    void dma_channel_enabled(int stream) override;

    // SystemHost
    std::uint64_t system_now_ns() const override { return time_ns(); }
    std::uint64_t system_cycles_at(std::uint64_t ns) const override { return cycles_at_ns(ns); }
    std::uint32_t pclk1_hz() const override;
    std::uint32_t rtc_clock_hz() const override;
    bool backup_writable() const override;
    void system_irq(int irq, bool level) override { cpu_.set_irq_line(irq, level); }
    void system_exti(int line, bool level) override;
    void watchdog_reset(bool independent) override;
    void system_schedule_changed() override { cpu_.request_stop(); }
    void system_diagnostic(const std::string& text) override { diagnostic(text); }

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
    std::array<std::unique_ptr<Stm32F1Timer>, num_timers> timers_; // TIM1..TIM14
    std::unique_ptr<Stm32F4Adc> adc_;
    std::unique_ptr<Stm32F4Dac> dac_;
    std::array<std::unique_ptr<Stm32F1Spi>, num_spis> spi_;
    std::array<std::unique_ptr<Stm32F1I2c>, num_i2cs> i2c_;
    std::array<std::unique_ptr<Stm32F4Dma>, 2> dma_;
    std::unique_ptr<Stm32F4Rtc> rtc_;
    std::unique_ptr<Stm32F4Rng> rng_;
    std::unique_ptr<Stm32F1Iwdg> iwdg_;
    std::unique_ptr<Stm32F1Wwdg> wwdg_;
    std::unique_ptr<Stm32F1Crc> crc_;
    std::vector<std::unique_ptr<Stub>> stubs_;
    std::unique_ptr<Dwt> dwt_;
    std::unique_ptr<Dbgmcu> dbgmcu_;
    std::unique_ptr<Zero> debug_zero_;
    std::vector<TimedDevice*> timed_;
    PinHook pin_hook_;
    AnalogSource analog_source_;
    std::array<std::uint32_t, 96> irq_sources_{}; // per interrupt line: the timers that request it
    struct DacPin {
        bool enabled = false;
        bool buffered = false;
        std::uint32_t code = 0;
    };
    std::array<DacPin, 2> dac_pins_{}; // PA4, PA5
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
    bool reset_low_ = false;  // NRST is held low
    bool boot0_ = false;
    bool bootloader_ = false; // a reset ended with BOOT0 high: nothing runs
    bool reported_bootloader_ = false;
    bool reported_iwdg_ = false;
    bool reported_wwdg_ = false;

    std::vector<F4PinEvent> pin_events_;
    std::vector<UartByte> uart_bytes_;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace simnodus::emu
