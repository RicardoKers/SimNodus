// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega machine of the SimNodus emulator (experiment E-10, SN-257, SN-260):
// the AVR core (avr_cpu.hpp) with the chip's peripherals, fuses and memories,
// written from the ATmega8A (DS40001974B), ATmega328P (DS40002061B) and
// ATmega32A (DS40002072A) datasheets. One class serves every chip of the family: each chip maps its
// register addresses and interrupt vectors onto the machine's (atmega.cpp). Time is counted in CPU cycles at the clock the fuses select;
// peripherals with events at known cycles (timers, the USART's bits, the
// EEPROM's write time, the watchdog) stop the core at their next event. See
// docs/experiments/E-10-plan.md.
#pragma once

#include "avr_cpu.hpp"
#include "elf.hpp"
#include "timed_device.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::emu {

class Atmega {
public:
    enum class Model { Atmega8, Atmega8A, Atmega328, Atmega328P, Atmega32, Atmega32A };

    struct Options {
        Model model = Model::Atmega8A;
        // Fuses and lock bits as the chip holds them (programmed = 0). The
        // defaults are the ATmega8's factory values (internal RC oscillator at
        // 1 MHz); factory() gives each chip's.
        std::uint8_t fuse_low = 0xE1;
        std::uint8_t fuse_high = 0xD9;
        std::uint8_t lock = 0xFF;
        std::uint8_t fuse_extended = 0xFF; // the ATmega328/328P's (BODLEVEL)
        // Frequency of the crystal or resonator on XTAL1/XTAL2, or of the
        // clock on XTAL1, when the fuses select one; 0: nothing connected (the
        // chip does not start, as on a breadboard).
        std::uint32_t external_hz = 0;
        // The ATmega32's crystal on TOSC1/TOSC2 (PC6, PC7) for Timer2's
        // asynchronous mode; the ATmega8's and 328's share XTAL1/XTAL2.
        std::uint32_t tosc_hz = 0;
    };
    // A chip's factory fuses: the ATmega8 E1/D9, the ATmega328/328P 62/D9/FF
    // (the internal 8 MHz RC oscillator divided by 8).
    static Options factory(Model model);

    // Pins: index = port * 8 + bit, port 0 = B, 1 = C, 2 = D, 3 = A (PB0..PB7,
    // PC0..PC6, PD0..PD7 on the ATmega8; PA0..PA7 and PC7 on the ATmega32).
    static constexpr int pin_count = 32;
    // The supply pins, for their voltages from the circuit (analog source),
    // and the ATmega32's own RESET pin.
    static constexpr int vcc_pin = 100, avcc_pin = 101, aref_pin = 102, reset_pin = 103;
    static int pin_index(char port, int bit) { return port == 'A' ? 24 + bit : (port - 'B') * 8 + bit; }
    static std::string pin_name(int pin);
    bool pin_exists(int pin) const;
    bool pin_usable(int pin) const; // not taken by RESET, the oscillator or TOSC

    struct PinState {
        bool output = false; // driven by the chip
        bool level = false;  // the driven level
        bool pull_up = false; // an input with its pull-up on
    };
    struct PinEvent {
        std::uint64_t time_ns = 0;
        int pin = 0;
        PinState state;
    };
    struct UartByte {
        std::uint64_t time_ns = 0; // when the stop bit ends
        std::uint8_t value = 0;
    };

    Atmega();
    explicit Atmega(const Options& options);
    ~Atmega();
    Atmega(const Atmega&) = delete;
    Atmega& operator=(const Atmega&) = delete;

    const Options& options() const { return options_; }
    const char* chip_name() const;
    bool mega328() const { return options_.model == Model::Atmega328 || options_.model == Model::Atmega328P; }
    bool mega32() const { return options_.model == Model::Atmega32 || options_.model == Model::Atmega32A; }
    std::uint32_t flash_size() const { return flash_bytes_; }
    std::size_t eeprom_size() const { return eeprom_.size(); }

