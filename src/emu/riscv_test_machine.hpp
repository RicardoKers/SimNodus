// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// A bare RISC-V machine for testing the RV32IMC core (experiment E-08): RAM,
// a 16550 transmit register, the SiFive test finisher and a CLINT timer at the
// addresses of QEMU's `virt` board, so that one ELF runs unchanged in both for
// the differential comparison, the architecture tests and the benchmark. It
// is not a model of any chip.
#pragma once

#include "bus.hpp"
#include "elf.hpp"
#include "riscv_cpu.hpp"
#include "timed_device.hpp" // no_event

#include <cstdint>
#include <string>

namespace simnodus::emu {

class RiscvTestMachine {
public:
    static constexpr std::uint32_t ram_base = 0x80000000u;
    static constexpr std::uint32_t uart_base = 0x10000000u;
    static constexpr std::uint32_t finisher_base = 0x00100000u;
    static constexpr std::uint32_t clint_base = 0x02000000u;
    static constexpr std::uint64_t timebase_hz = 10'000'000; // CLINT mtime, as on QEMU's virt

    struct Options {
        std::uint32_t ram_size = 8u * 1024u * 1024u;
        std::uint64_t cpu_hz = 160'000'000; // the ESP32-C3's clock
    };

    RiscvTestMachine();
    explicit RiscvTestMachine(const Options& options);
    RiscvTestMachine(const RiscvTestMachine&) = delete;
    RiscvTestMachine& operator=(const RiscvTestMachine&) = delete;

    // Loads the image's segments into RAM, resets and starts at its entry.
    bool load(const ElfImage& image, std::string& error);
    // Runs until the program writes the finisher, the core halts, or
    // `max_cycles` more cycles have passed. Returns true if it finished.
    bool run(std::uint64_t max_cycles);
    // Runs until virtual time `ns` (or the finisher).
    void run_until_ns(std::uint64_t ns);

    bool finished() const { return finished_; }
    // The finisher's code: 0 for pass (0x5555), the code for fail (0x3333).
    std::uint32_t exit_code() const { return exit_code_; }
    const std::string& output() const { return uart_.output; }
    std::uint64_t time_ns() const;

    RiscvCpu& cpu() { return cpu_; }
    Bus& bus() { return bus_; }

private:
    struct Uart : Device {
        std::string output;
        const char* name() const override { return "UART (16550)"; }
        bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override;
        bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override;
    };
    struct Finisher : Device {
        RiscvTestMachine* machine = nullptr;
        const char* name() const override { return "test finisher"; }
        bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override;
        bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override;
    };
    struct Clint : Device {
        RiscvTestMachine* machine = nullptr;
        std::uint64_t mtimecmp = ~0ull;
        bool msip = false;
        const char* name() const override { return "CLINT"; }
        bool read(std::uint32_t offset, unsigned size, std::uint32_t& value) override;
        bool write(std::uint32_t offset, unsigned size, std::uint32_t value) override;
    };

    std::uint64_t mtime() const;
    std::uint64_t timer_event() const; // cycle at which mtime reaches mtimecmp
    void update_lines();

    Options options_;
    Bus bus_;
    RiscvCpu cpu_;
    Uart uart_;
    Finisher finisher_;
    Clint clint_;
    bool finished_ = false;
    std::uint32_t exit_code_ = 0;
};

} // namespace simnodus::emu
