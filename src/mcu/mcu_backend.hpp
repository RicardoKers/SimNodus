// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Microcontroller engine interface (SN-171, docs/planning/MCU_EMULATOR_PLAN.md):
// what SimulationSession needs from a running board (NativeMcuRunner, SimNodus's
// own emulator; Renode was removed, ADR 0094). Times are virtual
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
    // Driven outputs of chips: a fraction of the full drive (a MAX7219's
    // intensity, SN-204); board pins always drive fully.
    double strength = 1.0;

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
    std::uint64_t late_analog_samples = 0; // ADC samples that took the circuit's latest value
    int gdb_port = -1;           // GDB server port, -1 without one
    bool debug_waiting = false;  // waiting for the first GDB client
    bool debug_halted = false;   // halted by the debugger (or waiting for it)
    int debug_resumes = 0;       // times the debugger let a halted firmware run again
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
    // pin_names()) since circuit time `time_ns` (MCU-04), the accepted point that
    // showed it. `changed_ns` (not after time_ns) estimates when the voltage
    // crossed the threshold, for serial receivers that decode edge times (SN-192).
    virtual void set_input(std::size_t pin, bool level, std::uint64_t time_ns, std::uint64_t changed_ns) = 0;
    // Voltages of the watched pins (pin_names() order; NaN for a pin without a
    // node) at an accepted circuit point, for the ADC (MCU-06). Engines without
    // an ADC model ignore them.
    virtual void set_analog(std::uint64_t time_ns, const std::vector<double>& volts)
    {
        (void)time_ns;
        (void)volts;
    }
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
    // levels late return the drive at `to_ns`.
    virtual std::vector<PinDrive> average_drives(std::uint64_t from_ns, std::uint64_t to_ns)
    {
        (void)from_ns;
        return drives_at(to_ns);
    }
    // Edge look-ahead for exact edges (ADR 0093): times in (from_ns, to_ns] at
    // which the drive of one of `pins` changes, for a span the firmware has run.
    // Engines that only report edges late return nothing.
    virtual bool reports_edges_ahead() const { return false; }
    virtual std::vector<std::uint64_t> drive_changes(
        const std::vector<std::size_t>& pins, std::uint64_t from_ns, std::uint64_t to_ns) const
    {
        (void)pins;
        (void)from_ns;
        (void)to_ns;
        return {};
    }
    // Level changes of watched pin `pin` from `from_ns`.
    virtual PinLog pin_log(std::size_t pin, std::uint64_t from_ns) const = 0;
    // Messages for the user since the last call (faults, unsimulated features).
    virtual std::vector<std::string> take_diagnostics() { return {}; }
    // A live parameter of one of the board's chips changed (SN-207).
    virtual void set_device_parameter(std::uint64_t component, const std::string& name, const std::string& value)
    {
        (void)component;
        (void)name;
        (void)value;
    }
    // What the board's display chips show (component -> UTF-8 text, SN-206).
    virtual std::map<std::uint64_t, std::string> device_texts() const { return {}; }
};

} // namespace simnodus::mcu
