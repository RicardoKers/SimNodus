// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// GDB remote serial protocol server for SimNodus's emulators (SN-180, MCU-08;
// RISC-V SN-249), for STM32CubeIDE ("Connect to remote GDB server"),
// arm-none-eabi-gdb and riscv32-esp-elf-gdb. Supported: the target's register
// description, register and memory reads and writes, breakpoints (Z0/Z1, any
// number), continue, step, Ctrl-C, `monitor reset` / `monitor reset halt`,
// detach and kill (the firmware runs on), watchpoints on writes, reads or both
// (Z2-Z4). The chip is a GdbTarget: the STM32F103 (writes to flash go
// through the loader, so `load` works; the first run or step after a flash
// write resets the chip, as an ST-LINK does) or the ESP32-C3 (its flash is
// written with esptool or the board's Firmware; gdb reads the ELF's symbols).
//
// The machine's owner calls service() between run steps and after_run() after
// each, on the thread that runs the machine. A client that connects halts the
// firmware where it is.
#pragma once

#include "gdb_socket.hpp"

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace simnodus::emu {

class Esp32C3;
class Stm32F103;

// A chip as the debugger sees it.
class GdbTarget {
public:
    enum class WatchKind { Write = 2, Read = 3, Access = 4 }; // GDB's Z2, Z3, Z4
    struct Watchpoint {
        std::uint32_t address = 0;
        std::uint32_t length = 1;
        WatchKind kind = WatchKind::Write;
    };
    struct WatchHit {
        bool hit = false;
        std::uint32_t address = 0;
        WatchKind kind = WatchKind::Write;
    };

    virtual ~GdbTarget() = default;
    // The target description (qXfer:features:read:target.xml).
    virtual const char* target_xml() const = 0;
    // The registers of the `g` packet, in order (numbers of the description).
    virtual int general_registers() const = 0;
    virtual bool read_register(int n, std::uint32_t& value) = 0;
    virtual bool write_register(int n, std::uint32_t value) = 0;
    virtual bool read_byte(std::uint32_t address, std::uint8_t& value) = 0;
    // False if an address cannot be written. `loaded`: code was written as
    // `load` writes it (the next run or step starts from a reset).
    virtual bool write_memory(std::uint32_t address, const std::string& bytes, bool& loaded) = 0;
    virtual void set_debugger_attached(bool attached) = 0;
    virtual void set_breakpoints(std::vector<std::uint32_t> addresses) = 0;
    virtual void set_watchpoints(const std::vector<Watchpoint>& watchpoints) = 0;
    virtual WatchHit last_watch() const = 0;
    virtual bool halted() const = 0; // at a breakpoint or watchpoint
    virtual void resume() = 0;       // from a halt, past a breakpoint at the PC
    virtual void step() = 0;         // one instruction
    virtual void reset() = 0;        // the chip's reset; the firmware stays halted
};

std::unique_ptr<GdbTarget> make_gdb_target(Stm32F103& machine);
std::unique_ptr<GdbTarget> make_gdb_target(Esp32C3& machine);

class GdbServer {
public:
    explicit GdbServer(GdbTarget& target);
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
    void release_target(); // the firmware runs free, without breakpoints
    void detach();
    void handle(const std::string& packet);
    void reply(const std::string& payload) { socket_.send(payload); }
    std::string read_registers();
    std::string read_memory(std::uint32_t address, std::uint32_t length);
    void monitor(const std::string& command);
    void update_breakpoints();
    std::string stop_reply() const;
    void start_after_load();

    GdbTarget& target_;
    GdbSocket socket_;
    State state_ = State::NoClient;
    std::set<std::uint32_t> breakpoints_;
    std::vector<GdbTarget::Watchpoint> watchpoints_;
    bool wait_first_ = false;
    bool flash_written_ = false; // since the last reset: the next run starts with one
};

} // namespace simnodus::emu
