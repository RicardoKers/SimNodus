// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Runs one microcontroller (SN-131): a worker thread owns a RenodeEngine,
// advances virtual time up to a limit set by the analog run, and keeps the
// level and mode of every watched GPIO pin for the display (INT-06). The
// analog run sets the limit, so the firmware never runs ahead of the circuit
// by more than the lead it allows; the electrical coupling is SN-132.
#pragma once

#include "renode_engine.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace simnodus::mcu {

struct GpioPin {
    std::string port; // Renode peripheral, e.g. "gpioPortA"
    int number = 0;
};

// "PA0" -> {"gpioPortA", 0}; nullopt for power and control pins.
std::optional<GpioPin> gpio_pin(const std::string& name);

enum class PinMode { Unknown, Input, Analog, Output, Alternate };

// Mode of an STM32F1 pin from its CRL/CRH nibble (MODE bits 1:0, CNF bits 3:2).
// MODE 00 is reported as Input: Renode's model resets CNF to 00 although the
// chip resets to 01 (floating input), so analog cannot be told apart there.
PinMode decode_pin_mode(std::uint32_t nibble);

struct PinState {
    bool level = false; // output level (for outputs and alternate functions)
    PinMode mode = PinMode::Unknown;
};

enum class McuState { Starting, Running, Failed, Stopped };

struct McuStatus {
    McuState state = McuState::Stopped;
    std::uint64_t time_us = 0;
    double startup_seconds = 0.0;
    int resets = 0;      // resets done with the reset button
    std::string message; // failure reason
};

struct McuRunOptions {
    MachineOptions machine;
    std::vector<std::string> pins; // GPIO pin names to watch, e.g. "PC13"
    std::uint64_t chunk_us = 10000; // longest single run_for
};

class McuRunner {
public:
    explicit McuRunner(McuRunOptions options);
    ~McuRunner(); // stop()

    McuRunner(const McuRunner&) = delete;
    McuRunner& operator=(const McuRunner&) = delete;

    // Starts the worker; Renode starts in it (seconds). Returns at once.
    void start();
    // Waits until the firmware is ready to run, the start failed, or `cancel`
    // becomes true. Returns true if it is running.
    bool wait_ready(const std::atomic<bool>& cancel);
    // Virtual time (microseconds) the firmware may advance to.
    void set_limit_us(std::uint64_t limit);
    // Blocks until the firmware has reached `time_us`, it stopped or failed, or
    // `cancel` becomes true. Returns true if the time was reached.
    bool wait_time_us(std::uint64_t time_us, const std::atomic<bool>& cancel);
    // System reset of the microcontroller (the board's reset button).
    void request_reset();
    void stop();

    McuStatus status() const;
    std::map<std::string, PinState> pins() const;

private:
    void work();
    void read_modes(RenodeEngine& engine);
    void fail(const std::string& message);

    McuRunOptions options_;
    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> reset_{false};
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::uint64_t limit_us_ = 0;
    McuStatus status_;
    std::map<std::string, PinState> pins_;
    std::map<std::string, std::string> event_pin_; // "port:number" -> pin name
};

} // namespace simnodus::mcu
