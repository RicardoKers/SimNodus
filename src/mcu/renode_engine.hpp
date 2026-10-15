// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The microcontroller engine (SN-130): one Renode 1.16.1 process per run,
// controlled over Renode's external control protocol on a loopback-only
// port (ADR 0082). Virtual time advances only in run_for(); GPIO changes of
// watched pins are collected with their virtual timestamps while it runs.
//
// Renode is started only by start(), never when a project is opened. The
// engine is not thread-safe: one thread owns it. On Linux the child receives
// SIGKILL when the thread that started it exits, so start it on the thread
// that runs the simulation.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::mcu {

// The Renode executable: $SIMNODUS_RENODE, then renode/ next to the
// application (directly or in one sub-folder), then the development copy
// staged by tools/fetch_backends.py --renode.
std::optional<std::filesystem::path> find_renode(const std::filesystem::path& application_dir);
// Folder with LoopbackControl.cs and the platform descriptions: mcu/ next to
// the application, ../share/simnodus/mcu (Linux install), then resources/renode
// in the source tree.
std::optional<std::filesystem::path> find_mcu_resources(const std::filesystem::path& application_dir);

struct MachineOptions {
    std::filesystem::path renode;    // executable
    std::filesystem::path resources; // see find_mcu_resources()
    std::string platform = "stm32f103c8.repl";
    // C# peripheral models in `resources` that the platform uses; Renode
    // compiles them at start-up (trusted application files).
    std::vector<std::string> extensions{"SimNodusGpio.cs", "SimNodusRcc.cs"};
    std::filesystem::path firmware;  // ELF; empty: no firmware
    // Monitor commands of the board profile, run after the platform is
    // loaded and after the firmware is loaded. Trusted application text.
    std::vector<std::string> before_firmware;
    std::vector<std::string> after_firmware;
    std::uint32_t quantum_us = 100; // Renode global quantum
    // Generous: a cold first start on a slow PC took 54 s in CI (warm: 4-9 s).
    std::chrono::milliseconds start_timeout{120000};
    const std::atomic<bool>* cancel = nullptr; // start() gives up when it becomes true
};

struct GpioEvent {
    std::uint64_t time_us = 0; // Renode virtual time
    std::string port;
    int pin = 0;
    bool state = false;
};

struct UartByte {
    std::uint64_t time_us = 0; // Renode virtual time
    std::string uart;          // peripheral, e.g. "usart1"
    std::uint8_t value = 0;
};

class RenodeEngine {
public:
    static std::unique_ptr<RenodeEngine> start(const MachineOptions& options, std::string& error);
    ~RenodeEngine(); // stop()

    RenodeEngine(const RenodeEngine&) = delete;
    RenodeEngine& operator=(const RenodeEngine&) = delete;

    // False after a transport failure, a protocol error or a backend exit.
    bool usable() const;
    const std::string& error() const { return error_; }

    std::optional<std::uint64_t> time_us();
    // Advances virtual time by exactly `microseconds`; the wall-time limit
    // grows with the request. Gpio events arrive during the call.
    bool run_for(std::uint64_t microseconds);

    // GPIO by peripheral name ("gpioPortA") and pin number.
    bool watch_gpio(const std::string& port, int pin);
    std::vector<GpioEvent> take_gpio_events();
    bool set_gpio(const std::string& port, int pin, bool state); // does not advance time
    std::optional<bool> gpio(const std::string& port, int pin);

    // UART by peripheral name ("usart1"): bytes the firmware transmits arrive
    // during run_for; written bytes reach the peripheral's receiver at once.
    bool watch_uart(const std::string& uart);
    std::vector<UartByte> take_uart_bytes();
    bool write_uart(const std::string& uart, const std::vector<std::uint8_t>& bytes);

    std::optional<std::uint32_t> read32(std::uint64_t address);
    bool write32(std::uint64_t address, std::uint32_t value);

    // Asks Renode to quit, kills it after `timeout`. Returns the exit code
    // (nullopt if it had to be killed). The run folder is removed after a
    // clean exit and kept with its log otherwise.
    std::optional<int> stop(std::chrono::milliseconds timeout = std::chrono::milliseconds(10000));

    long long process_id() const;
    const std::filesystem::path& run_directory() const { return run_dir_; }
    std::filesystem::path log_file() const { return run_dir_ / "renode.log"; }
    // The last lines of the Renode log, for diagnostics.
    std::string log_tail(std::size_t lines = 20) const;

private:
    RenodeEngine();
    bool check(void* renode_error, const char* what);
    void* gpio_handle(const std::string& port);
    void* uart_handle(const std::string& uart);

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::filesystem::path run_dir_;
    std::string error_;
};

} // namespace simnodus::mcu