    // ---- Firmware ----
    // Intel HEX text (what avr-objcopy, Microchip Studio and the Arduino IDE
    // write): the flash, or the EEPROM for an .eep file.
    bool load_hex(const std::string& text, std::string& error, bool eeprom = false);
    // An avr-gcc ELF: the flash, and its .eeprom, .fuse and .lock sections
    // when present (as a production programmer writes them). `fuses_set`
    // tells whether the fuses or lock bits changed.
    bool load_elf(const ElfImage& image, std::string& error, bool* fuses_set = nullptr);
    bool load_flash(const std::vector<std::uint8_t>& bytes, std::string& error, std::uint32_t at = 0);
    // Loading a firmware into a bare chip is a chip erase and programming:
    // the EEPROM is erased unless EESAVE is programmed. Bootloader uploads
    // (the UNO) do not call this.
    void chip_erase();
    std::uint8_t flash_byte(std::uint32_t address) const;

    // ---- Fuses ----
    std::uint8_t fuse_low() const { return options_.fuse_low; }
    std::uint8_t fuse_high() const { return options_.fuse_high; }
    std::uint8_t lock_bits() const { return options_.lock; }
    std::uint8_t fuse_extended() const { return options_.fuse_extended; }
    // New fuses take effect at the next reset, as on the chip.
    void set_fuses(std::uint8_t low, std::uint8_t high, std::uint8_t lock);
    void set_extended_fuse(std::uint8_t value) { options_.fuse_extended = value; }
    void set_external_hz(std::uint32_t hz) { options_.external_hz = hz; }
    void set_tosc_hz(std::uint32_t hz) { options_.tosc_hz = hz; }
    // The clock the fuses select (0 when the chip cannot start), and a
    // sentence describing it for the log.
    std::uint32_t clock_hz() const { return cpu_hz_; }
    std::uint8_t clock_prescaler() const { return clkps_; } // CLKPR's CLKPS (the 328/328P)
    std::string clock_description() const;

    // ---- EEPROM (kept in the project by the board layer) ----
    const std::vector<std::uint8_t>& eeprom() const { return eeprom_; }
    void set_eeprom(const std::vector<std::uint8_t>& bytes);
    std::uint64_t eeprom_writes() const { return eeprom_writes_; }
    // Pages the firmware erased or wrote with SPM (a bootloader's upload).
    std::uint64_t flash_writes() const { return flash_writes_; }

    // ---- Running ----
    enum class ResetKind { PowerOn, External, Watchdog, BrownOut };
    void reset(ResetKind kind = ResetKind::PowerOn);
    void run_until_ns(std::uint64_t ns);
    void step_instruction();
    std::uint64_t time_ns() const { return cpu_hz_ ? ns_at(cpu_.cycles()) : idle_ns_; }
    // Interrupt vectors of the chip (2 words each above 8 KB of flash).
    int vector_count() const { return vector_count_; }
    std::uint64_t cycles() const { return cpu_.cycles(); }
    std::uint64_t ns_at(std::uint64_t cycle) const;
    std::uint64_t cycles_at(std::uint64_t ns) const; // rounded up
    bool running() const { return cpu_hz_ != 0; }
    // The core runs in the boot loader section (a bootloader is in control).
    bool in_boot_section() const { return cpu_.pc() >= boot_start_words(); }
    std::uint8_t reset_flags() const { return mcucsr_; }

    // ---- Pins ----
    PinState pin(int pin) const;
    std::vector<PinEvent> take_pin_events();
    // The level the circuit puts on a pin; std::nullopt: nothing drives it.
    // PC6 is the RESET pin unless RSTDISBL is programmed.
    void set_pin_input(int pin, std::optional<bool> level);
    // The level on a pin: driven by the chip, by the circuit, or the pull-up
    // (a floating input reads 0).
    bool pad_level(int pin) const;
    void set_pin_hook(std::function<void(int pin, std::uint64_t time_ns)> hook) { pin_hook_ = std::move(hook); }
    // Voltages for the ADC (SN-259).
    void set_analog_source(std::function<std::optional<double>(int pin, std::uint64_t time_ns)> source)
    {
        analog_source_ = std::move(source);
    }

    // ---- USART ----
    std::vector<UartByte> take_uart_bytes();
    // Bytes from the computer (a host link): received one frame after the
    // other at the USART's baud rate, without the RXD pin.
    void uart_receive(std::uint8_t byte);
    // Decode frames on RXD from the circuit (default on).
    void set_uart_pin_rx(bool on) { uart_pin_rx_ = on; }

    std::vector<std::string> take_diagnostics();

