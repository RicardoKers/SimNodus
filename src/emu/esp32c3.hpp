// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ESP32-C3 machine of the SimNodus emulator (experiment E-08, SN-244): the
// RV32IMC core (riscv_cpu.hpp), the mask ROM rebuilt from Espressif's ROM ELF
// (esp32c3_rom.hpp), internal SRAM in its instruction and data views, RTC
// memory, the external flash, and the peripherals the ROM, the second-stage
// bootloader and ESP-IDF applications use while starting. Register addresses
// and fields come from the ESP32-C3 Technical Reference Manual and ESP-IDF's
// register headers (components/soc/esp32c3, Apache-2.0). A peripheral that is
// not modelled yet keeps what is written to it and reads it back, and its
// accesses can be traced, so that a hang shows which register it waits on.
#pragma once

#include "bus.hpp"
#include "elf.hpp"
#include "riscv_cpu.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace simnodus::emu {

class Esp32C3 {
public:
    struct Options {
        std::uint32_t flash_size = 4u * 1024u * 1024u;
        // Strapping pins sampled at reset (GPIO_STRAP_REG): 0xC is SPI boot
        // (GPIO8 and GPIO9 high), as a board without buttons pressed.
        std::uint32_t strapping = 0xC;
    };
    struct Output {
        std::uint64_t time_ns = 0;
        int port = 0; // 0 UART0, 1 UART1, 2 USB-Serial-JTAG
        std::uint8_t value = 0;
    };

    Esp32C3();
    explicit Esp32C3(const Options& options);
    ~Esp32C3();
    Esp32C3(const Esp32C3&) = delete;
    Esp32C3& operator=(const Esp32C3&) = delete;

    // The ROM ELF (third_party/esp-rom-elfs/esp32c3_rev3_rom.elf).
    bool load_rom(const ElfImage& rom_elf, std::string& error);
    // Writes bytes into the flash at `offset` (a merged image goes at 0).
    bool load_flash(std::uint32_t offset, const std::vector<std::uint8_t>& bytes, std::string& error);
    // Symbols of a program in the flash, for diagnostics (bootloader or application ELF).
    void add_symbols(const ElfImage& elf);
    // Power-on reset: the core starts in the ROM.
    void reset();
    // Resets with their reason in RTC_CNTL_RESET_STATE (the ROM prints it):
    // a system reset (RTC_CNTL_SW_SYS_RST, 0x3) or a CPU reset
    // (RTC_CNTL_SW_PROCPU_RST, 0xC) keep the SRAM and the RTC domain;
    // the run loop takes a requested reset between instructions.
    enum class ResetKind { PowerOn, System, Cpu };
    void reset(ResetKind kind);
    void request_reset(ResetKind kind);
    int resets() const { return resets_; }

    void run_until_ns(std::uint64_t ns);
    std::uint64_t time_ns() const;
    std::uint32_t cpu_hz() const { return cpu_hz_; }

    std::vector<Output> take_output();
    // Bytes from outside for port 0 (UART0), 1 (UART1) or 2 (USB-Serial-JTAG).
    void send(int port, const std::string& bytes);
    std::vector<std::string> take_diagnostics();
    // Traces every access to a peripheral register (for finding what the
    // firmware waits on). The text names the register block, the offset, the
    // value, and the function at the PC.
    void set_trace(std::function<void(const std::string&)> trace) { trace_ = std::move(trace); }
    // Function name containing `address` from the ROM or an added ELF, or empty.
    std::string function_at(std::uint32_t address) const;

    RiscvCpu& cpu() { return cpu_; }
    Bus& bus() { return bus_; }
    const std::vector<std::uint8_t>& flash() const { return flash_; }

    class Block;     // a peripheral's register block (esp32c3.cpp)
    class FlashChip; // the SPI NOR flash (esp32c3.cpp)
    class Interrupts; // interrupt matrix and controller (esp32c3.cpp)
    // A block with events at known times in nanoseconds (timers); the run
    // loop stops the core at the earliest one.
    class Timed {
    public:
        virtual ~Timed() = default;
        virtual std::uint64_t next_ns() const = 0; // no_event for none
        virtual void on_ns(std::uint64_t now) = 0;
    };
    // Level of interrupt source `source` (ETS_*_INTR_SOURCE, 0..61).
    void set_interrupt_source(int source, bool level);

private:
    friend class Block;
    void output_byte(int port, std::uint8_t value);
    void diagnostic(const std::string& text);
    void trace_access(const Block& block, std::uint32_t offset, bool write, std::uint32_t value);
    void attach(std::uint32_t base, std::unique_ptr<Block> block, std::uint32_t size = 0x1000);
    // Cache and MMU: entry `page` maps 64 KiB of flash into both windows.
    void map_page(std::size_t page);
    void invalidate_cache(); // the cache reads the flash again
    void set_cpu_hz(std::uint32_t hz);
    void run_timers();
    std::uint64_t cycles_for_ns(std::uint64_t ns) const; // at the present clock, rounded up

    Options options_;
    Bus bus_;
    RiscvCpu cpu_;
    std::vector<std::unique_ptr<Block>> blocks_;
    std::deque<std::string> unnamed_; // names of the generic blocks (stable addresses)
    std::vector<Timed*> timed_;
    Block* rtc_cntl_ = nullptr;
    Block* usb_ = nullptr;
    bool reset_requested_ = false;
    ResetKind requested_kind_ = ResetKind::PowerOn;
    int resets_ = 0;
    Interrupts* interrupts_ = nullptr;
    std::vector<std::uint8_t> flash_;
    std::unique_ptr<FlashChip> flash_chip_;
    std::uint8_t* cache_window_ = nullptr; // 8 MiB, instruction and data views
    std::uint32_t mmu_[128] = {};
    std::vector<std::uint8_t> rom_;
    std::vector<ElfSymbol> functions_; // sorted, from the ROM and added ELF files

    // Time: the core's clock changes when the firmware switches it.
    std::uint32_t cpu_hz_ = 40'000'000; // the 40 MHz crystal after reset
    std::uint64_t base_ns_ = 0;
    std::uint64_t base_cycles_ = 0;

    static bool vendor_csr(std::uint16_t csr);
    void trace_csr(std::uint16_t csr, bool write, std::uint32_t value);
    std::map<std::uint16_t, std::uint32_t> vendor_csrs_;
    std::uint64_t pccr_base_ = 0;  // mpccr counts from here
    std::uint64_t upccr_base_ = 0; // CSR 0x802 counts from here
    bool halt_reported_ = false;

    std::vector<Output> output_;
    std::vector<std::string> diagnostics_;
    std::function<void(const std::string&)> trace_;
};

} // namespace simnodus::emu
