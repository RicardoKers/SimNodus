// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// GDB remote serial protocol server for the STM32F103 emulator (SN-180,
// MCU-08), for STM32CubeIDE ("Connect to remote GDB server") and
// arm-none-eabi-gdb. Supported: Cortex-M target description (r0-r12, sp, lr,
// pc, xpsr, msp, psp, primask, control), register and memory reads and writes
// (writes to flash go through the loader, so `load` works; the first run or
// step after a flash write resets the chip, as an ST-LINK does), breakpoints (Z0/Z1,
// any number), continue, step, Ctrl-C, `monitor reset` / `monitor reset halt`,
// detach and kill (the firmware runs on). Not yet: watchpoints (Z2-Z4).
//
// The machine's owner calls service() between run steps and after_run() after
// each, on the thread that runs the machine. A client that connects halts the
// firmware where it is.
#pragma once

#include "gdb_socket.hpp"

#include <cstdint>
#include <set>
#include <string>

namespace simnodus::emu {

class Stm32F103;

class GdbServer {
public:
    explicit GdbServer(Stm32F103& machine);
    ~GdbServer();
    GdbServer(const GdbServer&) = delete;
    GdbServer& operator=(const GdbServer&) = delete;

    bool start(std::uint16_t port, std::string& error); // 0: any free port
    std::uint16_t port() const { return socket_.port(); }

    // Accepts a client and handles its packets. Returns true if the firmware may
    // run now (no client, or the client said continue). While the firmware is
    // halted, waits up to wait_ms for packets.
    bool service(int wait_ms);
    // After a run step: reports a breakpoint halt to the client.
    void after_run();

    bool attached() const { return socket_.connected(); }
    bool halted() const { return state_ == State::Halted; }
    // Until the first client connects, service() keeps the firmware halted (a
    // debugging run starts with the firmware waiting at its reset handler).
    void wait_for_client(bool on) { wait_first_ = on; }
    bool waiting() const { return wait_first_ && state_ == State::NoClient; }

private:
    enum class State { NoClient, Halted, Running };

    void attach();
    void detach();
    void handle(const std::string& packet);
    void reply(const std::string& payload) { socket_.send(payload); }
    std::string read_registers() const;
    std::uint32_t read_register(int n) const;
    bool write_register(int n, std::uint32_t value);
    std::string read_memory(std::uint32_t address, std::uint32_t length) const;
    bool write_memory(std::uint32_t address, const std::string& bytes);
    void monitor(const std::string& command);
    void update_breakpoints();
    void start_after_load();

    Stm32F103& machine_;
    GdbSocket socket_;
    State state_ = State::NoClient;
    std::set<std::uint32_t> breakpoints_;
    bool wait_first_ = false;
    bool flash_written_ = false; // since the last reset: the next run starts with one
};

} // namespace simnodus::emu
