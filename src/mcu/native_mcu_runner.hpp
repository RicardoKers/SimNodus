// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Runs one microcontroller in SimNodus's own STM32F103 emulator (src/emu,
// SN-172, ADR 0089): the same McuBackend contract as RenodeMcuRunner, without
// a child process. A worker thread advances the firmware; pin changes carry
// their exact virtual time in nanoseconds.
//
// Deterministic coupling (SN-173, MCU_EMULATOR.md section 7): time is cut into
// windows of L = chunk_us. The firmware runs only up to the window boundary at
// or below the limit the circuit grants, and an input the circuit produces at
// time tc applies exactly at the next boundary at or after tc. The session
// delivers every input before it raises the limit past the input's time, so
// the result depends only on simulated times, never on thread scheduling;
// inputs are late by less than L (labeled approximate).
#pragma once

#include "mcu_backend.hpp"
#include "mcu_runner.hpp" // McuRunOptions

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace simnodus::emu {
class Stm32F103;
}

namespace simnodus::mcu {

class NativeMcuRunner : public McuBackend {
public:
    // Uses options.machine.firmware, pins, uarts, pin_model and chunk_us.
    explicit NativeMcuRunner(McuRunOptions options);
    ~NativeMcuRunner() override;

    NativeMcuRunner(const NativeMcuRunner&) = delete;
    NativeMcuRunner& operator=(const NativeMcuRunner&) = delete;

    void start() override;
    bool wait_ready(const std::atomic<bool>& cancel) override;
    void set_limit_ns(std::uint64_t limit) override;
    bool wait_time_ns(std::uint64_t time_ns, const std::atomic<bool>& cancel) override;
    void request_reset() override;
    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override;
    void set_analog(std::uint64_t time_ns, const std::vector<double>& volts) override;
    void uart_send(const std::string& uart, const std::string& bytes) override;
    std::uint64_t uart_received(const std::string& uart, std::uint64_t from, std::string& bytes,
        std::vector<std::uint64_t>* times = nullptr) const override;
    void stop() override;

    McuStatus status() const override;
    std::map<std::string, PinState> pins() const override;
    const std::vector<std::string>& pin_names() const override { return options_.pins; }
    std::vector<PinDrive> drives_at(std::uint64_t time_ns) override;
    std::vector<PinDrive> average_drives(std::uint64_t from_ns, std::uint64_t to_ns) override;
    PinLog pin_log(std::size_t pin, std::uint64_t from_ns) const override;
    std::vector<std::string> take_diagnostics() override;

private:
    using History = std::vector<std::pair<std::uint64_t, PinState>>; // sorted by time

    void work();
    void fail(const std::string& message);
    // Pin states from the machine, recorded at `time_ns` where they changed (under the lock).
    void sample_pins(emu::Stm32F103& machine, std::uint64_t time_ns);
    // Pin voltage for an ADC sample at firmware time `time_ns` (worker thread).
    double analog_value(std::size_t pin, std::uint64_t time_ns);

    McuRunOptions options_;
    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> reset_{false};
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::uint64_t limit_ns_ = 0;
    McuStatus status_;
    std::vector<std::pair<int, int>> ports_;       // per watched pin: port, number
    std::vector<History> history_;                 // per watched pin
    std::vector<std::vector<std::pair<std::uint64_t, bool>>> levels_; // per pin: level changes
    struct InputChange {
        std::size_t pin = 0;
        bool level = false;
        std::uint64_t time_ns = 0;  // circuit time
        std::uint64_t apply_ns = 0; // the window boundary at which the firmware sees it
    };
    std::vector<InputChange> input_queue_;
    std::vector<bool> input_desired_;
    std::vector<std::vector<std::pair<std::uint64_t, double>>> analog_; // per pin: circuit voltages
    struct UartLog {
        int number = 1;
        std::string bytes;
        std::vector<std::uint64_t> times;
        std::uint64_t total = 0;
        std::string pending;
    };
    std::map<std::string, UartLog> uarts_;
    std::vector<std::string> diagnostics_;
};

} // namespace simnodus::mcu
