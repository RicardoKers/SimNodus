// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Runs one microcontroller (SN-131, SN-132): a worker thread owns a
// RenodeEngine, advances virtual time up to a limit set by the analog run, and
// records every watched GPIO pin's level and configuration over time. The
// display shows the latest state (INT-06); the analog run asks for the pins'
// electrical drive at its own time (MCU-03), which the firmware has always
// reached before (the circuit waits otherwise).
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
#include <utility>
#include <vector>

namespace simnodus::mcu {

struct GpioPin {
    std::string port; // Renode peripheral, e.g. "gpioPortA"
    int number = 0;
};

// "PA0" -> {"gpioPortA", 0}; nullopt for power and control pins.
std::optional<GpioPin> gpio_pin(const std::string& name);

enum class PinMode { Unknown, Input, Analog, Output, Alternate };
enum class Pull { None, Up, Down };

// Mode of an STM32F1 pin from its CRL/CRH nibble (MODE bits 1:0, CNF bits 3:2).
// MODE 00 is reported as Input: Renode's model resets CNF to 00 although the
// chip resets to 01 (floating input), so analog cannot be told apart there.
PinMode decode_pin_mode(std::uint32_t nibble);

struct PinState {
    bool level = false; // output level (for outputs and alternate functions)
    PinMode mode = PinMode::Unknown;
    bool open_drain = false; // outputs: CNF bit 2
    Pull pull = Pull::None;  // inputs: CNF 10 with the ODR bit choosing up or down

    bool same_config(const PinState& other) const
    {
        return mode == other.mode && open_drain == other.open_drain && pull == other.pull;
    }
};

// Full configuration from the CRL/CRH nibble and the pin's ODR bit.
PinState decode_pin_config(std::uint32_t nibble, bool odr_bit);

// Electrical GPIO model (MCU-03): the pin pushes current
// `current - conductance * V(pin)` into its net (V relative to the board's
// ground). Push-pull outputs are a source of `supply` or 0 V behind
// `output_resistance`; open-drain outputs only pull low; inputs are open
// unless a pull resistor is enabled.
struct PinModel {
    double supply = 3.3;
    double output_resistance = 40.0;
    double pull_resistance = 40000.0;
};
struct PinDrive {
    double conductance = 0.0; // S
    double current = 0.0;     // A
};
PinDrive pin_drive(const PinState& state, const PinModel& model);

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
    PinModel pin_model;
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
    // Latest state of every watched pin (the firmware's current time).
    std::map<std::string, PinState> pins() const;
    const std::vector<std::string>& pin_names() const { return options_.pins; }
    // Drive of each watched pin (in pin_names() order) at virtual time
    // `time_us`; history older than 100 ms before it is discarded.
    std::vector<PinDrive> drives_at(std::uint64_t time_us);

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
    std::map<std::string, std::size_t> event_pin_; // "port:number" -> index in options_.pins
};

} // namespace simnodus::mcu
