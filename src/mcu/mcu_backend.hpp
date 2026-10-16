// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Microcontroller engine interface (SN-171, docs/planning/MCU_EMULATOR_PLAN.md):
// what SimulationSession needs from a running board, independent of the engine
// (Renode today, SimNodus's own emulator later). Times are virtual
// nanoseconds since the run started; pins are named as on the board ("PA9").
#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace simnodus::mcu {

enum class PinMode { Unknown, Input, Analog, Output, Alternate };
enum class Pull { None, Up, Down };

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

// Electrical GPIO model (MCU-03): the pin pushes current
// `current - conductance * V(pin)` into its net (V relative to the board's
// ground). Push-pull outputs are a source of `supply` or 0 V behind
// `output_resistance`; open-drain outputs only pull low; inputs are open
// unless a pull resistor is enabled.
struct PinModel {
    double supply = 3.3;
    double output_resistance = 40.0;
    double pull_resistance = 40000.0;
    double input_high = 1.83; // Schmitt thresholds of inputs (MCU-04)
    double input_low = 1.16;
};

// Input level after a voltage sample: 1 above input_high, 0 below input_low,
// unchanged in between.
bool schmitt(bool previous, double volts, const PinModel& model);
struct PinDrive {
    double conductance = 0.0; // S
    double current = 0.0;     // A
};
PinDrive pin_drive(const PinState& state, const PinModel& model);

// Level changes of one pin for the logic lanes (AN-07): the level at `from`
// and every change after it, in virtual nanoseconds.
struct PinLog {
    bool initial = false;
    std::vector<std::pair<std::uint64_t, bool>> changes;
};

enum class McuState { Starting, Running, Failed, Stopped };

struct McuStatus {
    McuState state = McuState::Stopped;
    std::uint64_t time_ns = 0;
    double startup_seconds = 0.0;
    int resets = 0;                       // resets done with the reset button
    std::uint64_t max_input_delay_ns = 0; // circuit-to-firmware delay of input changes
    std::string message;                  // failure reason
};

class McuBackend {
public:
    virtual ~McuBackend() = default;

    // Starts the engine (it may take seconds); returns at once.
    virtual void start() = 0;
    // Waits until the firmware is ready to run, the start failed, or `cancel`
    // becomes true. Returns true if it is running.
    virtual bool wait_ready(const std::atomic<bool>& cancel) = 0;
    // Virtual time the firmware may advance to.
    virtual void set_limit_ns(std::uint64_t limit) = 0;
    // Blocks until the firmware has reached `time_ns`, it stopped or failed, or
    // `cancel` becomes true. Returns true if the time was reached.
    virtual bool wait_time_ns(std::uint64_t time_ns, const std::atomic<bool>& cancel) = 0;
    // System reset of the microcontroller (the board's reset button).
    virtual void request_reset() = 0;
    // Logic level the circuit applies to watched pin `pin` (index in
    // pin_names()) since circuit time `time_ns` (MCU-04).
    virtual void set_input(std::size_t pin, bool level, std::uint64_t time_ns) = 0;
    // Terminal (MCU-05): bytes for a UART's receiver; and the bytes it
    // transmitted since byte number `from`, with their times if `times` is
    // given. Returns the total number of bytes transmitted.
    virtual void uart_send(const std::string& uart, const std::string& bytes) = 0;
    virtual std::uint64_t uart_received(const std::string& uart, std::uint64_t from, std::string& bytes,
        std::vector<std::uint64_t>* times = nullptr) const = 0;
    virtual void stop() = 0;

    virtual McuStatus status() const = 0;
    // Latest state of every watched pin (the firmware's current time).
    virtual std::map<std::string, PinState> pins() const = 0;
    virtual const std::vector<std::string>& pin_names() const = 0;
    // Drive of each watched pin (in pin_names() order) at virtual time `time_ns`.
    virtual std::vector<PinDrive> drives_at(std::uint64_t time_ns) = 0;
    // Drive of each watched pin averaged over (from_ns, to_ns] (SN-176): a circuit
    // step that contains an edge gets the charge the pin delivered during the
    // step, so PWM averages are exact whatever the step. Engines that only know
    // levels late (Renode) return the drive at `to_ns`.
    virtual std::vector<PinDrive> average_drives(std::uint64_t from_ns, std::uint64_t to_ns)
    {
        (void)from_ns;
        return drives_at(to_ns);
    }
    // Level changes of watched pin `pin` from `from_ns`.
    virtual PinLog pin_log(std::size_t pin, std::uint64_t from_ns) const = 0;
    // Messages for the user since the last call (faults, unsimulated features).
    virtual std::vector<std::string> take_diagnostics() { return {}; }
};

} // namespace simnodus::mcu