    // Timed devices from outside the chip, in CPU cycles (digital parts); the
    // run loop stops at their next_event() too. timed_changed(): one moved.
    void add_timed(TimedDevice* device) { external_.push_back(device); }
    void remove_timed(TimedDevice* device) { std::erase(external_, device); }
    void timed_changed() { cpu_.request_stop(); }

    AvrCpu& cpu() { return cpu_; }
    const AvrCpu& cpu() const { return cpu_; }

    class Peripheral; // a block of I/O registers with events (atmega.cpp)
    class TimerBase;

    // ---- For the peripherals (atmega.cpp) ----
    // An interrupt flag, by the machine's vector names (atmega.cpp; the chip
    // maps them to its own vector numbers).
    void set_flag(int vector, bool on);
    bool flag(int vector) const { return flags_[static_cast<std::size_t>(vector)]; }
    void update_interrupts();
    void diagnostic(const std::string& text);
    void note_once(const std::string& key, const std::string& text);
    void output_changed(); // a peripheral's pin driving changed
    void refresh_pins(); // pin driving changed now
    void refresh_pins_at(std::uint64_t cycle); // pin driving, with events at `cycle`
    std::uint64_t cycles_for_ns(std::uint64_t ns) const; // a duration, rounded up
    void uart_byte(std::uint8_t value, std::uint64_t cycle);
    // The circuit's voltage on a pin at a cycle (the ADC, the comparator).
    double analog_voltage(int pin, std::uint64_t cycle) const;
    bool io_clock_stopped() const; // asleep in a mode that stops the I/O clock
    void halt_cpu(unsigned cycles) { cpu_.set_cycles(cpu_.cycles() + cycles); }
    void request_reset(ResetKind kind);

private:
    friend class Peripheral;
    void attach_core();
    std::uint8_t read_io(std::uint16_t address);
    void write_io(std::uint16_t address, std::uint8_t value);
    int pending_vector();
    void taken(int vector);
    void apply_fuses();
    void apply_fuses_328();
    void set_cpu_hz(std::uint32_t hz); // a new CPU clock from now on (the time base moves)
    bool pud() const; // pull-ups disabled
    std::uint8_t flag_register(const int* order) const;
    void clear_flags(const int* order, std::uint8_t value);
    void sync_timers();
    bool interrupt_enabled(int vector) const;
    void pin_changed(int pin, bool before, bool after);
    std::uint64_t next_event() const;
    void run_events(std::uint64_t cycle);

