// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Digital devices on a microcontroller's nets (SN-203, ADR 0102): chips such
// as shift registers, EEPROMs, sensors and display controllers whose logic
// runs in step with the firmware. The microcontroller runner resolves the nets
// between the board and its devices at once; the devices' output pins also
// drive the circuit, so their waveforms show on the scope. Qt-free, no
// dependencies.
#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace simnodus::devices {

inline constexpr std::uint64_t no_event = std::numeric_limits<std::uint64_t>::max();

// What a device does on one of its pins.
struct Output {
    bool driven = false;     // false: high impedance (an input, or a released open-drain output)
    bool high = false;       // level while driven
    bool open_drain = false; // can only pull low (released means not driven)
    bool pull_up = false;    // not driven: a weak pull-up (a PCF8574's quasi-bidirectional pin)
    double strength = 1.0;   // driven: the fraction of the full drive (a MAX7219's intensity)
};

// A push-pull output at `high`; an open-drain output pulling low; a weak pull-up.
inline Output push_pull(bool high, double strength = 1.0)
{
    Output o;
    o.driven = true;
    o.high = high;
    o.strength = strength;
    return o;
}
inline Output open_drain_low()
{
    Output o;
    o.driven = true;
    o.open_drain = true;
    return o;
}
inline Output weak_high()
{
    Output o;
    o.pull_up = true;
    return o;
}

class Device {
public:
    virtual ~Device() = default;
    // The level on the net of signal pin `pin` (index in the pin list given to
    // make_device) changed at `time_ns`.
    virtual void set_input(std::size_t pin, bool level, std::uint64_t time_ns) = 0;
    // The level of every signal pin when the run starts: not an edge.
    virtual void initial_input(std::size_t pin, bool level) = 0;
    virtual Output output(std::size_t pin) const = 0;
    // Time-driven behavior (write cycles, multiplexing): the next time the device
    // changes by itself, and the call that lets it.
    virtual std::uint64_t next_event_ns() const { return no_event; }
    virtual void advance(std::uint64_t time_ns) { (void)time_ns; }
    // A parameter changed while running (the library marks it "live").
    virtual void set_parameter(const std::string& name, const std::string& value)
    {
        (void)name;
        (void)value;
    }
    // Memory chips (SN-208): their contents (empty for other chips), a number that
    // changes whenever the contents change, and new contents (from a file).
    virtual std::vector<std::uint8_t> memory() const { return {}; }
    virtual std::uint64_t memory_version() const { return 0; }
    virtual void load_memory(const std::vector<std::uint8_t>& bytes) { (void)bytes; }
    // What a display shows (UTF-8, one row per line); empty for other chips.
    virtual std::string text() const { return {}; }
    // Messages about the device's use (protocol errors), taken by the runner.
    std::vector<std::string> take_messages() { return std::exchange(messages_, {}); }

protected:
    void message(std::string text) { messages_.push_back(std::move(text)); }

private:
    std::vector<std::string> messages_;
};

// A device of `kind` ("74hc595", ...) whose signal pins are `pins` (in this
// order); nullptr and `error` for an unknown kind or a missing pin.
std::unique_ptr<Device> make_device(const std::string& kind, const std::vector<std::string>& pins,
    const std::map<std::string, std::string>& parameters, std::string& error);

} // namespace simnodus::devices
