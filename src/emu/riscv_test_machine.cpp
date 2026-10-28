// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "riscv_test_machine.hpp"

#include <algorithm>

namespace simnodus::emu {

namespace {

RiscvCpu::Config test_config()
{
    RiscvCpu::Config config;
    config.reset_pc = RiscvTestMachine::ram_base;
    return config;
}

} // namespace

RiscvTestMachine::RiscvTestMachine() : RiscvTestMachine(Options{}) {}

RiscvTestMachine::RiscvTestMachine(const Options& options)
    : options_(options), cpu_(bus_, test_config())
{
    bus_.add_memory("RAM", ram_base, options.ram_size, true);
    finisher_.machine = this;
    clint_.machine = this;
    bus_.add_device(uart_base, 0x100, &uart_);
    bus_.add_device(finisher_base, 0x1000, &finisher_);
    bus_.add_device(clint_base, 0x10000, &clint_);
    cpu_.add_code_region(ram_base, options.ram_size);
}

bool RiscvTestMachine::load(const ElfImage& image, std::string& error)
{
    bus_.fill(ram_base, 0, options_.ram_size);
    for (const ElfSegment& segment : image.segments) {
        if (!bus_.load(segment.physical, segment.data.data(), static_cast<std::uint32_t>(segment.data.size()))) {
            error = "A segment of the ELF file lies outside the test machine's RAM.";
            return false;
        }
    }
    cpu_.reset();
    cpu_.set_pc(image.entry);
    finished_ = false;
    exit_code_ = 0;
    uart_.output.clear();
    clint_.mtimecmp = ~0ull;
    clint_.msip = false;
    update_lines();
    return true;
}

std::uint64_t RiscvTestMachine::mtime() const
{
    // cycles * timebase / cpu_hz without overflowing 64 bits
    const std::uint64_t cycles = cpu_.cycles();
    return cycles / options_.cpu_hz * timebase_hz + cycles % options_.cpu_hz * timebase_hz / options_.cpu_hz;
}

std::uint64_t RiscvTestMachine::time_ns() const
{
    const std::uint64_t cycles = cpu_.cycles();
    return cycles / options_.cpu_hz * 1'000'000'000ull + cycles % options_.cpu_hz * 1'000'000'000ull / options_.cpu_hz;
}

std::uint64_t RiscvTestMachine::timer_event() const
{
    if (clint_.mtimecmp / timebase_hz >= no_event / options_.cpu_hz) {
        return no_event; // never, in practice
    }
    // First cycle c with c * timebase / cpu_hz >= mtimecmp.
    const std::uint64_t whole = clint_.mtimecmp / timebase_hz * options_.cpu_hz;
    const std::uint64_t part = (clint_.mtimecmp % timebase_hz * options_.cpu_hz + timebase_hz - 1) / timebase_hz;
    return whole + part;
}

void RiscvTestMachine::update_lines()
{
    std::uint32_t lines = 0;
    if (clint_.msip) {
        lines |= 1u << 3;
    }
    if (mtime() >= clint_.mtimecmp) {
        lines |= 1u << 7;
    }
    cpu_.set_interrupt_lines(lines);
}

bool RiscvTestMachine::run(std::uint64_t max_cycles)
{
    const std::uint64_t end = cpu_.cycles() + max_cycles;
    while (!finished_ && cpu_.cycles() < end) {
        const std::uint64_t event = timer_event();
        const std::uint64_t limit = std::max(std::min(end, event), cpu_.cycles() + 1);
        const RvState state = cpu_.run(limit);
        update_lines();
        if (state == RvState::Halted) {
            break;
        }
    }
    return finished_;
}

void RiscvTestMachine::run_until_ns(std::uint64_t ns)
{
    const std::uint64_t target = ns / 1'000'000'000ull * options_.cpu_hz
        + ns % 1'000'000'000ull * options_.cpu_hz / 1'000'000'000ull;
    if (target > cpu_.cycles()) {
        run(target - cpu_.cycles());
    }
}

bool RiscvTestMachine::Uart::read(std::uint32_t offset, unsigned, std::uint32_t& value)
{
    value = offset == 5 ? 0x60 : 0; // LSR: the transmitter is always empty
    return true;
}

bool RiscvTestMachine::Uart::write(std::uint32_t offset, unsigned, std::uint32_t value)
{
    if (offset == 0) {
        output.push_back(static_cast<char>(value & 0xFF));
    }
    return true;
}

bool RiscvTestMachine::Finisher::read(std::uint32_t, unsigned, std::uint32_t& value)
{
    value = 0;
    return true;
}

bool RiscvTestMachine::Finisher::write(std::uint32_t offset, unsigned, std::uint32_t value)
{
    if (offset == 0) {
        const std::uint32_t kind = value & 0xFFFF;
        if (kind == 0x5555 || kind == 0x3333) {
            machine->finished_ = true;
            machine->exit_code_ = kind == 0x5555 ? 0 : (value >> 16);
            machine->cpu_.request_stop();
        }
    }
    return true;
}

bool RiscvTestMachine::Clint::read(std::uint32_t offset, unsigned size, std::uint32_t& value)
{
    std::uint64_t word = 0;
    std::uint32_t at = offset;
    if (offset < 4) {
        word = msip ? 1 : 0;
    } else if (offset >= 0x4000 && offset < 0x4008) {
        word = mtimecmp;
        at = offset - 0x4000;
    } else if (offset >= 0xBFF8 && offset < 0xC000) {
        word = machine->mtime();
        at = offset - 0xBFF8;
    } else {
        value = 0;
        return true;
    }
    value = static_cast<std::uint32_t>(word >> (8 * at));
    if (size < 4) {
        value &= (1u << (8 * size)) - 1;
    }
    return true;
}

bool RiscvTestMachine::Clint::write(std::uint32_t offset, unsigned size, std::uint32_t value)
{
    if (size != 4) {
        return true;
    }
    if (offset == 0) {
        msip = (value & 1) != 0;
    } else if (offset == 0x4000) {
        mtimecmp = (mtimecmp & ~0xFFFFFFFFull) | value;
    } else if (offset == 0x4004) {
        mtimecmp = (mtimecmp & 0xFFFFFFFFull) | (static_cast<std::uint64_t>(value) << 32);
    }
    machine->update_lines();
    machine->cpu_.request_stop(); // the next timer event may have moved
    return true;
}

} // namespace simnodus::emu