    Options options_;
    AvrCpu cpu_;
    std::uint32_t cpu_hz_ = 0;
    std::uint32_t source_hz_ = 0; // the oscillator, before CLKPR's division
    std::uint8_t clkps_ = 0;
    std::uint64_t clkpce_until_ = 0;
    std::uint64_t clock_base_cycle_ = 0; // ns_at() counts from here at cpu_hz_
    std::uint64_t clock_base_ns_ = 0;
    std::uint32_t flash_bytes_ = 8192;
    std::uint32_t page_words_ = 32;
    std::uint32_t boot_min_words_ = 128;
    int vector_count_ = 19;
    std::array<int, 32> vector_role_{}; // the chip's vector number -> the machine's
    std::array<std::uint16_t, 0x100> io_role_{}; // the chip's data address -> the machine's register
    std::uint64_t startup_cycles_ = 0;
    std::uint64_t run_from_ = 0; // the first instruction after a reset's start-up time
    std::vector<std::uint8_t> eeprom_;
    std::uint64_t eeprom_writes_ = 0;
    std::uint64_t flash_writes_ = 0;
    std::uint8_t mcucsr_ = 0;
    std::array<bool, 32> flags_{};
    std::vector<std::unique_ptr<Peripheral>> peripherals_;
    std::array<Peripheral*, 0x140> io_owner_{}; // by the machine's register
    std::uint64_t idle_ns_ = 0; // the time while the chip has no clock
    std::uint8_t timsk_ = 0;
    // Port registers and pins.
    std::array<std::uint8_t, 4> port_{}; // B, C, D, A
    std::array<std::uint8_t, 4> ddr_{};
    // Pins by function: they differ between the PDIP-28 chips and the ATmega32.
    struct FunctionPins {
        int t0, t1, icp1, oc1a, oc1b, oc2, oc0, int0, int1, int2, ain0, ain1, ss, mosi, miso, sck, sda, scl, adc0, txd,
            rxd, tosc1, tosc2;
    };
    FunctionPins fp_{};
    std::uint64_t jtd_written_ = 0; // the ATmega32's JTD takes two writes within four cycles
    int jtd_pending_ = -1;
    bool jtag_on() const;
    std::array<std::optional<bool>, pin_count> input_{};
    std::array<PinState, pin_count> driven_{};
    std::array<bool, pin_count> level_{};
    std::vector<PinEvent> pin_events_;
    std::function<void(int, std::uint64_t)> pin_hook_;
    std::function<std::optional<double>(int, std::uint64_t)> analog_source_;
    std::vector<UartByte> uart_bytes_;
    std::vector<std::string> diagnostics_;
    std::vector<std::string> noted_;
    std::vector<TimedDevice*> external_;
    bool uart_pin_rx_ = true;
    bool reset_requested_ = false;
    ResetKind requested_kind_ = ResetKind::PowerOn;
    bool reset_pin_low_ = false;
    // Registers kept in the machine.
    std::uint8_t gicr_ = 0;
    std::uint8_t mcucr_ = 0;
    std::uint8_t sfior_ = 0;
    std::uint8_t osccal_ = 0;
    // The ATmega328/328P's.
    std::uint8_t eicra_ = 0, eimsk_ = 0, pcicr_ = 0, smcr_ = 0, prr_ = 0, didr0_ = 0, didr1_ = 0, gtccr_ = 0;
    std::array<std::uint8_t, 3> pcmsk_{};
    std::array<std::uint8_t, 3> gpior_{};
    std::array<std::uint8_t, 3> timskn_{}; // TIMSK0..TIMSK2
    std::uint64_t ivce_until_ = 0;
    // SPM and LPM.
    std::uint8_t spmcr_ = 0;
    std::uint64_t spm_until_ = 0;
    std::uint64_t spm_window_ = 0;   // SPMEN written: SPM or LPM must follow within four cycles
    std::uint64_t blbset_until_ = 0;
    bool rww_busy_ = false;
    // Prescaler phases: Timer0 and Timer1 share one (PSR10), Timer2 has its own.
    std::uint64_t prescaler10_ = 0;
    std::uint64_t prescaler2_ = 0;
    std::vector<std::uint16_t> page_buffer_;

    class Ports;
    class ExternalInterrupts;
    class Timer0;
    class Timer1;
    class Timer2;
    class Timer8AB;
    class Usart;
    class Eeprom;
    class Watchdog;
    class Adc;
    class AnalogComparator;
    class Spi;
    class Twi;
    class Power;
    friend class Ports;
    friend class ExternalInterrupts;
    friend class Timer0;
    friend class Timer1;
    friend class Timer2;
    friend class Timer8AB;
    friend class Usart;
    friend class Eeprom;
    friend class Watchdog;
    friend class Adc;
    friend class AnalogComparator;
    friend class Spi;
    friend class Twi;
    friend class Power;
    TimerBase* timer0_ = nullptr;
    Timer1* timer1_ = nullptr;
    TimerBase* timer2_ = nullptr;
    Usart* usart_ = nullptr;
    Eeprom* eeprom_block_ = nullptr;
    Watchdog* watchdog_ = nullptr;
    Adc* adc_ = nullptr;
    Spi* spi_ = nullptr;
    Twi* twi_ = nullptr;
    Power* power_ = nullptr;
    enum class Hold { None, Off, BrownOut };
    Hold power_hold_ = Hold::None;
    int sleep_mode_ = -1; // SM2:0 while asleep, -1 awake
    std::uint64_t sleep_start_ = 0;
    std::uint64_t startup_clocks_ = 6;
    void enter_sleep(int mode);
    void leave_sleep();
    bool wakes(int vector) const;
    bool ac_capture_ = false; // ACIC: Timer1 captures the comparator's output
    ExternalInterrupts* extint_ = nullptr;
    unsigned spm(std::uint16_t z, std::uint16_t r1r0);
    bool lpm_special(std::uint16_t z, std::uint8_t& value);
    std::uint32_t boot_start_words() const;
};

} // namespace simnodus::emu
