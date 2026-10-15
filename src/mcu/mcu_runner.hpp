// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Runs one microcontroller in Renode (SN-131, SN-132; McuBackend since SN-171):
// a worker thread owns a RenodeEngine, advances virtual time up to a limit set by the analog run, and
// records every watched GPIO pin's level and configuration over time. The
// display shows the latest state (INT-06); the analog run asks for the pins'
// electrical drive at its own time (MCU-03), which the firmware has always
// reached before (the circuit waits otherwise).
#pragma once

#include "mcu_backend.hpp"
#include "renode_engine.hpp"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace simnodus::mcu {

struct GpioPin {
    std::string port; // Renode peripheral, e.g. "gpioPortA"
    int number = 0;
};

// "PA0" -> {"gpioPortA", 0}; nullopt for power and control pins.
std::optional<GpioPin> gpio_pin(const std::string& name);

// Mode of an STM32F1 pin from its CRL/CRH nibble (MODE bits 1:0, CNF bits 3:2).
// Needs the SimNodus GPIO model (resources/renode/SimNodusGpio.cs): Renode's
// own model resets CNF to 00, which reads as analog.
PinMode decode_pin_mode(std::uint32_t nibble);

// Full configuration from the CRL/CRH nibble and the pin's ODR bit.
PinState decode_pin_config(std::uint32_t nibble, bool odr_bit);

struct McuRunOptions {
    MachineOptions machine;
    std::vector<std::string> pins; // GPIO pin names to watch, e.g. "PC13"
    std::vector<std::string> uarts; // UART peripherals for the terminal, e.g. "usart1"
    // TX pin of each UART ("usart1" -> "PA9"). Renode moves bytes without
    // driving the pin, so a watched TX pin in alternate-function mode is held
    // high (the line at rest) while the USART and its transmitter are enabled.
    std::map<std::string, std::string> uart_tx_pins;
    PinModel pin_model;
    // Longest single run_for (the coupling quantum). The runner may run one chunk
    // past the limit and reports a chunk as complete one chunk later.
    std::uint64_t chunk_us = 1000;
    std::uint64_t config_interval_us = 10000; // CRL/CRH/ODR reads (each costs a bus round trip)
};

class RenodeMcuRunner : public McuBackend {
public:
    explicit RenodeMcuRunner(McuRunOptions options);
    ~RenodeMcuRunner() override; // stop()

    RenodeMcuRunner(const RenodeMcuRunner&) = delete;
    RenodeMcuRunner& operator=(const RenodeMcuRunner&) = delete;

    // McuBackend. Renode's time grid is the microsecond: times are rounded down
    // to it on the way in and multiplied on the way out.
    void start() override;
    bool wait_ready(const std::atomic<bool>& cancel) override;
    void set_limit_ns(std::uint64_t limit) override;
    bool wait_time_ns(std::uint64_t time_ns, const std::atomic<bool>& cancel) override;
    void request_reset() override;
    void set_input(std::size_t pin, bool level, std::uint64_t time_ns) override;
    // Bytes reach the receiver before the next run chunk; the newest 1 MiB of
    // transmitted bytes is kept.
    void uart_send(const std::string& uart, const std::string& bytes) override;
    std::uint64_t uart_received(const std::string& uart, std::uint64_t from, std::string& bytes,
        std::vector<std::uint64_t>* times = nullptr) const override;
    void stop() override;

    McuStatus status() const override;
    std::map<std::string, PinState> pins() const override;
    const std::vector<std::string>& pin_names() const override { return options_.pins; }
    // History older than 100 ms before `time_ns` is discarded.
    std::vector<PinDrive> drives_at(std::uint64_t time_ns) override;
    // The newest 50 000 changes per pin are kept.
    PinLog pin_log(std::size_t pin, std::uint64_t from_ns) const override;

private:
    using History = std::vector<std::pair<std::uint64_t, PinState>>; // sorted by time

    void work();
    // Configuration of every watched pin from CRL/CRH/ODR; false on a bus error.
    bool read_configs(RenodeEngine& engine, std::vector<PinState>& configs);
    // Records configurations that changed, effective from `time_us` (under the lock).
    void apply_configs(std::uint64_t time_us, const std::vector<PinState>& configs);
    void record_level(std::size_t pin, std::uint64_t time_us, bool level);
    void fail(const std::string& message);

    McuRunOptions options_;
    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> reset_{false};
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::uint64_t limit_us_ = 0;
    McuStatus status_;
    std::vector<History> history_;                 // per watched pin
    std::vector<std::uint64_t> tx_usart_;          // per watched pin: USART base if it is a TX pin, else 0
    struct Input {
        bool desired = false;
        bool applied = false;
        std::uint64_t since_us = 0;
    };
    std::vector<Input> inputs_; // per watched pin
    // Every input change in circuit order: applied one after the other with the
    // same spacing, so pulses shorter than a run chunk (contact bounce) survive.
    struct InputChange {
        std::size_t pin = 0;
        bool level = false;
        std::uint64_t time_us = 0;
    };
    std::vector<InputChange> input_queue_;
    std::vector<std::vector<std::pair<std::uint64_t, bool>>> levels_; // per pin: level changes, from time 0
    struct UartLog {
        std::string bytes;      // the newest bytes
        std::vector<std::uint64_t> times; // virtual time of each kept byte
        std::uint64_t total = 0; // bytes transmitted so far
        std::string pending;    // to send to the receiver
    };
    std::map<std::string, UartLog> uarts_;
    std::map<std::string, std::size_t> event_pin_; // "port:number" -> index in options_.pins
};

} // namespace simnodus::mcu
